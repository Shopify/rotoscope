# frozen_string_literal: true
class FixtureOuter
  def initialize
    @inner = FixtureInner.new
  end

  def do_work
    @inner.do_work
  end
end
