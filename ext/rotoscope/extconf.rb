# frozen_string_literal: true
require "mkmf"

$CFLAGS << ' -std=c99 -Wall -Werror -Wno-declaration-after-statement'
$defs << "-D_POSIX_SOURCE"

uthash_path = File.expand_path('../../../lib/uthash', __FILE__)
find_header('uthash.h', uthash_path) || raise

create_makefile('rotoscope/rotoscope')
