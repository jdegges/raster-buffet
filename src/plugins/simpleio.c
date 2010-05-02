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
#include <errno.h>

#include "image.h"
#include "plugin.h"

/* function definitions */
int simpleio_query (plugin_stage   stage,
                    plugin_info**  pi);

int sio_input_init (plugin_context* ctx,
                    int             thread_id,
                    char*           args);
int sio_input_exec (plugin_context* ctx,
                    int             thread_id,
                    image_t**       src_data,
                    image_t**       dst_data);
int sio_input_exit (plugin_context* ctx,
                    int             thread_id);

int sio_output_init (plugin_context* ctx,
                     int             thread_id,
                     char*           args);
int sio_output_exec (plugin_context* ctx,
                     int             thread_id,
                     image_t**        src_data,
                     image_t**        dst_data);
int sio_output_exit (plugin_context* ctx,
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
static const char input_name[] = "simpleio_input";
static plugin_info pi_simpleio_input = {.stage=PLUGIN_STAGE_INPUT,
                                        .type=PLUGIN_TYPE_ASYNC,
                                        .src_fmt=input_src_fmt,
                                        .dst_fmt=input_dst_fmt,
                                        .name=input_name,
                                        .init=sio_input_init,
                                        .exit=sio_input_exit,
                                        .exec=sio_input_exec};

/* output plugin configuration */
static const char output_name[] = "simpleio_output";
static plugin_info pi_simpleio_output = {.stage=PLUGIN_STAGE_OUTPUT,
                                         .type=PLUGIN_TYPE_ASYNC,
                                         .src_fmt=input_dst_fmt,
                                         .dst_fmt=input_src_fmt,
                                         .name=output_name,
                                         .init=sio_output_init,
                                         .exit=sio_output_exit,
                                         .exec=sio_output_exec};

int simpleio_query (plugin_stage stage, plugin_info** pi)
{
    switch (stage) {
        case PLUGIN_STAGE_INPUT:
            *pi = &pi_simpleio_input;
            return 0;
        case PLUGIN_STAGE_OUTPUT:
            *pi = &pi_simpleio_output;
            return 0;
        default:
            *pi = NULL;
            return -1;
    }
}

typedef struct sio_input_context {
    FILE* filep;
    char* filen;
    int   references;
} sio_input_context;

int sio_input_init (plugin_context* ctx,
                    int             thread_id,
                    char*           args)
{
    sio_input_context* c;
    int ret_val = -1;

    pthread_mutex_lock (&ctx->mutex);

    if (ctx->data == NULL) {
        if (NULL == (c = malloc (sizeof *c))) {
            error_exit ("Out of memory");
        }

        parse_args (args, 0, "rsc", &c->filen);
        if (NULL == c->filen || '-' == *c->filen) {
            c->filep = stdin;
        } else {
            if (NULL == (c->filep = fopen(c->filen, "r"))) {
                error_exit ("Unable to open %s for reading", c->filen);
            }
        }

        c->references = 0;
        ctx->data = c;
    }

    if (NULL == (c = (sio_input_context*) ctx->data)) {
        error_exit ("Invalid context");
    }

    c->references++;
    ret_val = 0;

exit:
    pthread_mutex_unlock (&ctx->mutex);
    return ret_val;
}

int sio_input_exec (plugin_context* ctx,
                    int             thread_id,
                    image_t**       src_data,
                    image_t**       dst_data)
{
    sio_input_context* c;
    char* path;
    image_t* im;
    size_t size = 1031;
    int ret_val = -1;

    pthread_mutex_lock (&ctx->mutex);

    if (NULL == (c = (sio_input_context*) ctx->data)) {
        error_exit ("Invalid context");
    }

    if (NULL == c->filep) {
        error_exit ("Invalid file pointer");
    }

    if (NULL == (im = calloc (1, sizeof *im))) {
        error_exit ("Out of memory");
    }

    im->width = im->height = im->bpp = -1;

    im->size = 0;
    if (NULL == (im->pix = malloc (size))) {
        error_exit ("Out of memory");
    }

    while (size == fread (im->pix + im->size, 1, size, c->filep)) {
        im->size += size;
        if (NULL == (im->pix = realloc (im->pix, im->size + size))) {
            error_exit ("Out of memory");
        }
    }
    im->size += size;

    if (0 != fclose (c->filep)) {
        error_exit ("%s", strerror (errno));
    }
    c->filep = NULL;

    *dst_data = im;
    ret_val = 0;

exit:
    pthread_mutex_unlock (&ctx->mutex);
    return ret_val;
}

int sio_input_exit (plugin_context* ctx,
                    int             thread_id)
{
    sio_input_context* c;
    int i;
    int ret_val = -1;

    if (NULL == (c = (sio_input_context*) ctx->data)) {
        error_exit ("Invalid context");
    }

    pthread_mutex_lock (&ctx->mutex);

    if (--c->references) {
        ret_val = 0;
        goto exit;
    }

    free (c->filen);
    free (c);

    ctx->data = NULL;
    ret_val = 0;

exit:
    pthread_mutex_unlock (&ctx->mutex);
    return ret_val;
}


typedef struct sio_output_context {
    FILE*   filep;
    char*   filen;
    int     references;
} fi_output_context;

int sio_output_init (plugin_context* ctx,
                     int             thread_id,
                     char*           args)
{
    fi_output_context* c;
    int ret_val = -1;

    pthread_mutex_lock (&ctx->mutex);

    if (ctx->data == NULL) {
        if (NULL == (c = malloc (sizeof(fi_output_context)))) {
            error_exit ("Out of memory");
        }

        parse_args (args, 0, "rsc", &c->filen);
        if (NULL == c->filen || '-' == *c->filen) {
            c->filep = stdout;
        } else {
            if (NULL == (c->filep = fopen (c->filen, "w"))) {
                error_exit ("Unable to open %s for writing\n", c->filen);
            }
        }

        c->references = 0;
        ctx->data = c;
    }

    if (NULL == (c = (fi_output_context*) ctx->data)) {
        error_exit ("Invalid context");
    }

    c->references++;
    ret_val = 0;

exit:
    pthread_mutex_unlock (&ctx->mutex);
    return ret_val;
}

int sio_output_exec (plugin_context* ctx,
                     int             thread_id,
                     image_t**        src_data,
                     image_t**        dst_data)
{
    fi_output_context* c;
    image_t* im;
    int size;
    int ret_val = -1;

    pthread_mutex_lock (&ctx->mutex);

    if (NULL == (c = (fi_output_context*) ctx->data) ||
        NULL == (im = *src_data))
    {
        error_exit ("Invalid context");
    }

    if ((uintmax_t) im->size != (uintmax_t) fwrite (im->pix,
                                                    sizeof(uint8_t),
                                                    im->size,
                                                    c->filep))
    {
        error_exit ("Error writing %ld bytes to %s", im->size, c->filen);
    }

    ret_val = 0;

exit:
    pthread_mutex_unlock (&ctx->mutex);
    return ret_val;
}

int sio_output_exit (plugin_context* ctx,
                     int             thread_id)
{
    fi_output_context* c;
    int i;
    int ret_val = -1;

    pthread_mutex_lock (&ctx->mutex);

    if (NULL == (c = (fi_output_context*) ctx->data)) {
        error_exit ("Invalid context");
    }

    if (--c->references) {
        ret_val = 0;
        goto exit;
    }

    fclose (c->filep);
    free (c->filen);
    free (c);
    ctx->data = NULL;
    ret_val = 0;

exit:
    pthread_mutex_unlock (&ctx->mutex);
    return ret_val;
}
