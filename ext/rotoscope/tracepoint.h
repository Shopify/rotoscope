#ifndef _INC_ROTOSCOPE_TRACEPOINT_H_
#define _INC_ROTOSCOPE_TRACEPOINT_H_

#include <ruby.h>
#include <ruby/debug.h>
#include <stdbool.h>

typedef struct rs_raw_tracepoint_t {
  VALUE method_id;
  VALUE self;
} rs_raw_tracepoint_t;

rs_raw_tracepoint_t rs_raw_from_tracepoint(VALUE tracepoint);
void rs_raw_tracepoint_mark(rs_raw_tracepoint_t *tracepoint);
bool rs_raw_tracepoint_cmp(rs_raw_tracepoint_t *tp1, rs_raw_tracepoint_t *tp2);

typedef struct rs_tracepoint_t {
  const char *event;
  VALUE entity;
  VALUE filepath;
  VALUE method_name;
  const char *method_level;
  unsigned int lineno;
  rs_raw_tracepoint_t raw;
} rs_tracepoint_t;

void rs_tracepoint_mark(rs_tracepoint_t *tracepoint);

#endif
