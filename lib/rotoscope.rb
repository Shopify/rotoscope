# frozen_string_literal: true
require 'rotoscope/rotoscope'

class Rotoscope
  autoload :CallLogger, 'rotoscope/call_logger'

  def trace
    start_trace
    yield
  ensure
    stop_trace
  end
end
