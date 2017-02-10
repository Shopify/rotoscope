#include "ruby.h"
#include "ruby/debug.h"
#include "ruby/intern.h"
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include "rotoscope.h"
#include "zlib.h"

VALUE cRotoscope;
// recursive with singleton2str
static rs_class_desc_t class2str(VALUE klass);

static int write_csv_header(gzFile *log)
{
  return gzprintf(*log, RS_CSV_HEADER);
}

static const char *evflag2name(rb_event_flag_t evflag)
{
  switch (evflag)
  {
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

static bool rejected_path(const char *path, Rotoscope *config)
{
  unsigned long i;
  Check_Type(config->blacklist, T_ARRAY);

  for (i = 0; i < config->blacklist_size; i++)
  {
    if (strstr(path, RSTRING_PTR(RARRAY_AREF(config->blacklist, i))))
      return true;
  }

  return false;
}

static char *singleton2str(VALUE klass)
{
  VALUE obj = rb_iv_get(klass, "__attached__");
  if (RB_TYPE_P(obj, T_MODULE) || RB_TYPE_P(obj, T_CLASS))
  {
    // singleton of a class
    VALUE cached_lookup = rb_class_path_cached(obj);
    VALUE name = (NIL_P(cached_lookup)) ? rb_class_name(obj) : cached_lookup;
    return RSTRING_PTR(name);
  }
  else
  {
    // singleton of an instance
    VALUE real_klass;
    VALUE ancestors = rb_mod_ancestors(klass);
    if (RARRAY_LEN(ancestors) > 0 && !NIL_P(real_klass = rb_ary_entry(ancestors, 1)))
    {
      VALUE cached_lookup = rb_class_path_cached(real_klass);
      if (RTEST(cached_lookup))
      {
        return RSTRING_PTR(cached_lookup);
      }
      else
      {
        return RSTRING_PTR(rb_class_name(real_klass));
      }
    }
    // fallback in case we can't come up with a name
    // based on the ancestors
    return RSTRING_PTR(rb_any_to_s(klass));
  }
}

static rs_class_desc_t class2str(VALUE klass)
{
  rs_class_desc_t real_class;
  real_class.method_level = INSTANCE_METHOD;

  VALUE cached_lookup = rb_class_path_cached(klass);
  if (RTEST(cached_lookup))
  {
    real_class.name = RSTRING_PTR(cached_lookup);
  }
  else
  {
    if (FL_TEST(klass, FL_SINGLETON))
    {
      real_class.method_level = SINGLETON_METHOD;
      real_class.name = singleton2str(klass);
    }
    else
    {
      real_class.name = RSTRING_PTR(rb_any_to_s(klass));
    }
  }

  return real_class;
}

static const char *tracearg_path(rb_trace_arg_t *trace_arg)
{
  VALUE path = rb_tracearg_path(trace_arg);
  return RTEST(path) ? RSTRING_PTR(path) : "";
}

static rs_class_desc_t tracearg_class(rb_trace_arg_t *trace_arg)
{
  VALUE klass;
  VALUE self = rb_tracearg_self(trace_arg);

  if (RB_TYPE_P(self, T_MODULE) || RB_TYPE_P(self, T_OBJECT) || RB_TYPE_P(self, T_CLASS))
  {
    klass = RBASIC_CLASS(self);
  }
  else
  {
    klass = rb_tracearg_defined_class(trace_arg);
  }

  return class2str(klass);
}

static rs_tracepoint_t extract_full_tracevals(rb_trace_arg_t *trace_arg)
{
  rs_class_desc_t method_owner = tracearg_class(trace_arg);

  return (rs_tracepoint_t){
      .event = evflag2name(rb_tracearg_event_flag(trace_arg)),
      .entity = method_owner.name,
      .method_name = RSTRING_PTR(rb_sym2str(rb_tracearg_method_id(trace_arg))),
      .filepath = tracearg_path(trace_arg),
      .lineno = FIX2INT(rb_tracearg_lineno(trace_arg)),
      .method_level = method_owner.method_level};
}

static void event_hook(VALUE tpval, void *data)
{
  Rotoscope *config = (Rotoscope *)data;
  rb_trace_arg_t *trace_arg = rb_tracearg_from_tracepoint(tpval);
  const char *trace_path = tracearg_path(trace_arg);

  if (rejected_path(trace_path, config))
    return;

  rs_tracepoint_t trace = extract_full_tracevals(trace_arg);
  gzprintf(config->log, RS_CSV_FORMAT, RS_CSV_VALUES(trace));
}

static void close_gz_handle(Rotoscope *config)
{
  if (config->log)
  {
    gzclose(config->log);
    config->log = NULL;
  }
}

static void rs_gc_mark(Rotoscope *config)
{
  rb_gc_mark(config->tracepoint);
  rb_gc_mark(config->blacklist);
}

void rs_dealloc(Rotoscope *config)
{
  close_gz_handle(config);
  free(config);
}

static VALUE rs_alloc(VALUE klass)
{
  Rotoscope *config;
  return Data_Make_Struct(klass, Rotoscope, rs_gc_mark, rs_dealloc, config);
}

static Rotoscope *get_config(VALUE self)
{
  Rotoscope *config;
  Data_Get_Struct(self, Rotoscope, config);
  return config;
}

VALUE initialize(int argc, VALUE *argv, VALUE self)
{
  Rotoscope *config = get_config(self);
  VALUE output_path;

  rb_scan_args(argc, argv, "11", &output_path, &config->blacklist);
  if (NIL_P(config->blacklist))
    config->blacklist = rb_ary_new();
  Check_Type(config->blacklist, T_ARRAY);
  config->blacklist_size = RARRAY_LEN(config->blacklist);

  Check_Type(output_path, T_STRING);
  const char *path = RSTRING_PTR(output_path);
  config->log = gzopen(path, "w");
  if (config->log == NULL)
  {
    fprintf(stderr, "\nERROR: Failed to open file handle at %s (%s)\n", path, strerror(errno));
    exit(1);
  }

  write_csv_header(&config->log);

  return self;
}

VALUE rotoscope_start_trace(VALUE self)
{
  Rotoscope *config = get_config(self);
  config->tracepoint = rb_tracepoint_new(Qnil, EVENT_CALL | EVENT_RETURN, event_hook, (void *)config);
  rb_tracepoint_enable(config->tracepoint);
  return Qnil;
}

VALUE rotoscope_stop_trace(VALUE self)
{
  Rotoscope *config = get_config(self);
  if (rb_tracepoint_enabled_p(config->tracepoint))
  {
    rb_tracepoint_disable(config->tracepoint);
  }
  return Qnil;
}

VALUE rotoscope_mark(VALUE self)
{
  Rotoscope *config = get_config(self);
  gzprintf(config->log, "---\n");
  return Qnil;
}
VALUE rotoscope_close(VALUE self)
{
  Rotoscope *config = get_config(self);
  close_gz_handle(config);
  return Qnil;
}

VALUE rotoscope_trace(VALUE self)
{
  rotoscope_start_trace(self);
  return rb_ensure(rb_yield, Qundef, rotoscope_stop_trace, self);
}

void Init_rotoscope(void)
{
  cRotoscope = rb_define_class("Rotoscope", rb_cObject);
  rb_define_alloc_func(cRotoscope, rs_alloc);
  rb_define_method(cRotoscope, "initialize", initialize, -1);
  rb_define_method(cRotoscope, "trace", (VALUE(*)(ANYARGS))rotoscope_trace, 0);
  rb_define_method(cRotoscope, "mark", (VALUE(*)(ANYARGS))rotoscope_mark, 0);
  rb_define_method(cRotoscope, "close", (VALUE(*)(ANYARGS))rotoscope_close, 0);
  rb_define_method(cRotoscope, "start_trace", (VALUE(*)(ANYARGS))rotoscope_start_trace, 0);
  rb_define_method(cRotoscope, "stop_trace", (VALUE(*)(ANYARGS))rotoscope_stop_trace, 0);
}
