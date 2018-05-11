#include <errno.h>
#include <ruby.h>
#include <ruby/debug.h>
#include <ruby/intern.h>
#include <ruby/io.h>
#include <ruby/version.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/file.h>

#include "callsite.h"
#include "method_desc.h"
#include "rotoscope.h"
#include "stack.h"

VALUE cRotoscope, cTracePoint;
ID id_initialize, id_gsub, id_close, id_match_p;
VALUE str_quote, str_escaped_quote, str_header;
VALUE str_unknown_class_name, str_unknown_method_name;

static unsigned long gettid() {
  return NUM2ULONG(rb_obj_id(rb_thread_current()));
}

static VALUE class_path(VALUE klass) {
  VALUE cached_path = rb_class_path_cached(klass);
  if (!NIL_P(cached_path)) {
    return cached_path;
  }
  return rb_class_path(klass);
}

static VALUE singleton_object(VALUE singleton_class) {
  return rb_iv_get(singleton_class, "__attached__");
}

static VALUE class2str(VALUE klass) {
  while (FL_TEST(klass, FL_SINGLETON)) {
    klass = singleton_object(klass);
    if (!RB_TYPE_P(klass, T_MODULE) && !RB_TYPE_P(klass, T_CLASS)) {
      // singleton of an instance
      klass = rb_obj_class(klass);
    }
  }
  return class_path(klass);
}

static rs_callsite_t tracearg_path(rb_trace_arg_t *trace_arg) {
  switch (rb_tracearg_event_flag(trace_arg)) {
    case RUBY_EVENT_C_RETURN:
    case RUBY_EVENT_C_CALL:
      return c_callsite(trace_arg);
    default:
      return ruby_callsite(trace_arg);
  }
}

static rs_method_desc_t called_method_desc(rb_trace_arg_t *trace_arg) {
  VALUE self = rb_tracearg_self(trace_arg);
  VALUE method_id = rb_tracearg_method_id(trace_arg);
  bool singleton_p = (RB_TYPE_P(self, T_CLASS) || RB_TYPE_P(self, T_MODULE)) &&
                     SYM2ID(method_id) != id_initialize;
  VALUE klass = singleton_p ? self : rb_obj_class(self);

  return (rs_method_desc_t){
      .class_name = class2str(klass),
      .id = method_id,
      .singleton_p = singleton_p,
  };
}

static bool in_fork(Rotoscope *config) { return config->pid != getpid(); }

// The GC sweep step will turn objects with finalizers (e.g. rs_dealloc)
// to zombie objects until their finalizer is run. In this state, any
// ruby objects in the Rotoscope struct may have already been collected
// so they can't safely be used. If tracing isn't stopped before the
// Rotoscope object has been garbage collected, then we still may receive
// trace events for method calls in finalizers that run before the one
// for the Rotoscope object.
bool rotoscope_marked_for_garbage_collection(Rotoscope *config) {
  return RB_BUILTIN_TYPE(config->self) == RUBY_T_ZOMBIE;
}

VALUE escape_csv_string(VALUE string) {
  if (!memchr(RSTRING_PTR(string), '"', RSTRING_LEN(string))) {
    return string;
  }
  return rb_funcall(string, id_gsub, 2, str_quote, str_escaped_quote);
}

typedef struct {
  VALUE class_name;
  VALUE name;
  const char *level;
} rs_method_log_fields_t;

static rs_method_log_fields_t method_log_fields(rs_method_desc_t *method) {
  if (method == NULL) {
    return (rs_method_log_fields_t){
        .class_name = str_unknown_class_name,
        .name = str_unknown_method_name,
        .level = "<UNKNOWN>",
    };
  } else {
    return (rs_method_log_fields_t){
        .class_name = method->class_name,
        .name = escape_csv_string(rb_sym2str(method->id)),
        .level = method->singleton_p ? CLASS_METHOD : INSTANCE_METHOD,
    };
  }
}

static void log_call(VALUE output_buffer, VALUE io, rs_callsite_t *call_site,
                     rs_method_desc_t *method_desc,
                     rs_method_desc_t *caller_method) {
  rs_method_log_fields_t method_fields = method_log_fields(method_desc);
  rs_method_log_fields_t caller_method_fields =
      method_log_fields(caller_method);

  while (true) {
    rb_str_modify(output_buffer);
    long out_len =
        snprintf(RSTRING_PTR(output_buffer), rb_str_capacity(output_buffer),
                 RS_CSV_FORMAT "\n",
                 RS_CSV_VALUES(method_fields, caller_method_fields, call_site));

    if (out_len < RSTRING_LEN(output_buffer)) {
      rb_str_set_len(output_buffer, out_len);
      break;
    }
    rb_str_resize(output_buffer, out_len + 1);
  }

  RB_GC_GUARD(method_fields.name);
  RB_GC_GUARD(caller_method_fields.name);

  rb_io_write(io, output_buffer);
}

static void stop_tracing_on_cleanup(Rotoscope *config) {
  if (config->state == RS_TRACING) {
    // During process cleanup, event hooks are removed and tracepoint may have
    // already have been GCed, so we need a sanity check before disabling the
    // tracepoint.
    if (RB_TYPE_P(config->tracepoint, T_DATA) &&
        CLASS_OF(config->tracepoint) == cTracePoint) {
      rb_tracepoint_disable(config->tracepoint);
    }
    config->state = RS_OPEN;
  }
}

static void event_hook(VALUE tpval, void *data) {
  Rotoscope *config = (Rotoscope *)data;

  if (rotoscope_marked_for_garbage_collection(config)) {
    stop_tracing_on_cleanup(config);
    return;
  }

  if (config->tid != gettid()) return;
  if (in_fork(config)) {
    rb_tracepoint_disable(config->tracepoint);
    config->state = RS_OPEN;
    return;
  }

  rb_trace_arg_t *trace_arg = rb_tracearg_from_tracepoint(tpval);

  if (rb_tracearg_defined_class(trace_arg) == cRotoscope) {
    return;
  }

  rb_event_flag_t event_flag = rb_tracearg_event_flag(trace_arg);

  if (event_flag & EVENT_RETURN) {
    if (!rs_stack_empty(&config->stack)) {
      rs_stack_pop(&config->stack);
    }
    return;
  }

  rs_callsite_t call_site = tracearg_path(trace_arg);

  rs_stack_frame_t *caller = rs_stack_peek(&config->stack);

  rs_method_desc_t method_desc = called_method_desc(trace_arg);
  rs_stack_push(&config->stack, (rs_stack_frame_t){.method = method_desc});

  bool blacklist =
      rb_funcall(config->blacklist, id_match_p, 1, call_site.filepath) == Qtrue;
  if (blacklist) {
    return;
  }

  rs_method_desc_t *caller_method = caller ? &caller->method : NULL;
  log_call(config->output_buffer, config->log, &call_site, &method_desc,
           caller_method);
}

static void rs_gc_mark(Rotoscope *config) {
  rb_gc_mark(config->log);
  rb_gc_mark(config->tracepoint);
  rb_gc_mark(config->output_buffer);
  rb_gc_mark(config->blacklist);
  rs_stack_mark(&config->stack);
}

void rs_dealloc(Rotoscope *config) {
  stop_tracing_on_cleanup(config);
  rs_stack_free(&config->stack);
  xfree(config);
}

static VALUE rs_alloc(VALUE klass) {
  Rotoscope *config;
  VALUE self =
      Data_Make_Struct(klass, Rotoscope, rs_gc_mark, rs_dealloc, config);
  config->self = self;
  config->log = Qnil;
  config->tracepoint = rb_tracepoint_new(Qnil, EVENT_CALL | EVENT_RETURN,
                                         event_hook, (void *)config);
  config->pid = getpid();
  config->tid = gettid();
  config->output_buffer = Qnil;
  return self;
}

static Rotoscope *get_config(VALUE self) {
  Rotoscope *config;
  Data_Get_Struct(self, Rotoscope, config);
  return config;
}

VALUE initialize(int argc, VALUE *argv, VALUE self) {
  Rotoscope *config = get_config(self);
  VALUE output, blacklist;

  rb_scan_args(argc, argv, "11", &output, &blacklist);

  Check_Type(blacklist, RUBY_T_REGEXP);

  config->blacklist = blacklist;
  config->log = output;

  rb_io_write(config->log, str_header);

  rs_stack_init(&config->stack, STACK_CAPACITY);
  config->state = RS_OPEN;
  config->output_buffer = rb_str_buf_new(LOG_BUFFER_SIZE);
  return self;
}

VALUE rotoscope_start_trace(VALUE self) {
  Rotoscope *config = get_config(self);
  rb_tracepoint_enable(config->tracepoint);
  config->state = RS_TRACING;
  return Qnil;
}

VALUE rotoscope_stop_trace(VALUE self) {
  Rotoscope *config = get_config(self);
  if (rb_tracepoint_enabled_p(config->tracepoint)) {
    rb_tracepoint_disable(config->tracepoint);
    config->state = RS_OPEN;
    rs_stack_reset(&config->stack);
  }

  return Qnil;
}

VALUE rotoscope_close(VALUE self) {
  Rotoscope *config = get_config(self);
  if (config->state == RS_CLOSED) {
    return Qtrue;
  }
  rb_tracepoint_disable(config->tracepoint);
  config->state = RS_OPEN;
  if (!in_fork(config)) {
    rb_funcall(config->log, id_close, 0);
  }
  config->state = RS_CLOSED;
  return Qtrue;
}

VALUE rotoscope_io(VALUE self) {
  Rotoscope *config = get_config(self);
  return config->log;
}

VALUE rotoscope_trace(VALUE self) {
  rotoscope_start_trace(self);
  return rb_ensure(rb_yield, Qundef, rotoscope_stop_trace, self);
}

VALUE rotoscope_state(VALUE self) {
  Rotoscope *config = get_config(self);
  switch (config->state) {
    case RS_OPEN:
      return ID2SYM(rb_intern("open"));
    case RS_TRACING:
      return ID2SYM(rb_intern("tracing"));
    default:
      return ID2SYM(rb_intern("closed"));
  }
}

void Init_rotoscope(void) {
  cTracePoint = rb_const_get(rb_cObject, rb_intern("TracePoint"));

  id_initialize = rb_intern("initialize");
  id_gsub = rb_intern("gsub");
  id_close = rb_intern("close");
  id_match_p = rb_intern("match?");

  str_quote = rb_str_new_literal("\"");
  rb_global_variable(&str_quote);
  str_escaped_quote = rb_str_new_literal("\"\"");
  rb_global_variable(&str_escaped_quote);

  str_header = rb_str_new_literal(RS_CSV_HEADER "\n");
  rb_global_variable(&str_header);

  str_unknown_class_name = rb_str_new_literal("<ROOT>");
  rb_global_variable(&str_unknown_class_name);

  str_unknown_method_name = rb_str_new_literal("<UNKNOWN>");
  rb_global_variable(&str_unknown_method_name);

  cRotoscope = rb_define_class("Rotoscope", rb_cObject);
  rb_define_alloc_func(cRotoscope, rs_alloc);
  rb_define_method(cRotoscope, "initialize", initialize, -1);
  rb_define_method(cRotoscope, "trace", (VALUE(*)(ANYARGS))rotoscope_trace, 0);
  rb_define_method(cRotoscope, "close", (VALUE(*)(ANYARGS))rotoscope_close, 0);
  rb_define_method(cRotoscope, "io", rotoscope_io, 0);
  rb_define_method(cRotoscope, "start_trace",
                   (VALUE(*)(ANYARGS))rotoscope_start_trace, 0);
  rb_define_method(cRotoscope, "stop_trace",
                   (VALUE(*)(ANYARGS))rotoscope_stop_trace, 0);
  rb_define_method(cRotoscope, "state", (VALUE(*)(ANYARGS))rotoscope_state, 0);

  init_callsite();
}
