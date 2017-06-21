#ifndef _INC_ROTOSCOPE_TRACEPOINT_H_
#define _INC_ROTOSCOPE_TRACEPOINT_H_

typedef struct
{
  const char *event;
  const char *entity;
  const char *filepath;
  const char *method_name;
  const char *method_level;
  unsigned int lineno;
} rs_tracepoint_args;

typedef struct rs_tracepoint_t
{
  const char *event;
  const char *entity;
  const char *filepath;
  const char *method_name;
  const char *method_level;
  unsigned int lineno;
} rs_tracepoint_t;

void rs_tracepoint_free(rs_tracepoint_t *tracepoint);

rs_tracepoint_t *rs_tracepoint_init(rs_tracepoint_args args);

#endif
