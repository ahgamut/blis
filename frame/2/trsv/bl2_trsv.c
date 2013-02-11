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

extern trsv_t* trsv_cntl_bs_ke_nrow_tcol;
extern trsv_t* trsv_cntl_bs_ke_ncol_trow;
extern trsv_t* trsv_cntl_ge_nrow_tcol;
extern trsv_t* trsv_cntl_ge_ncol_trow;

void bl2_trsv( obj_t*  alpha,
               obj_t*  a,
               obj_t*  x )
{
	trsv_t* trsv_cntl;
	num_t   dt_targ_a;
	num_t   dt_targ_x;
	bool_t  a_is_contig;
	bool_t  x_is_contig;
	obj_t   alpha_local;
	num_t   dt_alpha;

	// Check parameters.
	if ( bl2_error_checking_is_enabled() )
		bl2_trsv_check( alpha, a, x );


	// Query the target datatypes of each object.
	dt_targ_a = bl2_obj_datatype( *a );
	dt_targ_x = bl2_obj_datatype( *x );

	// Determine whether each operand is stored contiguously.
	a_is_contig = ( bl2_obj_is_row_stored( *a ) ||
	                bl2_obj_is_col_stored( *a ) );
	x_is_contig = ( bl2_obj_vector_inc( *x ) == 1 );


	// Create an object to hold a copy-cast of alpha. Notice that we use
	// the type union of the target datatypes of a and x to prevent any
	// unnecessary loss of information during the computation.
	dt_alpha = bl2_datatype_union( dt_targ_a, dt_targ_x );
	bl2_obj_init_scalar_copy_of( dt_alpha,
	                             BLIS_NO_CONJUGATE,
	                             alpha,
	                             &alpha_local );


	// If all operands are contiguous, we choose a control tree for calling
	// the unblocked implementation directly without any blocking.
	if ( a_is_contig &&
	     x_is_contig )
	{
		if ( bl2_obj_has_notrans( *a ) )
		{
			if ( bl2_obj_is_row_stored( *a ) ) trsv_cntl = trsv_cntl_bs_ke_nrow_tcol;
			else                               trsv_cntl = trsv_cntl_bs_ke_ncol_trow;
		}
		else // if ( bl2_obj_has_trans( *a ) )
		{
			if ( bl2_obj_is_row_stored( *a ) ) trsv_cntl = trsv_cntl_bs_ke_ncol_trow;
			else                               trsv_cntl = trsv_cntl_bs_ke_nrow_tcol;
		}
	}
	else
	{
		// Mark objects with unit stride as already being packed. This prevents
		// unnecessary packing from happening within the blocked algorithm.
		if ( a_is_contig ) bl2_obj_set_pack_schema( BLIS_PACKED_UNSPEC, *a );
		if ( x_is_contig ) bl2_obj_set_pack_schema( BLIS_PACKED_VECTOR, *x );

		// Here, we make a similar choice as above, except that (1) we look
		// at storage tilt, and (2) we choose a tree that performs blocking.
		if ( bl2_obj_has_notrans( *a ) )
		{
			if ( bl2_obj_is_row_tilted( *a ) ) trsv_cntl = trsv_cntl_ge_nrow_tcol;
			else                               trsv_cntl = trsv_cntl_ge_ncol_trow;
		}
		else // if ( bl2_obj_has_trans( *a ) )
		{
			if ( bl2_obj_is_row_tilted( *a ) ) trsv_cntl = trsv_cntl_ge_ncol_trow;
			else                               trsv_cntl = trsv_cntl_ge_nrow_tcol;
		}
	}


	// Invoke the internal back-end with the copy-cast of alpha and the
	// chosen control tree.
	bl2_trsv_int( &alpha_local,
	              a,
	              x,
	              trsv_cntl );
}


//
// Define BLAS-like interfaces with homogeneous-typed operands.
//
#undef  GENTFUNC
#define GENTFUNC( ctype, ch, opname, varname ) \
\
void PASTEMAC(ch,opname)( \
                          uplo_t   uploa, \
                          trans_t  transa, \
                          diag_t   diaga, \
                          dim_t    m, \
                          ctype*   alpha, \
                          ctype*   a, inc_t rs_a, inc_t cs_a, \
                          ctype*   x, inc_t incx \
                        ) \
{ \
	const num_t dt = PASTEMAC(ch,type); \
\
	obj_t       alphao, ao, xo; \
\
	inc_t       rs_x, cs_x; \
\
	rs_x = incx; cs_x = m * incx; \
\
	bl2_obj_create_scalar_with_attached_buffer( dt, alpha, &alphao ); \
\
	bl2_obj_create_with_attached_buffer( dt, m, m, a, rs_a, cs_a, &ao ); \
	bl2_obj_create_with_attached_buffer( dt, m, 1, x, rs_x, cs_x, &xo ); \
\
	bl2_obj_set_uplo( uploa, ao ); \
	bl2_obj_set_conjtrans( transa, ao ); \
	bl2_obj_set_diag( diaga, ao ); \
\
	PASTEMAC0(opname)( &alphao, \
	                   &ao, \
	                   &xo ); \
}

INSERT_GENTFUNC_BASIC( trsv, trsv )


//
// Define BLAS-like interfaces with heterogeneous-typed operands.
//
#undef  GENTFUNC2U
#define GENTFUNC2U( ctype_a, ctype_x, ctype_ax, cha, chx, chax, opname, varname ) \
\
void PASTEMAC2(cha,chx,opname)( \
                                uplo_t    uploa, \
                                trans_t   transa, \
                                diag_t    diaga, \
                                dim_t     m, \
                                ctype_ax* alpha, \
                                ctype_a*  a, inc_t rs_a, inc_t cs_a, \
                                ctype_x*  x, inc_t incx \
                              ) \
{ \
	bl2_check_error_code( BLIS_NOT_YET_IMPLEMENTED ); \
}

INSERT_GENTFUNC2U_BASIC( trsv, trsv )

#ifdef BLIS_ENABLE_MIXED_DOMAIN_SUPPORT
INSERT_GENTFUNC2U_MIX_D( trsv, trsv )
#endif

#ifdef BLIS_ENABLE_MIXED_PRECISION_SUPPORT
INSERT_GENTFUNC2U_MIX_P( trsv, trsv )
#endif

