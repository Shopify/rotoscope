# Rotoscope

Rotoscope performs introspection of method calls in Ruby programs.

## Status &nbsp; ![status](https://circleci.com/gh/Shopify/rotoscope/tree/master.svg?style=shield&circle-token=cddbd315df7a81ab944adf4dfc14a5800cd589fc)

Alpha!

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

The resulting method calls are saved in the specified `output_path` in the order they were received.

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

If you're interested solely in the flattened caller/callee list, you can pass the `flatten` option to retrieve that instead.

```ruby
# ... same code as above

Rotoscope.trace(log_file, flatten: true) do
  dog1 = Dog.new
  dog1.bark
end
```

Sample output:

```
entity,method_name,method_level,filepath,lineno,caller_entity,caller_method_name
Dog,new,class,example/flattened_dog.rb,19,<ROOT>,unknown
Dog,initialize,instance,example/flattened_dog.rb,19,Dog,new
Dog,bark,instance,example/flattened_dog.rb,20,<ROOT>,unknown
Noisemaker,speak,class,example/flattened_dog.rb,5,Dog,bark
Noisemaker,puts,class,example/flattened_dog.rb,11,Noisemaker,speak
IO,puts,instance,example/flattened_dog.rb,11,Noisemaker,puts
IO,write,instance,example/flattened_dog.rb,11,IO,puts
IO,write,instance,example/flattened_dog.rb,11,IO,puts
```

## API

- [Public Class Methods](#public-class-methods)
  - [`trace`](#rotoscopetraceoutput_path-blacklist--flatten-false-compress-false)
  - [`new`](#rotoscopenewoutput_path-blacklist)
- [Public Instance Methods](#public-instance-methods)
  - [`trace`](#rotoscopetraceblock)
  - [`start_trace`](#rotoscopestart_trace)
  - [`stop_trace`](#rotoscopestop_trace)
  - [`flatten`](#rotoscopeflatten)
  - [`flatten_into`](#rotoscopeflatten_intoio)
  - [`mark`](#rotoscopemark)
  - [`close`](#rotoscopeclose)
  - [`state`](#rotoscopestate)
  - [`closed?`](#rotoscopeclosed)

---

### Public Class Methods

#### `Rotoscope::trace(output_path, blacklist: [], flatten: false, compress: false)`

Logs all calls and returns of methods to `output_path`, except for those whose filepath contains any entry in `blacklist`. The provided `output_path` must be an absolute file path. `compress` will write a GZip-format file to the specified `output_path` if enabled. For details on the `flatten` option, see [`Rotoscope#flatten`](#rotoscopeflatten).

```ruby
Rotoscope.trace(output_path) { |rs| ... }
# or...
Rotoscope.trace(output_path, blacklist: ["/.gem/", "/gems/"]) { |rs| ... }
```

#### `Rotoscope::new(output_path, blacklist=[])`

Similar to `Rotoscope::trace`, but allows fine-grain control with `Rotoscope#start_trace` and `Rotoscope#stop_trace`.
```ruby
rs = Rotoscope.new(output_path)
# or...
rs = Rotoscope.new(output_path, ["/.gem/", "/gems/"])
```

---

### Public Instance Methods

#### `Rotoscope#trace(&block)`

Similar to `Rotoscope::trace`, but does not need to create a file handle on invocation.

```ruby
rs = Rotoscope.new(output_path)
rs.trace do |rotoscope|
  # code to trace...
end
```

#### `Rotoscope#start_trace`

Begins writing method calls and returns to the `output_path` specified in the initializer.

```ruby
rs = Rotoscope.new(output_path)
rs.start_trace
# code to trace...
rs.stop_trace
```

#### `Rotoscope#stop_trace`

Stops writing method invocations to the `output_path`. Subsequent calls to `Rotoscope#start_trace` may be invoked to resume tracing.

```ruby
rs = Rotoscope.new(output_path)
rs.start_trace
# code to trace...
rs.stop_trace
```

#### `Rotoscope#flatten(output_path)`
Reduces the output data to a list of method invocations and their caller, instead of all `call` and `return` events. Methods invoked at the top of the trace will have a caller entity of `<ROOT>` and a caller method name of `unknown`.


```ruby
rs = Rotoscope.new(output_path)
rs.trace { |rotoscope| ... }
rs.close
rs.flatten('tmp/flattened.csv')
```

#### `Rotoscope#flatten_into(io)`
The same as [`Rotoscope#flatten`](#rotoscopeflatten), but accepts an IO-like object for writing into.

```ruby
rs = Rotoscope.new(output_path)
rs.trace { |rotoscope| ... }
rs.close

Zlib::GzipWriter.open(output_path) do |gz|
  rs.flatten_into(gz)
end
```

#### `Rotoscope#mark`

 Inserts a marker '---' to divide output. Useful for segmenting multiple blocks of code that are being profiled.

```ruby
rs = Rotoscope.new(output_path)
rs.start_trace
# code to trace...
rs.mark
# more code ...
rs.stop_trace
```

#### `Rotoscope#close`

Flushes the buffer and closes the file handle. Once this is invoked, no more writes can be performed on the `Rotoscope` object. Sets `state` to `:RS_CLOSED`.

```ruby
rs = Rotoscope.new(output_path)
rs.trace { |rotoscope| ... }
rs.close
```

#### `Rotoscope#state`

Returns the current state of the Rotoscope object. Valid values are `:RS_OPEN` and `:RS_CLOSED`.

```ruby
rs = Rotoscope.new(output_path)
rs.state # :RS_OPEN
rs.close
rs.state # :RS_CLOSED
```

#### `Rotoscope#closed?`

Shorthand to check if the `state` is set to `:RS_CLOSED`.

```ruby
rs = Rotoscope.new(output_path)
rs.closed? # false
rs.close
rs.closed? # true
```
