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

rs = Rotoscope.new(OUTPUT_PATH)
rs.trace do
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

### Rotoscope#new(output_path, blacklist=[])
```ruby
rs = Rotoscope.new(output_path)
# or...
rs = Rotoscope.new(output_path, %w(/.gem/ /gems/))
```

### Rotoscope#trace(&block)
```ruby
rs.trace do
  # code to trace...
end
```

### Rotoscope#start_trace
```ruby
rs.start_trace
# code to trace...
rs.stop_trace
```

### Rotoscope#stop_trace
```ruby
rs.start_trace
# code to trace...
rs.stop_trace
```
