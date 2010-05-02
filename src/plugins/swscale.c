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

#define _BSD_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <assert.h>

#include <unistd.h>
#include <pthread.h>

#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libavcodec/avcodec.h>

#include <loomlib/async_queue.h>

#include <image.h>
#include <plugin.h>

typedef struct swscale_decode_context {
    struct async_queue* sws_context_queue;
    int references;

    int dst_width;
    int dst_height;
    data_fmt dst_native_fmt;
    enum PixelFormat dst_sws_fmt;
} swscale_decode_context;

static enum PixelFormat supported_fmt_table[FMT_LIST] = {-1};


/* function definitions */
int swscale_query (plugin_stage   stage,
                   plugin_info**  pi);

int swscale_decode_init (plugin_context*  ctx,
                         int              thread_id,
                         char*            args);
int swscale_decode_exec (plugin_context*  ctx,
                         int              thread_id,
                         image_t**        src_data,
                         image_t**        dst_data);
int swscale_decode_exit (plugin_context*  ctx,
                         int              thread_id);


/* decode plugin configuration */
static const char decode_name[] = "swscale_decode";
static const data_fmt decode_src_fmt[] = {FMT_LIST, -1};
static const data_fmt decode_dst_fmt[] = {
    FMT_BMP,    FMT_CUT,    FMT_DDS,
    FMT_EXR,    FMT_FAXG3,  FMT_GIF,
    FMT_HDR,    FMT_ICO,    FMT_IFF,
    FMT_J2K,    FMT_JNG,    FMT_JP2,
    FMT_KOALA,  FMT_MNG,    FMT_PBM,
    FMT_PBMRAW, FMT_PCD,    FMT_PCX,
    FMT_PFM,    FMT_PGM,    FMT_PGMRAW,
    FMT_PICT,   FMT_PNG,    FMT_PPM,
    FMT_PPMRAW, FMT_PSD,    FMT_RAS,
    FMT_RAW,    FMT_SGI,    FMT_TARGA,
    FMT_TIFF,   FMT_WBMP,   FMT_XBM,
    FMT_XPM,    FMT_JPEG,   -1};
static plugin_info pi_swscale_decode = {.stage=PLUGIN_STAGE_INPUT,
                                        .type=PLUGIN_TYPE_ASYNC,
                                        .src_fmt=decode_src_fmt,
                                        .dst_fmt=decode_dst_fmt,
                                        .name=decode_name,
                                        .init=swscale_decode_init,
                                        .exit=swscale_decode_exit,
                                        .exec=swscale_decode_exec};


int swscale_query (plugin_stage stage, plugin_info** pi)
{
    *pi = NULL;
    switch (stage) {
        case PLUGIN_STAGE_DECODE:
            *pi = &pi_swscale_decode;
            break;
        default:
            return -1;
    }
    return 0;
}

static int stofmt (char* str, data_fmt* native_fmt, enum PixelFormat* sws_fmt)
{
    if (NULL == str) {
        *native_fmt = FMT_NONE;
        *sws_fmt = PIX_FMT_NONE;
        return -1;
    }

    if (0 == strcasecmp (str, "PAL8"))
        *native_fmt = FMT_PAL8;
    else if (0 == strcasecmp (str, "BGR24"))
        *native_fmt = FMT_BGR24;
    else if (0 == strcasecmp (str, "BGR32"))
        *native_fmt = FMT_BGR32;
    else if (0 == strcasecmp (str, "BGR32_1"))
        *native_fmt = FMT_BGR32_1;
    else if (0 == strcasecmp (str, "RGB24"))
        *native_fmt = FMT_RGB24;
    else if (0 == strcasecmp (str, "RGB32"))
        *native_fmt = FMT_RGB32;
    else if (0 == strcasecmp (str, "RGB32_1"))
        *native_fmt = FMT_RGB32_1;
    else if (0 == strcasecmp (str, "YUYV"))
        *native_fmt = FMT_YUYV;
    else if (0 == strcasecmp (str, "UYVY"))
        *native_fmt = FMT_UYVY;
    else
        *native_fmt = FMT_NONE;

    *sws_fmt = supported_fmt_table[*native_fmt];

    return 0;
}

int swscale_decode_init (plugin_context*  ctx,
                         int              thread_id,
                         char*            args)
{
    swscale_decode_context* c;
    int ret_val = -1;

    (void) thread_id;

    pthread_mutex_lock (&ctx->mutex);

    if (ctx->data == NULL) {
        char* param;
        data_fmt dst_fmt;
        enum PixelFormat sws_fmt;
        int dst_width;
        int dst_height;

        /* set up state shared between all threads */
        if (NULL == (c = calloc (1, sizeof *c))) {
            error_exit ("Out of memory");
        }

        if (NULL == (c->sws_context_queue = async_queue_new())) {
            error_exit ("Failed to create async queue");
        }

        parse_args (args, 0, "width",  &param);
        if (NULL == param || (dst_width = atoi (param)) <= 0) {
            dst_width = -1;
        }
        free (param);

        parse_args (args, 0, "height", &param);
        if (NULL == param || (dst_height = atoi (param)) <= 0) {
            dst_height = -1;
        }
        free (param);

        parse_args (args, 0, "fmt", &param);
        if (NULL == param || stofmt (param, &dst_fmt, &sws_fmt) < 0) {
            dst_fmt = FMT_RGB24;
            sws_fmt = PIX_FMT_RGB24;
        }
        free (param);

        c->dst_width = dst_width;
        c->dst_height = dst_height;
        c->dst_native_fmt = dst_fmt;
        c->dst_sws_fmt = sws_fmt;
        ctx->data = c;

        supported_fmt_table[FMT_NONE] = PIX_FMT_NONE;
        supported_fmt_table[FMT_PAL8] = PIX_FMT_PAL8;
        supported_fmt_table[FMT_BGR24] = PIX_FMT_BGR24;
        supported_fmt_table[FMT_BGR32] = PIX_FMT_BGR32;
        supported_fmt_table[FMT_BGR32_1] = PIX_FMT_BGR32_1;
        supported_fmt_table[FMT_RGB24] = PIX_FMT_RGB24;
        supported_fmt_table[FMT_RGB32] = PIX_FMT_RGB32;
        supported_fmt_table[FMT_RGB32_1] = PIX_FMT_RGB32_1;
        supported_fmt_table[FMT_YUYV] = PIX_FMT_YUYV422;
        supported_fmt_table[FMT_UYVY] = PIX_FMT_UYVY422;
    }

    if (NULL == (c = (swscale_decode_context*) ctx->data) ||
        NULL == c->sws_context_queue)
    {
        error_exit ("Context is not properly set");
    }

    c->references++;
    ret_val = 0;

    pthread_mutex_unlock (&ctx->mutex);

exit:
    return ret_val;
}


int swscale_decode_exit (plugin_context*  ctx,
                         int              thread_id)
{
    swscale_decode_context* c;

    (void) thread_id;

    pthread_mutex_lock (&ctx->mutex);

    assert (NULL != (c = ctx->data));

    if (!--c->references) {
        struct SwsContext* sws_context;
        while (NULL != (sws_context = async_queue_pop (c->sws_context_queue,
                                                       false)))
        {
            sws_freeContext (sws_context);
        }

        async_queue_free (c->sws_context_queue);
        free (c);
        ctx->data = NULL;
    }

    pthread_mutex_unlock (&ctx->mutex);
     return 0;
}

static enum PixelFormat native_to_sws (data_fmt fmt)
{
    return supported_fmt_table[fmt];
}

int swscale_decode_exec (plugin_context*  ctx,
                         int              thread_id,
                         image_t**        src_data,
                         image_t**        dst_data)
{
    swscale_decode_context* c;
    struct SwsContext* sws_context;
    AVPicture src_picture;
    AVPicture dst_picture;
    int ret_val = -1;

    image_t* sim;
    uint8_t* pix;

    int dst_width;
    int dst_height;

    (void) thread_id;

    if (NULL == (sim = *src_data) || NULL != *dst_data) {
        error_exit ("Bad src/dst data pointers");
    }

    assert (NULL != (c = ctx->data));
    assert (NULL != c->sws_context_queue);

    if (c->dst_width <= 0 || c->dst_height <= 0) {
        dst_width = sim->width;
        dst_height = sim->height;
    } else {
        dst_width = c->dst_width;
        dst_height = c->dst_height;
    }

    if (NULL == (sws_context = async_queue_pop (c->sws_context_queue, false)))
    {
        sws_context = sws_getContext (sim->width, sim->height, native_to_sws (sim->fmt),
                                      dst_width, dst_height, c->dst_sws_fmt,
                                      SWS_BICUBIC, NULL, NULL, NULL);
        if (NULL == sws_context) {
            error_exit ( "Error creating sws_context");
        }
        
    }

    if (NULL == (pix = malloc ((sizeof *pix) * dst_width * dst_height * 3))) {
        error_exit ("Out of memory");
    }

    avpicture_fill (&src_picture, sim->pix, native_to_sws (sim->fmt), sim->width, sim->height);
    avpicture_fill (&dst_picture, pix, c->dst_sws_fmt, dst_width, dst_height);

    if (dst_height != sws_scale (sws_context,
                                 (const uint8_t**) src_picture.data, src_picture.linesize,
                                 0, dst_height,
                                 dst_picture.data, dst_picture.linesize))
    {
        error_exit ("sws_scale failed to convert src->dst");
    }

    assert (async_queue_push (c->sws_context_queue, sws_context));

    if (sim->ext_data && sim->ext_free) {
        sim->ext_free (sim->ext_data);
        sim->ext_data = NULL;
        sim->ext_free = NULL;
    } else {
        free (sim->pix);
    }

    sim->fmt = c->dst_native_fmt;
    sim->pix = pix;
    sim->bpp = 24;

    *dst_data = sim;
    *src_data = NULL;

    ret_val = 0;

exit:
    return ret_val;
}
