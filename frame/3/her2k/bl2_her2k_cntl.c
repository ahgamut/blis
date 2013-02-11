/*

   BLIS    
   An object-based framework for developing high-performance BLAS-like
   libraries.

   Copyright (C) 2013, The University of Texas

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    - Neither the name of The University of Texas nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "blis2.h"

extern scalm_t*   scalm_cntl;
extern herk_t*    herk_cntl_bp_ke;

her2k_t*          her2k_cntl;

her2k_t*          her2k_cntl_bp_ke;
her2k_t*          her2k_cntl_op_bp;
her2k_t*          her2k_cntl_mm_op;
her2k_t*          her2k_cntl_vl_mm;

packm_t*          her2k_packa_cntl;
packm_t*          her2k_packb_cntl;
packm_t*          her2k_packc_cntl;
unpackm_t*        her2k_unpackc_cntl;

blksz_t*          her2k_mc;
blksz_t*          her2k_nc;
blksz_t*          her2k_kc;
blksz_t*          her2k_mr;
blksz_t*          her2k_nr;
blksz_t*          her2k_kr;
blksz_t*          her2k_ni;

// Cache blocksizes.

#define BLIS_HER2K_KC_S BLIS_DEFAULT_KC_S
#define BLIS_HER2K_KC_D BLIS_DEFAULT_KC_D
#define BLIS_HER2K_KC_C BLIS_DEFAULT_KC_C
#define BLIS_HER2K_KC_Z BLIS_DEFAULT_KC_Z

#define BLIS_HER2K_MC_S BLIS_DEFAULT_MC_S
#define BLIS_HER2K_MC_D BLIS_DEFAULT_MC_D
#define BLIS_HER2K_MC_C BLIS_DEFAULT_MC_C
#define BLIS_HER2K_MC_Z BLIS_DEFAULT_MC_Z

#define BLIS_HER2K_NC_S BLIS_DEFAULT_NC_S
#define BLIS_HER2K_NC_D BLIS_DEFAULT_NC_D
#define BLIS_HER2K_NC_C BLIS_DEFAULT_NC_C
#define BLIS_HER2K_NC_Z BLIS_DEFAULT_NC_Z

// Register blocking 

#define BLIS_HER2K_KR_S BLIS_DEFAULT_KR_S
#define BLIS_HER2K_KR_D BLIS_DEFAULT_KR_D
#define BLIS_HER2K_KR_C BLIS_DEFAULT_KR_C
#define BLIS_HER2K_KR_Z BLIS_DEFAULT_KR_Z

#define BLIS_HER2K_MR_S BLIS_DEFAULT_MR_S
#define BLIS_HER2K_MR_D BLIS_DEFAULT_MR_D
#define BLIS_HER2K_MR_C BLIS_DEFAULT_MR_C
#define BLIS_HER2K_MR_Z BLIS_DEFAULT_MR_Z

#define BLIS_HER2K_NR_S BLIS_DEFAULT_NR_S
#define BLIS_HER2K_NR_D BLIS_DEFAULT_NR_D
#define BLIS_HER2K_NR_C BLIS_DEFAULT_NR_C
#define BLIS_HER2K_NR_Z BLIS_DEFAULT_NR_Z

// Incremental pack blocking

#define BLIS_HER2K_NI_S BLIS_DEFAULT_NI_S
#define BLIS_HER2K_NI_D BLIS_DEFAULT_NI_D
#define BLIS_HER2K_NI_C BLIS_DEFAULT_NI_C
#define BLIS_HER2K_NI_Z BLIS_DEFAULT_NI_Z


void bl2_her2k_cntl_init()
{
	// Create blocksize objects for each dimension.
	her2k_mc = bl2_blksz_obj_create( BLIS_HER2K_MC_S,
	                                 BLIS_HER2K_MC_D,
	                                 BLIS_HER2K_MC_C,
	                                 BLIS_HER2K_MC_Z );

	her2k_nc = bl2_blksz_obj_create( BLIS_HER2K_NC_S,
	                                 BLIS_HER2K_NC_D,
	                                 BLIS_HER2K_NC_C,
	                                 BLIS_HER2K_NC_Z );

	her2k_kc = bl2_blksz_obj_create( BLIS_HER2K_KC_S,
	                                 BLIS_HER2K_KC_D,
	                                 BLIS_HER2K_KC_C,
	                                 BLIS_HER2K_KC_Z );

	her2k_mr = bl2_blksz_obj_create( BLIS_HER2K_MR_S,
	                                 BLIS_HER2K_MR_D,
	                                 BLIS_HER2K_MR_C,
	                                 BLIS_HER2K_MR_Z );

	her2k_nr = bl2_blksz_obj_create( BLIS_HER2K_NR_S,
	                                 BLIS_HER2K_NR_D,
	                                 BLIS_HER2K_NR_C,
	                                 BLIS_HER2K_NR_Z );

	her2k_kr = bl2_blksz_obj_create( BLIS_HER2K_KR_S,
	                                 BLIS_HER2K_KR_D,
	                                 BLIS_HER2K_KR_C,
	                                 BLIS_HER2K_KR_Z );

	her2k_ni = bl2_blksz_obj_create( BLIS_HER2K_NI_S,
	                                 BLIS_HER2K_NI_D,
	                                 BLIS_HER2K_NI_C,
	                                 BLIS_HER2K_NI_Z );


	// Create control tree objects for packm operations on a, b, and c.
	her2k_packa_cntl
	=
	bl2_packm_cntl_obj_create( BLIS_BLOCKED,
	                           BLIS_VARIANT2,
	                           her2k_mr,
	                           her2k_kr,
	                           FALSE, // do NOT scale by alpha
	                           FALSE, // already dense; densify not necessary
	                           FALSE, // do NOT invert diagonal
	                           FALSE, // reverse iteration if upper?
	                           FALSE, // reverse iteration if lower?
	                           BLIS_PACKED_ROW_PANELS );

	her2k_packb_cntl
	=
	bl2_packm_cntl_obj_create( BLIS_BLOCKED,
	                           BLIS_VARIANT2,
	                           her2k_kr,
	                           her2k_nr,
	                           FALSE, // do NOT scale by alpha
	                           FALSE, // already dense; densify not necessary
	                           FALSE, // do NOT invert diagonal
	                           FALSE, // reverse iteration if upper?
	                           FALSE, // reverse iteration if lower?
	                           BLIS_PACKED_COL_PANELS );

	her2k_packc_cntl
	=
	bl2_packm_cntl_obj_create( BLIS_UNBLOCKED,
	                           BLIS_VARIANT1,
	                           her2k_mr,
	                           her2k_nr,
	                           FALSE, // do NOT scale by beta
	                           FALSE, // already dense; densify not necessary
	                           FALSE, // do NOT invert diagonal
	                           FALSE, // reverse iteration if upper?
	                           FALSE, // reverse iteration if lower?
	                           BLIS_PACKED_COLUMNS );

	her2k_unpackc_cntl
	=
	bl2_unpackm_cntl_obj_create( BLIS_UNBLOCKED,
	                             BLIS_VARIANT1,
	                             NULL ); // no blocksize needed

	// Create control tree object for lowest-level block-panel kernel.
	her2k_cntl_bp_ke
	=
	bl2_her2k_cntl_obj_create( BLIS_UNB_OPT,
	                           BLIS_VARIANT2,
	                           NULL, NULL, NULL, NULL, NULL,
	                           NULL, NULL, NULL, NULL );

	// Create control tree object for outer panel (to block-panel)
	// problem, packing a and b only.
	her2k_cntl_op_bp
	=
	bl2_her2k_cntl_obj_create( BLIS_BLOCKED,
	                           //BLIS_VARIANT4,  // var1 with incremental pack in iter 0
	                           BLIS_VARIANT1,
	                           her2k_mc,
	                           her2k_ni,
	                           NULL,
	                           her2k_packa_cntl,
	                           her2k_packb_cntl,
	                           NULL,
	                           her2k_cntl_bp_ke,
	                           herk_cntl_bp_ke,
	                           NULL );

	// Create control tree object for general problem via multiple
	// rank-k (outer panel) updates, packing a and b only.
	her2k_cntl_mm_op
	=
	bl2_her2k_cntl_obj_create( BLIS_BLOCKED,
	                           BLIS_VARIANT3,
	                           her2k_kc,
	                           NULL,
	                           NULL,
	                           NULL, 
	                           NULL,
	                           NULL,
	                           her2k_cntl_op_bp,
	                           NULL,
	                           NULL );

	// Create control tree object for very large problem via multiple
	// general problems, packing a and b only.
	her2k_cntl_vl_mm
	=
	bl2_her2k_cntl_obj_create( BLIS_BLOCKED,
	                           BLIS_VARIANT2,
	                           her2k_nc,
	                           NULL,
	                           NULL,
	                           NULL, 
	                           NULL,
	                           NULL,
	                           her2k_cntl_mm_op,
	                           NULL,
	                           NULL );

	// Alias the "master" her2k control tree to a shorter name.
	her2k_cntl = her2k_cntl_mm_op;

}

void bl2_her2k_cntl_finalize()
{
	bl2_blksz_obj_free( her2k_mc );
	bl2_blksz_obj_free( her2k_nc );
	bl2_blksz_obj_free( her2k_kc );
	bl2_blksz_obj_free( her2k_mr );
	bl2_blksz_obj_free( her2k_nr );
	bl2_blksz_obj_free( her2k_kr );
	bl2_blksz_obj_free( her2k_ni );

	bl2_cntl_obj_free( her2k_packa_cntl );
	bl2_cntl_obj_free( her2k_packb_cntl );
	bl2_cntl_obj_free( her2k_packc_cntl );
	bl2_cntl_obj_free( her2k_unpackc_cntl );

	bl2_cntl_obj_free( her2k_cntl_bp_ke );
	bl2_cntl_obj_free( her2k_cntl_op_bp );
	bl2_cntl_obj_free( her2k_cntl_mm_op );
	bl2_cntl_obj_free( her2k_cntl_vl_mm );
}

her2k_t* bl2_her2k_cntl_obj_create( impl_t     impl_type,
                                    varnum_t   var_num,
                                    blksz_t*   b,
                                    blksz_t*   b_aux,
                                    scalm_t*   sub_scalm,
                                    packm_t*   sub_packm_a,
                                    packm_t*   sub_packm_b,
                                    packm_t*   sub_packm_c,
                                    her2k_t*   sub_her2k,
                                    herk_t*    sub_herk,
                                    unpackm_t* sub_unpackm_c )
{
	her2k_t* cntl;

	cntl = ( her2k_t* ) bl2_malloc( sizeof(her2k_t) );	

	cntl->impl_type     = impl_type;
	cntl->var_num       = var_num;
	cntl->b             = b;
	cntl->b_aux         = b_aux;
	cntl->sub_scalm     = sub_scalm;
	cntl->sub_packm_a   = sub_packm_a;
	cntl->sub_packm_b   = sub_packm_b;
	cntl->sub_packm_c   = sub_packm_c;
	cntl->sub_her2k     = sub_her2k;
	cntl->sub_herk      = sub_herk;
	cntl->sub_unpackm_c = sub_unpackm_c;

	return cntl;
}

