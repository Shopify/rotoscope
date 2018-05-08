# frozen_string_literal: true
require 'rotoscope/rotoscope'
require 'csv'

class Rotoscope
  class << self
    def new(output, blacklist: [])
      if output.is_a?(String)
        io = File.open(output, 'w')
        prevent_flush_from_finalizer_in_fork(io)
        obj = super(io, blacklist)
        obj.log_path = output
        obj
      else
        super(output, blacklist)
      end
    end

    def trace(dest, blacklist: [])
      rs = new(dest, blacklist: blacklist)
      rs.trace { yield rs }
      rs
    ensure
      rs.close if rs && dest.is_a?(String)
    end

    private

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

  attr_accessor :log_path

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
