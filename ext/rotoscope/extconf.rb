# frozen_string_literal: true

require "mkmf"

$CFLAGS << " -std=c99"
$CFLAGS << " -Wall"
$CFLAGS << " -Wno-declaration-after-statement"

unless ["0", "", nil].include?(ENV["ROTOSCOPE_COMPILE_ERROR"])
  $CFLAGS << " -Werror"
end

$defs << "-D_POSIX_SOURCE"

create_makefile("rotoscope/rotoscope")
