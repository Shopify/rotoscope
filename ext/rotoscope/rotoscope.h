#ifndef _INC_ROTOSCOPE_H_
#define _INC_ROTOSCOPE_H_

#include <unistd.h>
#include "stack.h"

#define EVENT_CALL (RUBY_EVENT_CALL | RUBY_EVENT_C_CALL)
#define EVENT_RETURN (RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN)

#define STACK_CAPACITY 500

typedef struct {
  VALUE self;            // Rotoscope
  VALUE tracepoint;      // Tracepoint
  VALUE whitelist_path;  // String
  VALUE trace_proc;      // Proc, called for every call event (that isn't filtered out)

  pid_t pid;
  unsigned long tid;
  bool tracing;
  rs_stack_t stack;
  rs_stack_frame_t *caller;
  rs_callsite_t callsite;
} Rotoscope;

#endif
