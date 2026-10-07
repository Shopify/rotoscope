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
  unsigned long tid;
  bool tracing;
  rs_stack_t stack;
  // Copy, not a pointer into `stack`: pushing may realloc the stack buffer.
  rs_stack_frame_t caller;
  bool has_caller;
  rs_callsite_t callsite;
  VALUE trace_proc;
} Rotoscope;

#endif
