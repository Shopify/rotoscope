#include "tracepoint.h"
#include "ruby.h"

rs_raw_tracepoint_t rs_raw_from_tracepoint(VALUE tracepoint) {
  rb_trace_arg_t *trace_arg = rb_tracearg_from_tracepoint(tracepoint);
  return (rs_raw_tracepoint_t){
      .self = rb_tracearg_self(trace_arg),
      .method_id = rb_tracearg_method_id(trace_arg),
  };
}

void rs_raw_tracepoint_mark(rs_raw_tracepoint_t *tracepoint) {
  rb_gc_mark(tracepoint->method_id);
  rb_gc_mark(tracepoint->self);
}

void rs_tracepoint_mark(rs_tracepoint_t *tracepoint) {
  rb_gc_mark(tracepoint->entity);
  rb_gc_mark(tracepoint->filepath);
  rb_gc_mark(tracepoint->method_name);
  rs_raw_tracepoint_mark(&tracepoint->raw);
}

bool rs_raw_tracepoint_cmp(rs_raw_tracepoint_t *tp1, rs_raw_tracepoint_t *tp2) {
  return (tp1->method_id == tp2->method_id) && (tp1->self == tp2->self);
}
