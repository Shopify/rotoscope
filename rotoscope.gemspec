# frozen_string_literal: true

lib = File.expand_path("../lib", __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require "rotoscope/version"

Gem::Specification.new do |s|
  s.name        = "rotoscope"
  s.version     = Rotoscope::VERSION

  s.authors     = ["Jahfer Husain", "Dylan Thacker-Smith"]
  s.email       = "jahfer.husain@shopify.com"
  s.homepage    = "https://github.com/shopify/rotoscope"
  s.license     = "MIT"

  s.summary     = "Tracing Ruby"
  s.description = "High-performance logger of Ruby method invocations"

  s.files       = %x(git ls-files -z).split("\x0").reject do |f|
    f.match(%r{^(test)/})
  end
  s.metadata["allowed_push_host"] = "https://rubygems.org/"

  s.required_ruby_version = ">= 2.7.0"
  s.extensions = ["ext/rotoscope/extconf.rb"]

  s.add_development_dependency("minitest", "~> 5.0")
  s.add_development_dependency("mocha", "~> 2.4")
  s.add_development_dependency("rake-compiler", "~> 1.2")
end
