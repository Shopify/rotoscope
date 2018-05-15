#ifndef _INC_ROTOSCOPE_METHOD_DESC_H_
#define _INC_ROTOSCOPE_METHOD_DESC_H_

#include "ruby.h"
#include "stdbool.h"

typedef struct rs_method_desc_t {
  VALUE self;
  VALUE id;
  bool singleton_p;
} rs_method_desc_t;

VALUE rs_method_class(rs_method_desc_t *method);
void rs_method_desc_mark(rs_method_desc_t *method);

#endif
