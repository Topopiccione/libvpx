/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "onyx_int.h"
#include "tokenize.h"
#include "vpx_mem/vpx_mem.h"

/* Global event counters used for accumulating statistics across several
   compressions, then generating context.c = initial stats. */

#ifdef ENTROPY_STATS
_int64 context_counters[BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [MAX_ENTROPY_TOKENS];
#if CONFIG_T8X8
_int64 context_counters_8x8[BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [MAX_ENTROPY_TOKENS];
#endif
#endif
void vp8_stuff_mb(VP8_COMP *cpi, MACROBLOCKD *x, TOKENEXTRA **t) ;
#if CONFIG_T8X8
void vp8_stuff_mb_8x8(VP8_COMP *cpi, MACROBLOCKD *x, TOKENEXTRA **t) ;
#endif
void vp8_fix_contexts(MACROBLOCKD *x);

static TOKENVALUE dct_value_tokens[DCT_MAX_VALUE*2];
const TOKENVALUE *vp8_dct_value_tokens_ptr;
static int dct_value_cost[DCT_MAX_VALUE*2];
const int *vp8_dct_value_cost_ptr;

#ifdef ENC_DEBUG
extern int mb_row_debug;
extern int mb_col_debug;
extern int enc_debug;
#endif

static void fill_value_tokens()
{

    TOKENVALUE *const t = dct_value_tokens + DCT_MAX_VALUE;
    vp8_extra_bit_struct *const e = vp8_extra_bits;

    int i = -DCT_MAX_VALUE;
    int sign = 1;

    do
    {
        if (!i)
            sign = 0;

        {
            const int a = sign ? -i : i;
            int eb = sign;

            if (a > 4)
            {
                int j = 4;

                while (++j < 11  &&  e[j].base_val <= a) {}

                t[i].Token = --j;
                eb |= (a - e[j].base_val) << 1;
            }
            else
                t[i].Token = a;

            t[i].Extra = eb;
        }

        // initialize the cost for extra bits for all possible coefficient value.
        {
            int cost = 0;
            vp8_extra_bit_struct *p = vp8_extra_bits + t[i].Token;

            if (p->base_val)
            {
                const int extra = t[i].Extra;
                const int Length = p->Len;

                if (Length)
                    cost += vp8_treed_cost(p->tree, p->prob, extra >> 1, Length);

                cost += vp8_cost_bit(vp8_prob_half, extra & 1); /* sign */
                dct_value_cost[i + DCT_MAX_VALUE] = cost;
            }

        }

    }
    while (++i < DCT_MAX_VALUE);

    vp8_dct_value_tokens_ptr = dct_value_tokens + DCT_MAX_VALUE;
    vp8_dct_value_cost_ptr   = dct_value_cost + DCT_MAX_VALUE;
}

#if CONFIG_T8X8
static void tokenize2nd_order_b_8x8
(
    const BLOCKD *const b,
    TOKENEXTRA **tp,
    const int type,     /* which plane: 0=Y no DC, 1=Y2, 2=UV, 3=Y with DC */
    const FRAME_TYPE frametype,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,
    VP8_COMP *cpi
)
{
    int pt; /* near block/prev token context index */
    int c = 0;          /* start at DC */
    const int eob = b->eob;     /* one beyond last nonzero coeff */
    TOKENEXTRA *t = *tp;        /* store tokens starting here */
    int x;
    const short *qcoeff_ptr = b->qcoeff;
    VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

    assert(eob<=4);

    do
    {
        const int band = vp8_coef_bands[c];

        if (c < eob)
        {
            int rc = vp8_default_zig_zag1d[c];
            const int v = qcoeff_ptr[rc];

            assert(-DCT_MAX_VALUE <= v  &&  v < (DCT_MAX_VALUE));

            t->Extra = vp8_dct_value_tokens_ptr[v].Extra;
            x        = vp8_dct_value_tokens_ptr[v].Token;
        }
        else
            x = DCT_EOB_TOKEN;

        t->Token = x;
        //printf("Token : %d\n", x);
        t->context_tree = cpi->common.fc.coef_probs_8x8 [type] [band] [pt];

        t->skip_eob_node = pt == 0 && ((band > 0 && type > 0) || (band > 1 && type == 0));

#ifdef ENC_DEBUG
        if (t->skip_eob_node && vp8_coef_encodings[x].Len==1)
          printf("Trouble 2 x=%d Len=%d skip=%d eob=%d c=%d band=%d type=%d: [%d %d %d]\n",
                 x, vp8_coef_encodings[x].Len, t->skip_eob_node, eob, c, band, type,
                 cpi->count, mb_row_debug, mb_col_debug);
#endif

        ++cpi->coef_counts_8x8       [type] [band] [pt] [x];
    }
    while (pt = vp8_prev_token_class[x], ++t, c < eob  &&  ++c < 4);

    *tp = t;
    pt = (c != !type); /* 0 <-> all coeff data is zero */
    *a = *l = pt;

}
#endif

static void tokenize2nd_order_b
(
    MACROBLOCKD *x,
    TOKENEXTRA **tp,
    VP8_COMP *cpi
)
{
    int pt;             /* near block/prev token context index */
    int c;              /* start at DC */
    TOKENEXTRA *t = *tp;/* store tokens starting here */
    const BLOCKD *b;
    const short *qcoeff_ptr;
    ENTROPY_CONTEXT * a;
    ENTROPY_CONTEXT * l;
    int band, rc, v, token;

    b = x->block + 24;
    qcoeff_ptr = b->qcoeff;
    a = (ENTROPY_CONTEXT *)x->above_context + 8;
    l = (ENTROPY_CONTEXT *)x->left_context + 8;

    VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

    for (c = 0; c < b->eob; c++)
    {
        rc = vp8_default_zig_zag1d[c];
        band = vp8_coef_bands[c];
        v = qcoeff_ptr[rc];

        t->Extra = vp8_dct_value_tokens_ptr[v].Extra;
        token    = vp8_dct_value_tokens_ptr[v].Token;

        t->Token = token;
        t->context_tree = cpi->common.fc.coef_probs [1] [band] [pt];

        t->skip_eob_node = ((pt == 0) && (band > 0));

        ++cpi->coef_counts       [1] [band] [pt] [token];

        pt = vp8_prev_token_class[token];
        t++;
    }
    if (c < 16)
    {
        band = vp8_coef_bands[c];
        t->Token = DCT_EOB_TOKEN;
        t->context_tree = cpi->common.fc.coef_probs [1] [band] [pt];

        t->skip_eob_node = ((pt == 0) && (band > 0));

        ++cpi->coef_counts       [1] [band] [pt] [DCT_EOB_TOKEN];

        t++;
    }

    *tp = t;
    pt = (c != 0); /* 0 <-> all coeff data is zero */
    *a = *l = pt;

}
#if CONFIG_T8X8
static void tokenize1st_order_b_8x8
(
    const BLOCKD *const b,
    TOKENEXTRA **tp,
    const int type,     /* which plane: 0=Y no DC, 1=Y2, 2=UV, 3=Y with DC */
    const FRAME_TYPE frametype,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,
    ENTROPY_CONTEXT *a1,
    ENTROPY_CONTEXT *l1,
    VP8_COMP *cpi
)
{
    int pt; /* near block/prev token context index */
    int c = type ? 0 : 1;       /* start at DC unless type 0 */
    const int eob = b->eob;     /* one beyond last nonzero coeff */
    TOKENEXTRA *t = *tp;        /* store tokens starting here */
    int x;
    const short *qcoeff_ptr = b->qcoeff;
    VP8_COMBINEENTROPYCONTEXTS_8x8(pt, *a, *l, *a1, *l1);

    do
    {
        const int band = vp8_coef_bands_8x8[c];

        x = DCT_EOB_TOKEN;

        if (c < eob)
        {
            int rc = vp8_default_zig_zag1d_8x8[c];
            const int v = qcoeff_ptr[rc];

            assert(-DCT_MAX_VALUE <= v  &&  v < (DCT_MAX_VALUE));

            t->Extra = vp8_dct_value_tokens_ptr[v].Extra;
            x        = vp8_dct_value_tokens_ptr[v].Token;
        }

        t->Token = x;
        t->context_tree = cpi->common.fc.coef_probs_8x8 [type] [band] [pt];

        t->skip_eob_node = pt == 0 && ((band > 0 && type > 0) || (band > 1 && type == 0));

#ifdef ENC_DEBUG
        if (t->skip_eob_node && vp8_coef_encodings[x].Len==1)
          printf("Trouble 1 x=%d Len=%d skip=%d eob=%d c=%d band=%d type=%d: [%d %d %d]\n", x, vp8_coef_encodings[x].Len, t->skip_eob_node, eob, c, band, type, cpi->count, mb_row_debug, mb_col_debug);
#endif

        ++cpi->coef_counts_8x8       [type] [band] [pt] [x];
    }
    while (pt = vp8_prev_token_class[x], ++t, c < eob  &&  ++c < 64);

    *tp = t;
    pt = (c != !type); /* 0 <-> all coeff data is zero */
    *a = *l = pt;
}

#endif


static void tokenize1st_order_b
(
    MACROBLOCKD *x,
    TOKENEXTRA **tp,
    int type,           /* which plane: 0=Y no DC, 1=Y2, 2=UV, 3=Y with DC */
    VP8_COMP *cpi
)
{
    unsigned int block;
    const BLOCKD *b;
    int pt;             /* near block/prev token context index */
    int c;
    int token;
    TOKENEXTRA *t = *tp;/* store tokens starting here */
    const short *qcoeff_ptr;
    ENTROPY_CONTEXT * a;
    ENTROPY_CONTEXT * l;
    int band, rc, v;
    int tmp1, tmp2;

    b = x->block;
    /* Luma */
    for (block = 0; block < 16; block++, b++)
    {
        tmp1 = vp8_block2above[block];
        tmp2 = vp8_block2left[block];
        qcoeff_ptr = b->qcoeff;
        a = (ENTROPY_CONTEXT *)x->above_context + tmp1;
        l = (ENTROPY_CONTEXT *)x->left_context + tmp2;

        VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

        c = type ? 0 : 1;

        for (; c < b->eob; c++)
        {
            rc = vp8_default_zig_zag1d[c];
            band = vp8_coef_bands[c];
            v = qcoeff_ptr[rc];

            t->Extra = vp8_dct_value_tokens_ptr[v].Extra;
            token    = vp8_dct_value_tokens_ptr[v].Token;

            t->Token = token;
            t->context_tree = cpi->common.fc.coef_probs [type] [band] [pt];

            t->skip_eob_node = pt == 0 &&
                ((band > 0 && type > 0) || (band > 1 && type == 0));

            ++cpi->coef_counts       [type] [band] [pt] [token];

            pt = vp8_prev_token_class[token];
            t++;
        }
        if (c < 16)
        {
            band = vp8_coef_bands[c];
            t->Token = DCT_EOB_TOKEN;
            t->context_tree = cpi->common.fc.coef_probs [type] [band] [pt];

            t->skip_eob_node = pt == 0 &&
                ((band > 0 && type > 0) || (band > 1 && type == 0));

            ++cpi->coef_counts       [type] [band] [pt] [DCT_EOB_TOKEN];

            t++;
        }
        *tp = t;
        pt = (c != !type); /* 0 <-> all coeff data is zero */
        *a = *l = pt;

    }
    /* Chroma */
    for (block = 16; block < 24; block++, b++)
    {
        tmp1 = vp8_block2above[block];
        tmp2 = vp8_block2left[block];
        qcoeff_ptr = b->qcoeff;
        a = (ENTROPY_CONTEXT *)x->above_context + tmp1;
        l = (ENTROPY_CONTEXT *)x->left_context + tmp2;

        VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

        for (c = 0; c < b->eob; c++)
        {
            rc = vp8_default_zig_zag1d[c];
            band = vp8_coef_bands[c];
            v = qcoeff_ptr[rc];

            t->Extra = vp8_dct_value_tokens_ptr[v].Extra;
            token    = vp8_dct_value_tokens_ptr[v].Token;

            t->Token = token;
            t->context_tree = cpi->common.fc.coef_probs [2] [band] [pt];

            t->skip_eob_node = ((pt == 0) && (band > 0));

            ++cpi->coef_counts       [2] [band] [pt] [token];

            pt = vp8_prev_token_class[token];
            t++;
        }
        if (c < 16)
        {
            band = vp8_coef_bands[c];
            t->Token = DCT_EOB_TOKEN;
            t->context_tree = cpi->common.fc.coef_probs [2] [band] [pt];

            t->skip_eob_node = ((pt == 0) && (band > 0));

            ++cpi->coef_counts       [2] [band] [pt] [DCT_EOB_TOKEN];

            t++;
        }
        *tp = t;
        pt = (c != 0); /* 0 <-> all coeff data is zero */
        *a = *l = pt;
    }

}


static int mb_is_skippable(MACROBLOCKD *x, int has_y2_block)
{
    int skip = 1;
    int i = 0;

    if (has_y2_block)
    {
        for (i = 0; i < 16; i++)
            skip &= (x->block[i].eob < 2);
    }

    for (; i < 24 + has_y2_block; i++)
        skip &= (!x->block[i].eob);

    return skip;
}

#if CONFIG_T8X8
static int mb_is_skippable_8x8(MACROBLOCKD *x)
{
    int has_y2_block;
    int skip = 1;
    int i = 0;
 
    has_y2_block = (x->mode_info_context->mbmi.mode != B_PRED
                    && x->mode_info_context->mbmi.mode != SPLITMV);
    if (has_y2_block)
    {
        for (i = 0; i < 16; i+=4)
            skip &= (x->block[i].eob < 2);
    }

    for (; i < 24 + has_y2_block; i+=4)
        skip &= (!x->block[i].eob);

    return skip;
}
#endif

void vp8_tokenize_mb(VP8_COMP *cpi, MACROBLOCKD *x, TOKENEXTRA **t)
{
    int plane_type;
    int has_y2_block;
    int b;

    has_y2_block = (x->mode_info_context->mbmi.mode != B_PRED
#if CONFIG_I8X8
                    && x->mode_info_context->mbmi.mode != I8X8_PRED
#endif
                    && x->mode_info_context->mbmi.mode != SPLITMV);

    x->mode_info_context->mbmi.mb_skip_coeff =
#if CONFIG_T8X8
        (x->mode_info_context->mbmi.segment_id >= 2 ?
         mb_is_skippable_8x8(x) :
         mb_is_skippable(x, has_y2_block));
#else
         mb_is_skippable(x, has_y2_block);
#endif

    if (x->mode_info_context->mbmi.mb_skip_coeff)
    {
        cpi->skip_true_count++;

        if (!cpi->common.mb_no_coeff_skip)
        {
#if CONFIG_T8X8
            if (x->mode_info_context->mbmi.segment_id >= 2)
                vp8_stuff_mb_8x8(cpi, x, t) ;
            else
#endif
                vp8_stuff_mb(cpi, x, t) ;
        }
        else
        {
            vp8_fix_contexts(x);
        }

        return;
    }

    cpi->skip_false_count++;

    plane_type = 3;
    if(has_y2_block)
    {
#if CONFIG_T8X8
        if (x->mode_info_context->mbmi.segment_id >= 2)
        {
            ENTROPY_CONTEXT * A = (ENTROPY_CONTEXT *)x->above_context;
            ENTROPY_CONTEXT * L = (ENTROPY_CONTEXT *)x->left_context;
            tokenize2nd_order_b_8x8(x->block + 24, t, 1, x->frame_type,
                       A + vp8_block2above[24], L + vp8_block2left[24], cpi);
        }
        else
#endif
            tokenize2nd_order_b(x, t, cpi);

            plane_type = 0;

    }
#if CONFIG_T8X8
    if (x->mode_info_context->mbmi.segment_id >= 2)
    {
        ENTROPY_CONTEXT * A = (ENTROPY_CONTEXT *)x->above_context;
        ENTROPY_CONTEXT * L = (ENTROPY_CONTEXT *)x->left_context;
        for (b = 0; b < 16; b+=4)
        {
            tokenize1st_order_b_8x8(x->block + b, t, plane_type, x->frame_type,
                                A + vp8_block2above[b],
                                L + vp8_block2left[b],
                                A + vp8_block2above[b+1],
                                L + vp8_block2left[b+4],
                                cpi);
          /* *(A + vp8_block2above[b+1]) = *(A + vp8_block2above[b+2]) = *(A + vp8_block2above[b+3]) =
                *(A + vp8_block2above[b]);
            *(L + vp8_block2left[b+1]) = *(L + vp8_block2left[b+2]) = *(L + vp8_block2left[b+3]) =
                *(L + vp8_block2left[b]);*/
            // build coeff context for 8x8 transform
              if(b==0)
              {
                *(A + vp8_block2above[1]) = *(A + vp8_block2above[4]) = *(A + vp8_block2above[5]) = *(A + vp8_block2above[b]);
                *(L + vp8_block2left[1]) = *(L + vp8_block2left[4]) = *(L + vp8_block2left[5]) = *(L + vp8_block2left[b]);
              }
              else if(b==4)
              {
                *(A + vp8_block2above[2]) = *(A + vp8_block2above[3]) = *(A + vp8_block2above[6]) = *(A + vp8_block2above[7]) = *(A + vp8_block2above[b]);
                *(L + vp8_block2left[2]) = *(L + vp8_block2left[3]) = *(L + vp8_block2left[6]) = *(L + vp8_block2left[7]) = *(L + vp8_block2left[b]);
                *(A + vp8_block2above[4]) = *(A + vp8_block2above[1]);
                *(L + vp8_block2left[4]) = *(L + vp8_block2left[1]);
              }
              else if(b==8)
              {
                *(A + vp8_block2above[9]) = *(A + vp8_block2above[12]) = *(A + vp8_block2above[13]) = *(A + vp8_block2above[b]);
                *(L + vp8_block2left[9]) = *(L + vp8_block2left[12]) = *(L + vp8_block2left[13]) = *(L + vp8_block2left[b]);
              }
              else if(b==12)
              {
                *(A + vp8_block2above[10]) = *(A + vp8_block2above[11]) = *(A + vp8_block2above[14]) = *(A + vp8_block2above[15]) = *(A + vp8_block2above[b]);
                *(L + vp8_block2left[10]) = *(L + vp8_block2left[11]) = *(L + vp8_block2left[14]) = *(L + vp8_block2left[15]) = *(L + vp8_block2left[b]);
                *(A + vp8_block2above[12]) = *(A + vp8_block2above[8]);
                *(L + vp8_block2left[12]) = *(L + vp8_block2left[8]);
             }

        }

        for (b = 16; b < 24; b+=4) {
            tokenize1st_order_b_8x8(x->block + b, t, 2, x->frame_type,
                                    A + vp8_block2above[b],
                                    L + vp8_block2left[b],
                                    A + vp8_block2above[b+1],
                                    L + vp8_block2left[b+2],
                                    cpi);
            *(A + vp8_block2above[b+1]) = *(A + vp8_block2above[b+2]) = *(A + vp8_block2above[b+3]) =
                *(A + vp8_block2above[b]);
            *(L + vp8_block2left[b+1]) = *(L + vp8_block2left[b+2]) = *(L + vp8_block2left[b+3]) =
                *(L + vp8_block2left[b]);
        }
    }
    else
#endif
        tokenize1st_order_b(x, t, plane_type, cpi);
}


#ifdef ENTROPY_STATS

void init_context_counters(void)
{
    vpx_memset(context_counters, 0, sizeof(context_counters));
#if CONFIG_T8X8
    vpx_memset(context_counters_8x8, 0, sizeof(context_counters_8x8));
#endif
}

void print_context_counters()
{

    int type, band, pt, t;

    FILE *const f = fopen("context.c", "w");

    fprintf(f, "#include \"entropy.h\"\n");

    fprintf(f, "\n/* *** GENERATED FILE: DO NOT EDIT *** */\n\n");

    fprintf(f, "int Contexts[BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [MAX_ENTROPY_TOKENS];\n\n");

    fprintf(f, "const int default_contexts[BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [MAX_ENTROPY_TOKENS] = {");

# define Comma( X) (X? ",":"")

    type = 0;

    do
    {
        fprintf(f, "%s\n  { /* block Type %d */", Comma(type), type);

        band = 0;

        do
        {
            fprintf(f, "%s\n    { /* Coeff Band %d */", Comma(band), band);

            pt = 0;

            do
            {
                fprintf(f, "%s\n      {", Comma(pt));

                t = 0;

                do
                {
                    const _int64 x = context_counters [type] [band] [pt] [t];
                    const int y = (int) x;

                    assert(x == (INT64) y);  /* no overflow handling yet */
                    fprintf(f, "%s %d", Comma(t), y);

                }
                while (++t < MAX_ENTROPY_TOKENS);

                fprintf(f, "}");
            }
            while (++pt < PREV_COEF_CONTEXTS);

            fprintf(f, "\n    }");

        }
        while (++band < COEF_BANDS);

        fprintf(f, "\n  }");
    }
    while (++type < BLOCK_TYPES);

#if CONFIG_T8X8
    fprintf(f, "int Contexts_8x8[BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [MAX_ENTROPY_TOKENS];\n\n");

    fprintf(f, "const int default_contexts_8x8[BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [MAX_ENTROPY_TOKENS] = {");

    type = 0;

    do
    {
        fprintf(f, "%s\n  { /* block Type %d */", Comma(type), type);

        band = 0;

        do
        {
            fprintf(f, "%s\n    { /* Coeff Band %d */", Comma(band), band);

            pt = 0;

            do
            {
                fprintf(f, "%s\n      {", Comma(pt));

                t = 0;

                do
                {
                    const _int64 x = context_counters [type] [band] [pt] [t];
                    const int y = (int) x;

                    assert(x == (_int64) y);  /* no overflow handling yet */
                    fprintf(f, "%s %d", Comma(t), y);

                }
                while (++t < MAX_ENTROPY_TOKENS);

                fprintf(f, "}");
            }
            while (++pt < PREV_COEF_CONTEXTS);

            fprintf(f, "\n    }");

        }
        while (++band < COEF_BANDS);

        fprintf(f, "\n  }");
    }
    while (++type < BLOCK_TYPES);
#endif

    fprintf(f, "\n};\n");
    fclose(f);
}
#endif


void vp8_tokenize_initialize()
{
    fill_value_tokens();
}

#if CONFIG_T8X8
static __inline void stuff2nd_order_b_8x8
(
    const BLOCKD *const b,
    TOKENEXTRA **tp,
    const int type,     /* which plane: 0=Y no DC, 1=Y2, 2=UV, 3=Y with DC */
    const FRAME_TYPE frametype,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,
    VP8_COMP *cpi
)
{
    int pt; /* near block/prev token context index */
    TOKENEXTRA *t = *tp;        /* store tokens starting here */
    VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);
    (void) frametype;
    (void) type;
    (void) b;

    t->Token = DCT_EOB_TOKEN;
    t->context_tree = cpi->common.fc.coef_probs_8x8 [1] [0] [pt];
    //t->section = 11;
    t->skip_eob_node = 0;
    ++cpi->coef_counts_8x8       [1] [0] [pt] [DCT_EOB_TOKEN];
    ++t;

    *tp = t;
    pt = 0;
    *a = *l = pt;

}

static __inline void stuff1st_order_b_8x8
(
    const BLOCKD *const b,
    TOKENEXTRA **tp,
    const int type,     /* which plane: 0=Y no DC, 1=Y2, 2=UV, 3=Y with DC */
    const FRAME_TYPE frametype,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,
    ENTROPY_CONTEXT *a1,
    ENTROPY_CONTEXT *l1,
    VP8_COMP *cpi
)
{
    int pt; /* near block/prev token context index */
    TOKENEXTRA *t = *tp;        /* store tokens starting here */
    VP8_COMBINEENTROPYCONTEXTS_8x8(pt, *a, *l, *a1, *l1);
    (void) frametype;
    (void) type;
    (void) b;

    t->Token = DCT_EOB_TOKEN;
    t->context_tree = cpi->common.fc.coef_probs_8x8 [0] [1] [pt];
    //t->section = 8;
    t->skip_eob_node = 0;
    ++cpi->coef_counts_8x8       [0] [1] [pt] [DCT_EOB_TOKEN];
    ++t;
    *tp = t;
    pt = 0; /* 0 <-> all coeff data is zero */
    *a = *l = pt;


}

static __inline
void stuff1st_order_buv_8x8
(
    const BLOCKD *const b,
    TOKENEXTRA **tp,
    const int type,     /* which plane: 0=Y no DC, 1=Y2, 2=UV, 3=Y with DC */
    const FRAME_TYPE frametype,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,
    ENTROPY_CONTEXT *a1,
    ENTROPY_CONTEXT *l1,
    VP8_COMP *cpi
)
{
    int pt; /* near block/prev token context index */
    TOKENEXTRA *t = *tp;        /* store tokens starting here */
    VP8_COMBINEENTROPYCONTEXTS_8x8(pt, *a, *l, *a1, *l1);
    (void) frametype;
    (void) type;
    (void) b;

    t->Token = DCT_EOB_TOKEN;
    t->context_tree = cpi->common.fc.coef_probs_8x8 [2] [0] [pt];
    //t->section = 13;
    t->skip_eob_node = 0;
    ++cpi->coef_counts_8x8[2] [0] [pt] [DCT_EOB_TOKEN];
    ++t;
    *tp = t;
    pt = 0; /* 0 <-> all coeff data is zero */
    *a = *l = pt;

}

void vp8_stuff_mb_8x8(VP8_COMP *cpi, MACROBLOCKD *x, TOKENEXTRA **t)
{
    ENTROPY_CONTEXT * A = (ENTROPY_CONTEXT *)x->above_context;
    ENTROPY_CONTEXT * L = (ENTROPY_CONTEXT *)x->left_context;
    int plane_type;
    int b;

    stuff2nd_order_b_8x8(x->block + 24, t, 1, x->frame_type,
                         A + vp8_block2above[24], L + vp8_block2left[24], cpi);
    plane_type = 0;

    for (b = 0; b < 16; b+=4) {
        stuff1st_order_b_8x8(x->block + b, t, plane_type, x->frame_type,
                             A + vp8_block2above[b],
                             L + vp8_block2left[b],
                             A + vp8_block2above[b+1],
                             L + vp8_block2left[b+4],
                             cpi);
        // build coeff context for 8x8 transform
        if(b==0)
        {
          *(A + vp8_block2above[1]) = *(A + vp8_block2above[4]) = *(A + vp8_block2above[5]) = *(A + vp8_block2above[b]);
          *(L + vp8_block2left[1]) = *(L + vp8_block2left[4]) = *(L + vp8_block2left[5]) = *(L + vp8_block2left[b]);
        }
        else if(b==4)
        {
          *(A + vp8_block2above[2]) = *(A + vp8_block2above[3]) = *(A + vp8_block2above[6]) = *(A + vp8_block2above[7]) = *(A + vp8_block2above[b]);
          *(L + vp8_block2left[2]) = *(L + vp8_block2left[3]) = *(L + vp8_block2left[6]) = *(L + vp8_block2left[7]) = *(L + vp8_block2left[b]);
          *(A + vp8_block2above[4]) = *(A + vp8_block2above[1]);
          *(L + vp8_block2left[4]) = *(L + vp8_block2left[1]);
        }
        else if(b==8)
        {
          *(A + vp8_block2above[9]) = *(A + vp8_block2above[12]) = *(A + vp8_block2above[13]) = *(A + vp8_block2above[b]);
          *(L + vp8_block2left[9]) = *(L + vp8_block2left[12]) = *(L + vp8_block2left[13]) = *(L + vp8_block2left[b]);

        }
        else if(b==12)
        {
          *(A + vp8_block2above[10]) = *(A + vp8_block2above[11]) = *(A + vp8_block2above[14]) = *(A + vp8_block2above[15]) = *(A + vp8_block2above[b]);
          *(L + vp8_block2left[10]) = *(L + vp8_block2left[11]) = *(L + vp8_block2left[14]) = *(L + vp8_block2left[15]) = *(L + vp8_block2left[b]);
          *(A + vp8_block2above[12]) = *(A + vp8_block2above[8]);
          *(L + vp8_block2left[12]) = *(L + vp8_block2left[8]);

        }

    }
    /*
    for (b = 0; b < 16; b+=4) {
        stuff1st_order_b_8x8(x->block + b, t, plane_type, x->frame_type,
                             A + vp8_block2above[b],
                             L + vp8_block2left[b], cpi);
        *(A + vp8_block2above[b+1]) = *(A + vp8_block2above[b+2]) = *(A + vp8_block2above[b+3]) =
            *(A + vp8_block2above[b]);
        *(L + vp8_block2left[b+1]) = *(L + vp8_block2left[b+2]) = *(L + vp8_block2left[b+3]) =
            *(L + vp8_block2left[b]);
    }
    */

    for (b = 16; b < 24; b+=4) {
      stuff1st_order_buv_8x8(x->block + b, t, 2, x->frame_type,
                             A + vp8_block2above[b],
                             L + vp8_block2left[b],
                             A + vp8_block2above[b+1],
                             L + vp8_block2left[b+2],
                             cpi);
      *(A + vp8_block2above[b+1]) = *(A + vp8_block2above[b+2]) = *(A + vp8_block2above[b+3]) =
          *(A + vp8_block2above[b]);
      *(L + vp8_block2left[b+1]) = *(L + vp8_block2left[b+2]) = *(L + vp8_block2left[b+3]) =
          *(L + vp8_block2left[b]);
    }
    /*
    for (b = 16; b < 24; b+=4) {
        stuff1st_order_buv_8x8(x->block + b, t, 2, x->frame_type,
                               A + vp8_block2above[b],
                               L + vp8_block2left[b], cpi);
        *(A + vp8_block2above[b+1]) = *(A + vp8_block2above[b+2]) = *(A + vp8_block2above[b+3]) =
            *(A + vp8_block2above[b]);
        *(L + vp8_block2left[b+1]) = *(L + vp8_block2left[b+2]) = *(L + vp8_block2left[b+3]) =
            *(L + vp8_block2left[b]);
    }
    */
}
#endif

static __inline void stuff2nd_order_b
(
    TOKENEXTRA **tp,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,
    VP8_COMP *cpi
)
{
    int pt; /* near block/prev token context index */
    TOKENEXTRA *t = *tp;        /* store tokens starting here */
    VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

    t->Token = DCT_EOB_TOKEN;
    t->context_tree = cpi->common.fc.coef_probs [1] [0] [pt];
    t->skip_eob_node = 0;
    ++cpi->coef_counts       [1] [0] [pt] [DCT_EOB_TOKEN];
    ++t;

    *tp = t;
    pt = 0;
    *a = *l = pt;

}

static __inline void stuff1st_order_b
(
    TOKENEXTRA **tp,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,
    VP8_COMP *cpi
)
{
    int pt; /* near block/prev token context index */
    TOKENEXTRA *t = *tp;        /* store tokens starting here */
    VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

    t->Token = DCT_EOB_TOKEN;
    t->context_tree = cpi->common.fc.coef_probs [0] [1] [pt];
    t->skip_eob_node = 0;
    ++cpi->coef_counts       [0] [1] [pt] [DCT_EOB_TOKEN];
    ++t;
    *tp = t;
    pt = 0; /* 0 <-> all coeff data is zero */
    *a = *l = pt;

}
static __inline
void stuff1st_order_buv
(
    TOKENEXTRA **tp,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,
    VP8_COMP *cpi
)
{
    int pt; /* near block/prev token context index */
    TOKENEXTRA *t = *tp;        /* store tokens starting here */
    VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

    t->Token = DCT_EOB_TOKEN;
    t->context_tree = cpi->common.fc.coef_probs [2] [0] [pt];
    t->skip_eob_node = 0;
    ++cpi->coef_counts[2] [0] [pt] [DCT_EOB_TOKEN];
    ++t;
    *tp = t;
    pt = 0; /* 0 <-> all coeff data is zero */
    *a = *l = pt;

}

void vp8_stuff_mb(VP8_COMP *cpi, MACROBLOCKD *x, TOKENEXTRA **t)
{
    ENTROPY_CONTEXT * A = (ENTROPY_CONTEXT *)x->above_context;
    ENTROPY_CONTEXT * L = (ENTROPY_CONTEXT *)x->left_context;
    int plane_type;
    int b;

    stuff2nd_order_b(t,
                     A + vp8_block2above[24], L + vp8_block2left[24], cpi);
    plane_type = 0;

    for (b = 0; b < 16; b++)
        stuff1st_order_b(t,
                         A + vp8_block2above[b],
                         L + vp8_block2left[b], cpi);

    for (b = 16; b < 24; b++)
        stuff1st_order_buv(t,
                           A + vp8_block2above[b],
                           L + vp8_block2left[b], cpi);

}
void vp8_fix_contexts(MACROBLOCKD *x)
{
    /* Clear entropy contexts for Y2 blocks */
    if (x->mode_info_context->mbmi.mode != B_PRED && x->mode_info_context->mbmi.mode != SPLITMV)
    {
        vpx_memset(x->above_context, 0, sizeof(ENTROPY_CONTEXT_PLANES));
        vpx_memset(x->left_context, 0, sizeof(ENTROPY_CONTEXT_PLANES));
    }
    else
    {
        vpx_memset(x->above_context, 0, sizeof(ENTROPY_CONTEXT_PLANES)-1);
        vpx_memset(x->left_context, 0, sizeof(ENTROPY_CONTEXT_PLANES)-1);
    }

}
