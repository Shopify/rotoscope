# Rotoscope

Rotoscope performs introspection of method calls in Ruby programs.

## Usage
```
$ rake install
```

```ruby
require 'rotoscope'

class Dog
  def bark
    make_sound('woof!')
  end
end

def make_sound(sound)
  puts sound
end

Rotoscope.trace do
  dog1 = Dog.new
  dog1.bark
end
```

The resulting method calls are saved in `/tmp/trace.log` in the order they were received.

Sample output:

```
c_call   > Class#new
  c_call   > Dog#initialize
  c_return > Dog#initialize
c_return > Class#new
call     > Dog#bark
  call     > Dog#make_sound
    c_call   > Dog#puts
      c_call   > IO#puts
        c_call   > IO#write
        c_return > IO#write
        c_call   > IO#write
        c_return > IO#write
      c_return > IO#puts
    c_return > Dog#puts
  return   > Dog#make_sound
return   > Dog#bark
```

Optionally, you may provide a blacklist of paths to ignore. This is useful for limiting the footprint of the output file as well as improving performance in hotspots.

```ruby
BLACKLIST = ['/.gem/', '/lib/ruby/', '(eval)']
Rotoscope.trace(BLACKLIST) { ... }
```
