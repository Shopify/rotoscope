Struct.new("RubyObject", :object_id, :inspect, :class)
Struct.new("TracePoint", :event, :method_id, :defined_class, :self) do
  def self.from_tracepoint(tp)
    tp_self = Struct::RubyObject.new(tp.self.object_id, tp.self.inspect[0,500], tp.self.class)
    Struct::TracePoint.new(tp.event, tp.method_id, tp.defined_class, tp_self)
  end

  def self.marshal(struct)
    struct.self = struct.self.to_h
    struct.to_h.to_json
  end

  def self.unmarshal(json)
    parsed = JSON.parse(json)
    tp = Struct::TracePoint.new(*parsed.values)
    tp.defined_class = eval(tp.defined_class)
    tp.self = Struct::RubyObject.new(*parsed['self'].values)
    tp.self.class = eval(tp.self.class)
    tp
  end
end
