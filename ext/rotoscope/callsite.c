#include "callsite.h"
#include <ruby.h>
#include <ruby/debug.h>
#include <stdbool.h>

static VALUE caller_frame(int *line, bool ruby_call) {
  VALUE frames[2] = {Qnil, Qnil};
  int lines[2] = {0, 0};

  int frame_index = ruby_call ? 1 : 0;

  // There is currently a bug in rb_profile_frames that
  // causes the start argument to effectively always
  // act as if it were 0, so we need to also get the top
  // frame. (https://bugs.ruby-lang.org/issues/14607)
  rb_profile_frames(0, frame_index + 1, frames, lines);

  *line = lines[frame_index];
  return frames[frame_index];
}

rs_callsite_t c_callsite(rb_trace_arg_t *trace_arg) {
  VALUE path = rb_tracearg_path(trace_arg);
  return (rs_callsite_t){
      .filepath = path, .lineno = FIX2INT(rb_tracearg_lineno(trace_arg)),
  };
}

rs_callsite_t ruby_callsite() {
  int line;
  VALUE frame = caller_frame(&line, true);

  return (rs_callsite_t){
      .filepath = rb_profile_frame_path(frame), .lineno = line,
  };
}
