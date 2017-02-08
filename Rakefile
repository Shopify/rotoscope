# ==========================================================
# Packaging
# ==========================================================
GEMSPEC = Gem::Specification::load('rotoscope.gemspec')

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

task :build => :compile

task :install => [:build] do |t|
  sh "gem build rotoscope.gemspec && gem install rotoscope-*.gem"
end

# ==========================================================
# Testing
# ==========================================================

require 'rake/testtask'
Rake::TestTask.new 'test' do |t|
  t.test_files = FileList['test/*_test.rb']
end
task :test => :build
