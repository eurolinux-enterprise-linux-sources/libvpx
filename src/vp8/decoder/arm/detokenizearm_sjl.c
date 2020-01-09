/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license and patent
 *  grant that can be found in the LICENSE file in the root of the source
 *  tree. All contributing project authors may be found in the AUTHORS
 *  file in the root of the source tree.
 */


#include "type_aliases.h"
#include "blockd.h"
#include "onyxd_int.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/mem.h"

#define BR_COUNT 8
#define BOOL_DATA UINT8

#define OCB_X PREV_COEF_CONTEXTS * ENTROPY_NODES
//ALIGN16 UINT16 onyx_coef_bands_x[16] = { 0, 1*OCB_X, 2*OCB_X, 3*OCB_X, 6*OCB_X, 4*OCB_X, 5*OCB_X, 6*OCB_X, 6*OCB_X, 6*OCB_X, 6*OCB_X, 6*OCB_X, 6*OCB_X, 6*OCB_X, 6*OCB_X, 7*OCB_X};
DECLARE_ALIGNED(16, UINT8, vp8_coef_bands_x[16]) = { 0, 1 * OCB_X, 2 * OCB_X, 3 * OCB_X, 6 * OCB_X, 4 * OCB_X, 5 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 7 * OCB_X};

#define EOB_CONTEXT_NODE            0
#define ZERO_CONTEXT_NODE           1
#define ONE_CONTEXT_NODE            2
#define LOW_VAL_CONTEXT_NODE        3
#define TWO_CONTEXT_NODE            4
#define THREE_CONTEXT_NODE          5
#define HIGH_LOW_CONTEXT_NODE       6
#define CAT_ONE_CONTEXT_NODE        7
#define CAT_THREEFOUR_CONTEXT_NODE  8
#define CAT_THREE_CONTEXT_NODE      9
#define CAT_FIVE_CONTEXT_NODE       10




DECLARE_ALIGNED(16, static const TOKENEXTRABITS, vp8d_token_extra_bits2[MAX_ENTROPY_TOKENS]) =
{
    {  0, -1, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },  //ZERO_TOKEN
    {  1, 0, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },   //ONE_TOKEN
    {  2, 0, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },   //TWO_TOKEN
    {  3, 0, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },   //THREE_TOKEN
    {  4, 0, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },   //FOUR_TOKEN
    {  5, 0, { 159, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },  //DCT_VAL_CATEGORY1
    {  7, 1, { 145, 165, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } }, //DCT_VAL_CATEGORY2
    { 11, 2, { 140, 148, 173, 0,  0,  0,  0,  0,  0,  0,  0,  0   } }, //DCT_VAL_CATEGORY3
    { 19, 3, { 135, 140, 155, 176, 0,  0,  0,  0,  0,  0,  0,  0   } }, //DCT_VAL_CATEGORY4
    { 35, 4, { 130, 134, 141, 157, 180, 0,  0,  0,  0,  0,  0,  0   } }, //DCT_VAL_CATEGORY5
    { 67, 10, { 129, 130, 133, 140, 153, 177, 196, 230, 243, 254, 254, 0   } }, //DCT_VAL_CATEGORY6
    {  0, -1, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },  // EOB TOKEN
};

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
DECLARE_ALIGNED(16, const UINT8, vp8_block2context_leftabove[25*3]) =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, //end of vp8_block2context
    0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 0, 0, 1, 1, 0, 0, 1, 1, 0, //end of vp8_block2left
    0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 0, 1, 0, 1, 0, 1, 0 //end of vp8_block2above
};

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

void vp8_reset_mb_tokens_context(MACROBLOCKD *x)
{
    ENTROPY_CONTEXT **const A = x->above_context;
    ENTROPY_CONTEXT(* const L)[4] = x->left_context;

    ENTROPY_CONTEXT *a;
    ENTROPY_CONTEXT *l;
    int i;

    for (i = 0; i < 24; i++)
    {

        a = A[ vp8_block2context[i] ] + vp8_block2above[i];
        l = L[ vp8_block2context[i] ] + vp8_block2left[i];

        *a = *l = 0;
    }

    if (x->mbmi.mode != B_PRED && x->mbmi.mode != SPLITMV)
    {
        a = A[Y2CONTEXT] + vp8_block2above[24];
        l = L[Y2CONTEXT] + vp8_block2left[24];
        *a = *l = 0;
    }


}

#define ONYXBLOCK2CONTEXT_OFFSET    0
#define ONYXBLOCK2LEFT_OFFSET       25
#define ONYXBLOCK2ABOVE_OFFSET 50

DECLARE_ALIGNED(16, const static unsigned char, norm[128]) =
{
    0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
void init_detokenizer(VP8D_COMP *dx)
{
    const VP8_COMMON *const oc = & dx->common;
    MACROBLOCKD *x = & dx->mb;

    dx->detoken.norm_ptr = (unsigned char *)norm;
    dx->detoken.vp8_coef_tree_ptr = (vp8_tree_index *)vp8_coef_tree;
    dx->detoken.ptr_onyxblock2context_leftabove = (UINT8 *)vp8_block2context_leftabove;
    dx->detoken.ptr_onyx_coef_bands_x = vp8_coef_bands_x;
    dx->detoken.scan = (int *)vp8_default_zig_zag1d;
    dx->detoken.teb_base_ptr = (TOKENEXTRABITS *)vp8d_token_extra_bits2;

    dx->detoken.qcoeff_start_ptr = &x->qcoeff[0];


    dx->detoken.coef_probs[0] = (unsigned char *)(oc->fc.coef_probs [0] [ 0 ] [0]);
    dx->detoken.coef_probs[1] = (unsigned char *)(oc->fc.coef_probs [1] [ 0 ] [0]);
    dx->detoken.coef_probs[2] = (unsigned char *)(oc->fc.coef_probs [2] [ 0 ] [0]);
    dx->detoken.coef_probs[3] = (unsigned char *)(oc->fc.coef_probs [3] [ 0 ] [0]);

}

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/


//shift = norm[range]; \
//      shift = norm_ptr[range]; \

#define NORMALIZE \
    /*if(range < 0x80)*/                            \
    { \
        shift = detoken->norm_ptr[range]; \
        range <<= shift; \
        value <<= shift; \
        count -= shift; \
        if(count <= 0) \
        { \
            count += BR_COUNT ; \
            value |= (*bufptr) << (BR_COUNT-count); \
            bufptr++; \
        } \
    }
#if 1
#define DECODE_AND_APPLYSIGN(value_to_sign) \
    split = (range + 1) >> 1; \
    if ( (value >> 24) < split ) \
    { \
        range = split; \
        v= value_to_sign; \
    } \
    else \
    { \
        range = range-split; \
        value = value-(split<<24); \
        v = -value_to_sign; \
    } \
    range +=range;                   \
    value +=value;                   \
    if (!--count) \
    { \
        count = BR_COUNT; \
        value |= *bufptr; \
        bufptr++; \
    }

#define DECODE_AND_BRANCH_IF_ZERO(probability,branch) \
    { \
        split = 1 +  ((( probability*(range-1) ) )>> 8); \
        if ( (value >> 24) < split ) \
        { \
            range = split; \
            NORMALIZE \
            goto branch; \
        } \
        value -= (split<<24); \
        range = range - split; \
        NORMALIZE \
    }

#define DECODE_AND_LOOP_IF_ZERO(probability,branch) \
    { \
        split = 1 + ((( probability*(range-1) ) ) >> 8); \
        if ( (value >> 24) < split ) \
        { \
            range = split; \
            NORMALIZE \
            Prob = coef_probs; \
            ++c; \
            Prob += vp8_coef_bands_x[c]; \
            goto branch; \
        } \
        value -= (split<<24); \
        range = range - split; \
        NORMALIZE \
    }

#define DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val) \
    DECODE_AND_APPLYSIGN(val) \
    Prob = coef_probs + (ENTROPY_NODES*2); \
    if(c < 15){\
        qcoeff_ptr [ scan[c] ] = (INT16) v; \
        ++c; \
        goto DO_WHILE; }\
    qcoeff_ptr [ scan[15] ] = (INT16) v; \
    goto BLOCK_FINISHED;


#define DECODE_EXTRABIT_AND_ADJUST_VAL(t,bits_count)\
    split = 1 +  (((range-1) * vp8d_token_extra_bits2[t].Probs[bits_count]) >> 8); \
    if(value >= (split<<24))\
    {\
        range = range-split;\
        value = value-(split<<24);\
        val += ((UINT16)1<<bits_count);\
    }\
    else\
    {\
        range = split;\
    }\
    NORMALIZE
#endif

#if 0
int vp8_decode_mb_tokens(VP8D_COMP *dx, MACROBLOCKD *x)
{
    ENTROPY_CONTEXT **const A = x->above_context;
    ENTROPY_CONTEXT(* const L)[4] = x->left_context;
    const VP8_COMMON *const oc = & dx->common;

    BOOL_DECODER *bc = x->current_bc;

    ENTROPY_CONTEXT *a;
    ENTROPY_CONTEXT *l;
    int i;

    int eobtotal = 0;

    register int count;

    BOOL_DATA *bufptr;
    register unsigned int range;
    register unsigned int value;
    const int *scan;
    register unsigned int shift;
    UINT32 split;
    INT16 *qcoeff_ptr;

    UINT8 *coef_probs;
    int type;
    int stop;
    INT16 val, bits_count;
    INT16 c;
    INT16 t;
    INT16 v;
    vp8_prob *Prob;

    //int *scan;
    type = 3;
    i = 0;
    stop = 16;

    if (x->mbmi.mode != B_PRED && x->mbmi.mode != SPLITMV)
    {
        i = 24;
        stop = 24;
        type = 1;
        qcoeff_ptr = &x->qcoeff[24*16];
        scan = vp8_default_zig_zag1d;
        eobtotal -= 16;
    }
    else
    {
        scan = vp8_default_zig_zag1d;
        qcoeff_ptr = &x->qcoeff[0];
    }

    count   = bc->count;
    range   = bc->range;
    value   = bc->value;
    bufptr  = &bc->buffer[bc->pos];


    coef_probs = (unsigned char *)(oc->fc.coef_probs [type] [ 0 ] [0]);

BLOCK_LOOP:
    a = A[ vp8_block2context[i] ] + vp8_block2above[i];
    l = L[ vp8_block2context[i] ] + vp8_block2left[i];
    c = (INT16)(!type);

    VP8_COMBINEENTROPYCONTEXTS(t, *a, *l);
    Prob = coef_probs;
    Prob += t * ENTROPY_NODES;

DO_WHILE:
    Prob += vp8_coef_bands_x[c];
    DECODE_AND_BRANCH_IF_ZERO(Prob[EOB_CONTEXT_NODE], BLOCK_FINISHED);

CHECK_0_:
    DECODE_AND_LOOP_IF_ZERO(Prob[ZERO_CONTEXT_NODE], CHECK_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[ONE_CONTEXT_NODE], ONE_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[LOW_VAL_CONTEXT_NODE], LOW_VAL_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[HIGH_LOW_CONTEXT_NODE], HIGH_LOW_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[CAT_THREEFOUR_CONTEXT_NODE], CAT_THREEFOUR_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[CAT_FIVE_CONTEXT_NODE], CAT_FIVE_CONTEXT_NODE_0_);
    val = vp8d_token_extra_bits2[DCT_VAL_CATEGORY6].min_val;
    bits_count = vp8d_token_extra_bits2[DCT_VAL_CATEGORY6].Length;

    do
    {
        DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY6, bits_count);
        bits_count -- ;
    }
    while (bits_count >= 0);

    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_FIVE_CONTEXT_NODE_0_:
    val = vp8d_token_extra_bits2[DCT_VAL_CATEGORY5].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 4);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 3);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 2);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_THREEFOUR_CONTEXT_NODE_0_:
    DECODE_AND_BRANCH_IF_ZERO(Prob[CAT_THREE_CONTEXT_NODE], CAT_THREE_CONTEXT_NODE_0_);
    val = vp8d_token_extra_bits2[DCT_VAL_CATEGORY4].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY4, 3);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY4, 2);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY4, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY4, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_THREE_CONTEXT_NODE_0_:
    val = vp8d_token_extra_bits2[DCT_VAL_CATEGORY3].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY3, 2);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY3, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY3, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

HIGH_LOW_CONTEXT_NODE_0_:
    DECODE_AND_BRANCH_IF_ZERO(Prob[CAT_ONE_CONTEXT_NODE], CAT_ONE_CONTEXT_NODE_0_);

    val = vp8d_token_extra_bits2[DCT_VAL_CATEGORY2].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY2, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY2, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_ONE_CONTEXT_NODE_0_:
    val = vp8d_token_extra_bits2[DCT_VAL_CATEGORY1].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY1, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

LOW_VAL_CONTEXT_NODE_0_:
    DECODE_AND_BRANCH_IF_ZERO(Prob[TWO_CONTEXT_NODE], TWO_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[THREE_CONTEXT_NODE], THREE_CONTEXT_NODE_0_);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(4);

THREE_CONTEXT_NODE_0_:
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(3);

TWO_CONTEXT_NODE_0_:
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(2);

ONE_CONTEXT_NODE_0_:
    DECODE_AND_APPLYSIGN(1);
    Prob = coef_probs + ENTROPY_NODES;

    if (c < 15)
    {
        qcoeff_ptr [ scan[c] ] = (INT16) v;
        ++c;
        goto DO_WHILE;
    }

    qcoeff_ptr [ scan[15] ] = (INT16) v;
BLOCK_FINISHED:
    t = ((x->Block[i].eob = c) != !type);   // any nonzero data?
    eobtotal += x->Block[i].eob;
    *a = *l = t;
    qcoeff_ptr += 16;

    i++;

    if (i < stop)
        goto BLOCK_LOOP;

    if (i == 25)
    {
        scan = vp8_default_zig_zag1d;//x->scan_order1d;
        type = 0;
        i = 0;
        stop = 16;
        coef_probs = (unsigned char *)(oc->fc.coef_probs [type] [ 0 ] [0]);
        qcoeff_ptr = &x->qcoeff[0];
        goto BLOCK_LOOP;
    }

    if (i == 16)
    {
        type = 2;
        coef_probs = (unsigned char *)(oc->fc.coef_probs [type] [ 0 ] [0]);
        stop = 24;
        goto BLOCK_LOOP;
    }

    bc->count = count;
    bc->value = value;
    bc->range = range;
    bc->pos  = bufptr - bc->buffer;
    return eobtotal;

}
//#endif
#else
/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

#if 0
//uses relative offsets

const vp8_tree_index vp8_coef_tree_x[ 22] =   /* corresponding _CONTEXT_NODEs */
{
    -DCT_EOB_TOKEN, 1,                             /* 0 = EOB */
    -ZERO_TOKEN, 1,                               /* 1 = ZERO */
    -ONE_TOKEN, 1,                               /* 2 = ONE */
    2, 5,                                       /* 3 = LOW_VAL */
    -TWO_TOKEN, 1,                         /* 4 = TWO */
    -THREE_TOKEN, -FOUR_TOKEN,                /* 5 = THREE */
    2, 3,                                  /* 6 = HIGH_LOW */
    -DCT_VAL_CATEGORY1, -DCT_VAL_CATEGORY2,   /* 7 = CAT_ONE */
    2, 3,                                 /* 8 = CAT_THREEFOUR */
    -DCT_VAL_CATEGORY3, -DCT_VAL_CATEGORY4,  /* 9 = CAT_THREE */
    -DCT_VAL_CATEGORY5, -DCT_VAL_CATEGORY6   /* 10 = CAT_FIVE */
};
#endif

#define _SCALEDOWN 8 //16 //8

int vp8_decode_mb_tokens_v5(DETOK *detoken, int type);

int vp8_decode_mb_tokens_v5_c(DETOK *detoken, int type)
{
    BOOL_DECODER *bc = detoken->current_bc;

    ENTROPY_CONTEXT *a;
    ENTROPY_CONTEXT *l;
    int i;

    register int count;

    BOOL_DATA *bufptr;
    register unsigned int range;
    register unsigned int value;
    register unsigned int shift;
    UINT32 split;
    INT16 *qcoeff_ptr;

    UINT8 *coef_probs;
//  int type;
    int stop;
    INT16 c;
    INT16 t;
    INT16 v;
    vp8_prob *Prob;



//  type = 3;
    i = 0;
    stop = 16;
    qcoeff_ptr = detoken->qcoeff_start_ptr;

//  if( detoken->mode != B_PRED && detoken->mode != SPLITMV)
    if (type == 1)
    {
        i += 24;
        stop += 8; //24;
//      type = 1;
        qcoeff_ptr += 24 * 16;
//      eobtotal-=16;
    }

    count   = bc->count;
    range   = bc->range;
    value   = bc->value;
    bufptr  = &bc->buffer[bc->pos];


    coef_probs = detoken->coef_probs[type]; //(unsigned char *)( oc->fc.coef_probs [type] [ 0 ] [0]);

BLOCK_LOOP:
    a = detoken->A[ detoken->ptr_onyxblock2context_leftabove[i] ];
    l = detoken->L[ detoken->ptr_onyxblock2context_leftabove[i] ];
    c = !type;
    a += detoken->ptr_onyxblock2context_leftabove[i + ONYXBLOCK2ABOVE_OFFSET];
    l += detoken->ptr_onyxblock2context_leftabove[i + ONYXBLOCK2LEFT_OFFSET];

    //#define ONYX_COMBINEENTROPYCONTEXTS( Dest, A, B) \
    //Dest = ((A)!=0) + ((B)!=0);

    VP8_COMBINEENTROPYCONTEXTS(t, *a, *l);

    Prob = coef_probs;
    Prob += t * ENTROPY_NODES;
    t = 0;

    do
    {

        {
//                  onyx_tree_index * onyx_coef_tree_ptr = onyx_coef_tree_x;

            Prob += detoken->ptr_onyx_coef_bands_x[c];

        GET_TOKEN_START:

            do
            {
                split = 1 + (((range - 1) * (Prob[t>>1])) >> 8);

                if (value >> 24 >= split)
                {
                    range = range - split;
                    value = value - (split << 24);
                    t += 1;

                    //used to eliminate else branch
                    split = range;
                }

                range = split;

                t = detoken->vp8_coef_tree_ptr[ t ];

                NORMALIZE

            }
            while (t  > 0) ;
        }
    GET_TOKEN_STOP:

        if (t == -DCT_EOB_TOKEN)
        {
            break;
        }

        v = -t;

        if (v > FOUR_TOKEN)
        {
            INT16 bits_count;
            TOKENEXTRABITS *teb_ptr;

//                      teb_ptr = &onyxd_token_extra_bits2[t];
//                  teb_ptr = &onyxd_token_extra_bits2[v];
            teb_ptr = &detoken->teb_base_ptr[v];


            v = teb_ptr->min_val;
            bits_count = teb_ptr->Length;

            do
            {
                split = 1 + (((range - 1) * teb_ptr->Probs[bits_count]) >> _SCALEDOWN);

                if ((value >> 24) >= split)
                {
                    range = range - split;
                    value = value - (split << 24);
                    v += ((UINT16)1 << bits_count);

                    //used to eliminate else branch
                    split = range;
                }

                range = split;

                NORMALIZE

                bits_count -- ;
            }
            while (bits_count >= 0);
        }

        Prob = coef_probs;

        if (t)
        {
            split = 1 + (((range - 1) * vp8_prob_half) >> 8);

            if ((value >> 24) >= split)
            {
                range = range - split;
                value = value - (split << 24);
                v = (v ^ -1) + 1;           /* negate w/out conditionals */

                //used to eliminate else branch
                split = range;
            }

            range = split;

            NORMALIZE
            Prob += ENTROPY_NODES;

            if (t < -ONE_TOKEN)
                Prob += ENTROPY_NODES;

            t = -2;
        }

        //if t is zero, we will skip the eob table check
        t += 2;
        qcoeff_ptr [detoken->scan [c] ] = (INT16) v;

    }
    while (++c < 16);

    if (t != -DCT_EOB_TOKEN)
    {
        --c;
    }

    t = ((detoken->eob[i] = c) != !type);   // any nonzero data?
//  eobtotal += detoken->eob[i];
    *a = *l = t;
    qcoeff_ptr += 16;

    i++;

    if (i < stop)
        goto BLOCK_LOOP;

    if (i == 25)
    {
        type = 0;
        i = 0;
        stop = 16;
//      coef_probs = (unsigned char *)(oc->fc.coef_probs [type] [ 0 ] [0]);
        coef_probs = detoken->coef_probs[type]; //(unsigned char *)( oc->fc.coef_probs [type] [ 0 ] [0]);
        qcoeff_ptr = detoken->qcoeff_start_ptr;
        goto BLOCK_LOOP;
    }

    if (i == 16)
    {
        type = 2;
//      coef_probs =(unsigned char *)( oc->fc.coef_probs [type] [ 0 ] [0]);
        coef_probs = detoken->coef_probs[type]; //(unsigned char *)( oc->fc.coef_probs [type] [ 0 ] [0]);
        stop = 24;
        goto BLOCK_LOOP;
    }

    bc->count = count;
    bc->value = value;
    bc->range = range;
    bc->pos  = bufptr - bc->buffer;
    return 0;
}
//#if 0
int vp8_decode_mb_tokens(VP8D_COMP *dx, MACROBLOCKD *x)
{
//  const ONYX_COMMON * const oc = & dx->common;
    int eobtotal = 0;
    int i, type;
    /*
        dx->detoken.norm_ptr = norm;
        dx->detoken.onyx_coef_tree_ptr = onyx_coef_tree;
        dx->detoken.ptr_onyxblock2context_leftabove = ONYXBLOCK2CONTEXT_LEFTABOVE;
        dx->detoken.ptr_onyx_coef_bands_x = onyx_coef_bands_x;
        dx->detoken.scan = default_zig_zag1d;
        dx->detoken.teb_base_ptr = onyxd_token_extra_bits2;

        dx->detoken.qcoeff_start_ptr = &x->qcoeff[0];

        dx->detoken.A = x->above_context;
        dx->detoken.L = x->left_context;

        dx->detoken.coef_probs[0] = (unsigned char *)( oc->fc.coef_probs [0] [ 0 ] [0]);
        dx->detoken.coef_probs[1] = (unsigned char *)( oc->fc.coef_probs [1] [ 0 ] [0]);
        dx->detoken.coef_probs[2] = (unsigned char *)( oc->fc.coef_probs [2] [ 0 ] [0]);
        dx->detoken.coef_probs[3] = (unsigned char *)( oc->fc.coef_probs [3] [ 0 ] [0]);
    */

    dx->detoken.current_bc = x->current_bc;
    dx->detoken.A = x->above_context;
    dx->detoken.L = x->left_context;

    type = 3;

    if (x->mbmi.mode != B_PRED && x->mbmi.mode != SPLITMV)
    {
        type = 1;
        eobtotal -= 16;
    }

    vp8_decode_mb_tokens_v5(&dx->detoken, type);

    for (i = 0; i < 25; i++)
    {
        x->Block[i].eob = dx->detoken.eob[i];
        eobtotal += dx->detoken.eob[i];
    }

    return eobtotal;
}
#endif
