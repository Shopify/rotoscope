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
void rs_stack_mark(rs_stack_t *stack);

// Hot-path stack operations inlined for event_hook performance
void rs_stack_resize(rs_stack_t *stack);

static inline bool rs_stack_empty(rs_stack_t *stack) {
  return stack->top < 0;
}

static inline bool rs_stack_full(rs_stack_t *stack) {
  return stack->top >= stack->capacity - 1;
}

static inline void rs_stack_push(rs_stack_t *stack, rs_stack_frame_t frame) {
  if (rs_stack_full(stack)) {
    rs_stack_resize(stack);
  }
  stack->contents[++stack->top] = frame;
}

static inline rs_stack_frame_t rs_stack_pop(rs_stack_t *stack) {
  return stack->contents[stack->top--];
}

static inline rs_stack_frame_t *rs_stack_peek(rs_stack_t *stack) {
  if (rs_stack_empty(stack)) {
    return NULL;
  }
  return &stack->contents[stack->top];
}

#endif
