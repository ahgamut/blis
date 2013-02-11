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

#define FUNCPTR_T trmv_fp

typedef void (*FUNCPTR_T)(
                           uplo_t  uplo,
                           trans_t trans,
                           diag_t  diag,
                           dim_t   m,
                           void*   alpha,
                           void*   a, inc_t rs_a, inc_t cs_a,
                           void*   x, inc_t incx
                         );

// If some mixed datatype functions will not be compiled, we initialize
// the corresponding elements of the function array to NULL.
#ifdef BLIS_ENABLE_MIXED_PRECISION_SUPPORT
static FUNCPTR_T GENARRAY2_ALL(ftypes,trmv_unb_var1);
#else
#ifdef BLIS_ENABLE_MIXED_DOMAIN_SUPPORT
static FUNCPTR_T GENARRAY2_EXT(ftypes,trmv_unb_var1);
#else
static FUNCPTR_T GENARRAY2_MIN(ftypes,trmv_unb_var1);
#endif
#endif


void bl2_trmv_unb_var1( obj_t*  alpha,
                        obj_t*  a,
                        obj_t*  x,
                        trmv_t* cntl )
{
	num_t     dt_a      = bl2_obj_datatype( *a );
	num_t     dt_x      = bl2_obj_datatype( *x );

	uplo_t    uplo      = bl2_obj_uplo( *a );
	trans_t   trans     = bl2_obj_conjtrans_status( *a );
	diag_t    diag      = bl2_obj_diag( *a );

	dim_t     m         = bl2_obj_length( *a );

	void*     buf_a     = bl2_obj_buffer_at_off( *a );
	inc_t     rs_a      = bl2_obj_row_stride( *a );
	inc_t     cs_a      = bl2_obj_col_stride( *a );

	void*     buf_x     = bl2_obj_buffer_at_off( *x );
	inc_t     incx      = bl2_obj_vector_inc( *x );

	num_t     dt_alpha;
	void*     buf_alpha;

	FUNCPTR_T f;

	// The datatype of alpha MUST be the type union of a and x. This is to
	// prevent any unnecessary loss of information during computation.
	dt_alpha  = bl2_datatype_union( dt_a, dt_x );
	buf_alpha = bl2_obj_scalar_buffer( dt_alpha, *alpha );

	// Index into the type combination array to extract the correct
	// function pointer.
	f = ftypes[dt_a][dt_x];

	// Invoke the function.
	f( uplo,
	   trans,
	   diag,
	   m,
	   buf_alpha,
	   buf_a, rs_a, cs_a,
	   buf_x, incx );
}


#undef  GENTFUNC2U
#define GENTFUNC2U( ctype_a, ctype_x, ctype_ax, cha, chx, chax, varname, kername ) \
\
void PASTEMAC2(cha,chx,varname)( \
                                 uplo_t  uplo, \
                                 trans_t trans, \
                                 diag_t  diag, \
                                 dim_t   m, \
                                 void*   alpha, \
                                 void*   a, inc_t rs_a, inc_t cs_a, \
                                 void*   x, inc_t incx  \
                               ) \
{ \
	ctype_ax* alpha_cast = alpha; \
	ctype_a*  a_cast     = a; \
	ctype_x*  x_cast     = x; \
	ctype_a*  a10t; \
	ctype_a*  alpha11; \
	ctype_a*  a12t; \
	ctype_x*  x0; \
	ctype_x*  chi1; \
	ctype_x*  x2; \
	ctype_ax  alpha_alpha11_conj; \
	ctype_ax  rho; \
	dim_t     iter, i; \
	dim_t     n_ahead; \
	inc_t     rs_at, cs_at; \
	uplo_t    uplo_trans; \
	conj_t    conja; \
\
	if ( bl2_zero_dim1( m ) ) return; \
\
	if      ( bl2_does_notrans( trans ) ) \
	{ \
		rs_at = rs_a; \
		cs_at = cs_a; \
		uplo_trans = uplo; \
	} \
	else /* if ( bl2_does_trans( trans ) ) */ \
	{ \
		rs_at = cs_a; \
		cs_at = rs_a; \
		uplo_trans = bl2_uplo_toggled( uplo ); \
	} \
\
	conja = bl2_extract_conj( trans ); \
\
	/* We reduce all of the possible cases down to just lower/upper. */ \
	if      ( bl2_is_upper( uplo_trans ) ) \
	{ \
		for ( iter = 0; iter < m; ++iter ) \
		{ \
			i        = iter; \
			n_ahead  = m - iter - 1; \
			alpha11  = a_cast + (i  )*rs_at + (i  )*cs_at; \
			a12t     = a_cast + (i  )*rs_at + (i+1)*cs_at; \
			chi1     = x_cast + (i  )*incx; \
			x2       = x_cast + (i+1)*incx; \
\
			/* chi1 = alpha * alpha11 * chi1; */ \
			PASTEMAC2(chax,chax,copys)( *alpha_cast, alpha_alpha11_conj ); \
			if ( bl2_is_nonunit_diag( diag ) ) \
				PASTEMAC2(cha,chax,scalcjs)( conja, *alpha11, alpha_alpha11_conj ); \
			PASTEMAC2(chax,chx,scals)( alpha_alpha11_conj, *chi1 ); \
\
			/* chi1 = chi1 + alpha * a12t * x2; */ \
			PASTEMAC3(cha,chx,chax,dotv)( conja, \
			                              BLIS_NO_CONJUGATE, \
			                              n_ahead, \
			                              a12t, cs_at, \
			                              x2,   incx, \
			                              &rho ); \
			PASTEMAC3(chax,chax,chx,axpys)( *alpha_cast, rho, *chi1 ); \
		} \
	} \
	else /* if ( bl2_is_lower( uplo_trans ) ) */ \
	{ \
		for ( iter = 0; iter < m; ++iter ) \
		{ \
			i        = m - iter - 1; \
			n_ahead  = i; \
			alpha11  = a_cast + (i  )*rs_at + (i  )*cs_at; \
			a10t     = a_cast + (i  )*rs_at + (0  )*cs_at; \
			chi1     = x_cast + (i  )*incx; \
			x0       = x_cast + (0  )*incx; \
\
			/* chi1 = alpha * alpha11 * chi1; */ \
			PASTEMAC2(chax,chax,copys)( *alpha_cast, alpha_alpha11_conj ); \
			if ( bl2_is_nonunit_diag( diag ) ) \
				PASTEMAC2(cha,chax,scalcjs)( conja, *alpha11, alpha_alpha11_conj ); \
			PASTEMAC2(chax,chx,scals)( alpha_alpha11_conj, *chi1 ); \
\
			/* chi1 = chi1 + alpha * a10t * x0; */ \
			PASTEMAC3(cha,chx,chax,kername)( conja, \
			                                 BLIS_NO_CONJUGATE, \
			                                 n_ahead, \
			                                 a10t, cs_at, \
			                                 x0,   incx, \
			                                 &rho ); \
			PASTEMAC3(chax,chax,chx,axpys)( *alpha_cast, rho, *chi1 ); \
		} \
	} \
}

// Define the basic set of functions unconditionally, and then also some
// mixed datatype functions if requested.
INSERT_GENTFUNC2U_BASIC( trmv_unb_var1, DOTV_KERNEL )

#ifdef BLIS_ENABLE_MIXED_DOMAIN_SUPPORT
INSERT_GENTFUNC2U_MIX_D( trmv_unb_var1, DOTV_KERNEL )
#endif

#ifdef BLIS_ENABLE_MIXED_PRECISION_SUPPORT
INSERT_GENTFUNC2U_MIX_P( trmv_unb_var1, DOTV_KERNEL )
#endif

