require 'rotoscope'
require_relative 'main'

class Dog
  def foo
    3
  end
end

Rotoscope.trace do
  dog1 = Dog.new
  dog2 = Dog.new
  dog2.to_s
  dog1.foo
end
