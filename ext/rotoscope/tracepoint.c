#include "ruby.h"
#include "tracepoint.h"

static char *ownstr(const char *cstr)
{
  char *owned_str = ALLOC_N(char, strlen(cstr) + 1);
  strcpy(owned_str, cstr);
  return owned_str;
}

void rs_tracepoint_free(rs_tracepoint_t *tracepoint)
{
  xfree((char *)tracepoint->event);
  xfree((char *)tracepoint->entity);
  xfree((char *)tracepoint->filepath);
  xfree((char *)tracepoint->method_name);
  xfree((char *)tracepoint->method_level);

  tracepoint->event = tracepoint->entity = tracepoint->filepath = tracepoint->method_name = tracepoint->method_level = NULL;
}

rs_tracepoint_t *rs_tracepoint_init(rs_tracepoint_args args)
{
  rs_tracepoint_t *trace = ALLOC(rs_tracepoint_t);
  trace->event = ownstr(args.event);
  trace->entity = ownstr(args.entity);
  trace->filepath = ownstr(args.filepath);
  trace->method_name = ownstr(args.method_name);
  trace->method_level = ownstr(args.method_level);
  trace->lineno = args.lineno;

  return trace;
}
