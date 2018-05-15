#include "method_desc.h"

VALUE rs_method_class(rs_method_desc_t *method) {
  return method->singleton_p ? method->self : rb_obj_class(method->self);
}

void rs_method_desc_mark(rs_method_desc_t *method) {
  rb_gc_mark(method->self);
  rb_gc_mark(method->id);
}
