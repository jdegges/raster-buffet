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

#define _BSD_SOURCE 500

#include <config.h>

#include <stdlib.h>
#include <ltdl.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>

#include <loomlib/pipeline.h>
#include <loomlib/async_queue.h>

#include "image.h"
#include "plugin.h"


#if 1 == BUILD_DEBUG
#define dprintf printf
#else
#define dprintf(...)
#endif

typedef struct plugin_entry {
    char*           path;
    lt_dlhandle     h;
    plugin_info*    pi[PLUGIN_STAGE_MAX];
} plugin_entry;

typedef struct plugin_state {
    struct async_queue* tid_queue;
    plugin_stage stage;
    plugin_entry* plugin;
    plugin_context* context;
} plugin_state;

static int nframes;
static pthread_mutex_t nframes_lock;


void usage (void) {
    fprintf (stderr, "Usage...\n");
}

int load_plugin (const char* path, plugin_entry* pe) {
    int size;
    char* buf;
    int stage;
    int (*plugin_query)(plugin_stage, plugin_info**);
    lt_dladvise advise;

    if (NULL == path ||
        pe->path != NULL ||
        pe->h != NULL)

    {
        return -1;
    }

    size = strlen (path);
    if (FILENAME_MAX <= (size = strlen (path)) ||
        NULL == (pe->path = calloc (size+1, sizeof(char))) ||
        NULL == (buf = malloc (sizeof(char)*size+7)))
    {
        free (pe);
        return -1;
    } 

    strncpy (pe->path, path, size);

    if (lt_dladvise_init (&advise) || lt_dladvise_ext (&advise) ||
        lt_dladvise_local (&advise))
    {
        fprintf (stderr, "Error setting up lt_dladvise\n");
        return -1;
    }

    if (NULL == (pe->h = lt_dlopenadvise (path, advise))) {
        fprintf (stderr, "Error dlopening %s: %s\n", path, lt_dlerror());
        free (pe->path);
        free (pe);
        return -1;
    }

    lt_dladvise_destroy (&advise);

    snprintf (buf, size+7, "%s_query", path);
    if (NULL == (plugin_query = (int (*)(plugin_stage, plugin_info**)) lt_dlsym (pe->h, buf))) {
        fprintf (stderr, "Error from dlsym(): %s\n", lt_dlerror());
        return -1;
    }

    /* load all stages supported by this plugin */
    for (stage = 0; stage < PLUGIN_STAGE_MAX; stage++) {
        plugin_query (stage, &pe->pi[stage]);
        if (0 <= plugin_query (stage, &pe->pi[stage]) &&
            NULL != &pe->pi[stage])
        {
            dprintf ("Found plugin '%s' providing for stage %d\n", path, stage);
        }
    }
    free (buf);
    return 0;
}

int close_plugin (plugin_entry* pe) {
    dprintf("Closing plugin: %s\n", pe->path);

    lt_dlclose (pe->h);
    free (pe->path);
    return 0;
}

int load_all_plugins (plugin_entry** pe_list, int pe_size) {
    DIR* dir;
    struct dirent* dp;
    const char* dname = PKGLIBDIR;
    int i;

    dprintf ("Searching for plugins in: %s\n", dname);
    
    if (NULL == (dir = opendir (dname))) {
        fprintf (stderr, "Cannot open %s\n", dname);
        return -1;
    }

    for (i = 0; i < pe_size && NULL != (dp = readdir (dir)); i++) {
        char* sub;
        char* name;
        int size = strlen(dp->d_name);
        plugin_entry* pe;

        /* find all files that end with ".so" */
        if (NULL == (sub = strstr (dp->d_name+size-3, ".so")) ||
            '\0' != sub[3])
        {
            continue;
        }

        size = sub - dp->d_name + 1;
        name = calloc (size, sizeof(char));
        strncpy (name, dp->d_name, size-1);

        if (NULL == (pe = calloc (1, sizeof(plugin_entry))) ||
            load_plugin (name, pe) < 0)
        {
            fprintf(stderr,"ERROR, returning -1 MEMORY LEAK!\n");
            return -1;
        }

        pe_list[i] = pe;
        free (name);
    }

    if (closedir (dir)) {
        fprintf (stderr, "Cannot close %s\n", dname);
        return -1;
    }

    dprintf ("\n");
    return 0;
}

void close_all_plugins (plugin_entry** pe_list, int pe_size) {
    int i;

    for (i = 0; i < pe_size; i++) {
        if (pe_list[i]) {
            close_plugin (pe_list[i]);
            free (pe_list[i]);
        }
    }
}

static void
exec_plugin (struct async_queue* tid_queue,
                  int stage,
                  image_t** src_im,
                  image_t** dst_im,
                  plugin_entry* plugin,
                  plugin_context* context)
{
    int* tid = async_queue_pop (tid_queue, true);
    if (plugin->pi[stage]->exec(context, *tid, src_im, dst_im) < 0) {
        fprintf (stderr,
                 "Error executing plugin.exec %s on stage %d\n",
                 plugin->path,
                 stage);
        *dst_im = NULL;
    }
    async_queue_push (tid_queue, tid);
    image_close (*src_im);
}

static void *
exec_inlet_plugin (void* data)
{
    plugin_state *args = data;
    image_t* src_im = NULL;
    image_t* dst_im = NULL;

    pthread_mutex_lock (&nframes_lock);
    if (0 == nframes) {
        pthread_mutex_unlock (&nframes_lock);
        return NULL;
    }
    nframes = 0 < nframes ? nframes - 1 : nframes;
    pthread_mutex_unlock (&nframes_lock);

    exec_plugin (args->tid_queue, args->stage, &src_im, &dst_im,
                 args->plugin, args->context);

    return dst_im;
}

static void *
exec_pump_plugin (void* data, void* product)
{
    plugin_state *args = data;
    image_t* src_im = product;
    image_t* dst_im = NULL;

    exec_plugin (args->tid_queue, args->stage, &src_im, &dst_im,
                 args->plugin, args->context);

    return dst_im;
}

static void
exec_outlet_plugin (void* data, void* product)
{
    plugin_state *args = data;
    image_t* src_im = product;
    image_t* dst_im = NULL;

    exec_plugin (args->tid_queue, args->stage, &src_im, &dst_im,
                 args->plugin, args->context);
}

#define SET_STAGE_ARGS(opt, stage) {                                    \
    case opt:                                                           \
        if (NULL == (stage_options[stage] = calloc (strlen(optarg)+1,   \
                                                    sizeof(char))))     \
        {                                                               \
            return -1;                                                  \
        }                                                               \
        strncpy (stage_options[stage], optarg, strlen(optarg));         \
        break;                                                          \
}

int main (int argc, char** argv) {
    int c;
    char* stage_options[PLUGIN_STAGE_MAX] = {0};
    plugin_entry* plugins[PLUGIN_STAGE_MAX] = {NULL};
    plugin_entry* pe_list[100] = {NULL};
    plugin_context context_list[PLUGIN_STAGE_MAX];
    size_t parallel = 1;
    size_t tid;
    nframes = -1;

    pthread_mutex_init (&nframes_lock, NULL);

    lt_dlinit();
    lt_dlsetsearchpath(PKGLIBDIR);

    /* { parse args */
    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"input",     required_argument,  0,  'i'},
            {"decode",    required_argument,  0,  'd'},
            {"process",   required_argument,  0,  'p'},
            {"encode",    required_argument,  0,  'e'},
            {"output",    required_argument,  0,  'o'},
            {"parallel",  required_argument,  0,  'j'},
            {"frames",    required_argument,  0,  'f'},
            {0,           0,                  0,  0}
        };

        c = getopt_long (argc, argv, "i:d:p:e:o:j:f:", long_options, &option_index);
        if (-1 == c) {
            break;
        }

        switch (c) {
            SET_STAGE_ARGS ('i', PLUGIN_STAGE_INPUT);
            SET_STAGE_ARGS ('d', PLUGIN_STAGE_DECODE);
            SET_STAGE_ARGS ('p', PLUGIN_STAGE_PROCESS);
            SET_STAGE_ARGS ('e', PLUGIN_STAGE_ENCODE);
            SET_STAGE_ARGS ('o', PLUGIN_STAGE_OUTPUT);
            case 'j':
                parallel = strtoul (optarg, NULL, 10);
                if (EINVAL == errno || ERANGE == errno) {
                    usage ();
                    return -1;
                }
                break;
            case 'f':
                nframes = strtoul (optarg, NULL, 10);
                if (EINVAL == errno || ERANGE == errno) {
                    usage ();
                    return -1;
                }
                if (0 == nframes) {
                    nframes = -1;
                }
                break;
            case '?':
            default:
                usage ();
                return -1;
        }
    }
    /* } end parse args */

    /* { load all plugins from ./plugins */
    if (load_all_plugins (pe_list, 100) < 0) {
        return -1;
    }
    /* } end load all */

    /* { go through plugin list and pick plugins that were specified with cli */
    dprintf ("Active plugin summary:\n");
    for (c = 0; c < PLUGIN_STAGE_MAX; c++) {
        char* value;
        int i;

        if (!stage_options[c]) {
            continue;
        }

        if (parse_args (stage_options[c],  0, "plugin", &value) < 0) {
            continue;
        }

        for (i = 0; i < 100; i++) {
            if (pe_list[i] &&
                !strcmp (pe_list[i]->path, value))
            {
                plugins[c] = pe_list[i];

                dprintf ("Using plugin '%s' on stage %d\n", plugins[c]->path, c);

                context_list[c].num_threads = parallel;
                context_list[c].data = NULL;
                pthread_mutex_init (&context_list[c].mutex, NULL);
            }
        }
        free (value);
    }
    dprintf ("\n");
    /* } end plugin selection */

    /* TODO: spawn a new thread for each stage.
    * - each thread should have an input queue associated with it
    * - each thread should have an output queue associated with it
    *     + exceptions: input/output threads
    * - each plugin can specify the number of frames it needs to work on
    *     the system should collect the specified number of frames before
    *     invoking that stages exec function
    *     each exec() function will be given a list of source images: 
    *      {current_frame, cf-1, cf-2, cf-3, ..., cf-n}
    * - at each stage, several different plugins may need to be called in serial
    *     eg: --process plugin=foo,arg1=val1,arg2=val2; \
    *                   plugin=bar,arg1=val1,arg2=val2;
    *     here, the thread managing this stage should invoke foo.exec() followed by bar.exec()
    *
    *     the core will need to handle the case where 'bar' requires a different number of frames
    *     than 'foo'. in which case a frame queue/cache is going to need to be implemented between the two
    * - after exec'n each of the input/decode/convert/encode/output stage plugins the input to that stage
    *     will no longer be available. the memory will be free'd
    */

    /* init all selected plugins in stage order */
    for (tid = 0; tid < parallel; tid++) {
        for (c = 0; c < PLUGIN_STAGE_MAX; c++) {
            if (plugins[c] && plugins[c]->pi[c] && plugins[c]->pi[c]->init) {
                if (plugins[c]->pi[c]->init (&context_list[c],
                                             tid,
                                             stage_options[c]) < 0)
                {
                    fprintf (stderr,
                             "Error executing plugin.init %s on stage %d.\n",
                             plugins[c]->path, c);
                    return -1;
                } else {
                    dprintf ("Initialized '%s' on stage %d\n", plugins[c]->path, c);
                }
            }
        }
    }
    dprintf ("\n");

    /* exec all selected plugins */
    {
        int* t;
        struct pipeline* pipe = pipeline_new (parallel);
        struct async_queue* tid_queue = async_queue_new ();
        plugin_state args[PLUGIN_STAGE_MAX];

        for (tid = 0; tid < parallel; tid++) {
            t = malloc (sizeof *t);
            *t = tid;
            async_queue_push (tid_queue, t);
        }

        for (c = 0; c < PLUGIN_STAGE_MAX; c++) {
            if (plugins[c] && plugins[c]->pi[c] && plugins[c]->pi[c]->exec) {
                args[c].tid_queue = tid_queue;
                args[c].stage = c;
                args[c].plugin = plugins[c];
                args[c].context = &context_list[c];

                if (PLUGIN_STAGE_INPUT == c) {
                    pipeline_add_inlet (pipe, exec_inlet_plugin, &args[c]);
                } else if (c < PLUGIN_STAGE_OUTPUT) {
                    pipeline_add_pump (pipe, exec_pump_plugin, &args[c]);
                } else {
                    pipeline_add_outlet (pipe, exec_outlet_plugin, &args[c]);
                }
            }
        }

        pipeline_execute (pipe);
        pipeline_free (pipe);

        while (NULL != (t = async_queue_pop (tid_queue, false))) {
            free (t);
        }
        async_queue_free (tid_queue);
    }

    /* exit all selected plugins in stage order */
    for (tid = 0; tid < parallel; tid++) {
        for (c = 0; c < PLUGIN_STAGE_MAX; c++) {
            if (plugins[c] && plugins[c]->pi[c]->exit &&
                plugins[c]->pi[c]->exit (&context_list[c], tid) < 0)
            {
                fprintf (stderr,
                         "Error executing plugin.exit %s on stage %d.\n",
                         plugins[c]->path, c);
            }
        }
    }

    /* { unload all plugins */
    close_all_plugins (pe_list, 100);

    for (c = 0; c < PLUGIN_STAGE_MAX; c++) {
        int status;
        if (plugins[c] &&
            (status = pthread_mutex_destroy (&context_list[c].mutex)))
        {
            fprintf (stderr, "Error destroying mutex: %s\n", strerror(status));
        }
        free (stage_options[c]);
    }
    /* } end unload plugins */

    lt_dlexit();

    pthread_mutex_destroy (&nframes_lock);
    return 0;
}
