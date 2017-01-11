#include "ruby.h"
#include "ruby/debug.h"
#include "ruby/intern.h"
#include <stdio.h>
#include <errno.h>

#define EVENT_CALL   (RUBY_EVENT_CALL | RUBY_EVENT_C_CALL)
#define EVENT_RETURN (RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN)
#define MAX_LOG_SIZE 100000000 /* bytes */

typedef enum { false, true } bool;

typedef struct {
  FILE* log;
  VALUE tracepoint;
  VALUE blacklist;
} TRACE;

typedef struct {
  const char* event;
  const char* method_name;
  const char* method_owner;
  const char* filepath;
  int lineno;
} TRACEVALS;

void log_output(const char* str) {
  printf("[!]\t%s\n", str);
  fflush(stdout);
}

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

static bool rejected_path(const char* path, VALUE blacklist) {
  long i;
  for (i=0; i < RARRAY_LEN(blacklist); i++) {
    if (strstr(path, RSTRING_PTR(RARRAY_AREF(blacklist, i)))) {
      return true;
    }
  }

  return false;
}

#define MIN_BUFSIZE 150
#define MAX_BUFSIZE 500

int format_for_csv(char* buffer, size_t size, TRACEVALS trace) {
  return snprintf(buffer, size, "%s,\"%s\",\"%s\",\"%s\",%d",
    trace.event,
    trace.method_owner,
    trace.method_name,
    trace.filepath,
    trace.lineno);
}

static char* trace_as_csv(TRACEVALS trace) {
  char *buf = (char *)malloc(MIN_BUFSIZE);

  if (format_for_csv(buf, MIN_BUFSIZE, trace) >= MIN_BUFSIZE) {
    free(buf);
    buf = (char *)malloc(MAX_BUFSIZE);
    format_for_csv(buf, MAX_BUFSIZE, trace);
  }
  return buf;
}

static const char* tracearg_path(rb_trace_arg_t *trace_arg) {
  VALUE path = rb_tracearg_path(trace_arg);
  return RTEST(path) ? RSTRING_PTR(path) : "";
}

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
    .method_owner = method_owner,
    .filepath = tracearg_path(trace_arg),
    .lineno = FIX2INT(rb_tracearg_lineno(trace_arg))
  };
}

static void event_hook(VALUE tpval, void *data) {
  TRACE* tp = (TRACE *)data;

  if (tp->log == NULL) return;
  if (ftell(tp->log) > MAX_LOG_SIZE) return;

  rb_trace_arg_t *trace_arg = rb_tracearg_from_tracepoint(tpval);
  const char* trace_path = tracearg_path(trace_arg);

  if (rejected_path(trace_path, tp->blacklist)) return;

  TRACEVALS trace_values = extract_full_tracevals(trace_arg);
  char* formatted_str = trace_as_csv(trace_values);
  fprintf(tp->log, "%s\n", formatted_str);
  free(formatted_str);
}

TRACE tp_container;

VALUE rotoscope_start_trace(VALUE self, VALUE args) {
  FILE* log;

  if (RARRAY_LEN(args) > 0) {
    const char* path = RSTRING_PTR(rb_ary_entry(args, 0));

    log = fopen(path, "a");
    if (log == NULL) {
      printf("failed to open file handle at %s (%s)", path, strerror(errno));
      exit(1);
    }
  } else {
    rb_raise(rb_eArgError, "wrong number of arguments (0 for 1..2)");
  }

  if (RARRAY_LEN(args) > 1) {
    tp_container.blacklist = rb_ary_entry(args, 1);
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
  if (tp_container.log) fclose(tp_container.log);
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
