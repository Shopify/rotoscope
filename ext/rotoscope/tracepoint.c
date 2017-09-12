#include "tracepoint.h"
#include "ruby.h"

void rs_tracepoint_mark(rs_tracepoint_t *tracepoint) {
  rb_gc_mark(tracepoint->entity);
  rb_gc_mark(tracepoint->filepath);
  rb_gc_mark(tracepoint->method_name);
  rb_gc_mark(tracepoint->raw);
}
