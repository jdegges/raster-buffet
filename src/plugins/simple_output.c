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
#include <string.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include "image.h"
#include "plugin.h"

/* function definitions */
int freeimage_query (plugin_stage   stage,
                     plugin_info**  pi);

int fi_output_init (plugin_context* ctx,
                    int             thread_id,
                    char*           args);
int fi_output_exec (plugin_context* ctx,
                    int             thread_id,
                    image_t**        src_data,
                    image_t**        dst_data);
int fi_output_exit (plugin_context* ctx,
                    int             thread_id);


/* input plugin configuration */
static const data_fmt input_src_fmt[] = {FMT_LIST, -1};
static const data_fmt input_dst_fmt[] = {
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

/* output plugin configuration */
static const char output_name[] = "simple_output_output";
static plugin_info pi_freeimage_output = {.stage=PLUGIN_STAGE_OUTPUT,
                                          .type=PLUGIN_TYPE_ASYNC,
                                          .src_fmt=input_dst_fmt,
                                          .dst_fmt=input_src_fmt,
                                          .name=output_name,
                                          .init=fi_output_init,
                                          .exit=fi_output_exit,
                                          .exec=fi_output_exec};

int simple_output_query (plugin_stage stage, plugin_info** pi)
{
    *pi = NULL;
    if (PLUGIN_STAGE_OUTPUT == stage) {
        *pi = &pi_freeimage_output;
        return 0;
    }
    return -1;
}

typedef struct fi_output_context {
    FILE*   filep;
    char*   filen;
    int     references;
} fi_output_context;

int fi_output_init (plugin_context* ctx,
                    int             thread_id,
                    char*           args)
{
    fi_output_context* c;

    pthread_mutex_lock (&ctx->mutex);

    if (ctx->data == NULL) {
        if (NULL == (c = malloc (sizeof(fi_output_context)))) {
            return -1;
        }

        parse_args (args, 0, "rsc", &c->filen);
        if (NULL == c->filen || '-' == *c->filen) {
            c->filep = stdout;
        } else {
            if (NULL == (c->filep = fopen (c->filen, "w"))) {
                return -1;
            }
        }

        c->references = 0;
        ctx->data = c;
    }

    if (NULL == (c = (fi_output_context*) ctx->data))
    {
        return -1;
    }

    c->references++;

    pthread_mutex_unlock (&ctx->mutex);
    return 0;
}

int fi_output_exec (plugin_context* ctx,
                    int             thread_id,
                    image_t**        src_data,
                    image_t**        dst_data)
{
    fi_output_context* c;
    image_t* im;
    int size;

    if (NULL == (c = (fi_output_context*) ctx->data) ||
        NULL == (im = *src_data)) {
        return -1;
    }

    pthread_mutex_lock (&ctx->mutex);

    if ((uintmax_t) im->size != (uintmax_t) fwrite (im->pix,
                                                    sizeof(uint8_t),
                                                    im->size,
                                                    c->filep))
    {
        return -1;
    }

    pthread_mutex_unlock (&ctx->mutex);

    return 0;
}

int fi_output_exit (plugin_context* ctx,
                    int             thread_id)
{
    fi_output_context* c;
    int i;

    if (NULL == (c = (fi_output_context*) ctx->data))
    {
        return -1;
    }

    pthread_mutex_lock (&ctx->mutex);

    if (--c->references) {
        pthread_mutex_unlock (&ctx->mutex);
        return 0;
    }

    fclose (c->filep);
    free (c->filen);
    free (c);
    ctx->data = NULL;

    pthread_mutex_unlock (&ctx->mutex);
    return 0;
}
