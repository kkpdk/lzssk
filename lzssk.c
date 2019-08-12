/* Copyright (c) 2008 Kasper Kjeld Pedersen
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
  
  
  This is an implementation, or variant of, Lempel Ziv Storer Szymanski
  compression. It comes with three compressors, one of which is quite slow
  but uses no memory, one which is reasonably fast but uses twice as much
  extra memory as the input, and one which compresses a few bytes worse,
  but runs multithreaded/multicore.
  Decompression is fast, basically memcpy speed. It comes in a single-call
  form, as well as a streaming decompressor.
  
*/


#include "lzssk.h"
/*

  A compressed stream consists of tags and objects:

  tag obj0 obj1 obj2 obj3 obj4 obj5 obj6 obj7 tag obj0 obj1 ...

  A tag is followed by 1..8 objects. All chunks but the last always
  have 8 objecs, the last one has 1..8.

  A tag is an 8 bit byte, with the corresponding bit indicating
  whether a following object is a literal (0) or a reference (1).

  An object that is a literal is a byte. It is copied to the output
  stream.
  An object that is a reference is two bytes long. The first byte contains
  the lower 8 bits of offset, the second byte has the upper bits of offset
  in the upper bits, with length code in the lower bits.
  The length code in the second byte is coded as length-3, so that the
  shortest possible copy is 3 bytes, coding as 0.
  offset is relative backwards in teh stream. An offset of 0 is reserved.
  
  Example compression:

  a b c d a b c d:
  10 'a' 'b' 'c' 'd' 04 01     offset -4, 3+1 byte copied

  a b c a b c:
  08 'a' 'b' 'c' 03 00     offset -3, 3+0 byte copied

  a b c d e f g h i a b c d e f g h i j k:
  00 a b c d e f g h 02 i 09 06 j k

  If the first byte of a stream has bit 0 of the tag byte set,
  it is not a compressed stream. Instead this indicates not compressed.

*/

//////////////////////////////////////////
//
//  Compress a buffer into another buffer.
//  dst buffer must be able to hold 1.125*input+2 byte, as
//  that is the maximum expansion for uncompressable input.
//  returns 'stream pointer'
//

#include <malloc.h>
uint16_t *lzssk_build_deltamap(unsigned char *src, int srclen)
{
	int i,last,d,h;
	uint16_t *dm;
	int32_t  *hm;

	dm=malloc(sizeof(uint16_t)*srclen);
	if (!dm) return 0;
	hm=malloc(sizeof(int32_t)*65536);
	if (!hm) {
		free(dm);
		return 0;
	}
	for (i=0; i<65536; ++i) hm[i]=-65536;
	for (i=0; i<srclen-2; ++i) {
		h=(src[0]+(src[1]<<8)+(src[2]*197))&65535;
		++src;
		last=hm[h];
		d=i-last;
		if (d>65535) d=65535;
		dm[i]=d;
		hm[h]=i;
	}
	for (; i<srclen; ++i) dm[i]=65535;
	free(hm);
	return dm;
}

unsigned char *lzssk_encode_wdm(unsigned char *dst, const unsigned char *src, unsigned srclen, int winbit, int preview/*0*/, lzsskcombine_t *endstate, uint16_t *map )
{
	unsigned bestmatch,i,imax;
	unsigned remain;
	unsigned char *flagptr;
	unsigned flagbit,betpos,bestlen;
	const unsigned char *test;
	const unsigned char *srclim=src-preview; //used for multithreaded compression. Can also be used to force an uncompressed lead-in
	int rel;

	int lengthbits=16-winbit;
	unsigned copymax=(1<<lengthbits)+2; // 18 for 12 bit window, 66 for 10 bit window
	int scanlength=(1<<winbit)-1;  //max backreference all-ones in length

	if (!dst) {
		flagbit=endstate->bitcount;
		dst=endstate->optr;
		flagptr=dst+endstate->flagrel;
	} else {
		//printf("predst srclen=%i winbit=%i pre=%i\n",srclen,winbit,preview);
		flagbit=8;
	}
	remain=srclen;
	do {
		if (flagbit==8) { //allocate room for tag byte
			flagptr=dst;
			*dst++=0;
			flagbit=0; //we have room for 8 flag bits now.
		}
		if ((src-srclim)>scanlength) srclim=src-scanlength; //is srclim is more than 4k behind, move srclim ahead
		bestlen=0;
		test=src-1; //search backwards, starting with last output byte
		imax=copymax-1;
		if (remain<copymax) imax=remain-1; //searchlength should not exceed input
		if (remain>2) {
			rel=0;
			test=src-*map;
			while (test>=srclim) { //still in the buffer
				if ((*test==*src) && (test[1]==src[1]) && (test[2]==src[2])) { //the first 3 are identical. 
					i=3; //0,1,2 already tested
					while (i<=imax) if (test[i]==src[i]) ++i; else break;
					//i is 3..18 for 12 bit window
					if (i>bestlen) { //new highscore
						bestmatch=src-test;
						bestlen=i;
						if (i>=copymax) break;
					}
				}
				test-=*(map - (src-test));
			}
		}
		if (bestlen<3) { //literal
			*dst++=*src++;
			++map;
			--remain;
		} else { //reference
			*flagptr=(*flagptr) | (1<<flagbit);
			*dst++=bestmatch; //lower 8 bits of distance
			*dst++=((bestmatch>>8)<<lengthbits) | (bestlen-3);
			remain-=bestlen;
			src+=bestlen;
			map+=bestlen;
		}
		++flagbit; //flag bit consumed
	} while (remain);
	if (endstate) {
		endstate->optr=dst;
		endstate->flagrel=flagptr-dst;
		endstate->bitcount=flagbit;
	}
	//printf("enddst srclen=%i winbit=%i pre=%i\n",srclen,winbit,preview);
	return dst;
}


unsigned char *lzssk_encode_w(unsigned char *dst, const unsigned char *src, unsigned srclen, int winbit, int preview/*0*/, lzsskcombine_t *endstate )
{
	unsigned bestmatch,i,imax;
	unsigned remain;
	unsigned char *flagptr;
	unsigned flagbit,betpos,bestlen;
	const unsigned char *test;
	const unsigned char *srclim=src-preview; //used for multithreaded compression. Can also be used to force an uncompressed lead-in

	int lengthbits=16-winbit;
	unsigned copymax=(1<<lengthbits)+2; // 18 for 12 bit window, 66 for 10 bit window
	int scanlength=(1<<winbit)-1;  //max backreference all-ones in length

	remain=srclen;
	if (!dst) {
		flagbit=endstate->bitcount;
		dst=endstate->optr;
		flagptr=dst+endstate->flagrel;
	} else {
		flagbit=8;
	}
	do {
		if (flagbit==8) { //allocate room for tag byte
			flagptr=dst;
			*dst++=0;
			flagbit=0; //we have room for 8 flag bits now.
		}
		if ((src-srclim)>scanlength) srclim=src-scanlength; //is srclim is more than 4k behind, move srclim ahead
		bestlen=0;
		test=src-1; //search backwards, starting with last output byte
		imax=copymax-1;
		if (remain<copymax) imax=remain-1; //searchlength should not exceed input
		if (remain>2) {
			while (test>=srclim) { //still in the buffer
				if ((*test==*src) && (test[1]==src[1]) && (test[2]==src[2])) { //the first 3 are identical. 
					i=3; //0,1,2 already tested
					while (i<=imax) if (test[i]==src[i]) ++i; else break;
					//i is 3..18 for 12 bit window
					if (i>bestlen) { //new highscore
						bestmatch=src-test;
						bestlen=i;
						if (i>=copymax) break;
					}
				}
				--test;
			}
		}
		if (bestlen<3) { //literal
			*dst++=*src++;
			--remain;
		} else { //reference
			*flagptr=(*flagptr) | (1<<flagbit);
			*dst++=bestmatch; //lower 8 bits of distance
			*dst++=((bestmatch>>8)<<lengthbits) | (bestlen-3);
			remain-=bestlen;
			src+=bestlen;
		}
		++flagbit; //flag bit consumed
	} while (remain);
	if (endstate) {
		endstate->optr=dst;
		endstate->flagrel=flagptr-dst;
		endstate->bitcount=flagbit;
	}
	return dst;
}

////////////////////// multithreaded compression ///////////////////
#if (defined(_WIN32) || defined(_WIN64) || defined(unix) || \
    defined(__linux__) || defined(__unix__) || defined(__unix) || \
    (defined(__APPLE__) && defined(__MACH__))) && (!defined(NOMULTITHREAD))

#if (defined(_WIN32) || defined(_WIN64))
 #include <windows.h>
 #include <tchar.h>
 #include <strsafe.h>
#else
 #include <pthread.h>
 #include <sys/sysinfo.h>
 #include <unistd.h>
#endif

#define LZSSK_DELTAMAPS /*use more memory for speed*/
#include <malloc.h>

typedef struct { //information for a worker thread
	unsigned char *dst; //this one we wish to keep for length calculation
	union {
		struct { //these are input only
			unsigned char *src;
			unsigned srclen;
			unsigned lookback;
			unsigned char windowbit;
			unsigned char deltamap;
		};
		struct { //these are output only
			lzsskcombine_t cm;
		};
	};
} cpworker_t;

#if (defined(_WIN32) || defined(_WIN64))
static DWORD WINAPI cpworker( LPVOID arg )
#else
static void *cpworker(void *arg)
#endif
{
	cpworker_t *d=(cpworker_t *)arg;
#ifdef LZSSK_DELTAMAPS
	int lb=d->lookback;
	int window=1<<(d->windowbit);
	if (lb>window) lb=window;
	uint16_t *tm=0;
	if (d->deltamap) tm=lzssk_build_deltamap(d->src-lb, d->srclen+lb);
	if (tm) {
		lzssk_encode_wdm(d->dst, d->src, d->srclen, d->windowbit, lb, &d->cm, tm+lb);
		free(tm);
		//warning: at this point, d->src and struct-mates are overwritten by d->cm
	} else
#endif    
	lzssk_encode_w(d->dst,d->src, d->srclen, d->windowbit, d->lookback, &d->cm);
	return 0;
}

int lzssk_cpus(void)
{
#if defined(FORCECPUS)
	return FORCECPUS;
#else
	static int cputhreads=0;
	if (cputhreads==0) {
 #if (defined(_WIN32) || defined(_WIN64))
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		cputhreads=sysinfo.dwNumberOfProcessors; //number of logical processors
 #else
		//linux, *bsd, macos, OSX, all have the same interface
  #ifdef _SC_NPROCESSORS_ONLN
		cputhreads=sysconf(_SC_NPROCESSORS_ONLN); //get_nprocs();
  #endif
  #ifdef _SC_NPROCESSORS_CONF
		if (cputhreads<1) cputhreads=sysconf(_SC_NPROCESSORS_CONF); /*other unix*/
  #endif
 #endif
		if (cputhreads<1) cputhreads=1;
	}
	return cputhreads;
#endif
}


int lzssk_threadpack(unsigned char *dst, unsigned char *src, int srclen, int wbit, int cachespacing, int deltamap, int minblock)
{
	cpworker_t ctl[LZSSK_MAX_USED_CPUS]; //768 byte for control with 32 threads
	int block;
	int omax=0;
	int i,k;
	int threads;
	int minblk = (1<<(16-wbit))+3; //a full length copy with the used window, not really the
                                   //minimum, but the minimum where compression can be any good
	if (minblk<minblock) minblk=minblock;

	threads=lzssk_cpus();
	if (threads>LZSSK_MAX_USED_CPUS) threads=LZSSK_MAX_USED_CPUS;
	block=srclen/threads;
	while ((block<minblk)&&(threads>1)) { //very small input to compressor
		threads/=2;
		if (threads<1) threads=1;
		block=srclen/threads;
	}
	if (cachespacing==1) cachespacing=64; //i3,i5,i7,i9,R3,R5,R7,R9 have 64byte cache lines
	if (!dst) { //asking for compression buffer length
		if (threads==1) return (((srclen+7)/8)+block);
		omax += (((block+7)/8)+block)*(threads-1);
		block+=srclen-(block*threads); //last thread gets rounding added in
		omax += ((block+7)/8)+block;
		omax += (threads-1)*cachespacing;
		return omax;
	}
	if (threads==1) { //not enough work to run multithreaded
		unsigned char *po;
#ifdef LZSSK_DELTAMAPS
		uint16_t *tm=0;
		if (deltamap) tm=lzssk_build_deltamap(src, srclen);
		if (tm) {
			po=lzssk_encode_wdm(dst,src,srclen,wbit,0,0,tm);
			free(tm);
		} else
#endif
		po=lzssk_encode_w(dst,src,srclen,wbit,0,0);
		return po-dst;
	}
	//prepare jobs. The output buffer is divided into N blocks,
	//with 'cachespacing' between them. As blocks complete,
	//this thread will collate them.
	for (i=0; i<threads; ++i) {
		ctl[i].src=src+(i*block);
		ctl[i].srclen=block;
		ctl[i].dst=dst+omax;
		ctl[i].lookback=i*block;
		ctl[i].windowbit=wbit;
		ctl[i].deltamap=deltamap;
		omax+=((block+7)/8)+block;
		omax+=cachespacing; //the last round's update of omax is a dummy. We do not use it.
	}
	ctl[i-1].srclen+=srclen-(block*threads); //last thread takes care of the rounding error
#if (defined(_WIN32) || defined(_WIN64))
	DWORD   dwThreadIdArray[LZSSK_MAX_USED_CPUS];
	HANDLE  hThreadArray[LZSSK_MAX_USED_CPUS];
	for (i=0; i<threads; ++i) {
		hThreadArray[i] = CreateThread(
			NULL/*SA*/, 0/*stacksize*/, cpworker, ctl+i,
			0/*flags*/, &dwThreadIdArray[i]);
	}
	WaitForSingleObject(hThreadArray[0],INFINITE); //retire job 0
	for (i=1; i<threads; ++i) {
		WaitForSingleObject(hThreadArray[i],INFINITE);
		CloseHandle(hThreadArray[i]);
		k=lzssk_combine_add(&ctl[0].cm, ctl[i].dst, ctl[i].cm.optr-ctl[i].dst)-dst;
	}
#else
	pthread_t tid[LZSSK_MAX_USED_CPUS];
	for (i = 0; i < threads; i++) pthread_create(&tid[i], NULL, cpworker, ctl+i); //start jobs
	pthread_join(tid[0], NULL); //retire job 0
	for (i = 1; i < threads; i++) {
		pthread_join(tid[i], NULL); //retire job
		k=lzssk_combine_add(&ctl[0].cm, ctl[i].dst, ctl[i].cm.optr-ctl[i].dst)-dst;
	}
#endif
	return k;
}

int lzssk_threadpack_dstsize(int srclen, int wbit, int cachespacing)
{
	return lzssk_threadpack(0,0,srclen,wbit,cachespacing,0,0);
}
/*end of multithreaded compression*/
#endif


//////////////////////////////////////////
//
//  create an uncompressed image that will
//  be readeble by the compressor, only 1
//  byte larger
//  .. and not the 12.5% expansion
//
char *lzssk_encodecopy(unsigned char *dst, const unsigned char *src, unsigned srclen)
{
	*dst++=0x01;
	while (srclen) {
		*dst++=*src++;
		--srclen;
	}
	return dst;
}


//////////////////////////////////////////
//
//  memory-memory decompress with destination length check
//
//
unsigned lzssk_decode(unsigned char *dst, unsigned dstlen, const unsigned char *src, int srclen, int win)
{ //output length limited version
	unsigned char *sdst=dst;
	unsigned flag; //16 bit used
	int lenbit=16-win;
	int lenmask=(1<<lenbit)-1;
  
	//copy support
	if (*src==0x01) {
		if (srclen<2) return 0;
		--srclen;
		++src;
		if (dstlen<(unsigned)srclen) return 0; //safety
		do *dst++=*src++; while (--srclen);
	}
	//depack
	while (srclen>0) {
		flag=(*(src++))|256; //get tagbyte
		if (!(--srclen)) return 0;
		do {
			if (flag&1) {
				unsigned len,pos;
				unsigned char *s;
				pos=*src++;
				if (!(--srclen)) return 0;
				len=*src++;
				pos|=(len>>lenbit)<<8;
				len=(len&lenmask)+3;
				s=dst-pos;
				if (dstlen<len) return 0;
				dstlen-=len;
				do *dst++=*s++; while (--len);
			} else {
				if (!dstlen) return 0;
				--dstlen;
				*dst++=*src++;
			}
			if (!(--srclen)) break;
			flag>>=1;
		} while (flag!=1);
	}
	return dst-sdst;
}

static unsigned char rdbytei(struct lzsskstruct *s)
{
	return *(s->srcp++);
}

int lzssk_init(lzssk_t *st, unsigned char *srcaddress, unsigned srclen, int winbit)
{
	return lzssk_init2(st,srcaddress,srclen,winbit,rdbytei);
}

int lzssk_init2(lzssk_t *st, unsigned char *srcaddress, unsigned srclen, int winbit, unsigned char (*getsrcfn)(struct lzsskstruct *s))
{
	st->getsrc=getsrcfn;
	st->optr=0;
	st->ref_len=0;
	st->srcleft=srclen;
	st->srcp=srcaddress;
	st->flag=st->getsrc(st); //replace if you are reading from something not memory
	st->lenbit=16-winbit;
	st->ringmask=(1<<winbit)-1;
	st->lenmask=(1<<(st->lenbit ))-1;
	st->srcleft--;
	if (st->flag==0x01) { //bit 0 of the first tag byte cannot be set in an lzssk stream
		st->flag=0; //unlimited literal copy
	} else {
		st->flag|=256; //compressed, first byte is literal
	}
	return 1;
}

int lzssk_readbyte(lzssk_t *st)
{
	unsigned len,pos,s,tmp;
	unsigned ringmask=st->ringmask;

	if (!(st->ref_len)) {
		if (st->flag==1) { //our tagbuffer has gone empty
			if ((st->srcleft)<2) return -1; //not a valid encoding
			st->flag=st->getsrc(st) | 256;
			--st->srcleft;
		}
		if (st->flag&1) { //set up refcopy
			st->flag>>=1;
			if ((st->srcleft)<2) { st->srcleft=0; return -1; } //not a valid encoding
			st->srcleft-=2;
			pos=st->getsrc(st);
			len=st->getsrc(st);
			pos|=(len>>(st->lenbit))<<8;
			st->ref_len=(len&(st->lenmask))+3;
			st->ref_s=(st->optr-pos) & ringmask;
		} else { //literal
			st->flag>>=1;
			if (!st->srcleft) return -1;
			st->srcleft--;
			tmp=st->getsrc(st);
			st->buf[st->optr] = tmp;
			st->optr = (st->optr+1) & ringmask;
			return tmp;
		}
	}
	 //refcopy state
	st->ref_len--;
	s=st->ref_s;
	tmp=st->buf[s];
	st->buf[st->optr]=tmp;
	st->optr = (st->optr+1) & ringmask;
	st->ref_s = (s+1) & ringmask;
	return tmp;
}

int lzssk_eof(lzssk_t *st)
{
	return ((st->srcleft<=0) && (st->ref_len==0)); //ref_len is 32768 for uncompressed
}

void lzssk_combine_init(lzsskcombine_t *st, unsigned char *dst)
{
	st->bitcount=8;
	st->optr=dst;
}

unsigned char *lzssk_combine_add(lzsskcombine_t *st, const unsigned char *src, int srclen)
{
	unsigned flag=1;
	unsigned char *flagptr=st->optr+st->flagrel;
	while (srclen>0) {
		if (flag==1) {
			flag=(*(src++))|256; //get tagbyte
			if (!(--srclen)) return 0; //fail, there must be data
		}
		if (st->bitcount==8) {
			flagptr=st->optr;
			*(st->optr++)=0;
			st->bitcount=0;
		}
		//transfer extra character if tagged
		if (flag&1) {
			*st->optr++=*src++;
			if (!(--srclen)) return 0; //fail, no second byte
			*flagptr|=1<<(st->bitcount);
		}
		*st->optr++=*src++;
		--srclen;
		st->bitcount++;
		flag>>=1;
	}
	st->flagrel=flagptr-(st->optr);
	return st->optr;
}

/////////////////////////////////////////////////////////////////////////////
