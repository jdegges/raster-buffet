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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include <FreeImage.h>

#include "image.h"
#include "plugin.h"

// function definitions
int freeimage_query (plugin_stage   stage,
                     plugin_info**  pi);

int fi_input_init (plugin_context* ctx,
                   int              thread_id,
                   char*            args);
int fi_input_exec (plugin_context*  ctx,
                   int              thread_id,
                   image_t**        src_data,
                   image_t**        dst_data);
int fi_input_exit (plugin_context*  ctx,
                   int              thread_id);

int fi_decode_exec (plugin_context* ctx,
                    int             thread_id,
                    image_t**       src_data,
                    image_t**       dst_data);

int fi_encode_init (plugin_context* ctx,
                    int             thread_id,
                    char*           args);
int fi_encode_exec (plugin_context* ctx,
                    int             thread_id,
                    image_t**       src_data,
                    image_t**       dst_data);
int fi_encode_exit (plugin_context* ctx,
                    int             thread_id);

int fi_output_init (plugin_context* ctx,
                    int             thread_id,
                    char*           args);
int fi_output_exec (plugin_context* ctx,
                    int             thread_id,
                    image_t**        src_data,
                    image_t**        dst_data);
int fi_output_exit (plugin_context* ctx,
                    int             thread_id);

data_fmt fif_to_native (FREE_IMAGE_FORMAT fif);

// input plugin configuration
static const char input_name[] = "freeimage_input";
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
static plugin_info pi_freeimage_input = {.stage=PLUGIN_STAGE_INPUT,
                                         .type=PLUGIN_TYPE_ASYNC,
                                         .src_fmt=input_src_fmt,
                                         .dst_fmt=input_dst_fmt,
                                         .name=input_name,
                                         .init=fi_input_init,
                                         .exit=fi_input_exit,
                                         .exec=fi_input_exec};

// decode plugin configuration
static const char decode_name[] = "freeimage_decode";
static const data_fmt decode_dst_fmt[] = {FMT_RGB24, -1};
static plugin_info pi_freeimage_decode = {.stage=PLUGIN_STAGE_DECODE,
                                          .type=PLUGIN_TYPE_ASYNC,
                                          .src_fmt=input_dst_fmt,
                                          .dst_fmt=decode_dst_fmt,
                                          .name=decode_name,
                                          .init=NULL,
                                          .exit=NULL,
                                          .exec=fi_decode_exec};

// output plugin configuration
static const char encode_name[] = "freeimage_encode";
static plugin_info pi_freeimage_encode = {.stage=PLUGIN_STAGE_ENCODE,
                                          .type=PLUGIN_TYPE_ASYNC,
                                          .src_fmt=decode_dst_fmt,
                                          .dst_fmt=input_dst_fmt,
                                          .name=encode_name,
                                          .init=fi_encode_init,
                                          .exit=fi_encode_exit,
                                          .exec=fi_encode_exec};

// output plugin configuration
static const char output_name[] = "freeimage_output";
static plugin_info pi_freeimage_output = {.stage=PLUGIN_STAGE_OUTPUT,
                                          .type=PLUGIN_TYPE_ASYNC,
                                          .src_fmt=input_dst_fmt,
                                          .dst_fmt=input_src_fmt,
                                          .name=output_name,
                                          .init=fi_output_init,
                                          .exit=fi_output_exit,
                                          .exec=fi_output_exec};

int freeimage_query (plugin_stage stage, plugin_info** pi)
{
    *pi = NULL;
    switch (stage) {
        case PLUGIN_STAGE_INPUT:
            *pi = &pi_freeimage_input;
            break;
        case PLUGIN_STAGE_DECODE:
            *pi = &pi_freeimage_decode;
            break;
        case PLUGIN_STAGE_ENCODE:
            *pi = &pi_freeimage_encode;
            break;
        case PLUGIN_STAGE_OUTPUT:
            *pi = &pi_freeimage_output;
            break;
        default:
            return -1;
    }
    return 0;
}

typedef struct fi_input_context {
    FILE*   filep;
    char*   filen;
    char**  buf;
} fi_input_context;

#define BUF_LEN 1031

int fi_input_init (plugin_context*  ctx,
                   int              thread_id,
                   char*            args)
{
    fi_input_context* c;

    pthread_mutex_lock (&ctx->mutex);

    if (ctx->data == NULL) {
        // set up state shared between all threads
        if (NULL == (c = malloc (sizeof(fi_input_context)))) {
            return -1;
        }

        parse_args (args, 0, "rsc",  &(c->filen));
        if (NULL == c->filen) {
            fprintf (stderr, "No input rsc defined.\n");
            return -1;
        }
     
        if (NULL == (c->filep = fopen(c->filen, "r"))) {
            return -1;
        }

        if (NULL == (c->buf = calloc (ctx->num_threads, sizeof(char*)))) {
            return -1;
        }

        ctx->data = c;
    }

    // setup state for this thread only (in this case each thread allocates its
    // own fi_input_context.buf buffer)
    if (NULL == (c = (fi_input_context*) ctx->data) ||
        NULL != (c->buf[thread_id]))
    {
        return -1;
    }

    if (NULL == (c->buf[thread_id] = malloc (sizeof(char)*BUF_LEN))) {
        return -1;
    }

    pthread_mutex_unlock (&ctx->mutex);

    return 0;
}

int fi_input_exec (plugin_context*  ctx,
                   int              thread_id,
                   image_t**        src_data,
                   image_t**        dst_data)
{
    fi_input_context* c;
    char* path;
    image_t* im;
    struct stat sbuf;
    FILE* fptr;

    if (NULL == (c = (fi_input_context*) ctx->data)) {
        return -1;
    }

    if (NULL == (im = calloc (1 ,sizeof(image_t)))) {
        return -1;
    }
    im->width = im->height = im->bpp = -1;

    // lock source file list
    pthread_mutex_lock (&ctx->mutex);

    if (NULL == fgets (c->buf[thread_id], BUF_LEN, c->filep)
        || NULL == (path = strtok(c->buf[thread_id], "\n")))
    {
        return -1;
    }

    // unlock source file list
    pthread_mutex_unlock (&ctx->mutex);

    if (0 != stat (path, &sbuf)) {
        return -1;
    }

    if (NULL == (im->pix = malloc (sizeof(uint8_t)*sbuf.st_size))) {
        return -1;
    }
    im->size = sizeof(uint8_t)*sbuf.st_size;

    if (NULL == (fptr = fopen(path, "rb"))) {
        return -1;
    }

    if (sbuf.st_size != fread (im->pix, sizeof(uint8_t), sbuf.st_size, fptr)) {
        return -1;
    }

    if (EOF == fclose (fptr)) {
        return -1;
    }

    im->fmt = fif_to_native (FreeImage_GetFileType (path, 0));
    *dst_data = im;

    return 0;
}

int fi_input_exit (plugin_context*  ctx,
                   int              thread_id)
{
    fi_input_context* c;
    int i;

    if (NULL == (c = (fi_input_context*) ctx->data) ||
        NULL == c->buf[thread_id])
    {
        return -1;
    }

    free (c->buf[thread_id]);
    c->buf[thread_id] = NULL;

    pthread_mutex_lock (&ctx->mutex);

    for (i = 0; i < ctx->num_threads; i++) {
        // return only if some other thread still hasn't free'd all of its
        // resources
        if (c->buf[i] != NULL) {
            pthread_mutex_unlock (&ctx->mutex);
            return 0;
        }
    }

    // at this point all other threads have free'd their resources (c->buf[i])
    // so it is safe to free the shared resources

    fclose (c->filep);
    free (c->filen);
    free (c->buf);
    free (c);

    ctx->data = NULL;

    pthread_mutex_unlock (&ctx->mutex);
    return 0;
}

int fi_decode_exec (plugin_context* ctx,
                    int             thread_id,
                    image_t**       src_data,
                    image_t**       dst_data)
{
    image_t* sim;
    image_t* dim;
    FIMEMORY* hmem;
    FREE_IMAGE_FORMAT fif;
    FIBITMAP *dib, *dib24;

    if (NULL == (sim = *src_data) || NULL != *dst_data) {
        return -1;
    }

    if (NULL == (hmem = FreeImage_OpenMemory (sim->pix, sim->size))) {
        return -1;
    }

    if (FIF_UNKNOWN == (fif = FreeImage_GetFileTypeFromMemory (hmem, 0))) {
        return -1;
    }

    if (NULL == (dib = FreeImage_LoadFromMemory (fif, hmem, 0))) {
        return -1;
    }

    if (NULL == (dim = malloc (sizeof(image_t)))) {
        return -1;
    }

    dib24 = FreeImage_ConvertTo24Bits (dib);

    fprintf (stderr, "pitch:%d\n", FreeImage_GetPitch(dib24));
    fprintf (stderr, "width:%d\n", FreeImage_GetWidth(dib24));
    fprintf (stderr, "bpp:%d\n", FreeImage_GetBPP(dib24));

    // plug decoded data into new image
    dim->pix = FreeImage_GetBits (dib24);
    dim->width = FreeImage_GetWidth (dib24);
    dim->height = FreeImage_GetHeight (dib24);
    dim->bpp = FreeImage_GetBPP (dib24);
    dim->fmt = FMT_RGB24;
    dim->ext_data = dib24;
    dim->ext_free = (void*) &FreeImage_Unload;

    *dst_data = dim;

    // free up intermediate structs
    FreeImage_Unload (dib);
    FreeImage_CloseMemory (hmem);

    return 0;
}

typedef struct fi_encode_context {
    data_fmt            dst_fmt;
    FREE_IMAGE_FORMAT   dst_fif;
    int*                threads;
} fi_encode_context;

char* native_to_charfmt (data_fmt fmt)
{
    int size = 11;
    char* str = calloc (size, sizeof(char));

    switch (fmt) {
        case FMT_BMP:       return strncpy (str, "bmp", size);
        case FMT_CUT:       return strncpy (str, "cut", size);
        case FMT_DDS:       return strncpy (str, "dds", size);
        case FMT_EXR:       return strncpy (str, "exr", size);
        case FMT_FAXG3:     return strncpy (str, "g3", size);
        case FMT_GIF:       return strncpy (str, "gif", size);
        case FMT_HDR:       return strncpy (str, "hdr", size);
        case FMT_ICO:       return strncpy (str, "ico", size);
        case FMT_IFF:       return strncpy (str, "iff", size);
        case FMT_J2K:       return strncpy (str, "j2k", size);
        case FMT_JNG:       return strncpy (str, "jng", size);
        case FMT_JP2:       return strncpy (str, "jp2", size);
        case FMT_JPEG:      return strncpy (str, "jpeg", size);
        case FMT_KOALA:     return strncpy (str, "koa", size);
        case FMT_MNG:       return strncpy (str, "mng", size);
        case FMT_PBM:       return strncpy (str, "pbm", size);
        case FMT_PBMRAW:    return strncpy (str, "pbm", size);
        case FMT_PCD:       return strncpy (str, "pcd", size);
        case FMT_PCX:       return strncpy (str, "pcx", size);
        case FMT_PFM:       return strncpy (str, "pfm", size);
        case FMT_PGM:       return strncpy (str, "pgm", size);
        case FMT_PGMRAW:    return strncpy (str, "pgm", size);
        case FMT_PICT:      return strncpy (str, "pict", size);
        case FMT_PNG:       return strncpy (str, "png", size);
        case FMT_PPM:       return strncpy (str, "ppm", size);
        case FMT_PPMRAW:    return strncpy (str, "ppm", size);
        case FMT_PSD:       return strncpy (str, "psd", size);
        case FMT_RAS:       return strncpy (str, "ras", size);
        case FMT_RAW:       return strncpy (str, "raw", size);
        case FMT_SGI:       return strncpy (str, "sgi", size);
        case FMT_TARGA:     return strncpy (str, "targa", size);
        case FMT_TIFF:      return strncpy (str, "tiff", size);
        case FMT_WBMP:      return strncpy (str, "wbmp", size);
        case FMT_XBM:       return strncpy (str, "xbm", size);
        case FMT_XPM:       return strncpy (str, "xpm", size);
        default:            free (str); return NULL;
    }
    free (str);
    return NULL;
}

data_fmt charfmt_to_native (char* fmt)
{
    if (NULL == fmt) {
        return FMT_NONE;
    }

    if (0 == strcasecmp ("BMP", fmt)) {
        return FMT_BMP;
    } else if (0 == strcasecmp ("CUT", fmt)) {
        return FMT_CUT;
    } else if (0 == strcasecmp ("DDS", fmt)) {
        return FMT_DDS;
    } else if (0 == strcasecmp ("EXR", fmt)) {
        return FMT_EXR;
    } else if (0 == strcasecmp ("FAXG3", fmt) ||
               0 == strcasecmp ("G3", fmt))
    {
        return FMT_FAXG3;
    } else if (0 == strcasecmp ("GIF", fmt)) {
        return FMT_GIF;
    } else if (0 == strcasecmp ("HDR", fmt)) {
        return FMT_HDR;
    } else if (0 == strcasecmp ("ICO", fmt)) {
        return FMT_ICO;
    } else if (0 == strcasecmp ("IFF", fmt) ||
               0 == strcasecmp ("LBM", fmt))
    {
        return FMT_IFF;
    } else if (0 == strcasecmp ("J2K", fmt) ||
               0 == strcasecmp ("J2C", fmt))
    {
        return FMT_J2K;
    } else if (0 == strcasecmp ("JNG", fmt)) {
        return FMT_JNG;
    } else if (0 == strcasecmp ("JP2", fmt)) {
        return FMT_JP2;
    } else if (0 == strcasecmp ("JPEG", fmt) ||
               0 == strcasecmp ("JPG", fmt) ||
               0 == strcasecmp ("JIF", fmt) ||
               0 == strcasecmp ("JPE", fmt))
    {
        return FMT_JPEG;
    } else if (0 == strcasecmp ("KOALA", fmt) ||
               0 == strcasecmp ("KOA", fmt))
    {
        return FMT_KOALA;
    } else if (0 == strcasecmp ("MNG", fmt)) {
        return FMT_MNG;
    } else if (0 == strcasecmp ("PBM", fmt)) {
        return FMT_PBM;
    } else if (0 == strcasecmp ("PBMRAW", fmt)) {
        return FMT_PBMRAW;
    } else if (0 == strcasecmp ("PCD", fmt)) {
        return FMT_PCD;
    } else if (0 == strcasecmp ("PCX", fmt)) {
        return FMT_PCX;
    } else if (0 == strcasecmp ("PFM", fmt)) {
        return FMT_PFM;
    } else if (0 == strcasecmp ("PGM", fmt)) {
        return FMT_PGM;
    } else if (0 == strcasecmp ("PGMRAW", fmt)) {
        return FMT_PGMRAW;
    } else if (0 == strcasecmp ("PICT", fmt) ||
               0 == strcasecmp ("PCT", fmt) ||
               0 == strcasecmp ("PIC", fmt))
    {
        return FMT_PICT;
    } else if (0 == strcasecmp ("PNG", fmt)) {
        return FMT_PNG;
    } else if (0 == strcasecmp ("PPM", fmt)) {
        return FMT_PPM;
    } else if (0 == strcasecmp ("PPMRAW", fmt)) {
        return FMT_PPMRAW;
    } else if (0 == strcasecmp ("PSD", fmt)) {
        return FMT_PSD;
    } else if (0 == strcasecmp ("RAS", fmt)) {
        return FMT_RAS;
    } else if (0 == strcasecmp ("SGI", fmt)) {
        return FMT_SGI;
    } else if (0 == strcasecmp ("TARGA", fmt) ||
               0 == strcasecmp ("TGA", fmt))
    {
        return FMT_TARGA;
    } else if (0 == strcasecmp ("RAW", fmt)) {
        return FMT_RAW;
    } else if (0 == strcasecmp ("TIFF", fmt) ||
               0 == strcasecmp ("TIF", fmt))
    {
        return FMT_TIFF;
    } else if (0 == strcasecmp ("WBMP", fmt)) {
        return FMT_WBMP;
    } else if (0 == strcasecmp ("XBM", fmt)) {
        return FMT_XBM;
    } else if (0 == strcasecmp ("XPM", fmt)) {
        return FMT_XPM;
    } else {
        return FMT_NONE;
    }
}

int fi_encode_init (plugin_context* ctx,
                    int             thread_id,
                    char*           args)
{
    fi_encode_context* c;
    char* str;
    data_fmt dst_fmt;

    pthread_mutex_lock (&ctx->mutex);
    
    if (NULL == ctx->data) {
        if (-1 == parse_args (args, 0, "dst_fmt", &str)) {
            return -1;
        }

        if (FMT_NONE == (dst_fmt = charfmt_to_native (str))) {
            free (str);
            return -1;
        }
        free (str);

        if (NULL == (c = malloc (sizeof(fi_encode_context)))) {
            return -1;
        }

        if (NULL == (c->threads = calloc (ctx->num_threads, sizeof(int)))) {
            free (c);
            return -1;
        }
        c->dst_fmt = dst_fmt;
        c->dst_fif = native_to_fif (dst_fmt);
        ctx->data = c;
    }

    c = (fi_encode_context*) ctx->data;

    if (NULL == c->threads ||
        0 != c->threads[thread_id])
    {
        return -1;
    }
    c->threads[thread_id] = 1;

    pthread_mutex_unlock (&ctx->mutex);
    return 0;
}

int fi_encode_exec (plugin_context* ctx,
                    int             thread_id,
                    image_t**       src_data,
                    image_t**       dst_data)
{
    fi_encode_context* c;
    image_t* sim;
    image_t* dim;
    FIMEMORY* hmem;
    FREE_IMAGE_FORMAT fif;
    FIBITMAP* dib;
    uint32_t size = 0;
    uint8_t* pix;

    if (NULL == (c = (fi_encode_context*) ctx->data) ||
        NULL == (sim = *src_data) ||
        NULL != (*dst_data) ||
        NULL == (dim = calloc (1, sizeof(image_t))))
    {
        fprintf (stderr, "Error with args: fi_encode_exec\n");
        fprintf (stderr, "c        : NULL == %p\n", c);
        fprintf (stderr, "sim      : NULL == %p\n", sim);
        fprintf (stderr, "*dst_data: NULL != %p\n", *dst_data);
        fprintf (stderr, "dim      : NULL == %p\n", dim);
        return -1;
    }

    if (sim->ext_free == (void*)&FreeImage_Unload) {
        if (NULL == (dib = sim->ext_data)) {
            return -1;
        }
    } else {
        if (NULL == (dib = FreeImage_Allocate (sim->width, sim->height,
                                               sim->bpp, 0, 0, 0)))
        {
            return -1;
        }
    }

    pix = FreeImage_GetBits (dib);
    memcpy (pix, sim->pix, sim->width*sim->height*sim->bpp/8);
    fif = c->dst_fif;

    if(NULL == (hmem = FreeImage_OpenMemory (0, 0)) ||
       !FreeImage_SaveToMemory (fif, dib, hmem, 0) ||
       !FreeImage_AcquireMemory (hmem, &dim->pix, &size))
    {
        return -1;
    }

    dim->size = size;
    FreeImage_Unload (dib);

    dim->ext_data = hmem;
    dim->ext_free = (void*)&FreeImage_CloseMemory;
    dim->fmt = c->dst_fmt;

    *dst_data = dim;
    return 0;
}

int fi_encode_exit (plugin_context* ctx,
                    int             thread_id)
{
    fi_encode_context* c;
    int i;

    pthread_mutex_lock (&ctx->mutex);

    if (NULL == (c = (fi_encode_context*) ctx->data)) {
        return -1;
    }

    c->threads[thread_id] = -1;

    for (i = 0; i < ctx->num_threads; i++) {
        if (0 <= c->threads[i]) {
            pthread_mutex_unlock (&ctx->mutex);
            return 0;
        }
    }

    free (c->threads);
    free (c);
    ctx->data = NULL;

    pthread_mutex_unlock (&ctx->mutex);
    return 0;
}

typedef struct fi_output_context {
    FILE*   filep;
    char*   filen;
    char*   dir;
    char**  buf;
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
        if (NULL == c->filen) {
            return -1;
        }

        parse_args (args, 0, "dir", &c->dir);
        if (NULL == c->dir) {
            return -1;
        }

        if (NULL == (c->filep = fopen (c->filen, "w"))) {
            return -1;
        }

        if (NULL == (c->buf = calloc (ctx->num_threads, sizeof(char*)))) {
            return -1;
        }

        ctx->data = c;
    }

    if (NULL == (c = (fi_output_context*) ctx->data) ||
        NULL != (c->buf[thread_id]))
    {
        return -1;
    }

    if (NULL == (c->buf[thread_id] = malloc (sizeof(char)*BUF_LEN))) {
        return -1;
    }

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
    FILE* fptr;
    int got_ctx_lock = 0;
    int size;
    char* ext;

    if (NULL == (c = (fi_output_context*) ctx->data) ||
        NULL == (im = *src_data)) {
        return -1;
    }

    ext = native_to_charfmt (im->fmt);

    if (0 > (size = snprintf (c->buf[thread_id], BUF_LEN, "%s/frame-%ld.%s",
                              c->dir, im->frame, ext)))
    {
        return -1;
    }

    free (ext);

    if (0 == pthread_mutex_lock(&ctx->mutex)) {
        got_ctx_lock = 1;
        if (size != fwrite (c->buf[thread_id], sizeof(char), size, c->filep) ||
            1 != fwrite ("\n", sizeof(char), 1, c->filep))
        {
            return -1;
        }

        pthread_mutex_unlock (&ctx->mutex);
    }

    if (NULL == (fptr = fopen (c->buf[thread_id], "w"))) {
        return -1;
    }

    if (im->size != fwrite (im->pix, sizeof(uint8_t), im->size, fptr)) {
        return -1;
    }

    if (0 != fclose (fptr)) {
        return -1;
    }

    if (!got_ctx_lock) {
        pthread_mutex_lock (&ctx->mutex);
        if (size != fwrite (c->buf[thread_id], size, sizeof(char), c->filep) ||
            1 != fwrite ("\n", 1, sizeof(char), c->filep))
        {
            return -1;
        }
        pthread_mutex_unlock (&ctx->mutex);
    }
    return 0;
}

int fi_output_exit (plugin_context* ctx,
                    int             thread_id)
{
    fi_output_context* c;
    int i;

    if (NULL == (c = (fi_output_context*) ctx->data) ||
        NULL == c->buf[thread_id])
    {
        return -1;
    }

    free (c->buf[thread_id]);
    c->buf[thread_id] = NULL;

    pthread_mutex_lock (&ctx->mutex);

    for (i = 0; i < ctx->num_threads; i++) {
        if (c->buf[i] != NULL) {
            pthread_mutex_unlock (&ctx->mutex);
            return 0;
        }
    }

    free (c->buf);
    fclose (c->filep);
    free (c->dir);
    free (c->filen);
    free (c);
    ctx->data = NULL;

    pthread_mutex_unlock (&ctx->mutex);
    return 0;
}

data_fmt fif_to_native (FREE_IMAGE_FORMAT fif)
{
    switch (fif) {
        case FIF_BMP:       return FMT_BMP;
        case FIF_CUT:       return FMT_CUT;
        case FIF_DDS:       return FMT_DDS;
        case FIF_EXR:       return FMT_EXR;
        case FIF_FAXG3:     return FMT_FAXG3;
        case FIF_GIF:       return FMT_GIF;
        case FIF_HDR:       return FMT_HDR;
        case FIF_ICO:       return FMT_ICO;
        case FIF_IFF:       return FMT_IFF;
        case FIF_J2K:       return FMT_J2K;
        case FIF_JNG:       return FMT_JNG;
        case FIF_JP2:       return FMT_JP2;
        case FIF_JPEG:      return FMT_JPEG;
        case FIF_KOALA:     return FMT_KOALA;
        case FIF_MNG:       return FMT_MNG;
        case FIF_PBM:       return FMT_PBM;
        case FIF_PBMRAW:    return FMT_PBMRAW;
        case FIF_PCD:       return FMT_PCD;
        case FIF_PCX:       return FMT_PCX;
        case FIF_PFM:       return FMT_PFM;
        case FIF_PGM:       return FMT_PGM;
        case FIF_PGMRAW:    return FMT_PGMRAW;
//        case FIF_PICT:      return FMT_PICT;
        case FIF_PNG:       return FMT_PNG;
        case FIF_PPM:       return FMT_PPM;
        case FIF_PPMRAW:    return FMT_PPMRAW;
        case FIF_PSD:       return FMT_PSD;
        case FIF_RAS:       return FMT_RAS;
//        case FIF_RAW:       return FMT_RAW;
        case FIF_SGI:       return FMT_SGI;
        case FIF_TARGA:     return FMT_TARGA;
        case FIF_TIFF:      return FMT_TIFF;
        case FIF_WBMP:      return FMT_WBMP;
        case FIF_XBM:       return FMT_XBM;
        case FIF_XPM:       return FMT_XPM;
        default:            return FMT_NONE;
    }
    return FMT_NONE;
}

FREE_IMAGE_FORMAT native_to_fif (data_fmt fmt)
{
    switch (fmt) {
        case FMT_BMP:       return FIF_BMP;
        case FMT_CUT:       return FIF_CUT;
        case FMT_DDS:       return FIF_DDS;
        case FMT_EXR:       return FIF_EXR;
        case FMT_FAXG3:     return FIF_FAXG3;
        case FMT_GIF:       return FIF_GIF;
        case FMT_HDR:       return FIF_HDR;
        case FMT_ICO:       return FIF_ICO;
        case FMT_IFF:       return FIF_IFF;
        case FMT_J2K:       return FIF_J2K;
        case FMT_JNG:       return FIF_JNG;
        case FMT_JP2:       return FIF_JP2;
        case FMT_JPEG:      return FIF_JPEG;
        case FMT_KOALA:     return FIF_KOALA;
        case FMT_MNG:       return FIF_MNG;
        case FMT_PBM:       return FIF_PBM;
        case FMT_PBMRAW:    return FIF_PBMRAW;
        case FMT_PCD:       return FIF_PCD;
        case FMT_PCX:       return FIF_PCX;
        case FMT_PFM:       return FIF_PFM;
        case FMT_PGM:       return FIF_PGM;
        case FMT_PGMRAW:    return FIF_PGMRAW;
        //case FMT_PICT:      return FIF_PICT;
        case FMT_PNG:       return FIF_PNG;
        case FMT_PPM:       return FIF_PPM;
        case FMT_PPMRAW:    return FIF_PPMRAW;
        case FMT_PSD:       return FIF_PSD;
        case FMT_RAS:       return FIF_RAS;
        //case FMT_RAW:       return FIF_RAW;
        case FMT_SGI:       return FIF_SGI;
        case FMT_TARGA:     return FIF_TARGA;
        case FMT_TIFF:      return FIF_TIFF;
        case FMT_WBMP:      return FIF_WBMP;
        case FMT_XBM:       return FIF_XBM;
        case FMT_XPM:       return FIF_XPM;
        default:            return FIF_UNKNOWN;
    }
    return FIF_UNKNOWN;
}
