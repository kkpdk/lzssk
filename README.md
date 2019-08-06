# lzssk
An implementation of Lempel-Ziv-Storer-Szymanski with multithreaded compression,
mostly targeted at compressing firmware, data for firmware, and other embeddable bits,
though it also works for packet data.

It comes with various compressor implementations, all of which have configurable window sizes.

- lzssk_encode_w is slow, but uses no memory during compression.
- lzssk_encode_wdm is typicalle 10x faster, but uses extra memory during compression (twice as much as the input)
- lzssk_threadpack uses the _wdm approach, using every available thread on the CPU.

There are two decompressors available, both of them are small

- lzssk_decode is a memory to memory single-call decompressor, very fast, no memory requirements.
- lzssk_readbyte is a bytewise decompressor, but requires as much memory as the windowsize.

For all operations, it is up to the user to specify window size. Window size can be varied
from as small as 256 bytes (useful in 8bit microcontrollers decompressing XSVF data, as an
example) up to 4096 bytes (more suitable for code), with no change in decompression performance.
Compression time grows linearly with window size.

