#ifndef _INC_CALLSITE_H_
#define _INC_CALLSITE_H_

typedef struct {
  VALUE filepath;
  unsigned int lineno;
} rs_callsite_t;

void init_callsite();

rs_callsite_t c_callsite(rb_trace_arg_t *trace_arg);
rs_callsite_t ruby_callsite(rb_trace_arg_t *trace_arg);

#endif
