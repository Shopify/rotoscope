# frozen_string_literal: true

at_exit { GC.start } # to reduce false positives in the test:valgrind rake task

$LOAD_PATH.unshift(File.expand_path("../../lib", __FILE__))
$LOAD_PATH.unshift(File.expand_path("../", __FILE__))
require "rotoscope"
require "minitest"
require "zlib"
require "fileutils"
require "csv"

require "fixture_inner"
require "fixture_outer"
require "monadify"

module MyModule
  def module_method; end
end

module PrependedModule
  def prepended_method; end
end

class Example
  prepend PrependedModule
  include MyModule
  extend MyModule
  extend Monadify

  class << self
    def singleton_method
      true
    end

    def apply(val)
      monad(val)
    end

    define_method(:'escaping"needed') do
      true
    end

    define_method(:'escaping"needed2') do
      call_escaping_needed
    end

    def call_escaping_needed
      public_send(:'escaping"needed')
    end
  end

  def normal_method
    true
  end

  def exception_method
    oops
  rescue
    nil
  end

  def yielding_method
    yield
  end

  private

  def oops
    raise "I've made a terrible mistake"
  end
end

ROOT_FIXTURE_PATH = File.expand_path("../", __FILE__)
INNER_FIXTURE_PATH = File.expand_path("../fixture_inner.rb", __FILE__)
OUTER_FIXTURE_PATH = File.expand_path("../fixture_outer.rb", __FILE__)
MONADIFY_PATH = File.expand_path("monadify.rb", ROOT_FIXTURE_PATH)

class RotoscopeTest < Minitest::Test
  def setup
    @logfile = File.expand_path("tmp/test.csv")
  end

  def teardown
    FileUtils.remove_file(@logfile) if File.file?(@logfile)
  end

  def test_new
    rs = Rotoscope::CallLogger.new(@logfile, excludelist: ["tmp"])
    assert(rs.is_a?(Rotoscope::CallLogger))
  end

  def test_close
    rs = Rotoscope::CallLogger.new(@logfile)
    assert(rs.close)
  end

  def test_closed?
    rs = Rotoscope::CallLogger.new(@logfile)
    refute_predicate(rs, :closed?)
    rs.close
    assert_predicate(rs, :closed?)
  end

  def test_state
    rs = Rotoscope::CallLogger.new(@logfile)
    assert_equal(:open, rs.state)
    rs.trace do
      assert_equal(:tracing, rs.state)
    end
    assert_equal(:open, rs.state)
    rs.close
    assert_equal(:closed, rs.state)
  end

  def test_mark
    contents = rotoscope_trace do |rs|
      Example.new.normal_method
      rs.mark
    end

    assert_includes(contents.split("\n"), "--- ")
  end

  def test_mark_with_custom_strings
    mark_strings = ["Hello", "ÅÉÎØÜ åéîøü"]
    contents = rotoscope_trace do |rs|
      e = Example.new
      e.normal_method
      mark_strings.each { |str| rs.mark(str) }
    end

    content_lines = contents.split("\n")
    mark_strings.each do |str|
      assert_includes(content_lines, "--- #{str}")
    end
  end

  def test_flatten
    contents = rotoscope_trace do
      Example.new.normal_method
    end

    assert_equal([
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: "test_flatten", caller_method_level: "instance" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "test_flatten", caller_method_level: "instance" },
      { entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: "test_flatten", caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_start_trace_and_stop_trace
    rs = Rotoscope::CallLogger.new(@logfile)
    rs.start_trace
    Example.new.normal_method
    rs.stop_trace
    rs.io.flush
    rs.close
    contents = File.read(@logfile)

    assert_equal([
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_traces_instance_method
    contents = rotoscope_trace { Example.new.normal_method }
    assert_equal([
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_traces_yielding_method
    contents = rotoscope_trace do
      e = Example.new
      e.yielding_method { e.normal_method }
    end

    assert_equal([
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "yielding_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_traces_and_formats_singletons_of_a_class
    contents = rotoscope_trace { Example.singleton_method }
    assert_equal([
      { entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_traces_and_formats_singletons_of_an_instance
    contents = rotoscope_trace { Example.new.singleton_class.singleton_method }
    assert_equal([
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "singleton_class", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_traces_included_module_method
    contents = rotoscope_trace { Example.new.module_method }
    assert_equal([
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "module_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_traces_extended_module_method
    contents = rotoscope_trace { Example.module_method }
    assert_equal([
      { entity: "Example", method_name: "module_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_traces_prepended_module_method
    contents = rotoscope_trace { Example.new.prepended_method }
    assert_equal([
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "prepended_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_trace_ignores_calls_if_excluded
    contents = rotoscope_trace(excludelist: [INNER_FIXTURE_PATH, OUTER_FIXTURE_PATH]) do
      foo = FixtureOuter.new
      foo.do_work
    end

    assert_equal([
      { entity: "FixtureOuter", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "FixtureOuter", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "FixtureOuter", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "FixtureOuter", method_name: "do_work", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_trace_ignores_writes_in_fork
    contents = rotoscope_trace do |rotoscope|
      fork do
        Example.singleton_method
        rotoscope.mark
        rotoscope.close
      end
      Example.singleton_method
      Process.wait
    end
    assert_equal([
      { entity: "RotoscopeTest", method_name: "fork", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Process", method_name: "wait", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_trace_disabled_on_close
    mark_err = nil
    contents = rotoscope_trace do |rotoscope|
      Example.singleton_method
      rotoscope.close
      begin
        rotoscope.mark
      rescue IOError => err
        mark_err = err
      end
      Example.singleton_method
    end
    assert_equal([
      { entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
    assert_equal("closed stream", mark_err.message)
  end

  def test_trace_flatten
    contents = rotoscope_trace { Example.new.normal_method }
    assert_equal([
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_trace_flatten_across_files
    contents = rotoscope_trace do
      foo = FixtureOuter.new
      foo.do_work
    end

    assert_equal([
      { entity: "FixtureOuter", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "FixtureOuter", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "FixtureOuter", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "FixtureInner", method_name: "new", method_level: "class", filepath: "/fixture_outer.rb", lineno: -1, caller_entity: "FixtureOuter", caller_method_name: "initialize", caller_method_level: "instance" },
      { entity: "FixtureInner", method_name: "initialize", method_level: "instance", filepath: "/fixture_outer.rb", lineno: -1, caller_entity: "FixtureInner", caller_method_name: "initialize", caller_method_level: "instance" },
      { entity: "FixtureOuter", method_name: "do_work", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "FixtureInner", method_name: "do_work", method_level: "instance", filepath: "/fixture_outer.rb", lineno: -1, caller_entity: "FixtureOuter", caller_method_name: "do_work", caller_method_level: "instance" },
      { entity: "FixtureInner", method_name: "sum", method_level: "instance", filepath: "/fixture_inner.rb", lineno: -1, caller_entity: "FixtureInner", caller_method_name: "do_work", caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_trace_flatten_with_excluded_caller
    foo = FixtureOuter.new
    contents = rotoscope_trace(excludelist: ["/rotoscope_test.rb"]) do
      foo.do_work
    end

    assert_equal([
      { entity: "FixtureInner", method_name: "do_work", method_level: "instance", filepath: "/fixture_outer.rb", lineno: -1,
        caller_entity: "FixtureOuter", caller_method_name: "do_work", caller_method_level: "instance", },
      { entity: "FixtureInner", method_name: "sum", method_level: "instance", filepath: "/fixture_inner.rb", lineno: -1,
        caller_entity: "FixtureInner", caller_method_name: "do_work", caller_method_level: "instance", },
    ], parse_and_normalize(contents))
  end

  def test_trace_uses_io_objects
    string_io = StringIO.new
    Rotoscope::CallLogger.trace(string_io) do
      Example.new.normal_method
    end
    refute_predicate(string_io, :closed?)
    assert_predicate(string_io, :eof?)
    contents = string_io.string

    assert_equal([
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_stop_trace_before_start_does_not_raise
    rs = Rotoscope::CallLogger.new(@logfile)
    rs.stop_trace
  end

  def test_gc_rotoscope_without_stop_trace_does_not_crash
    proc {
      rs = Rotoscope::CallLogger.new(@logfile)
      rs.start_trace
    }.call
    GC.start
  end

  def test_gc_rotoscope_without_stop_trace_does_not_break_process_cleanup
    child_pid = fork do
      rs = Rotoscope::CallLogger.new(@logfile)
      rs.start_trace
    end
    Process.waitpid(child_pid)
    assert_equal(true, $CHILD_STATUS.success?)
  end

  def test_ignores_calls_inside_of_threads
    thread = nil
    contents = rotoscope_trace do
      thread = Thread.new { Example.new }
    end
    thread.join

    assert_equal([
      { entity: "Thread", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Thread", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Thread", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_dynamic_class_creation
    contents = rotoscope_trace { Class.new }

    assert_equal([
      { entity: "Class", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Class", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Class", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Object", method_name: "inherited", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Class", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_block_defined_methods
    contents = rotoscope_trace { Example.apply("my value!") }

    assert_equal([
      { entity: "Example", method_name: "apply", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "monad", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "apply", caller_method_level: "class" },
      { entity: "Example", method_name: "contents", method_level: "class", filepath: "/monadify.rb", lineno: -1, caller_entity: "Example", caller_method_name: "monad", caller_method_level: "instance" },
      { entity: "Example", method_name: "contents=", method_level: "class", filepath: "/monadify.rb", lineno: -1, caller_entity: "Example", caller_method_name: "monad", caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_block_defined_methods_in_excluded
    contents = rotoscope_trace(excludelist: [MONADIFY_PATH]) { Example.apply("my value!") }

    assert_equal([
      { entity: "Example", method_name: "apply", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "monad", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "apply", caller_method_level: "class" },
    ], parse_and_normalize(contents))
  end

  def test_flatten_with_invoking_block_defined_methods
    contents = rotoscope_trace(excludelist: [MONADIFY_PATH]) { Example.contents }

    assert_equal([
      { entity: "Example", method_name: "contents", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_module_extend_self
    contents = rotoscope_trace { Module.new { extend self } }

    assert_equal([
      { entity: "Module", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Module", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Module", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "#<Module:0xXXXXXX>", method_name: "extend", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Module", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "#<Module:0xXXXXXX>", method_name: "extend_object", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "#<Module:0xXXXXXX>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "#<Module:0xXXXXXX>", method_name: "extended", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "#<Module:0xXXXXXX>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_module_extend
    contents = rotoscope_trace do
      m = Module.new { extend(MyModule) }
      m.module_method
    end

    assert_equal([
      { entity: "Module", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Module", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Module", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "#<Module:0xXXXXXX>", method_name: "extend", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Module", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "MyModule", method_name: "extend_object", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "#<Module:0xXXXXXX>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "MyModule", method_name: "extended", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "#<Module:0xXXXXXX>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "#<Module:0xXXXXXX>", method_name: "module_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
    ], parse_and_normalize(contents))
  end

  def test_methods_with_quotes
    contents = rotoscope_trace do
      Example.public_send(:'escaping"needed2')
    end

    assert_equal([
      { entity: "Example", method_name: "public_send", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<UNKNOWN>", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: 'escaping"needed2', method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: __method__.to_s, caller_method_level: "instance" },
      { entity: "Example", method_name: "call_escaping_needed", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
      { entity: "Example", method_name: "public_send", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "call_escaping_needed", caller_method_level: "class" },
      { entity: "Example", method_name: 'escaping"needed', method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "call_escaping_needed", caller_method_level: "class" },
    ], parse_and_normalize(contents))
  end

  def test_trace_block
    calls = []
    rotoscope = Rotoscope.new do |rs|
      calls << {
        receiver_class: rs.receiver_class,
        receiver_class_name: rs.receiver_class_name,
        method_name: rs.method_name,
        singleton_method: rs.singleton_method?,
      }
    end
    rotoscope.trace do
      Example.singleton_method
    end
    assert_equal([
      {
        receiver_class: Example,
        receiver_class_name: "Example",
        method_name: "singleton_method",
        singleton_method: true,
      },
    ], calls)
  end

  def test_caller
    last_call = nil
    rotoscope = Rotoscope.new do |rs|
      last_call = {
        method_name: rs.method_name,
        caller_class: rs.caller_class,
        caller_class_name: rs.caller_class_name,
        caller_method_name: rs.caller_method_name,
        caller_singleton_method: rs.caller_singleton_method?,
      }
    end
    rotoscope.trace do
      FixtureOuter.new.do_work
    end
    assert_equal({
      method_name: "sum",
      caller_class: FixtureInner,
      caller_class_name: "FixtureInner",
      caller_method_name: "do_work",
      caller_singleton_method: false,
    }, last_call)
  end

  private

  EXPECTATION_ORDER = [:entity, :method_name, :method_level, :filepath, :lineno, :caller_entity, :caller_method_name, :caller_method_level]

  def parse_and_normalize(csv_string)
    CSV.parse(csv_string, headers: true, header_converters: :symbol).map do |row|
      row = row.to_a.sort_by { |name, _| EXPECTATION_ORDER.index(name) }.to_h
      row[:lineno] = -1
      unless row[:filepath].empty?
        row[:filepath] = File.expand_path(row[:filepath]).gsub(ROOT_FIXTURE_PATH, "")
      end
      row[:entity] = row[:entity].gsub(/:0x[a-fA-F0-9]{4,}/m, ":0xXXXXXX")
      if row.key?(:caller_entity)
        row[:caller_entity] = row[:caller_entity].gsub(/:0x[a-fA-F0-9]{4,}/m, ":0xXXXXXX")
      end
      row
    end
  end

  def rotoscope_trace(excludelist: [], &block)
    Rotoscope::CallLogger.trace(@logfile, excludelist: excludelist, &block)
    File.read(@logfile)
  end

  def unzip(path)
    File.open(path) { |f| Zlib::GzipReader.new(f).read }
  end
end

Minitest::Test.make_my_diffs_pretty!

# https://github.com/seattlerb/minitest/pull/683 needed to use
# autorun without affecting the exit status of forked processes
exit(Minitest.run(ARGV))
