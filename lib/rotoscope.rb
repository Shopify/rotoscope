# frozen_string_literal: true
require 'rotoscope/rotoscope'
require 'fileutils'
require 'tempfile'
require 'csv'

class Rotoscope
  InvalidStateError = Class.new(StandardError)

  def self.trace(dest, blacklist: [], flatten: false)
    io_given = false
    dest_file = if flatten
      Tempfile.new("rotoscope_output")
    elsif dest.respond_to?(:to_io)
      io_given = true
      dest.to_io
    else
      File.open(dest, 'w')
    end

    begin
      rs = Rotoscope.new(dest_file.path, blacklist)
      rs.trace { yield rs }
    ensure
      rs.close
    end
    rs.flatten(dest) if flatten
    rs
  ensure
    unless io_given
      dest_file.close if dest_file.nil?
      dest_file.unlink if dest_file.is_a?(Tempfile)
    end
  end

  def flatten(dest)
    io_given = false
    dest_file = if dest.respond_to?(:to_io)
      io_given = true
      dest.to_io
    else
      File.open(dest, 'w')
    end

    flatten_into(dest_file)
  ensure
    dest_file.close unless io_given
  end

  def closed?
    state == :closed
  end

  def inspect
    "Rotoscope(state: #{state}, log_path: \"#{short_log_path}\", object_id: #{format('0x00%x', object_id << 1)})"
  end

  private

  Caller = Struct.new(:entity, :method_name)
  DEFAULT_CALLER = Rotoscope::Caller.new('<ROOT>', 'unknown').freeze
  CSV_HEADER_FIELDS = %w(entity method_name method_level filepath lineno caller_entity caller_method_name)

  def flatten_into(io)
    raise(Rotoscope::InvalidStateError, "#{inspect} must be closed to perform operation") unless closed?

    call_stack = []
    io.puts(CSV_HEADER_FIELDS.join(','))

    CSV.foreach(log_path, headers: true) do |line|
      case line.fetch('event')
      when '---'
        call_stack = []
      when 'call'
        caller = call_stack.last || DEFAULT_CALLER
        line << { 'caller_entity' => caller.entity, 'caller_method_name' => caller.method_name }
        call_stack << Rotoscope::Caller.new(line.fetch('entity'), line.fetch('method_name'))
        out_str = CSV_HEADER_FIELDS.map { |field| line.fetch(field) }.join(',')
        io.puts(out_str)
      when 'return'
        caller = Rotoscope::Caller.new(line.fetch('entity'), line.fetch('method_name'))
        call_stack.pop if call_stack.last == caller
      end
    end
  end

  def short_log_path
    return log_path if log_path.length < 40
    chars = log_path.chars
    "#{chars.first(17).join('')}...#{chars.last(20).join('')}"
  end
end
