#include "ruby.h"
#include "ruby/debug.h"
#include "ruby/intern.h"
#include <stdio.h>
#include <errno.h>
#include "rotoscope.h"

static const char* evflag2name(rb_event_flag_t evflag) {
  switch(evflag) {
    case RUBY_EVENT_CALL:
      return "call";
    case RUBY_EVENT_C_CALL:
      return "c_call";
    case RUBY_EVENT_RETURN:
      return "return";
    case RUBY_EVENT_C_RETURN:
      return "c_return";
    default:
      return "unknown";
  }
}

static char* class2str(VALUE klass) {
  VALUE cached_lookup = rb_class_path_cached(klass);
  if (NIL_P(cached_lookup)) return RSTRING_PTR(rb_class_name(klass));
  else return RSTRING_PTR(cached_lookup);
}

static bool rejected_path(const char* path, VALUE blacklist) {
  long i;
  char *blacklist_path;
  Check_Type(blacklist, T_ARRAY);

  for (i=0; i < RARRAY_LEN(blacklist); i++) {
    blacklist_path = RSTRING_PTR(RARRAY_AREF(blacklist, i));
    if (strlen(path) < strlen(blacklist_path)) continue;
    if (strstr(path, RSTRING_PTR(RARRAY_AREF(blacklist, i)))) return true;
  }

  return false;
}

int format_for_csv(char* buffer, size_t size, rs_tracepoint_t trace) {
  return snprintf(buffer, size, "%s,\"%s\",\"%s\",\"%s\",%d",
    trace.event, trace.method_owner, trace.method_name, trace.filepath, trace.lineno);
}

static char* trace_as_csv(rs_tracepoint_t trace) {
  int result;
  char *buf = ALLOC_N(char, MIN_CSV_BUFSIZE);
  result = format_for_csv(buf, MIN_CSV_BUFSIZE, trace);

  if (result >= MIN_CSV_BUFSIZE) {
    REALLOC_N(buf, char, MAX_CSV_BUFSIZE);
    result = format_for_csv(buf, MAX_CSV_BUFSIZE, trace);

    if (result >= MAX_CSV_BUFSIZE) {
      fprintf(stderr, "\nERROR: Could not allocate enough room for tracepoint, larger than %d bytes (%s,\"%s\",\"%s\",\"%s\",%d)\n",
        MAX_CSV_BUFSIZE, trace.event, trace.method_owner, trace.method_name, trace.filepath, trace.lineno);
      exit(1);
    }
  }

  return buf;
}

static const char* tracearg_path(rb_trace_arg_t *trace_arg) {
  VALUE path = rb_tracearg_path(trace_arg);
  return RTEST(path) ? RSTRING_PTR(path) : "";
}

static rs_tracepoint_t extract_full_tracevals(rb_trace_arg_t* trace_arg) {
  VALUE self = rb_tracearg_self(trace_arg);

  VALUE klass = (RB_TYPE_P(self, T_OBJECT) || RB_TYPE_P(self, T_CLASS) || RB_TYPE_P(self, T_MODULE)) ?
    RBASIC_CLASS(self) :
    rb_tracearg_defined_class(trace_arg);
  const char *method_owner = class2str(klass);

  return (rs_tracepoint_t) {
    .event = evflag2name(rb_tracearg_event_flag(trace_arg)),
    .method_name = RSTRING_PTR(rb_sym2str(rb_tracearg_method_id(trace_arg))),
    .method_owner = method_owner,
    .filepath = tracearg_path(trace_arg),
    .lineno = FIX2INT(rb_tracearg_lineno(trace_arg))
  };
}

static void event_hook(VALUE tpval, void *data) {
  Rotoscope* config = (Rotoscope *)data;
  rb_trace_arg_t *trace_arg = rb_tracearg_from_tracepoint(tpval);
  const char* trace_path = tracearg_path(trace_arg);

  if (rejected_path(trace_path, config->blacklist)) return;

  rs_tracepoint_t trace_values = extract_full_tracevals(trace_arg);
  char* formatted_str = trace_as_csv(trace_values);

  if (config->log != NULL) {
    fprintf(config->log, "%s\n", formatted_str);
  } else {
    fprintf(stderr, "\nERROR: Unable to write to NULL file handle.\n");
    exit(1);
  }

  free(formatted_str);
}

static void gc_mark(Rotoscope* config) {
  rb_gc_mark(config->tracepoint);
  rb_gc_mark(config->blacklist);
}

void dealloc(Rotoscope* config) {
  if (config->log) fclose(config->log);
  free(config);
}

static VALUE alloc(VALUE klass) {
  Rotoscope* config;
  return Data_Make_Struct(klass, Rotoscope, gc_mark, dealloc, config);
}

static Rotoscope* get_config(VALUE self) {
  Rotoscope* config;
  Data_Get_Struct(self, Rotoscope, config);
  return config;
}

VALUE initialize(int argc, VALUE* argv, VALUE self) {
  Rotoscope* config = get_config(self);
  VALUE output_path;

  rb_scan_args(argc, argv, "11", &output_path, &config->blacklist);
  if (NIL_P(config->blacklist)) config->blacklist = rb_ary_new();

  Check_Type(config->blacklist, T_ARRAY);
  Check_Type(output_path, T_STRING);

  const char* path = RSTRING_PTR(output_path);
  config->log = fopen(path, "a");
  if (config->log == NULL) {
    fprintf(stderr, "\nERROR: Failed to open file handle at %s (%s)\n", path, strerror(errno));
    exit(1);
  }

  return self;
}

VALUE rotoscope_start_trace(VALUE self) {
  Rotoscope* config = get_config(self);
  config->tracepoint = rb_tracepoint_new(Qnil, EVENT_CALL | EVENT_RETURN, event_hook, (void *)config);
  rb_tracepoint_enable(config->tracepoint);
  return Qnil;
}

VALUE rotoscope_stop_trace(VALUE self) {
  Rotoscope* config = get_config(self);
  if (rb_tracepoint_enabled_p(config->tracepoint)) {
    rb_tracepoint_disable(config->tracepoint);
  }
  return Qnil;
}

VALUE rotoscope_trace(VALUE self) {
  rotoscope_start_trace(self);
  return rb_ensure(rb_yield, Qundef, rotoscope_stop_trace, self);
}

void Init_rotoscope(void) {
  VALUE cRotoscope = rb_define_class("Rotoscope", rb_cObject);
  rb_define_alloc_func(cRotoscope, alloc);
  rb_define_method(cRotoscope, "initialize", initialize, -1);
  rb_define_method(cRotoscope, "trace", (VALUE(*)(ANYARGS))rotoscope_trace, 0);
  rb_define_method(cRotoscope, "start_trace", (VALUE(*)(ANYARGS))rotoscope_start_trace, 0);
  rb_define_method(cRotoscope, "stop_trace", (VALUE(*)(ANYARGS))rotoscope_stop_trace, 0);
}
