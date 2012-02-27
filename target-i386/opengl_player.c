/*
 *  Plays a sequence of OpenGL calls recorded either under qemu or with opengl_server
 * 
 *  Copyright (c) 2007 Even Rouault
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* gcc -O2 -g -Wall opengl_player.c opengl_exec.c -o opengl_player -I../i386-softmmu -I. -I.. -lGL */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>

#include <GL/gl.h>
#include <X11/Xlib.h>
#include "opengl_func.h"
#include "opengl_utils.h"
    
//#include "ffmpeg/avcodec.h"

extern int last_process_id;
extern void init_process_tab(void);
extern int do_function_call(Display*, int, int, int*, char*);

typedef struct
{
  int n_used;
  unsigned int crc;
  int size;
  long long last_use;
} RecordBufferedArray;

typedef struct
{
  int size;
  long file_offset;
} ReplayBufferedArray;

#define N_BUFFERED_ARRAYS 1024
RecordBufferedArray recordBufferedArrays[N_BUFFERED_ARRAYS];

#define INSTR_WINDOW_SIZE 65536
short instrWindow[INSTR_WINDOW_SIZE];
int instrWindowCount = 0;
int instrWindowPtr = 0;
int instrWindowBeginPtr = 0;

#include "ghash.c"

#define PRIME     131

typedef struct
{
  int iFirstOccur;
  int nbOccur;
  int hash;
} SeqDesc;

typedef struct
{
  int hash;
  int offset;
} HashElt;

int sort_hash_N;
short* sort_hash_tab;
int count_compar;
int collision_detected;

static int sort_hash(const void* ptrA, const void* ptrB)
{
  const HashElt* a = (const HashElt*)ptrA;
  const HashElt* b = (const HashElt*)ptrB;
  count_compar++;
  if (a->hash == b->hash)
  {
    int j;
    for(j=0;j<sort_hash_N;j++)
    {
      if (sort_hash_tab[a->offset + j] != sort_hash_tab[b->offset + j])
      {
        collision_detected = 1;
        //fprintf(stderr, "collision de hash !\n");
        return sort_hash_tab[a->offset + j] < sort_hash_tab[b->offset + j] ? -1 : 1;
      }
    }
    return a->offset - b->offset;
  }
  else if (a->hash < b->hash)
    return -1;
  else
    return 1;
}

static int sort_seq_desc_by_occur(const void* ptrA, const void* ptrB)
{
  const SeqDesc* a = (const SeqDesc*)ptrA;
  const SeqDesc* b = (const SeqDesc*)ptrB;
  return b->nbOccur - a->nbOccur;
}

static int sort_seq_desc_by_offset(const void* ptrA, const void* ptrB)
{
  const SeqDesc* a = (const SeqDesc*)ptrA;
  const SeqDesc* b = (const SeqDesc*)ptrB;
  return a->iFirstOccur - b->iFirstOccur;
}

void find_repeated_seq(short* tab, int iStart, int length)
{
  //SimpleHashTable* tableHash = simple_hash_table_new(free);
  int N = 10;
  int i;
  int hash = 0;
  int primeN = 1;
  HashElt* tabHash = malloc(sizeof(HashElt) * (length - N + 1));
  
  short* new_tab = malloc(sizeof(short) * length);
  memcpy(&new_tab[0], &tab[iStart], sizeof(short) * (length - iStart));
  memcpy(&new_tab[length-iStart], &tab[0], sizeof(short) * iStart);
  tab = new_tab;
    
  sort_hash_N = N;
  sort_hash_tab = tab;
  
  fprintf(stderr, "(start) iStart = %d\n", iStart);
  for(i=0;i<N;i++)
  {
    primeN *= PRIME;
  }
  for(i=0;i<length;i++)
  {
    hash = hash * PRIME + tab[i];
    if (i >= N)
    {
      hash -= tab[i - N] * primeN;
    }
    if (i >= N - 1)
    {
      tabHash[i - (N - 1)].offset = i - (N - 1);
      tabHash[i - (N - 1)].hash = /*(i == 100) ? tabHash[0].hash :*/ hash;
#if 0
      void** p_n_occurences = simple_hash_table_lookup_pointer(tableHash, hash);
      if (p_n_occurences == NULL)
      {
        SeqDesc* seqDesc = malloc(sizeof(SeqDesc));
        seqDesc->iFirstOccur = i - (N - 1);
        seqDesc->nbOccur = 1;
        simple_hash_table_insert(tableHash, hash, seqDesc);
      }
      else
      {
        SeqDesc* seqDesc = (SeqDesc*)(*p_n_occurences);
        int j;
        for(j=0;j<N;j++)
        {
          if (tab[(iStart + seqDesc->iFirstOccur + j) % length] != tab[(iStart + i - (N - 1) + j) % length])
            break;
        }
        if (j != N)
          fprintf(stderr, "arg\n");
        seqDesc->nbOccur ++;
        //fprintf(stderr, "iStart = %d, i = %d, iFirstOccur = %d, nbOccur = %d\n", iStart, i, seqDesc->iFirstOccur, seqDesc->nbOccur);
      }
#endif
    }
  }
  collision_detected = 0;
  count_compar = 0;
  qsort(tabHash, length - N + 1, sizeof(HashElt), sort_hash);
  if (! collision_detected)
  {
    SeqDesc* tabSeqDesc = (SeqDesc*)malloc(sizeof(SeqDesc) * length);
    int nbSeqDesc = 0;
    int lastI = 0;
    int prevHash = tabHash[0].hash;
    for(i=1;i<length - N + 1;i++)
    {
      if (tabHash[i].hash != prevHash)
      {
        tabSeqDesc[nbSeqDesc].iFirstOccur = tabHash[lastI].offset;
        tabSeqDesc[nbSeqDesc].nbOccur = i - lastI;
        tabSeqDesc[nbSeqDesc].hash = prevHash;
        nbSeqDesc++;
        lastI = i;
        prevHash = tabHash[i].hash;
      }
    }
    tabSeqDesc[nbSeqDesc].iFirstOccur = tabHash[lastI].offset;
    tabSeqDesc[nbSeqDesc].nbOccur = i - lastI;
    nbSeqDesc++;
    qsort(tabSeqDesc, nbSeqDesc, sizeof(SeqDesc), sort_seq_desc_by_occur);
    for(i=0;i<nbSeqDesc;i++)
    {
      if (tabSeqDesc[i].nbOccur < 10)
        break;
      //fprintf(stderr, "%d %d\n", tabSeqDesc[i].iFirstOccur, tabSeqDesc[i].nbOccur);
    }
    nbSeqDesc = i;
    qsort(tabSeqDesc, nbSeqDesc, sizeof(SeqDesc), sort_seq_desc_by_offset);
    lastI = tabSeqDesc[0].iFirstOccur;
    int maxI = 0;
    int j = 0;
    for(i=1;i<nbSeqDesc;i++)
    {
      if (tabSeqDesc[i].iFirstOccur - lastI <= N)
      {
        if (tabSeqDesc[i].nbOccur > tabSeqDesc[maxI].nbOccur)
        {
          maxI = i;
        }
      }
      else
      {
        tabSeqDesc[j].iFirstOccur = tabSeqDesc[maxI].iFirstOccur;
        tabSeqDesc[j].nbOccur = tabSeqDesc[maxI].nbOccur;
        j++;
#define MAX(a,b) (((a)>(b)) ? (a) : (b))
        lastI = tabSeqDesc[maxI].iFirstOccur;
        while(i < nbSeqDesc && tabSeqDesc[i].iFirstOccur - lastI <= N) i++;
        if (i == nbSeqDesc) break;
        maxI = i;
      }
    }
    nbSeqDesc = j;
    qsort(tabSeqDesc, nbSeqDesc, sizeof(SeqDesc), sort_seq_desc_by_occur);
    for(i=0;i<nbSeqDesc;i++)
    {
      fprintf(stderr, "offset=%d occurNb=%d hash=%d:", tabSeqDesc[i].iFirstOccur, tabSeqDesc[i].nbOccur, tabSeqDesc[i].hash);
      for(j=0;j<N;j++)
        fprintf(stderr,"%d ", tab[tabSeqDesc[i].iFirstOccur + j]);
      fprintf(stderr, "\n");
    }
    free(tabSeqDesc);
  }
  else
  {
    fprintf(stderr, "collision de hash !\n");
  }
  
  fprintf(stderr, "(end) iStart = %d (count_compar = %d)\n", iStart, count_compar);
  //simple_hash_table_foreach(tableHash, 
  free(tabHash);
  free(tab);
#if 0
  simple_hash_table_destroy(tableHash);
#endif
#if 0
  int i, j, k;
  for(i=0;i<length;i++)
  {
    int nMaxContiguous = 0;
    int jMax = 0;
    for(j=0;j<length;j++)
    {
      int nContiguous = 0;
      for(k=0;k<j - i;k++)
      {
        if (tab[(iStart + i + k) % length] == tab[(iStart + i + j + k) % length])
        {
          nContiguous++;
        }
        else
          break;
      }
      if (nContiguous >= nMaxContiguous)
      {
        nMaxContiguous = nContiguous;
        jMax = j;
      }
    }
    fprintf(stderr, "iStart = %d, i = %d, j = %d, nContiguous = %d\n", iStart, i, i + jMax, nMaxContiguous);
  }
#endif
}

#define MAX_SERVER_STATE_STACK_SIZE 16

typedef struct
{
  GLbitfield     mask;
  int            matrixMode;
  int            lastMatrixOp;
} ServerState;

typedef struct
{
  ServerState stackAttrib[MAX_SERVER_STATE_STACK_SIZE];
  int stackAttribPtr;
  int matrixMode;
  int lastMatrixOp;
} GLState;

#define NB_STATES       100
GLState states[NB_STATES];

void usage()
{
  printf("Usage : opengl_player [OPTION] filename\n\n");
  printf("filename is the file where to read the OpenGL flow ('/tmp/debug_gl.bin' by default)\n");
  printf("'-' is supported and stands for standard input\n\n");
  printf("The following options are available :\n");
  printf("--debug                       : output debugging trace on stderr\n");
  printf("--disable-real-time-play-back : play as fast as possible\n");
  printf("--show-hard-disk-bandwidth    : displays regularly the data bandwidth\n");
  printf("--h or --help                 : display this help\n");
}

int main(int argc, char* argv[])
{
  static int args_size[50];
  int i;
  int visualid_fbconfig_read = -1;
  int visualid_fbconfig_real = 0;
  char* filename = "/tmp/debug_gl.bin";
  int slowdown = 0;
  int debug = 0;
  char* ret_string;
  int args[50];
  Display* dpy = NULL;
  struct timeval start_time, last_time, current_time;
  int noplay = 0;
  int count_last_time = 0, count_current = 0;
  int refresh_rate = 500;
  int disable_real_time_play_back = 0;
  int show_hard_disk_bandwith = 0;
  int last_offset = 0;
  int resize = 0;
  int window_width = 0, window_height = 0;
  int orig_window_width = 0;
  int show_offset = 0;
  int show_diff_offset = 0;
  int last_cmd_offset = 0;
  FILE* compressed_file = NULL;
  int sizeBufferedArraysPlay = 0;
  ReplayBufferedArray* replayBufferedArrays = NULL;
  int currentState = 0;
  
  memset(recordBufferedArrays, 0, sizeof(recordBufferedArrays));
  memset(states, 0, sizeof(states));
  for(i=0;i<NB_STATES;i++)
  {
    states[i].matrixMode = GL_MODELVIEW;
    states[i].lastMatrixOp = -1;
  }
  
  /*avcodec_init();
  AVCodec* avCodec = avcodec_find_encoder(CODEC_ID_MPEG2VIDEO);
  AVCodecContext* avCodecContext = avcodec_alloc_context();*/

  for(i=1;i<argc;i++)
  {
    if (argv[i][0] == '-' && argv[i][1] == '-')
      argv[i] = argv[i]+1;

    if (strncmp(argv[i], "-slowdown=", strlen("-slowdown=")) == 0)
    {
      slowdown = atoi(argv[i] + strlen("-slowdown="));
    }
    else if (strcmp(argv[i], "-noplay") == 0)
    {
      noplay = 1;
    }
    else if (strcmp(argv[i], "-debug") == 0)
    {
      debug = 1;
    }
    else if (strcmp(argv[i], "-disable-real-time-play-back") == 0)
    {
      disable_real_time_play_back = 1;
    }
    else if (strcmp(argv[i], "-show-hard-disk-bandwidth") == 0)
    {
      show_hard_disk_bandwith = 1;
    }
    else if (strcmp(argv[i], "-show-offset") == 0)
    {
      show_offset = 1;
    }
    else if (strncmp(argv[i], "-show-diff-offset", strlen( "-show-diff-offset")) == 0)
    {
      show_diff_offset = atoi(argv[i] +  strlen("-show-diff-offset="));
    }
    else if (strncmp(argv[i], "-width=", strlen("-width=")) == 0)
    {
      window_width = atoi(argv[i] +  strlen("-width="));
      resize = 1;
    }
    else if (strncmp(argv[i], "-refreshrate=", strlen("-refreshrate=")) == 0)
    {
      refresh_rate = atoi(argv[i] + strlen("-refreshrate="));
    }
    else if (strncmp(argv[i], "-output-compressed-filename=", strlen("-output-compressed-filename=")) == 0)
    {
      char* compressed_filename = argv[i] + strlen("-output-compressed-filename=");
      compressed_file = fopen(compressed_filename, "wb");
    }
    else if (strcmp(argv[i], "-") == 0)
    {
      filename = NULL;
    }
    else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0)
    {
      usage();
      return 0;
    }
    else
      filename = argv[i];
  }
  FILE* f = (filename) ? fopen(filename, "rb") : stdin;
  if (f == NULL)
  {
    fprintf(stderr, "cannot open %s\n", filename);
    return 0;
  }
  //FILE* fout = fopen("/tmp/raw", "wb");
  
  dpy = XOpenDisplay(NULL);
  init_process_tab();
  ret_string = malloc(32768);
  
  gettimeofday(&start_time, NULL);
  gettimeofday(&last_time, NULL);
  
//  void* buffer= NULL;
#define WRITE_UNSIGNED_CHAR(x) do { unsigned char myvar = x; fwrite(&myvar, sizeof(myvar), 1, compressed_file); } while(0)
#define WRITE_SHORT(x) do { short myvar = x; fwrite(&myvar, sizeof(myvar), 1, compressed_file); } while(0)
#define WRITE_INT(x) do { int myvar = x; fwrite(&myvar, sizeof(myvar), 1, compressed_file); } while(0)
#define WRITE_LONGLONG(x) do { long long myvar = x; fwrite(&myvar, sizeof(myvar), 1, compressed_file); } while(0)
#define WRITE_FLOAT(x) do { float myvar = (float)x; fwrite(&myvar, sizeof(myvar), 1, compressed_file); } while(0)
#define WRITE_3DOUBLE_AS_3FLOAT(x) do { float myfloats[] = { x[0], x[1], x[2] }; fwrite(myfloats, sizeof(myvar), 1, compressed_file); } while(0)
  
  //FILE* fopcodes = fopen("/tmp/opcodes.bin", "wb");
  
  long long instr_count = 0;
  
  while(1)
  {
    short func_number;
    short memorize_array_play = -1;
    short reuse_array_play = -1;
    
begin:
    if (fread(&func_number, sizeof(short), 1, f) == 0)
      break;
    if (func_number == _exit_process_func)
      break;
    
    if (func_number == _serialized_calls_func)
    {
      continue;
    }
    
    //fwrite(&func_number, sizeof(short), 1, fopcodes);
    
    /*
    instrWindow[instrWindowPtr] = func_number;
    instrWindowPtr++;
    if (instrWindowPtr == INSTR_WINDOW_SIZE)
      instrWindowPtr = 0;
    instrWindowCount++;
    if (instrWindowCount >= INSTR_WINDOW_SIZE)
    {
      if ((instrWindowCount % (INSTR_WINDOW_SIZE / 2)) == 0)
      find_repeated_seq(instrWindow, instrWindowBeginPtr, INSTR_WINDOW_SIZE);
      instrWindowBeginPtr++;
      if (instrWindowBeginPtr == INSTR_WINDOW_SIZE)
        instrWindowBeginPtr = 0;
    }*/
    
    /* -1 is special code that indicates time synchro */
    if (func_number == timesynchro_func)
    {
      long long ts_file;
      fread(&ts_file, sizeof(long long), 1, f);
      
      if (compressed_file)
      {
        WRITE_SHORT(timesynchro_func);
        WRITE_LONGLONG(ts_file);
      }
      
      if (!disable_real_time_play_back)
      {
        gettimeofday(&current_time, NULL);
        long long ts_real= (current_time.tv_sec - start_time.tv_sec) * (long long)1000000 + current_time.tv_usec - start_time.tv_usec;
        //fprintf(stderr, "%d %d\n", (int)ts_real, (int)ts_file);
        if (ts_real < ts_file /*&& ts_file - ts_real > 100 * 1000*/) /* we're playing too fast */
        {
          //fprintf(stderr, "waiting %d usec\n", (int)(ts_file - ts_real));
          usleep((int)(ts_file - ts_real));
        }
      }
      continue;
    }
    else if (func_number == memorize_array_func)
    {
      fread(&memorize_array_play, sizeof(short), 1, f);
      goto begin;
    }
    else if (func_number == reuse_array_func)
    {
      fread(&reuse_array_play, sizeof(short), 1, f);
      goto begin;
    }
    
    assert(func_number >= 0);
    
    Signature* signature = (Signature*)tab_opengl_calls[func_number];
    int nb_args = signature->nb_args;
    int* args_type = signature->args_type;

    for(i=0;i<nb_args;i++)
    {
      switch(args_type[i])
      {
        case TYPE_UNSIGNED_CHAR:
        case TYPE_CHAR:
        {
          char c;
          fread(&c, sizeof(c), 1, f);
          args[i] = c;
          break;
        }

        case TYPE_UNSIGNED_SHORT:
        case TYPE_SHORT:
        {
          short s;
          fread(&s, sizeof(s), 1, f);
          args[i] = s;
          break;
        }

        case TYPE_UNSIGNED_INT:
        case TYPE_INT:
        {
          fread(&args[i], sizeof(int), 1, f);
          break;
        }

        case TYPE_FLOAT:
          fread(&args[i], sizeof(int), 1, f);
          break;

        case TYPE_NULL_TERMINATED_STRING:
        CASE_IN_UNKNOWN_SIZE_POINTERS:
          fread(&args_size[i], sizeof(int), 1, f);
          if (args_size[i])
          {
            args[i] = (long)malloc(args_size[i]);
            fread((void*)args[i], args_size[i], 1, f);
          }
          else
          {
            if (!IS_NULL_POINTER_OK_FOR_FUNC(func_number))
            {
              fprintf(stderr, "call %s arg %d\n", tab_opengl_calls_name[func_number], i);
            }
            args[i] = 0;
          }
          if (reuse_array_play >= 0)
          {
            long current_pos = ftell(f);
            args_size[i] = replayBufferedArrays[reuse_array_play].size;
            args[i] = (long)malloc(args_size[i]);
            fseek(f, replayBufferedArrays[reuse_array_play].file_offset, SEEK_SET);
            fread((void*)args[i], args_size[i], 1, f);
            fseek(f, current_pos, SEEK_SET);
          }
          if (memorize_array_play >= 0)
          {
            assert (memorize_array_play <= sizeBufferedArraysPlay);
            if (memorize_array_play == sizeBufferedArraysPlay)
            {
              sizeBufferedArraysPlay++;
              replayBufferedArrays = realloc(replayBufferedArrays, sizeBufferedArraysPlay * sizeof(ReplayBufferedArray));
              replayBufferedArrays[sizeBufferedArraysPlay-1].file_offset = 0;
            }
            replayBufferedArrays[memorize_array_play].file_offset = ftell(f) - args_size[i];
            replayBufferedArrays[memorize_array_play].size = args_size[i];
          }
          break;
          
        CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
        {
          args_size[i] = compute_arg_length(stderr, func_number, i, args);
          args[i] = (args_size[i]) ? (long)malloc(args_size[i]) : 0;
          fread((void*)args[i], args_size[i], 1, f);
          break;
        }
        
        CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
        {
          args_size[i] = compute_arg_length(stderr, func_number, i, args);
          args[i] = (long)malloc(args_size[i]);
          break;
        }
          
        CASE_OUT_UNKNOWN_SIZE_POINTERS:
        {
          fread(&args_size[i], sizeof(int), 1, f);
          if (args_size[i])
          {
            args[i] = (long)malloc(args_size[i]);
          }
          else
          {
            if (!IS_NULL_POINTER_OK_FOR_FUNC(func_number))
            {
              fprintf(stderr, "call %s arg %d\n", tab_opengl_calls_name[func_number], i);
            }
            args[i] = 0;
          }
          break;
        }

        CASE_OUT_KNOWN_SIZE_POINTERS:
        {
          args_size[i] = tab_args_type_length[args_type[i]];
          assert(args_size[i]);
          args[i] = (long)malloc(args_size[i]);
          break;
        }

        case TYPE_DOUBLE:
        CASE_IN_KNOWN_SIZE_POINTERS:
          args_size[i] = tab_args_type_length[args_type[i]];
          args[i] = (long)malloc(args_size[i]);
          fread((void*)args[i], args_size[i], 1, f);
          break;

        case TYPE_IN_IGNORED_POINTER:
          args[i] = 0;
          break;

        default:
          fprintf(stderr, "shouldn't happen : call %s arg %d\n", tab_opengl_calls_name[func_number], i);
          last_process_id = 0;
          return 0;
          break;
      }
    }
    if (debug) display_gl_call(stderr, func_number, args, args_size);
    
    if (debug && reuse_array_play != -1) fprintf(stderr, "reuse_array_play %d\n", reuse_array_play);
    if (debug && memorize_array_play != -1) fprintf(stderr, "memorize_array_play %d\n", memorize_array_play);
    
    if (compressed_file)
    {
      int reuse_array = -1;
      int memorize_array = -1;
      void* ptr = NULL;
      int bytes_size = 0;
      int do_default = 1;

      switch (func_number)
      {
        case glXMakeCurrent_func:
        {
          currentState = args[2];
          assert(currentState >= 0 && currentState < NB_STATES);
          break;
        }

        case glMatrixMode_func:
        {
          if (states[currentState].matrixMode == args[0])
          {
            do_default = 0;
          }
          else
          {
            states[currentState].matrixMode = args[0];
          }
          break;
        }
        
        case glLoadIdentity_func:
        case glLoadMatrixd_func:
        case glLoadMatrixf_func:
        case glMultMatrixd_func:
        case glMultMatrixf_func:
        case glOrtho_func:
        case glFrustum_func:
        case glRotated_func:
        case glRotatef_func:
        case glScaled_func:
        case glScalef_func:
        case glTranslated_func:
        case glTranslatef_func:
        case glPushMatrix_func:
        case glPopMatrix_func:
        {
          if (states[currentState].matrixMode == 5890)
          {
            if (states[currentState].lastMatrixOp == glLoadIdentity_func &&
                func_number == glLoadIdentity_func)
            {
              do_default = 0;
            }
            states[currentState].lastMatrixOp = func_number;
          }
          break;
        }
          
        case glPushAttrib_func:
        {
          if (states[currentState].stackAttribPtr < MAX_SERVER_STATE_STACK_SIZE)
          {
            int mask = args[0];
            states[currentState].stackAttrib[states[currentState].stackAttribPtr].mask = mask;
            if (mask & GL_TRANSFORM_BIT)
            {
              states[currentState].stackAttrib[states[currentState].stackAttribPtr].matrixMode =
                  states[currentState].matrixMode;
              states[currentState].stackAttrib[states[currentState].stackAttribPtr].lastMatrixOp =
                  states[currentState].lastMatrixOp;
            }
            states[currentState].stackAttribPtr++;
          }
          break;
        }
        
        case glPopAttrib_func:
        {
          if (states[currentState].stackAttribPtr > 0)
          {
            --states[currentState].stackAttribPtr;
            if (states[currentState].stackAttrib[states[currentState].stackAttribPtr].mask & GL_TRANSFORM_BIT)
            {
              states[currentState].matrixMode =
                  states[currentState].stackAttrib[states[currentState].stackAttribPtr].matrixMode;
              states[currentState].lastMatrixOp =
                  states[currentState].stackAttrib[states[currentState].stackAttribPtr].lastMatrixOp;
            }
          }
          break;
        }
        
        case glBufferDataARB_func:
        {
          ptr = args[2];
          bytes_size = args_size[2];
          break;
        }
          
        case glVertexPointer_fake_func:
        case glTexCoordPointer_fake_func:
        case glTexCoordPointer01_fake_func:
        case glDrawElements_func:
        {
          ptr = (void*)args[nb_args - 1];
          bytes_size = args_size[nb_args - 1];
          break;
        }
        
        default:
          break;
      }
      
      if (ptr)
      {
        unsigned int crc = calc_checksum(ptr, bytes_size, 0xFFFFFFFF);
        long long minInterest = 0x7FFFFFFFFFFFFFFFLL;
        int iMinUsed = -1;

        instr_count++;

        for(i=0;i<N_BUFFERED_ARRAYS;i++)
        {
          if (recordBufferedArrays[i].crc == crc)
          {
            // fprintf(stderr, "reusing %d for crc %d\n", i, crc);
            reuse_array = i;
            //fprintf(stderr, "reuse_array %d\n", i);
            recordBufferedArrays[i].n_used++;
            recordBufferedArrays[i].last_use = instr_count;
            break;
          }
          else if (recordBufferedArrays[i].n_used == 0)
          {
            //fprintf(stderr, "memorize_array %d\n", i);
            memorize_array = i;
            recordBufferedArrays[i].n_used = 1;
            recordBufferedArrays[i].crc = crc;
            recordBufferedArrays[i].size = bytes_size;
            recordBufferedArrays[i].last_use = instr_count;
            break;
          }
          else
          {
            long long interest = N_BUFFERED_ARRAYS * recordBufferedArrays[i].n_used / (instr_count - recordBufferedArrays[i].last_use) /** recordBufferedArrays[i].size*/;
            if (interest < minInterest)
            {
              iMinUsed = i;
              minInterest = interest;
            }
          }
        }
        static unsigned int* discardedCrcs = NULL;
        static int nDiscardedCrcs = 0;
        if (i == N_BUFFERED_ARRAYS)
        {
          int j;
          for(j=0;j<nDiscardedCrcs;j++)
          {
            if (discardedCrcs[j] == crc)
            {
              fprintf(stderr, "%X was discarded before and is asked now...\n", crc);
              discardedCrcs[j] = recordBufferedArrays[iMinUsed].crc;
              break;
            }
          }
          if (j == nDiscardedCrcs)
          {
            discardedCrcs = realloc(discardedCrcs, sizeof(int) * (nDiscardedCrcs + 1));
            discardedCrcs[nDiscardedCrcs++] = recordBufferedArrays[iMinUsed].crc;
          }
          fprintf(stderr, "discarding %X\n", recordBufferedArrays[iMinUsed].crc);

          memorize_array = iMinUsed;
          recordBufferedArrays[iMinUsed].n_used = 1;
          recordBufferedArrays[iMinUsed].crc = crc;
          recordBufferedArrays[iMinUsed].size = bytes_size;
          recordBufferedArrays[iMinUsed].last_use = instr_count;
        }
      
        if (reuse_array != -1)
        {
          WRITE_SHORT(reuse_array_func);
          WRITE_SHORT(reuse_array);
        }
        else
        if (memorize_array != -1)
        {
          WRITE_SHORT(memorize_array_func);
          WRITE_SHORT(memorize_array);
        }
      }

      switch (func_number)
      {
        case glXChooseVisual_func:
        case glXQueryVersion_func:
        case glXQueryExtension_func:
        case glXGetClientString_func:
        case glXQueryExtensionsString_func:
        case glXQueryServerString_func:
        case glXGetProcAddress_fake_func:
        case glXGetProcAddress_global_fake_func:
        case glXGetConfig_func:
        case glXGetConfig_extended_func:
        case glGetIntegerv_func:
        case glGetFloatv_func:
        case glGetBooleanv_func:
        case glGetDoublev_func:
        case glIsEnabled_func:
        case _glGetError_fake_func:
        case glGetString_func:
        {
          do_default = 0;
          break;
        }
          
        case glLoadMatrixd_func:
        {
          int j;
          WRITE_SHORT(glLoadMatrixf_func);
          double* ptr = (double*)args[0];
          for(j=0;j<16;j++)
          {
            WRITE_FLOAT(ptr[j]);
          }
          do_default = 0;
          break;
        }
        
        case glMultMatrixd_func:
        {
          int j;
          WRITE_SHORT(glMultMatrixf_func);
          double* ptr = (double*)args[0];
          for(j=0;j<16;j++)
          {
            WRITE_FLOAT(ptr[j]);
          }
          do_default = 0;
          break;
        }
        
        case glTranslated_func:
        {
          WRITE_SHORT(glTranslatef_func);
          WRITE_FLOAT(*(double*)args[0]);
          WRITE_FLOAT(*(double*)args[1]);
          WRITE_FLOAT(*(double*)args[2]);
          do_default = 0;
          break;
        }
        
        case glScalef_func:
        {
          if (*(float*)&args[0] == 1 && *(float*)&args[1] == 1 && *(float*)&args[2] == 1)
            do_default = 0;
          break;
        }
        
        case glScaled_func:
        {
          if (!(*(double*)args[0] == 1 && *(double*)args[1] == 1 && *(double*)args[2] == 1))
          {
            WRITE_SHORT(glScalef_func);
            WRITE_FLOAT(*(double*)args[0]);
            WRITE_FLOAT(*(double*)args[1]);
            WRITE_FLOAT(*(double*)args[2]);
          }
          do_default = 0;
          break;
        }
        
        case glRotated_func:
        {
          WRITE_SHORT(glRotatef_func);
          WRITE_FLOAT(*(double*)args[0]);
          WRITE_FLOAT(*(double*)args[1]);
          WRITE_FLOAT(*(double*)args[2]);
          WRITE_FLOAT(*(double*)args[3]);
          do_default = 0;
          break;
        }
#define IS_SHORT(x) ((x) >= -32768 && (x) < 32768)
        
        case glRasterPos2i_func:
        {
          if (IS_SHORT(args[0]) && IS_SHORT(args[1]))
          {
            WRITE_SHORT(glRasterPos2s_func);
            WRITE_SHORT(args[0]);
            WRITE_SHORT(args[1]);
            do_default = 0;
          }
          break;
        }
        
        case glVertex2i_func:
        {
          if (IS_SHORT(args[0]) && IS_SHORT(args[1]))
          {
            WRITE_SHORT(glVertex2s_func);
            WRITE_SHORT(args[0]);
            WRITE_SHORT(args[1]);
            do_default = 0;
          }
          break;
        }
        
        case glTexCoord2i_func:
        {
          if (IS_SHORT(args[0]) && IS_SHORT(args[1]))
          {
            WRITE_SHORT(glTexCoord2s_func);
            WRITE_SHORT(args[0]);
            WRITE_SHORT(args[1]);
            do_default = 0;
          }
          break;
        }
        
        case glTexCoord2fv_func:
        {
          float* ptr = (float*)args[0];
          float u = ptr[0];
          float v = ptr[1];
          if (fabs(u - (int)u) < 1e-7 && fabs(v - (int)v) < 1e-7)
          {
            int ui = (int)u;
            int vi = (int)v;
            if (IS_SHORT(ui) && IS_SHORT(vi))
            {
              WRITE_SHORT(glTexCoord2s_func);
              WRITE_SHORT(ui);
              WRITE_SHORT(vi);
              do_default = 0;
            }
          }
          break;
        }
        
        case glTexCoord2f_func:
        {
          float u = *(float*)&args[0];
          float v = *(float*)&args[1];
          if (fabs(u - (int)u) < 1e-7 && fabs(v - (int)v) < 1e-7)
          {
            int ui = (int)u;
            int vi = (int)v;
            if (IS_SHORT(ui) && IS_SHORT(vi))
            {
              WRITE_SHORT(glTexCoord2s_func);
              WRITE_SHORT(ui);
              WRITE_SHORT(vi);
              do_default = 0;
            }
          }
          break;
        }
        
        case glColor3f_func:
        {
          WRITE_SHORT(glColor3ub_func);
          WRITE_UNSIGNED_CHAR(255 * *(float*)&args[0]);
          WRITE_UNSIGNED_CHAR(255 * *(float*)&args[1]);
          WRITE_UNSIGNED_CHAR(255 * *(float*)&args[2]);
          do_default = 0;
          break;
        }
        
        case glColor4f_func:
        {
          if (*(float*)&args[3] == 1)
          {
            WRITE_SHORT(glColor3ub_func);
            WRITE_UNSIGNED_CHAR(255 * *(float*)&args[0]);
            WRITE_UNSIGNED_CHAR(255 * *(float*)&args[1]);
            WRITE_UNSIGNED_CHAR(255 * *(float*)&args[2]);
          }
          else
          {
            WRITE_SHORT(glColor4ub_func);
            WRITE_UNSIGNED_CHAR(255 * *(float*)&args[0]);
            WRITE_UNSIGNED_CHAR(255 * *(float*)&args[1]);
            WRITE_UNSIGNED_CHAR(255 * *(float*)&args[2]);
            WRITE_UNSIGNED_CHAR(255 * *(float*)&args[3]);
          }
          do_default = 0;
          break;
        }
        
        case glColor4fv_func:
        {
          float* ptr = (float*)args[0];
          if (ptr[3] == 1)
          {
            WRITE_SHORT(glColor3ub_func);
            WRITE_UNSIGNED_CHAR(255 * ptr[0]);
            WRITE_UNSIGNED_CHAR(255 * ptr[1]);
            WRITE_UNSIGNED_CHAR(255 * ptr[2]);
          }
          else
          {
            WRITE_SHORT(glColor4ub_func);
            WRITE_UNSIGNED_CHAR(255 * ptr[0]);
            WRITE_UNSIGNED_CHAR(255 * ptr[1]);
            WRITE_UNSIGNED_CHAR(255 * ptr[2]);
            WRITE_UNSIGNED_CHAR(255 * ptr[3]);
          }
          do_default = 0;
          break;
        }
        
        case glColor3d_func:
        {
          WRITE_SHORT(glColor3ub_func);
          WRITE_UNSIGNED_CHAR(255 * *(double*)args[0]);
          WRITE_UNSIGNED_CHAR(255 * *(double*)args[1]);
          WRITE_UNSIGNED_CHAR(255 * *(double*)args[2]);
          do_default = 0;
          break;
        }
        
        case glVertex3f_func:
        {
          if (*(float*)&args[2] == 0)
          {
            WRITE_SHORT(glVertex2f_func);
            WRITE_FLOAT(*(float*)&args[0]);
            WRITE_FLOAT(*(float*)&args[1]);
            do_default = 0;
          }
          break;
        }
        
        case glVertex3d_func:
        {
          WRITE_SHORT(glVertex3f_func);
          WRITE_FLOAT(*(double*)args[0]);
          WRITE_FLOAT(*(double*)args[1]);
          WRITE_FLOAT(*(double*)args[2]);
          do_default = 0;
          break;
        }
        
        case glNormal3fv_func:
        {
          float* ptr = (float*)args[0];
          float u = ptr[0];
          float v = ptr[1];
          float w = ptr[2];
          if (fabs(u - (int)u) < 1e-7 && fabs(v - (int)v) < 1e-7 && fabs(w - (int)w) < 1e-7)
          {
            int ui = (int)u;
            int vi = (int)v;
            int wi = (int)w;
            if (IS_SHORT(ui) && IS_SHORT(vi) && IS_SHORT(wi))
            {
              WRITE_SHORT(glNormal3sv_func);
              WRITE_SHORT(ui);
              WRITE_SHORT(vi);
              WRITE_SHORT(wi);
              do_default = 0;
            }
          }
          break;
        }
        
        case glNormal3d_func:
        {
          WRITE_SHORT(glNormal3f_func);
          WRITE_FLOAT(*(double*)args[0]);
          WRITE_FLOAT(*(double*)args[1]);
          WRITE_FLOAT(*(double*)args[2]);
          do_default = 0;
          break;
        }
        
        case glDrawElements_func:
        {
          int mode = args[0];
          int count = args[1];
          int type = args[2];
          if (type == GL_UNSIGNED_INT)
          {
            int j;
            unsigned int* ptr = (unsigned int*)args[3];
            for(j=0;j<count;j++)
            {
              if ((ptr[j] >> 16) != 0)
                break;
            }
            if (j == count)
            {
              WRITE_SHORT(func_number);
              WRITE_INT(mode);
              WRITE_INT(count);
              WRITE_INT(GL_UNSIGNED_SHORT);
              if (reuse_array != -1)
              {
                WRITE_INT(0);
              }
              else
              {
                WRITE_INT(count * sizeof(short));
                for(j=0;j<count;j++)
                {
                  WRITE_SHORT(ptr[j]);
                }
              }
              do_default = 0;
            }
            else
              do_default = 1;
          }
          else if (reuse_array != -1)
          {
            WRITE_SHORT(func_number);
            WRITE_INT(mode);
            WRITE_INT(count);
            WRITE_INT(type);
            WRITE_INT(0);
            do_default = 0;
          }
          else
            do_default = 1; 
          break;
        }
        
        case glBufferDataARB_func:
        {
          if (reuse_array != -1)
          {
            WRITE_SHORT(glBufferDataARB_func);
            WRITE_INT(args[0]);
            WRITE_INT(args[1]);
            WRITE_INT(0);
            WRITE_INT(args[3]);
            do_default = 0;
          }
          break;
        }
        
        case glVertexPointer_fake_func:
        case glTexCoordPointer_fake_func:
        case glTexCoordPointer01_fake_func:
        {
          int offset = 0, index = 0, size, type, stride, bytes_size;
          void* ptr;
          int countarg = 0;
          if (func_number == glVertexPointer_fake_func ||
              func_number == glTexCoordPointer_fake_func)
            offset = args[countarg++];
          if (func_number == glTexCoordPointer_fake_func)
            index = args[countarg++];
          size = args[countarg++];
          type = args[countarg++];
          stride = args[countarg++];
          bytes_size = args[countarg++];
          ptr = (void*)args[countarg++];
          if (type == GL_DOUBLE)
          {
            int count;
            int j, k;
            if (stride == 0) stride = size * sizeof(double);
            assert((bytes_size % stride) == 0);
            assert((offset % stride) == 0);
            offset = (offset / stride) * size * sizeof(float);
            count = bytes_size / stride;
            WRITE_SHORT(func_number);
            if (func_number == glVertexPointer_fake_func ||
                func_number == glTexCoordPointer_fake_func)
              WRITE_INT(offset);
            if (func_number == glTexCoordPointer_fake_func)
              WRITE_INT(index);
            WRITE_INT(size);
            WRITE_INT(GL_FLOAT);
            WRITE_INT(0);
            WRITE_INT(count * size * sizeof(float));
            if (reuse_array != -1)
            {
              WRITE_INT(0);
            }
            else
            {
              WRITE_INT(count * size * sizeof(float));
              for(j=0;j<count;j++)
              {
                for(k=0;k<size;k++)
                {
                  WRITE_FLOAT(*(double*)(ptr + j * stride + k * sizeof(double)));
                }
              }
            }
            do_default = 0;
          }
          else if (reuse_array != -1)
          {
            WRITE_SHORT(func_number);
            if (func_number == glVertexPointer_fake_func ||
                func_number == glTexCoordPointer_fake_func)
              WRITE_INT(offset);
            if (func_number == glTexCoordPointer_fake_func)
              WRITE_INT(index);
            WRITE_INT(size);
            WRITE_INT(type);
            WRITE_INT(stride);
            WRITE_INT(bytes_size);
            WRITE_INT(0);
            do_default = 0;
          }
          else
            do_default = 1;
          break;
        }
        
        default:
          break;
      }
      
      if (do_default)
      {
        fwrite(&func_number, sizeof(short), 1, compressed_file);
        for(i=0;i<nb_args;i++)
        {
          switch(args_type[i])
          {
            case TYPE_UNSIGNED_CHAR:
            case TYPE_CHAR:
            {
              char c = args[i];
              fwrite(&c, sizeof(c), 1, compressed_file);
              break;
            }
            
            case TYPE_UNSIGNED_SHORT:
            case TYPE_SHORT:
            {
              short s = args[i];
              fwrite(&s, sizeof(s), 1, compressed_file);
              break;
            }

            case TYPE_UNSIGNED_INT:
            case TYPE_INT:
            case TYPE_FLOAT:
              fwrite(&args[i], sizeof(int), 1, compressed_file);
              break;
    
            case TYPE_NULL_TERMINATED_STRING:
            CASE_IN_UNKNOWN_SIZE_POINTERS:
              fwrite(&args_size[i], sizeof(int), 1, compressed_file);
              if (args_size[i])
              {
                fwrite((void*)args[i], args_size[i], 1, compressed_file);
              }
              break;
              
            CASE_OUT_UNKNOWN_SIZE_POINTERS:
            {
              fwrite(&args_size[i], sizeof(int), 1, compressed_file);
              break;
            }
            
            CASE_OUT_KNOWN_SIZE_POINTERS:
            CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
            {
              break;
            }
    
            case TYPE_DOUBLE:
            CASE_IN_KNOWN_SIZE_POINTERS:
            CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
              fwrite((void*)args[i], args_size[i], 1, compressed_file);
              break;
    
            case TYPE_IN_IGNORED_POINTER:
              break;
    
            default:
              fprintf(stderr, "shouldn't happen : call %s arg %d\n", tab_opengl_calls_name[func_number], i);
              last_process_id = 0;
              return 0;
              break;
          }
        }
      }
    }
    
    int new_offset = ftell(f);
    if (show_offset || (show_diff_offset && new_offset - last_cmd_offset >= show_diff_offset))
    {
      fprintf(stderr, "offset = %d, diff=%d\n", new_offset, new_offset - last_cmd_offset);
    }
    last_cmd_offset = new_offset;
    
    if (func_number == glXCreateContext_func && args[1] == visualid_fbconfig_read)
    {
      args[1] = visualid_fbconfig_real;
    }
    if (debug)
    {
      if (func_number == glBindProgramARB_func)
      {
        fprintf(stderr, "glBindProgramARB_func(y, x) : x = %d\n", args[1]);
      }
      else if (func_number == glXGetProcAddress_fake_func)
      {
        fprintf(stderr, "glXGetProcAddress_fake(%s)\n", (char*)args[0]);
      }
    }
    
    if (debug && func_number == glEnable_func)
    {
      fprintf(stderr, "enable(0x%X)\n", args[0]);
    }
    else if (debug && func_number == glDisable_func)
    {
      fprintf(stderr, "disable(0x%X)\n", args[0]);
    }
    
    if (resize)
    {
      if (func_number == _moveResizeWindow_func)
      {
        int* params = (int*)args[1];
        orig_window_width = params[2];
        window_height = params[3] = params[3] * window_width / params[2];
        params[2] = window_width;
        //buffer = malloc(4 * window_width * window_height);
      }
      else if (func_number == glViewport_func || func_number == glScissor_func)
      {
        args[0] = args[0] * window_width / orig_window_width;
        args[1] = args[1] * window_width / orig_window_width;
        args[2] = args[2] * window_width / orig_window_width;
        args[3] = args[3] * window_width / orig_window_width;
      }
    }
    else if (func_number == _moveResizeWindow_func)
    {
      int* params = (int*)args[1];
      window_width = params[2];
      window_height = params[3];
      //buffer = malloc(4 * window_width * window_height);
    }
    
    int ret = (noplay) ? 0 : do_function_call(dpy, func_number, 1, args, ret_string);
    
    if (func_number == glXSwapBuffers_func)
    {
      /*glReadPixels(0, 0, window_width, window_height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
      fwrite(buffer, 4 * window_width * window_height, 1, fout);*/
      
      int diff_time;
      count_current++;
      gettimeofday(&current_time, NULL);
      diff_time = (current_time.tv_sec - last_time.tv_sec) * 1000 + (current_time.tv_usec - last_time.tv_usec) / 1000;
      if (diff_time > refresh_rate)
      {
        printf("%d frames in %.1f seconds = %.3f FPS\n",
               count_current - count_last_time,
               diff_time / 1000.,
               (count_current - count_last_time) * 1000. / diff_time);
        if (show_hard_disk_bandwith)
        {
          int current_offset = ftell(f);
          printf("bandwidth : %.1f MB/s\n", (current_offset - last_offset) * 1e-6);
          last_offset = current_offset;
        }
        last_time.tv_sec = current_time.tv_sec;
        last_time.tv_usec = current_time.tv_usec;
        count_last_time = count_current;
      }
      usleep(slowdown * 50000);
    }
    if (debug && func_number == glGenProgramsARB_func && args[0] == 1)
    {
      fprintf(stderr, "glGenProgramsARB_func(1, &x) : x = %d\n", *(int*)args[1]);
    }

    if (func_number == glXGetVisualFromFBConfig_func)
    {
      fread(&visualid_fbconfig_read, sizeof(int), 1, f);
      visualid_fbconfig_real = ret;
    }
    
    for(i=0;i<nb_args;i++)
    {
      switch(args_type[i])
      {
        case TYPE_UNSIGNED_INT:
        case TYPE_INT:
        case TYPE_UNSIGNED_CHAR:
        case TYPE_CHAR:
        case TYPE_UNSIGNED_SHORT:
        case TYPE_SHORT:
        case TYPE_FLOAT:
          break;

        case TYPE_NULL_TERMINATED_STRING:
        CASE_POINTERS:
        case TYPE_DOUBLE:
          if (args[i]) free((void*)args[i]);
          break;

        case TYPE_IN_IGNORED_POINTER:
          args[i] = 0;
          break;

        default:
          fprintf(stderr, "shouldn't happen : call %s arg %d\n", tab_opengl_calls_name[func_number], i);
          last_process_id = 0;
          return 0;
          break;
      }
    }
  }
  return 0;
}
