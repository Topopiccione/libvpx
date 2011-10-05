/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include "dct.h"
#include "vpx_ports/mem.h"

unsigned int vp8_satd16x16_c(const unsigned char *src_ptr,
                             int  src_stride,
                             const unsigned char *ref_ptr,
                             int  ref_stride,
                             unsigned int *psatd)
{
    int r, c, i;
    unsigned int satd = 0;
    DECLARE_ALIGNED(16, short, diff_in[256]);
    DECLARE_ALIGNED(16, short, diff_out[16]);
    short *in;

    for (r = 0; r < 16; r++)
    {
        for (c = 0; c < 16; c++)
        {
            diff_in[r * 16 + c] = src_ptr[c] - ref_ptr[c];
        }
        src_ptr += src_stride;
        ref_ptr += ref_stride;
    }

    in = diff_in;
    for (r = 0; r < 16; r += 4)
    {
        for (c = 0; c < 16; c+=4)
        {
            vp8_short_walsh4x4_c(in + c, diff_out, 32);
            for(i = 0; i < 16; i++)
                satd += abs(diff_out[i]);
        }
        in += 64;
    }

    if (psatd)
        *psatd = satd;

    return satd;
}
