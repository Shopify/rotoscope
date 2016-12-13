module Neo4Apis
  class Rotoscope < Base
    common_label :Rotoscope

    uuid :Object, :ruby_object_id
    uuid :TracePoint, :uuid

    batch_size 6000

    IMPORTED_OBJECT_NODES = {}

    importer :Object do |object|
      next IMPORTED_OBJECT_NODES[object.object_id] if IMPORTED_OBJECT_NODES.key?(object.object_id)

      object_node = add_node(:Object) do |node|
        node.ruby_object_id = object.object_id
        node.ruby_inspect = object.inspect[0,500]
      end

      IMPORTED_OBJECT_NODES[object.object_id] = object_node

      class_node = import(:Object, object.klass) if object.klass
      add_relationship(:IS_A, object_node, class_node) if class_node

      object_node
    end

    importer :TracePoint do |tp, parent|
      next nil if tp.method_id.strip.empty? && tp.defined_class.strip.empty?

      trace_point_node = add_node :TracePoint, tp, [:event, :method_id, :defined_class] do |node|
        node.uuid = SecureRandom.uuid
      end

      unless tp.defined_class == tp.self.klass
        ruby_object_node = import(:Object, tp.self)
        add_relationship(:FROM_OBJECT, trace_point_node, ruby_object_node)
      end

      add_relationship(:HAS_PARENT, trace_point_node, parent) if parent

      trace_point_node
    end
  end
end
