/*

   BLIS    
   An object-based framework for developing high-performance BLAS-like
   libraries.

   Copyright (C) 2014, The University of Texas at Austin

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    - Neither the name of The University of Texas at Austin nor the names
      of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "blis.h"

#include "bli_avx512_macros.h"

extern int32_t offsets[24];

#define A_PREFETCH_DIST 5
#define PREFETCH_A 1
#define PIPELINE_A 1
#define UNROLL_X2 0
#define UNROLL_X4 1

#define UPDATE_SCATTERED(n) \
    KMOV(K(1), ESI) \
    VGATHERDPD(ZMM(2) MASK_K(1), MEM(RCX,YMM(4),8)) \
    VFMADD231PD(ZMM(n), ZMM(1), ZMM(2)) \
    KMOV(K(1), ESI) \
    VSCATTERDPD(MEM(RCX,YMM(4),8) MASK_K(1), ZMM(n)) \
    ADD(RCX, RDI)

#define UPDATE_SCATTERED_BZ(n) \
    KMOV(K(1), ESI) \
    VSCATTERDPD(MEM(RCX,YMM(4),8) MASK_K(1), ZMM(n)) \
    ADD(RCX, RDI)

void bli_dgemm_opt_8x24
     (
       dim_t               k,
       double*    restrict alpha,
       double*    restrict a,
       double*    restrict b,
       double*    restrict beta,
       double*    restrict c, inc_t rs_c, inc_t cs_c,
       auxinfo_t* restrict data,
       cntx_t*    restrict cntx
     )
{
	//const void* a_next = bli_auxinfo_next_a( data );
	//const void* b_next = bli_auxinfo_next_b( data );

    const int32_t * offsetPtr = &offsets[ 0];

    uint64_t k64 = k;

	__asm__ volatile
	(
	//if columns of C are cache-split, prefetch second cache line
	//so that both lines are ready in all m_r iterations but the first
	//use vscatterpfdps to prefetch 12 lines at once
    VPXORD(ZMM(8), ZMM(8), ZMM(8)) VBROADCASTSS(ZMM(4), VAR(cs_c))
    VMOVAPD(ZMM( 9), ZMM(8))       MOV(RCX, VAR(c))
    VMOVAPD(ZMM(10), ZMM(8))       MOV(RDI, VAR(offsetPtr))
    VMOVAPD(ZMM(11), ZMM(8))       VMOVUPS(ZMM(5), MEM(RDI))
    VMOVAPD(ZMM(12), ZMM(8))       VMOVUPS(ZMM(6), MEM(RDI,12*4))
    VMOVAPD(ZMM(13), ZMM(8))       VPMULLD(ZMM(5), ZMM(5), ZMM(4))
    VMOVAPD(ZMM(14), ZMM(8))       VPMULLD(ZMM(6), ZMM(6), ZMM(4))
    VMOVAPD(ZMM(15), ZMM(8))       MOV(RDX, IMM(0xFFF))
    VMOVAPD(ZMM(16), ZMM(8))       KMOV(K(1), EDX)
    VMOVAPD(ZMM(17), ZMM(8))       //KMOV(K(2), EDX)
    VMOVAPD(ZMM(18), ZMM(8))       //VSCATTERPFDPS(0, MEM(RCX,ZMM(5),8,0*8) MASK_K(2))
    VMOVAPD(ZMM(19), ZMM(8))       VSCATTERPFDPS(0, MEM(RCX,ZMM(5),8,7*8) MASK_K(1))
    VMOVAPD(ZMM(20), ZMM(8))       //KMOV(K(1), EDX)
    VMOVAPD(ZMM(21), ZMM(8))       KMOV(K(2), EDX)
    VMOVAPD(ZMM(22), ZMM(8))       //VSCATTERPFDPS(0, MEM(RCX,ZMM(6),8,0*8) MASK_K(1))
    VMOVAPD(ZMM(23), ZMM(8))       VSCATTERPFDPS(0, MEM(RCX,ZMM(6),8,7*8) MASK_K(2))
    VMOVAPD(ZMM(24), ZMM(8))       MOV(RAX, VAR(a))
    VMOVAPD(ZMM(25), ZMM(8))       MOV(RBX, VAR(b))
    VMOVAPD(ZMM(26), ZMM(8))       ADD(RBX, IMM(15*8))
    VMOVAPD(ZMM(27), ZMM(8))       VMOVAPD(ZMM(0), MEM(RAX))
    VMOVAPD(ZMM(28), ZMM(8))       ADD(RAX, IMM(8*8))
    VMOVAPD(ZMM(29), ZMM(8))       MOV(RSI, VAR(k))
    VMOVAPD(ZMM(30), ZMM(8))
    VMOVAPD(ZMM(31), ZMM(8))

    TEST(RSI, RSI)
    JZ(.DPOSTACCUM)

#if !(UNROLL_X2 || UNROLL_X4) || !PIPELINE_A

    ALIGN32
    LABEL(.DLOOPKITER)

#if PREFETCH_A
	PREFETCH(0, MEM(RAX,A_PREFETCH_DIST*8*8))
#endif

#if PIPELINE_A
    VMOVAPD(ZMM(1), MEM(RAX))
#else
    VMOVAPD(ZMM(0), MEM(RAX))
#endif

    VFMADD231PD(ZMM( 8), ZMM(0), MEM_1TO8(RBX,-15*8))
    VFMADD231PD(ZMM( 9), ZMM(0), MEM_1TO8(RBX,-14*8))
    VFMADD231PD(ZMM(10), ZMM(0), MEM_1TO8(RBX,-13*8))
    VFMADD231PD(ZMM(11), ZMM(0), MEM_1TO8(RBX,-12*8))
    VFMADD231PD(ZMM(12), ZMM(0), MEM_1TO8(RBX,-11*8))
    VFMADD231PD(ZMM(13), ZMM(0), MEM_1TO8(RBX,-10*8))
    VFMADD231PD(ZMM(14), ZMM(0), MEM_1TO8(RBX, -9*8))
    VFMADD231PD(ZMM(15), ZMM(0), MEM_1TO8(RBX, -8*8))
    VFMADD231PD(ZMM(16), ZMM(0), MEM_1TO8(RBX, -7*8))
    VFMADD231PD(ZMM(17), ZMM(0), MEM_1TO8(RBX, -6*8))
    VFMADD231PD(ZMM(18), ZMM(0), MEM_1TO8(RBX, -5*8))
    VFMADD231PD(ZMM(19), ZMM(0), MEM_1TO8(RBX, -4*8))
    VFMADD231PD(ZMM(20), ZMM(0), MEM_1TO8(RBX, -3*8))
    VFMADD231PD(ZMM(21), ZMM(0), MEM_1TO8(RBX, -2*8))
    VFMADD231PD(ZMM(22), ZMM(0), MEM_1TO8(RBX, -1*8))
    VFMADD231PD(ZMM(23), ZMM(0), MEM_1TO8(RBX,  0*8))
    VFMADD231PD(ZMM(24), ZMM(0), MEM_1TO8(RBX,  1*8))
    VFMADD231PD(ZMM(25), ZMM(0), MEM_1TO8(RBX,  2*8))
    VFMADD231PD(ZMM(26), ZMM(0), MEM_1TO8(RBX,  3*8))
    VFMADD231PD(ZMM(27), ZMM(0), MEM_1TO8(RBX,  4*8))
    VFMADD231PD(ZMM(28), ZMM(0), MEM_1TO8(RBX,  5*8))
    VFMADD231PD(ZMM(29), ZMM(0), MEM_1TO8(RBX,  6*8))
    VFMADD231PD(ZMM(30), ZMM(0), MEM_1TO8(RBX,  7*8))
    VFMADD231PD(ZMM(31), ZMM(0), MEM_1TO8(RBX,  8*8))

#if PIPELINE_A
    VMOVAPD(ZMM(0), ZMM(1))
#endif

    ADD(RAX, IMM(8*8))
    ADD(RBX, IMM(24*8))

    SUB(RSI, IMM(1))
    JNZ(.DLOOPKITER)

#elif UNROLL_X2

    SAR1(RSI) // k -> k/2, jump to .DEXTRAITER if k was odd
    JC(.DEXTRAITER)

    LABEL(.DMAINLOOP)

    MOV(RDI, IMM(24*8))

    ALIGN32
    LABEL(.DLOOPKITER)

#if PREFETCH_A
    PREFETCH(0, MEM(RAX,A_PREFETCH_DIST*8*8))
#endif
    VMOVAPD(ZMM(1), MEM(RAX))

    VFMADD231PD(ZMM( 8), ZMM(0), MEM_1TO8(RBX,-15*8))
    VFMADD231PD(ZMM( 9), ZMM(0), MEM_1TO8(RBX,-14*8))
    VFMADD231PD(ZMM(10), ZMM(0), MEM_1TO8(RBX,-13*8))
    VFMADD231PD(ZMM(11), ZMM(0), MEM_1TO8(RBX,-12*8))
    VFMADD231PD(ZMM(12), ZMM(0), MEM_1TO8(RBX,-11*8))
    VFMADD231PD(ZMM(13), ZMM(0), MEM_1TO8(RBX,-10*8))
    VFMADD231PD(ZMM(14), ZMM(0), MEM_1TO8(RBX, -9*8))
    VFMADD231PD(ZMM(15), ZMM(0), MEM_1TO8(RBX, -8*8))
    VFMADD231PD(ZMM(16), ZMM(0), MEM_1TO8(RBX, -7*8))
    VFMADD231PD(ZMM(17), ZMM(0), MEM_1TO8(RBX, -6*8))
    VFMADD231PD(ZMM(18), ZMM(0), MEM_1TO8(RBX, -5*8))
    VFMADD231PD(ZMM(19), ZMM(0), MEM_1TO8(RBX, -4*8))
    VFMADD231PD(ZMM(20), ZMM(0), MEM_1TO8(RBX, -3*8))
    VFMADD231PD(ZMM(21), ZMM(0), MEM_1TO8(RBX, -2*8))
    VFMADD231PD(ZMM(22), ZMM(0), MEM_1TO8(RBX, -1*8))
    VFMADD231PD(ZMM(23), ZMM(0), MEM_1TO8(RBX,  0*8))
    VFMADD231PD(ZMM(24), ZMM(0), MEM_1TO8(RBX,  1*8))
    VFMADD231PD(ZMM(25), ZMM(0), MEM_1TO8(RBX,  2*8))
    VFMADD231PD(ZMM(26), ZMM(0), MEM_1TO8(RBX,  3*8))
    VFMADD231PD(ZMM(27), ZMM(0), MEM_1TO8(RBX,  4*8))
    VFMADD231PD(ZMM(28), ZMM(0), MEM_1TO8(RBX,  5*8))
    VFMADD231PD(ZMM(29), ZMM(0), MEM_1TO8(RBX,  6*8))
    VFMADD231PD(ZMM(30), ZMM(0), MEM_1TO8(RBX,  7*8))
    VFMADD231PD(ZMM(31), ZMM(0), MEM_1TO8(RBX,  8*8))

#if PREFETCH_A
    PREFETCH(0, MEM(RAX,A_PREFETCH_DIST*8*8+8*8))
#endif
    VMOVAPD(ZMM(0), MEM(RAX,8*8))

    VFMADD231PD(ZMM( 8), ZMM(1), MEM_1TO8(RBX,RDI,1,-15*8))
    VFMADD231PD(ZMM( 9), ZMM(1), MEM_1TO8(RBX,RDI,1,-14*8))
    VFMADD231PD(ZMM(10), ZMM(1), MEM_1TO8(RBX,RDI,1,-13*8))
    VFMADD231PD(ZMM(11), ZMM(1), MEM_1TO8(RBX,RDI,1,-12*8))
    VFMADD231PD(ZMM(12), ZMM(1), MEM_1TO8(RBX,RDI,1,-11*8))
    VFMADD231PD(ZMM(13), ZMM(1), MEM_1TO8(RBX,RDI,1,-10*8))
    VFMADD231PD(ZMM(14), ZMM(1), MEM_1TO8(RBX,RDI,1, -9*8))
    VFMADD231PD(ZMM(15), ZMM(1), MEM_1TO8(RBX,RDI,1, -8*8))
    VFMADD231PD(ZMM(16), ZMM(1), MEM_1TO8(RBX,RDI,1, -7*8))
    VFMADD231PD(ZMM(17), ZMM(1), MEM_1TO8(RBX,RDI,1, -6*8))
    VFMADD231PD(ZMM(18), ZMM(1), MEM_1TO8(RBX,RDI,1, -5*8))
    VFMADD231PD(ZMM(19), ZMM(1), MEM_1TO8(RBX,RDI,1, -4*8))
    VFMADD231PD(ZMM(20), ZMM(1), MEM_1TO8(RBX,RDI,1, -3*8))
    VFMADD231PD(ZMM(21), ZMM(1), MEM_1TO8(RBX,RDI,1, -2*8))
    VFMADD231PD(ZMM(22), ZMM(1), MEM_1TO8(RBX,RDI,1, -1*8))
    VFMADD231PD(ZMM(23), ZMM(1), MEM_1TO8(RBX,RDI,1,  0*8))
    VFMADD231PD(ZMM(24), ZMM(1), MEM_1TO8(RBX,RDI,1,  1*8))
    VFMADD231PD(ZMM(25), ZMM(1), MEM_1TO8(RBX,RDI,1,  2*8))
    VFMADD231PD(ZMM(26), ZMM(1), MEM_1TO8(RBX,RDI,1,  3*8))
    VFMADD231PD(ZMM(27), ZMM(1), MEM_1TO8(RBX,RDI,1,  4*8))
    VFMADD231PD(ZMM(28), ZMM(1), MEM_1TO8(RBX,RDI,1,  5*8))
    VFMADD231PD(ZMM(29), ZMM(1), MEM_1TO8(RBX,RDI,1,  6*8))
    VFMADD231PD(ZMM(30), ZMM(1), MEM_1TO8(RBX,RDI,1,  7*8))
    VFMADD231PD(ZMM(31), ZMM(1), MEM_1TO8(RBX,RDI,1,  8*8))

    ADD(RAX, IMM(2*8*8))
    ADD(RBX, IMM(2*24*8))

    SUB(RSI, IMM(1))
    JNZ(.DLOOPKITER)

    JMP(.DPOSTACCUM)

    LABEL(.DEXTRAITER)

#if PREFETCH_A
    PREFETCH(0, MEM(RAX,A_PREFETCH_DIST*8*8))
#endif
    VMOVAPD(ZMM(1), MEM(RAX))

    VFMADD231PD(ZMM( 8), ZMM(0), MEM_1TO8(RBX,-15*8))
    VFMADD231PD(ZMM( 9), ZMM(0), MEM_1TO8(RBX,-14*8))
    VFMADD231PD(ZMM(10), ZMM(0), MEM_1TO8(RBX,-13*8))
    VFMADD231PD(ZMM(11), ZMM(0), MEM_1TO8(RBX,-12*8))
    VFMADD231PD(ZMM(12), ZMM(0), MEM_1TO8(RBX,-11*8))
    VFMADD231PD(ZMM(13), ZMM(0), MEM_1TO8(RBX,-10*8))
    VFMADD231PD(ZMM(14), ZMM(0), MEM_1TO8(RBX, -9*8))
    VFMADD231PD(ZMM(15), ZMM(0), MEM_1TO8(RBX, -8*8))
    VFMADD231PD(ZMM(16), ZMM(0), MEM_1TO8(RBX, -7*8))
    VFMADD231PD(ZMM(17), ZMM(0), MEM_1TO8(RBX, -6*8))
    VFMADD231PD(ZMM(18), ZMM(0), MEM_1TO8(RBX, -5*8))
    VFMADD231PD(ZMM(19), ZMM(0), MEM_1TO8(RBX, -4*8))
    VFMADD231PD(ZMM(20), ZMM(0), MEM_1TO8(RBX, -3*8))
    VFMADD231PD(ZMM(21), ZMM(0), MEM_1TO8(RBX, -2*8))
    VFMADD231PD(ZMM(22), ZMM(0), MEM_1TO8(RBX, -1*8))
    VFMADD231PD(ZMM(23), ZMM(0), MEM_1TO8(RBX,  0*8))
    VFMADD231PD(ZMM(24), ZMM(0), MEM_1TO8(RBX,  1*8))
    VFMADD231PD(ZMM(25), ZMM(0), MEM_1TO8(RBX,  2*8))
    VFMADD231PD(ZMM(26), ZMM(0), MEM_1TO8(RBX,  3*8))
    VFMADD231PD(ZMM(27), ZMM(0), MEM_1TO8(RBX,  4*8))
    VFMADD231PD(ZMM(28), ZMM(0), MEM_1TO8(RBX,  5*8))
    VFMADD231PD(ZMM(29), ZMM(0), MEM_1TO8(RBX,  6*8))
    VFMADD231PD(ZMM(30), ZMM(0), MEM_1TO8(RBX,  7*8))
    VFMADD231PD(ZMM(31), ZMM(0), MEM_1TO8(RBX,  8*8))

    VMOVAPD(ZMM(0), ZMM(1))
    ADD(RAX, IMM(8*8))
    ADD(RBX, IMM(24*8))

    TEST(RSI, RSI)
    JNZ(.DMAINLOOP)

#elif UNROLL_X4

    MOV(RDI, RSI)
    SAR(RSI, IMM(2)) // k/4
    AND(RDI, IMM(3)) // k%4
    JNZ(.DEXTRALOOP)

    LABEL(.DMAINLOOP)

    MOV(RDI, IMM(24*8))
    LEA(RDX, MEM(RDI,RDI,2))

    ALIGN32
    LABEL(.DLOOPKITER)

#if PREFETCH_A
    PREFETCH(0, MEM(RAX,A_PREFETCH_DIST*8*8))
#endif
    VMOVAPD(ZMM(1), MEM(RAX))

    VFMADD231PD(ZMM( 8), ZMM(0), MEM_1TO8(RBX,-15*8))
    VFMADD231PD(ZMM( 9), ZMM(0), MEM_1TO8(RBX,-14*8))
    VFMADD231PD(ZMM(10), ZMM(0), MEM_1TO8(RBX,-13*8))
    VFMADD231PD(ZMM(11), ZMM(0), MEM_1TO8(RBX,-12*8))
    VFMADD231PD(ZMM(12), ZMM(0), MEM_1TO8(RBX,-11*8))
    VFMADD231PD(ZMM(13), ZMM(0), MEM_1TO8(RBX,-10*8))
    VFMADD231PD(ZMM(14), ZMM(0), MEM_1TO8(RBX, -9*8))
    VFMADD231PD(ZMM(15), ZMM(0), MEM_1TO8(RBX, -8*8))
    VFMADD231PD(ZMM(16), ZMM(0), MEM_1TO8(RBX, -7*8))
    VFMADD231PD(ZMM(17), ZMM(0), MEM_1TO8(RBX, -6*8))
    VFMADD231PD(ZMM(18), ZMM(0), MEM_1TO8(RBX, -5*8))
    VFMADD231PD(ZMM(19), ZMM(0), MEM_1TO8(RBX, -4*8))
    VFMADD231PD(ZMM(20), ZMM(0), MEM_1TO8(RBX, -3*8))
    VFMADD231PD(ZMM(21), ZMM(0), MEM_1TO8(RBX, -2*8))
    VFMADD231PD(ZMM(22), ZMM(0), MEM_1TO8(RBX, -1*8))
    VFMADD231PD(ZMM(23), ZMM(0), MEM_1TO8(RBX,  0*8))
    VFMADD231PD(ZMM(24), ZMM(0), MEM_1TO8(RBX,  1*8))
    VFMADD231PD(ZMM(25), ZMM(0), MEM_1TO8(RBX,  2*8))
    VFMADD231PD(ZMM(26), ZMM(0), MEM_1TO8(RBX,  3*8))
    VFMADD231PD(ZMM(27), ZMM(0), MEM_1TO8(RBX,  4*8))
    VFMADD231PD(ZMM(28), ZMM(0), MEM_1TO8(RBX,  5*8))
    VFMADD231PD(ZMM(29), ZMM(0), MEM_1TO8(RBX,  6*8))
    VFMADD231PD(ZMM(30), ZMM(0), MEM_1TO8(RBX,  7*8))
    VFMADD231PD(ZMM(31), ZMM(0), MEM_1TO8(RBX,  8*8))

#if PREFETCH_A
    PREFETCH(0, MEM(RAX,A_PREFETCH_DIST*8*8+8*8))
#endif
    VMOVAPD(ZMM(0), MEM(RAX,8*8))

    VFMADD231PD(ZMM( 8), ZMM(1), MEM_1TO8(RBX,RDI,1,-15*8))
    VFMADD231PD(ZMM( 9), ZMM(1), MEM_1TO8(RBX,RDI,1,-14*8))
    VFMADD231PD(ZMM(10), ZMM(1), MEM_1TO8(RBX,RDI,1,-13*8))
    VFMADD231PD(ZMM(11), ZMM(1), MEM_1TO8(RBX,RDI,1,-12*8))
    VFMADD231PD(ZMM(12), ZMM(1), MEM_1TO8(RBX,RDI,1,-11*8))
    VFMADD231PD(ZMM(13), ZMM(1), MEM_1TO8(RBX,RDI,1,-10*8))
    VFMADD231PD(ZMM(14), ZMM(1), MEM_1TO8(RBX,RDI,1, -9*8))
    VFMADD231PD(ZMM(15), ZMM(1), MEM_1TO8(RBX,RDI,1, -8*8))
    VFMADD231PD(ZMM(16), ZMM(1), MEM_1TO8(RBX,RDI,1, -7*8))
    VFMADD231PD(ZMM(17), ZMM(1), MEM_1TO8(RBX,RDI,1, -6*8))
    VFMADD231PD(ZMM(18), ZMM(1), MEM_1TO8(RBX,RDI,1, -5*8))
    VFMADD231PD(ZMM(19), ZMM(1), MEM_1TO8(RBX,RDI,1, -4*8))
    VFMADD231PD(ZMM(20), ZMM(1), MEM_1TO8(RBX,RDI,1, -3*8))
    VFMADD231PD(ZMM(21), ZMM(1), MEM_1TO8(RBX,RDI,1, -2*8))
    VFMADD231PD(ZMM(22), ZMM(1), MEM_1TO8(RBX,RDI,1, -1*8))
    VFMADD231PD(ZMM(23), ZMM(1), MEM_1TO8(RBX,RDI,1,  0*8))
    VFMADD231PD(ZMM(24), ZMM(1), MEM_1TO8(RBX,RDI,1,  1*8))
    VFMADD231PD(ZMM(25), ZMM(1), MEM_1TO8(RBX,RDI,1,  2*8))
    VFMADD231PD(ZMM(26), ZMM(1), MEM_1TO8(RBX,RDI,1,  3*8))
    VFMADD231PD(ZMM(27), ZMM(1), MEM_1TO8(RBX,RDI,1,  4*8))
    VFMADD231PD(ZMM(28), ZMM(1), MEM_1TO8(RBX,RDI,1,  5*8))
    VFMADD231PD(ZMM(29), ZMM(1), MEM_1TO8(RBX,RDI,1,  6*8))
    VFMADD231PD(ZMM(30), ZMM(1), MEM_1TO8(RBX,RDI,1,  7*8))
    VFMADD231PD(ZMM(31), ZMM(1), MEM_1TO8(RBX,RDI,1,  8*8))

#if PREFETCH_A
    PREFETCH(0, MEM(RAX,A_PREFETCH_DIST*8*8+2*8*8))
#endif
    VMOVAPD(ZMM(1), MEM(RAX,2*8*8))

    VFMADD231PD(ZMM( 8), ZMM(0), MEM_1TO8(RBX,RDI,2,-15*8))
    VFMADD231PD(ZMM( 9), ZMM(0), MEM_1TO8(RBX,RDI,2,-14*8))
    VFMADD231PD(ZMM(10), ZMM(0), MEM_1TO8(RBX,RDI,2,-13*8))
    VFMADD231PD(ZMM(11), ZMM(0), MEM_1TO8(RBX,RDI,2,-12*8))
    VFMADD231PD(ZMM(12), ZMM(0), MEM_1TO8(RBX,RDI,2,-11*8))
    VFMADD231PD(ZMM(13), ZMM(0), MEM_1TO8(RBX,RDI,2,-10*8))
    VFMADD231PD(ZMM(14), ZMM(0), MEM_1TO8(RBX,RDI,2, -9*8))
    VFMADD231PD(ZMM(15), ZMM(0), MEM_1TO8(RBX,RDI,2, -8*8))
    VFMADD231PD(ZMM(16), ZMM(0), MEM_1TO8(RBX,RDI,2, -7*8))
    VFMADD231PD(ZMM(17), ZMM(0), MEM_1TO8(RBX,RDI,2, -6*8))
    VFMADD231PD(ZMM(18), ZMM(0), MEM_1TO8(RBX,RDI,2, -5*8))
    VFMADD231PD(ZMM(19), ZMM(0), MEM_1TO8(RBX,RDI,2, -4*8))
    VFMADD231PD(ZMM(20), ZMM(0), MEM_1TO8(RBX,RDI,2, -3*8))
    VFMADD231PD(ZMM(21), ZMM(0), MEM_1TO8(RBX,RDI,2, -2*8))
    VFMADD231PD(ZMM(22), ZMM(0), MEM_1TO8(RBX,RDI,2, -1*8))
    VFMADD231PD(ZMM(23), ZMM(0), MEM_1TO8(RBX,RDI,2,  0*8))
    VFMADD231PD(ZMM(24), ZMM(0), MEM_1TO8(RBX,RDI,2,  1*8))
    VFMADD231PD(ZMM(25), ZMM(0), MEM_1TO8(RBX,RDI,2,  2*8))
    VFMADD231PD(ZMM(26), ZMM(0), MEM_1TO8(RBX,RDI,2,  3*8))
    VFMADD231PD(ZMM(27), ZMM(0), MEM_1TO8(RBX,RDI,2,  4*8))
    VFMADD231PD(ZMM(28), ZMM(0), MEM_1TO8(RBX,RDI,2,  5*8))
    VFMADD231PD(ZMM(29), ZMM(0), MEM_1TO8(RBX,RDI,2,  6*8))
    VFMADD231PD(ZMM(30), ZMM(0), MEM_1TO8(RBX,RDI,2,  7*8))
    VFMADD231PD(ZMM(31), ZMM(0), MEM_1TO8(RBX,RDI,2,  8*8))

#if PREFETCH_A
    PREFETCH(0, MEM(RAX,A_PREFETCH_DIST*8*8+3*8*8))
#endif
    VMOVAPD(ZMM(0), MEM(RAX,3*8*8))

    VFMADD231PD(ZMM( 8), ZMM(1), MEM_1TO8(RBX,RDX,1,-15*8))
    VFMADD231PD(ZMM( 9), ZMM(1), MEM_1TO8(RBX,RDX,1,-14*8))
    VFMADD231PD(ZMM(10), ZMM(1), MEM_1TO8(RBX,RDX,1,-13*8))
    VFMADD231PD(ZMM(11), ZMM(1), MEM_1TO8(RBX,RDX,1,-12*8))
    VFMADD231PD(ZMM(12), ZMM(1), MEM_1TO8(RBX,RDX,1,-11*8))
    VFMADD231PD(ZMM(13), ZMM(1), MEM_1TO8(RBX,RDX,1,-10*8))
    VFMADD231PD(ZMM(14), ZMM(1), MEM_1TO8(RBX,RDX,1, -9*8))
    VFMADD231PD(ZMM(15), ZMM(1), MEM_1TO8(RBX,RDX,1, -8*8))
    VFMADD231PD(ZMM(16), ZMM(1), MEM_1TO8(RBX,RDX,1, -7*8))
    VFMADD231PD(ZMM(17), ZMM(1), MEM_1TO8(RBX,RDX,1, -6*8))
    VFMADD231PD(ZMM(18), ZMM(1), MEM_1TO8(RBX,RDX,1, -5*8))
    VFMADD231PD(ZMM(19), ZMM(1), MEM_1TO8(RBX,RDX,1, -4*8))
    VFMADD231PD(ZMM(20), ZMM(1), MEM_1TO8(RBX,RDX,1, -3*8))
    VFMADD231PD(ZMM(21), ZMM(1), MEM_1TO8(RBX,RDX,1, -2*8))
    VFMADD231PD(ZMM(22), ZMM(1), MEM_1TO8(RBX,RDX,1, -1*8))
    VFMADD231PD(ZMM(23), ZMM(1), MEM_1TO8(RBX,RDX,1,  0*8))
    VFMADD231PD(ZMM(24), ZMM(1), MEM_1TO8(RBX,RDX,1,  1*8))
    VFMADD231PD(ZMM(25), ZMM(1), MEM_1TO8(RBX,RDX,1,  2*8))
    VFMADD231PD(ZMM(26), ZMM(1), MEM_1TO8(RBX,RDX,1,  3*8))
    VFMADD231PD(ZMM(27), ZMM(1), MEM_1TO8(RBX,RDX,1,  4*8))
    VFMADD231PD(ZMM(28), ZMM(1), MEM_1TO8(RBX,RDX,1,  5*8))
    VFMADD231PD(ZMM(29), ZMM(1), MEM_1TO8(RBX,RDX,1,  6*8))
    VFMADD231PD(ZMM(30), ZMM(1), MEM_1TO8(RBX,RDX,1,  7*8))
    VFMADD231PD(ZMM(31), ZMM(1), MEM_1TO8(RBX,RDX,1,  8*8))

    ADD(RAX, IMM(4*8*8))
    ADD(RBX, IMM(4*24*8))

    SUB(RSI, IMM(1))
    JNZ(.DLOOPKITER)

    JMP(.DPOSTACCUM)

    LABEL(.DEXTRALOOP)

#if PREFETCH_A
    PREFETCH(0, MEM(RAX,A_PREFETCH_DIST*8*8))
#endif
    VMOVAPD(ZMM(1), MEM(RAX))

    VFMADD231PD(ZMM( 8), ZMM(0), MEM_1TO8(RBX,-15*8))
    VFMADD231PD(ZMM( 9), ZMM(0), MEM_1TO8(RBX,-14*8))
    VFMADD231PD(ZMM(10), ZMM(0), MEM_1TO8(RBX,-13*8))
    VFMADD231PD(ZMM(11), ZMM(0), MEM_1TO8(RBX,-12*8))
    VFMADD231PD(ZMM(12), ZMM(0), MEM_1TO8(RBX,-11*8))
    VFMADD231PD(ZMM(13), ZMM(0), MEM_1TO8(RBX,-10*8))
    VFMADD231PD(ZMM(14), ZMM(0), MEM_1TO8(RBX, -9*8))
    VFMADD231PD(ZMM(15), ZMM(0), MEM_1TO8(RBX, -8*8))
    VFMADD231PD(ZMM(16), ZMM(0), MEM_1TO8(RBX, -7*8))
    VFMADD231PD(ZMM(17), ZMM(0), MEM_1TO8(RBX, -6*8))
    VFMADD231PD(ZMM(18), ZMM(0), MEM_1TO8(RBX, -5*8))
    VFMADD231PD(ZMM(19), ZMM(0), MEM_1TO8(RBX, -4*8))
    VFMADD231PD(ZMM(20), ZMM(0), MEM_1TO8(RBX, -3*8))
    VFMADD231PD(ZMM(21), ZMM(0), MEM_1TO8(RBX, -2*8))
    VFMADD231PD(ZMM(22), ZMM(0), MEM_1TO8(RBX, -1*8))
    VFMADD231PD(ZMM(23), ZMM(0), MEM_1TO8(RBX,  0*8))
    VFMADD231PD(ZMM(24), ZMM(0), MEM_1TO8(RBX,  1*8))
    VFMADD231PD(ZMM(25), ZMM(0), MEM_1TO8(RBX,  2*8))
    VFMADD231PD(ZMM(26), ZMM(0), MEM_1TO8(RBX,  3*8))
    VFMADD231PD(ZMM(27), ZMM(0), MEM_1TO8(RBX,  4*8))
    VFMADD231PD(ZMM(28), ZMM(0), MEM_1TO8(RBX,  5*8))
    VFMADD231PD(ZMM(29), ZMM(0), MEM_1TO8(RBX,  6*8))
    VFMADD231PD(ZMM(30), ZMM(0), MEM_1TO8(RBX,  7*8))
    VFMADD231PD(ZMM(31), ZMM(0), MEM_1TO8(RBX,  8*8))

    VMOVAPD(ZMM(0), ZMM(1))
    ADD(RAX, IMM(8*8))
    ADD(RBX, IMM(24*8))

    SUB(RDI, IMM(1))
    JNZ(.DEXTRALOOP)

    TEST(RSI, RSI)
    JNZ(.DMAINLOOP)

#endif

    LABEL(.DPOSTACCUM)

    MOV(RAX, VAR(alpha))
    MOV(RBX, VAR(beta))
    VBROADCASTSD(ZMM(0), MEM(RAX))
    VBROADCASTSD(ZMM(1), MEM(RBX))

    VMULPD(ZMM( 8), ZMM( 8), ZMM(0))
    VMULPD(ZMM( 9), ZMM( 9), ZMM(0))
    VMULPD(ZMM(10), ZMM(10), ZMM(0))
    VMULPD(ZMM(11), ZMM(11), ZMM(0))
    VMULPD(ZMM(12), ZMM(12), ZMM(0))
    VMULPD(ZMM(13), ZMM(13), ZMM(0))
    VMULPD(ZMM(14), ZMM(14), ZMM(0))
    VMULPD(ZMM(15), ZMM(15), ZMM(0))
    VMULPD(ZMM(16), ZMM(16), ZMM(0))
    VMULPD(ZMM(17), ZMM(17), ZMM(0))
    VMULPD(ZMM(18), ZMM(18), ZMM(0))
    VMULPD(ZMM(19), ZMM(19), ZMM(0))
    VMULPD(ZMM(20), ZMM(20), ZMM(0))
    VMULPD(ZMM(21), ZMM(21), ZMM(0))
    VMULPD(ZMM(22), ZMM(22), ZMM(0))
    VMULPD(ZMM(23), ZMM(23), ZMM(0))
    VMULPD(ZMM(24), ZMM(24), ZMM(0))
    VMULPD(ZMM(25), ZMM(25), ZMM(0))
    VMULPD(ZMM(26), ZMM(26), ZMM(0))
    VMULPD(ZMM(27), ZMM(27), ZMM(0))
    VMULPD(ZMM(28), ZMM(28), ZMM(0))
    VMULPD(ZMM(29), ZMM(29), ZMM(0))
    VMULPD(ZMM(30), ZMM(30), ZMM(0))
    VMULPD(ZMM(31), ZMM(31), ZMM(0))

    MOV(RDI, VAR(rs_c))
    SUB(RDI, IMM(1))
    JNZ(.DGENSTORED)

    LABEL(.COLSTORED)

    MOV(RSI, VAR(cs_c))
    MOV(R8, MEM(RBX))
    LEA(RSI, MEM(,RSI,8))    // cs_c in bytes
    LEA(RDX, MEM(RSI,RSI,2)) // cs_c*3
    LEA(RDI, MEM(RSI,RSI,4)) // cs_c*5
    LEA(R13, MEM(RDX,RSI,4)) // cs_c*7
    SAL1(R8) // shift out the sign bit to check for +/- zero
    JZ(.DCOLSTORBZ)

    VFMADD231PD(ZMM( 8), ZMM(1), MEM(RCX))
    VFMADD231PD(ZMM( 9), ZMM(1), MEM(RCX,RSI,1))
    VFMADD231PD(ZMM(10), ZMM(1), MEM(RCX,RSI,2))
    VFMADD231PD(ZMM(11), ZMM(1), MEM(RCX,RDX,1))
    VFMADD231PD(ZMM(12), ZMM(1), MEM(RCX,RSI,4))
    VFMADD231PD(ZMM(13), ZMM(1), MEM(RCX,RDI,1))
    VFMADD231PD(ZMM(14), ZMM(1), MEM(RCX,RDX,2))
    VFMADD231PD(ZMM(15), ZMM(1), MEM(RCX,R13,1))
    VMOVUPD(MEM(RCX)      , ZMM( 8))
    VMOVUPD(MEM(RCX,RSI,1), ZMM( 9))
    VMOVUPD(MEM(RCX,RSI,2), ZMM(10))
    VMOVUPD(MEM(RCX,RDX,1), ZMM(11))
    VMOVUPD(MEM(RCX,RSI,4), ZMM(12))
    VMOVUPD(MEM(RCX,RDI,1), ZMM(13))
    VMOVUPD(MEM(RCX,RDX,2), ZMM(14))
    VMOVUPD(MEM(RCX,R13,1), ZMM(15))

    LEA(RCX, MEM(RCX,RSI,8))

    VFMADD231PD(ZMM(16), ZMM(1), MEM(RCX))
    VFMADD231PD(ZMM(17), ZMM(1), MEM(RCX,RSI,1))
    VFMADD231PD(ZMM(18), ZMM(1), MEM(RCX,RSI,2))
    VFMADD231PD(ZMM(19), ZMM(1), MEM(RCX,RDX,1))
    VFMADD231PD(ZMM(20), ZMM(1), MEM(RCX,RSI,4))
    VFMADD231PD(ZMM(21), ZMM(1), MEM(RCX,RDI,1))
    VFMADD231PD(ZMM(22), ZMM(1), MEM(RCX,RDX,2))
    VFMADD231PD(ZMM(23), ZMM(1), MEM(RCX,R13,1))
    VMOVUPD(MEM(RCX)      , ZMM(16))
    VMOVUPD(MEM(RCX,RSI,1), ZMM(17))
    VMOVUPD(MEM(RCX,RSI,2), ZMM(18))
    VMOVUPD(MEM(RCX,RDX,1), ZMM(19))
    VMOVUPD(MEM(RCX,RSI,4), ZMM(20))
    VMOVUPD(MEM(RCX,RDI,1), ZMM(21))
    VMOVUPD(MEM(RCX,RDX,2), ZMM(22))
    VMOVUPD(MEM(RCX,R13,1), ZMM(23))

    LEA(RCX, MEM(RCX,RSI,8))

    VFMADD231PD(ZMM(24), ZMM(1), MEM(RCX))
    VFMADD231PD(ZMM(25), ZMM(1), MEM(RCX,RSI,1))
    VFMADD231PD(ZMM(26), ZMM(1), MEM(RCX,RSI,2))
    VFMADD231PD(ZMM(27), ZMM(1), MEM(RCX,RDX,1))
    VFMADD231PD(ZMM(28), ZMM(1), MEM(RCX,RSI,4))
    VFMADD231PD(ZMM(29), ZMM(1), MEM(RCX,RDI,1))
    VFMADD231PD(ZMM(30), ZMM(1), MEM(RCX,RDX,2))
    VFMADD231PD(ZMM(31), ZMM(1), MEM(RCX,R13,1))
    VMOVUPD(MEM(RCX)      , ZMM(24))
    VMOVUPD(MEM(RCX,RSI,1), ZMM(25))
    VMOVUPD(MEM(RCX,RSI,2), ZMM(26))
    VMOVUPD(MEM(RCX,RDX,1), ZMM(27))
    VMOVUPD(MEM(RCX,RSI,4), ZMM(28))
    VMOVUPD(MEM(RCX,RDI,1), ZMM(29))
    VMOVUPD(MEM(RCX,RDX,2), ZMM(30))
    VMOVUPD(MEM(RCX,R13,1), ZMM(31))

    JMP(.DDONE)

    LABEL(.DCOLSTORBZ)

    VMOVUPD(MEM(RCX)      , ZMM( 8))
    VMOVUPD(MEM(RCX,RSI,1), ZMM( 9))
    VMOVUPD(MEM(RCX,RSI,2), ZMM(10))
    VMOVUPD(MEM(RCX,RDX,1), ZMM(11))
    VMOVUPD(MEM(RCX,RSI,4), ZMM(12))
    VMOVUPD(MEM(RCX,RDI,1), ZMM(13))
    VMOVUPD(MEM(RCX,RDX,2), ZMM(14))
    VMOVUPD(MEM(RCX,R13,1), ZMM(15))

    LEA(RCX, MEM(RCX,RSI,8))

    VMOVUPD(MEM(RCX)      , ZMM(16))
    VMOVUPD(MEM(RCX,RSI,1), ZMM(17))
    VMOVUPD(MEM(RCX,RSI,2), ZMM(18))
    VMOVUPD(MEM(RCX,RDX,1), ZMM(19))
    VMOVUPD(MEM(RCX,RSI,4), ZMM(20))
    VMOVUPD(MEM(RCX,RDI,1), ZMM(21))
    VMOVUPD(MEM(RCX,RDX,2), ZMM(22))
    VMOVUPD(MEM(RCX,R13,1), ZMM(23))

    LEA(RCX, MEM(RCX,RSI,8))

    VMOVUPD(MEM(RCX)      , ZMM(24))
    VMOVUPD(MEM(RCX,RSI,1), ZMM(25))
    VMOVUPD(MEM(RCX,RSI,2), ZMM(26))
    VMOVUPD(MEM(RCX,RDX,1), ZMM(27))
    VMOVUPD(MEM(RCX,RSI,4), ZMM(28))
    VMOVUPD(MEM(RCX,RDI,1), ZMM(29))
    VMOVUPD(MEM(RCX,RDX,2), ZMM(30))
    VMOVUPD(MEM(RCX,R13,1), ZMM(31))

    JMP(.DDONE)

    LABEL(.DGENSTORED)

    MOV(RDI, VAR(cs_c))
    LEA(RDI, MEM(,RDI,8))
    MOV(R8, MEM(RBX))
    MOV(RDX, VAR(rs_c))
    VBROADCASTSS(YMM(5), MEM(RDX))
    //MOV(RAX, 0xCC)
    //MOV(RBX, 0xF0)
    //MOV(RSI, 0xAA)
    //KMOV(K(1), EAX)
    //KMOV(K(2), EBX)
    //KMOV(K(3), ESI)
    //VPSLLD(ZMM(2), ZMM(5), IMM(1))
    //VPSLLD(ZMM(3), ZMM(5), IMM(2))
    //VMOVAPS(ZMM(4) MASK_KZ(3), ZMM(5))
    //VPADDD(ZMM(4) MASK_K(1), ZMM(4), ZMM(2))
    //VPADDD(ZMM(4) MASK_K(2), ZMM(4), ZMM(3))
    MOV(RSI, VAR(offsetPtr))
    VMOVAPS(YMM(5), MEM(RSI))
    VPMULLD(YMM(4), YMM(5), YMM(4))
    MOV(RSI, 0xFF)
    SAL1(R8) // shift out the sign bit to check for +/- zero
    //JZ(.DGENSTORBZ)

    UPDATE_SCATTERED( 8)
    UPDATE_SCATTERED( 9)
    UPDATE_SCATTERED(10)
    UPDATE_SCATTERED(11)
    UPDATE_SCATTERED(12)
    UPDATE_SCATTERED(13)
    UPDATE_SCATTERED(14)
    UPDATE_SCATTERED(15)
    UPDATE_SCATTERED(16)
    UPDATE_SCATTERED(17)
    UPDATE_SCATTERED(18)
    UPDATE_SCATTERED(19)
    UPDATE_SCATTERED(20)
    UPDATE_SCATTERED(21)
    UPDATE_SCATTERED(22)
    UPDATE_SCATTERED(23)
    UPDATE_SCATTERED(24)
    UPDATE_SCATTERED(25)
    UPDATE_SCATTERED(26)
    UPDATE_SCATTERED(27)
    UPDATE_SCATTERED(28)
    UPDATE_SCATTERED(29)
    UPDATE_SCATTERED(30)
    UPDATE_SCATTERED(31)

    JMP(.DDONE)

    LABEL(.DGENSTORBZ)

    UPDATE_SCATTERED_BZ( 8)
    UPDATE_SCATTERED_BZ( 9)
    UPDATE_SCATTERED_BZ(10)
    UPDATE_SCATTERED_BZ(11)
    UPDATE_SCATTERED_BZ(12)
    UPDATE_SCATTERED_BZ(13)
    UPDATE_SCATTERED_BZ(14)
    UPDATE_SCATTERED_BZ(15)
    UPDATE_SCATTERED_BZ(16)
    UPDATE_SCATTERED_BZ(17)
    UPDATE_SCATTERED_BZ(18)
    UPDATE_SCATTERED_BZ(19)
    UPDATE_SCATTERED_BZ(20)
    UPDATE_SCATTERED_BZ(21)
    UPDATE_SCATTERED_BZ(22)
    UPDATE_SCATTERED_BZ(23)
    UPDATE_SCATTERED_BZ(24)
    UPDATE_SCATTERED_BZ(25)
    UPDATE_SCATTERED_BZ(26)
    UPDATE_SCATTERED_BZ(27)
    UPDATE_SCATTERED_BZ(28)
    UPDATE_SCATTERED_BZ(29)
    UPDATE_SCATTERED_BZ(30)
    UPDATE_SCATTERED_BZ(31)

    LABEL(.DDONE)

	: // output operands (none)
    : // input operands
      [k]         "m" (k64),
      [a]         "m" (a),
      [b]         "m" (b),
      [alpha]     "m" (alpha),
      [beta]      "m" (beta),
      [c]         "m" (c),
      [rs_c]      "m" (rs_c),
      [cs_c]      "m" (cs_c),
      //[a_next]    "m" (a_next),
      //[b_next]    "m" (b_next),
      [offsetPtr] "m" (offsetPtr)
    : // register clobber list
      "rax", "rbx", "rcx", "rdx", "rdi", "rsi", "r8", "r9", "r10", "r11", "r12",
      "r13", "r14", "r15", "zmm0", "zmm1", "zmm2", "zmm3", "zmm4", "zmm5",
      "zmm6", "zmm7", "zmm8", "zmm9", "zmm10", "zmm11", "zmm12", "zmm13",
      "zmm14", "zmm15", "zmm16", "zmm17", "zmm18", "zmm19", "zmm20", "zmm21",
      "zmm22", "zmm23", "zmm24", "zmm25", "zmm26", "zmm27", "zmm28", "zmm29",
      "zmm30", "zmm31", "memory"
	);
}
