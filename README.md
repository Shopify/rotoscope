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

gzip_file = File.expand_path('dog_trace.log.gz')
puts "Writing to #{gzip_file}..."

Rotoscope.trace(gzip_file) do
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

## API

### Public Class Methods

#### `Rotoscope::trace(output_path, blacklist=[])`

Logs all calls and returns of methods to `output_path`, except for those whose filepath contains any entry in `blacklist`. The provided `output_path` must be an absolute file path.

```ruby
Rotoscope.trace(output_path) { |rs| ... }
# or...
Rotoscope.trace(output_path, ["/.gem/", "/gems/"]) { |rs| ... }
```

#### `Rotoscope::new(output_path, blacklist=[])`

Similar to `Rotoscope::trace`, but allows fine-grain control with `Rotoscope#start_trace` and `Rotoscope#stop_trace`.
```ruby
rs = Rotoscope.new(output_path)
# or...
rs = Rotoscope.new(output_path, ["/.gem/", "/gems/"])
```

### Public Instance Methods

#### `Rotoscope#trace(&block)`

Same as `Rotoscope::trace`, but does not need to create a file handle on invocation.

```ruby
rs = Rotoscope.new(output_path)
rs.trace do
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

Flushes the buffer and closes the file handle. Once this is invoked, no more writes can be performed on the `Rotoscope` object.

```ruby
rs = Rotoscope.new(output_path)
rs.start_trace
# code to trace...
rs.close
# more code ...
rs.stop_trace
```
