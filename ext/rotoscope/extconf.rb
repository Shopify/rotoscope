# frozen_string_literal: true
require "mkmf"

$CFLAGS << ' -std=c99 -Wall -Werror -Wno-declaration-after-statement'
create_makefile('rotoscope/rotoscope')
