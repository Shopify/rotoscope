require 'benchmark'
require 'rotoscope'
require_relative 'main'

def run_test_case
  o = Order.new
  o.process
end

Benchmark.bmbm(20) do |bm|
  bm.report('no trace') do
    10.times { run_test_case }
  end

  bm.report('c') do
    10.times { Rotoscope.trace { run_test_case } }
  end
end
