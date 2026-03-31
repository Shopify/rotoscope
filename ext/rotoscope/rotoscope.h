#ifndef _INC_ROTOSCOPE_H_
#define _INC_ROTOSCOPE_H_

#include <ruby.h>
#include <unistd.h>

#include "callsite.h"
#include "stack.h"

#define EVENT_CALL (RUBY_EVENT_CALL | RUBY_EVENT_C_CALL)
#define EVENT_RETURN (RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN)

#define STACK_CAPACITY 500

typedef struct {
  VALUE self;
  VALUE tracepoint;
  pid_t pid;
  VALUE tid;
  bool tracing;
  rs_stack_t stack;
  rs_stack_frame_t *caller;
  rs_callsite_t callsite;
  VALUE trace_proc;
  // Native logger fields — when set, event_hook calls the log function
  // directly in C, bypassing the Ruby proc callback entirely.
  VALUE log_io;
  VALUE log_excludelist;
  VALUE log_self_obj;
  VALUE log_buffer;
} Rotoscope;

#endif
