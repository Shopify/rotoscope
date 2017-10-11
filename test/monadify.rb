module Monadify
  define_method("contents") do
    42
  end

  def foo
    false
  end

  def monad(value)
    foo
    contents
    define_singleton_method("contents=") { |val| val }
    self.contents = value
  end
end
