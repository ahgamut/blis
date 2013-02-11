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

struct unpackv_s
{
	impl_t         impl_type;
	varnum_t       var_num;
};
typedef struct unpackv_s unpackv_t;

#define cntl_sub_unpackv( cntl )      cntl->sub_unpackv
#define cntl_sub_unpackv_x( cntl )    cntl->sub_unpackv_x
#define cntl_sub_unpackv_x1( cntl )   cntl->sub_unpackv_x1
#define cntl_sub_unpackv_y( cntl )    cntl->sub_unpackv_y
#define cntl_sub_unpackv_y1( cntl )   cntl->sub_unpackv_y1

void     bl2_unpackv_cntl_init( void );
void     bl2_unpackv_cntl_finalize( void );
unpackv_t* bl2_unpackv_cntl_obj_create( impl_t     impl_type,
                                        varnum_t   var_num );
void bl2_unpackv_cntl_obj_init( unpackv_t* cntl,
                                impl_t     impl_type,
                                varnum_t   var_num );

