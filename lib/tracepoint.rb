require 'json'

Struct.new("RubyObject", :object_id, :inspect, :klass)
Struct.new("TracePoint", :event, :method_id, :defined_class, :self) do
  def self.from_tracepoint(tp)
    obj_name = lambda { |klass| Module.instance_method(:inspect).bind(klass).call }

    klass_name = tp.self.is_a?(Module) ? obj_name.call(tp.self) : obj_name.call(tp.self.class)

    tp_class = Struct::RubyObject.new(tp.self.class.object_id, klass_name, nil)
    tp_self = Struct::RubyObject.new(tp.self.object_id, tp.inspect, tp_class)

    puts "#{tp.defined_class}##{tp.method_id}-[:IS_A]->(#{klass_name})"

    Struct::TracePoint.new(tp.event, tp.method_id, tp.defined_class.to_s, tp_self)
  end

  def self.marshal(struct)
    struct.self.klass = struct.self.klass.to_h
    struct.self = struct.self.to_h
    struct.to_h.to_json
  end

  def self.unmarshal(json)
    parsed = JSON.parse(json)
    tp = Struct::TracePoint.new(*parsed.values)
    tp.self = Struct::RubyObject.new(*parsed['self'].values)
    tp.self.klass = Struct::RubyObject.new(*parsed['self']['klass'].values)
    tp
  end
end
