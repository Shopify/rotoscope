# Rotoscope

## Usage
```
$ rake install
```

```ruby
require 'rotoscope'

Rotoscope.trace do
  Order.find(1).refundable?
end
```
