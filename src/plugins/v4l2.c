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

#include <stddef.h>
#include "image.h"
#include "plugin.h"

typedef enum {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
} io_method;

typedef struct buffer {
    void *                  start;
    size_t                  length;
} buffer;

//typedef unsigned int fourcc;

typedef struct ia_v4l2_t {
    char                dev_name[1031];
    io_method           io;
    int                 fd;
    struct buffer *     buffers;
    unsigned int        n_buffers;
    int                 width;
    int                 height;
    unsigned int        fmt;
    data_fmt            native_fmt;
    int64_t             frame;
    int references;
} ia_v4l2_t;

//image_t*
//v4l2_readimage (ia_v4l2_t* v, ia_image_t* im);

//ia_v4l2_t*
//v4l2_open (int width, int height, const char* dev_name);

//void
//v4l2_close (ia_v4l2_t* v);

/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

/* external function definitions */
int v4l2_query (plugin_stage          stage,
                plugin_info**         pi);

int v4l2_input_init (plugin_context*  ctx,
                     int              thread_id,
                     char*            args);
int v4l2_input_exec (plugin_context*  ctx,
                     int              thread_id,
                     image_t**        src_data,
                     image_t**        dst_data);
int v4l2_input_exit (plugin_context*  ctx,
                     int              thread_id);

data_fmt fourcc_to_native (unsigned int fmt);
unsigned int native_to_fourcc (data_fmt fmt);

/* input plugin configuration */
static const char input_name[] = "v4l2_input";
static const data_fmt input_src_fmt[] = {-1};
static const data_fmt input_dst_fmt[] = {FMT_RGB24, -1};
static plugin_info pi_v4l2_input = {.stage=PLUGIN_STAGE_INPUT,
                                    .type=PLUGIN_TYPE_SYNC,
                                    .src_fmt=input_src_fmt,
                                    .dst_fmt=input_dst_fmt,
                                    .name="v4l2_input",
                                    .init=v4l2_input_init,
                                    .exit=v4l2_input_exit,
                                    .exec=v4l2_input_exec};

int v4l2_query (plugin_stage stage, plugin_info** pi)
{
    *pi = NULL;
    switch (stage) {
        case PLUGIN_STAGE_INPUT:
            *pi = &pi_v4l2_input;
            break;
        default:
            return -1;
    }
    return 0;
}

typedef ia_v4l2_t v4l2_input_context;

#define error_exit(func, msg) { \
    fprintf (stderr, "%s_%s(): %s\n", input_name, func, msg); \
    ret_val = -1; \
    goto exit; \
}

int v4l2_input_init (plugin_context* ctx,
                     int             thread_id,
                     char*           args)
{
    v4l2_input_context* c;
    char* param;
    char* rsc;
    int width;
    int height;
    data_fmt native_fmt;
    unsigned int fmt;
    struct stat buf;
    int ret_val = -1;

    pthread_mutex_lock (&ctx->mutex);


    if (NULL != (c = ctx->data)) {
        c->references++;
        ret_val = 0;
        goto exit;
    }

    if (NULL == (c = malloc (sizeof *c))) {
        error_exit ("init", "out of memory");
    }

    c->references = 1;
    c->frame = 0;

    parse_args (args, 0, "width", &param);
    if (NULL == param || (width = atoi (param)) <= 0) {
        error_exit ("init", "invalid/missing width");
    }

    parse_args (args, 0, "height", &param);
    if (NULL == param || (height = atoi (param)) <= 0) {
        error_exit ("init", "invalid/missing height");
    }

    parse_args (args, 0, "fmt", &param);
    if (NULL == param || stofmt (param, &native_fmt, &fmt) < 0) {
        error_exit ("init", "invalid/missing desired format");
    }

    parse_args (args, 0, "rsc", &rsc);
    if (NULL == param || stat (rsc, &buf) < 0) {
        error_exit ("init", "invalid/missing v4l2 resource\n");
    }

    ret_val = v4l2_open (c, width, height, rsc, native_fmt, fmt);

    ctx->data = c;
exit:
    pthread_mutex_unlock (&ctx->mutex);
    return ret_val;
}

data_fmt fourcc_to_native (unsigned int fmt)
{
    switch (fmt) {
        case V4L2_PIX_FMT_YUYV:
          fprintf (stderr, "FOUND NATIVE FORMAT\n");
          return FMT_YUYV;
    }
    fprintf (stderr, "COULDNT FIND A SUITABLE NATIVE FORMAT\n");
    return FMT_NONE;
}

int stofmt (char* str, data_fmt* native_fmt, unsigned int* fmt)
{
    size_t len = strlen (str);
    
    switch (len)
    {
        case 3:
            *fmt = v4l2_fourcc (str[0], str[1], str[2], ' ');
            break;
        case 4:
            *fmt = v4l2_fourcc (str[0], str[1], str[2], str[3]);
            break;
        default:
            fprintf (stderr, "invalid format length: %s is %lu chars long\n", str, len);
            return -1;
    }

    *native_fmt = fourcc_to_native (*fmt);

    return 0;
}

int v4l2_input_exec (plugin_context*    ctx,
                     int                thread_id,
                     image_t**          src_data,
                     image_t**          dst_data)
{
    image_t* im;
    v4l2_input_context* v4l2_ctx;
    int ret_val;

    assert (NULL == *src_data);
    assert (NULL == *dst_data);
    assert (ctx != NULL);
    assert (ctx->data != NULL);
    assert (NULL != (v4l2_ctx = ctx->data));

    assert (im = calloc (1, sizeof *im));
    im->width = im->height = im->bpp = -1;
    im->fmt = v4l2_ctx->native_fmt;

    pthread_mutex_lock (&ctx->mutex);

    im->frame = v4l2_ctx->frame++;
    ret_val = v4l2_readimage (v4l2_ctx, im); 

    pthread_mutex_unlock (&ctx->mutex);

    *dst_data = im;

    return ret_val;
}

int v4l2_input_exit (plugin_context*    ctx,
                     int                thread_id)
{
    v4l2_input_context* v4l2_ctx;

    assert (v4l2_ctx = ctx->data);

    pthread_mutex_lock (&ctx->mutex);

    if (!--v4l2_ctx->references) {
        v4l2_close (v4l2_ctx);
    }

    pthread_mutex_unlock (&ctx->mutex);
}

static void
errno_exit                      (const char *           s)
{
    fprintf (stderr, "%s error %d, %s\n",
             s, errno, strerror (errno));

    exit (EXIT_FAILURE);
}

static int
xioctl                          (int                    fd,
                                 int                    request,
                                 void *                 arg)
{
    int r;

    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);

    return r;
}

int
read_frame          (ia_v4l2_t*             v,
                     image_t*               im)
{
    struct v4l2_buffer buf;
    unsigned int i;

    io_method io = v->io;
    int fd = v->fd;
    unsigned int n_buffers = v->n_buffers;
    struct buffer* buffers = v->buffers;


    switch (io) {
        case IO_METHOD_READ:
            assert (im->pix = malloc ((sizeof *im->pix) * buffers[0].length));
            im->size = buffers[0].length;

            if (-1 == read (fd, im->pix, buffers[0].length)) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        errno_exit ("read");
                }
            }

            break;

        case IO_METHOD_MMAP:
            CLEAR (buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        errno_exit ("VIDIOC_DQBUF");
                }
            }

            assert (im->pix = malloc ((sizeof *im->pix) * buf.bytesused));
            im->size = buf.bytesused;
            memcpy (im->pix, buffers[buf.index].start, buf.bytesused);

            if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                errno_exit ("VIDIOC_QBUF");

            break;

        case IO_METHOD_USERPTR:
            CLEAR (buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;

            if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                    /* Could ignore EIO, see spec. */

                    /* fall through */

                    default:
                        errno_exit ("VIDIOC_DQBUF");
                }
            }

            for (i = 0; i < n_buffers; ++i)
                if (buf.m.userptr == (unsigned long) buffers[i].start
                    && buf.length == buffers[i].length)
                    break;

            assert (im->pix = malloc ((sizeof *im->pix) * buf.length));
            im->size = buf.length;
            memcpy (im->pix, (void*)buf.m.userptr, buf.length);

            if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                errno_exit ("VIDIOC_QBUF");

            break;
    }

    return 1;
}

int
v4l2_readimage                        (v4l2_input_context*                v,
                                       image_t*                  im)
{
    int fd          = v->fd;

    for (;;) {
        fd_set fds;
        struct timeval tv;
        int r;

        FD_ZERO (&fds);
        FD_SET (fd, &fds);

        // Timeout.
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select (fd + 1, &fds, NULL, NULL, &tv);

        if (-1 == r) {
            if (EINTR == errno)
                continue;
            errno_exit ("select");
        }

        if (0 == r) {
            fprintf (stderr, "select timeout\n");
            exit (EXIT_FAILURE);
        }

        if (read_frame (v, im))
            break;
    
        // EAGAIN - continue select loop.
    }

    return 0;
}

static void
stop_capturing                  (ia_v4l2_t*             v)
{
    enum v4l2_buf_type type;

    io_method io    = v->io;
    int fd          = v->fd;

    switch (io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
                errno_exit ("VIDIOC_STREAMOFF");

            break;
    }
}

int
start_capturing                 (ia_v4l2_t*             v)
{
    unsigned int i;
    enum v4l2_buf_type type;

    io_method io            = v->io;
    unsigned int n_buffers  = v->n_buffers;
    int fd                  = v->fd;
    struct buffer* buffers  = v->buffers;


    switch (io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

                CLEAR (buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = i;

                if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    errno_exit ("VIDIOC_QBUF");
            }
        
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
                errno_exit ("VIDIOC_STREAMON");
            
            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

                CLEAR (buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_USERPTR;
                buf.index       = i;
                buf.m.userptr   = (unsigned long) buffers[i].start;
                buf.length      = buffers[i].length;

                if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                errno_exit ("VIDIOC_QBUF");
            }

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
                errno_exit ("VIDIOC_STREAMON");

            break;
    }

    return 0;
}

static void
uninit_device                   (ia_v4l2_t*         v)
{
    unsigned int i;

    io_method io            = v->io;
    struct buffer* buffers  = v->buffers;
    unsigned int n_buffers  = v->n_buffers;

    switch (io) {
        case IO_METHOD_READ:
            free (buffers[0].start);
            break;
        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i)
                if (-1 == munmap (buffers[i].start, buffers[i].length))
                    errno_exit ("munmap");
            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i)
                free (buffers[i].start);
            break;
    }

    free (buffers);
}

static void
init_read           (ia_v4l2_t*             v,
                     unsigned int           buffer_size)
{
    struct buffer* buffers = calloc (1, sizeof (struct buffer));

    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    buffers[0].length = buffer_size;
    buffers[0].start = malloc (buffer_size);

    if (!buffers[0].start) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    v->buffers = buffers;
}

static void
init_mmap           (ia_v4l2_t*             v)
{
    struct v4l2_requestbuffers req;

    int fd = v->fd;
    char* dev_name = v->dev_name;
    struct buffer* buffers;
    unsigned int n_buffers;

    CLEAR (req);

    req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;

    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s does not support "
                     "memory mapping\n", dev_name);
            exit (EXIT_FAILURE);
        } else {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        fprintf (stderr, "Insufficient buffer memory on %s\n",
                 dev_name);
        exit (EXIT_FAILURE);
    }

    buffers = calloc (req.count, sizeof (struct buffer));

    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        CLEAR (buf);
    
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
            errno_exit ("VIDIOC_QUERYBUF");

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
            mmap (NULL /* start anywhere */,
                  buf.length,
                  PROT_READ | PROT_WRITE /* required */,
                  MAP_SHARED /* recommended */,
                  fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start)
            errno_exit ("mmap");
    }

    v->buffers = buffers;
    v->n_buffers = n_buffers;
}

static void
init_userp          (ia_v4l2_t*                 v,
                     unsigned int               buffer_size)
{
    struct v4l2_requestbuffers req;
    unsigned int page_size;

    int fd          = v->fd;
    char* dev_name  = v->dev_name;
    struct buffer* buffers;
    int n_buffers;


    page_size = getpagesize ();
    buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

    CLEAR (req);

    req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_USERPTR;

    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s does not support "
            "user pointer i/o\n", dev_name);
            exit (EXIT_FAILURE);
        } else {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }

    buffers = calloc (4, sizeof (struct buffer));

    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
        buffers[n_buffers].length = buffer_size;
        buffers[n_buffers].start = memalign (/* boundary */ page_size,
                                             buffer_size);

        if (!buffers[n_buffers].start) {
            fprintf (stderr, "Out of memory\n");
            exit (EXIT_FAILURE);
        }
    }

    v->buffers = buffers;
    v->n_buffers = n_buffers;
}

int
init_device                     (ia_v4l2_t*                 v)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_frmsizeenum frmsize;
    unsigned int min;
    int i, j;
    unsigned int available_frmsize[500][2] = {{0,0}};
    int found_fmt;

    int fd          = v->fd;
    char* dev_name  = v->dev_name;
    io_method io    = v->io;
    int width       = v->width;
    int height      = v->height;
    unsigned int img_fmt      = v->fmt;

    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s is no V4L2 device\n",
                     dev_name);
            return -1;
        } else {
            errno_exit ("VIDIOC_QUERYCAP");
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf (stderr, "%s is no video capture device\n",
                 dev_name);
        return -1;
    }

    if (io == IO_METHOD_MMAP || io == IO_METHOD_USERPTR) {
        if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
            fprintf (stderr, "%s does not support streaming i/o\n",
                     dev_name);

            /* try read method */
            io = IO_METHOD_READ;
        }
    }

    if (io == IO_METHOD_READ) {
        if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
            fprintf (stderr, "%s does not support read i/o\n",
                     dev_name);
            return -1;
        }
    }


    /* Select video input, video standard and tune here. */


    CLEAR (cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                    break;
            }
        }
    } else {    
        /* Errors ignored. */
    }


    CLEAR (fmt);

    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = 0; 
    fmt.fmt.pix.height      = 0;
    fmt.fmt.pix.pixelformat = 0;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    // discover what pixel formats and frmsizes are supported by the camera 
    for (i = found_fmt = 0; !found_fmt; i++) {
        CLEAR (fmtdesc);
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmtdesc.index = i;

        // if enum_fmt not supported, just try what user provided
        if (-1 == xioctl (fd, VIDIOC_ENUM_FMT, &fmtdesc)) {
            if (errno == EINVAL) {
                break;
            }
        }

        // verify that the device supports the desired format and frame sizes
        if (fmtdesc.pixelformat == img_fmt) {
            // get best available frmsize 
            for (j = 0; !found_fmt; j++) {
                CLEAR (frmsize);
                frmsize.pixel_format = img_fmt;
                frmsize.index = j;

                // abort search if unable to enumerate a formats frame sizes
                if (-1 == xioctl (fd, VIDIOC_ENUM_FRAMESIZES, &frmsize)) {
                    break;
                }

                if (frmsize.discrete.width == width
                    && frmsize.discrete.height == height)
                {
                    found_fmt = 1;
                }
            }
        }
    }

    if (!found_fmt) {
        fprintf (stderr, "This device should not support the specified pixel "
                         "formats or frame size.... trying just in case...\n");
    }

    fmt.fmt.pix.pixelformat = img_fmt;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;

    if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt)) {
        printf("got error here\n");
        errno_exit ("VIDIOC_S_FMT");
    }

    /* Note VIDIOC_S_FMT may change width and height. */

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    switch (io) {
        case IO_METHOD_READ:
            init_read (v, fmt.fmt.pix.sizeimage);
            break;
        case IO_METHOD_MMAP:
            init_mmap (v);
            break;
        case IO_METHOD_USERPTR:
            init_userp (v, fmt.fmt.pix.sizeimage);
            break;
    }

    return 0;
}

static void
close_device                    (ia_v4l2_t*             v)
{
    int fd = v->fd;
    if (-1 == close (fd))
        errno_exit ("close");

    fd = -1;
}

int
open_device                     (ia_v4l2_t*             v)
{
    struct stat st;
    char* dev_name = v->dev_name;
    int fd;

    if (-1 == stat (dev_name, &st)) {
        fprintf (stderr, "Cannot identify '%s': %d, %s\n",
                 dev_name, errno, strerror (errno));
        return -1;
    }

    if (!S_ISCHR (st.st_mode)) {
        fprintf (stderr, "%s is no device\n", dev_name);
        return -1;
    }

    fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == fd) {
        fprintf (stderr, "Cannot open '%s': %d, %s\n",
        dev_name, errno, strerror (errno));
        return -1;
    }

    v->fd = fd;
    return 0;
}

int
v4l2_open                       (v4l2_input_context*    v,
                                 int                    width,
                                 int                    height,
                                 const char*            dev_name,
                                 data_fmt               native_fmt,
                                 unsigned int           fmt)
{
    if (!v) return -1;
    v->width    = width;
    v->height   = height;
    strncpy (v->dev_name, dev_name, 1031);
    v->io       = IO_METHOD_MMAP;
    v->fmt      = fmt;
    v->native_fmt = native_fmt;

    if (-1 == open_device (v)) {
        return -1;
    }

    if (-1 == init_device (v)) {
        close_device (v);
        return -1;
    }

    if (-1 == start_capturing (v)) {
        uninit_device (v);
        close_device (v);
        return -1;
    }
    return 0;
}

void
v4l2_close                      (v4l2_input_context*             v)
{
    stop_capturing (v);
    uninit_device (v);
    close_device (v);
    ia_free (v);
}
