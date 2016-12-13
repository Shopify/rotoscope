require 'neo4apis'
require 'logger'
require_relative 'neo4apis/rotoscope'
require_relative 'log_formatter'
require_relative 'tracepoint'

class Rotoscope
  TP_EVENTS = [:call, :c_call, :return, :c_return].freeze
  OUTPUT_FILE = 'tmp/trace/trace.log'

  def self.trace(path: OUTPUT_FILE)
    rotoscope = new
    rotoscope.trace(path) { yield }
    rotoscope.import(path)
  end

  def trace(path)
    create_log_file
    record_trace { yield }
    true
  end

  def import(path, session: neo4j_session)
    import_to_neo4j(path, Neo4Apis::Rotoscope.new(session))
  end

  private

  def import_to_neo4j(path, session)
    session.batch do
      last_tracepoint_node = nil
      ancestor_stack = []

      IO.foreach(path) do |json|
        tp = Struct::TracePoint.unmarshal(json)
        ancestor_stack.pop if ['return', 'c_return'].include?(tp.event)
        last_tracepoint_node = session.import(:TracePoint, tp, ancestor_stack.last)
        ancestor_stack.push(last_tracepoint_node) if ['call', 'c_call'].include?(tp.event)
      end
    end
  end

  private

  def record_trace
    ruby_tp = TracePoint.new(*TP_EVENTS) do |tp|
      next if tp.path.match(%r{/rotoscope/})
      tracepoint = Struct::TracePoint.from_tracepoint(tp)
      neo4j_writer_log.info Struct::TracePoint.marshal(tracepoint)
    end

    ruby_tp.enable
    yield
  ensure
    ruby_tp.disable
  end

  def create_log_file
    FileUtils.mkpath('tmp/trace')
    FileUtils.rm(OUTPUT_FILE, force: true)
    FileUtils.touch(OUTPUT_FILE)
  end

  def neo4j_writer_log
    @logger ||= begin
      logger = Logger.new(OUTPUT_FILE)
      logger.formatter = LogFormatter.new
      logger
    end
  end

  def neo4j_session
    @session ||= Neo4j::Session.open(:server_db, 'http://neo4j:pass@localhost:7474')
  end
end
