require 'neo4apis'
require 'logger'
require_relative 'neo4apis/rotoscope'
require_relative 'log_formatter'
require_relative 'tracepoint'

class Recorder
  TP_EVENTS = [:call, :c_call, :return, :c_return].freeze
  OUTPUT_FILE = 'tmp/log/trace.log'

  attr_accessor :session, :events

  def initialize(session: neo4j_session, events: TP_EVENTS)
    self.session = session
    self.events = events
  end

  def record(&block)
    record_trace do
      begin
        block.call
      rescue => e
        nil
      end
    end

    session.batch do
      last_tracepoint_node = nil
      ancestor_stack = []

      IO.foreach(OUTPUT_FILE) do |json|
        tp = Struct::TracePoint.unmarshal(json)
        ancestor_stack.pop if ['return', 'c_return'].include?(tp.event)
        last_tracepoint_node = session.import(:TracePoint, tp, ancestor_stack.last)
        ancestor_stack.push(last_tracepoint_node) if ['call', 'c_call'].include?(tp.event)
      end
    end
  end

  def record_trace
    trace = TracePoint.new(*events) do |tp|
      tracepoint = Struct::TracePoint.from_tracepoint(tp)
      neo4j_writer_log.info Struct::TracePoint.marshal(tracepoint)
    end

    trace.enable
    yield
  ensure
    trace.disable
  end

  def neo4j_writer_log
    @logger ||= begin
      logger = Logger.new(OUTPUT_FILE)
      logger.formatter = LogFormatter.new
      logger
    end
  end

  def neo4j_session(options: {})
    @neo4j_session ||= begin
      sess = Neo4j::Session.open(:server_db, 'http://neo4j:pass@localhost:7474')
      Neo4Apis::Rotoscope.new(sess, options)
    end
  end
end
