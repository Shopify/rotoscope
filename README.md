# Rotoscope

Rotoscope performs introspection of method calls in Ruby programs.

## Status

[![status](https://circleci.com/gh/Shopify/rotoscope/tree/master.svg?style=shield&circle-token=cddbd315df7a81ab944adf4dfc14a5800cd589fc)](https://circleci.com/gh/Shopify/rotoscope/tree/master) [![Gem Version](https://badge.fury.io/rb/rotoscope.svg)](https://badge.fury.io/rb/rotoscope)

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

Rotoscope.trace(log_file) do
  dog1 = Dog.new
  dog1.bark
end
```

The resulting method calls are saved in the specified `dest` in the order they were received.

Sample output:

```
event,entity,method_name,method_level,filepath,lineno
call,"Dog","new",class,"example/dog.rb",19
call,"Dog","initialize",instance,"example/dog.rb",19
return,"Dog","initialize",instance,"example/dog.rb",19
return,"Dog","new",class,"example/dog.rb",19
call,"Dog","bark",instance,"example/dog.rb",4
call,"Noisemaker","speak",class,"example/dog.rb",10
call,"Noisemaker","puts",class,"example/dog.rb",11
call,"IO","puts",instance,"example/dog.rb",11
call,"IO","write",instance,"example/dog.rb",11
return,"IO","write",instance,"example/dog.rb",11
call,"IO","write",instance,"example/dog.rb",11
return,"IO","write",instance,"example/dog.rb",11
return,"IO","puts",instance,"example/dog.rb",11
return,"Noisemaker","puts",class,"example/dog.rb",11
return,"Noisemaker","speak",class,"example/dog.rb",12
return,"Dog","bark",instance,"example/dog.rb",6
```

If you're interested solely in the flattened caller/callee list, you can pass the `flatten` option to retrieve that instead. This step will also remove all duplicate lines, which can produce significantly smaller output on large codebases.

```ruby
# ... same code as above

Rotoscope.trace(log_file, flatten: true) do
  dog1 = Dog.new
  dog1.bark
end
```

Sample output:

```
entity,method_name,method_level,filepath,lineno,caller_entity,caller_method_name,caller_method_level
Dog,new,class,example/flattened_dog.rb,19,<ROOT>,<UNKNOWN>,<UNKNOWN>
Dog,initialize,instance,example/flattened_dog.rb,19,Dog,new,class
Dog,bark,instance,example/flattened_dog.rb,20,<ROOT>,<UNKNOWN>,<UNKNOWN>
Noisemaker,speak,class,example/flattened_dog.rb,5,Dog,bark,instance
Noisemaker,puts,class,example/flattened_dog.rb,11,Noisemaker,speak,class
IO,puts,instance,example/flattened_dog.rb,11,Noisemaker,puts,class
IO,write,instance,example/flattened_dog.rb,11,IO,puts,instance
```

## API

- [Public Class Methods](#public-class-methods)
  - [`trace`](#rotoscopetracedest-blacklist--flatten-false)
  - [`new`](#rotoscopenewdest-blacklist)
- [Public Instance Methods](#public-instance-methods)
  - [`trace`](#rotoscopetraceblock)
  - [`start_trace`](#rotoscopestart_trace)
  - [`stop_trace`](#rotoscopestop_trace)
  - [`mark`](#rotoscopemarkstr--)
  - [`close`](#rotoscopeclose)
  - [`state`](#rotoscopestate)
  - [`closed?`](#rotoscopeclosed)
  - [`log_path`](#rotoscopelog_path)

---

### Public Class Methods

#### `Rotoscope::trace(dest, blacklist: [], flatten: false)`

Writes all calls and returns of methods to `dest`, except for those whose filepath contains any entry in `blacklist`. `dest` is either a filename or an `IO`. The `flatten` option reduces the output data to a deduplicated list of method invocations and their caller, instead of all `call` and `return` events. Methods invoked at the top of the trace will have a caller entity of `<ROOT>` and a caller method name of `<UNKNOWN>`.

```ruby
Rotoscope.trace(dest) { |rs| ... }
# or...
Rotoscope.trace(dest, blacklist: ["/.gem/"], flatten: true) { |rs| ... }
```

#### `Rotoscope::new(dest, blacklist: [], flatten: false)`

Same interface as `Rotoscope::trace`, but returns a `Rotoscope` instance, allowing fine-grain control via `Rotoscope#start_trace` and `Rotoscope#stop_trace`.
```ruby
rs = Rotoscope.new(dest)
# or...
rs = Rotoscope.new(dest, blacklist: ["/.gem/"], flatten: true)
```

---

### Public Instance Methods

#### `Rotoscope#trace(&block)`

Similar to `Rotoscope::trace`, but does not need to create a file handle on invocation.

```ruby
rs = Rotoscope.new(dest)
rs.trace do |rotoscope|
  # code to trace...
end
```

#### `Rotoscope#start_trace`

Begins writing method calls and returns to the `dest` specified in the initializer.

```ruby
rs = Rotoscope.new(dest)
rs.start_trace
# code to trace...
rs.stop_trace
```

#### `Rotoscope#stop_trace`

Stops writing method invocations to the `dest`. Subsequent calls to `Rotoscope#start_trace` may be invoked to resume tracing.

```ruby
rs = Rotoscope.new(dest)
rs.start_trace
# code to trace...
rs.stop_trace
```

#### `Rotoscope#mark(str = "")`

 Inserts a marker '--- ' to divide output. Useful for segmenting multiple blocks of code that are being profiled. If `str` is provided, the line will be prefixed by '--- ', followed by the string passed.

```ruby
rs = Rotoscope.new(dest)
rs.start_trace
# code to trace...
rs.mark('Something goes wrong here') # produces `--- Something goes wrong here` in the output
# more code ...
rs.stop_trace
```

#### `Rotoscope#close`

Flushes the buffer and closes the file handle. Once this is invoked, no more writes can be performed on the `Rotoscope` object. Sets `state` to `:closed`.

```ruby
rs = Rotoscope.new(dest)
rs.trace { |rotoscope| ... }
rs.close
```

#### `Rotoscope#state`

Returns the current state of the Rotoscope object. Valid values are `:open`, `:tracing` and `:closed`.

```ruby
rs = Rotoscope.new(dest)
rs.state # :open
rs.trace do
  rs.state # :tracing
end
rs.close
rs.state # :closed
```

#### `Rotoscope#closed?`

Shorthand to check if the `state` is set to `:closed`.

```ruby
rs = Rotoscope.new(dest)
rs.closed? # false
rs.close
rs.closed? # true
```


#### `Rotoscope#log_path`

Returns the output filepath set in the Rotoscope constructor.

```ruby
dest = '/foo/bar/rotoscope.csv'
rs = Rotoscope.new(dest)
rs.log_path # "/foo/bar/rotoscope.csv"
```
