#ifndef _INC_ROTOSCOPE_H_
#define _INC_ROTOSCOPE_H_

#define EVENT_CALL   (RUBY_EVENT_CALL | RUBY_EVENT_C_CALL)
#define EVENT_RETURN (RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN)

#define RS_CSV_VALUES(trace) trace.event, trace.method_owner, trace.method_name, trace.filepath, trace.lineno
#define RS_CSV_HEADER "event,method_owner,method_name,filepath,lineno\n"
#define RS_CSV_FORMAT "%s,\"%s\",\"%s\",\"%s\",%d\n"

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
