# Rotoscope

Rotoscope is a high-performance logger of Ruby method invocations.

## Status

[![Build Status](https://travis-ci.org/Shopify/rotoscope.svg?branch=master)](https://travis-ci.org/Shopify/rotoscope) [![Gem Version](https://badge.fury.io/rb/rotoscope.svg)](https://badge.fury.io/rb/rotoscope)

Rotoscope is subject to breaking changes in minor versions until `1.0` is available.

## Example

```ruby
require 'rotoscope'

class Dog
  def bark
    Noisemaker.speak('woof!')
  end
end

class Noisemaker
  def self.speak(str)
    puts(str)
  end
end

log_file = File.expand_path('dog_trace.log')
puts "Writing to #{log_file}..."

Rotoscope::CallLogger.trace(log_file) do
  dog1 = Dog.new
  dog1.bark
end
```

The resulting method calls are saved in the specified `dest` in the order they were received.

Sample output:

```
entity,method_name,method_level,filepath,lineno,caller_entity,caller_method_name,caller_method_level
Dog,new,class,example/dog.rb,19,<ROOT>,<UNKNOWN>,<UNKNOWN>
Dog,initialize,instance,example/dog.rb,19,Dog,new,class
Dog,bark,instance,example/dog.rb,20,<ROOT>,<UNKNOWN>,<UNKNOWN>
Noisemaker,speak,class,example/dog.rb,5,Dog,bark,instance
Noisemaker,puts,class,example/dog.rb,11,Noisemaker,speak,class
IO,puts,instance,example/dog.rb,11,Noisemaker,puts,class
IO,write,instance,example/dog.rb,11,IO,puts,instance
IO,write,instance,example/dog.rb,11,IO,puts,instance
```

## API

- [Public Class Methods](#public-class-methods)
  - [`trace`](#rotoscopecallloggertracedest-blacklist-)
  - [`new`](#rotoscopecallloggernewdest-blacklist-)
- [Public Instance Methods](#public-instance-methods)
  - [`trace`](#rotoscopecallloggertraceblock)
  - [`start_trace`](#rotoscopecallloggerstart_trace)
  - [`stop_trace`](#rotoscopecallloggerstop_trace)
  - [`mark`](#rotoscopecallloggermarkstr--)
  - [`close`](#rotoscopecallloggerclose)
  - [`state`](#rotoscopecallloggerstate)
  - [`closed?`](#rotoscopecallloggerclosed)

---

### Public Class Methods

#### `Rotoscope::CallLogger::trace(dest, blacklist: [])`

Writes all calls of methods to `dest`, except for those whose filepath contains any entry in `blacklist`. `dest` is either a filename or an `IO`. Methods invoked at the top of the trace will have a caller entity of `<ROOT>` and a caller method name of `<UNKNOWN>`.

```ruby
Rotoscope::CallLogger.trace(dest) { |rs| ... }
# or...
Rotoscope::CallLogger.trace(dest, blacklist: ["/.gem/"]) { |rs| ... }
```

#### `Rotoscope::CallLogger::new(dest, blacklist: [])`

Same interface as `Rotoscope::CallLogger::trace`, but returns a `Rotoscope::CallLogger` instance, allowing fine-grain control via `Rotoscope::CallLogger#start_trace` and `Rotoscope::CallLogger#stop_trace`.
```ruby
rs = Rotoscope::CallLogger.new(dest)
# or...
rs = Rotoscope::CallLogger.new(dest, blacklist: ["/.gem/"])
```

---

### Public Instance Methods

#### `Rotoscope::CallLogger#trace(&block)`

Similar to `Rotoscope::CallLogger::trace`, but does not need to create a file handle on invocation.

```ruby
rs = Rotoscope::CallLogger.new(dest)
rs.trace do |rotoscope|
  # code to trace...
end
```

#### `Rotoscope::CallLogger#start_trace`

Begins writing method calls to the `dest` specified in the initializer.

```ruby
rs = Rotoscope::CallLogger.new(dest)
rs.start_trace
# code to trace...
rs.stop_trace
```

#### `Rotoscope::CallLogger#stop_trace`

Stops writing method invocations to the `dest`. Subsequent calls to `Rotoscope::CallLogger#start_trace` may be invoked to resume tracing.

```ruby
rs = Rotoscope::CallLogger.new(dest)
rs.start_trace
# code to trace...
rs.stop_trace
```

#### `Rotoscope::CallLogger#mark(str = "")`

 Inserts a marker '--- ' to divide output. Useful for segmenting multiple blocks of code that are being profiled. If `str` is provided, the line will be prefixed by '--- ', followed by the string passed.

```ruby
rs = Rotoscope::CallLogger.new(dest)
rs.start_trace
# code to trace...
rs.mark('Something goes wrong here') # produces `--- Something goes wrong here` in the output
# more code ...
rs.stop_trace
```

#### `Rotoscope::CallLogger#close`

Flushes the buffer and closes the file handle. Once this is invoked, no more writes can be performed on the `Rotoscope::CallLogger` object. Sets `state` to `:closed`.

```ruby
rs = Rotoscope::CallLogger.new(dest)
rs.trace { |rotoscope| ... }
rs.close
```

#### `Rotoscope::CallLogger#state`

Returns the current state of the Rotoscope::CallLogger object. Valid values are `:open`, `:tracing` and `:closed`.

```ruby
rs = Rotoscope::CallLogger.new(dest)
rs.state # :open
rs.trace do
  rs.state # :tracing
end
rs.close
rs.state # :closed
```

#### `Rotoscope::CallLogger#closed?`

Shorthand to check if the `state` is set to `:closed`.

```ruby
rs = Rotoscope::CallLogger.new(dest)
rs.closed? # false
rs.close
rs.closed? # true
```
