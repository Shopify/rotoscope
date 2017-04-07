#ifndef _INC_ROTOSCOPE_H_
#define _INC_ROTOSCOPE_H_

#include "unistd.h"

#define EVENT_CALL (RUBY_EVENT_CALL | RUBY_EVENT_C_CALL)
#define EVENT_RETURN (RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN)

#define RS_CSV_VALUES(trace) \
    trace.event, \
    StringValueCStr(trace.entity), \
    StringValueCStr(trace.method_name), \
    trace.method_level, \
    StringValueCStr(trace.filepath), \
    trace.lineno
#define RS_CSV_HEADER "event,entity,method_name,method_level,filepath,lineno\n"
#define RS_CSV_FORMAT "%s,\"%s\",\"%s\",%s,\"%s\",%d\n"

#define CLASS_METHOD "class"
#define INSTANCE_METHOD "instance"

#define UNKNOWN_FILE_PATH "Unknown"

typedef enum {
  RS_CLOSED = 0,
  RS_OPEN,
  RS_TRACING,
} rs_state;

typedef struct
{
  const char *event;
  VALUE method_name;
  VALUE entity;
  const char *method_level;
  VALUE filepath;
  unsigned int lineno;
} rs_tracepoint_t;

typedef struct
{
  FILE *log;
  VALUE log_path;
  VALUE tracepoint;
  const char **blacklist;
  unsigned long blacklist_size;
  pid_t pid;
  rs_state state;
} Rotoscope;

typedef struct
{
  VALUE name;
  const char *method_level;
} rs_class_desc_t;

#endif
