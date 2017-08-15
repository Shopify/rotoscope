# frozen_string_literal: true
Gem::Specification.new do |s|
  s.name        = 'rotoscope'
  s.version     = '0.2.1'
  s.date        = '2017-06-19'

  s.authors     = ["Jahfer Husain"]
  s.email       = 'jahfer.husain@shopify.com'
  s.homepage    = 'https://github.com/shopify/rotoscope'
  s.license     = 'MIT'

  s.summary     = "Tracing Ruby"
  s.description = "High-performance logger of Ruby method invocations"

  s.files       = `git ls-files`.split("\n")
  s.required_ruby_version = ">= 2.2.0"
  s.extensions = %w(ext/rotoscope/extconf.rb)

  s.add_development_dependency 'rake-compiler', '~> 0.9'
  s.add_development_dependency 'mocha', '~> 0.14'
  s.add_development_dependency 'minitest', '~> 5.0'
  s.add_development_dependency 'rubocop', '~> 0.49'
end
