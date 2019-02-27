#include "callsite.h"
#include <ruby.h>
#include <ruby/debug.h>
#include <stdbool.h>

static VALUE caller_frame(int *line, bool ruby_call) {
  VALUE frames[2] = {Qnil, Qnil};
  int lines[2] = {0, 0};

  // At this point, the top ruby stack frame is for the method
  // being called, so we want to skip that frame and get
  // the caller location. This is why we use 1 for ruby calls.
  //
  // However, rb_profile_frames also automatically skips over
  // non-ruby stack frames, so we don't want to have to skip
  // over any extra stack frames for a C call.
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
  int line;
  VALUE frame = caller_frame(&line, false);
  return (rs_callsite_t){
      .filepath = rb_tracearg_path(trace_arg),
      .lineno = FIX2INT(rb_tracearg_lineno(trace_arg)),
      .method_name = rb_profile_frame_method_name(frame),
      .singleton_p = rb_profile_frame_singleton_method_p(frame),
  };
}

rs_callsite_t ruby_callsite() {
  int line;
  VALUE frame = caller_frame(&line, true);

  return (rs_callsite_t){
      .filepath = rb_profile_frame_path(frame),
      .lineno = line,
      .method_name = rb_profile_frame_method_name(frame),
      .singleton_p = rb_profile_frame_singleton_method_p(frame),
  };
}
