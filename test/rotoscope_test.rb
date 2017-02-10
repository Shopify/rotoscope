# frozen_string_literal: true
$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)
require 'rotoscope'
require 'minitest/autorun'
require 'zlib'
require 'fileutils'
require 'csv'

class Example
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

class RotoscopeTest < MiniTest::Test
  def setup
    @logfile = File.expand_path('tmp/test.csv.gz')
  end

  def teardown
    # FileUtils.remove_file(@logfile)
  end

  def test_basic
    contents = rotoscope_trace { Example.new.normal_method }
    assert_equal contents.scan(/\Acall/).size, contents.scan(/\Areturn/).size
    csv_rows = contents.split("\n")
    row = CSV.parse_line(csv_rows[2], headers: csv_rows[0])
    assert_equal "Example", row.fetch("entity")
    assert_equal "initialize", row.fetch("method_name")
    assert_equal "instance", row.fetch("method_level")
  end

  def test_exception
    contents = rotoscope_trace { Example.new.exception_method }
    assert_equal contents.scan(/\Acall/).size, contents.scan(/\Areturn/).size
  end

  private

  def rotoscope_trace
    Rotoscope.trace(@logfile) { yield }
    unzip(@logfile)
  end

  def unzip(path)
    File.open(path) { |f| Zlib::GzipReader.new(f).read }
  end
end
