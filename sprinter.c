/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 2021, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <curl/curl.h>

#ifdef _WIN32
#define FORMAT_SIZE_T "Iu"
#else
#define FORMAT_SIZE_T "zu"
#endif

#ifndef CURL_VERSION_BITS
#define CURL_VERSION_BITS(x,y,z) ((x)<<16|(y)<<8|(z))
#endif

#ifndef CURL_AT_LEAST_VERSION
#define CURL_AT_LEAST_VERSION(x,y,z) \
  (LIBCURL_VERSION_NUM >= CURL_VERSION_BITS(x, y, z))
#endif

#if !CURL_AT_LEAST_VERSION(7, 28, 0)
#error "this code needs libcurl 7.28.0 or later"
#endif

#ifdef _WIN32
static LARGE_INTEGER tool_freq;
#endif

#define MAXPARALLEL 500 /* max parallelism */
#define NPARALLEL 100  /* Default number of concurrent transfers */
#define NTOTAL 100000  /* Default number of transfers in total */
#define RESPCODE 200   /* Default expected response code from server */

size_t downloaded;

static size_t write_cb(char *data, size_t n, size_t l, void *userp)
{
  /* ignore the data here */
  (void)data;
  (void)userp;
  downloaded += n*l;
  return n*l;
}

static void timestamp(struct timeval *t)
{
#ifdef WIN32
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  t->tv_sec = (long)(count.QuadPart / tool_freq.QuadPart);
  t->tv_usec = (long)((count.QuadPart % tool_freq.QuadPart) * 1000000 /
                      tool_freq.QuadPart);
#else
  (void)gettimeofday(t, NULL);
#endif
}

static size_t timediff(struct timeval newer, struct timeval older)
{
  size_t diff = (size_t)newer.tv_sec-older.tv_sec;
  return diff * 1000000 + newer.tv_usec-older.tv_usec;
}

#define SPRINTER_VERSION "0.1"

int main(int argc, char **argv)
{
  CURL *handles[MAXPARALLEL];
  CURLM *multi_handle;
  int still_running = 1; /* keep number of running handles */
  int i;
  CURLMsg *msg; /* for picking up messages with the transfer status */
  int msgs_left; /* how many messages are left */
  int total;
  int add;
  struct timeval start;
  struct timeval end;
  size_t diff;
  curl_version_info_data *v;
  const char *url;
  int ntotal = NTOTAL;
  int nparallel = NPARALLEL;
  int expected_respcode = RESPCODE;

#ifdef _WIN32
  QueryPerformanceFrequency(&tool_freq);
#endif

  if(argc < 2) {
    printf("curl sprinter version %s\n"
           "Usage: sprinter <URL> [total] [parallel] [respcode]\n"
           " <URL> will be downloaded\n"
           " [total] number of times (default %d) using\n"
           " [parallel] simultaneous transfers (default %d)\n"
           " [respcode] expected response code from server (default %d)\n",
           SPRINTER_VERSION, NTOTAL, NPARALLEL, RESPCODE);
    return 1;
  }

  url = argv[1];
  if(argc > 2)
    ntotal = atoi(argv[2]);
  if(argc > 3)
    nparallel = atoi(argv[3]);
  if(argc > 4)
    expected_respcode = atoi(argv[4]);
  if(nparallel > ntotal)
    nparallel = ntotal;
  if(nparallel > MAXPARALLEL)
    nparallel = MAXPARALLEL;

  total = add = ntotal;

  /* Allocate one CURL handle per transfer */
  for(i = 0; i<nparallel; i++) {
    handles[i] = curl_easy_init();
    curl_easy_setopt(handles[i], CURLOPT_URL, url);
    curl_easy_setopt(handles[i], CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(handles[i], CURLOPT_HEADERFUNCTION, write_cb);
    curl_easy_setopt(handles[i], CURLOPT_BUFFERSIZE, 100000L);
    curl_easy_setopt(handles[i], CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(handles[i], CURLOPT_SSL_VERIFYPEER, 0L);
  }

  /* init a multi stack */
  multi_handle = curl_multi_init();
  curl_multi_setopt(multi_handle, CURLMOPT_MAXCONNECTS, (long)(nparallel + 2));

  /* add the first NPARALLEL individual transfers */
  for(i = 0; i<nparallel; i++) {
    curl_multi_add_handle(multi_handle, handles[i]);
    add--;
  }

  v = curl_version_info(CURLVERSION_NOW);

  printf("curl: %s\n"
         "URL: %s\n"
         "Transfers: %d [%d in parallel]...\n",
         v->version, url, ntotal, nparallel);
  timestamp(&start);
  do {
    CURLMcode mc = curl_multi_perform(multi_handle, &still_running);

    if(still_running) {
      /* wait for activity, timeout or "nothing" */
#if (CURL_AT_LEAST_VERSION(7, 66, 0)) && !defined(FOR_OLDER)
      mc = curl_multi_poll(multi_handle, NULL, 0, 1000, NULL);
#else
      /* should be mostly okay */
      mc = curl_multi_wait(multi_handle, NULL, 0, 1000, NULL);
#endif
    }
    if(mc)
      break;

    /* See how the transfers went */
    while((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
      if(msg->msg == CURLMSG_DONE) {
        CURL *e = msg->easy_handle;
        long respcode;
        /* anything but CURLE_OK here disqualifies this entire round */
        if(msg->data.result) {
          fprintf(stderr, "Transfer returned %d!\n", msg->data.result);
          return 2;
        }
        if(curl_easy_getinfo(e, CURLINFO_RESPONSE_CODE, &respcode) ||
           expected_respcode != respcode) {
          fprintf(stderr, "Transfer returned unexpected response code %d!\n",
                  respcode);
          return 2;
        }
        total--;
        curl_multi_remove_handle(multi_handle, e);
        if(add) {
          /* add it back in to get it restarted */
          curl_multi_add_handle(multi_handle, e);
          add--;
        }
      }
    }
  } while(total);
  timestamp(&end);

  diff = timediff(end, start);
  printf("Time: %" FORMAT_SIZE_T " us, %.2f us/transfer\n",
         diff, (double)diff/(unsigned)ntotal);
  printf("Freq: %.2f requests/second\n",
         (unsigned)ntotal / ((double)diff/1000000.0));
  printf("Downloaded: %" FORMAT_SIZE_T " bytes, %.1f GB\n",
         downloaded, downloaded/(double)(1024*1024*1024));
  printf("Speed: %.1f bytes/sec %.1f MB/s (%" FORMAT_SIZE_T
         " bytes/transfer)\n",
         downloaded / ((double)diff/1000000.0),
         (downloaded / ((double)diff/1000000.0))/(1024*1024),
         downloaded / (unsigned)ntotal);

  curl_multi_cleanup(multi_handle);

  /* Free the CURL handles */
  for(i = 0; i<nparallel; i++)
    curl_easy_cleanup(handles[i]);

  return 0;
}
