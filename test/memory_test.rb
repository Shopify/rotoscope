# frozen_string_literal: true

# Memory safety test using macOS `leaks` tool.
# Run: ruby -Ilib test/memory_test.rb

$LOAD_PATH.unshift(File.expand_path("../../lib", __FILE__))
require "rotoscope"
require "tempfile"

puts "PID: #{Process.pid}"
puts ""

# Exercise all code paths thoroughly before leak check
puts "Exercising all code paths..."

# 1. Native logger path (CallLogger)
100.times do
  tmp = Tempfile.new("memtest")
  Rotoscope::CallLogger.trace(tmp.path) do
    a = [1, 2, 3]
    a.map(&:to_s).join(",")
    a.select(&:odd?)
  end
  tmp.close!
end

# 2. Native logger with excludelist
50.times do
  tmp = Tempfile.new("memtest")
  Rotoscope::CallLogger.trace(tmp.path, excludelist: ["/gems/"]) do
    [1, 2, 3].map(&:to_s)
  end
  tmp.close!
end

# 3. Low-level API — noop callback
50.times do
  rs = Rotoscope.new { |_| }
  rs.trace { [1, 2, 3].map(&:to_s) }
end

# 4. Low-level API — read all attributes
50.times do
  rs = Rotoscope.new do |call|
    call.receiver
    call.receiver_class
    call.receiver_class_name
    call.method_name
    call.singleton_method?
    call.caller_object
    call.caller_class
    call.caller_class_name
    call.caller_method_name
    call.caller_singleton_method?
    call.caller_path
    call.caller_lineno
  end
  rs.trace { [1, 2, 3].map(&:to_s) }
end

# 5. No-block construction with native logger
50.times do
  tmp = Tempfile.new("memtest")
  rs = Rotoscope.new
  rs.set_native_logger(tmp, nil, rs)
  rs.start_trace
  [1, 2, 3].map(&:to_s)
  rs.stop_trace
  tmp.close!
end

# 6. Start/stop cycles
tmp = Tempfile.new("memtest")
rs = Rotoscope::CallLogger.new(tmp.path)
50.times do
  rs.start_trace
  [1, 2, 3].map(&:to_s)
  rs.stop_trace
end
rs.close
tmp.close!

# 7. Mark
tmp = Tempfile.new("memtest")
rs = Rotoscope::CallLogger.new(tmp.path)
rs.start_trace
10.times { |i| rs.mark("mark #{i}") }
rs.stop_trace
rs.close
tmp.close!

# 8. Methods with quotes (CSV escaping edge case)
klass = Class.new do
  define_method('has"quotes') { 42 }
end
tmp = Tempfile.new("memtest")
Rotoscope::CallLogger.trace(tmp.path) do
  50.times { klass.new.send('has"quotes') }
end
tmp.close!

# 9. Large trace (exercises buffer flush)
tmp = Tempfile.new("memtest")
Rotoscope::CallLogger.trace(tmp.path) do
  5000.times { [1, 2, 3].map(&:to_s).join(",") }
end
tmp.close!

# Force full GC to release everything reclaimable
5.times { GC.start(full_mark: true, immediate_sweep: true) }
GC.compact

puts "All paths exercised. Running leak check..."
puts ""

# Use macOS leaks tool on ourselves
result = `leaks #{Process.pid} 2>&1`
puts result

if result.include?("0 leaks")
  puts ""
  puts "RESULT: NO LEAKS"
  exit 0
else
  puts ""
  puts "RESULT: LEAKS DETECTED"
  exit 1
end
