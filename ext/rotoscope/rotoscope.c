#include "ruby.h"
#include "ruby/debug.h"
#include "ruby/intern.h"
#include <stdio.h>

#define EVENT_CALL   (RUBY_EVENT_CALL | RUBY_EVENT_C_CALL)
#define EVENT_RETURN (RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN)

typedef enum { false, true } bool;

typedef struct {
  FILE* log;
  VALUE tracepoint;
  VALUE blacklist;
} TRACE;

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
  if (NIL_P(cached_lookup)) {
    return RSTRING_PTR(rb_class_name(klass));
  } else {
    return RSTRING_PTR(cached_lookup);
  }
}

int indent = 0;

static bool rejected_path(char* path, VALUE blacklist) {

  long i;
  for (i=0; i < RARRAY_LEN(blacklist); i++) {
    if (strstr(path, RSTRING_PTR(RARRAY_AREF(blacklist, i)))) {
      return true;
    }
  }

  return false;
}

typedef struct {
  const char* event;
  const char* method_name;
  const char* method_owner;
} TRACEVALS;

static TRACEVALS extract_full_tracevals(rb_trace_arg_t* trace_arg) {
  VALUE self = rb_tracearg_self(trace_arg);
  const char *method_owner;
  VALUE klass;

  switch (TYPE(self)) {
    case T_OBJECT:
    case T_CLASS:
    case T_MODULE:
      klass = RBASIC_CLASS(self);
      method_owner = class2str(klass);
      break;
    default:
      klass = rb_tracearg_defined_class(trace_arg);
      method_owner = class2str(klass);
      break;
  }

  return (TRACEVALS) {
    .event = evflag2name(rb_tracearg_event_flag(trace_arg)),
    .method_name = RSTRING_PTR(rb_sym2str(rb_tracearg_method_id(trace_arg))),
    .method_owner = method_owner
  };
}

static void event_hook(VALUE tpval, void *data) {
  TRACE* tp = (TRACE *)data;

  rb_trace_arg_t *trace_arg = rb_tracearg_from_tracepoint(tpval);

  char* trace_path = RSTRING_PTR(rb_tracearg_path(trace_arg));
  if (rejected_path(trace_path, tp->blacklist)) return;

  TRACEVALS trace_values = extract_full_tracevals(trace_arg);
  // if (rb_tracearg_event_flag(trace_arg) & EVENT_RETURN && indent > 0) indent--;
  fprintf(tp->log, "%*s%-8s > %s#%s\n", indent, "", trace_values.event, trace_values.method_owner, trace_values.method_name/*, trace_path, FIX2INT(rb_tracearg_lineno(trace_arg))*/);
  // if (rb_tracearg_event_flag(trace_arg) & EVENT_CALL) indent++;
}

TRACE tp_container;

VALUE rotoscope_start_trace(VALUE self, VALUE args) {
  FILE* log = fopen("/tmp/trace.log", "a");

  if (RARRAY_LEN(args) > 0) {
    tp_container.blacklist = rb_ary_entry(args, 0);
  } else {
    tp_container.blacklist = rb_ary_new();
  }

  tp_container.log = log;
  tp_container.tracepoint = rb_tracepoint_new(Qnil, EVENT_CALL | EVENT_RETURN, event_hook, (void *)&tp_container);
  rb_tracepoint_enable(tp_container.tracepoint);

  return Qnil;
}

VALUE rotoscope_stop_trace(VALUE self) {
  rb_tracepoint_disable(tp_container.tracepoint);

  fclose(tp_container.log);
  return Qnil;
}

VALUE rotoscope_trace(VALUE self, VALUE args)
{
  rotoscope_start_trace(self, args);
  VALUE ret = rb_yield(Qundef);
  rotoscope_stop_trace(self);
  return ret;
}

void Init_rotoscope(void)
{
  VALUE mRotoscope = rb_define_module("Rotoscope");
  rb_define_module_function(mRotoscope, "trace", (VALUE(*)(ANYARGS))rotoscope_trace, -2);
  rb_define_module_function(mRotoscope, "start_trace", (VALUE(*)(ANYARGS))rotoscope_start_trace, -2);
  rb_define_module_function(mRotoscope, "stop_trace", (VALUE(*)(ANYARGS))rotoscope_stop_trace, 0);
}
