require 'msgpack'

Struct.new("RubyObject", :root, :object_id, :inspect, :klass) do |variable|
  def to_msgpack_ext
    root = klass.nil? ? 1 : 0
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
  def self.mod_inspect
    @inspect ||= Module.instance_method(:inspect)
  end

  def self.class_name(struct)
    @class_name ||= Hash.new do |h, key|
      h[key] = (struct.is_a?(Module) ? mod_inspect.bind(struct).call : mod_inspect.bind(struct.class).call)[0,200]
    end
    @class_name[struct.object_id]

  end

  def self.from_tracepoint(tp)
    tp_class = [1, tp.self.class.object_id, class_name(tp.self), nil]
    tp_self = [0, tp.self.object_id, tp.inspect[0,200], tp_class]
    [tp.event, tp.method_id, tp.defined_class.to_s, tp_self]
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
end

MessagePack::DefaultFactory.register_type(0x00, Struct::RubyObject)
MessagePack::DefaultFactory.register_type(0x01, Struct::TracePoint)
