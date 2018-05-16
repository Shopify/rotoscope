# frozen_string_literal: true
lib = File.expand_path("../lib", __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require "rotoscope/version"

Gem::Specification.new do |s|
  s.name        = 'rotoscope'
  s.version     = Rotoscope::VERSION
  s.date        = '2017-09-20'

  s.authors     = ["Jahfer Husain", "Dylan Thacker-Smith"]
  s.email       = 'jahfer.husain@shopify.com'
  s.homepage    = 'https://github.com/shopify/rotoscope'
  s.license     = 'MIT'

  s.summary     = "Tracing Ruby"
  s.description = "High-performance logger of Ruby method invocations"

  s.files       = `git ls-files -z`.split("\x0").reject do |f|
    f.match(%r{^(test)/})
  end
  s.required_ruby_version = ">= 2.2.0"
  s.extensions = %w(ext/rotoscope/extconf.rb)

  s.add_development_dependency 'rake-compiler', '~> 0.9'
  s.add_development_dependency 'mocha', '~> 0.14'
  s.add_development_dependency 'minitest', '~> 5.0'
  s.add_development_dependency 'rubocop', '~> 0.56'
end
