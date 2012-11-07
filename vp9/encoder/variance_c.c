/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "variance.h"
#include "vp9/common/filter.h"
#include "vp9/common/subpelvar.h"


unsigned int vp9_get_mb_ss_c(const short *src_ptr) {
  unsigned int i, sum = 0;

  for (i = 0; i < 256; i++) {
    sum += (src_ptr[i] * src_ptr[i]);
  }

  return sum;
}



#if CONFIG_SUPERBLOCKS
unsigned int vp9_variance32x32_c(const unsigned char *src_ptr,
                                 int  source_stride,
                                 const unsigned char *ref_ptr,
                                 int  recon_stride,
                                 unsigned int *sse) {
  unsigned int var;
  int avg;

  variance(src_ptr, source_stride, ref_ptr, recon_stride, 32, 32, &var, &avg);
  *sse = var;
  return (var - ((avg * avg) >> 10));
}
#endif

unsigned int vp9_variance16x16_c(const unsigned char *src_ptr,
                                 int  source_stride,
                                 const unsigned char *ref_ptr,
                                 int  recon_stride,
                                 unsigned int *sse) {
  unsigned int var;
  int avg;

  variance(src_ptr, source_stride, ref_ptr, recon_stride, 16, 16, &var, &avg);
  *sse = var;
  return (var - ((avg * avg) >> 8));
}

unsigned int vp9_variance8x16_c(const unsigned char *src_ptr,
                                int  source_stride,
                                const unsigned char *ref_ptr,
                                int  recon_stride,
                                unsigned int *sse) {
  unsigned int var;
  int avg;

  variance(src_ptr, source_stride, ref_ptr, recon_stride, 8, 16, &var, &avg);
  *sse = var;
  return (var - ((avg * avg) >> 7));
}

unsigned int vp9_variance16x8_c(const unsigned char *src_ptr,
                                int  source_stride,
                                const unsigned char *ref_ptr,
                                int  recon_stride,
                                unsigned int *sse) {
  unsigned int var;
  int avg;

  variance(src_ptr, source_stride, ref_ptr, recon_stride, 16, 8, &var, &avg);
  *sse = var;
  return (var - ((avg * avg) >> 7));
}


unsigned int vp9_variance8x8_c(const unsigned char *src_ptr,
                               int  source_stride,
                               const unsigned char *ref_ptr,
                               int  recon_stride,
                               unsigned int *sse) {
  unsigned int var;
  int avg;

  variance(src_ptr, source_stride, ref_ptr, recon_stride, 8, 8, &var, &avg);
  *sse = var;
  return (var - ((avg * avg) >> 6));
}

unsigned int vp9_variance4x4_c(const unsigned char *src_ptr,
                               int  source_stride,
                               const unsigned char *ref_ptr,
                               int  recon_stride,
                               unsigned int *sse) {
  unsigned int var;
  int avg;

  variance(src_ptr, source_stride, ref_ptr, recon_stride, 4, 4, &var, &avg);
  *sse = var;
  return (var - ((avg * avg) >> 4));
}


unsigned int vp9_mse16x16_c(const unsigned char *src_ptr,
                            int  source_stride,
                            const unsigned char *ref_ptr,
                            int  recon_stride,
                            unsigned int *sse) {
  unsigned int var;
  int avg;

  variance(src_ptr, source_stride, ref_ptr, recon_stride, 16, 16, &var, &avg);
  *sse = var;
  return var;
}


unsigned int vp9_sub_pixel_variance4x4_c(const unsigned char  *src_ptr,
                                         int  src_pixels_per_line,
                                         int  xoffset,
                                         int  yoffset,
                                         const unsigned char *dst_ptr,
                                         int dst_pixels_per_line,
                                         unsigned int *sse) {
  unsigned char  temp2[20 * 16];
  const short *HFilter, *VFilter;
  unsigned short FData3[5 * 4]; // Temp data bufffer used in filtering

  HFilter = vp9_bilinear_filters[xoffset];
  VFilter = vp9_bilinear_filters[yoffset];

  // First filter 1d Horizontal
  var_filter_block2d_bil_first_pass(src_ptr, FData3, src_pixels_per_line, 1, 5, 4, HFilter);

  // Now filter Verticaly
  var_filter_block2d_bil_second_pass(FData3, temp2, 4,  4,  4,  4, VFilter);

  return vp9_variance4x4_c(temp2, 4, dst_ptr, dst_pixels_per_line, sse);
}


unsigned int vp9_sub_pixel_variance8x8_c(const unsigned char  *src_ptr,
                                         int  src_pixels_per_line,
                                         int  xoffset,
                                         int  yoffset,
                                         const unsigned char *dst_ptr,
                                         int dst_pixels_per_line,
                                         unsigned int *sse) {
  unsigned short FData3[9 * 8]; // Temp data bufffer used in filtering
  unsigned char  temp2[20 * 16];
  const short *HFilter, *VFilter;

  HFilter = vp9_bilinear_filters[xoffset];
  VFilter = vp9_bilinear_filters[yoffset];

  var_filter_block2d_bil_first_pass(src_ptr, FData3, src_pixels_per_line, 1, 9, 8, HFilter);
  var_filter_block2d_bil_second_pass(FData3, temp2, 8, 8, 8, 8, VFilter);

  return vp9_variance8x8_c(temp2, 8, dst_ptr, dst_pixels_per_line, sse);
}

unsigned int vp9_sub_pixel_variance16x16_c(const unsigned char  *src_ptr,
                                           int  src_pixels_per_line,
                                           int  xoffset,
                                           int  yoffset,
                                           const unsigned char *dst_ptr,
                                           int dst_pixels_per_line,
                                           unsigned int *sse) {
  unsigned short FData3[17 * 16]; // Temp data bufffer used in filtering
  unsigned char  temp2[20 * 16];
  const short *HFilter, *VFilter;

  HFilter = vp9_bilinear_filters[xoffset];
  VFilter = vp9_bilinear_filters[yoffset];

  var_filter_block2d_bil_first_pass(src_ptr, FData3, src_pixels_per_line, 1, 17, 16, HFilter);
  var_filter_block2d_bil_second_pass(FData3, temp2, 16, 16, 16, 16, VFilter);

  return vp9_variance16x16_c(temp2, 16, dst_ptr, dst_pixels_per_line, sse);
}

#if CONFIG_SUPERBLOCKS
unsigned int vp9_sub_pixel_variance32x32_c(const unsigned char  *src_ptr,
                                           int  src_pixels_per_line,
                                           int  xoffset,
                                           int  yoffset,
                                           const unsigned char *dst_ptr,
                                           int dst_pixels_per_line,
                                           unsigned int *sse) {
  unsigned short FData3[33 * 32]; // Temp data bufffer used in filtering
  unsigned char  temp2[36 * 32];
  const short *HFilter, *VFilter;

  HFilter = vp9_bilinear_filters[xoffset];
  VFilter = vp9_bilinear_filters[yoffset];

  var_filter_block2d_bil_first_pass(src_ptr, FData3, src_pixels_per_line, 1, 33, 32, HFilter);
  var_filter_block2d_bil_second_pass(FData3, temp2, 32, 32, 32, 32, VFilter);

  return vp9_variance32x32_c(temp2, 32, dst_ptr, dst_pixels_per_line, sse);
}
#endif

unsigned int vp9_variance_halfpixvar16x16_h_c(const unsigned char *src_ptr,
                                              int  source_stride,
                                              const unsigned char *ref_ptr,
                                              int  recon_stride,
                                              unsigned int *sse) {
  return vp9_sub_pixel_variance16x16_c(src_ptr, source_stride, 8, 0,
                                       ref_ptr, recon_stride, sse);
}

#if CONFIG_SUPERBLOCKS
unsigned int vp9_variance_halfpixvar32x32_h_c(const unsigned char *src_ptr,
                                              int  source_stride,
                                              const unsigned char *ref_ptr,
                                              int  recon_stride,
                                              unsigned int *sse) {
  return vp9_sub_pixel_variance32x32_c(src_ptr, source_stride, 8, 0,
                                       ref_ptr, recon_stride, sse);
}
#endif


unsigned int vp9_variance_halfpixvar16x16_v_c(const unsigned char *src_ptr,
                                              int  source_stride,
                                              const unsigned char *ref_ptr,
                                              int  recon_stride,
                                              unsigned int *sse) {
  return vp9_sub_pixel_variance16x16_c(src_ptr, source_stride, 0, 8,
                                       ref_ptr, recon_stride, sse);
}

#if CONFIG_SUPERBLOCKS
unsigned int vp9_variance_halfpixvar32x32_v_c(const unsigned char *src_ptr,
                                              int  source_stride,
                                              const unsigned char *ref_ptr,
                                              int  recon_stride,
                                              unsigned int *sse) {
  return vp9_sub_pixel_variance32x32_c(src_ptr, source_stride, 0, 8,
                                       ref_ptr, recon_stride, sse);
}
#endif

unsigned int vp9_variance_halfpixvar16x16_hv_c(const unsigned char *src_ptr,
                                               int  source_stride,
                                               const unsigned char *ref_ptr,
                                               int  recon_stride,
                                               unsigned int *sse) {
  return vp9_sub_pixel_variance16x16_c(src_ptr, source_stride, 8, 8,
                                       ref_ptr, recon_stride, sse);
}

#if CONFIG_SUPERBLOCKS
unsigned int vp9_variance_halfpixvar32x32_hv_c(const unsigned char *src_ptr,
                                               int  source_stride,
                                               const unsigned char *ref_ptr,
                                               int  recon_stride,
                                               unsigned int *sse) {
  return vp9_sub_pixel_variance32x32_c(src_ptr, source_stride, 8, 8,
                                       ref_ptr, recon_stride, sse);
}
#endif

unsigned int vp9_sub_pixel_mse16x16_c(const unsigned char  *src_ptr,
                                      int  src_pixels_per_line,
                                      int  xoffset,
                                      int  yoffset,
                                      const unsigned char *dst_ptr,
                                      int dst_pixels_per_line,
                                      unsigned int *sse) {
  vp9_sub_pixel_variance16x16_c(src_ptr, src_pixels_per_line,
                                xoffset, yoffset, dst_ptr,
                                dst_pixels_per_line, sse);
  return *sse;
}

#if CONFIG_SUPERBLOCKS
unsigned int vp9_sub_pixel_mse32x32_c(const unsigned char  *src_ptr,
                                      int  src_pixels_per_line,
                                      int  xoffset,
                                      int  yoffset,
                                      const unsigned char *dst_ptr,
                                      int dst_pixels_per_line,
                                      unsigned int *sse) {
  vp9_sub_pixel_variance32x32_c(src_ptr, src_pixels_per_line,
                                xoffset, yoffset, dst_ptr,
                                dst_pixels_per_line, sse);
  return *sse;
}
#endif

unsigned int vp9_sub_pixel_variance16x8_c(const unsigned char  *src_ptr,
                                          int  src_pixels_per_line,
                                          int  xoffset,
                                          int  yoffset,
                                          const unsigned char *dst_ptr,
                                          int dst_pixels_per_line,
                                          unsigned int *sse) {
  unsigned short FData3[16 * 9];  // Temp data bufffer used in filtering
  unsigned char  temp2[20 * 16];
  const short *HFilter, *VFilter;

  HFilter = vp9_bilinear_filters[xoffset];
  VFilter = vp9_bilinear_filters[yoffset];

  var_filter_block2d_bil_first_pass(src_ptr, FData3, src_pixels_per_line, 1, 9, 16, HFilter);
  var_filter_block2d_bil_second_pass(FData3, temp2, 16, 16, 8, 16, VFilter);

  return vp9_variance16x8_c(temp2, 16, dst_ptr, dst_pixels_per_line, sse);
}

unsigned int vp9_sub_pixel_variance8x16_c(const unsigned char  *src_ptr,
                                          int  src_pixels_per_line,
                                          int  xoffset,
                                          int  yoffset,
                                          const unsigned char *dst_ptr,
                                          int dst_pixels_per_line,
                                          unsigned int *sse) {
  unsigned short FData3[9 * 16];  // Temp data bufffer used in filtering
  unsigned char  temp2[20 * 16];
  const short *HFilter, *VFilter;

  HFilter = vp9_bilinear_filters[xoffset];
  VFilter = vp9_bilinear_filters[yoffset];

  var_filter_block2d_bil_first_pass(src_ptr, FData3, src_pixels_per_line,
                                    1, 17, 8, HFilter);
  var_filter_block2d_bil_second_pass(FData3, temp2, 8, 8, 16, 8, VFilter);

  return vp9_variance8x16_c(temp2, 8, dst_ptr, dst_pixels_per_line, sse);
}

