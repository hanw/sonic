/* 
 * Copyright 2011 Ki Suh Lee <kslee@cs.cornell.edu>
 *
 * The algorithm is based on 
 * 	"Fast CRC Computation for Generic Polynomials Using PCLMULQDQ Instruction" 
 * 									by Intel
 * 
 */

#if SONIC_KERNEL
#include <asm/i387.h>
#include <asm/asm.h>
#include <asm/cpufeature.h>
#include <linux/slab.h>
#else /* SONIC_KERNEL */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#endif /* SONIC_KERNEL */

#include "sonic.h"
#include "crc32.h"
//#include "tester.h"

//#if SONIC_DDEBUG && !SONIC_KERNEL
//#define FAST_CRC_DEBUG
//#define FAST_CRC_DEBUG_BIT
#define NUM_OUTPUT      1
//#endif /*SONIC_DDEBUG*/

static uint64_t k3[] __attribute__ ((aligned (16))) ={	
                0x653d9822,         // k1
                0x111a288ce,        // k2
                0x65673b46,         // k3
                0x140d44a2e,        // k4
                0xccaa009e,         // k5
                0x163cd6124,        // k6
                0x1f7011641,        // u
                0x1db710641,        // p
};

#ifdef FAST_CRC_DEBUG_BIT
static char * print_binary_128 (char * buf, uint64_t h, uint64_t l)
{
    int j;
    char *pos = buf;

    for ( j = 63 ; j >= 0 ; j --) {
        *pos++ = (h>>j) & 0x1 ? '1' : '0';
        if (j %8 == 0)
            *pos++ = ' ';
    }

    for ( j = 63 ; j >= 0 ; j --) {
        *pos++ = (h>>j) & 0x1 ? '1' : '0';
        if (j %8 == 0)
            *pos++ = ' ';
    }

    return pos;
}
#endif

uint32_t crc32_bitwise(uint32_t crc, unsigned char *bytes, int len)
{
    int i;
    while (len--)
    {   
        crc ^= *bytes++;
        for (i = 0; i < 8; i++)
            crc = (crc >> 1) ^ ((crc & 1) ? CRCPOLY_LE : 0); 
    }   
    return crc;
}

#if SONIC_FAST_CRC
uint32_t fast_crc(uint32_t crc, unsigned char * data, int len)
{
//	uint32_t crc; 
	uint64_t tmp[6] __attribute__ ((aligned (16))) ;
#ifdef FAST_CRC_DEBUG
	uint64_t *output;
	int i;

	output = ALLOC(sizeof(uint64_t) * 56);
#endif
    len += 4;

#if !SONIC_KERNEL
	if (len < 64) {
		SONIC_DPRINT("ERROR len = %d\n", len);
		return -1;
	}
//	assert (data != NULL && len >= 64);
#endif

#if  SONIC_KERNEL
//	kernel_fpu_begin();
#endif

	/* len is always larger than 64 */
	__asm__ (
#ifdef FAST_CRC_DEBUG
		"mov %5, %%rax;"
#endif /* FAST_CRC_DEBUG */

		"movdqa (%3), %%xmm15;"		// xmm15 = k1 | k2;
		"movdqa 16(%3), %%xmm14;"	// xmm14 = k3 | k4;
		"movdqa 32(%3), %%xmm13;"  	// xmm13 k5 | k6;
		"movdqa 48(%3), %%xmm12;"	// xmm12 u and p
		// 802.3 requires first 32bit to be complemented

		"movl $0xffffffff, %%ecx;"
		"movq %%rcx, %%xmm4;"

		"movq %2, %%rcx;"		// rcx = data
		"movl %1, %%ebx;"		// rbx = len

		// read 64 bytes first
		"movdqa (%%rcx), %%xmm2;"
		// complement first 4 bytes
		"pxor %%xmm2, %%xmm4;"	

		"movdqa 16(%%rcx), %%xmm5;"
		"movdqa 32(%%rcx), %%xmm6;"
		"movdqa 48(%%rcx), %%xmm7;"

		"add $64, %%rcx;"
		"sub $64, %%rbx;"

		"cmp $64, %%rbx;"
		"jl fold_by_4_done;"

		// while (len >= 128)
		// folding by 4 loop begins	
	"__loop_fold_by_4_begin: \n\t"
		"movdqa %%xmm4, %%xmm0;"
		"movdqa %%xmm5, %%xmm1;"
		"movdqa %%xmm6, %%xmm2;"
		"movdqa %%xmm7, %%xmm3;"
		"movdqa (%%rcx), %%xmm8;"	// xmm1 = *data
		"movdqa 16(%%rcx), %%xmm9;"
		"movdqa 32(%%rcx), %%xmm10;"
		"movdqa 48(%%rcx), %%xmm11;"
		"pclmulqdq $0x00, %%xmm15, %%xmm4;"
		"pclmulqdq $0x00, %%xmm15, %%xmm5;"
		"pclmulqdq $0x00, %%xmm15, %%xmm6;"
		"pclmulqdq $0x00, %%xmm15, %%xmm7;"
		"pclmulqdq $0x11, %%xmm15, %%xmm0;"
		"pclmulqdq $0x11, %%xmm15, %%xmm1;"
		"pclmulqdq $0x11, %%xmm15, %%xmm2;"
		"pclmulqdq $0x11, %%xmm15, %%xmm3;"
		"pxor %%xmm0, %%xmm4;"
		"pxor %%xmm1, %%xmm5;"
		"pxor %%xmm2, %%xmm6;"
		"pxor %%xmm3, %%xmm7;"
		"pslldq $4, %%xmm4;"
		"pslldq $4, %%xmm5;"
		"pslldq $4, %%xmm6;"
		"pslldq $4, %%xmm7;"
		"pxor %%xmm8, %%xmm4;"
		"pxor %%xmm9, %%xmm5;"
		"pxor %%xmm10, %%xmm6;"
		"pxor %%xmm11, %%xmm7;"
		"sub $64, %%rbx;"
		"add $64, %%rcx;"

		"cmp $64, %%rbx;"
		"jge __loop_fold_by_4_begin;"

#ifdef FAST_CRC_DEBUG
//		"movdqu %%xmm4, (%%rax);"	// 0
//		"add $16, %%rax;"
//		"movdqu %%xmm5, (%%rax);"	// 1
//		"add $16, %%rax;"
//		"movdqu %%xmm6, (%%rax);"	// 2
//		"add $16, %%rax;"
//		"movdqu %%xmm7, (%%rax);"	// 3
//		"add $16, %%rax;"
#endif /* FAST_CRC_DEBUG */

		// fold by 4 done!!
	"fold_by_4_done:\n\t"

		// folding by 1 with 4-folded data first
		"movdqa %%xmm4, %%xmm2;"
		"pclmulqdq $0x00, %%xmm14, %%xmm4;"
		"pclmulqdq $0x11, %%xmm14, %%xmm2;"
		"pxor %%xmm2, %%xmm4;"
		"pslldq $4, %%xmm4;"
		"pxor %%xmm5, %%xmm4;"
		
		"movdqa %%xmm4, %%xmm2;"
		"pclmulqdq $0x00, %%xmm14, %%xmm4;"
		"pclmulqdq $0x11, %%xmm14, %%xmm2;"
		"pxor %%xmm2, %%xmm4;"
		"pslldq $4, %%xmm4;"
		"pxor %%xmm6, %%xmm4;"

		"movdqa %%xmm4, %%xmm2;"
		"pclmulqdq $0x00, %%xmm14, %%xmm4;"
		"pclmulqdq $0x11, %%xmm14, %%xmm2;"
		"pxor %%xmm2, %%xmm4;"
		"pslldq $4, %%xmm4;"
		"pxor %%xmm7, %%xmm4;"

#ifdef FAST_CRC_DEBUG
//		"movdqu %%xmm4, (%%rax);"	// 4
//		"add $16, %%rax;"
#endif /* FAST_CRC_DEBUG */

		"cmp $16, %%rbx;"
		"jl fold_by_1_done;"

		// folding by 1 loop begins
	"__loop_fold_by_1_begin: \n\t"
		"movdqa %%xmm4, %%xmm2;"
		"movdqa (%%rcx), %%xmm1;"
		"pclmulqdq $0x00, %%xmm14, %%xmm4;"
		"pclmulqdq $0x11, %%xmm14, %%xmm2;"
		"pxor %%xmm2, %%xmm4;"
		"pslldq $4, %%xmm4;"
		"pxor %%xmm1, %%xmm4;"

		"sub $16, %%rbx;"
		"add $16, %%rcx;"
		"cmp $16, %%rbx;"
	
		"jge __loop_fold_by_1_begin;"

#ifdef FAST_CRC_DEBUG
//		"movdqu %%xmm4, (%%rax);"	// 5
//		"add $16, %%rax;"
#endif /* FAST_CRC_DEBUG */

		// fold by 1 done!!
	"fold_by_1_done:\n\t"
		// FIXME
		// temporarily store to tmp and get it back
		// then data will be padded with most-significant zeros

		"cmp $0, %%rbx;"
		"je __final_reduction;"

		// fetch last data
		"movdqa (%%rcx), %%xmm1;"

		"pxor %%xmm3, %%xmm3;"
		"movdqu %%xmm3, (%4);"
		"movdqu %%xmm3, 16(%4);"
		"movdqu %%xmm3, 32(%4);"

		"movdqu %%xmm1, 32(%4);"
		"movdqu %%xmm4, 16(%4);"

		"movdqu (%4, %%rbx, 1), %%xmm1;"	// xmm1 = first
		"movdqu 16(%4, %%rbx, 1), %%xmm2;"	// xmm2 = second

		"movaps %%xmm1, %%xmm4;"
		"pclmulqdq $0x00, %%xmm14, %%xmm1;"
		"pclmulqdq $0x11, %%xmm14, %%xmm4;"
		"pxor %%xmm1, %%xmm4;"
		"pslldq $4, %%xmm4;"
		"pxor %%xmm2, %%xmm4;"
		// xmm4 is last 128-bit data

#ifdef FAST_CRC_DEBUG	
//		"movdqu %%xmm4, (%%rax);"	// 6
//		"add $16, %%rax;"
#endif /* FAST_CRC_DEBUG */	
	"__final_reduction:\n\t"
		"movdqa %%xmm4, %%xmm2;"

		"pxor %%xmm3, %%xmm3;"
		"punpckldq %%xmm3, %%xmm4;"
		"movdqa %%xmm4, %%xmm1;"
		"pclmulqdq $0x00, %%xmm13, %%xmm4;"
		"pclmulqdq $0x11, %%xmm13, %%xmm1;"
		"psrldq $8, %%xmm2;"
		"pxor %%xmm2, %%xmm4;"
		"pxor %%xmm1, %%xmm4;"

#ifdef FAST_CRC_DEBUG
		"movdqu %%xmm4, (%%rax);"	// 7
		"add $16, %%rax;"
#endif /* FAST_CRC_DEBUG */
	
		//"jmp __fast_crc_done;"

		// very last step, running BG on 64bits
		"movdqa %%xmm4, %%xmm2;"
		
		"pslldq $4, %%xmm4;"
		"pclmulqdq $0x00, %%xmm12, %%xmm4;"
		"pclmulqdq $0x10, %%xmm12, %%xmm4;"
		"psrldq $4, %%xmm4;"
		"pxor %%xmm2, %%xmm4;"
	"__fast_crc_done:"
		"pextrd $1, %%xmm4, %0;"

#ifdef FAST_CRC_DEBUG
//		"movdqu %%xmm4, (%%rax);"	// 8
//		"add $16, %%rax;"
#endif /* FAST_CRC_DEBUG */
		
		: "=r" (crc)// output
		:"r" (len), "r" (data), "r" (k3), "r" (tmp)
#ifdef FAST_CRC_DEBUG
		, "r" (output)
#endif /* FAST_CRC_DEBUG */
		: "%rax", "%rbx", "%rcx"
#if !SONIC_KERNEL
		  , "%xmm0", "%xmm1", "%xmm2", "%xmm3", 
		  "%xmm4", "%xmm5", "%xmm6", "%xmm7", 
		  "%xmm8", "%xmm9", "%xmm10", "%xmm11", 
		  "%xmm12", "%xmm13", "%xmm14", "%xmm15"
#endif
	);

#ifdef FAST_CRC_DEBUG
	for (i = 0 ; i < NUM_OUTPUT *2 ; i += 2) {
		char buf[512], *pos;
		uint64_t x = output[i];
		uint64_t y = output[i+1];

		pos = buf;

		sprintf(buf, "i = %3d %.16llx %.16llx: ", i, (unsigned long long ) y, 
				(unsigned long long ) x);
		pos = buf + strlen(buf);
#ifdef FAST_CRC_DEBUG_BIT
		pos = print_binary_128 (buf + strlen(buf), y, x);
#endif /* FAST_CRC_DEBUG_BIT */
		*pos++ = '\n';
		*pos = '\0';
		SONIC_PRINT("%s", buf);
	}
	SONIC_PRINT("\n");
	FREE(output);
#endif	 /* FAST_CRC_DEBUG */

#if  SONIC_KERNEL
//	kernel_fpu_end();
#endif
	return crc;
}

uint32_t fast_crc_nobr(uint32_t crc, unsigned char * data, int len)
{
//	uint32_t crc =0; 
	uint64_t tmp[6] __attribute__ ((aligned (16))) ;
#ifdef FAST_CRC_DEBUG
	uint64_t *output;
	int i;

	output = ALLOC(sizeof(uint64_t) * 1000);
#endif
//	tmp[0] = 0; tmp[1] = 0; tmp[2] = 0; tmp[3] = 0; tmp[4] = 0; tmp[5] = 0;

	/* len is always larger than 64 */

	__asm__ (
#ifdef FAST_CRC_DEBUG
		"mov %5, %%rax;"
#endif /* FAST_CRC_DEBUG */

		"movdqa (%3), %%xmm15;"
		"movdqa 16(%3), %%xmm14;"
		"movdqa 32(%3), %%xmm13;"
		"movdqa 48(%3), %%xmm12;"

		// 802.3 requires first 32bit to be complemented
		"movl $0xffffffff, %%ecx;"
		"movq %%rcx, %%xmm4;"

		"movq %2, %%rcx;"		// rcx = data
		"movl %1, %%ebx;"		// rbx = len

		// there must be at least 4 128-bits
		// since minimum size of ethernet frame is 64 bytes

		"movdqa (%%rcx), %%xmm2;"
		// complement first 4 bytes
		"pxor %%xmm2, %%xmm4;"

		"sub $16, %%rbx;"
		"add $16, %%rcx;"
		"cmp $48, %%rbx;"
		"jl __loop_fold_by_1_begin_;"


		"movdqa 0(%%rcx), %%xmm5;"
		"movdqa 16(%%rcx), %%xmm6;"
		"movdqa 32(%%rcx), %%xmm7;"

		"add $48, %%rcx;"
		"sub $48, %%rbx;"

		"cmp $64, %%rbx;"
		"jl __fold_by_4_done_;"

		// while (len >= 128)
		// folding by 4 loop begins	
	"__loop_fold_by_4_begin_: \n\t"
		"movdqa %%xmm4, %%xmm0;"
		"movdqa %%xmm5, %%xmm1;"
		"movdqa %%xmm6, %%xmm2;"
		"movdqa %%xmm7, %%xmm3;"
		"movdqa (%%rcx), %%xmm8;"	// xmm1 = *data
		"movdqa 16(%%rcx), %%xmm9;"
		"movdqa 32(%%rcx), %%xmm10;"
		"movdqa 48(%%rcx), %%xmm11;"
		"pclmulqdq $0x00, %%xmm15, %%xmm4;"
		"pclmulqdq $0x00, %%xmm15, %%xmm5;"
		"pclmulqdq $0x00, %%xmm15, %%xmm6;"
		"pclmulqdq $0x00, %%xmm15, %%xmm7;"
		"pclmulqdq $0x11, %%xmm15, %%xmm0;"
		"pclmulqdq $0x11, %%xmm15, %%xmm1;"
		"pclmulqdq $0x11, %%xmm15, %%xmm2;"
		"pclmulqdq $0x11, %%xmm15, %%xmm3;"
		"pxor %%xmm0, %%xmm4;"
		"pxor %%xmm1, %%xmm5;"
		"pxor %%xmm2, %%xmm6;"
		"pxor %%xmm3, %%xmm7;"
		"pslldq $4, %%xmm4;"
		"pslldq $4, %%xmm5;"
		"pslldq $4, %%xmm6;"
		"pslldq $4, %%xmm7;"
		"pxor %%xmm8, %%xmm4;"
		"pxor %%xmm9, %%xmm5;"
		"pxor %%xmm10, %%xmm6;"
		"pxor %%xmm11, %%xmm7;"
		"sub $64, %%rbx;"
		"add $64, %%rcx;"

		"cmp $64, %%rbx;"
		"jge __loop_fold_by_4_begin_;"

#ifdef FAST_CRC_DEBUG
		"movdqu %%xmm4, (%%rax);"
		"add $16, %%rax;"
		"movdqu %%xmm5, (%%rax);"
		"add $16, %%rax;"
		"movdqu %%xmm6, (%%rax);"
		"add $16, %%rax;"
		"movdqu %%xmm7, (%%rax);"
		"add $16, %%rax;"
#endif /* FAST_CRC_DEBUG */

		// fold by 4 done!!
	"__fold_by_4_done_:\n\t"

		// folding by 1 with 4-folded data first
		"movdqa %%xmm4, %%xmm2;"
		"pclmulqdq $0x00, %%xmm14, %%xmm4;"
		"pclmulqdq $0x11, %%xmm14, %%xmm2;"
		"pxor %%xmm2, %%xmm4;"
		"pslldq $4, %%xmm4;"
		"pxor %%xmm5, %%xmm4;"
		
		"movdqa %%xmm4, %%xmm2;"
		"pclmulqdq $0x00, %%xmm14, %%xmm4;"
		"pclmulqdq $0x11, %%xmm14, %%xmm2;"
		"pxor %%xmm2, %%xmm4;"
		"pslldq $4, %%xmm4;"
		"pxor %%xmm6, %%xmm4;"

		"movdqa %%xmm4, %%xmm2;"
		"pclmulqdq $0x00, %%xmm14, %%xmm4;"
		"pclmulqdq $0x11, %%xmm14, %%xmm2;"
		"pxor %%xmm2, %%xmm4;"
		"pslldq $4, %%xmm4;"
		"pxor %%xmm7, %%xmm4;"

#ifdef FAST_CRC_DEBUG
		"movdqu %%xmm4, (%%rax);"
		"add $16, %%rax;"
#endif /* FAST_CRC_DEBUG */

		"cmp $16, %%rbx;"
		"jl fold_by_1_done_;"

		// folding by 1 loop begins
	"__loop_fold_by_1_begin_: \n\t"
		"movdqa %%xmm4, %%xmm2;"
		"movdqa (%%rcx), %%xmm1;"
		"pclmulqdq $0x00, %%xmm14, %%xmm4;"
		"pclmulqdq $0x11, %%xmm14, %%xmm2;"
		"pxor %%xmm2, %%xmm4;"
		"pslldq $4, %%xmm4;"
		"pxor %%xmm1, %%xmm4;"

		"sub $16, %%rbx;"
		"add $16, %%rcx;"
		"cmp $16, %%rbx;"

		"jge __loop_fold_by_1_begin_;"

#ifdef FAST_CRC_DEBUG
		"movdqu %%xmm4, (%%rax);"
		"add $16, %%rax;"
#endif /* FAST_CRC_DEBUG */

		// fold by 1 done!!
	"fold_by_1_done_:\n\t"
		// temporarily store to tmp and get it back
		// then data will be padded with most-significant zeros

//		"jmp __fast_crc_nobr_done;"
		"cmp $0, %%rbx;"
		"je __final_reduction_;"

		// fetch last data
		"movdqa (%%rcx), %%xmm1;"

		"pxor %%xmm3, %%xmm3;"
		"movdqa %%xmm3, (%4);"
//		"movdqa %%xmm3, 16(%4);"

		"movdqa %%xmm1, 32(%4);"
		"movdqa %%xmm4, 16(%4);"

		"movdqu (%4, %%rbx, 1), %%xmm1;"	// xmm1 = first
		"movdqu 16(%4, %%rbx, 1), %%xmm2;"	// xmm2 = second

		"movaps %%xmm1, %%xmm4;"
		"pclmulqdq $0x00, %%xmm14, %%xmm1;"
		"pclmulqdq $0x11, %%xmm14, %%xmm4;"
		"pxor %%xmm1, %%xmm4;"
		"pslldq $4, %%xmm4;"
		"pxor %%xmm2, %%xmm4;"
		// xmm4 is last 128-bit data

#ifdef FAST_CRC_DEBUG	//12
		"movdqu %%xmm4, (%%rax);"
		"add $16, %%rax;"
#endif /* FAST_CRC_DEBUG */	

	"__final_reduction_:\n\t"
//		"jmp __fast_crc_nobr_done;"
		"movdqa %%xmm4, %%xmm2;"

		"pclmulqdq $0x00, %%xmm13, %%xmm2;"	// xmm2 = x^64
		"psrldq $8, %%xmm4;"
		"pxor %%xmm2, %%xmm4;"		// x^96
		"pslldq $4, %%xmm4;"
		"movdqa %%xmm4, %%xmm3;"
		"pclmulqdq $0x10, %%xmm13, %%xmm4;"
		"pslldq $4, %%xmm4;"
		"pxor %%xmm3, %%xmm4;"
		"psrldq $8, %%xmm4;"	// R

#ifdef FAST_CRC_DEBUG
		"movdqu %%xmm4, (%%rax);"
		"add $16, %%rax;"
#endif /* FAST_CRC_DEBUG */
	
		// very last step, running BG on 64bits
		"movdqa %%xmm4, %%xmm2;"

		"pslldq $4, %%xmm4;"
		"pclmulqdq $0x00, %%xmm12, %%xmm4;"
		"pclmulqdq $0x10, %%xmm12, %%xmm4;"
		"psrldq $4, %%xmm4;"
		"pxor %%xmm2, %%xmm4;"
		"pextrd $1, %%xmm4, %0;"

#ifdef FAST_CRC_DEBUG
		"movdqu %%xmm4, (%%rax);"
		"add $16, %%rax;"
#endif /* FAST_CRC_DEBUG */
		
	"__fast_crc_nobr_done_:"
		: "=r" (crc)// output
		:"r" (len), "r" (data), "r" (k3), "r" (tmp)
#ifdef FAST_CRC_DEBUG
		, "r" (output)
#endif /* FAST_CRC_DEBUG */
		: "%rax", "%rbx", "%rcx"
#if !SONIC_KERNEL
		  , "%rdx", "%rdi", 
		  "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6",
		  "%xmm7", "%xmm8", "%xmm9", "%xmm10", "%xmm11", "%xmm12",
		  "%xmm13", "%xmm14", "%xmm15"
#endif
	);

#ifdef FAST_CRC_DEBUG
	for (i = 0 ; i < NUM_OUTPUT *2 ; i += 2) {
#ifdef REVERSE_DEBUG
		uint64_t x = bit_reverse_64(output[i]);
		uint64_t y = bit_reverse_64(output[i+1]);
		printf("i = %3d %.16llx %.16llx: ", i, x, y);
#ifdef FAST_CRC_DEBUG_BIT
		print_binary_128 (y, x);
#endif /* FAST_CRC_DEBUG_BIT */
#else
		uint64_t x = output[i];
		uint64_t y = output[i+1];

//		printf("i = %3d %.16llx %.16llx: ", i, y, x);
#ifdef FAST_CRC_DEBUG_BIT
		print_binary_128 (y, x);
#endif /* FAST_CRC_DEBUG_BIT */
#endif /* REVERSE_DEBUG */
//		printf("\n");
	}
//	printf("\n");
#endif	 /* FAST_CRC_DEBUG */

	return crc;
}
#else /* SONIC_FAST_CRC */
uint32_t fast_crc(uint32_t crc, unsigned char * data, int len) {
    return crc32_le(crc, data, len);
}
uint32_t fast_crc_nobr(uint32_t crc, unsigned char * data, int len) {
    return crc32_le(crc, data, len);
}
#endif /* SONIC_FAST_CRC */

uint32_t (* sonic_crc) (uint32_t, unsigned char *, int) = fast_crc;
