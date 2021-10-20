# frozen_string_literal: true

class FixtureInner
  def do_work
    raise unless sum == 2
  end

  def sum
    1 + 1
  end
end
