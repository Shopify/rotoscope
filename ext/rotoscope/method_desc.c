#include "method_desc.h"

void rs_method_desc_mark(rs_method_desc_t *method) {
  rb_gc_mark(method->class_name);
  rb_gc_mark(method->id);
}
