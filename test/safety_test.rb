# frozen_string_literal: true

$LOAD_PATH.unshift(File.expand_path("../../lib", __FILE__))
require "rotoscope"
require "tempfile"
require "csv"

$pass_count = 0
$fail_count = 0

def check(name)
  print "  #{name}... "
  begin
    yield
    puts "PASS"
    $pass_count += 1
  rescue => e
    puts "FAIL: #{e.message}"
    $fail_count += 1
  end
end

puts "=== COMPREHENSIVE SAFETY TESTS ==="
puts ""

# 1. GC stress — native logger path
check("GC stress (native logger)") do
  GC.stress = true
  10.times do
    tmp = Tempfile.new("test")
    Rotoscope::CallLogger.trace(tmp.path) { [1, 2, 3].map(&:to_s) }
    tmp.close!
  end
ensure
  GC.stress = false
end

# 2. GC stress — proc callback path
check("GC stress (proc callback)") do
  GC.stress = true
  10.times do
    rs = Rotoscope.new { |c| c.receiver_class_name; c.caller_path }
    rs.trace { [1, 2, 3].map(&:to_s) }
  end
ensure
  GC.stress = false
end

# 3. GC stress — excludelist path
check("GC stress (with excludelist)") do
  GC.stress = true
  5.times do
    tmp = Tempfile.new("test")
    Rotoscope::CallLogger.trace(tmp.path, excludelist: ["/gems/"]) { [1, 2, 3].map(&:to_s) }
    tmp.close!
  end
ensure
  GC.stress = false
end

# 4. Leak test
check("Leak test") do
  3.times { GC.start }
  run = lambda do
    500.times do
      tmp = Tempfile.new("test")
      Rotoscope::CallLogger.trace(tmp.path) { [1, 2, 3].map(&:to_s).join(",") }
      tmp.close!
    end
  end
  run.call
  3.times { GC.start }
  before = GC.stat[:total_allocated_objects]
  run.call
  3.times { GC.start }
  after = GC.stat[:total_allocated_objects]
  allocated = after - before

  run.call
  3.times { GC.start }
  after2 = GC.stat[:total_allocated_objects]
  allocated2 = after2 - after

  growth = (allocated2 - allocated).abs
  raise "possible leak: growth=#{growth}" if growth > 200
end

# 5. Fork safety
check("Fork safety") do
  tmp = Tempfile.new("fork_test")
  rs = Rotoscope::CallLogger.new(tmp.path)
  rs.start_trace
  pid = fork { [1, 2, 3].map(&:to_s); exit!(0) }
  Process.wait(pid)
  rs.stop_trace
  rs.close
  raise "child crashed" unless $?.exitstatus == 0
  tmp.close!
end

# 6. Thread safety
check("Thread safety") do
  tmp = Tempfile.new("thread_test")
  Rotoscope::CallLogger.trace(tmp.path) do
    threads = 5.times.map { Thread.new { 100.times { [1, 2, 3].map(&:to_s) } } }
    threads.each(&:join)
  end
  tmp.close!
end

# 7. Large trace (buffer flush)
check("Large trace (buffer flush)") do
  tmp = Tempfile.new("large_test")
  Rotoscope::CallLogger.trace(tmp.path) do
    10_000.times { [1, 2, 3].map(&:to_s).join(",") }
  end
  lines = File.read(tmp.path).lines.count
  raise "only #{lines} lines" unless lines > 1000
  tmp.close!
end

# 8. Singleton/module/class methods
check("Singleton/module methods") do
  tmp = Tempfile.new("singleton_test")
  m = Module.new do
    def self.foo
      42
    end
  end
  Rotoscope::CallLogger.trace(tmp.path) do
    m.foo
    Class.new { def bar; end }.new.bar
  end
  lines = File.read(tmp.path).lines.count
  raise "only #{lines} lines" unless lines > 1
  tmp.close!
end

# 9. Methods with quotes in names (CSV escaping)
check("Quote escaping in CSV") do
  tmp = Tempfile.new("quote_test")
  klass = Class.new do
    define_method('has"quotes') { 42 }
  end
  Rotoscope::CallLogger.trace(tmp.path) do
    klass.new.send('has"quotes')
  end
  content = File.read(tmp.path)
  parsed = CSV.parse(content, headers: true)
  raise "CSV parse failed" if parsed.count < 1
  tmp.close!
end

# 10. Mark/separator
check("Mark/separator") do
  tmp = Tempfile.new("mark_test")
  rs = Rotoscope::CallLogger.new(tmp.path)
  rs.start_trace
  [1].map(&:to_s)
  rs.mark("checkpoint")
  [2].map(&:to_s)
  rs.stop_trace
  rs.close
  content = File.read(tmp.path)
  raise "mark not found" unless content.include?("--- checkpoint")
  tmp.close!
end

# 11. State transitions
check("State transitions") do
  tmp = Tempfile.new("state_test")
  rs = Rotoscope::CallLogger.new(tmp.path)
  raise "expected :open" unless rs.state == :open
  rs.start_trace
  raise "expected :tracing" unless rs.state == :tracing
  rs.stop_trace
  raise "expected :open" unless rs.state == :open
  rs.close
  raise "expected :closed" unless rs.state == :closed
  raise "expected closed?" unless rs.closed?
  tmp.close!
end

# 12. Low-level API — all accessors
check("Low-level API accessors") do
  results = []
  rs = Rotoscope.new do |call|
    results << {
      receiver: call.receiver,
      receiver_class: call.receiver_class,
      receiver_class_name: call.receiver_class_name,
      method_name: call.method_name,
      singleton_method: call.singleton_method?,
      caller_object: call.caller_object,
      caller_class: call.caller_class,
      caller_class_name: call.caller_class_name,
      caller_method_name: call.caller_method_name,
      caller_singleton_method: call.caller_singleton_method?,
      caller_path: call.caller_path,
      caller_lineno: call.caller_lineno,
    }
  end
  rs.trace { "hello".upcase }
  raise "no results" if results.empty?
  r = results.first
  raise "bad method_name" unless r[:method_name] == "upcase"
  raise "bad receiver_class_name" unless r[:receiver_class_name] == "String"
end

# 13. Excludelist actually filters
check("Excludelist filtering") do
  tmp = Tempfile.new("exclude_test")
  Rotoscope::CallLogger.trace(tmp.path, excludelist: [File.dirname(__FILE__)]) do
    [1, 2, 3].map(&:to_s)
  end
  content = File.read(tmp.path)
  # With our file excluded, only the header should remain (or very few lines)
  lines = content.lines.count
  raise "excludelist not working: #{lines} lines" if lines > 5
  tmp.close!
end

# 14. Repeated start/stop cycles
check("Repeated start/stop") do
  tmp = Tempfile.new("cycle_test")
  rs = Rotoscope::CallLogger.new(tmp.path)
  10.times do
    rs.start_trace
    [1, 2, 3].map(&:to_s)
    rs.stop_trace
  end
  rs.close
  lines = File.read(tmp.path).lines.count
  raise "only #{lines} lines" unless lines > 10
  tmp.close!
end

# 15. Rotoscope.new without block (native-only)
check("No-block construction") do
  rs = Rotoscope.new
  rs.set_native_logger($stdout, nil, rs)
  rs.start_trace
  [1].map(&:to_s)
  rs.stop_trace
end

puts ""
puts "=== DONE ==="
