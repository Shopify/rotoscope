require "rake/extensiontask"

Rake::ExtensionTask.new "rotoscope" do |ext|
  ext.lib_dir = "lib/rotoscope"
end

task :install => [:compile] do |t|
  sh "gem build rotoscope.gemspec && gem install rotoscope-0.0.2.gem"
end
