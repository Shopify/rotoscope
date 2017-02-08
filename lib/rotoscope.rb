class Rotoscope
  def self.inspect(obj)
    @mod_inspect ||= Module.instance_method(:inspect)
    @mod_inspect.bind(obj).call
  end

  def self.trace(output_path, blacklist=[])
    rs = new(output_path, blacklist)
    rs.trace { yield rs }
    rs.close
    rs
  end
end

require 'rotoscope/rotoscope'
