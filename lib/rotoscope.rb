# require 'msgpack'
# require 'json'
require 'rotoscope/rotoscope'

require 'neo4apis'
require_relative 'neo4apis/rotoscope'
require_relative 'tracepoint'

# class Rotoscope
#   TP_EVENTS = [:call, :c_call, :return, :c_return].freeze
#   OUTPUT_FILE = 'tmp/trace/trace.log'

#   attr_accessor :serialization_format

#   def self.trace
#     rotoscope = new
#     rotoscope.trace { yield }
#     rotoscope.import
#   end

#   def initialize(serialize: :manual)
#     self.serialization_format = serialize
#   end

#   def trace(path: OUTPUT_FILE)
#     create_log_file
#     record_trace { yield }
#     true
#   end

#   def import(path: OUTPUT_FILE, session: neo4j_session)
#     import_to_neo4j(path, Neo4Apis::Rotoscope.new(session))
#   end

#   private

#   def import_to_neo4j(path, session)
#     session.batch do
#       last_tracepoint_node = nil
#       ancestor_stack = []

#       IO.foreach(path) do |data|
#         tp = deserialize(data)
#         # puts "#{tp.defined_class}##{tp.method_id}-[:IS_A]->(#{tp.self.klass.inspect})"
#         ancestor_stack.pop if ['return', 'c_return'].include?(tp.event)
#         last_tracepoint_node = session.import(:TracePoint, tp, ancestor_stack.last)
#         ancestor_stack.push(last_tracepoint_node) if ['call', 'c_call'].include?(tp.event)
#       end
#     end
#   end

#   private

#   def record_trace
#     fh = File.open(OUTPUT_FILE, 'a', encoding: Encoding::ASCII_8BIT) unless serialization_format == :c
#     ruby_tp = TracePoint.new(*TP_EVENTS) do |tp|
#       begin
#         next if tp.path.match(%r{/rotoscope/})
#         if serialization_format == :c
#           log_tracepoint(tp)
#         else
#           tracepoint = Struct::TracePoint.from_tracepoint(tp)
#           fh.puts(serialize(tracepoint))
#         end
#       rescue => e
#         puts e.inspect
#         puts e.backtrace
#       end
#     end
#     ruby_tp.enable
#     yield
#   ensure
#     ruby_tp.disable
#     fh.close unless serialization_format == :c
#   end

#   def serialize(tracepoint)
#     case serialization_format
#     when :msgpack
#       MessagePack.pack(tracepoint)
#     when :json
#       tracepoint.self.klass = tracepoint.self.klass.to_h
#       tracepoint.self = tracepoint.self.to_h
#       tracepoint.to_h.to_json
#     when :manual
#       <<-JSON
# {"event":"#{tracepoint.event}","method_id":"#{tracepoint.method_id}","defined_class":"#{tracepoint.defined_class.to_s}","self":{"root":0,"object_id":#{tracepoint.self.object_id},"inspect":"#{tracepoint.inspect[0,200].gsub('"', '\"')}","klass":{"root":0,"object_id":#{tracepoint.self.klass.object_id},"inspect":"#{tracepoint.self.klass.inspect[0,200]}","klass":null}}}
#       JSON
#     else
#       raise 'Serializer format #{serialization_format} not supported'
#     end
#   end

#   def deserialize(line)
#     case serialization_format
#     when :msgpack
#      msgpack_serializer.unpacker.feed_each(line) do |tp_arr|
#       tp = Struct::TracePoint.new(*tp_arr[0..2])
#       tp.self = Struct::RubyObject.new(*tp_arr[3])
#       tp.self.klass = Struct::RubyObject.new(*tp.self.klass)
#       return tp
#      end
#     when :json, :manual
#       parsed = JSON.parse(line)
#       tp = Struct::TracePoint.new(*parsed.values)
#       tp.self = Struct::RubyObject.new(*parsed['self'].values)
#       tp.self.klass = Struct::RubyObject.new(*parsed['self']['klass'].values)
#       tp
#     else
#       raise 'Serializer format #{serialization_format} not supported'
#     end
#   end

#   def msgpack_serializer
#     @serializer ||= begin
#       factory = MessagePack::Factory.new
#       factory.register_type(0x00, Struct::RubyObject)
#       factory.register_type(0x01, Struct::TracePoint)
#       factory
#     end
#   end

#   def create_log_file
#     FileUtils.mkpath('tmp/trace')
#     FileUtils.rm(OUTPUT_FILE, force: true)
#   end

#   def neo4j_session
#     @session ||= Neo4j::Session.open(:server_db, 'http://neo4j:pass@localhost:7474')
#   end
# end
