Gem::Specification.new do |s|
  s.name        = 'rotoscope'
  s.required_ruby_version = ">= 2.2.2"
  s.version     = '0.0.2'
  s.date        = '2016-12-13'
  s.summary     = "Tracing Ruby"
  s.description = "Rotoscope performs introspection of method calls in Ruby programs."
  s.authors     = ["Jahfer Husain"]
  s.email       = 'jahfer.husain@shopify.com'
  s.files       = Dir['lib/**/*']
  s.homepage    = 'https://github.com/shopify/rotoscope'
  s.license     = 'MIT'
  s.add_runtime_dependency 'neo4apis', '~> 0.9', '>= 0.9.1'
end
