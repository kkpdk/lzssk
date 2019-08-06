# lzssk
An implementation of Lempel-Ziv-Storer-Szymanski with multithreaded compression,
mostly targeted at compressing firmware, data for firmware, and other embeddable bits,
though it also works for packet data.

It comes with various compressor implementations, all of which have configurable tradeoff
between window size and maximum substring length

- lzssk_encode_w is slow, but uses no memory during compression.
- lzssk_encode_wdm is typically 10x faster, but uses extra memory during compression (twice as much as the input)
- lzssk_threadpack uses the _wdm approach, using every available thread on the CPU.

There are two decompressors available, both of them are small

- lzssk_decode is a memory to memory single-call decompressor, very fast, no memory requirements.
- lzssk_readbyte is a bytewise decompressor, but requires as much memory as the windowsize.

For all operations, it is up to the user to specify window size. Window size can be varied
from as small as 256 bytes (useful in 8bit microcontrollers decompressing XSVF data, as an
example) up to 4096 bytes (more suitable for code), with no change in decompression performance.
Compression time grows linearly with window size.

What window size you want to use depends on the input data. I have found that for XSVF streams
for Coolrunner2 CPLD's, a 256 byte window size performs miracles as there are very long runs of
repeated XSVF instructions, and a 8 bit window leaves 8 bits for copy length, leading to
extremely good compression performance. On compiled code, the larger window sizes perform better,
as the runs are typically shorter, and more is gained by referencing strings further away in
memory.


###### How multithreaded compression is achieved

lzssk_threadpack queries the number of threads on the processors in the system, and splits
the compressed input into a number of chunks, one for each thread.
Each thread then builds a distance map for the chunk it was given, with threads being
allowed to look back into the previous chunk's input. This allows compression on a given
chunk to produce references into the previous chunk, thereby not incurring the compression
penalty that would occur if each chunk was compressed individually.

The resulting output streams are then decoded into tokens and re-encoded as a single stream.
This last step is single threaded, but is so fast that it is not significant.
The result is a valid compressed stream that is slightly larger (1 byte per thread worst case)
than what would have been produced by the single threaded compressor.
