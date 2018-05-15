# frozen_string_literal: true
require 'rotoscope/rotoscope'
require 'csv'

class Rotoscope
  HEADER = "entity,caller_entity,filepath,lineno,method_name,method_level,caller_method_name,caller_method_level\n"
  private_constant :HEADER

  class << self
    def trace(dest, blacklist: [])
      rs = new(dest, blacklist: blacklist)
      rs.trace { yield rs }
      rs
    ensure
      rs.io.close if rs && dest.is_a?(String)
    end
  end

  attr_reader :io, :log_path, :blacklist

  def initialize(output = nil, blacklist: nil, &block)
    if block
      unless output.nil? || blacklist.nil?
        raise ArgumentError, "Cannot provide output or blacklist with trace block"
      end
      initialize_ext(&block)
    else
      unless blacklist.is_a?(Regexp)
        blacklist = Regexp.union(blacklist || [])
      end
      @blacklist = blacklist

      if output.is_a?(String)
        @log_path = output
        @io = File.open(output, 'w')
        prevent_flush_from_finalizer_in_fork(@io)
      else
        @log_path = nil
        @io = output
      end
      @output_buffer = ''.dup
      initialize_ext(&method(:log_call))

      io << HEADER
    end

    @pid = Process.pid
    @thread = Thread.current
  end

  def trace
    start_trace
    yield
  ensure
    stop_trace
  end

  def mark(message = "")
    was_tracing = tracing?
    if was_tracing
      # stop tracing to avoid logging these io method calls
      stop_trace
    end
    if @pid == Process.pid && @thread == Thread.current
      io.write("--- ")
      io.puts(message)
    end
  ensure
    start_trace if was_tracing
  end

  def close
    stop_trace
    if @pid == Process.pid && @thread == Thread.current
      io.close
    end
    true
  end

  def closed?
    io.closed?
  end

  def state
    return :closed if io.closed?
    tracing? ? :tracing : :open
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

  def log_call(_rotoscope)
    return if blacklist.match?(caller_path)

    if caller_method_name.nil?
      caller_class_name = '<ROOT>'
      caller_method_name = '<UNKNOWN>'
      caller_method_level = '<UNKNOWN>'
    else
      caller_class_name = self.caller_class_name
      caller_method_name = escape_csv_string(self.caller_method_name)
      caller_method_level = caller_singleton_method? ? 'class' : 'instance'
    end

    call_method_level = singleton_method? ? 'class' : 'instance'
    method_name = escape_csv_string(self.method_name)

    buffer = @output_buffer
    buffer.clear
    buffer <<
      '"' << class_name << '",' \
      '"' << caller_class_name << '",' \
      '"' << caller_path << '",' \
      << caller_lineno.to_s << ',' \
      '"' << method_name << '",' \
      << call_method_level << ',' \
      '"' << caller_method_name << '",' \
      << caller_method_level << "\n"
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
