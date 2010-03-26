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

#ifndef _H_RB_PLUGIN
#define _H_RB_PLUGIN

#include <pthread.h>

#include "image.h"

int parse_args(char* args, const int clear, char* key, char** value);

typedef enum {
    PLUGIN_STAGE_NONE,
    PLUGIN_STAGE_INPUT,
    PLUGIN_STAGE_DECODE,
    PLUGIN_STAGE_CONVERT,
    PLUGIN_STAGE_PROCESS,
    PLUGIN_STAGE_ENCODE,
    PLUGIN_STAGE_OUTPUT,
    PLUGIN_STAGE_MAX
} plugin_stage;

typedef enum {
    PLUGIN_TYPE_SYNC,
    PLUGIN_TYPE_ASYNC
} plugin_type;

typedef struct plugin_context {
    pthread_mutex_t mutex;
    int             num_threads;
    void*           data;
} plugin_context;

typedef struct plugin_info {
    const plugin_stage    stage;
    const plugin_type     type;
    const data_fmt*       src_fmt;
    const data_fmt*       dst_fmt;
    const char*           name;

    int (*init) (plugin_context* ctx, int thread_id, char* args);
    int (*exit) (plugin_context* ctx, int thread_id);
    int (*exec) (plugin_context* ctx, int thread_id, image_t** src_data, image_t** dst_data);
    int (*query)(char* args, void* info);
} plugin_info;

#endif
