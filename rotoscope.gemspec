Gem::Specification.new do |s|
  s.name        = 'rotoscope'
  s.version     = '0.0.2'
  s.date        = '2016-12-13'
  s.summary     = "Tracing Ruby"
  s.description = "Rotoscope performs introspection of method calls in Ruby programs."
  s.authors     = ["Jahfer Husain"]
  s.email       = 'jahfer.husain@shopify.com'
  s.files       = `git ls-files`.split("\n")
  s.homepage    = 'https://github.com/shopify/rotoscope'
  s.license     = 'MIT'
  s.required_ruby_version = ">= 2.2.0"
  s.extensions = %w[ext/rotoscope/extconf.rb]
end
