#ifndef _INC_ROTOSCOPE_H_
#define _INC_ROTOSCOPE_H_

#include "unistd.h"

#define EVENT_CALL (RUBY_EVENT_CALL | RUBY_EVENT_C_CALL)
#define EVENT_RETURN (RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN)

#define RS_CSV_VALUES(trace) trace.event, trace.entity, trace.method_name, trace.method_level, trace.filepath, trace.lineno
#define RS_CSV_HEADER "event,entity,method_name,method_level,filepath,lineno\n"
#define RS_CSV_FORMAT "%s,\"%s\",\"%s\",%s,\"%s\",%d\n"

#define CLASS_METHOD "class"
#define INSTANCE_METHOD "instance"

#define UNKNOWN_FILE_PATH "Unknown"

typedef enum {
  RS_OPEN = 1,
  RS_CLOSED
} rs_state;

typedef struct
{
  const char *event;
  const char *method_name;
  const char *entity;
  const char *method_level;
  const char *filepath;
  unsigned int lineno;
} rs_tracepoint_t;

typedef struct
{
  FILE *log;
  char *log_path;
  VALUE tracepoint;
  VALUE blacklist;
  unsigned long blacklist_size;
  pid_t pid;
  rs_state state;
} Rotoscope;

typedef struct
{
  const char *name;
  const char *method_level;
} rs_class_desc_t;

typedef struct
{
  const char *filepath;
  unsigned int lineno;
} rs_callsite_t;

#endif
