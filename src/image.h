/******************************************************************************
 * Copyright (c) 2010 Joey Degges
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *****************************************************************************/

#ifndef __H_RB_IMAGE_H
#define __H_RB_IMAGE_H

#include <stdlib.h>
#include <inttypes.h>

typedef enum {
/* originally inspired from v4l, v4l2, and ffmpeg fmt lists */
    FMT_NONE,           FMT_BGR8,           FMT_RGB444,
    FMT_RGB555,         FMT_RGB565,         FMT_BGR555,
    FMT_BGR565,         FMT_BGR24,          FMT_RGB24,
    FMT_BGR32,          FMT_RGB32,          FMT_GREY8,
    FMT_GREY16,         FMT_PAL8,
    FMT_YVU410,         FMT_YVU420,         FMT_YUYV,
    FMT_UYVY,           FMT_YUV422P,        FMT_YUV411P,
    FMT_Y41P,           FMT_YUV444,         FMT_YUV555,
    FMT_YUV565,         FMT_YUV32,          FMT_NV12,
    FMT_NV21,           FMT_YUV410P,        FMT_YUV420P,
    FMT_YUYV422,        FMT_HI240,          FMT_HM12,
    FMT_SBGGR8,         FMT_SGBRG8,         FMT_SBGGR16,
    FMT_MJPEG,          FMT_JPEG,           FMT_DV,
    FMT_MPEG,           FMT_WNVA,           FMT_SN9C10X,
    FMT_PWC1,           FMT_PWC2,           FMT_ET61X251,
    FMT_SPCA501,        FMT_SPCA505,        FMT_SPCA508,
    FMT_SPCA561,        FMT_PAC207,         FMT_PJPG,
    FMT_YVYU,           FMT_RAW,
    FMT_YUV444P,        FMT_MONOWHITE,
    FMT_MONOBLACK,      FMT_YUVJ420P,       FMT_YUVJ422P,
    FMT_YUVJ444P,       FMT_XVMC_MPEG2_MC,  FMT_XVMC_MPEG2_IDCT,
    FMT_BGR4,           FMT_BGR4_BYTE,      FMT_RGB8,
    FMT_RGB4,           FMT_RGB4_BYTE,      FMT_RGB32_1,
    FMT_BGR32_1,        FMT_YUV440P,        FMT_YUVJ440P,
    FMT_YUVA420P,       FMT_VDPAU_H264,     FMT_VDPAU_MPEG1,
    FMT_VDPAU_MPEG2,    FMT_VDPAU_WMV3,     FMT_VDPAU_VC1,
    FMT_RGB48BE,        FMT_RGB48LE,        FMT_VAAPI_MOCO,
    FMT_VAAPI_IDCT,     FMT_VAAPI_VLD,

/* originally inspired from FreeImage's FIF */
    FMT_BMP,            FMT_CUT,            FMT_DDS,
    FMT_EXR,            FMT_FAXG3,          FMT_GIF,
    FMT_HDR,            FMT_ICO,            FMT_IFF,
    FMT_J2K,            FMT_JNG,            FMT_JP2,
    FMT_KOALA,          FMT_MNG,            FMT_PBM,
    FMT_PBMRAW,         FMT_PCD,            FMT_PCX,
    FMT_PFM,            FMT_PGM,            FMT_PGMRAW,
    FMT_PICT,           FMT_PNG,            FMT_PPM,
    FMT_PPMRAW,         FMT_PSD,            FMT_RAS,
    FMT_SGI,            FMT_TARGA,
    FMT_TIFF,           FMT_WBMP,           FMT_XBM,
    FMT_XPM,

/* used to specify a list of single image files */
    FMT_LIST
} data_fmt;

typedef struct image_t {
    uint8_t* pix;
    int64_t width;
    int64_t height;
    int64_t bpp;
    int64_t size;
    int64_t frame;
    data_fmt fmt;
    void* ext_data;
    void (*ext_free)(void*);
} image_t;

void image_close (image_t* im) {
    if (im) {
        if (im->ext_data && im->ext_free) {
            im->ext_free (im->ext_data);
        } else {
            free (im->pix);
        }

        free (im);
        im = NULL;
    }
}
#endif
