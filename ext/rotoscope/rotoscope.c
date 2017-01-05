#include "ruby.h"
#include "ruby/debug.h"
#include "ruby/intern.h"
#include <stdio.h>

#define EVENT_CALL   (RUBY_EVENT_CALL | RUBY_EVENT_C_CALL)
#define EVENT_RETURN (RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN)

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

static void event_hook(VALUE tpval, void *data) {
  FILE *log;
  const char *event, *method_name, *method_owner;

  log = (FILE *)data;
  rb_trace_arg_t *trace_arg = rb_tracearg_from_tracepoint(tpval);

  event = evflag2name(rb_tracearg_event_flag(trace_arg));
  method_name = RSTRING_PTR(rb_sym2str(rb_tracearg_method_id(trace_arg)));

  // causes sketchy behaviour in Rubyland
  // inspect = RSTRING_PTR(rb_inspect(rb_tracearg_self(trace_arg)));

  VALUE self = rb_tracearg_self(trace_arg);
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

  fprintf(log, "%s:   \t%s#%s (0x%lx)\n", event, method_owner, method_name, rb_obj_id(klass));
}

VALUE rotoscope_trace(VALUE self)
{
  FILE* log = fopen("/tmp/trace.log", "w");
  /* TODO: check return */

  VALUE tracepoint = rb_tracepoint_new(Qnil, EVENT_CALL | EVENT_RETURN, event_hook, (void *)log);
  rb_tracepoint_enable(tracepoint);
  VALUE ret = rb_yield(Qundef);
  rb_tracepoint_disable(tracepoint);

  fclose(log);
  return ret;
}

void Init_rotoscope(void)
{
  VALUE mRotoscope = rb_define_module("Rotoscope");
  rb_define_module_function(mRotoscope, "trace", (VALUE(*)(ANYARGS))rotoscope_trace, 0);
}
