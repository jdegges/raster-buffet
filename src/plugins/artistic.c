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
 *
 * The algorithms implemented in this file were first presented here:
 * http://www.cs.rug.nl/~imaging/artisticsmoothing/
 * @Article{PapariPetkovCampisi, 
 *   author = {Papari, Giuseppe and Petkov, Nicolai and Campisi, Patrizio},
 *   title = {Artistic Edge and Corner Preserving Smoothing},
 *   journal = {IEEE Transactions on Image Processing},
 *   year = {2007},
 *   volume = {29},
 *   issue = {9}
 *   pages = {###--###},
 * }
 *****************************************************************************/

#include <string.h>
#include <math.h>

/* fft stuff */
#include <complex.h>
#include <fftw3.h>

#include "image.h"
#include "plugin.h"

#ifndef M_PI
#define M_PI 3.1415926536
#endif

/* function definitions */
int artistic_query (plugin_stage    stage,
                    plugin_info**   pi);

int artistic_proc_init (plugin_context* ctx,
                        int             thread_id,
                        char*           args);
int artistic_proc_exec (plugin_context* ctx,
                        int             thread_id,
                        image_t**       src_data,
                        image_t**       dst_data);
int artistic_proc_exit (plugin_context* ctx,
                        int             thread_id);

/* process plugin configuration */
static const data_fmt supported_fmt[] = {FMT_RGB24};
static plugin_info pi_artistic_proc = {.stage=PLUGIN_STAGE_PROCESS,
                                       .type=PLUGIN_TYPE_ASYNC,
                                       .src_fmt=supported_fmt,
                                       .dst_fmt=supported_fmt,
                                       .name="artistic_process",
                                       .init=artistic_proc_init,
                                       .exit=artistic_proc_exit,
                                       .exec=artistic_proc_exec};

int artistic_query (plugin_stage stage, plugin_info** pi)
{
    *pi = NULL;
    switch (stage) {
        case PLUGIN_STAGE_PROCESS:
            *pi = &pi_artistic_proc;
            break;
        default:
            return -1;
    }
    return 0;
}

typedef struct fft_plans {
    fftw_plan forward;
    fftw_plan backward;
} fft_plans_t;

typedef struct artistic_buf {
    fftw_complex** g;

    fftw_complex** src1;
    fftw_complex** src2;
    fftw_complex** s;
    fftw_complex** m;

    double** num;
    double* den;
} artistic_buf_t;

fftw_complex* gen_sec (const int nx, const int ny,
                       fft_plans_t* f, const double a, const double wd,
                       const double* g1, const fftw_complex* g2) {
    int i, j, x, y;
    double total = 0;
    double* out;

    double* sec_d = fftw_malloc (sizeof(double)*ny*2*(nx/2+1));
    if (NULL == sec_d) {
      return NULL;
    }
    fftw_complex* sec = (fftw_complex*)sec_d;
    memset(sec_d, 0, sizeof(double)*ny*2*(nx/2+1));
    for (j = 0; j < ny; j++) {
        for (i = 0; i < nx; i++) {
            register double x_val = -nx/2.0 + (i) * (double)nx/(double)(nx-1);
            register double y_val =  ny/2.0 - (j) * (double)ny/(double)(ny-1);

            sec_d[i+j*2*(nx/2+1)] = (x_val*cos(a-wd+M_PI/2.0) + y_val*sin(a-wd+M_PI/2.0) >  0 ? 1 : 0) *
                                    (x_val*cos(a+wd+M_PI/2.0) + y_val*sin(a+wd+M_PI/2.0) <= 0 ? 1 : 0);
        }
    }
    fftw_execute_dft_r2c (f->forward, sec_d, sec);

    for (i = 0; i < ny*(nx/2+1); i++) {
        sec[i] *= g2[i];
    }

    fftw_execute_dft_c2r (f->backward, sec, sec_d);

    if (NULL == (out = fftw_malloc (sizeof(double)*ny*2*(nx/2+1)))) {
      return NULL;
    }
    x = nx/2;
    y = ny/2;
    for (j = 0; j < ny; j++) {
        int k = (j - y) % ny;
        k = k < 0 ? ny + k : k;
        for (i = 0; i < nx; i++) {
            int h = (i - x) % nx;
            h = h < 0 ? nx + h : h;
            out[i+j*2*(nx/2+1)] = sec_d[h+k*2*(nx/2+1)] / (nx*ny) * g1[i+j*nx];
            total += out[i+j*2*(nx/2+1)];
        }
    }

    for (i = 0; i < ny*2*(nx/2+1); i++) {
        out[i] /= total;
    }

    fftw_execute_dft_r2c (f->forward, out, (fftw_complex*)out);

    fftw_free (sec);
    return (fftw_complex*)out;
}

typedef struct artistic_proc_context {
    char*               args;
    fft_plans_t*        p;
    artistic_buf_t**    b;
    int                 width;
    int                 height;
    double              sgm;
    int                 ns;
} artistic_proc_context;

static double g_sgm = 3.8;

int init_global_bufs (plugin_context* ctx, int width, int height, double sgm, int ns)
{
    artistic_proc_context* c;
    int nx = width;
    int ny = height;
    fft_plans_t* f;
    fftw_complex* in;

    if (nx <= 0 || ny <= 0) {
        return -1;
    }

    if (NULL == (f = malloc (sizeof(fft_plans_t)))) {
      return -1;
    }
    if (NULL == (in = fftw_malloc (sizeof(fftw_complex)*ny*(nx/2+1)))) {
      return -1;
    }
    f->forward  = fftw_plan_dft_r2c_2d (ny, nx, (double*)in, in, FFTW_ESTIMATE);
    f->backward = fftw_plan_dft_c2r_2d (ny, nx, in, (double*)in, FFTW_ESTIMATE);
    fftw_free (in);

    if (NULL == (c = malloc (sizeof(artistic_proc_context)))) {
        return -1;
    }

    if (NULL == (c->b = calloc (ctx->num_threads, sizeof(artistic_buf_t*)))) {
        return -1;
    }

    c->width = width;
    c->height = height;
    c->sgm = sgm;
    c->ns = ns;
    c->p = f;
    ctx->data = c;

    return 0;
}

int init_thread_bufs (plugin_context* ctx, int thread_id)
{
    artistic_proc_context* c = ctx->data;
    const int nx = c->width;
    const int ny = c->height;
    const int nxny = nx*ny;
    const double sgm = c->sgm;
    const int ns = c->ns;
    fft_plans_t* p = c->p;

    {
        int i;
        double* g1;
        double* g;
        fftw_complex* g2;
        double* g2_d;
        fftw_complex** gc;

        artistic_buf_t* f = c->b[thread_id];
        f->g    = fftw_malloc (sizeof(fftw_complex*)*ns);
        f->src1 = fftw_malloc (sizeof(fftw_complex*)*3);
        f->src2 = fftw_malloc (sizeof(fftw_complex*)*3);
        f->s    = fftw_malloc (sizeof(fftw_complex*)*3);
        f->m    = fftw_malloc (sizeof(fftw_complex*)*3);
        f->num  = fftw_malloc (sizeof(double*)*3);
        f->den  = fftw_malloc (sizeof(double)*ny*2*(nx/2+1));
        if (NULL == f->g || NULL == f->src1 || NULL == f->src2 || NULL == f->s
         || NULL == f->m || NULL == f->num || NULL == f->den) {
          return -1;
        }

        for (i = 0; i < 3; i++) {
            f->src1[i]  = fftw_malloc (sizeof(fftw_complex)*ny*(nx/2+1));
            f->src2[i]  = fftw_malloc (sizeof(fftw_complex)*ny*(nx/2+1));
            f->s[i]     = fftw_malloc (sizeof(fftw_complex)*ny*(nx/2+1));
            f->m[i]     = fftw_malloc (sizeof(fftw_complex)*ny*(nx/2+1));
            f->num[i]   = fftw_malloc (sizeof(double)*ny*2*(nx/2+1));
            if (NULL == f->src1[i] || NULL == f->src2[i] || NULL == f->s[i]
              || NULL == f->m[i] || NULL == f->num[i]) {
              return -1;
            }
        }

        if (NULL == (g1 = malloc (sizeof(double)*nxny))) {
          return -1;
        }
        i = nxny;
        g = g1 + nxny - 1;
        while (i--) {
            register double x_val = -nx/2.0 + (i%nx) * nx/(nx-1);
            register double y_val =  ny/2.0 - (i/nx) * ny/(ny-1);
            *g-- = exp(-(x_val*x_val+y_val*y_val)/(2.0*sgm*sgm));
        }

        g2 = fftw_malloc (sizeof(fftw_complex)*ny*(nx/2+1));
        g2_d = (double*)g2;
        memset(g2, 0, sizeof(fftw_complex)*ny*(nx/2+1));
        g = g2_d + ny*2*(nx/2+1) - 1;
        i = ny*2*(nx/2+1);
        while (i--) {
            register const int y = (i / (2*(nx/2+1))) - ny/2.0;
            register const int x = (i % (2*(nx/2+1))) - nx/2.0;
            *g-- = exp(-0.5*(x*x+y*y));
        }
        fftw_execute_dft_r2c (((fft_plans_t*)(p))->forward, g2_d, g2);

        gc = f->g + ns - 1;
        i = ns;
        while (i--) {
            *gc-- = gen_sec(nx, ny, (fft_plans_t*)(p), i*2.0*M_PI/ns, M_PI/ns, g1, g2);
        }

        fftw_free (g2);
        free (g1);
    }
    return 0;
}

int artistic_proc_init (plugin_context* ctx,
                        int             thread_id,
                        char*           args)
{
    artistic_proc_context* c;
    const int ns = 8;
    int nx;
    int ny;
    fft_plans_t* p;

    if (NULL == ctx) {
        return -1;    
    }

    pthread_mutex_lock (&ctx->mutex);

    if (NULL == ctx->data) {
        char* str = NULL;

        parse_args(args, 0, "sgm", &str);
        if (NULL != str) {
          g_sgm = strtod(str, NULL);
          free (str);
          str = NULL;
        }

        parse_args (args, 0, "width", &str);
        if (NULL == str) {
            ctx->data = NULL;
            pthread_mutex_unlock (&ctx->mutex);
            return 0;
        } else {
          nx = atoi (str);
          free (str);
          str = NULL;
        }

        parse_args (args, 0, "height", &str);
        if (NULL == str) {
            ctx->data = NULL;
            pthread_mutex_unlock (&ctx->mutex);
            return 0;
        } else {
          ny = atoi (str);
          free (str);
          str = NULL;
        }

        if (init_global_bufs (ctx, nx, ny, g_sgm, ns)) {
            return -1;
        }
    }

    pthread_mutex_unlock (&ctx->mutex);

    if (NULL == (c = (artistic_proc_context*) ctx->data) ||
        NULL == (p = (fft_plans_t*) c->p) ||
        NULL == (c->b[thread_id] = malloc (sizeof(artistic_buf_t))))
    {
        return -1;
    }

    if (init_thread_bufs (ctx, thread_id)) {
        return -1;
    }

    return 0;
}

int artistic_proc_exit (plugin_context* ctx,
                        int             thread_id)
{
    artistic_proc_context* c;
    int ns = 8;
    int i;

    if (NULL == (c = (artistic_proc_context*) ctx->data) ||
        NULL == c->b[thread_id])
    {
        return -1;
    }

    {
        artistic_buf_t* f = c->b[thread_id];
        int i;
        for (i = 0; i < ns; i++) {
            fftw_free (f->g[i]);
        }
        fftw_free (f->g);

        for (i = 0; i < 3; i++) {
            fftw_free (f->src1[i]);
            fftw_free (f->src2[i]);
            fftw_free (f->s[i]);
            fftw_free (f->m[i]);
            fftw_free (f->num[i]);
        }

        fftw_free (f->src1);
        fftw_free (f->src2);
        fftw_free (f->s);
        fftw_free (f->m);
        fftw_free (f->num);
        fftw_free (f->den);
        free (f);

        c->b[thread_id] = NULL;
    }

    pthread_mutex_lock (&ctx->mutex);

    for (i = 0; i < ctx->num_threads; i++) {
        if (c->b[i] != NULL) {
            pthread_mutex_unlock (&ctx->mutex);
            return 0;
        }
    }

    {
        fft_plans_t* f = c->p;
        fftw_destroy_plan (f->forward);
        fftw_destroy_plan (f->backward);
        free (f);
        free (c->b);
        free (c);
        fftw_cleanup ();
        ctx->data = NULL;
    }

    pthread_mutex_unlock (&ctx->mutex);

    return 0;
}

#define MULTIPLY_6_C_INNER \
*d1++ = *s1++ * *m; *d2++ = *s2++ * *m;\
*d3++ = *s3++ * *m; *d4++ = *s4++ * *m;\
*d5++ = *s5++ * *m; *d6++ = *s6++ * *m++;

static void multiply_6_c(fftw_complex* d1, const fftw_complex* s1,
                                fftw_complex* d2, const fftw_complex* s2,
                                fftw_complex* d3, const fftw_complex* s3,
                                fftw_complex* d4, const fftw_complex* s4,
                                fftw_complex* d5, const fftw_complex* s5,
                                fftw_complex* d6, const fftw_complex* s6,
                                const fftw_complex* m, const int n) {
    register unsigned int i = n;
    unsigned int correction = n % 8;
    i -= correction;

    while (i) {
        MULTIPLY_6_C_INNER;
        MULTIPLY_6_C_INNER;
        MULTIPLY_6_C_INNER;
        MULTIPLY_6_C_INNER;
        MULTIPLY_6_C_INNER;
        MULTIPLY_6_C_INNER;
        MULTIPLY_6_C_INNER;
        MULTIPLY_6_C_INNER;
        i -= 8;
    }

    while (correction--) {
        MULTIPLY_6_C_INNER;
    }
}

static void divide_mul_const_d(double* d, const double* s, const double c, const int n) {
    register unsigned int i = n;
    unsigned int correction = n % 8;
    i -= correction;

    while (i) {
        *d++ /= *s++ * c;
        *d++ /= *s++ * c;
        *d++ /= *s++ * c;
        *d++ /= *s++ * c;
        *d++ /= *s++ * c;
        *d++ /= *s++ * c;
        *d++ /= *s++ * c;
        *d++ /= *s++ * c;
        i -= 8;
    }

    while (correction--) {
        *d++ /= *s++ * c;
    }
}

#define MESH_D_INNER \
rdst = ((*a++ + *b++ + *c++)/k - (*d * *d + *e * *e + *f * *f)/k2) + 0.0001;\
rdst = 1 / (rdst * rdst * rdst * rdst);\
*d1++ += *d++ * rdst;\
*d2++ += *e++ * rdst;\
*d3++ += *f++ * rdst;\
*d4++ += rdst;

static void mesh_d (const double* a, const double* b, const double* c,
                           const double* d, const double* e, const double* f,
                           const double k, const double k2,
                           double* d1, double* d2, double* d3, double* d4, const int n) {

    register unsigned int i = n;
    register double rdst;
    unsigned int correction = n % 8;
    i -= correction;

    while (i) {
        MESH_D_INNER;
        MESH_D_INNER;
        MESH_D_INNER;
        MESH_D_INNER;
        MESH_D_INNER;
        MESH_D_INNER;
        MESH_D_INNER;
        MESH_D_INNER;
        i -= 8;
    }

    while (correction--) {
        MESH_D_INNER;
    }
}

void artistic_smooth (uint8_t* src,
                      uint8_t* dst,
                      double ns,
                      double q,
                      int nx, int ny, int pitch,
                      fft_plans_t* f,
                      artistic_buf_t* ab)
{
    int i, j, k, x, y;
    const int width = 2*(nx/2+1);
    const double nxny = nx*ny;
    const double nxny2 = nxny*nxny;
    const int n = ny*(nx/2+1);
    const int n2 = n*2;

    (void) q;

    fftw_complex** src1_c = ab->src1;
    fftw_complex** src2_c = ab->src2;
    fftw_complex** s_c = ab->s;
    fftw_complex** m_c = ab->m;

    double** num = ab->num;
    double* den = ab->den;

    double* src1_d[3];
    double* src2_d[3];
    double* s_d[3];
    double* m_d[3];

    for (k = 0; k < 3; k++) {
        src1_d[k]   = (double*) src1_c[k];
        src2_d[k]   = (double*) src2_c[k];
        s_d[k]      = (double*) s_c[k];
        m_d[k]      = (double*) m_c[k];
    }
                     
    for (j = 0; j < ny; j++) {
        uint8_t* data = src + j*pitch;
        double* src1_ptr[] = {src1_d[0]+j*width, src1_d[1]+j*width, src1_d[2]+j*width};
        double* src2_ptr[] = {src2_d[0]+j*width, src2_d[1]+j*width, src2_d[2]+j*width};

        for (i = 0; i < nx; i++) {
            for (k = 0; k < 3; k++) {
                register double temp = *src1_ptr[k]++ = *data++;
                *src2_ptr[k]++ = temp*temp;
            }
        }
    }

    for (k = 0; k < 3; k++) {
        fftw_execute_dft_r2c (f->forward, src1_d[k], src1_c[k]);
        fftw_execute_dft_r2c (f->forward, src2_d[k], src2_c[k]);
    }

    for (k = 0; k < 3; k++) {
        memset (num[k], 0, sizeof(double)*n2);
    }
    memset (den, 0, sizeof(double)*n2);

    for (i = 0; i < ns; i++) {
        fftw_complex* g_fft = ab->g[i];

        multiply_6_c(m_c[0], src1_c[0],
                     m_c[1], src1_c[1],
                     m_c[2], src1_c[2],
                     s_c[0], src2_c[0],
                     s_c[1], src2_c[1],
                     s_c[2], src2_c[2],
                     g_fft, n);

        for (k = 0; k < 3; k++) {
            fftw_execute_dft_c2r (f->backward, m_c[k], m_d[k]);
            fftw_execute_dft_c2r (f->backward, s_c[k], s_d[k]);
        }

        mesh_d (s_d[0], s_d[1], s_d[2], m_d[0], m_d[1], m_d[2], nxny, nxny2,
                num[0], num[1], num[2], den, n2);
    }

    for (k = 0; k < 3; k++) {
        divide_mul_const_d(num[k], den, nxny, n2);
    }

    x = nx/2.0;
    y = ny/2.0;
    for (j = 0; j < ny; j++) {
        int k = (j - y) % ny;
        k = (k < 0 ? ny + k : k) * 2*(nx/2+1);
        uint8_t* d = dst + j*pitch;
        for (i = 0; i < nx; i++) {
            int z;
            int h = (i - x) % nx;
            h = (h < 0 ? nx + h : h) + k;
            for (z = 0; z < 3; z++) {
                *d++ = num[z][h];
            }
        }
    }
}

int artistic_proc_exec (plugin_context* ctx,
                        int             thread_id,
                        image_t**       src_data,
                        image_t**       dst_data)
{
    artistic_proc_context* c;
    int nx;
    int ny;
    int pitch;
    int ns = 8;
    image_t* sim;
    image_t* dim;

    if (NULL == (sim = *src_data) || NULL != *dst_data) {
        return -1;
    }

    pthread_mutex_lock (&ctx->mutex);
    if (NULL == ctx->data) {
        if (init_global_bufs (ctx, sim->width, sim->height, g_sgm, ns)) {
            return -1;
        }
    }
    pthread_mutex_unlock (&ctx->mutex);

    if (NULL == (c = (artistic_proc_context*) ctx->data) ||
        NULL == c->p)
    {
        return -1;
    }

    if (c->width != sim->width || c->height != sim->height)
    {
      printf("artistic: frame size mismatch\n");
      return -1;
    }

    if (NULL == c->b[thread_id]) {
        if (NULL == (c->b[thread_id] = malloc (sizeof(artistic_buf_t))))
        {
            return -1;
        }
        if (init_thread_bufs (ctx, thread_id)) {
            return -1;
        }
    }

    nx = sim->width;
    ny = sim->height;
    pitch = sim->bpp/8 * nx;

    if (NULL == (dim = calloc (1, sizeof(image_t))) ||
        NULL == (dim->pix = malloc (sizeof(uint8_t)*ny*pitch)))
    {
        return -1;
    }

    dim->width = nx;
    dim->height = ny;
    dim->bpp = sim->bpp;
    dim->fmt = FMT_RGB24;
    dim->frame = sim->frame;

    artistic_smooth (sim->pix, dim->pix, ns, 8.0, nx, ny, pitch, c->p, c->b[thread_id]);

    *dst_data = dim;
    return 0;
}
