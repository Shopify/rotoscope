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

class Example
  class << self
    def singleton_method
      true
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

  private

  def oops
    raise "I've made a terrible mistake"
  end
end

ROOT_FIXTURE_PATH = File.expand_path('../', __FILE__)
INNER_FIXTURE_PATH = File.expand_path('../fixture_inner.rb', __FILE__)
OUTER_FIXTURE_PATH = File.expand_path('../fixture_outer.rb', __FILE__)

class RotoscopeTest < MiniTest::Test
  def setup
    @logfile = File.expand_path('tmp/test.csv.gz')
  end

  def teardown
    FileUtils.remove_file(@logfile) if File.file?(@logfile)
  end

  def test_new
    rs = Rotoscope.new(@logfile)
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

    assert_includes contents.split("\n"), '---'
  end

  def test_flatten
    flatten_fh = Tempfile.new("flattened")
    rs = Rotoscope.new(@logfile)
    rs.trace { Example.new.normal_method }
    rs.close
    rs.flatten(flatten_fh.path)
    contents = flatten_fh.read

    assert_equal [
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "new", caller_method_level: "class" },
      { entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
    ], parse_and_normalize(contents)
  end

  def test_flatten_raises_if_handle_open
    flatten_fh = Tempfile.new("flattened")
    rs = Rotoscope.new(@logfile)
    rs.trace { Example.new.normal_method }

    refute rs.closed?
    assert_raises Rotoscope::InvalidStateError do
      rs.flatten(flatten_fh.path)
    end
  end

  def test_flatten_removes_orphaned_returns
    flatten_fh = Tempfile.new("flattened")
    rs = Rotoscope.new(@logfile)

    rs.start_trace
    Example.new.normal_method
    rs.stop_trace
    rs.close

    rs.flatten(flatten_fh.path)
    contents = flatten_fh.read

    assert_equal [
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "new", caller_method_level: "class" },
      { entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
    ], parse_and_normalize(contents)

    assert_frames_consistent contents
  end

  def test_flatten_supports_io_objects
    rs = Rotoscope.new(@logfile)
    rs.trace { Example.new.normal_method }
    rs.close

    contents = Tempfile.open('flattened_debug') do |tracefile|
      rs.flatten(tracefile)
      refute_predicate tracefile, :closed?
      tracefile.rewind
      tracefile.read
    end

    assert_equal [
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "new", caller_method_level: "class" },
      { entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
    ], parse_and_normalize(contents)
  end

  def test_flatten_supports_io_like_objects
    rs = Rotoscope.new(@logfile)
    rs.trace { Example.new.normal_method }
    rs.close

    zip_path = File.expand_path('../../tmp/trace.gz', __FILE__)
    Zlib::GzipWriter.open(zip_path) do |gz|
      rs.flatten(gz)
      refute_predicate gz, :closed?
    end
    contents = unzip(zip_path)

    assert_equal [
      { entity: "Example", method_name: "new", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
      { entity: "Example", method_name: "initialize", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "Example", caller_method_name: "new", caller_method_level: "class" },
      { entity: "Example", method_name: "normal_method", method_level: "instance", filepath: "/rotoscope_test.rb", lineno: -1, caller_entity: "<ROOT>", caller_method_name: "<UNKNOWN>", caller_method_level: "<UNKNOWN>" },
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
    Rotoscope.trace(string_io) do |rs|
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
    rs = Rotoscope.new(@logfile)
    rs.start_trace
    rs = nil
    GC.start
  end

  def test_gc_rotoscope_without_stop_trace_does_not_break_process_cleanup
    child_pid = fork do
      rs = Rotoscope.new(@logfile)
      rs.start_trace
    end
    Process.waitpid(child_pid)
    assert_equal true, $?.success?
  end

  def test_log_path
    rs = Rotoscope.new(File.expand_path('tmp/test.csv.gz'))
    GC.start
    assert_equal File.expand_path('tmp/test.csv.gz'), rs.log_path
  end

  private

  def parse_and_normalize(csv_string)
    CSV.parse(csv_string, headers: true, header_converters: :symbol).map do |row|
      row = row.to_h
      row[:lineno] = -1
      row[:filepath] = row[:filepath].gsub(ROOT_FIXTURE_PATH, '')
      row
    end
  end

  def assert_frames_consistent(csv_string)
    assert_equal csv_string.scan(/\Acall/).size, csv_string.scan(/\Areturn/).size
  end

  def rotoscope_trace(config = {})
    Rotoscope.trace(@logfile, config) { |rotoscope| yield rotoscope }
    File.read(@logfile)
  end

  def unzip(path)
    File.open(path) { |f| Zlib::GzipReader.new(f).read }
  end
end

# https://github.com/seattlerb/minitest/pull/683 needed to use
# autorun without affecting the exit status of forked processes
Minitest.run(ARGV)
