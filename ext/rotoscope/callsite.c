#include "callsite.h"
#include <ruby.h>
#include <ruby/debug.h>

VALUE empty_ruby_string;

rs_callsite_t c_callsite(rb_trace_arg_t *trace_arg) {
  VALUE path = rb_tracearg_path(trace_arg);
  return (rs_callsite_t){
      .filepath = NIL_P(path) ? empty_ruby_string : path,
      .lineno = FIX2INT(rb_tracearg_lineno(trace_arg)),
  };
}

rs_callsite_t ruby_callsite() {
  VALUE frames[2];
  int lines[2];
  // There is currently a bug in rb_profile_frames that
  // causes the start argument to effectively always
  // act as if it were 0, so we need to also get the top
  // frame.
  if (rb_profile_frames(0, 2, frames, lines) < 2) {
    return (rs_callsite_t){
        .filepath = empty_ruby_string, .lineno = 0,
    };
  }

  return (rs_callsite_t){
      .filepath = rb_profile_frame_path(frames[1]), .lineno = lines[1],
  };
}

void init_callsite() {
  empty_ruby_string = rb_str_new_literal("");
  RB_OBJ_FREEZE(empty_ruby_string);
  rb_global_variable(&empty_ruby_string);
}
