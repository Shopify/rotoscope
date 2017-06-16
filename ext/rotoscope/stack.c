#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stack.h"

rs_stack_frame_t root_context;

bool stack_full(rs_stack_t *stack)
{
  return stack->top >= stack->capacity - 1;
}

bool stack_empty(rs_stack_t *stack)
{
  return stack->top < 0;
}

static void resize_buffer(rs_stack_t *stack)
{
  unsigned int newsize = stack->capacity * 2;
  rs_stack_frame_t *new_contents = (rs_stack_frame_t *)malloc(sizeof(rs_stack_frame_t) * newsize);
  memcpy(new_contents, stack->contents, sizeof(rs_stack_frame_t) * stack->capacity);
  stack->capacity = newsize;
  stack->contents = new_contents;
}

rs_stack_frame_t stack_push(rs_stack_t *stack, rs_tracepoint_t trace)
{
  if (stack_full(stack))
  {
    resize_buffer(stack);
  }

  rs_stack_frame_t new_frame = (rs_stack_frame_t){
    .event = trace.event,
    .method_name = trace.method_name,
    .entity = trace.entity,
    .method_level = trace.method_level,
    .filepath = trace.filepath,
    .lineno = trace.lineno,
    .caller = stack_peek(stack),
  };

  stack->contents[++stack->top] = new_frame;
  return new_frame;
}

rs_stack_frame_t stack_pop(rs_stack_t *stack)
{
  if (stack_empty(stack))
  {
    fprintf(stderr, "Stack has nothing to pop!\n");
    exit(1);
  }

  return stack->contents[stack->top--];
}

rs_stack_frame_t *stack_peek(rs_stack_t *stack)
{
  if (stack_empty(stack))
  {
    return &root_context;
  }

  return &stack->contents[stack->top];
}

void reset_stack(rs_stack_t *stack, unsigned int capacity)
{
  free_stack(stack);
  init_stack(stack, capacity);
}

void free_stack(rs_stack_t *stack)
{
  free(stack->contents);
  stack->contents = NULL;
  stack->top = -1;
  stack->capacity = 0;
}

void init_stack(rs_stack_t *stack, unsigned int capacity)
{
  rs_stack_frame_t *contents;
  root_context = (rs_stack_frame_t) {
    .method_level = "<UNKNOWN>",
    .method_name = "<UNKNOWN>",
    .entity = "<ROOT>"
  };

  contents = (rs_stack_frame_t *)malloc(sizeof(rs_stack_frame_t) * capacity);
  if (contents == NULL) {
    fprintf(stderr, "Not enough memory to allocate stack\n");
    exit(1);
  }

  stack->contents = contents;
  stack->capacity = capacity;
  stack->top = -1;
}
