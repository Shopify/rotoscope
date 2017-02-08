Gem::Specification.new do |s|
  s.name        = 'rotoscope'
  s.version     = '0.1.0'
  s.date        = '2016-12-13'

  s.authors     = ["Jahfer Husain"]
  s.email       = 'jahfer.husain@shopify.com'
  s.homepage    = 'https://github.com/shopify/rotoscope'
  s.license     = 'MIT'

  s.summary     = "Tracing Ruby"
  s.description = "Rotoscope performs introspection of method calls in Ruby programs."

  s.files       = `git ls-files`.split("\n")
  s.required_ruby_version = ">= 2.2.0"
  s.extensions = %w[ext/rotoscope/extconf.rb]

  s.add_development_dependency 'rake-compiler', '~> 0.9'
  s.add_development_dependency 'mocha', '~> 0.14'
  s.add_development_dependency 'minitest', '~> 5.0'
end
