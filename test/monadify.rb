module Monadify
  define_singleton_method("contents=") { |val| val }

  define_singleton_method("contents") do
    42
  end

  def monad(value)
    Monadify.contents
    Monadify.contents = value
  end
end
