# Braze's Rotoscope
This is a fork of https://github.com/Shopify/rotoscope with a few modifications made by Braze to 
1. Add support for includelist to allow to only include in our results files within a given folder
2. Add support for prefix exclude to allow us to exclude files from our results using a prefix
3. Add suport for detailed vs simple mode. Detailed mode is the old way, simple was added to only include a subset of fields

---

# Rotoscope

Rotoscope is a high-performance logger of Ruby method invocations.

## Status

[![Build Status](https://github.com/Shopify/rotoscope/actions/workflows/ci.yml/badge.svg)](https://github.com/Shopify/rotoscope/actions?query=branch%3Amain)
[![Gem Version](https://badge.fury.io/rb/rotoscope.svg)](https://badge.fury.io/rb/rotoscope)

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

### Default Logging Interface

Rotoscope ships with a default logger, `Rotoscope::CallLogger`. This provides a simple-to-use interface to the tracing engine that maintains performance as much as possible.

- [`.trace`](#rotoscopecallloggertracedest-excludelist-)
- [`.new`](#rotoscopecallloggernewdest-excludelist-)
- [`#trace`](#rotoscopecallloggertraceblock)
- [`#start_trace`](#rotoscopecallloggerstart_trace)
- [`#stop_trace`](#rotoscopecallloggerstop_trace)
- [`#mark`](#rotoscopecallloggermarkstr--)
- [`#close`](#rotoscopecallloggerclose)
- [`#state`](#rotoscopecallloggerstate)
- [`#closed?`](#rotoscopecallloggerclosed)

#### `Rotoscope::CallLogger.trace(dest, excludelist: [])`

Writes all calls of methods to `dest`, except for those whose filepath contains any entry in `excludelist`. `dest` is either a filename or an `IO`. Methods invoked at the top of the trace will have a caller entity of `<ROOT>` and a caller method name of `<UNKNOWN>`.

```ruby
Rotoscope::CallLogger.trace(dest) { |call| ... }
# or...
Rotoscope::CallLogger.trace(dest, excludelist: ["/.gem/"]) { |call| ... }
```

#### `Rotoscope::CallLogger.new(dest, excludelist: [])`

Same interface as `Rotoscope::CallLogger::trace`, but returns a `Rotoscope::CallLogger` instance, allowing fine-grain control via `Rotoscope::CallLogger#start_trace` and `Rotoscope::CallLogger#stop_trace`.
```ruby
rs = Rotoscope::CallLogger.new(dest)
# or...
rs = Rotoscope::CallLogger.new(dest, excludelist: ["/.gem/"])
```

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

### Low-level API

For those who prefer to define their own logging logic, Rotoscope also provides a low-level API. This is the same one used by `Rotoscope::CallLogger` internally. Users may specify a block that is invoked on each detected method call.

- [`.new`](#rotoscopenewblk)
- [`#trace`](#rotoscopetraceblk)
- [`#start_trace`](#rotoscopestart_trace)
- [`#stop_trace`](#rotoscopestop_trace)
- [`#tracing?`](#rotoscopetracing)
- [`#receiver`](#rotoscopereceiver)
- [`#receiver_class`](#rotoscopereceiver_class)
- [`#receiver_class_name`](#rotoscopereceiver_class_name)
- [`#method_name`](#rotoscopemethod_name)
- [`#singleton_method?`](#rotoscopesingleton_method)
- [`#caller_object`](#rotoscopecaller_object)
- [`#caller_class`](#rotoscopecaller_class)
- [`#caller_class_name`](#rotoscopecaller_class_name)
- [`#caller_method_name`](#rotoscopecaller_method_name)
- [`#caller_singleton_method?`](#rotoscopecaller_singleton_method)
- [`#caller_path`](#rotoscopecaller_path)
- [`#caller_lineno`](#rotoscopecaller_lineno)

#### `Rotoscope.new(&blk)`

Creates a new instance of the `Rotoscope` class. The block argument is invoked on every call detected by Rotoscope. The block is passed the same instance returned by `Rotoscope#new` allowing the low-level methods to be called.

```ruby
rs = Rotoscope.new do |call|
  # We likely don't want to record calls to Rotoscope
  return if self == call.receiver
  ...
end
```


#### `Rotoscope#trace(&blk)`

The equivalent of calling [`Rotoscope#start_trace`](#rotoscopestart_trace) and then [`Rotoscope#stop_trace`](#rotoscopestop_trace). The call to `#stop_trace` is within an `ensure` block so it is always called when the block terminates.

```ruby
rs = Rotoscope.new do |call|
  ...
end

rs.trace do
  # call some code
end
```

#### `Rotoscope#start_trace`

Begins detecting method calls invoked after this point.

```ruby
rs = Rotoscope.new do |call|
  ...
end

rs.start_trace
# Calls after this points invoke the
# block passed to `Rotoscope.new`
```

#### `Rotoscope#stop_trace`

Disables method call detection invoked after this point.

```ruby
rs = Rotoscope.new do |call|
  ...
end

rs.start_trace
...
rs.stop_trace
# Calls after this points will no longer
# invoke the block passed to `Rotoscope.new`
```

#### `Rotoscope#tracing?`

Identifies whether the Rotoscope object is actively tracing method calls.

```ruby
rs = Rotoscope.new do |call|
  ...
end

rs.tracing? # => false
rs.start_trace
rs.tracing? # => true
```

#### `Rotoscope#receiver`

Returns the object that the method is being called against.

```ruby
rs = Rotoscope.new do |call|
  call.receiver # => #<Foo:0x00007fa3d2197c10>
end
```

#### `Rotoscope#receiver_class`

Returns the class of the object that the method is being called against.

```ruby
rs = Rotoscope.new do |call|
  call.receiver_class # => Foo
end
```

#### `Rotoscope#receiver_class_name`

Returns the stringified class of the object that the method is being called against.

```ruby
rs = Rotoscope.new do |call|
  call.receiver_class_name # => "Foo"
end
```

#### `Rotoscope#method_name`

Returns the name of the method being invoked.

```ruby
rs = Rotoscope.new do |call|
  call.method_name # => "bar"
end
```

#### `Rotoscope#singleton_method?`

Returns `true` if the method called is defined at the class level. If the call is to an instance method, this returns `false`.

```ruby
rs = Rotoscope.new do |call|
  call.singleton_method? # => false
end
```

#### `Rotoscope#caller_object`

Returns the object whose context we invoked the call from.

```ruby
rs = Rotoscope.new do |call|
  call.caller_object # => #<SomeClass:0x00008aa6d2cd91b61>
end
```

#### `Rotoscope#caller_class`

Returns the class of the object whose context we invoked the call from.

```ruby
rs = Rotoscope.new do |call|
  call.caller_class # => SomeClass
end
```

#### `Rotoscope#caller_class_name`

Returns the tringified class of the object whose context we invoked the call from.

```ruby
rs = Rotoscope.new do |call|
  call.caller_class_name # => "SomeClass"
end
```

#### `Rotoscope#caller_method_name`

Returns the stringified class of the object whose context we invoked the call from.

```ruby
rs = Rotoscope.new do |call|
  call.caller_method_name # => "call_foobar"
end
```

#### `Rotoscope#caller_singleton_method?`

Returns `true` if the method invoking the call is defined at the class level. If the call is to an instance method, this returns `false`.

```ruby
rs = Rotoscope.new do |call|
  call.caller_singleton_method? # => true
end
```

#### `Rotoscope#caller_path`

Returns the path to the file where the call was invoked.

```ruby
rs = Rotoscope.new do |call|
  call.caller_path # => "/rotoscope_test.rb"
end
```

#### `Rotoscope#caller_lineno`

Returns the line number corresponding to the `#caller_path` where the call was invoked. If unknown, returns `-1`.

```ruby
rs = Rotoscope.new do |call|
  call.caller_lineno # => 113
end
```
