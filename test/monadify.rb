# frozen_string_literal: true

module Monadify
  class << self
    def extended(base)
      base.define_singleton_method("contents=") { |val| val }
    end
  end

  define_method("contents") do
    42
  end

  def monad(value)
    contents
    self.contents = value
  end
end
