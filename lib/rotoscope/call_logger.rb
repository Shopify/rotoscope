# frozen_string_literal: true

require "csv"
require 'set'

class Rotoscope
  class CallLogger
    UNSPECIFIED = Object.new
    private_constant :UNSPECIFIED

    class << self
      def trace(dest, blacklist: UNSPECIFIED, excludelist: [])
        if blacklist != UNSPECIFIED
          excludelist = blacklist
          warn("Rotoscope::CallLogger.trace blacklist argument is deprecated, use excludelist instead")
        end
        rs = new(dest, excludelist: excludelist)
        rs.trace { yield rs }
        rs
      ensure
        rs.io.close if rs && dest.is_a?(String)
      end
    end

    HEADER = "entity,caller_entity,filepath,lineno,method_name,method_level,caller_method_name,caller_method_level\n"
    SIMPLIFIED_HEADER = "file,test\n"

    attr_reader :io, :excludelist, :includelist, :prefix_to_exclude, :detailed

    def blacklist
      warn("Rotoscope::CallLogger#blacklist is deprecated, use excludelist instead")
      excludelist
    end

    def initialize(output = nil, blacklist: UNSPECIFIED, excludelist: nil, includelist: nil, prefix_to_exclude: nil, detailed: false)
      if blacklist != UNSPECIFIED
        excludelist = blacklist
        warn("Rotoscope::CallLogger#initialize blacklist argument is deprecated, use excludelist instead")
      end
      unless excludelist.is_a?(Regexp)
        excludelist = Regexp.union(excludelist || [])
      end
      unless includelist.is_a?(Regexp)
        includelist = Regexp.union(includelist || [])
      end
      @excludelist = excludelist
      @includelist = includelist
      @prefix_to_exclude = prefix_to_exclude
      @detailed = detailed

      if output.is_a?(String)
        @io = File.open(output, "w")
        prevent_flush_from_finalizer_in_fork(@io)
      else
        @io = output
      end
      @output_buffer = "".dup
      @pid = Process.pid
      @thread = Thread.current

      @files = Set.new()
      @test_file = nil

      if detailed
        @io << HEADER
      else
        @io << SIMPLIFIED_HEADER
      end

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

    def stop_trace()
      # If we are stopping trace and there are still files to be processed, process them
      process_files()
      @rotoscope.stop_trace
    end

    def process_files()
      if @files.length > 0 && !@test_file.nil?
        printFilesSet()
        # Reset state
        @test_file = nil
        @files = Set.new()
      end
    end

    def mark(message = "")
      was_tracing = @rotoscope.tracing?
      if was_tracing
        # stop tracing to avoid logging these io method calls
        @rotoscope.stop_trace
      end
      if @pid == Process.pid && @thread == Thread.current
        # Only output once a new file is being run
        if message != @test_file
          # This will process files and reset state
          process_files()
          # Update state to new message
          @test_file = message
        end
      end
    ensure
      @rotoscope.start_trace if was_tracing
    end

    def printFilesSet()
      @files.each do |file|
        # pattern = /(\'|\"|\.|\*|\/|\-|\\)/
        # test_file = @test_file.gsub(pattern){|match|"\\"  + match}.gsub("\n", "") # \\n
        buffer = @output_buffer
        buffer.clear
        buffer <<
          '"' << file << '",' \
            '"' << @test_file.to_s << '"' << "\n"
        io.write(buffer)
      end
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

    def log_call(call)
      caller_path = call.caller_path || ""
      return if excludelist.match?(caller_path) || !includelist.match?(caller_path)
      return if self == call.receiver

      if detailed
        caller_class_name = call.caller_class_name || "<UNKNOWN>"
        if call.caller_method_name.nil?
          caller_method_name = "<UNKNOWN>"
          caller_method_level = "<UNKNOWN>"
        else
          caller_method_name = escape_csv_string(call.caller_method_name)
          caller_method_level = call.caller_singleton_method? ? "class" : "instance"
        end

        call_method_level = call.singleton_method? ? "class" : "instance"
        method_name = escape_csv_string(call.method_name)

        buffer = @output_buffer
        buffer.clear
        buffer <<
          '"' << call.receiver_class_name << '",' \
            '"' << caller_class_name << '",' \
              '"' << caller_path << '",' \
          << call.caller_lineno.to_s << "," \
            '"' << method_name << '",' \
          << call_method_level << "," \
            '"' << caller_method_name << '",' \
          << caller_method_level << "\n"
        io.write(buffer)
      else
        if !prefix_to_exclude.nil?
          caller_path = caller_path.sub(Regexp.new(prefix_to_exclude), "")
        end
        if @pid == Process.pid && @thread == Thread.current && !@test_file.nil?
          @files.add(caller_path)
        end
      end
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
