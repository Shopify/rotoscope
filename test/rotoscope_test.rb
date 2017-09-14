# frozen_string_literal: true
$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)
$LOAD_PATH.unshift File.expand_path('../', __FILE__)
require 'rotoscope'
require 'minitest'
require 'zlib'
require 'fileutils'
require 'csv'

require 'fixture_inner'
require 'fixture_outer'
require 'monadify'

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
      monad val
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

ROOT_FIXTURE_PATH = File.expand_path('../', __FILE__)
INNER_FIXTURE_PATH = File.expand_path('../fixture_inner.rb', __FILE__)
OUTER_FIXTURE_PATH = File.expand_path('../fixture_outer.rb', __FILE__)
MONADIFY_PATH = File.expand_path('monadify.rb', ROOT_FIXTURE_PATH)

class RotoscopeTest < MiniTest::Test
  def setup
    @logfile = File.expand_path('tmp/test.csv')
  end

  def teardown
    FileUtils.remove_file(@logfile) if File.file?(@logfile)
  end

  def test_new
    rs = Rotoscope.new(@logfile, blacklist: ['tmp'], flatten: true)
    assert rs.is_a?(Rotoscope)
  end

  def test_close
    rs = Rotoscope.new(@logfile)
    assert rs.close
  end

  def test_closed?
    rs = Rotoscope.new(@logfile)
    refute_predicate rs, :closed?
    rs.close
    assert_predicate rs, :closed?
  end

  def test_state
    rs = Rotoscope.new(@logfile)
    assert_equal :open, rs.state
    rs.trace do
      assert_equal :tracing, rs.state
    end
    assert_equal :open, rs.state
    rs.close
    assert_equal :closed, rs.state
  end

  def test_mark
    contents = rotoscope_trace do |rs|
      Example.new.normal_method
      rs.mark
    end

    assert_includes contents.split("\n"), '--- '
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
      assert_includes content_lines, "--- #{str}"
    end
  end

  def test_flatten
    contents = rotoscope_trace(flatten: true) do
      Example.new.normal_method
    end

    assert_equal [
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "new", caller_method_level: "class" },
      { entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
    ], parse_and_normalize(contents)
  end

  def test_flatten_removes_duplicates
    contents = rotoscope_trace(flatten: true) do
      e = Example.new
      10.times { e.normal_method }
    end

    assert_equal [
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "new", caller_method_level: "class" },
      { entity: "Fixnum", method_name: "times", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
      { entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Fixnum", caller_method_name: "times", caller_method_level: "instance" },
    ], parse_and_normalize(contents)
  end

  def test_start_trace_and_stop_trace
    rs = Rotoscope.new(@logfile)
    rs.start_trace
    Example.new.normal_method
    rs.stop_trace
    rs.close
    contents = File.read(@logfile)

    assert_equal [
      { event: "call", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
    ], parse_and_normalize(contents)

    assert_frames_consistent contents
  end

  def test_traces_instance_method
    contents = rotoscope_trace { Example.new.normal_method }
    assert_equal [
      { event: "call", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 }
    ], parse_and_normalize(contents)

    assert_frames_consistent contents
  end

  def test_traces_yielding_method
    contents = rotoscope_trace do
      e = Example.new
      e.yielding_method { e.normal_method }
    end

    assert_equal [
      { event: "call", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "yielding_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "yielding_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 }
    ], parse_and_normalize(contents)

    assert_frames_consistent contents
  end

  def test_calls_are_consistent_after_exception
    contents = rotoscope_trace { Example.new.exception_method }
    assert_frames_consistent contents
  end

  def test_traces_and_formats_singletons_of_a_class
    contents = rotoscope_trace { Example.singleton_method }
    assert_equal [
      { event: "call", entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 }
    ], parse_and_normalize(contents)

    assert_frames_consistent contents
  end

  def test_traces_and_formats_singletons_of_an_instance
    contents = rotoscope_trace { Example.new.singleton_class.singleton_method }
    assert_equal [
      { event: "call", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "singleton_class", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "singleton_class", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
    ], parse_and_normalize(contents)

    assert_frames_consistent contents
  end

  def test_traces_included_module_method
    contents = rotoscope_trace { Example.new.module_method }
    assert_equal [
      { event: "call", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "module_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "module_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 }
    ], parse_and_normalize(contents)

    assert_frames_consistent contents
  end

  def test_traces_extended_module_method
    contents = rotoscope_trace { Example.module_method }
    assert_equal [
      { event: "call", entity: "Example", method_name: "module_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "module_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 }
    ], parse_and_normalize(contents)

    assert_frames_consistent contents
  end

  def test_traces_prepended_module_method
    contents = rotoscope_trace { Example.new.prepended_method }
    assert_equal [
      { event: "call", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "prepended_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "prepended_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 }
    ], parse_and_normalize(contents)

    assert_frames_consistent contents
  end

  def test_trace_ignores_calls_if_blacklisted
    contents = rotoscope_trace(blacklist: [INNER_FIXTURE_PATH, OUTER_FIXTURE_PATH]) do
      foo = FixtureOuter.new
      foo.do_work
    end

    assert_equal [
      { event: "call", entity: "FixtureOuter", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "FixtureOuter", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "FixtureOuter", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "FixtureOuter", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "FixtureOuter", method_name: "do_work", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "FixtureOuter", method_name: "do_work", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
    ], parse_and_normalize(contents)

    assert_frames_consistent contents
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
    assert_equal [
      { event: "call", entity: "RotoscopeTest", method_name: "fork", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "RotoscopeTest", method_name: "fork", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Process", method_name: "wait", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Process", method_name: "wait", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
    ], parse_and_normalize(contents)
  end

  def test_trace_disabled_on_close
    contents = rotoscope_trace do |rotoscope|
      Example.singleton_method
      rotoscope.close
      rotoscope.mark
      Example.singleton_method
    end
    assert_equal [
      { event: "call", entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
    ], parse_and_normalize(contents)
  end

  def test_trace_flatten
    contents = rotoscope_trace(flatten: true) { Example.new.normal_method }
    assert_equal [
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "new", caller_method_level: "class" },
      { entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
    ], parse_and_normalize(contents)
  end

  def test_trace_flatten_across_files
    contents = rotoscope_trace(flatten: true) do
      foo = FixtureOuter.new
      foo.do_work
    end

    assert_equal [
      { entity: "FixtureOuter", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
      { entity: "FixtureOuter", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "FixtureOuter", caller_method_name: "new", caller_method_level: "class" },
      { entity: "FixtureInner", method_name: "new", method_level: "class", filepath: "/fixture_outer.rb", lineno: -1, caller_entity: "FixtureOuter", caller_method_name: "initialize", caller_method_level: "instance" },
      { entity: "FixtureInner", method_name: "initialize", method_level: "instance", filepath: "/fixture_outer.rb", lineno: -1, caller_entity: "FixtureInner", caller_method_name: "new", caller_method_level: "class" },
      { entity: "FixtureOuter", method_name: "do_work", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
      { entity: "FixtureInner", method_name: "do_work", method_level: "instance", filepath: "/fixture_outer.rb", lineno: -1, caller_entity: "FixtureOuter", caller_method_name: "do_work", caller_method_level: "instance" },
      { entity: "FixtureInner", method_name: "sum", method_level: "instance", filepath: "/fixture_inner.rb", lineno: -1, caller_entity: "FixtureInner", caller_method_name: "do_work", caller_method_level: "instance" }
    ], parse_and_normalize(contents)
  end

  def test_trace_uses_io_objects
    string_io = StringIO.new
    Rotoscope.trace(string_io) do
      Example.new.normal_method
    end
    refute_predicate string_io, :closed?
    assert_predicate string_io, :eof?
    contents = string_io.string

    assert_equal [
      { event: "call", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
    ], parse_and_normalize(contents)

    assert_frames_consistent contents
  end

  def test_stop_trace_before_start_does_not_raise
    rs = Rotoscope.new(@logfile)
    rs.stop_trace
  end

  def test_gc_rotoscope_without_stop_trace_does_not_crash
    proc {
      rs = Rotoscope.new(@logfile)
      rs.start_trace
    }.call
    GC.start
  end

  def test_gc_rotoscope_without_stop_trace_does_not_break_process_cleanup
    child_pid = fork do
      rs = Rotoscope.new(@logfile)
      rs.start_trace
    end
    Process.waitpid(child_pid)
    assert_equal true, $CHILD_STATUS.success?
  end

  def test_log_path
    rs = Rotoscope.new(File.expand_path('tmp/test.csv.gz'))
    GC.start
    assert_equal File.expand_path('tmp/test.csv.gz'), rs.log_path
  end

  def test_ignores_calls_inside_of_threads
    thread = nil
    contents = rotoscope_trace do
      thread = Thread.new { Example.new }
    end
    thread.join

    assert_equal [
      { event: "call", entity: "Thread", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Thread", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Thread", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Thread", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
    ], parse_and_normalize(contents)
  end

  def test_dynamic_class_creation
    contents = rotoscope_trace { Class.new }

    assert_equal [
      { event: "call", entity: "Class", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Class", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Object", method_name: "inherited", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Object", method_name: "inherited", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Class", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Class", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 }
    ], parse_and_normalize(contents)
  end

  def test_dynamic_methods_in_blacklist
    skip <<-FAILING_TEST_CASE
      Return events for dynamically created methods (define_method, define_singleton_method)
      do not have the correct stack frame information (the call of a dynamically defined method
      is correctly treated as a Ruby :call, but its return must be treated as a :c_return)
    FAILING_TEST_CASE

    contents = rotoscope_trace(blacklist: [MONADIFY_PATH]) { Example.apply("my value!") }

    assert_equal [
      { event: "call", entity: "Example", method_name: "apply", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Example", method_name: "monad", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "monad", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "apply", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
    ], parse_and_normalize(contents)
  end

  def test_flatten_with_dynamic_methods_in_blacklist
    # the failing test above passes when using `flatten: true` since unmatched stack returns are ignored
    contents = rotoscope_trace(blacklist: [MONADIFY_PATH], flatten: true) { Example.apply("my value!") }

    assert_equal [
      { entity: "Example", method_name: "apply", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
      { entity: "Example", method_name: "monad", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "apply", caller_method_level: "class" },
    ], parse_and_normalize(contents)
  end

  def test_module_extend
    contents = rotoscope_trace { Module.new { extend(MyModule) } }

    assert_equal [
      { event: "call", entity: "Module", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Module", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "#<Module:0xXXXXXX>", method_name: "extend", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "MyModule", method_name: "extend_object", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "MyModule", method_name: "extend_object", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "MyModule", method_name: "extended", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "MyModule", method_name: "extended", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "#<Module:0xXXXXXX>", method_name: "extend", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Module", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Module", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
    ], parse_and_normalize(contents)
  end

  def test_module_extend_self
    contents = rotoscope_trace { Module.new { extend self } }

    assert_equal [
      { event: "call", entity: "Module", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "Module", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "#<Module:0xXXXXXX>", method_name: "extend", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "#<Module:0xXXXXXX>", method_name: "extend_object", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "#<Module:0xXXXXXX>", method_name: "extend_object", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "call", entity: "#<Module:0xXXXXXX>", method_name: "extended", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "#<Module:0xXXXXXX>", method_name: "extended", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "#<Module:0xXXXXXX>", method_name: "extend", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Module", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Module", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
    ], parse_and_normalize(contents)
  end

  def test_flatten_module_extend
    contents = rotoscope_trace(flatten: true) do
      m = Module.new { extend(MyModule) }
      m.module_method
    end

    assert_equal [
      { entity: "Module", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
      { entity: "Module", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Module", caller_method_name: "new", caller_method_level: "class" },
      { entity: "#<Module:0xXXXXXX>", method_name: "extend", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Module", caller_method_name: "initialize", caller_method_level: "instance" },
      { entity: "MyModule", method_name: "extend_object", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "#<Module:0xXXXXXX>", caller_method_name: "extend", caller_method_level: "class" },
      { entity: "MyModule", method_name: "extended", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "#<Module:0xXXXXXX>", caller_method_name: "extend", caller_method_level: "class" },
      { entity: "#<Module:0xXXXXXX>", method_name: "module_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
    ], parse_and_normalize(contents)
  end

  private

  def parse_and_normalize(csv_string)
    CSV.parse(csv_string, headers: true, header_converters: :symbol).map do |row|
      row = row.to_h
      row[:lineno] = -1
      row[:filepath] = row[:filepath].gsub(ROOT_FIXTURE_PATH, '')
      row[:entity] = row[:entity].gsub(/:0x[a-fA-F0-9]{4,}/m, ":0xXXXXXX")
      if row.key?(:caller_entity)
        row[:caller_entity] = row[:caller_entity].gsub(/:0x[a-fA-F0-9]{4,}/m, ":0xXXXXXX")
      end
      row
    end
  end

  def assert_frames_consistent(csv_string)
    assert_equal csv_string.scan(/\Acall/).size, csv_string.scan(/\Areturn/).size
  end

  def rotoscope_trace(blacklist: [], flatten: false)
    Rotoscope.trace(@logfile, blacklist: blacklist, flatten: flatten) { |rotoscope| yield rotoscope }
    File.read(@logfile)
  end

  def unzip(path)
    File.open(path) { |f| Zlib::GzipReader.new(f).read }
  end
end

# https://github.com/seattlerb/minitest/pull/683 needed to use
# autorun without affecting the exit status of forked processes
exit Minitest.run(ARGV)
