# Rotoscope

Rotoscope performs introspection of method calls in Ruby programs.

## Example

```ruby
require 'rotoscope'
OUTPUT_PATH = File.join(Rails.root, 'logs/trace.log')

class Dog
  def bark
    make_sound('woof!')
  end
end

def make_sound(sound)
  puts sound
end

Rotoscope.trace(OUTPUT_PATH) do
  dog1 = Dog.new
  dog1.bark
end
```

The resulting method calls are saved in the specified `output_path` in the order they were received.

Sample output:

```
c_call,"Class","new","test.rb",16
c_call,"Dog","initialize","test.rb",16
c_return,"Dog","initialize","test.rb",16
c_return,"Class","new","test.rb",16
call,"Dog","bark","test.rb",4
call,"Dog","make_sound","test.rb",9
c_call,"Dog","puts","test.rb",10
c_call,"IO","puts","test.rb",10
c_call,"IO","write","test.rb",10
c_return,"IO","write","test.rb",10
c_call,"IO","write","test.rb",10
c_return,"IO","write","test.rb",10
c_return,"IO","puts","test.rb",10
c_return,"Dog","puts","test.rb",10
return,"Dog","make_sound","test.rb",11
return,"Dog","bark","test.rb",6
```

## API

### Public Class Methods

`Rotoscope::trace(output_path, blacklist=[])`

Logs all calls and returns of methods to `output_path`, except for those whose filepath contains any entry in `blacklist`. The provided `output_path` must be an absolute file path.

```ruby
Rotoscope.trace(output_path) { |rs| ... }
# or...
Rotoscope.trace(output_path, ["/.gem/", "/gems/"]) { |rs| ... }
```

`Rotoscope::new(output_path, blacklist=[])`

Similar to `Rotoscope::trace`, but allows fine-grain control with `Rotoscope#start_trace` and `Rotoscope#stop_trace`.
```ruby
rs = Rotoscope.new(output_path)
# or...
rs = Rotoscope.new(output_path, ["/.gem/", "/gems/"])
```

### Public Instance Methods

`Rotoscope#trace(&block)`

Same as `Rotoscope::trace`, but does not need to create a file handle on invocation.

```ruby
rs = Rotoscope.new(output_path)
rs.trace do
  # code to trace...
end
```

`Rotoscope#start_trace`

Begins writing method calls and returns to the `output_path` specified in the initializer.

```ruby
rs = Rotoscope.new(output_path)
rs.start_trace
# code to trace...
rs.stop_trace
```

`Rotoscope#stop_trace`

Stops writing method calls and returns to the `output_path`.

```ruby
rs = Rotoscope.new(output_path)
rs.start_trace
# code to trace...
rs.stop_trace
```
