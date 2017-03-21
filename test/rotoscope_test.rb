# frozen_string_literal: true
$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)
$LOAD_PATH.unshift File.expand_path('../', __FILE__)
require 'rotoscope'
require 'minitest/autorun'
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
    FileUtils.remove_file(@logfile)
  end

  def test_instance_method
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

  def test_formats_singletons_of_a_class
    contents = rotoscope_trace { Example.singleton_method }
    assert_equal [
      { event: "call", entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 },
      { event: "return", entity: "Example", method_name: "singleton_method", method_level: "class", filepath: "/rotoscope_test.rb", lineno: -1 }
    ], parse_and_normalize(contents)

    assert_frames_consistent contents
  end

  def test_formats_singletons_of_an_instance
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

  def test_ignores_calls_if_blacklisted
    contents = rotoscope_trace([INNER_FIXTURE_PATH, OUTER_FIXTURE_PATH]) do
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

  def test_ignore_writes_in_fork
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

  def rotoscope_trace(blacklist = [])
    Rotoscope.trace(@logfile, blacklist) { |rotoscope| yield rotoscope }
    unzip(@logfile)
  end

  def unzip(path)
    File.open(path) { |f| Zlib::GzipReader.new(f).read }
  end
end
