module Neo4Apis
  class Spiderman < Base
    # Adds a prefix to labels so that they become AwesomeSiteUser and AwesomeSiteWidget (optional)
    common_label :Spiderman

    uuid :Object, :ruby_object_id
    uuid :TracePoint, :uuid

    IMPORTED_OBJECT_NODES = {}

    importer :Object do |object|
      next IMPORTED_OBJECT_NODES[object.object_id] if IMPORTED_OBJECT_NODES.key?(object.object_id)

      object_node = add_node(:Object) do |node|
        node.ruby_object_id = object.object_id
        node.ruby_inspect = object.inspect[0,500]
        node._extra_labels = []
        node._extra_labels << 'Class' if object.class == Class
        node._extra_labels << 'Module' if object.class == Module
      end

      IMPORTED_OBJECT_NODES[object.object_id] = object_node

      class_node = import :Object, object.class
      add_relationship :IS_A, object_node, class_node if class_node

      object_node
    end

    importer :TracePoint do |tp, parent|
      next nil if tp.method_id.to_s.strip.empty? && tp.defined_class.to_s.strip.empty?

      trace_point_node = add_node :TracePoint, tp, %i(event lineno method_id) do |node|
        node.uuid = SecureRandom.uuid
        node.defined_class = tp.defined_class.to_s
      end

      unless tp == tp.self
        begin
          ruby_object_node = import :Object, tp.self
          add_relationship :FROM_OBJECT, trace_point_node, ruby_object_node
        rescue Exception
          nil
        end
      end

      add_relationship :HAS_PARENT, trace_point_node, parent if parent

      trace_point_node
    end
  end
end
