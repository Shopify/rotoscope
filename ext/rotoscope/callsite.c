#include "callsite.h"

#include <ruby.h>
#include <ruby/debug.h>
#include <stdbool.h>

static VALUE caller_frame(int *line) {
  VALUE frame = Qnil;
  int frame_line = 0;

  // For ruby calls, the top frame is the callee — skip it to get the caller.
  // Ruby 4.0+ fixed rb_profile_frames start argument (ruby-lang bug #14607).
  rb_profile_frames(1, 1, &frame, &frame_line);

  *line = frame_line;
  return frame;
}

rs_callsite_t c_callsite(rb_trace_arg_t *trace_arg) {
  return (rs_callsite_t){
      .filepath = rb_tracearg_path(trace_arg),
      .lineno = FIX2INT(rb_tracearg_lineno(trace_arg)),
      .method_name = Qnil,
      .singleton_p = Qnil,
      .profile_frame = Qnil,
  };
}

rs_callsite_t ruby_callsite() {
  int line;
  VALUE frame = caller_frame(&line);

  return (rs_callsite_t){
      .filepath = rb_profile_frame_path(frame),
      .lineno = line,
      .method_name = Qnil,  // filled from stack or profile_frame in event_hook
      .singleton_p = Qnil,
      .profile_frame = frame,
  };
}
