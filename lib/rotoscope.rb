# frozen_string_literal: true
require 'rotoscope/rotoscope'
require 'fileutils'
require 'tempfile'
require 'zlib'
require 'csv'

class Rotoscope
  InvalidStateError = Class.new(StandardError)

  def self.trace(output_path, blacklist: [], compress: false, flatten: false)
    @rs_tmpfile = Tempfile.new("rotoscope_output")
    rs = new(@rs_tmpfile.path, blacklist)
    rs.trace { yield rs }
    rs.close

    out_fh = compress ? Zlib::GzipWriter.open(output_path) : File.new(output_path, 'w')
    if flatten
      rs.flatten_into(out_fh)
    else
      IO.copy_stream(@rs_tmpfile.path, out_fh)
    end
    out_fh.close

    @rs_tmpfile.close
    @rs_tmpfile.unlink
    rs
  end

  def flatten(output_path)
    File.open(output_path, 'w') do |fh|
      flatten_into(fh)
    end
  end

  def closed?
    state == :RS_CLOSED
  end

  Caller = Struct.new(:entity, :method_name)
  DEFAULT_CALLER = Caller.new('<ROOT>', 'unknown').freeze
  CSV_HEADER_FIELDS = %w(entity method_name method_level filepath lineno caller_entity caller_method_name)

  def flatten_into(io)
    raise(Rotoscope::InvalidStateError, "Rotoscope handle must be closed to perform operation") unless closed?

    io.puts(CSV_HEADER_FIELDS.join(','))
    call_stack = []
    CSV.foreach(log_path, headers: true) do |line|
      case line.fetch('event')
      when '---'
        call_stack = []
      when 'call'
        caller = call_stack.last || DEFAULT_CALLER
        line << { 'caller_entity' => caller.entity, 'caller_method_name' => caller.method_name }

        out_str = CSV_HEADER_FIELDS.map { |field| line.fetch(field) }.join(',')
        call_stack << Caller.new(line.fetch('entity'), line.fetch('method_name'))
        io.puts(out_str)
      when 'return'
        caller = Caller.new(line.fetch('entity'), line.fetch('method_name'))
        if call_stack.last == caller
          call_stack.pop
        end
      end
    end
  end
end
