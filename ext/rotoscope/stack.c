#include "stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ruby.h"
#include "tracepoint.h"

static void insert_root_node(rs_stack_t *stack) {
  VALUE rb_unknown_str = rb_str_new_cstr(UNKNOWN_STR);
  rs_tracepoint_t root_trace =
      (rs_tracepoint_t){.event = UNKNOWN_STR,
                        .entity = rb_str_new_cstr("<ROOT>"),
                        .filepath = rb_unknown_str,
                        .method_name = rb_unknown_str,
                        .method_level = UNKNOWN_STR,
                        .lineno = 0};
  rs_stack_push(stack, root_trace, false);
}

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

rs_stack_frame_t rs_stack_push(rs_stack_t *stack, rs_tracepoint_t trace,
                               bool blacklisted) {
  if (rs_stack_full(stack)) {
    resize_buffer(stack);
  }

  rs_stack_frame_t new_frame =
      (rs_stack_frame_t){.tp = trace, .blacklisted = blacklisted};

  stack->contents[++stack->top] = new_frame;
  return new_frame;
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
    fprintf(stderr, "Stack is empty!\n");
    exit(1);
  }

  return &stack->contents[stack->top];
}

rs_stack_frame_t *rs_stack_below(rs_stack_t *stack, rs_stack_frame_t *frame) {
  if (frame < stack->contents || frame > stack->contents + stack->top) {
    fprintf(stderr, "Invalid stack frame (bottom: %p, frame: %p, top: %p)\n",
            stack->contents, frame, &stack->contents[stack->top]);
    exit(1);
  } else if (stack->contents == frame) {
    return NULL;
  } else {
    return frame - 1;
  }
}

void rs_stack_reset(rs_stack_t *stack) {
  stack->top = -1;
  insert_root_node(stack);
}

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

  insert_root_node(stack);
}

void rs_stack_mark(rs_stack_t *stack) {
  for (int i = 0; i <= stack->top; i++) {
    rs_stack_frame_t frame = stack->contents[i];
    rs_tracepoint_mark(&frame.tp);
  }
}
