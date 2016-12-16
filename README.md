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

```
                           user     system      total        real
no trace               0.240000   0.020000   0.260000 (  0.379642)
trace w/o logging      0.440000   0.020000   0.460000 (  0.604192)
manual                33.440000   1.170000  34.610000 ( 35.602534)
json                 162.400000   2.450000 164.850000 (167.229680)
msgpack               59.650000   4.480000  64.130000 ( 65.702134)
```
