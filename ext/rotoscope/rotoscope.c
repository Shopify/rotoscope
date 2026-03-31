#include "rotoscope.h"

#include <errno.h>
#include <ruby/debug.h>
#include <ruby/intern.h>
#include <ruby/io.h>
#include <ruby/version.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/file.h>

#include "method_desc.h"
#include "stack.h"

VALUE cRotoscope, cTracePoint;
VALUE str_unknown, str_empty;
ID id_initialize, id_match_p;

// Forward declarations — used in event_hook before definition
VALUE rotoscope_log_call(VALUE self, VALUE io, VALUE excludelist,
                         VALUE self_obj);
static bool build_csv_line(Rotoscope *config, VALUE buf, VALUE excludelist,
                           VALUE self_obj);

#define LOG_BUFFER_FLUSH_SIZE 65536
static void flush_log_buffer(Rotoscope *config);

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

static rs_callsite_t tracearg_path(rb_trace_arg_t *trace_arg,
                                   rb_event_flag_t event_flag) {
  switch (event_flag) {
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

  // Pre-compute class name so build_csv_line reads it from the frame
  // for both the current receiver and the next event's caller.
  VALUE klass = singleton_p ? receiver : rb_obj_class(receiver);
  VALUE class_name = class2str(klass);

  return (rs_method_desc_t){
      .receiver = receiver,
      .id = method_id,
      .class_name = class_name,
      .singleton_p = singleton_p,
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

  if (config->tid != rb_thread_current()) return;
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

  config->callsite = tracearg_path(trace_arg, event_flag);

  config->caller = rs_stack_peek(&config->stack);

  // Derive caller method name from the stack when available.
  // For caller_singleton_p, always use the profile frame when we have one
  // (it correctly handles define_method/extend edge cases where our stack's
  // singleton_p can differ from rb_profile_frame_singleton_method_p).
  if (config->caller != NULL) {
    config->callsite.method_name = rb_sym2str(config->caller->method.id);
    if (config->callsite.profile_frame != Qnil) {
      config->callsite.singleton_p =
          rb_profile_frame_singleton_method_p(config->callsite.profile_frame);
    } else {
      config->callsite.singleton_p =
          config->caller->method.singleton_p ? Qtrue : Qfalse;
    }
  } else if (config->callsite.profile_frame != Qnil) {
    // Top-level call — no stack caller. Fall back to profile frame.
    config->callsite.method_name =
        rb_profile_frame_method_name(config->callsite.profile_frame);
    config->callsite.singleton_p =
        rb_profile_frame_singleton_method_p(config->callsite.profile_frame);
  } else if (event_flag == RUBY_EVENT_C_CALL) {
    // C_CALL with no stack caller (first call after start_trace is a C call).
    // Need rb_profile_frames to get the caller method name.
    VALUE frame = Qnil;
    int line = 0;
    rb_profile_frames(0, 1, &frame, &line);
    if (frame != Qnil) {
      config->callsite.method_name = rb_profile_frame_method_name(frame);
      config->callsite.singleton_p = rb_profile_frame_singleton_method_p(frame);
    }
  }

  rs_method_desc_t method_desc = called_method_desc(trace_arg);
  rs_stack_push(&config->stack, (rs_stack_frame_t){.method = method_desc});

  // If a native logger is registered, build CSV directly into the shared
  // buffer and flush periodically. Avoids per-event rb_funcall + temp string.
  if (config->log_io != Qnil) {
    if (build_csv_line(config, config->log_buffer, config->log_excludelist,
                       config->log_self_obj)) {
      if (RSTRING_LEN(config->log_buffer) >= LOG_BUFFER_FLUSH_SIZE) {
        flush_log_buffer(config);
      }
    }
  } else if (config->trace_proc != Qnil) {
    rb_proc_call_with_block(config->trace_proc, 1, &config->self, Qnil);
  }
}

static void rs_gc_mark(void *data) {
  Rotoscope *config = (Rotoscope *)data;
  rb_gc_mark(config->tracepoint);
  rb_gc_mark(config->trace_proc);
  rb_gc_mark(config->tid);
  rb_gc_mark(config->log_io);
  rb_gc_mark(config->log_excludelist);
  rb_gc_mark(config->log_self_obj);
  rb_gc_mark(config->log_buffer);
  rs_stack_mark(&config->stack);
}

static void rs_dealloc(void *data) {
  Rotoscope *config = (Rotoscope *)data;
  stop_tracing_on_cleanup(config);
  rs_stack_free(&config->stack);
  xfree(config);
}

static size_t rs_memsize(const void *data) { return sizeof(Rotoscope); }

static const rb_data_type_t rs_data_type = {
    .wrap_struct_name = "Rotoscope",
    .function =
        {
            .dmark = rs_gc_mark,
            .dfree = rs_dealloc,
            .dsize = rs_memsize,
        },
    .flags = RUBY_TYPED_FREE_IMMEDIATELY};

static VALUE rs_alloc(VALUE klass) {
  Rotoscope *config;
  VALUE self = TypedData_Make_Struct(klass, Rotoscope, &rs_data_type, config);
  config->self = self;
  config->pid = getpid();
  config->tid = rb_thread_current();
  config->tracing = false;
  config->caller = NULL;
  config->callsite = (rs_callsite_t){
      .filepath = Qnil,
      .lineno = 0,
      .method_name = Qnil,
      .singleton_p = Qnil,
      .profile_frame = Qnil,
  };
  config->trace_proc = Qnil;
  config->log_io = Qnil;
  config->log_excludelist = Qnil;
  config->log_self_obj = Qnil;
  config->log_buffer = Qnil;
  rs_stack_init(&config->stack, STACK_CAPACITY);
  config->tracepoint = rb_tracepoint_new(Qnil, EVENT_CALL | EVENT_RETURN,
                                         event_hook, (void *)config);
  return self;
}

static Rotoscope *get_config(VALUE self) {
  Rotoscope *config;
  TypedData_Get_Struct(self, Rotoscope, &rs_data_type, config);
  return config;
}

VALUE rotoscope_initialize(VALUE self) {
  Rotoscope *config = get_config(self);
  if (rb_block_given_p()) {
    config->trace_proc = rb_block_proc();
  }
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
    flush_log_buffer(config);
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
  return config->callsite.method_name;
}

VALUE rotoscope_caller_singleton_method_p(VALUE self) {
  Rotoscope *config = get_config(self);
  return config->callsite.singleton_p;
}

VALUE rotoscope_caller_path(VALUE self) {
  Rotoscope *config = get_config(self);
  return config->callsite.filepath;
}

VALUE rotoscope_caller_lineno(VALUE self) {
  Rotoscope *config = get_config(self);
  return UINT2NUM(config->callsite.lineno);
}

// Append "str", to buf. Fast path builds the whole field in a stack buffer
// and does a single rb_str_cat (1 call instead of 3). Falls back for long
// strings or strings with internal double quotes.
#define CSV_FIELD_BUF 128

static void csv_append_escaped(VALUE buf, VALUE str) {
  const char *src = RSTRING_PTR(str);
  long len = RSTRING_LEN(str);

  if (len <= CSV_FIELD_BUF - 4) {
    // Scan for quotes — no quotes is the overwhelmingly common case
    const char *end = src + len;
    const char *p = src;
    while (p < end && *p != '"') p++;
    if (p == end) {
      // No quotes: build "str", in stack buffer, single rb_str_cat
      char tmp[CSV_FIELD_BUF];
      tmp[0] = '"';
      memcpy(tmp + 1, src, len);
      tmp[len + 1] = '"';
      tmp[len + 2] = ',';
      rb_str_cat(buf, tmp, len + 3);
      return;
    }
  }

  // Slow path: long string or contains quotes
  rb_str_cat(buf, "\"", 1);
  const char *end = src + len;
  const char *chunk_start = src;
  while (src < end) {
    if (*src == '"') {
      if (src > chunk_start) {
        rb_str_cat(buf, chunk_start, src - chunk_start);
      }
      rb_str_cat(buf, "\"\"", 2);
      chunk_start = src + 1;
    }
    src++;
  }
  if (src > chunk_start) {
    rb_str_cat(buf, chunk_start, src - chunk_start);
  }
  rb_str_cat(buf, "\",", 2);
}

static ID id_write;

// Core CSV builder: appends one CSV line to `buf`. Returns true if a line
// was appended, false if filtered. Shared by both the public format_call
// API and the internal buffered native logger path.
static bool build_csv_line(Rotoscope *config, VALUE buf, VALUE excludelist,
                           VALUE self_obj) {

  // Fast-path: check filters before doing any formatting work
  VALUE cs_filepath = config->callsite.filepath;
  if (cs_filepath != Qnil && excludelist != Qnil) {
    if (rb_funcall(excludelist, id_match_p, 1, cs_filepath) == Qtrue) {
      return false;
    }
  }

  rs_stack_frame_t *call = rs_stack_peek(&config->stack);
  if (call == NULL) {
    return false;
  }

  // Check if receiver is the CallLogger itself (self-call filter).
  // Use the receiver already stored in the stack frame from called_method_desc
  // to avoid a redundant rb_tracearg_self + rb_tracearg_from_tracepoint call.
  if (call->method.receiver == self_obj) {
    return false;
  }

  // Current call info (from stack)
  VALUE receiver_class_name = call->method.class_name;
  VALUE method_name = rb_sym2str(call->method.id);

  // Caller entity info (from stack's previous frame)
  VALUE caller_class_name;
  if (config->caller == NULL) {
    caller_class_name = str_unknown;
  } else {
    caller_class_name = config->caller->method.class_name;
  }

  // Caller method name (from callsite / rb_profile_frames)
  VALUE caller_method_name;
  if (config->callsite.method_name == Qnil) {
    caller_method_name = str_unknown;
  } else {
    caller_method_name = config->callsite.method_name;
  }

  VALUE caller_path = config->callsite.filepath;
  if (caller_path == Qnil) caller_path = str_empty;
  unsigned int caller_lineno = config->callsite.lineno;

  // Build the entire CSV line in a stack buffer, then do a single rb_str_cat.
  // Typical line is ~80-100 bytes. Use 512 to handle long class/method names.
  char line[512];
  char *p = line;
  char *end = line + sizeof(line) - 2;

  // Helper: append "str", — single-pass copy with inline escaping.
  // For short strings (class/method names ~10-30 bytes), a byte loop
  // is faster than memchr + memcpy due to avoided function call overhead.
  #define APPEND_FIELD(str_val) do { \
    const char *_s = RSTRING_PTR(str_val); \
    long _l = RSTRING_LEN(str_val); \
    if (p + _l * 2 + 3 > end) goto fallback; \
    *p++ = '"'; \
    for (long _i = 0; _i < _l; _i++) { \
      if (_s[_i] == '"') *p++ = '"'; \
      *p++ = _s[_i]; \
    } \
    *p++ = '"'; *p++ = ','; \
  } while(0)

  // Helper: append literal string
  #define APPEND_LIT(lit, litlen) do { \
    if (p + (litlen) > end) goto fallback; \
    memcpy(p, (lit), (litlen)); p += (litlen); \
  } while(0)

  APPEND_FIELD(receiver_class_name);
  APPEND_FIELD(caller_class_name);
  APPEND_FIELD(caller_path);
  // lineno (unquoted)
  int lineno_len = snprintf(p, end - p, "%u,", caller_lineno);
  p += lineno_len;
  APPEND_FIELD(method_name);
  // method_level + comma
  if (call->method.singleton_p) {
    APPEND_LIT("class,", 6);
  } else {
    APPEND_LIT("instance,", 9);
  }
  APPEND_FIELD(caller_method_name);
  // caller_method_level + newline
  if (config->callsite.method_name == Qnil) {
    APPEND_LIT("<UNKNOWN>\n", 10);
  } else if (config->callsite.singleton_p == Qtrue) {
    APPEND_LIT("class\n", 6);
  } else {
    APPEND_LIT("instance\n", 9);
  }

  rb_str_cat(buf, line, p - line);
  #undef APPEND_FIELD
  #undef APPEND_LIT
  return true;

fallback:
  // Line too long for stack buffer — fall back to multiple rb_str_cat
  csv_append_escaped(buf, receiver_class_name);
  csv_append_escaped(buf, caller_class_name);
  csv_append_escaped(buf, caller_path);
  {
    char lbuf[16];
    int ll = snprintf(lbuf, sizeof(lbuf), "%u,", caller_lineno);
    rb_str_cat(buf, lbuf, ll);
  }
  csv_append_escaped(buf, method_name);
  rb_str_cat(buf, call->method.singleton_p ? "class," : "instance,",
             call->method.singleton_p ? 6 : 9);
  csv_append_escaped(buf, caller_method_name);
  if (config->callsite.method_name == Qnil)
    rb_str_cat(buf, "<UNKNOWN>\n", 10);
  else if (config->callsite.singleton_p == Qtrue)
    rb_str_cat(buf, "class\n", 6);
  else
    rb_str_cat(buf, "instance\n", 9);
  return true;
}

// Public API: returns a new Ruby string with the CSV line, or nil if filtered.
VALUE rotoscope_format_call(VALUE self, VALUE excludelist, VALUE self_obj) {
  Rotoscope *config = get_config(self);
  VALUE buf = rb_str_buf_new(128);
  if (build_csv_line(config, buf, excludelist, self_obj)) {
    return buf;
  }
  return Qnil;
}

VALUE rotoscope_log_call(VALUE self, VALUE io, VALUE excludelist,
                         VALUE self_obj) {
  Rotoscope *config = get_config(self);
  VALUE buf = rb_str_buf_new(128);
  if (build_csv_line(config, buf, excludelist, self_obj)) {
    rb_funcall(io, id_write, 1, buf);
  }
  return Qnil;
}

static void flush_log_buffer(Rotoscope *config) {
  if (config->log_buffer != Qnil && RSTRING_LEN(config->log_buffer) > 0) {
    rb_funcall(config->log_io, id_write, 1, config->log_buffer);
    // Clear buffer while preserving capacity. rb_str_modify ensures we have
    // our own copy (unshares if needed), then set length to 0.
    rb_str_modify(config->log_buffer);
    rb_str_set_len(config->log_buffer, 0);
  }
}

VALUE rotoscope_set_native_logger(VALUE self, VALUE io, VALUE excludelist,
                                  VALUE self_obj) {
  Rotoscope *config = get_config(self);
  config->log_io = io;
  config->log_excludelist = excludelist;
  config->log_self_obj = self_obj;
  config->log_buffer = rb_str_buf_new(LOG_BUFFER_FLUSH_SIZE + 256);
  return Qnil;
}

void Init_rotoscope(void) {
  cTracePoint = rb_const_get(rb_cObject, rb_intern("TracePoint"));

  id_initialize = rb_intern("initialize");
  id_match_p = rb_intern("match?");
  id_write = rb_intern("write");

  str_unknown = rb_str_new_cstr("<UNKNOWN>");
  OBJ_FREEZE(str_unknown);
  rb_gc_register_mark_object(str_unknown);
  str_empty = rb_str_new_cstr("");
  OBJ_FREEZE(str_empty);
  rb_gc_register_mark_object(str_empty);

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
  rb_define_method(cRotoscope, "format_call", rotoscope_format_call, 2);
  rb_define_method(cRotoscope, "log_call_native", rotoscope_log_call, 3);
  rb_define_method(cRotoscope, "set_native_logger",
                   rotoscope_set_native_logger, 3);
}
