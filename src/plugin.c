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

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>

#include <stdio.h>

/* locate the "key:value[,|;]" string withing the args string */
int parse_args(const char* args, const int clear, char* key, char** value) {
    char* start;
    char* pos;
    char* end;

    if (args == NULL || key == NULL || value == NULL) {
        *value = NULL;
        return -1;
    }

    /* find the "key" string */
    if (NULL == (start = strcasestr(args, key))) {
        *value = NULL;
        return -1;
    }
    /*
    v-- start
    k  e  y  :  v  a  l  u  e  ;
    5  6  7  8  9  10 11 12 13 14
    */

    /* increment to the next ":" or "=" */
    for (pos = start; '\0' != *pos; pos++) {
        if (':' == *pos || '=' == *pos) {
            break;
        }
    }
    /*
    v- start v-- pos
    k  e  y  :  v  a  l  u  e  ;
    5  6  7  8  9  10 11 12 13 14
    */

    /* find the terminating character ',' or ';' */
    for (end = pos; '\0' != *end; end++) {
        if (',' == *end || ';' == *end) {
            break;
        }
    }
    /*
    v- start v-- pos           v-- end
    k  e  y  :  v  a  l  u  e  ;
    5  6  7  8  9  10 11 12 13 14
    */

    /* allocate space for the value */
    if (NULL == (*value = malloc (sizeof(char)*(end-pos)))) {
        return -1;
    }

    /* increment pos to the start of value */
    pos++;
    /*
    v- start    v-- pos        v-- end
    k  e  y  :  v  a  l  u  e  ;
    5  6  7  8  9  10 11 12 13 14
    */

    /* zero out key from args */
    char* i;
    for (i = start; clear && i < pos; i++) {
        *i = ' ';
    }

    /* copy value into output buffer and zero out value from args */
    for (i = *value; i-*value < end-pos; i++) {
        *i = pos[i-*value];
        if (clear) {
            pos[i-*value] = ' ';
        }
    }
    *i = '\0';
    if (clear) {
        pos[i-*value] = ' ';
    }
    
    return 0;
}
