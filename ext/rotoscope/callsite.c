#include "ruby.h"
#include "ruby/debug.h"

#include "callsite.h"

VALUE empty_ruby_string;

// Need the cfp field from this internal ruby structure.
struct rb_trace_arg_struct {
  // unused fields needed to make sure the cfp is at the
  // correct offset
  rb_event_flag_t unused1;
  void *unused2;

  void *cfp;

  // rest of fields are unused
};

size_t ruby_control_frame_size;

// We depend on MRI to store ruby control frames as an array
// to determine the control frame size, which is used here to
// get the caller's control frame
static void *caller_cfp(void *cfp)
{
    return ((char *)cfp) + ruby_control_frame_size;
}


static VALUE dummy(VALUE self, VALUE first)
{
  if (first == Qtrue) {
    rb_funcall(self, rb_intern("dummy"), 1, Qfalse);
  }
  return Qnil;
}

static void trace_control_frame_size(VALUE tpval, void *data)
{
  void **cfps = data;
  rb_trace_arg_t *trace_arg = rb_tracearg_from_tracepoint(tpval);

  if (cfps[0] == NULL) {
    cfps[0] = trace_arg->cfp;
  } else if (cfps[1] == NULL) {
    cfps[1] = trace_arg->cfp;
  }
}

rs_callsite_t c_callsite(rb_trace_arg_t *trace_arg)
{
  VALUE path = rb_tracearg_path(trace_arg);
  return (rs_callsite_t) {
    .filepath = NIL_P(path) ? empty_ruby_string : path,
    .lineno = FIX2INT(rb_tracearg_lineno(trace_arg)),
  };
}

rs_callsite_t ruby_callsite(rb_trace_arg_t *trace_arg)
{
  void *old_cfp = trace_arg->cfp;

  // Ruby uses trace_arg->cfp to get the path and line number
  trace_arg->cfp = caller_cfp(trace_arg->cfp);
  rs_callsite_t callsite = c_callsite(trace_arg);
  trace_arg->cfp = old_cfp;

  return callsite;
}

void init_callsite()
{
  empty_ruby_string = rb_str_new_literal("");
  RB_OBJ_FREEZE(empty_ruby_string);
  rb_global_variable(&empty_ruby_string);

  VALUE tmp_obj = rb_funcall(rb_cObject, rb_intern("new"), 0);
  rb_define_singleton_method(tmp_obj, "dummy", dummy, 1);

  char *cfps[2] = { NULL, NULL };
  VALUE tracepoint = rb_tracepoint_new(Qnil, RUBY_EVENT_C_CALL, trace_control_frame_size, &cfps);
  rb_tracepoint_enable(tracepoint);
  rb_funcall(tmp_obj, rb_intern("dummy"), 1, Qtrue);
  rb_tracepoint_disable(tracepoint);
  ruby_control_frame_size = (size_t)cfps[0] - (size_t)cfps[1];
}
