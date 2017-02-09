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

gzip_file = File.expand_path('tmp/dog_trace.log.gz')

puts "Writing to #{gzip_file}..."
Rotoscope.trace(gzip_file) do
  dog1 = Dog.new
  dog1.bark
end
