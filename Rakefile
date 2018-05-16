# frozen_string_literal: true
# ==========================================================
# Packaging
# ==========================================================
GEMSPEC = Gem::Specification.load('rotoscope.gemspec')

require 'bundler/gem_tasks'
require 'rubygems/package_task'

Gem::PackageTask.new(GEMSPEC) do |pkg|
end

# ==========================================================
# Ruby Extension
# ==========================================================

require 'rake/extensiontask'
Rake::ExtensionTask.new('rotoscope', GEMSPEC) do |ext|
  ext.lib_dir = 'lib/rotoscope'
end

task build: :compile

task install: [:build] do |_t|
  sh "gem build rotoscope.gemspec && gem install rotoscope-*.gem"
end

# ==========================================================
# Testing
# ==========================================================

require 'rake/testtask'
Rake::TestTask.new 'test' do |t|
  t.test_files = FileList['test/*_test.rb']
end
task test: :build

task :rubocop do
  require 'rubocop/rake_task'
  RuboCop::RakeTask.new
end

namespace :lint do
  task ruby: :rubocop

  task :c do
    grep_matches = system("find '#{__dir__}/ext' -iname '*.c' -o -iname '*.h' " \
      "| xargs clang-format -style=file -output-replacements-xml | grep -q '<replacement '")
    if grep_matches
      abort "C format changes are needed. Please run bin/fmt"
    end
  end
end
task lint: ['lint:ruby', 'lint:c']

task default: [:test, :lint]
