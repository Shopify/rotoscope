require 'msgpack'

Struct.new("RubyObject", :root, :object_id, :inspect, :klass) do |variable|
  def to_msgpack_ext
    root = klass.is_a?(Struct::RubyObject) ? 0 : 1
    [root, object_id.to_s, self[:inspect], MessagePack.pack(klass)].pack('CA24A200A*')
  end

  def self.from_msgpack_ext(data)
    unpacked = data.unpack('CA24A200A*')
    foo = new(*unpacked[0..2])
    foo.klass = MessagePack.unpack(unpacked[3]) if foo.root == 0
    foo
  end
end

Struct.new("TracePoint", :event, :method_id, :defined_class, :self) do
  def self.ruby_objects
    @ruby_objects ||= [Struct::RubyObject.new, Struct::RubyObject.new]
  end

  def self.from_tracepoint(tp)
    obj_name = lambda { |klass| Module.instance_method(:inspect).bind(klass).call }

    klass_name = tp.self.is_a?(Module) ? obj_name.call(tp.self) : obj_name.call(tp.self.class)

    tp_class = ruby_objects[0]
    tp_class.root = 1
    tp_class.object_id = tp.self.class.object_id
    tp_class.inspect = klass_name[0,200]
    tp_class.klass = nil

    tp_self = ruby_objects[1]
    tp_class.root = 0
    tp_self.object_id = tp.self.object_id
    tp_self.inspect = tp.inspect[0,200]
    tp_self.klass = tp_class

    Struct::TracePoint.new(tp.event, tp.method_id, tp.defined_class.to_s, tp_self)
  end

  def to_msgpack_ext
    [event.to_s, method_id.to_s[0,50], defined_class.to_s[0,100], MessagePack.pack(self.self)].pack('A8A50A100A*')
  end

  def self.from_msgpack_ext(data)
    data = data.unpack('A8A50A100A*')
    foo = new(*data[0..2])
    foo.self = MessagePack.unpack(data[3])
    foo
  end

  def self.marshal(struct)
    MessagePack.pack(struct)
  end

  def self.unmarshal(binary)
    MessagePack.unpack(binary)
  end
end

MessagePack::DefaultFactory.register_type(0x00, Struct::RubyObject)
MessagePack::DefaultFactory.register_type(0x01, Struct::TracePoint)
