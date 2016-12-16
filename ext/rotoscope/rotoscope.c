#include <ruby.h>
#include <stdio.h>

const char* rubyObjectFields[2] = {"object_id", "inspect"};
typedef struct RubyObject {
  unsigned long int object_id;
  char* inspect;
} RubyObject;

const char* tracePointFields[3] = {"event", "method_id", "defined_class"};;
typedef struct TracePoint {
  char* event;
  char* method_id;
  char* defined_class;
  struct RubyObject self;
} TracePoint;

static VALUE send_field(VALUE obj, char* field) {
  return rb_funcall(obj, rb_intern(field), 0);
}

static unsigned long int read_fixnum_field(VALUE obj, char* field) {
  VALUE out = send_field(obj, field);
  Check_Type(out, T_FIXNUM);
  return FIX2LONG(out);
}

static char* read_symbol_field(VALUE obj, char* field) {
  VALUE out = send_field(obj, field);
  Check_Type(out, T_SYMBOL);
  return rb_sym2str(out);
}

// static char* read_string_field(VALUE obj, char* field) {
//   VALUE out = send_field(obj, field);
//   Check_Type(out, T_STRING);
//   return RSTRING_PTR(out);
// }

static char* inspect_object(VALUE obj) {
  return rb_inspect(obj);
}

static struct RubyObject rubyobject_from_VALUE(VALUE value) {
  struct RubyObject obj;
  VALUE real_class = rb_obj_class(value);

  obj.object_id = read_fixnum_field(real_class, "object_id");
  obj.inspect = rb_inspect(real_class);
  return obj;
} 

static struct TracePoint tracepoint_from_VALUE(VALUE obj) {
  struct TracePoint tp;
  tp.event = read_symbol_field(obj, "event");
  tp.method_id = read_symbol_field(obj, "method_id");
  tp.defined_class = inspect_object(send_field(obj, "defined_class"));
  tp.self = rubyobject_from_VALUE(send_field(obj, "self"));
  return tp;
}

int serialize_to_file(struct TracePoint* tp) {
  FILE* fout = fopen("tmp/trace/trace.log", "w");
  serialize_tracepoint(tp, fout);
  serialize_rubyobject(&tp->self, fout);
  fwrite("\n", sizeof(char), 1, fout);
  fclose(fout);
}

int serialize_tracepoint(struct TracePoint* tp, FILE* fout) {
  fwrite(tp->event, sizeof(char), strlen(tp->event), fout);
  fwrite(tp->method_id, sizeof(char), strlen(tp->method_id), fout);
  fwrite(tp->defined_class, sizeof(char), strlen(tp->defined_class), fout);
  return 0;
}

int serialize_rubyobject(struct RubyObject* obj, FILE* fout) {
  fwrite(&obj->object_id, sizeof(unsigned long int), 1, fout);
  fwrite(obj->inspect, sizeof(char), strlen(obj->inspect), fout);
  return 0;
}

static VALUE log_tracepoint(VALUE self, VALUE trace) {
  struct TracePoint tp = tracepoint_from_VALUE(trace);
  serialize_to_file(&tp);
  return rb_str_new2("Hello");
}

void Init_rotoscope(void) {
  VALUE cRotoscope = rb_define_class("Rotoscope", rb_cObject);
  rb_define_method(cRotoscope, "log_tracepoint", log_tracepoint, 1);
}
