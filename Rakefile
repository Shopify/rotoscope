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

task :install => [:compile] do |t|
  sh "gem build rotoscope.gemspec && gem install rotoscope-0.0.2.gem"
end
