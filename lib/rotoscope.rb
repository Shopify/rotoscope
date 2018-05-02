# frozen_string_literal: true
require 'rotoscope/rotoscope'
require 'fileutils'
require 'tempfile'
require 'csv'

class Rotoscope
  class << self
    def new(output_path, blacklist: [], flatten: false, header: nil)
      super(output_path, blacklist, flatten, header)
    end

    def trace(dest, blacklist: [], flatten: false, header: nil, &block)
      config = { blacklist: blacklist, flatten: flatten, header: header }
      if dest.is_a?(String)
        event_trace(dest, config, &block)
      else
        io_event_trace(dest, config, &block)
      end
    end

    private

    def with_temp_file(name)
      temp_file = Tempfile.new(name)
      yield temp_file
    ensure
      temp_file.close! if temp_file
    end

    def temp_event_trace(config, block)
      with_temp_file("rotoscope_output") do |temp_file|
        rs = event_trace(temp_file.path, config, &block)
        yield rs
        rs
      end
    end

    def io_event_trace(dest_io, config, &block)
      temp_event_trace(config, block) do |rs|
        File.open(rs.log_path) do |rs_file|
          IO.copy_stream(rs_file, dest_io)
        end
      end
    end

    def event_trace(dest_path, config)
      rs = Rotoscope.new(dest_path, blacklist: config[:blacklist], flatten: config[:flatten], header: config[:header])
      rs.trace { yield rs }
      rs
    ensure
      rs.close if rs
    end
  end

  def closed?
    state == :closed
  end

  def inspect
    "Rotoscope(state: #{state}, log_path: \"#{short_log_path}\", object_id: #{format('0x00%x', object_id << 1)})"
  end

  private

  def short_log_path
    return log_path if log_path.length < 40
    chars = log_path.chars
    "#{chars.first(17).join('')}...#{chars.last(20).join('')}"
  end
end
