class LogFormatter < Logger::Formatter
  def new; end

  def call(_severity, _time, _progname, message)
    "#{message}\n"
  end
end
