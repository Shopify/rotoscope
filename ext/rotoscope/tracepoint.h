#ifndef _INC_ROTOSCOPE_TRACEPOINT_H_
#define _INC_ROTOSCOPE_TRACEPOINT_H_

#include "ruby.h"

typedef struct rs_tracepoint_t {
  const char *event;
  VALUE entity;
  VALUE filepath;
  VALUE method_name;
  const char *method_level;
  unsigned int lineno;
} rs_tracepoint_t;

void rs_tracepoint_mark(rs_tracepoint_t *tracepoint);

#endif
