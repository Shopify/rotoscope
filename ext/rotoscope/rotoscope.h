#ifndef _INC_ROTOSCOPE_H_
#define _INC_ROTOSCOPE_H_

#include <unistd.h>
#include "stack.h"
#include "strmemo.h"

#define EVENT_CALL (RUBY_EVENT_CALL | RUBY_EVENT_C_CALL)
#define EVENT_RETURN (RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN)
#define METHOD_CALL_P(event) event &EVENT_CALL
#define METHOD_RETURN_P(event) event &EVENT_RETURN

#define CLASS_METHOD "class"
#define INSTANCE_METHOD "instance"

#define VM_BLOCK_PREFIX "block "

#define STACK_CAPACITY 500
#define LOG_BUFFER_SIZE 1000

// clang-format off
#define _RS_COMMON_CSV_HEADER "entity,method_name,method_level,filepath,lineno"
#define _RS_COMMON_CSV_FORMAT "\"%s\",\"%s\",%s,\"%s\",%d"
#define _RS_COMMON_CSV_VALUES(trace)  \
  StringValueCStr(trace.entity),      \
  StringValueCStr(trace.method_name), \
  trace.method_level,                 \
  StringValueCStr(trace.filepath),    \
  trace.lineno

#define RS_CSV_HEADER "event," _RS_COMMON_CSV_HEADER
#define RS_CSV_FORMAT "%s," _RS_COMMON_CSV_FORMAT
#define RS_CSV_VALUES(trace) trace.event, _RS_COMMON_CSV_VALUES(trace)

#define RS_FLATTENED_CSV_HEADER \
  _RS_COMMON_CSV_HEADER ",caller_entity,caller_method_name,caller_method_level"
#define RS_FLATTENED_CSV_FORMAT _RS_COMMON_CSV_FORMAT ",\"%s\",\"%s\",%s"
#define RS_FLATTENED_CSV_VALUES(frame)           \
  _RS_COMMON_CSV_VALUES(frame.tp),               \
  StringValueCStr(frame.caller->tp.entity),      \
  StringValueCStr(frame.caller->tp.method_name), \
  frame.caller->tp.method_level
// clang-format on

typedef enum {
  RS_CLOSED = 0,
  RS_OPEN,
  RS_TRACING,
} rs_state;

typedef struct {
  FILE *log;
  VALUE log_path;
  VALUE tracepoint;
  const char **blacklist;
  unsigned long blacklist_size;
  bool flatten_output;
  pid_t pid;
  unsigned long tid;
  rs_state state;
  rs_stack_t stack;
  rs_strmemo_t *call_memo;
} Rotoscope;

typedef struct {
  VALUE name;
  const char *method_level;
} rs_class_desc_t;

#endif
