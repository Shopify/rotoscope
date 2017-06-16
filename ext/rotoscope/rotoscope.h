#ifndef _INC_ROTOSCOPE_H_
#define _INC_ROTOSCOPE_H_

#include <unistd.h>
#include "stack.h"

#define EVENT_CALL (RUBY_EVENT_CALL | RUBY_EVENT_C_CALL)
#define EVENT_RETURN (RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN)

#define RS_CSV_VALUES(trace) \
    trace.event, \
    trace.entity, \
    trace.method_name, \
    trace.method_level, \
    trace.filepath, \
    trace.lineno
#define RS_CSV_HEADER "event,entity,method_name,method_level,filepath,lineno\n"
#define RS_CSV_FORMAT "%s,\"%s\",\"%s\",%s,\"%s\",%d\n"

#define RS_FLATTENED_CSV_VALUES(frame) \
    trace.entity, \
    trace.method_name, \
    trace.method_level, \
    trace.filepath, \
    trace.lineno, \
    frame.caller->entity, \
    frame.caller->method_name, \
    frame.caller->method_level
#define RS_FLATTENED_CSV_HEADER "entity,method_name,method_level,filepath,lineno, caller_entity, caller_method_name, caller_method_level\n"
#define RS_FLATTENED_CSV_FORMAT "\"%s\",\"%s\",%s,\"%s\",%d,\"%s\",\"%s\",%s\n"

#define CLASS_METHOD "class"
#define INSTANCE_METHOD "instance"

#define UNKNOWN_FILE_PATH "Unknown"

#define STACK_CAPACITY 500

typedef enum {
  RS_CLOSED = 0,
  RS_OPEN,
  RS_TRACING,
} rs_state;

typedef struct
{
  FILE *log;
  VALUE log_path;
  VALUE tracepoint;
  const char **blacklist;
  unsigned long blacklist_size;
  bool flatten_output;
  pid_t pid;
  rs_state state;
  rs_stack_t stack;
} Rotoscope;

typedef struct
{
  VALUE name;
  const char *method_level;
} rs_class_desc_t;

#endif
