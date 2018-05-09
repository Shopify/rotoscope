# frozen_string_literal: true
require "mkmf"

$CFLAGS << ' -std=c99 -Wall -Werror -Wno-declaration-after-statement'
$defs << "-D_POSIX_SOURCE"

create_makefile('rotoscope/rotoscope')
