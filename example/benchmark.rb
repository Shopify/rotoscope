require 'benchmark'
require 'rotoscope'
require_relative 'main'

def run_test_case
  o = Order.new
  o.process
end

Benchmark.bmbm(100) do |bm|
  bm.report('no trace') do
    10.times { run_test_case }
  end

  bm.report('c') do
    r = Rotoscope.new(serialize: :c)
    10.times { r.trace { run_test_case } }
  end

  bm.report('msgpack') do
    r = Rotoscope.new(serialize: :msgpack)
    10.times { r.trace { run_test_case } }
  end
end
