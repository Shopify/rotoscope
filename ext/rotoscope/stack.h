#ifndef _INC_ROTOSCOPE_STACK_H_
#define _INC_ROTOSCOPE_STACK_H_
#include <stdbool.h>
#include "tracepoint.h"

typedef struct rs_stack_frame_t
{
  const char *event;
  const char *method_name;
  const char *entity;
  const char *method_level;
  const char *filepath;
  unsigned int lineno;
  struct rs_stack_frame_t *caller;
} rs_stack_frame_t;

typedef struct
{
  int capacity;
  int top;
  rs_stack_frame_t *contents;
} rs_stack_t;

void init_stack(rs_stack_t *stack, unsigned int capacity);
void reset_stack(rs_stack_t *stack, unsigned int capacity);
void free_stack(rs_stack_t *stack);
rs_stack_frame_t stack_push(rs_stack_t *stack, rs_tracepoint_t trace);
bool stack_empty(rs_stack_t *stack);
bool stack_full(rs_stack_t *stack);
rs_stack_frame_t stack_pop(rs_stack_t *stack);
rs_stack_frame_t *stack_peek(rs_stack_t *stack);

#endif
