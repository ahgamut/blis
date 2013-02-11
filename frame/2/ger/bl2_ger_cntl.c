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

extern packm_t*   packm_cntl_noscale;
extern packv_t*   packv_cntl;
extern unpackm_t* unpackm_cntl;

static blksz_t*   ger_mc;
static blksz_t*   ger_nc;

ger_t*            ger_cntl_bs_ke_row;
ger_t*            ger_cntl_bs_ke_col;

ger_t*            ger_cntl_rp_bs_row;
ger_t*            ger_cntl_rp_bs_col;

ger_t*            ger_cntl_cp_bs_row;
ger_t*            ger_cntl_cp_bs_col;

ger_t*            ger_cntl_ge_row;
ger_t*            ger_cntl_ge_col;

// Cache blocksizes.

#define BLIS_GER_MC_S BLIS_DEFAULT_L2_MC_S
#define BLIS_GER_MC_D BLIS_DEFAULT_L2_MC_D
#define BLIS_GER_MC_C BLIS_DEFAULT_L2_MC_C
#define BLIS_GER_MC_Z BLIS_DEFAULT_L2_MC_Z

#define BLIS_GER_NC_S BLIS_DEFAULT_L2_NC_S
#define BLIS_GER_NC_D BLIS_DEFAULT_L2_NC_D
#define BLIS_GER_NC_C BLIS_DEFAULT_L2_NC_C
#define BLIS_GER_NC_Z BLIS_DEFAULT_L2_NC_Z



void bl2_ger_cntl_init()
{
	// Create blocksize objects.
	ger_mc = bl2_blksz_obj_create( BLIS_GER_MC_S,
	                               BLIS_GER_MC_D,
	                               BLIS_GER_MC_C,
	                               BLIS_GER_MC_Z );

	ger_nc = bl2_blksz_obj_create( BLIS_GER_NC_S,
	                               BLIS_GER_NC_D,
	                               BLIS_GER_NC_C,
	                               BLIS_GER_NC_Z );


	// Create control trees for the lowest-level kernels. These trees induce
	// operations on (persumably) relatively small block-subvector problems.
	ger_cntl_bs_ke_row
	=
	bl2_ger_cntl_obj_create( BLIS_UNBLOCKED,
	                         BLIS_VARIANT1,
	                         NULL, NULL, NULL,
	                         NULL, NULL, NULL );
	ger_cntl_bs_ke_col
	=
	bl2_ger_cntl_obj_create( BLIS_UNBLOCKED,
	                         BLIS_VARIANT2,
	                         NULL, NULL, NULL,
	                         NULL, NULL, NULL );


	// Create control trees for problems with relatively small m dimension
	// (ie: where A is a row panel problem).
	ger_cntl_rp_bs_row
	=
	bl2_ger_cntl_obj_create( BLIS_BLOCKED,
	                         BLIS_VARIANT2,
	                         ger_nc,
	                         NULL,               // x is not partitioned in var2
	                         packv_cntl,         // pack y1 (if needed)
	                         packm_cntl_noscale, // pack A1 (if needed)
	                         ger_cntl_bs_ke_row,
	                         unpackm_cntl );     // unpack A1 (if packed)
	ger_cntl_rp_bs_col
	=
	bl2_ger_cntl_obj_create( BLIS_BLOCKED,
	                         BLIS_VARIANT2,
	                         ger_nc,
	                         NULL,               // x is not partitioned in var2
	                         packv_cntl,         // pack y1 (if needed)
	                         packm_cntl_noscale, // pack A1 (if needed)
	                         ger_cntl_bs_ke_col,
	                         unpackm_cntl );     // unpack A1 (if packed)


	// Create control trees for problems with relatively small n dimension
	// (ie: where A is a column panel problem).
	ger_cntl_cp_bs_row
	=
	bl2_ger_cntl_obj_create( BLIS_BLOCKED,
	                         BLIS_VARIANT1,
	                         ger_mc,
	                         packv_cntl,         // pack x1 (if needed)
	                         NULL,               // y is not partitioned in var1
	                         packm_cntl_noscale, // pack A1 (if needed)
	                         ger_cntl_bs_ke_row,
	                         unpackm_cntl );     // unpack A1 (if packed)
	ger_cntl_cp_bs_col
	=
	bl2_ger_cntl_obj_create( BLIS_BLOCKED,
	                         BLIS_VARIANT1,
	                         ger_mc,
	                         packv_cntl,         // pack x1 (if needed)
	                         NULL,               // y is not partitioned in var1
	                         packm_cntl_noscale, // pack A1 (if needed)
	                         ger_cntl_bs_ke_col,
	                         unpackm_cntl );     // unpack A1 (if packed)


	// Create control trees for generally large problems. Here, we choose a
	// variant that partitions subproblems into column panels.
	ger_cntl_ge_row   
	=
	bl2_ger_cntl_obj_create( BLIS_BLOCKED,
	                         BLIS_VARIANT2,
	                         ger_nc,
	                         NULL,               // x is not partitioned in var2
	                         packv_cntl,         // pack y1 (if needed)
	                         NULL,               // do not pack A1
	                         ger_cntl_cp_bs_row,
	                         NULL );             // do not unpack A1
	ger_cntl_ge_col   
	=
	bl2_ger_cntl_obj_create( BLIS_BLOCKED,
	                         BLIS_VARIANT2,
	                         ger_nc,
	                         NULL,               // x is not partitioned in var2
	                         packv_cntl,         // pack y1 (if needed)
	                         NULL,               // do not pack A1
	                         ger_cntl_cp_bs_col,
	                         NULL );             // do not unpack A1
}

void bl2_ger_cntl_finalize()
{
	bl2_cntl_obj_free( ger_mc );
	bl2_cntl_obj_free( ger_nc );

	bl2_cntl_obj_free( ger_cntl_bs_ke_row );
	bl2_cntl_obj_free( ger_cntl_bs_ke_col );

	bl2_cntl_obj_free( ger_cntl_rp_bs_row );
	bl2_cntl_obj_free( ger_cntl_rp_bs_col );

	bl2_cntl_obj_free( ger_cntl_cp_bs_row );
	bl2_cntl_obj_free( ger_cntl_cp_bs_col );

	bl2_cntl_obj_free( ger_cntl_ge_row );
	bl2_cntl_obj_free( ger_cntl_ge_col );
}


ger_t* bl2_ger_cntl_obj_create( impl_t     impl_type,
                                varnum_t   var_num,
                                blksz_t*   b,
                                packv_t*   sub_packv_x,
                                packv_t*   sub_packv_y,
                                packm_t*   sub_packm_a,
                                ger_t*     sub_ger,
                                unpackm_t* sub_unpackm_a )
{
	ger_t* cntl;

	cntl = ( ger_t* ) bl2_malloc( sizeof(ger_t) );	

	cntl->impl_type     = impl_type;
	cntl->var_num       = var_num;
	cntl->b             = b;
	cntl->sub_packv_x   = sub_packv_x;
	cntl->sub_packv_y   = sub_packv_y;
	cntl->sub_packm_a   = sub_packm_a;
	cntl->sub_ger       = sub_ger;
	cntl->sub_unpackm_a = sub_unpackm_a;

	return cntl;
}

void bl2_ger_cntl_obj_init( ger_t*     cntl,
                            impl_t     impl_type,
                            varnum_t   var_num,
                            blksz_t*   b,
                            packv_t*   sub_packv_x,
                            packv_t*   sub_packv_y,
                            packm_t*   sub_packm_a,
                            ger_t*     sub_ger,
                            unpackm_t* sub_unpackm_a )
{
	cntl->impl_type     = impl_type;
	cntl->var_num       = var_num;
	cntl->b             = b;
	cntl->sub_packv_x   = sub_packv_x;
	cntl->sub_packv_y   = sub_packv_y;
	cntl->sub_packm_a   = sub_packm_a;
	cntl->sub_ger       = sub_ger;
	cntl->sub_unpackm_a = sub_unpackm_a;
}


