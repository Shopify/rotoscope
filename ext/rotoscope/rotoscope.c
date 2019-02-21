#include <errno.h>
#include <ruby.h>
#include <ruby/debug.h>
#include <ruby/intern.h>
#include <ruby/io.h>
#include <ruby/version.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/file.h>

#include "callsite.h"
#include "method_desc.h"
#include "rotoscope.h"
#include "stack.h"

VALUE cRotoscope, cTracePoint;
ID id_initialize, id_call;

static unsigned long gettid() {
  return NUM2ULONG(rb_obj_id(rb_thread_current()));
}

static VALUE class_path(VALUE klass) {
  VALUE cached_path = rb_class_path_cached(klass);
  if (!NIL_P(cached_path)) {
    return cached_path;
  }
  return rb_class_path(klass);
}

static VALUE singleton_object(VALUE singleton_class) {
  return rb_iv_get(singleton_class, "__attached__");
}

static VALUE class2str(VALUE klass) {
  while (FL_TEST(klass, FL_SINGLETON)) {
    klass = singleton_object(klass);
    if (!RB_TYPE_P(klass, T_MODULE) && !RB_TYPE_P(klass, T_CLASS)) {
      // singleton of an instance
      klass = rb_obj_class(klass);
    }
  }
  return class_path(klass);
}

static rs_callsite_t tracearg_path(rb_trace_arg_t *trace_arg) {
  switch (rb_tracearg_event_flag(trace_arg)) {
    case RUBY_EVENT_C_RETURN:
    case RUBY_EVENT_C_CALL:
      return c_callsite(trace_arg);
    default:
      return ruby_callsite();
  }
}

static rs_method_desc_t called_method_desc(rb_trace_arg_t *trace_arg) {
  VALUE receiver = rb_tracearg_self(trace_arg);
  VALUE method_id = rb_tracearg_method_id(trace_arg);
  bool singleton_p =
      (RB_TYPE_P(receiver, T_CLASS) || RB_TYPE_P(receiver, T_MODULE)) &&
      SYM2ID(method_id) != id_initialize;

  return (rs_method_desc_t){
      .receiver = receiver, .id = method_id, .singleton_p = singleton_p,
  };
}

static bool in_fork(Rotoscope *config) { return config->pid != getpid(); }

// The GC sweep step will turn objects with finalizers (e.g. rs_dealloc)
// to zombie objects until their finalizer is run. In this state, any
// ruby objects in the Rotoscope struct may have already been collected
// so they can't safely be used. If tracing isn't stopped before the
// Rotoscope object has been garbage collected, then we still may receive
// trace events for method calls in finalizers that run before the one
// for the Rotoscope object.
bool rotoscope_marked_for_garbage_collection(Rotoscope *config) {
  return RB_BUILTIN_TYPE(config->self) == RUBY_T_ZOMBIE;
}

static void stop_tracing_on_cleanup(Rotoscope *config) {
  if (config->tracing) {
    // During process cleanup, event hooks are removed and tracepoint may have
    // already have been GCed, so we need a sanity check before disabling the
    // tracepoint.
    if (RB_TYPE_P(config->tracepoint, T_DATA) &&
        CLASS_OF(config->tracepoint) == cTracePoint) {
      rb_tracepoint_disable(config->tracepoint);
    }
    config->tracing = false;
  }
}

static void event_hook(VALUE tpval, void *data) {
  Rotoscope *config = (Rotoscope *)data;

  if (rotoscope_marked_for_garbage_collection(config)) {
    stop_tracing_on_cleanup(config);
    return;
  }

  if (config->tid != gettid()) return;
  if (in_fork(config)) {
    rb_tracepoint_disable(config->tracepoint);
    config->tracing = false;
    return;
  }

  rb_trace_arg_t *trace_arg = rb_tracearg_from_tracepoint(tpval);

  if (rb_tracearg_defined_class(trace_arg) == cRotoscope) {
    return;
  }

  rb_event_flag_t event_flag = rb_tracearg_event_flag(trace_arg);

  if (event_flag & EVENT_RETURN) {
    if (!rs_stack_empty(&config->stack)) {
      rs_stack_pop(&config->stack);
    }
    return;
  }

  config->callsite = tracearg_path(trace_arg);

  config->caller = rs_stack_peek(&config->stack);

  rs_method_desc_t method_desc = called_method_desc(trace_arg);
  rs_stack_push(&config->stack, (rs_stack_frame_t){.method = method_desc});

  rb_funcall(config->trace_proc, id_call, 1, config->self);
}

static void rs_gc_mark(Rotoscope *config) {
  rb_gc_mark(config->tracepoint);
  rb_gc_mark(config->trace_proc);
  rs_stack_mark(&config->stack);
}

void rs_dealloc(Rotoscope *config) {
  stop_tracing_on_cleanup(config);
  rs_stack_free(&config->stack);
  xfree(config);
}

static VALUE rs_alloc(VALUE klass) {
  Rotoscope *config;
  VALUE self =
      Data_Make_Struct(klass, Rotoscope, rs_gc_mark, rs_dealloc, config);
  config->self = self;
  config->pid = getpid();
  config->tid = gettid();
  config->tracing = false;
  config->caller = NULL;
  config->callsite.filepath = Qnil;
  config->callsite.lineno = 0;
  config->trace_proc = Qnil;
  rs_stack_init(&config->stack, STACK_CAPACITY);
  config->tracepoint = rb_tracepoint_new(Qnil, EVENT_CALL | EVENT_RETURN,
                                         event_hook, (void *)config);
  return self;
}

static Rotoscope *get_config(VALUE self) {
  Rotoscope *config;
  Data_Get_Struct(self, Rotoscope, config);
  return config;
}

VALUE rotoscope_initialize(VALUE self) {
  Rotoscope *config = get_config(self);
  config->trace_proc = rb_block_proc();
  return self;
}

VALUE rotoscope_start_trace(VALUE self) {
  Rotoscope *config = get_config(self);
  rb_tracepoint_enable(config->tracepoint);
  config->tracing = true;
  return Qnil;
}

VALUE rotoscope_stop_trace(VALUE self) {
  Rotoscope *config = get_config(self);
  if (rb_tracepoint_enabled_p(config->tracepoint)) {
    rb_tracepoint_disable(config->tracepoint);
    config->tracing = false;
    rs_stack_reset(&config->stack);
  }

  return Qnil;
}

VALUE rotoscope_tracing_p(VALUE self) {
  Rotoscope *config = get_config(self);
  return config->tracing ? Qtrue : Qfalse;
}

VALUE rotoscope_receiver(VALUE self) {
  Rotoscope *config = get_config(self);
  return rb_tracearg_self(rb_tracearg_from_tracepoint(config->tracepoint));
}

VALUE rotoscope_receiver_class(VALUE self) {
  Rotoscope *config = get_config(self);
  rs_stack_frame_t *call = rs_stack_peek(&config->stack);
  if (call == NULL) {
    return Qnil;
  }
  return rs_method_class(&call->method);
}

VALUE rotoscope_receiver_class_name(VALUE self) {
  VALUE klass = rotoscope_receiver_class(self);
  if (klass == Qnil) {
    return Qnil;
  }
  return class2str(klass);
}

VALUE rotoscope_method_name(VALUE self) {
  Rotoscope *config = get_config(self);
  rs_stack_frame_t *call = rs_stack_peek(&config->stack);
  if (call == NULL) {
    return Qnil;
  }
  return rb_sym2str(call->method.id);
}

VALUE rotoscope_singleton_method_p(VALUE self) {
  Rotoscope *config = get_config(self);
  rs_stack_frame_t *call = rs_stack_peek(&config->stack);
  if (call == NULL) {
    return Qnil;
  }
  return call->method.singleton_p ? Qtrue : Qfalse;
}

VALUE rotoscope_caller_object(VALUE self) {
  Rotoscope *config = get_config(self);
  if (config->caller == NULL) {
    return Qnil;
  }
  return config->caller->method.receiver;
}

VALUE rotoscope_caller_class(VALUE self) {
  Rotoscope *config = get_config(self);
  if (config->caller == NULL) {
    return Qnil;
  }
  return rs_method_class(&config->caller->method);
}

VALUE rotoscope_caller_class_name(VALUE self) {
  VALUE klass = rotoscope_caller_class(self);
  if (klass == Qnil) {
    return Qnil;
  }
  return class2str(klass);
}

VALUE rotoscope_caller_method_name(VALUE self) {
  Rotoscope *config = get_config(self);
  if (config->caller == NULL) {
    return Qnil;
  }
  return rb_sym2str(config->caller->method.id);
}

VALUE rotoscope_caller_singleton_method_p(VALUE self) {
  Rotoscope *config = get_config(self);
  if (config->caller == NULL) {
    return Qnil;
  }
  return config->caller->method.singleton_p ? Qtrue : Qfalse;
}

VALUE rotoscope_caller_path(VALUE self) {
  Rotoscope *config = get_config(self);
  return config->callsite.filepath;
}

VALUE rotoscope_caller_lineno(VALUE self) {
  Rotoscope *config = get_config(self);
  return UINT2NUM(config->callsite.lineno);
}

void Init_rotoscope(void) {
  cTracePoint = rb_const_get(rb_cObject, rb_intern("TracePoint"));

  id_initialize = rb_intern("initialize");
  id_call = rb_intern("call");

  cRotoscope = rb_define_class("Rotoscope", rb_cObject);
  rb_define_alloc_func(cRotoscope, rs_alloc);
  rb_define_method(cRotoscope, "initialize", rotoscope_initialize, 0);
  rb_define_method(cRotoscope, "start_trace", rotoscope_start_trace, 0);
  rb_define_method(cRotoscope, "stop_trace", rotoscope_stop_trace, 0);
  rb_define_method(cRotoscope, "tracing?", rotoscope_tracing_p, 0);
  rb_define_method(cRotoscope, "receiver", rotoscope_receiver, 0);
  rb_define_method(cRotoscope, "receiver_class", rotoscope_receiver_class, 0);
  rb_define_method(cRotoscope, "receiver_class_name",
                   rotoscope_receiver_class_name, 0);
  rb_define_method(cRotoscope, "method_name", rotoscope_method_name, 0);
  rb_define_method(cRotoscope, "singleton_method?",
                   rotoscope_singleton_method_p, 0);
  rb_define_method(cRotoscope, "caller_object", rotoscope_caller_object, 0);
  rb_define_method(cRotoscope, "caller_class", rotoscope_caller_class, 0);
  rb_define_method(cRotoscope, "caller_class_name", rotoscope_caller_class_name,
                   0);
  rb_define_method(cRotoscope, "caller_method_name",
                   rotoscope_caller_method_name, 0);
  rb_define_method(cRotoscope, "caller_singleton_method?",
                   rotoscope_caller_singleton_method_p, 0);
  rb_define_method(cRotoscope, "caller_path", rotoscope_caller_path, 0);
  rb_define_method(cRotoscope, "caller_lineno", rotoscope_caller_lineno, 0);

  init_callsite();
}
