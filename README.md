rle-util: Simple RLE (Run-Lenght Encoding) encoding/decoding utility
====================================================================

About
-----

rle is a simple utility for data encoding/decoding using the RLE
algorithm with command line syntax similar to gzip(1) and bzip(1). The
file format is an raw RLE-encoded stream, as described below.

RLE algorithm: https://en.wikipedia.org/wiki/Run-length_encoding

Compressed format: each run of bytes is preceded by an unsigned octet
LEN, if LEN < 128, then the next byte will be repeated LEN times in the
output stream, if LEN >= 128 then the subsequent (256 - LEN) bytes will
be copied to the output stream "as is".

Actually, RLE stream can be represented in many other ways, so you
should not expect the utility to support yours. But you can always
modify this utility for your needs, it is a really easy task.

Tested only with GNU/Linux OS. The build process is as simple as running
make(1).

License
-------

This project is licensed under the terms of the ISC license. See
the LICENSE file for license rights and limitations.
