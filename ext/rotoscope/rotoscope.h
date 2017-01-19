#ifndef _INC_ROTOSCOPE_H_
#define _INC_ROTOSCOPE_H_

#define EVENT_CALL   (RUBY_EVENT_CALL | RUBY_EVENT_C_CALL)
#define EVENT_RETURN (RUBY_EVENT_RETURN | RUBY_EVENT_C_RETURN)
#define MAX_LOG_SIZE 100000000 /* bytes */
#define MIN_CSV_BUFSIZE 150
#define MAX_CSV_BUFSIZE 500

typedef enum { false, true } bool;

typedef struct {
  const char* event;
  const char* method_name;
  const char* method_owner;
  const char* filepath;
  int lineno;
} rs_tracepoint_t;

typedef struct {
  FILE* log;
  VALUE tracepoint;
  VALUE blacklist;
} Rotoscope;

#endif
