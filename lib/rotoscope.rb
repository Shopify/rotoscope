require 'neo4apis'
require 'logger'
require 'msgpack'
require_relative 'neo4apis/rotoscope'
require_relative 'log_formatter'
require_relative 'tracepoint'

class Rotoscope
  TP_EVENTS = [:call, :c_call, :return, :c_return].freeze
  OUTPUT_FILE = 'tmp/trace/trace.log'

  def self.trace
    rotoscope = new
    rotoscope.trace { yield }
    rotoscope.import
  end

  def trace(path: OUTPUT_FILE)
    create_log_file
    record_trace { yield }
    true
  end

  def import(path: OUTPUT_FILE, session: neo4j_session)
    import_to_neo4j(path, Neo4Apis::Rotoscope.new(session))
  end

  private

  def serializer
    @serializer ||= begin
      factory = MessagePack::Factory.new
      factory.register_type(0x00, Struct::RubyObject)
      factory.register_type(0x01, Struct::TracePoint)
      factory
    end
  end

  def import_to_neo4j(path, session)
    session.batch do
      last_tracepoint_node = nil
      ancestor_stack = []

      IO.foreach(path) do |data|
        serializer.unpacker.feed_each(data) do |tp|
          next unless tp.is_a?(Struct::TracePoint)
          # puts "#{tp.defined_class}##{tp.method_id}-[:IS_A]->(#{tp.self.klass.inspect})"
          ancestor_stack.pop if ['return', 'c_return'].include?(tp.event)
          last_tracepoint_node = session.import(:TracePoint, tp, ancestor_stack.last)
          ancestor_stack.push(last_tracepoint_node) if ['call', 'c_call'].include?(tp.event)
        end
      end
    end
  end

  private

  def record_trace
    fh = File.open(OUTPUT_FILE, 'a', encoding: Encoding::ASCII_8BIT)
    ruby_tp = TracePoint.new(*TP_EVENTS) do |tp|
      next if tp.path.match(%r{/rotoscope/})
      tracepoint = Struct::TracePoint.from_tracepoint(tp)
      fh.puts serializer.packer.write(tracepoint).to_s
    end
    ruby_tp.enable
    yield
  ensure
    ruby_tp.disable
    fh.close
  end

  def create_log_file
    FileUtils.mkpath('tmp/trace')
    FileUtils.rm(OUTPUT_FILE, force: true)
  end

  def neo4j_session
    @session ||= Neo4j::Session.open(:server_db, 'http://neo4j:pass@localhost:7474')
  end
end
