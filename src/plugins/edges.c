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

#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "image.h"
#include "plugin.h"

/* start plugin interface */
int edges_query (plugin_stage   stage,
                 plugin_info**  pi);
int edges_proc_exec (plugin_context*    ctx,
                     int                thread_id,
                     image_t**          src_data,
                     image_t**          dst_data);

static const char edges_name[] = "edges_process";
static const data_fmt edges_fmts[] = {FMT_RGB24};
static plugin_info pi_edges_proc = {.stage=PLUGIN_STAGE_PROCESS,
                                    .type=PLUGIN_TYPE_ASYNC,
                                    .src_fmt=edges_fmts,
                                    .dst_fmt=edges_fmts,
                                    .name=edges_name,
                                    .init=NULL,
                                    .exit=NULL,
                                    .exec=edges_proc_exec};

/* returns the plugin_info struct to the system when queried for process
 * support */
int edges_query (plugin_stage   stage,
                 plugin_info**  pi)
{
    *pi = NULL;
    switch (stage) {
        case PLUGIN_STAGE_PROCESS:
            *pi = &pi_edges_proc;
            break;
        default:
            return -1;
    }
    return 0;
}
/* end plugin interface */

inline int clip_uint8( int v )
{
    return ( (v < 0) ? 0 : (v > 255) ? 255 : v );
}

#define o(x,y,p) (pitch*(y)+(x)*3+p)

int edges_proc_exec (plugin_context*    ctx,
                     int                thread_id,
                     image_t**          src_data,
                     image_t**          dst_data)
{
    image_t* im;
    image_t* dim;
    //const double op = 255/sqrt(pow(255*3,2)*2); //  = max / (lmax - lmin)
    int i, j;
    int pitch;

    // make sure inputs are valid
    if (
        NULL == (im = *src_data) ||
        NULL != *dst_data ||
        NULL == (dim = calloc (1, sizeof(image_t))))
    {
        return -1;
    }

    // allocate output image
    dim->pix = calloc (im->height*im->width*im->bpp/8 * 3, sizeof(uint8_t));
    dim->width = im->width;
    dim->height = im->height;
    dim->bpp = im->bpp;
    dim->fmt = FMT_RGB24;

    pitch = im->width*im->bpp/8;

    for( j = 0; j < im->height; j++ )
    {
        for( i = 1; i < im->width; i++ )
        {
            int ci, cj, pix;
            double dx[3] = {0}; // {r, g, b}
            double dy[3] = {0}; // {r, g, b}

            if( i == 0 || j == 0 || i == im->width-1 || j == im->height-1 )
                continue;

            for( pix = 0; pix < 3; pix++ ) {

                // left and right
                for( cj = j-1; cj < j+1; cj++ ) {
                    dx[pix] -= im->pix[o(i-1,cj,pix)]; // left
                    dx[pix] += im->pix[o(i+1,cj,pix)]; // right
                }

                // top and bottom
                for( ci = i-1; ci < i+1; ci++ ) {
                    dy[pix] += im->pix[o(ci,j-1,pix)]; // top
                    dy[pix] -= im->pix[o(ci,j+1,pix)]; // bottom
                }

                // to scale the resultant pixel down to its appropriate value it
                // should be multiplied by op, but visually, it looks better to
                // clip it... this may change in the future.
                dim->pix[o(i,j,pix)] = clip_uint8( sqrt(pow(dx[pix],2)+pow(dy[pix],2)) );
            }
        }
    }

    // pass the output image along
    *dst_data = dim;

    return 0;
}
