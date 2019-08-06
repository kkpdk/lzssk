#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "lzssk.h"










unsigned char pki[819200],pko[1219200],doo[1219200];
unsigned char *po,*p;

int main(void)
{
  int i,d,k;
  unsigned g=1;
  unsigned tag,a,b,r;
  FILE *f;
  

  f=fopen("testfil","rb");
  if (!f) return 0;
  int rdrd;
  rdrd=fread(pki,1,sizeof(pki),f);
  fclose(f);
  printf("rd %i\n",rdrd);
//  return 0;

for (k=0; k<13; ++k) {  
/*  po=lzssk_encode_w(pko,pki, sizeof(pki), 12, 0, 0);
  printf("i=%i\n",po-pko);*/
  
/*  uint16_t *m=lzssk_build_deltamap(pki, sizeof(pki));
  po=lzssk_encode_wdm(pko,pki, sizeof(pki), 12 , 0, 0, m);
  free(m);
  printf("i=%i\n",po-pko);*/
  
}

  // 5.242 search, 0.42 tabledriven

  
//  return 0;
               
  
  
//  d=4096-11;
//  r=10;

/*
  strcpy(pki,"this is this simple test text that i simply put here here here is the text**********************************************************************************************************************************************************************************************************************");
  uint16_t *tm=lzssk_build_deltamap(pki, 100);
  
  po=lzssk_encode_w(pko,pki, 100, 12, 0, 0);
  printf("i=%i\n",po-pko);
  i=lzssk_decode(doo,8192,pko,po-pko, 12);
  printf("i=%i\n",i);
  for (k=0; k<i; ++k) printf("%c",doo[k]);
  printf("\n\n");

  for (i=0; i<10000; ++i) pko[i]=0x55;
  po=lzssk_encode_wdm(pko,pki, 100, 12, 0, 0, tm);
  printf("i=%i\n",po-pko);
  i=lzssk_decode(doo,8192,pko,po-pko, 12);
  printf("i=%i\n",i);
  for (k=0; k<i; ++k) printf("%c",doo[k]);
  printf("\n\n");
  return 0;
  */
  
  



/*  strcpy(pki,"this is this simple test text that i simply put here here here is the text**********************************************************************************************************************************************************************************************************************");
  uint16_t *tm=lzssk_build_deltamap(pki, 100);
  for (i=0; i<100; ++i) printf("%i          %i\n",i,tm[i]);
  return 0;*/


  //deltamap time: 2.3 sec per GByte.

//  for (i=0; i<65536; ++i) { g*=123456789; pki[i]=g>>24; }

#ifdef fdgfdgdfg
  //1.5s per MB at 4k, 0.4s per MB at 1k  worstcase
  for (i=0; i<160; ++i) {
    for (i=0; i<160; ++i) po=lzssk_encode_w(pko,pki, 65536, 12 , 0, 0);

    /*for (i=0; i<160; ++i) {
      uint16_t *m=lzssk_build_deltamap(pki, 65536);
      po=lzssk_encode_wdm(pko,pki, 65536, 12 , 0, 0, m);
      free(m);
    }*/
    printf("po-pko=%i\n",po-pko);
    
//    printf("%i\n",m);
    
  }
  return 0;
#endif
  
  //for (i=0; i<160; ++i) lzssk_encode_w(pko,pki, 65536, 12 , 0);
  for (i=0; i<160; ++i) lzssk_threadpack(pko,pki, 65536, 12,1,1);
 
  //4.2 seconds for 4k window 10MB. 1.15s for 1k window = 8,6MB/s @4core
  
//  return;
  strcpy(pki,"this is this simple test text that i simply put here here here is the text**********************************************************************************************************************************************************************************************************************");

  printf("k=%i\n",k=lzssk_threadpack(pko,pki,4*30,12,1,1));
  po=pko+k;
  goto dodecode;

  return 0;





#define wbit 8
  d=2;
  r=250;
  
  for (i=0; i<r; ++i) pki[i]=i; //0..9 run
  for (i=0; i<d; ++i) { g*=123456789; pki[r+i]=g>>24; }
  for (i=0; i<r; ++i) pki[r+d+i]=i; //0..9 run
  for (i=r+d+r; i<8192; ++i) { g*=123456789; pki[i]=g>>24; }

  po=lzssk_encode_w(pko,pki, r+d+r, wbit, 0, 0);
  printf("len=%i  di=%i\n",po-pko, 10+d+10);
  p=pko;
  tag=1;
  for (;;) {
    if (tag==1) {
      if (p==po) break;
      tag=(*p++)|256;
      if (p==po) break;
      printf("tag %i\n",tag&255);
    }
    if (tag&1) {
      a=*p++;
      if (p==po) break;
      b=*p++;
      printf("ref %i %i         %i %i\n",a,b,  a+((b&0xf0)<<4), b&15);
      if (p==po) break;
    } else {
      printf("lit %i\n",*p++);
      if (p==po) break;
    }
    tag>>=1;
  }
  printf("in=%i\n",po-pko);
dodecode:
  i=lzssk_decode(doo,8192,pko,po-pko, wbit);
  printf("i=%i\n",i);
  for (k=0; k<i; ++k) printf("%c",doo[k]);
  printf("\n");

  printf("decode 2:\n");
  lzssk_t stk;
  lzssk_init(&stk, pko, po-pko, 12);
  for (;;) {
    i=lzssk_readbyte(&stk);
    if (i<0) break;
    printf("%c",i);
  
  
  }




}



/*
typedef struct {
  unsigned char *flags;
  unsigned char *optr,*initialoptr;
  unsigned bitcount;
} lzsskcombine_t;

void lzssk_combine_init(lzsskcombine_t *st, unsigned char *dst)
{
  st->bitcount=8;
  st->optr=dst;
  st->initialoptr=dst;
}

int lzssk_combine_add(lzsskcombine_t *st, const unsigned char *src, int srclen)
{ //output length limited version
  unsigned flag=1;
  while (srclen>0) {
	if (st->bitcount==8) {
	  st->flags=st->optr;
	  *st->optr++=0;
	  st->bitcount=0;
	}
	if (flag==1) {
	  flag=(*(src++))|256; //get tagbyte
          if (!(--srclen)) return 0; //fail, there must be data
	}
	//transfer extra character if tagged
        if (flag&1) {
	    *st->optr++=*src++;
	    if (!(--srclen)) return 0; //fail, no second byte
            *st->flags|=1<<(st->bitcount++);
        }
        *st->optr++=*src++;
        --srclen;
        flag>>=1;
  }
  return st->optr - st->initialoptr;
}
*/
//windows packer:
//break into 1..8 blocks, such that blocks are >8k
//create a semaphore
//create a thread for each block, with appropriate lookback value
//let threads run
//wait for semaphore that number of times
//destroy the threads
//combine the outputs








