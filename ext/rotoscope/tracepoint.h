#ifndef _INC_ROTOSCOPE_TRACEPOINT_H_
#define _INC_ROTOSCOPE_TRACEPOINT_H_

typedef struct
{
  const char *event;
  const char *method_name;
  const char *entity;
  const char *method_level;
  const char *filepath;
  unsigned int lineno;
} rs_tracepoint_t;

#endif
