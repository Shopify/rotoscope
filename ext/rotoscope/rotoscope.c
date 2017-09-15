#include <errno.h>
#include <ruby.h>
#include <ruby/debug.h>
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

VALUE cRotoscope, cTracePoint, cInstructionSeq;
ID id_initialize, id_of, id_label;

// recursive with singleton2str
static VALUE class2str(VALUE klass);

static unsigned long gettid() {
  return NUM2ULONG(rb_obj_id(rb_thread_current()));
}

static int write_csv_header(FILE *log, const char *header) {
  return fprintf(log, "%s\n", header);
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

static VALUE class_of_singleton(VALUE klass) {
  return rb_iv_get(klass, "__attached__");
}

static bool is_class_singleton(VALUE klass) {
  VALUE obj = class_of_singleton(klass);
  return (RB_TYPE_P(obj, T_MODULE) || RB_TYPE_P(obj, T_CLASS));
}

static VALUE singleton2str(VALUE klass) {
  if (is_class_singleton(klass)) {
    return class2str(class_of_singleton(klass));
  } else {
    return singleton2str(CLASS_OF(klass));
  }
}

static VALUE class_path(VALUE klass) {
  VALUE cached_path = rb_class_path_cached(klass);
  if (!NIL_P(cached_path)) {
    return cached_path;
  }
  return rb_class_path(klass);
}

static VALUE class2str(VALUE klass) {
  if (FL_TEST(klass, FL_SINGLETON)) {
    return singleton2str(klass);
  } else {
    return class_path(klass);
  }
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

static VALUE unsafe_obj_method(VALUE argv) {
  VALUE *args = (VALUE *)argv;
  return rb_obj_method(args[0], args[1]);
}
static VALUE nil_method(VALUE *args) { return Qnil; }

static bool endof_block_method(rb_trace_arg_t *trace_arg) {
  VALUE method_id = rb_tracearg_method_id(trace_arg);
  if (NIL_P(method_id)) return false;

  VALUE self = rb_tracearg_self(trace_arg);
  VALUE argv[2] = {self, method_id};
  VALUE method = rb_rescue2(unsafe_obj_method, (VALUE)&argv, nil_method,
                            (VALUE)&argv, rb_eNameError, (VALUE)0);
  if (NIL_P(method)) return false;

  VALUE iseq = rb_funcall(cInstructionSeq, id_of, 1, method);
  if (!RTEST(iseq)) return false;

  VALUE label = rb_funcall(iseq, id_label, 0);
  char *label_str = StringValueCStr(label);
  return strncmp(VM_BLOCK_PREFIX, label_str, strlen(VM_BLOCK_PREFIX)) == 0;
}

unsigned char output_buffer[LOG_BUFFER_SIZE];
static void log_trace_event_with_caller(FILE *stream, rs_strmemo_t **call_memo,
                                        rs_stack_frame_t frame) {
  snprintf((char *)output_buffer, LOG_BUFFER_SIZE, RS_FLATTENED_CSV_FORMAT "\n",
           RS_FLATTENED_CSV_VALUES(frame));
  if (rs_strmemo_uniq(call_memo, (unsigned char *)output_buffer)) {
    fputs((char *)output_buffer, stream);
  }
}

static void log_trace_event(FILE *stream, rs_tracepoint_t trace) {
  fprintf(stream, RS_CSV_FORMAT "\n", RS_CSV_VALUES(trace));
}

static bool invalid_stack_return(rs_stack_t *stack, rs_tracepoint_t *trace) {
  rs_stack_frame_t *last_frame = rs_stack_peek(stack);
  return !rs_raw_tracepoint_cmp(&trace->raw, &last_frame->tp.raw);
}

static void copy_filepath_from_caller(rs_stack_t *stack,
                                      rs_tracepoint_t *trace) {
  rs_stack_frame_t *last_frame = rs_stack_peek(stack);
  trace->filepath = last_frame->tp.filepath;
  trace->lineno = last_frame->tp.lineno;
}

static void event_hook(VALUE tpval, void *data) {
  Rotoscope *config = (Rotoscope *)data;
  if (config->tid != gettid()) return;
  if (in_fork(config)) {
    rb_tracepoint_disable(config->tracepoint);
    config->state = RS_OPEN;
    return;
  }

  rb_trace_arg_t *trace_arg = rb_tracearg_from_tracepoint(tpval);
  rb_event_flag_t event_flag = rb_tracearg_event_flag(trace_arg);
  rs_callsite_t trace_path = tracearg_path(trace_arg);
  rs_raw_tracepoint_t raw_trace = rs_raw_from_tracepoint(tpval);

  if (rejected_path(trace_path.filepath, config)) {
    if (METHOD_CALL_P(event_flag)) return;
    // We must ensure the event being blacklisted has correct filepath set,
    // since block-method returns always point one level too high in the stack.

    // Does this match the last caller?
    rs_stack_frame_t *last_frame = rs_stack_peek(&config->stack);
    if (!rs_raw_tracepoint_cmp(&raw_trace, &last_frame->tp.raw)) return;
    // Are we dealing with a block return in wrong context?
    if (!endof_block_method(trace_arg)) return;
  }

  rs_tracepoint_t trace = extract_full_tracevals(trace_arg, &trace_path);
  trace.raw = raw_trace;

  if (!strcmp("Rotoscope", StringValueCStr(trace.entity))) return;

  rs_stack_frame_t frame;
  if (METHOD_CALL_P(event_flag)) {
    frame = rs_stack_push(&config->stack, trace);
  } else {
    if (invalid_stack_return(&config->stack, &trace)) return;
    if (endof_block_method(trace_arg)) {
      copy_filepath_from_caller(&config->stack, &trace);
    }
    rs_stack_pop(&config->stack);
  }

  if (!config->flatten_output) {
    return log_trace_event(config->log, trace);
  } else if (METHOD_CALL_P(event_flag)) {
    log_trace_event_with_caller(config->log, &config->call_memo, frame);
  }
}

static void close_log_handle(Rotoscope *config) {
  if (config->log) {
    // Stop tracing so the event hook isn't called with a NULL log
    if (config->state == RS_TRACING) {
      // During process cleanup, event hooks are removed and tracepoint may have
      // already have been GCed, so we need a sanity check before disabling the
      // tracepoint.
      if (RB_TYPE_P(config->tracepoint, T_DATA) &&
          CLASS_OF(config->tracepoint) == cTracePoint) {
        rb_tracepoint_disable(config->tracepoint);
      }
    }

    if (in_fork(config)) {
      close(fileno(config->log));
    } else {
      fclose(config->log);
    }
    config->log = NULL;
  }
}

static void rs_gc_mark(Rotoscope *config) {
  rb_gc_mark(config->log_path);
  rb_gc_mark(config->tracepoint);
  rs_stack_mark(&config->stack);
}

void rs_dealloc(Rotoscope *config) {
  close_log_handle(config);
  rs_stack_free(&config->stack);
  rs_strmemo_free(config->call_memo);
  xfree(config->blacklist);
  xfree(config);
}

static VALUE rs_alloc(VALUE klass) {
  Rotoscope *config;
  VALUE self =
      Data_Make_Struct(klass, Rotoscope, rs_gc_mark, rs_dealloc, config);
  config->log_path = Qnil;
  config->tracepoint = rb_tracepoint_new(Qnil, EVENT_CALL | EVENT_RETURN,
                                         event_hook, (void *)config);
  config->pid = getpid();
  config->tid = gettid();
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
  VALUE output_path, blacklist, flatten;

  rb_scan_args(argc, argv, "12", &output_path, &blacklist, &flatten);
  Check_Type(output_path, T_STRING);

  if (!NIL_P(blacklist)) {
    copy_blacklist(config, blacklist);
  }

  config->flatten_output = RTEST(flatten);
  config->log_path = output_path;
  config->log = fopen(StringValueCStr(config->log_path), "w");

  if (config->log == NULL) {
    fprintf(stderr, "\nERROR: Failed to open file handle at %s (%s)\n",
            StringValueCStr(config->log_path), strerror(errno));
    exit(1);
  }

  if (config->flatten_output)
    write_csv_header(config->log, RS_FLATTENED_CSV_HEADER);
  else
    write_csv_header(config->log, RS_CSV_HEADER);

  rs_stack_init(&config->stack, STACK_CAPACITY);
  config->call_memo = NULL;
  config->state = RS_OPEN;
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
  }

  return Qnil;
}

VALUE rotoscope_log_path(VALUE self) {
  Rotoscope *config = get_config(self);
  return config->log_path;
}

VALUE rotoscope_mark(int argc, VALUE *argv, VALUE self) {
  VALUE str;
  rb_scan_args(argc, argv, "01", &str);

  if (NIL_P(str)) str = rb_str_new2("");
  Check_Type(str, T_STRING);

  Rotoscope *config = get_config(self);
  if (config->log != NULL && !in_fork(config)) {
    rs_stack_reset(&config->stack, STACK_CAPACITY);
    rs_strmemo_free(config->call_memo);
    fprintf(config->log, "--- %s\n", StringValueCStr(str));
  }
  return Qnil;
}

VALUE rotoscope_close(VALUE self) {
  Rotoscope *config = get_config(self);
  if (config->state == RS_CLOSED) {
    return Qtrue;
  }
  close_log_handle(config);
  config->state = RS_CLOSED;
  return Qtrue;
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
  VALUE cRubyVm = rb_const_get(rb_cObject, rb_intern("RubyVM"));
  cInstructionSeq = rb_const_get(cRubyVm, rb_intern("InstructionSequence"));
  cTracePoint = rb_const_get(rb_cObject, rb_intern("TracePoint"));

  id_initialize = rb_intern("initialize");
  id_of = rb_intern("of");
  id_label = rb_intern("label");

  cRotoscope = rb_define_class("Rotoscope", rb_cObject);
  rb_define_alloc_func(cRotoscope, rs_alloc);
  rb_define_method(cRotoscope, "initialize", initialize, -1);
  rb_define_method(cRotoscope, "trace", (VALUE(*)(ANYARGS))rotoscope_trace, 0);
  rb_define_method(cRotoscope, "mark", (VALUE(*)(ANYARGS))rotoscope_mark, -1);
  rb_define_method(cRotoscope, "close", (VALUE(*)(ANYARGS))rotoscope_close, 0);
  rb_define_method(cRotoscope, "start_trace",
                   (VALUE(*)(ANYARGS))rotoscope_start_trace, 0);
  rb_define_method(cRotoscope, "stop_trace",
                   (VALUE(*)(ANYARGS))rotoscope_stop_trace, 0);
  rb_define_method(cRotoscope, "log_path",
                   (VALUE(*)(ANYARGS))rotoscope_log_path, 0);
  rb_define_method(cRotoscope, "state", (VALUE(*)(ANYARGS))rotoscope_state, 0);

  init_callsite();
}
