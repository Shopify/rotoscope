#include "callsite.h"
#include <ruby.h>
#include <ruby/debug.h>

rs_callsite_t c_callsite(rb_trace_arg_t *trace_arg) {
  VALUE path = rb_tracearg_path(trace_arg);
  return (rs_callsite_t){
      .filepath = path, .lineno = FIX2INT(rb_tracearg_lineno(trace_arg)),
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
        .filepath = Qnil, .lineno = 0,
    };
  }

  return (rs_callsite_t){
      .filepath = rb_profile_frame_path(frames[1]), .lineno = lines[1],
  };
}
