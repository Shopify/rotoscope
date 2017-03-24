# frozen_string_literal: true
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

output_file = File.expand_path('dog_trace.log')
puts "Writing to #{output_file}..."

Rotoscope.trace(output_file, flatten: true) do
  dog1 = Dog.new
  dog1.bark
end
