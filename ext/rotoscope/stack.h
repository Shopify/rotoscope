#ifndef _INC_ROTOSCOPE_STACK_H_
#define _INC_ROTOSCOPE_STACK_H_
#include <stdbool.h>
#include "tracepoint.h"

#define UNKNOWN_STR "<UNKNOWN>"

typedef struct rs_stack_frame_t
{
  struct rs_tracepoint_t tp;
  struct rs_stack_frame_t *caller;
} rs_stack_frame_t;

typedef struct
{
  int capacity;
  int top;
  rs_stack_frame_t *contents;
} rs_stack_t;

void stack_init(rs_stack_t *stack, unsigned int capacity);
void stack_reset(rs_stack_t *stack, unsigned int capacity);
void stack_free(rs_stack_t *stack);
rs_stack_frame_t stack_push(rs_stack_t *stack, rs_tracepoint_t trace);
bool stack_empty(rs_stack_t *stack);
bool stack_full(rs_stack_t *stack);
rs_stack_frame_t stack_pop(rs_stack_t *stack);
rs_stack_frame_t *stack_peek(rs_stack_t *stack);

#endif
