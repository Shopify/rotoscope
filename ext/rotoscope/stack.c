#include "stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ruby.h"

static void resize_buffer(rs_stack_t *stack) {
  unsigned int newsize = stack->capacity * 2;
  rs_stack_frame_t *resized_contents =
      REALLOC_N(stack->contents, rs_stack_frame_t, newsize);
  stack->contents = resized_contents;
  stack->capacity = newsize;
}

bool rs_stack_full(rs_stack_t *stack) {
  return stack->top >= stack->capacity - 1;
}

bool rs_stack_empty(rs_stack_t *stack) { return stack->top < 0; }

void rs_stack_push(rs_stack_t *stack, rs_stack_frame_t new_frame) {
  if (rs_stack_full(stack)) {
    resize_buffer(stack);
  }

  stack->contents[++stack->top] = new_frame;
}

rs_stack_frame_t rs_stack_pop(rs_stack_t *stack) {
  if (rs_stack_empty(stack)) {
    fprintf(stderr, "Stack is empty!\n");
    exit(1);
  }

  return stack->contents[stack->top--];
}

rs_stack_frame_t *rs_stack_peek(rs_stack_t *stack) {
  if (rs_stack_empty(stack)) {
    return NULL;
  }

  return &stack->contents[stack->top];
}

void rs_stack_reset(rs_stack_t *stack) { stack->top = -1; }

void rs_stack_free(rs_stack_t *stack) {
  xfree(stack->contents);
  stack->contents = NULL;
  stack->top = -1;
  stack->capacity = 0;
}

void rs_stack_init(rs_stack_t *stack, unsigned int capacity) {
  rs_stack_frame_t *contents = ALLOC_N(rs_stack_frame_t, capacity);
  stack->contents = contents;
  stack->capacity = capacity;
  stack->top = -1;
}

void rs_stack_mark(rs_stack_t *stack) {
  for (int i = 0; i <= stack->top; i++) {
    rs_stack_frame_t *frame = &stack->contents[i];
    rs_method_desc_mark(&frame->method);
  }
}
