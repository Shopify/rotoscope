#include <errno.h>
#include <ruby.h>
#include <ruby/debug.h>
#include <ruby/io.h>
#include <ruby/intern.h>
#include <ruby/version.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/file.h>

#include "callsite.h"
#include "rotoscope.h"
#include "stack.h"
#include "strmemo.h"
#include "tracepoint.h"

VALUE cRotoscope, cTracePoint;
ID id_initialize, id_gsub;
VALUE str_quote, str_escaped_quote, str_header;

static unsigned long gettid() {
  return NUM2ULONG(rb_obj_id(rb_thread_current()));
}

static const char *evflag2name(rb_event_flag_t evflag) {
  switch (evflag) {
    case RUBY_EVENT_CALL:
    case RUBY_EVENT_C_CALL:
      return "call";
    case RUBY_EVENT_RETURN:
    case RUBY_EVENT_C_RETURN:
      return "return";
    default:
      return "unknown";
  }
}

static bool rejected_path(VALUE path, Rotoscope *config) {
  for (unsigned long i = 0; i < config->blacklist_size; i++) {
    if (strstr(StringValueCStr(path), config->blacklist[i])) return true;
  }

  return false;
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

static rs_class_desc_t tracearg_class(rb_trace_arg_t *trace_arg) {
  VALUE klass;
  const char *method_level;
  VALUE self = rb_tracearg_self(trace_arg);

  if ((RB_TYPE_P(self, T_CLASS) || RB_TYPE_P(self, T_MODULE)) &&
      SYM2ID(rb_tracearg_method_id(trace_arg)) != id_initialize) {
    method_level = CLASS_METHOD;
    klass = self;
  } else {
    method_level = INSTANCE_METHOD;
    klass = rb_obj_class(self);
  }

  return (rs_class_desc_t){
      .name = class2str(klass), .method_level = method_level,
  };
}

static VALUE tracearg_method_name(rb_trace_arg_t *trace_arg) {
  return rb_sym2str(rb_tracearg_method_id(trace_arg));
}

static rs_tracepoint_t extract_full_tracevals(rb_trace_arg_t *trace_arg,
                                              const rs_callsite_t *callsite) {
  rs_class_desc_t method_owner = tracearg_class(trace_arg);
  rb_event_flag_t event_flag = rb_tracearg_event_flag(trace_arg);

  VALUE method_name = tracearg_method_name(trace_arg);
  VALUE filepath = callsite->filepath;

  return (rs_tracepoint_t){.event = evflag2name(event_flag),
                           .entity = method_owner.name,
                           .filepath = filepath,
                           .method_name = method_name,
                           .method_level = method_owner.method_level,
                           .lineno = callsite->lineno};
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

static void log_trace_event_with_caller(VALUE output_buffer,
                                        VALUE io,
                                        rs_stack_frame_t *stack_frame,
                                        rs_stack_frame_t *caller_frame,
                                        rs_strmemo_t **call_memo) {
  VALUE escaped_method_name = escape_csv_string(stack_frame->tp.method_name);
  VALUE escaped_caller_method_name =
      escape_csv_string(caller_frame->tp.method_name);

  while (true) {
    rb_str_modify(output_buffer);
    long out_len = snprintf(RSTRING_PTR(output_buffer), rb_str_capacity(output_buffer), RS_CSV_FORMAT "\n",
                   RS_CSV_VALUES(&stack_frame->tp, &caller_frame->tp,
                           escaped_method_name, escaped_caller_method_name));

    if (out_len < RSTRING_LEN(output_buffer)) {
      rb_str_set_len(output_buffer, out_len);
      break;
    }
    rb_str_resize(output_buffer, out_len + 1);
  }

  RB_GC_GUARD(escaped_method_name);
  RB_GC_GUARD(escaped_caller_method_name);

  if (rs_strmemo_uniq(call_memo, RSTRING_PTR(output_buffer))) {
    rb_io_write(io, output_buffer);
  }
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

  rs_callsite_t trace_path = tracearg_path(trace_arg);
  bool blacklisted = rejected_path(trace_path.filepath, config);

  rs_tracepoint_t trace = extract_full_tracevals(trace_arg, &trace_path);

  rs_stack_push(&config->stack, trace, blacklisted);

  if (blacklisted) return;

  rs_stack_frame_t *stack_frame = rs_stack_peek(&config->stack);
  rs_stack_frame_t *caller_frame = rs_stack_below(&config->stack, stack_frame);
  log_trace_event_with_caller(config->output_buffer, config->log, stack_frame, caller_frame,
                              &config->call_memo);
}

static void rs_gc_mark(Rotoscope *config) {
  rb_gc_mark(config->log);
  rb_gc_mark(config->tracepoint);
  rb_gc_mark(config->output_buffer);
  rs_stack_mark(&config->stack);
}

void rs_dealloc(Rotoscope *config) {
  stop_tracing_on_cleanup(config);
  rs_stack_free(&config->stack);
  rs_strmemo_free(config->call_memo);
  xfree(config->blacklist);
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

void copy_blacklist(Rotoscope *config, VALUE blacklist) {
  Check_Type(blacklist, T_ARRAY);

  size_t blacklist_malloc_size =
      RARRAY_LEN(blacklist) * sizeof(*config->blacklist);

  for (long i = 0; i < RARRAY_LEN(blacklist); i++) {
    VALUE ruby_string = RARRAY_AREF(blacklist, i);
    Check_Type(ruby_string, T_STRING);
    blacklist_malloc_size += RSTRING_LEN(ruby_string) + 1;
  }

  config->blacklist = ruby_xmalloc(blacklist_malloc_size);
  config->blacklist_size = RARRAY_LEN(blacklist);
  char *str = (char *)(config->blacklist + config->blacklist_size);

  for (unsigned long i = 0; i < config->blacklist_size; i++) {
    VALUE ruby_string = RARRAY_AREF(blacklist, i);

    config->blacklist[i] = str;
    memcpy(str, RSTRING_PTR(ruby_string), RSTRING_LEN(ruby_string));
    str += RSTRING_LEN(ruby_string);
    *str = '\0';
    str++;
  }
}

VALUE initialize(int argc, VALUE *argv, VALUE self) {
  Rotoscope *config = get_config(self);
  VALUE output, blacklist;

  rb_scan_args(argc, argv, "11", &output, &blacklist);
  output = rb_io_check_io(output);

  if (!NIL_P(blacklist)) {
    copy_blacklist(config, blacklist);
  }

  config->log = output;

  rb_io_write(config->log, str_header);

  rs_stack_init(&config->stack, STACK_CAPACITY);
  config->call_memo = NULL;
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

VALUE rotoscope_mark(int argc, VALUE *argv, VALUE self) {
  VALUE str;
  rb_scan_args(argc, argv, "01", &str);

  if (NIL_P(str)) str = rb_str_new2("");
  Check_Type(str, T_STRING);

  Rotoscope *config = get_config(self);
  if (config->state != RS_CLOSED && !in_fork(config)) {
    rs_strmemo_free(config->call_memo);
    config->call_memo = NULL;
    rb_io_write(config->log, rb_str_new_cstr("--- "));
    rb_io_write(config->log, str);
    rb_io_write(config->log, rb_str_new_cstr("\n"));
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
    rb_io_close(config->log);
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

  str_quote = rb_str_new_literal("\"");
  rb_global_variable(&str_quote);
  str_escaped_quote = rb_str_new_literal("\"\"");
  rb_global_variable(&str_escaped_quote);

  str_header = rb_str_new_literal(RS_CSV_HEADER "\n");
  rb_global_variable(&str_header);

  cRotoscope = rb_define_class("Rotoscope", rb_cObject);
  rb_define_alloc_func(cRotoscope, rs_alloc);
  rb_define_method(cRotoscope, "initialize", initialize, -1);
  rb_define_method(cRotoscope, "trace", (VALUE(*)(ANYARGS))rotoscope_trace, 0);
  rb_define_method(cRotoscope, "mark", (VALUE(*)(ANYARGS))rotoscope_mark, -1);
  rb_define_method(cRotoscope, "close", (VALUE(*)(ANYARGS))rotoscope_close, 0);
  rb_define_method(cRotoscope, "io", rotoscope_io, 0);
  rb_define_method(cRotoscope, "start_trace",
                   (VALUE(*)(ANYARGS))rotoscope_start_trace, 0);
  rb_define_method(cRotoscope, "stop_trace",
                   (VALUE(*)(ANYARGS))rotoscope_stop_trace, 0);
  rb_define_method(cRotoscope, "state", (VALUE(*)(ANYARGS))rotoscope_state, 0);

  init_callsite();
}
