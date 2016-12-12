require 'neo4apis'
require 'logger'
require_relative 'neo4apis/spiderman'

class Recorder

  attr_accessor :session, :events
  TP_EVENTS = [:call, :c_call, :return, :c_return].freeze

  def neo4j_session(options: {})
    @neo4j_session ||= begin
      sess = Neo4j::Session.open(:server_db, 'http://neo4j:pass@localhost:7474')
      Neo4Apis::Spiderman.new(sess, options)
    end
  end

  def initialize(session: neo4j_session, events: TP_EVENTS)
    self.session = session
    self.events = events
  end

  def record(&block)
    session.batch do
      record_trace do
        begin
          block.call
        rescue => e
          nil
        end
      end
    end
  end

  def record_trace
    last_tracepoint_node = nil
    ancestor_stack = []

    trace = TracePoint.new(*events) do |tp|
      begin
        if [:return, :c_return].include?(tp.event)
          ancestor_stack.pop
        end

        last_tracepoint_node = session.import(:TracePoint, tp, ancestor_stack.last)

        if [:call, :c_call].include?(tp.event)
          ancestor_stack.push(last_tracepoint_node)
        end
      rescue => e
        puts 'Exception! 😱'
        puts e.message
        puts e.backtrace
      end
    end

    trace.enable
    yield
  ensure
    trace.disable
  end

  def logger
    @logger ||= begin
      logger = Logger.new($STDOUT)
      logger
    end
  end
end
