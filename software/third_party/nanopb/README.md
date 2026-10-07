# nanopb runtime

This directory contains the unmodified minimal C runtime from nanopb
`nanopb-0.4.9.1`, upstream commit
`cad3c18ef15a663e30e3e43e3a752b66378adec1`.

Upstream: <https://github.com/nanopb/nanopb>

Included files are `pb.h`, `pb_common.[ch]`, `pb_encode.[ch]`,
`pb_decode.[ch]`, and the upstream `LICENSE.txt`. The generator is installed
as a pinned Python build dependency rather than copied into this directory.
