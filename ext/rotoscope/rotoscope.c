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
  const char* event;
  const char* method_name;
  const char* method_owner;
  const char* filepath;
  int lineno;
} TRACEVALS;

typedef struct {
  FILE* log;
  VALUE tracepoint;
  VALUE blacklist;
  const char* foo;
} Rotoscope;

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
  char *blacklist_path;
  Check_Type(blacklist, T_ARRAY);

  for (i=0; i < RARRAY_LEN(blacklist); i++) {
    printf("2");fflush(stdout);
    blacklist_path = RSTRING_PTR(RARRAY_AREF(blacklist, i));
    printf("3");fflush(stdout);
    if (strlen(path) < strlen(blacklist_path)) continue;
    printf("4");fflush(stdout);
    if (strstr(path, RSTRING_PTR(RARRAY_AREF(blacklist, i)))) return true;
  }

  printf("5");fflush(stdout);

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
  printf("!");fflush(stdout);
  Rotoscope* config = (Rotoscope *)data;
  rb_trace_arg_t *trace_arg = rb_tracearg_from_tracepoint(tpval);
  printf("@");fflush(stdout);

  const char* trace_path = tracearg_path(trace_arg);
  printf("#");fflush(stdout);
  if (rejected_path(trace_path, config->blacklist)) return;
  printf("$");fflush(stdout);

  TRACEVALS trace_values = extract_full_tracevals(trace_arg);

  printf("6");fflush(stdout);
  char* formatted_str = trace_as_csv(trace_values);
  printf("7");fflush(stdout);
  if (config->log != NULL) {
    printf("8\nSTORED: %s\n", config->foo);fflush(stdout);
    fprintf(config->log, "%s\n", formatted_str);
  }

  printf("9");fflush(stdout);
  free(formatted_str);
  printf("0");fflush(stdout);
}

static VALUE allocate(VALUE klass) {
  Rotoscope* config;
  return Data_Make_Struct(klass, Rotoscope, NULL, -1, config);
}

VALUE initialize(VALUE self, VALUE args) {
  printf("A");fflush(stdout);
  Rotoscope* config;
  Data_Get_Struct(self, Rotoscope, config);

  config->foo = "HELLO WORLD";

  if (RARRAY_LEN(args) > 0) {
    const char* path = RSTRING_PTR(rb_ary_entry(args, 0));

    config->log = fopen(path, "a");
    if (config->log == NULL) {
      printf("failed to open file handle at %s (%s)", path, strerror(errno));
      exit(1);
    }
  } else {
    rb_raise(rb_eArgError, "wrong number of arguments (0 for 1..2)");
  }

  if (RARRAY_LEN(args) > 1) {
    config->blacklist = rb_ary_dup(rb_ary_entry(args, 1));
  } else {
    config->blacklist = rb_ary_new();
  }
  printf("B");fflush(stdout);
  return self;
}

VALUE rotoscope_start_trace(VALUE self) {
  printf("C");fflush(stdout);
  Rotoscope* config;
  Data_Get_Struct(self, Rotoscope, config);

  config->tracepoint = rb_tracepoint_new(Qnil, EVENT_CALL | EVENT_RETURN, event_hook, (void *)config);

  rb_tracepoint_enable(config->tracepoint);

  printf("D");fflush(stdout);
  return Qnil;
}

VALUE rotoscope_stop_trace(VALUE self) {
  printf("E");fflush(stdout);
  Rotoscope* config;
  Data_Get_Struct(self, Rotoscope, config);

  printf("F");fflush(stdout);
  rb_tracepoint_disable(config->tracepoint);
  printf("G");fflush(stdout);
  if (config->log) fclose(config->log);
  printf("H");fflush(stdout);
  return Qnil;
}

VALUE cRotoscope;

VALUE rotoscope_trace(VALUE self, VALUE trace_args)
{
  VALUE args[2];
  args[0] = (RARRAY_LEN(trace_args) > 0) ? rb_ary_entry(trace_args, 0) : (VALUE)"/tmp/trace";
  args[1] = (RARRAY_LEN(trace_args) > 1) ? rb_ary_entry(trace_args, 1) : Qnil;

  printf("G");fflush(stdout);
  VALUE trace = rb_class_new_instance(2, args, cRotoscope);
  rotoscope_start_trace(trace);
  VALUE ret = rb_yield(Qundef);
  rotoscope_stop_trace(trace);
  printf("H");fflush(stdout);
  return ret;
}

void Init_rotoscope(void)
{
  cRotoscope = rb_define_class("Rotoscope", rb_cObject);
  rb_define_alloc_func(cRotoscope, allocate);
  rb_define_method(cRotoscope, "initialize", initialize, -2);
  rb_define_singleton_method(cRotoscope, "trace", (VALUE(*)(ANYARGS))rotoscope_trace, -2);
  rb_define_method(cRotoscope, "start_trace", (VALUE(*)(ANYARGS))rotoscope_start_trace, 0);
  rb_define_method(cRotoscope, "stop_trace", (VALUE(*)(ANYARGS))rotoscope_stop_trace, 0);
}
