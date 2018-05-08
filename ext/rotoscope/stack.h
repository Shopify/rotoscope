#ifndef _INC_ROTOSCOPE_STACK_H_
#define _INC_ROTOSCOPE_STACK_H_
#include <stdbool.h>
#include "method_desc.h"

typedef struct {
  rs_method_desc_t method;
} rs_stack_frame_t;

typedef struct {
  int capacity;
  int top;
  rs_stack_frame_t *contents;
} rs_stack_t;

void rs_stack_init(rs_stack_t *stack, unsigned int capacity);
void rs_stack_reset(rs_stack_t *stack);
void rs_stack_free(rs_stack_t *stack);
void rs_stack_push(rs_stack_t *stack, rs_stack_frame_t frame);
bool rs_stack_empty(rs_stack_t *stack);
bool rs_stack_full(rs_stack_t *stack);
rs_stack_frame_t rs_stack_pop(rs_stack_t *stack);
rs_stack_frame_t *rs_stack_peek(rs_stack_t *stack);
void rs_stack_mark(rs_stack_t *stack);

#endif
