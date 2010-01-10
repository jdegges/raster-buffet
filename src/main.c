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

#include <config.h>

#include <stdlib.h>
#include <ltdl.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>

#include "image.h"
#include "plugin.h"

typedef struct plugin_entry {
    char*           path;
    lt_dlhandle     h;
    plugin_info*    pi[PLUGIN_STAGE_MAX];
} plugin_entry;

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
    if (NAME_MAX <= (size = strlen (path)) ||
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

    fprintf (stderr, "trying to open: %s\n", path);

    if (NULL == (pe->h = lt_dlopenadvise (path, advise))) {
        fprintf (stderr, "Error dlopening %s: %s\n", path, lt_dlerror());
        free (pe->path);
        free (pe);
        return -1;
    }

    lt_dladvise_destroy (&advise);

    snprintf (buf, size+7, "%s_query", path);
    if (NULL == (plugin_query = lt_dlsym (pe->h, buf))) {
        fprintf (stderr, "Error from dlsym(): %s\n", lt_dlerror());
        return -1;
    }

    // load all stages supported by this plugin
    for (stage = 0; stage < PLUGIN_STAGE_MAX; stage++) {
        plugin_query (stage, &pe->pi[stage]);
        if (plugin_query (stage, &pe->pi[stage]) < 0 ||
            NULL == &pe->pi[stage])
        {
            fprintf (stderr, "Plugin '%s' does not provide for stage %d.\n",
                     path, stage);
        }
    }
    free (buf);
    return 0;
}

int close_plugin (plugin_entry* pe) {
    int stage;

    printf("closing plugin: %s\n", pe->path);

    lt_dlclose (pe->h);
    free (pe->path);
    return 0;
}

int load_all_plugins (plugin_entry** pe_list, int pe_size) {
    DIR* dir;
    struct dirent* dp;
    const char* dname = PKGLIBDIR;
    int i;

    fprintf (stderr, "searching for plugins in: %s\n", dname);
    
    if (NULL == (dir = opendir (dname))) {
        fprintf (stderr, "Cannot open %s\n", dname);
        return -1;
    }

    for (i = 0; i < pe_size && NULL != (dp = readdir (dir)); i++) {
        char* sub;
        char* name;
        int size = strlen(dp->d_name);
        plugin_entry* pe;

        // find all files that end with ".so"
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
}

int close_all_plugins (plugin_entry** pe_list, int pe_size) {
    int i;

    for (i = 0; i < pe_size; i++) {
        if (pe_list[i]) {
            close_plugin (pe_list[i]);
            free (pe_list[i]);
        }
    }
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
    image_t* src_im = NULL;
    image_t* dst_im = NULL;
    int c;
    char* stage_options[PLUGIN_STAGE_MAX] = {0};
    plugin_entry* plugins[PLUGIN_STAGE_MAX] = {NULL};
    plugin_entry* pe_list[100] = {NULL};
    plugin_context context_list[PLUGIN_STAGE_MAX];

    lt_dlinit();

    /* { parse args */
    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"input",   required_argument,  0,  'i'},
            {"decode",  required_argument,  0,  'd'},
            {"process", required_argument,  0,  'p'},
            {"encode",  required_argument,  0,  'e'},
            {"output",  required_argument,  0,  'o'},
            {0,         0,                  0,  0}
        };

        c = getopt_long (argc, argv, "i:d:p:e:o:", long_options, &option_index);
        if (-1 == c) {
            break;
        }

        switch (c) {
            SET_STAGE_ARGS ('i', PLUGIN_STAGE_INPUT);
            SET_STAGE_ARGS ('d', PLUGIN_STAGE_DECODE);
            SET_STAGE_ARGS ('p', PLUGIN_STAGE_PROCESS);
            SET_STAGE_ARGS ('e', PLUGIN_STAGE_ENCODE);
            SET_STAGE_ARGS ('o', PLUGIN_STAGE_OUTPUT);
            case '?':
            default:
                usage ();
                return -1;
        }
    }
    /* } end parse args */

    /* { load all plugins from ./plugins */
    load_all_plugins (pe_list, 100);
    /* } end load all */

    /* { go through plugin list and pick plugins that were specified with cli */
    for (c = 0; c < PLUGIN_STAGE_MAX; c++) {
        char* value;
        char buf[1031];
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

                context_list[c].num_threads = 1;
                context_list[c].data = NULL;
                pthread_mutex_init (&context_list[c].mutex, NULL);
            }
        }
        free (value);
    }
    /* } end plugin selection */

    /* init all selected plugins in stage order */
    for (c = 0; c < PLUGIN_STAGE_MAX; c++) {
        if (plugins[c] && plugins[c]->pi[c] && plugins[c]->pi[c]->init &&
            plugins[c]->pi[c]->init (&context_list[c], 0, stage_options[c]) < 0)
        {
            fprintf (stderr, "Error executing plugin.init %s on stage %d.\n",
                     plugins[c]->path, c);
        }
    }

    /* exec all selected plugins in stage order until one of them returns with
     * an error code */
    c = PLUGIN_STAGE_MAX;
    while (c == PLUGIN_STAGE_MAX) {
        src_im = NULL;
        dst_im = NULL;
        for (c = 0; c < PLUGIN_STAGE_MAX; c++) {
            if (plugins[c]) {
                if (plugins[c]->pi[c] &&
                     (plugins[c]->pi[c]->exec &&
                        plugins[c]->pi[c]->exec (&context_list[c], 0, &src_im,
                                                 &dst_im) < 0))
                {
                    fprintf (stderr,
                             "Error executing plugin.exec %s on stage %d.\n",
                             plugins[c]->path, c);
                    image_close (src_im);
                    break;
                }
                image_close (src_im);
                src_im = dst_im;
                dst_im = NULL;
            }
        }
    }

    /* exit all selected plugins in stage order */
    for (c = 0; c < PLUGIN_STAGE_MAX; c++) {
        if (plugins[c] && plugins[c]->pi[c]->exit &&
            plugins[c]->pi[c]->exit (&context_list[c], 0) < 0 )
        {
            fprintf (stderr, "Error executing plugin.exit %s on stage %d.\n",
                     plugins[c]->path, c);
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

    return 0;
}
