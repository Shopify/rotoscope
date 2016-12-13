Gem::Specification.new do |s|
  s.name        = 'rotoscope'
  s.required_ruby_version = ">= 2.2.2"
  s.version     = '0.0.1'
  s.date        = '2016-12-13'
  s.summary     = "Tracing Ruby into Neo4j"
  s.description = "Installs a tracepoint hook which funnels data about method and object relationships into a Neo4j graph database"
  s.authors     = ["Jahfer Husain"]
  s.email       = 'jahfer.husain@shopify.com'
  s.files       = Dir['lib/**/*.rb']
  s.homepage    = 'http://rubygems.org/gems/rotoscope'
  s.license     = 'MIT'
  s.add_runtime_dependency 'neo4apis', '~> 0.9', '>= 0.9.1'
end
