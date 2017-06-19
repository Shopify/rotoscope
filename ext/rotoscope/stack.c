#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stack.h"

static void check_buffer(rs_stack_frame_t *contents)
{
  if (contents == NULL)
  {
    fprintf(stderr, "Not enough memory to allocate stack\n");
    exit(1);
  }
}

static void insert_root_node(rs_stack_t *stack)
{
  rs_tracepoint_t root_trace = (rs_tracepoint_t) {
      .entity = "<ROOT>",
      .event = UNKNOWN_STR,
      .method_name = UNKNOWN_STR,
      .method_level = UNKNOWN_STR,
      .filepath = UNKNOWN_STR,
      .lineno = 0
  };

  stack_push(stack, root_trace);
}

static void resize_buffer(rs_stack_t *stack)
{
  unsigned int newsize = stack->capacity * 2;
  stack->contents = realloc(stack->contents, sizeof(rs_stack_frame_t) * newsize);
  check_buffer(stack->contents);

  stack->capacity = newsize;
}

bool stack_full(rs_stack_t *stack)
{
  return stack->top >= stack->capacity - 1;
}

bool stack_empty(rs_stack_t *stack)
{
  return stack->top < 0;
}

rs_stack_frame_t stack_push(rs_stack_t *stack, rs_tracepoint_t trace)
{
  if (stack_full(stack))
  {
    resize_buffer(stack);
  }

  rs_stack_frame_t new_frame = (rs_stack_frame_t){
    .tp = trace,
    .caller = stack_peek(stack)
  };

  stack->contents[++stack->top] = new_frame;
  return new_frame;
}

rs_stack_frame_t stack_pop(rs_stack_t *stack)
{
  if (stack_empty(stack))
  {
    fprintf(stderr, "Stack is empty!\n");
    exit(1);
  }

  return stack->contents[stack->top--];
}

rs_stack_frame_t *stack_peek(rs_stack_t *stack)
{
  if (stack_empty(stack))
  {
    return NULL;
  }

  return &stack->contents[stack->top];
}

void stack_reset(rs_stack_t *stack, unsigned int capacity)
{
  stack->top = -1;
  insert_root_node(stack);
}

void stack_free(rs_stack_t *stack)
{
  free(stack->contents);
  stack->contents = NULL;
  stack->top = -1;
  stack->capacity = 0;
}

void stack_init(rs_stack_t *stack, unsigned int capacity)
{
  rs_stack_frame_t *contents = (rs_stack_frame_t *)malloc(sizeof(rs_stack_frame_t) * capacity);
  check_buffer(contents);

  stack->contents = contents;
  stack->capacity = capacity;
  stack->top = -1;

  insert_root_node(stack);
}
