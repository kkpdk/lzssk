/**                    *
 * file                lzssk.h
 * purpose             Lempel-Ziv-Storer-Szymanski derived compression routines
 *                     The code is extremely small, decompression is blazingly fast,
 *                     and it still provides good compression. Compression is a bit
 *                     slow at ~500k/sec on a PC, as the code runs with NO memory consumption.
 *                     Faster compression is possible at the expense of memory
 *
 * author              Kasper Pedersen <kkp2008@kasperkp.dk>
 *
 *
 */
/* Copyright (c) 2008 Kasper Kjeld Pedersen
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef LZSSK_H_
#define LZSSK_H_

#include <stdint.h>

//////////////////////// byte streaming decompressor ///////////////////////////
#define LZSSK_STREAMING_WINDOW 32768
typedef struct lzsskstruct {
        int optr;
        unsigned flag;
        int srcleft;
        uint16_t ringmask;
        uint8_t lenbit, lenmask;
        uint16_t ref_s, ref_len;
        
        const unsigned char *srcp;
        unsigned char (*getsrc)(struct lzsskstruct *s);
        unsigned char buf[LZSSK_STREAMING_WINDOW];
} lzssk_t;

int lzssk_init(lzssk_t *st, unsigned char *srcaddress, unsigned srclen, int winbit);
int lzssk_init2(lzssk_t *st, unsigned char *srcaddress, unsigned srclen, int winbit, unsigned char (*getsrcfn)(struct lzsskstruct *s));
int lzssk_readbyte(lzssk_t *st);
int lzssk_eof(lzssk_t *st);

/////////////////////// stream combination tools ///////////////////////////////
typedef struct {
  //unsigned char *flags;
  unsigned char *optr;
  unsigned char bitcount;
  signed char flagrel;
} lzsskcombine_t;

void lzssk_combine_init(lzsskcombine_t *st, unsigned char *dst);
unsigned char *lzssk_combine_add(lzsskcombine_t *st, const unsigned char *src, int srclen);

////////////// single threaded compression/block decompression //////////////////

//compression with deltamap (uses 262144+2*input heap memory)
uint16_t *lzssk_build_deltamap(unsigned char *src, int srclen);
unsigned char *lzssk_encode_wdm(unsigned char *dst, const unsigned char *src, unsigned srclen, int winbit, int preview/*0*/, lzsskcombine_t *endstate, uint16_t *map );

//compression without deltamap (uses no memory, 10x slower)
unsigned char *lzssk_encode_w(unsigned char *dst, const unsigned char *src, unsigned srclen, int winbit, int preview, lzsskcombine_t *endstate);
//uncompressed stream generation
char *lzssk_encodecopy(unsigned char *dst, const unsigned char *src, unsigned srclen);
//decompression for all compressed and uncompressed streams. Fast.
unsigned lzssk_decode(unsigned char *dst, unsigned dstlen, const unsigned char *src, int srclen, int win);

//////////////// multithreaded compression /////////////////////////////////////
#define LZSSK_MAX_USED_CPUS 32 /*read below before changing*/
int lzssk_cpus(void); //purely for information
//call threadpack_dstsize to get the required size of the compression buffer. It it a
//little larger than for unthreaded compression, and about an additional 2k larger
//if cachespacing is set to 1, depending on the number of processors in the
//system
int lzssk_threadpack_dstsize(int srclen, int wbit, int cachespacing);
//and to perform the compression use threadpack. cachespacing MUST be the same value used above,
//and if deltamap is set to 1, it will allocate heap to build maps (peak allocation is CPUS*(256k+64)+2*srclen
//if heap allocation fails, threadpack falls back to no-deltamap mode.
//Maximum CPUS is currently set to 32 above. This means a peak allocation of 8MB for the per-thread
//tables on a 32-thread processor (such as R9-3950X).
int lzssk_threadpack(unsigned char *dst, unsigned char *src, int srclen, int wbit, int cachespacing, int deltamap);

#endif /*LZSSK_H_*/
