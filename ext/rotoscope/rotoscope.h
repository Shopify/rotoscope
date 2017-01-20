#ifndef _INC_ROTOSCOPE_H_
#define _INC_ROTOSCOPE_H_

#define EVENT_CALL   (RUBY_EVENT_CALL | RUBY_EVENT_C_CALL)
#define EVENT_RETURN (RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN)

typedef enum { false, true } bool;

typedef struct {
  const char* event;
  const char* method_name;
  const char* method_owner;
  const char* filepath;
  unsigned int lineno;
} rs_tracepoint_t;

#include "zlib.h"

typedef struct {
  gzFile log;
  VALUE tracepoint;
  VALUE blacklist;
  unsigned long blacklist_size;
} Rotoscope;

#endif
