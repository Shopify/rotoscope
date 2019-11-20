# frozen_string_literal: true

require 'csv'

class Rotoscope
  class CallLogger
    class << self
      def trace(dest, blacklist: [])
        rs = new(dest, blacklist: blacklist)
        rs.trace { yield rs }
        rs
      ensure
        rs.io.close if rs && dest.is_a?(String)
      end
    end

    Error = Class.new(StandardError)
    InvalidHeader = Class.new(Error)
    EmptyHeader = Class.new(Error)

    POSSIBLE_HEADER_VALUES = [
      :entity, :caller_entity, :filepath, :lineno, :method_name,
      :method_level, :caller_method_name, :caller_method_level
    ]

    attr_reader :io, :blacklist

    def initialize(output = nil, blacklist: nil, header: nil)
      unless blacklist.is_a?(Regexp)
        blacklist = Regexp.union(blacklist || [])
      end
      @blacklist = blacklist

      if output.is_a?(String)
        @io = File.open(output, 'w')
        prevent_flush_from_finalizer_in_fork(@io)
      else
        @io = output
      end
      @output_buffer = ''.dup
      @pid = Process.pid
      @thread = Thread.current

      @header = header || POSSIBLE_HEADER_VALUES

      invalid_header = @header - POSSIBLE_HEADER_VALUES
      raise InvalidHeader, "Invalid headers defined #{invalid_hearder}" unless invalid_header.empty?
      raise EmptyHeader, "Header empty" if @header.empty?

      @io << @header.join(',')
      @io << "\n"

      @rotoscope = Rotoscope.new(&method(:log_call))
    end

    def trace
      start_trace
      yield
    ensure
      @rotoscope.stop_trace
    end

    def start_trace
      @rotoscope.start_trace
    end

    def stop_trace
      @rotoscope.stop_trace
    end

    def mark(message = "")
      was_tracing = @rotoscope.tracing?
      if was_tracing
        # stop tracing to avoid logging these io method calls
        @rotoscope.stop_trace
      end
      if @pid == Process.pid && @thread == Thread.current
        @io.write("--- ")
        @io.puts(message)
      end
    ensure
      @rotoscope.start_trace if was_tracing
    end

    def close
      @rotoscope.stop_trace
      if @pid == Process.pid && @thread == Thread.current
        @io.close
      end
      true
    end

    def closed?
      @io.closed?
    end

    def state
      return :closed if io.closed?
      @rotoscope.tracing? ? :tracing : :open
    end

    private

    def entity(call)
      call.receiver_class_name
    end

    def caller_entity(call)
      call.caller_class_name || '<UNKNOWN>'
    end

    def filepath(call)
      call.caller_path || ''
    end

    def lineno(call)
      call.caller_lineno.to_s
    end

    def method_name(call)
      escape_csv_string(call.method_name)
    end

    def method_level(call)
      call.singleton_method? ? 'class' : 'instance'
    end

    def caller_method_name(call)
      if call.caller_method_name.nil?
        '<UNKNOWN>'
      else
        escape_csv_string(call.caller_method_name)
      end
    end

    def caller_method_level(call)
      if call.caller_method_name.nil?
        '<UNKNOWN>'
      else
        call.caller_singleton_method? ? 'class' : 'instance'
      end
    end

    def log_call(call)
      return if blacklist.match?(filepath(call))
      return if self == call.receiver

      buffer = @output_buffer
      buffer.clear

      # TODO: check for last line
      if @header.size > 1
        @header[0...-1].each do |head|
          buffer << '"' << send(head, call) << '",'
        end
      end
      buffer << send(@header.last, call) << "\n"
      io.write(buffer)
    end

    def escape_csv_string(string)
      string.include?('"') ? string.gsub('"', '""') : string
    end

    def prevent_flush_from_finalizer_in_fork(io)
      pid = Process.pid
      finalizer = lambda do |_|
        next if Process.pid == pid
        # close the file descriptor from another IO object so
        # buffered writes aren't flushed
        IO.for_fd(io.fileno).close
      end
      ObjectSpace.define_finalizer(io, finalizer)
    end
  end
end
