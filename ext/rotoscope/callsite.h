#ifndef _INC_CALLSITE_H_
#define _INC_CALLSITE_H_

#include <ruby.h>
#include <ruby/debug.h>

typedef struct {
  VALUE filepath;
  unsigned int lineno;
  VALUE method_name;
  VALUE singleton_p;
} rs_callsite_t;

rs_callsite_t c_callsite(rb_trace_arg_t *trace_arg);
rs_callsite_t ruby_callsite();

#endif
