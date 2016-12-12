require_relative 'lib/recorder'

module SchmactiveRecord
  class Base
    def find
    end
  end
end

class Customer < SchmactiveRecord::Base
  def name
    "Tobi Lutke"
  end
end

class Order < SchmactiveRecord::Base
  def name
    "Order #1"
  end

  def customer
    @customer ||= Customer.new
  end

  def process
    customer.find
    "#{name} from #{customer.to_s} #{customer.name}"
  end
end

Recorder.new.record do
  o = Order.new
  o.process
end
