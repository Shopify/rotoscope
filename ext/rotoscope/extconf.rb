require "mkmf"

create_makefile "rotoscope/rotoscope"
$CFLAGS << " -Wall -O2"

Gem::Specification.new "rotoscope", "1.0" do |s|
  s.extensions = %w[ext/rotoscope/extconf.rb]
end
