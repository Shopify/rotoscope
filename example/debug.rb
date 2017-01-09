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
