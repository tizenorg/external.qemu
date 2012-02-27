/*
 *  Guest-side implementation of GL/GLX API. Replacement of standard libGL.so
 * 
 *  Copyright (c) 2006,2007 Even Rouault
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

/* This library can also be used independly of qemu as a TCP/IP client with the opengl_server TCP/IP server */

/* gcc -Wall -g -O2 opengl_client.c -shared -o libGL.so.1 */

/* Windows compilations instructions */
/* After building qemu, cd to target-i386 */
/* i586-mingw32msvc-gcc -O2 -Wall opengl_client.c -c -I../i386-softmmu -I. -DBUILD_GL32 */
/* i586-mingw32msvc-dllwrap -o opengl32.dll --def opengl32.def -Wl,-enable-stdcall-fixup opengl_client.o -lws2_32
 */


/* objdump -T ../i386-softmmu/libGL.so | grep Base | awk '{print $7}' | grep gl | sort > opengl32_temp.def */


/*  HACK_XGL=true USE_TCP_COMMUNICATION=1 LD_LIBRARY_PATH=/home/even/qemu/cvs280207/qemu/i386-softmmu:/home/even/cairo/glitz-0.5.6/src/glx/.libs/:/home/even/cairo/glitz-0.5.6/src/.libsvalgrind Xgl -screen 800x600 :10 -ac */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <fcntl.h>

#define GL_GLEXT_LEGACY
#include "mesa_gl.h"
#include "mesa_glext.h"

#ifndef WIN32
#include <dlfcn.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>

#include <mesa_glx.h>
#else
#include <excpt.h>
#include <windows.h>
WINGDIAPI const char* WINAPI wglGetExtensionsStringARB(HDC hdc);
#endif

#define ENABLE_THREAD_SAFETY

/* Opengl character device */
#define DEV_NAME "/dev/opengl"
int fd; /* file description */

/*void *pthread_getspecific(pthread_key_t key);
  int pthread_setspecific(pthread_key_t key, const void *value);*/

#define CHECK_ARGS(x, y) (1 / ((sizeof(x)/sizeof(x[0])) == (sizeof(y)/sizeof(y[0])) ? 1 : 0)) ? x : x, y

#ifndef _WIN32

#define TCP_COMMUNICATION_SUPPORT
#define USE_TCP_METHOD
//#define USE_DEVICE_METHOD
//#define USE_SWI_METHOD
#endif
//#define PROVIDE_STUB_IMPLEMENTATION

#define NOT_IMPLEMENTED(x) log_gl(#x " not implemented !\n")

static void log_gl(const char* format, ...);

static const char* interestingEnvVars[] =
{
	"USE_TCP_COMMUNICATION",   /* default : not set */
	"GET_IMG_FROM_SERVER",     /* default : not set */ /* unsupported for Win32 guest */
	"GL_SERVER",               /* default is localhost */
	"GL_SERVER_PORT",          /* */
	"GL_ERR_FILE",             /* default is stderr */
	"HACK_XGL",                /* default : not set */ /* unsupported for Win32 guest */
	"DEBUG_GL",                /* default : not set */
	"DEBUG_ARRAY_PTR",         /* default : not set */
	"DISABLE_OPTIM",           /* default : not set */
	"LIMIT_FPS",               /* default : not set */ /* unsupported for Win32 guest */
	"ENABLE_GL_BUFFERING",     /* default : set if TCP/IP communication or KQEMU detected */
	"NO_MOVE",                 /* default : set if TCP/IP communication */
};


#ifdef WIN32

typedef struct
{
	char* name;
	char* value;
} EnvVarStruct;

static int nbEnvVar = 0;
static EnvVarStruct* envVar = NULL;

const char* my_getenv(const char* name)
{
	int i;
	for(i=0;i<nbEnvVar;i++)
	{
		if (strcmp(envVar[i].name, name) == 0)
			return envVar[i].value;
	}
	return getenv(name);
}

void my_setenv(const char* name, const char* value, int ignored)
{
	int i;
	for(i=0;i<nbEnvVar;i++)
	{
		if (strcmp(envVar[i].name, name) == 0)
		{
			free(envVar[i].value);
			envVar[i].value = strdup(value);
			return;
		}
	}
	envVar = (EnvVarStruct*)realloc(envVar, sizeof(EnvVarStruct) * (nbEnvVar + 1));
	envVar[nbEnvVar].name = strdup(name);
	envVar[nbEnvVar].value = strdup(value);
	nbEnvVar++;
}

#define getenv my_getenv
#define setenv my_setenv

#endif

#define EXT_FUNC(x) x

#define CONCAT(a, b) a##b
#define DEFINE_EXT(glFunc, paramsDecl, paramsCall)  GLAPI void APIENTRY CONCAT(glFunc,EXT) paramsDecl { glFunc paramsCall; }

#ifdef WIN32
#define portableGetProcAddress wglGetProcAddress
#else
#define portableGetProcAddress glXGetProcAddress
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

static FILE* get_err_file()
{
	static FILE* err_file = NULL;
	if (err_file == NULL)
	{
		err_file = getenv("GL_ERR_FILE") ? fopen(getenv("GL_ERR_FILE"), "wt") : NULL;
		if (err_file == NULL)
			err_file = stderr;
	}
	return err_file;
}

#include "opengl_func.h"

#include "glgetv_cst.h"

typedef struct
{
	int size;
	int type;
	int stride;
	const void* ptr;

	int index;
	int normalized;

	int enabled;
	int vbo_name;

	unsigned int last_crc;
} ClientArray;

typedef struct {
	GLboolean swapEndian;
	GLboolean lsbFirst;
	GLuint rowLength;
	GLuint imageHeight;
	GLuint skipRows;
	GLuint skipPixels;
	GLuint skipImages;
	GLuint alignment;
} PixelStoreMode;

typedef struct
{
	int format;
	int stride;
	const void* ptr;
} InterleavedArrays;

typedef struct {
	ClientArray vertexArray;
	ClientArray normalArray;
	ClientArray colorArray;
	ClientArray secondaryColorArray;
	ClientArray indexArray;
	ClientArray edgeFlagArray;
	ClientArray weightArray;
	ClientArray matrixIndexArray;
	ClientArray fogCoordArray;
	ClientArray texCoordArray[NB_MAX_TEXTURES];
	ClientArray vertexAttribArray[MY_GL_MAX_VERTEX_ATTRIBS_ARB];
	ClientArray vertexAttribArrayNV[MY_GL_MAX_VERTEX_ATTRIBS_NV];
	ClientArray variantPointer[MY_GL_MAX_VARIANT_POINTER_EXT];
	ClientArray elementArrayATI;
	InterleavedArrays interleavedArrays;
} ClientArrays;




typedef struct
{
	GLbitfield     mask;
	PixelStoreMode pack;
	PixelStoreMode unpack;
	ClientArrays   arrays;
	int clientActiveTexture;
	int selectBufferSize;
	void* selectBufferPtr;
	int feedbackBufferSize;
	void* feedbackBufferPtr;
} ClientState;


typedef struct
{
	int texture;
	int level;

	int width;
	int height;
} Texture2DDim;

typedef struct
{
	float mode;
	float density;
	float start;
	float end;
	float index;
	float color[4];
} Fog;

#define N_CLIP_PLANES   8
typedef struct
{
	GLbitfield     mask;
	int            matrixMode;
	int            bindTexture2D; /* optimization for openquartz  */
	int            bindTextureRectangle;
	int            linesmoothEnabled; /* optimization for googleearth */
	int            lightingEnabled;
	int            texture2DEnabled;
	int            blendEnabled;
	int            scissorTestEnabled;
	int            vertexProgramEnabled;
	int            fogEnabled;
	int            depthFunc;
	Fog            fog;

	Texture2DDim*  texture2DCache;
	int            texture2DCacheDim;
	Texture2DDim*  textureProxy2DCache;
	int            textureProxy2DCacheDim;
	Texture2DDim*  textureRectangleCache;
	int            textureRectangleCacheDim;
	Texture2DDim*  textureProxyRectangleCache;
	int            textureProxyRectangleCacheDim;

	double         clipPlanes[N_CLIP_PLANES][4];
} ServerState;

typedef struct
{
	int size;
	void* ptr;
	int mapped;
	int usage;
	int access;
} Buffer;

typedef struct
{
	int start;
	int length;
} IntRange;

typedef struct
{
	IntRange* ranges;
	int nb;
	int maxNb;
} IntSetRanges;

typedef struct
{
	int bufferid;
	int size;
	void* ptr;
	void* ptrMapped;
	IntSetRanges updatedRangesAfterMapping;
	void* ptrUpdatedWhileMapped;
} ObjectBufferATI;

typedef struct
{
	GLuint program;
	char* txt;
	int location;
} UniformLocation;


#include "opengl_utils.h"

/**/
#define MAX_MATRIX_STACK_SIZE  64
#define NB_GL_MATRIX    (3+NB_MAX_TEXTURES+31)
typedef struct
{
	double val[16];
} Matrixd;

typedef struct
{
	Matrixd stack[MAX_MATRIX_STACK_SIZE];
	Matrixd current;
	int sp;
} GLMatrix;

typedef struct
{
	int x;
	int y;
	int width;
	int height;
} ViewportStruct;

typedef struct
{
	int x;
	int y;
	int width;
	int height;
	int map_state;
} WindowPosStruct;


typedef struct
{
	int id;
	int datatype;
	int components;
} Symbol;

typedef struct
{
	Symbol* tab;
	int count;
} Symbols;

#define MAX_CLIENT_STATE_STACK_SIZE 16
#define MAX_SERVER_STATE_STACK_SIZE 16

typedef struct
{
	int ref;
#ifndef WIN32
	Display* display;
	GLXContext context;
	GLXDrawable current_drawable;
	GLXDrawable current_read_drawable;
	struct timeval last_swap_buffer_time;
	GLXContext shareList;
	XFixesCursorImage last_cursor;
	GLXPbuffer pbuffer;
#endif
	int isAssociatedToFBConfigVisual;

	ClientState client_state_stack[MAX_CLIENT_STATE_STACK_SIZE];
	ClientState client_state;
	int client_state_sp;

	ServerState server_stack[MAX_SERVER_STATE_STACK_SIZE];
	ServerState current_server_state;
	int server_sp;

	int            arrayBuffer; /* optimization for ww2d */
	int            elementArrayBuffer;
	int            pixelUnpackBuffer;
	int            pixelPackBuffer;

	Buffer arrayBuffers[32768];
	Buffer elementArrayBuffers[32768];
	Buffer pixelUnpackBuffers[32768];
	Buffer pixelPackBuffers[32768];

	RangeAllocator ownTextureAllocator;
	RangeAllocator* textureAllocator;
	RangeAllocator ownBufferAllocator;
	RangeAllocator* bufferAllocator;
	RangeAllocator ownListAllocator;
	RangeAllocator* listAllocator;

	Symbols symbols;

	ObjectBufferATI objectBuffersATI[32768];

	UniformLocation* uniformLocations;
	int countUniformLocations;

	WindowPosStruct oldPos;
	ViewportStruct viewport;
	ViewportStruct scissorbox;
	int drawable_width;
	int drawable_height;

	int currentRasterPosKnown;
	float currentRasterPos[4];

	char* tuxRacerBuffer;

	int activeTexture;

	int isBetweenBegin;

	int lastGlError;

	int isBetweenLockArrays;
	int locked_first;
	int locked_count;

	GLMatrix matrix[NB_GL_MATRIX];
} GLState;

static GLState* new_gl_state()
{
	int i, j;
	GLState* state;

	state = malloc(sizeof(GLState));
	memset(state, 0, sizeof(GLState));
	state->activeTexture = GL_TEXTURE0_ARB;
	state->client_state.pack.alignment = 4;
	state->client_state.unpack.alignment = 4;
	state->current_server_state.matrixMode = GL_MODELVIEW;
	state->current_server_state.depthFunc = GL_LESS;
	state->current_server_state.fog.mode = GL_EXP;
	state->current_server_state.fog.density = 1;
	state->current_server_state.fog.start = 0;
	state->current_server_state.fog.end = 1;
	state->current_server_state.fog.index = 0;
	state->textureAllocator = &state->ownTextureAllocator;
	state->bufferAllocator = &state->ownBufferAllocator;
	state->listAllocator = &state->ownListAllocator;
	state->currentRasterPosKnown = 1;
	state->currentRasterPos[0] = 0;
	state->currentRasterPos[1] = 0;
	state->currentRasterPos[2] = 0;
	state->currentRasterPos[3] = 1;
	for(i=0;i<NB_GL_MATRIX;i++)
	{
		state->matrix[i].sp = 0;
		for(j=0;j<16;j++)
		{
			state->matrix[i].current.val[j] = (j == 0 || j == 5 || j == 10 || j == 15);
		}
	}
	return state;
}

/* The access to the following global variables shoud be done under the global lock */
static GLState* default_gl_state = NULL;
static int nbGLStates = 0;
static GLState** glstates = NULL;

#define IS_GLX_CALL(x) (x >= glXChooseVisual_func && x <= glXReleaseTexImageARB_func)

#ifdef ENABLE_THREAD_SAFETY

/* Posix threading */
/* The concepts used here are coming directly from http://www.mesa3d.org/dispatch.html */

#ifndef WIN32
static pthread_mutex_t global_mutex         = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t   key_current_gl_state;
#define GET_CURRENT_THREAD()  pthread_self()

static pthread_t last_current_thread = 0;
static GLState* _mono_threaded_current_gl_state = NULL;
#ifdef TEST_IF_LOCK_USE_IS_CORRECT_INTO_MONO_THREADED_CASE
static int lock_count = 0;
#define LOCK(func_number)  int __lock__ = 0; assert(lock_count == 0); lock_count++; pthread_mutex_lock( &global_mutex );
#define UNLOCK(func_number) assert(lock_count + __lock__ == 1); lock_count--; pthread_mutex_unlock( &global_mutex );
#define IS_MT() 2
static int _is_mt = 2;
#else
static int _is_mt = 0;
static inline int is_mt()
{
	if (_is_mt) return 1;
	pthread_t current_thread = GET_CURRENT_THREAD();
	if (last_current_thread == 0)
		last_current_thread = current_thread;
	if (current_thread != last_current_thread)
	{
		_is_mt = 1;
		log_gl("-------- Two threads at least are doing OpenGL ---------\n");
		pthread_key_create(&key_current_gl_state, NULL);
	}
	return _is_mt;
}
#define IS_MT() is_mt()

/* The idea here is that the first GL/GLX call made in each thread is necessary a GLX call */
/* So in the case where it's a GLX call we always take the lock and check if we're in MT case */
/* otherwise (regular GL call), we have to take the lock only in the MT case */
#define LOCK(func_number) do { if (IS_GLX_CALL(func_number)) { pthread_mutex_lock( &global_mutex ); IS_MT(); } else if (_is_mt) {  pthread_mutex_lock( &global_mutex ); } } while(0)
#define UNLOCK(func_number) do { if (IS_GLX_CALL(func_number) || _is_mt) pthread_mutex_unlock( &global_mutex ); } while(0)
#endif
static void set_current_state(GLState* current_gl_state)
{
	if (_is_mt)
		pthread_setspecific(key_current_gl_state, current_gl_state);
	else
		_mono_threaded_current_gl_state = current_gl_state;
}
static inline GLState* get_current_state()
{
	GLState* current_gl_state;
	if (_is_mt == 1 &&
			last_current_thread == GET_CURRENT_THREAD())
	{
		_is_mt = 2;
		set_current_state(_mono_threaded_current_gl_state);
		_mono_threaded_current_gl_state = NULL;
	}
	current_gl_state = (_is_mt) ? pthread_getspecific(key_current_gl_state) : _mono_threaded_current_gl_state;
	if (current_gl_state == NULL)
	{
		if (default_gl_state == NULL)
		{
			default_gl_state = new_gl_state();
		}
		current_gl_state = default_gl_state;
		set_current_state(current_gl_state);
	}
	return current_gl_state;
}
#define SET_CURRENT_STATE(_x) set_current_state(_x)

/* Win32 threading : TODO !*/

#else
#define LOCK(func_number)
#define UNLOCK(func_number)
#define GET_CURRENT_THREAD() 0 
#define IS_MT() 0
static GLState* current_gl_state = NULL;
static inline GLState* get_current_state()
{
	if (current_gl_state == NULL)
	{
		if (default_gl_state == NULL)
		{
			default_gl_state = new_gl_state();
		}
		return default_gl_state;
	}
	return current_gl_state;
}
#define SET_CURRENT_STATE(_x) current_gl_state = _x
#endif

/* No support for threading */
#else
#define LOCK(func_number)
#define UNLOCK(func_number) 
#define GET_CURRENT_THREAD() 0
#define IS_MT() 0
static GLState* current_gl_state = NULL;
static inline GLState* get_current_state()
{
	if (current_gl_state == NULL)
	{
		if (default_gl_state == NULL)
		{
			default_gl_state = new_gl_state();
		}
		return default_gl_state;
	}
	return current_gl_state;
}
#define SET_CURRENT_STATE(_x) current_gl_state = _x
#endif


#define GET_CURRENT_STATE()   GLState* state = get_current_state()

static void log_gl(const char* format, ...)
{
#if 0
	va_list list;
	va_start(list, format);
#ifdef ENABLE_THREAD_SAFETY
	if (IS_MT())
		fprintf(get_err_file(), "[thread %p] : ", (void*)GET_CURRENT_THREAD());
#endif
	vfprintf(get_err_file(), format, list);
	va_end(list);
#endif
}


/**/

#ifndef WIN32
typedef struct
{
	int attrib;
	int value;
	int ret;
} _glXGetConfigAttribs;

typedef struct
{
	int val;
	char* name;
} glXAttrib;

#define VAL_AND_NAME(x)  { x, #x }

static const glXAttrib tabRequestedAttribsPair[] =
{
	VAL_AND_NAME(GLX_USE_GL),
	VAL_AND_NAME(GLX_BUFFER_SIZE),
	VAL_AND_NAME(GLX_LEVEL),
	VAL_AND_NAME(GLX_RGBA),
	VAL_AND_NAME(GLX_DOUBLEBUFFER),
	VAL_AND_NAME(GLX_STEREO),
	VAL_AND_NAME(GLX_AUX_BUFFERS),
	VAL_AND_NAME(GLX_RED_SIZE),
	VAL_AND_NAME(GLX_GREEN_SIZE),
	VAL_AND_NAME(GLX_BLUE_SIZE),
	VAL_AND_NAME(GLX_ALPHA_SIZE),
	VAL_AND_NAME(GLX_DEPTH_SIZE),
	VAL_AND_NAME(GLX_STENCIL_SIZE),
	VAL_AND_NAME(GLX_ACCUM_RED_SIZE),
	VAL_AND_NAME(GLX_ACCUM_GREEN_SIZE),
	VAL_AND_NAME(GLX_ACCUM_BLUE_SIZE),
	VAL_AND_NAME(GLX_ACCUM_ALPHA_SIZE),
	VAL_AND_NAME(GLX_CONFIG_CAVEAT),
	VAL_AND_NAME(GLX_X_VISUAL_TYPE),
	VAL_AND_NAME(GLX_TRANSPARENT_TYPE),
	VAL_AND_NAME(GLX_TRANSPARENT_INDEX_VALUE),
	VAL_AND_NAME(GLX_TRANSPARENT_RED_VALUE),
	VAL_AND_NAME(GLX_TRANSPARENT_GREEN_VALUE),
	VAL_AND_NAME(GLX_TRANSPARENT_BLUE_VALUE),
	VAL_AND_NAME(GLX_TRANSPARENT_ALPHA_VALUE), 
	VAL_AND_NAME(GLX_SLOW_CONFIG),
	VAL_AND_NAME(GLX_TRUE_COLOR),
	VAL_AND_NAME(GLX_DIRECT_COLOR),
	VAL_AND_NAME(GLX_PSEUDO_COLOR),
	VAL_AND_NAME(GLX_STATIC_COLOR),
	VAL_AND_NAME(GLX_GRAY_SCALE),
	VAL_AND_NAME(GLX_STATIC_GRAY),
	VAL_AND_NAME(GLX_TRANSPARENT_RGB),
	VAL_AND_NAME(GLX_TRANSPARENT_INDEX),
	VAL_AND_NAME(GLX_VISUAL_ID),
	VAL_AND_NAME(GLX_DRAWABLE_TYPE),
	VAL_AND_NAME(GLX_RENDER_TYPE),
	VAL_AND_NAME(GLX_X_RENDERABLE),
	VAL_AND_NAME(GLX_FBCONFIG_ID),
	VAL_AND_NAME(GLX_RGBA_TYPE),
	VAL_AND_NAME(GLX_COLOR_INDEX_TYPE),
	VAL_AND_NAME(GLX_MAX_PBUFFER_WIDTH),
	VAL_AND_NAME(GLX_MAX_PBUFFER_HEIGHT),
	VAL_AND_NAME(GLX_MAX_PBUFFER_PIXELS),
	VAL_AND_NAME(GLX_PRESERVED_CONTENTS),
	VAL_AND_NAME(GLX_FLOAT_COMPONENTS_NV),
	VAL_AND_NAME(GLX_SAMPLE_BUFFERS),
	VAL_AND_NAME(GLX_SAMPLES)
};
#define N_REQUESTED_ATTRIBS (sizeof(tabRequestedAttribsPair)/sizeof(tabRequestedAttribsPair[0]))

static int* getTabRequestedAttribsInt()
{
	static int tabRequestedAttribsInt[N_REQUESTED_ATTRIBS] = { 0 };
	if (tabRequestedAttribsInt[0] == 0)
	{
		int i;
		for(i=0;i<N_REQUESTED_ATTRIBS;i++)
			tabRequestedAttribsInt[i] = tabRequestedAttribsPair[i].val;
	}
	return tabRequestedAttribsInt;
}

#define N_MAX_ATTRIBS N_REQUESTED_ATTRIBS+10
typedef struct
{
	int                  visualid;
	_glXGetConfigAttribs attribs[N_MAX_ATTRIBS];
	int                  nbAttribs;
} _glXConfigs;
#define N_MAX_CONFIGS 80
static _glXConfigs configs[N_MAX_CONFIGS];
static int nbConfigs = 0;

typedef struct
{
	GLXFBConfig config;
	_glXGetConfigAttribs attribs[N_MAX_ATTRIBS];
	int nbAttribs;
} _glXFBConfig;

static _glXFBConfig fbconfigs[N_MAX_CONFIGS];
static int nbFBConfigs = 0;

#define __CLIENT_WINDOW__

#ifdef __CLIENT_WINDOW__
/* Window Image�� �̿���  Guest Window Draw */
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>

static void glReadPixels_no_lock  ( GLint x, GLint y,
		GLsizei width, GLsizei height,
		GLenum format, GLenum type,
		GLvoid *pixels );

#define MAX_IMAGES 100

typedef struct {
	Window win;
	GC win_gc;
	int width;
	int height	;
	unsigned int bytes_per_pixel;
	unsigned int bytes_per_line;
	char *rData;

	char *imageData;
	XImage *xImage;
	XShmSegmentInfo *shminfo;
} WindowImage;

WindowImage wImage[MAX_IMAGES];

static Bool _create_image(Display *dpy, Window win, WindowImage *image)
{
	XWindowAttributes XWinAttribs;
	XShmSegmentInfo *shmseginfo;

	XGetWindowAttributes( dpy, win, &XWinAttribs );
	shmseginfo = (XShmSegmentInfo *)malloc(sizeof(XShmSegmentInfo));
	if(!shmseginfo)
	{
		return False;
	}
	image->xImage = XShmCreateImage(dpy, XWinAttribs.visual, XWinAttribs.depth, ZPixmap, NULL, shmseginfo, XWinAttribs.width, XWinAttribs.height);
	if( image->xImage == NULL ) 
	{
		free( shmseginfo );
		return False;
	}

	image->width = XWinAttribs.width;
	image->height = XWinAttribs.height;
	image->bytes_per_pixel = image->xImage->bits_per_pixel / 8;
	image->bytes_per_line = image->width * image->bytes_per_pixel;


	shmseginfo->shmid = shmget(IPC_PRIVATE, image->bytes_per_line * image->height, IPC_CREAT|0777);
	if(shmseginfo->shmid < 0 ) 
	{
		printf("shmget Err --------- \n");
		XDestroyImage(image->xImage);
		free( shmseginfo );
		return False;
	}

	shmseginfo->shmaddr = shmat(shmseginfo->shmid, NULL, 0);
	if(!shmseginfo->shmaddr)
	{
		printf("shmat Err --------- \n");
		XDestroyImage(image->xImage);
		shmctl(shmseginfo->shmid, IPC_RMID, NULL);
		free( shmseginfo );
		return False;

	}
	shmseginfo->readOnly = False;

	image->imageData = (char *) shmseginfo->shmaddr;
	image->rData = malloc(XWinAttribs.width * XWinAttribs.height * image->bytes_per_pixel);

	if(image->rData == NULL || !XShmAttach(dpy, shmseginfo))
	{
		XShmDetach(dpy, shmseginfo); 
		XDestroyImage(image->xImage);
		shmdt(shmseginfo->shmaddr);
		shmctl(shmseginfo->shmid, IPC_RMID, NULL); 
		free( shmseginfo );
		return False;
	}

	image->xImage->data = image->imageData;
	image->win = win;
	image->shminfo = shmseginfo;

	return True;
}

static void _destroy_image(Display *dpy, WindowImage *image)
{
	XShmDetach(dpy, image->shminfo); 
	XDestroyImage(image->xImage);
	image->imageData = NULL;
	image->win = 0;

	shmdt(image->shminfo->shmaddr);
	shmctl(image->shminfo->shmid, IPC_RMID, NULL); 
	free( image->shminfo );
	image->shminfo = NULL;

	free(image->rData);
	image->rData = NULL;
}

static void _draw_image(Display *dpy, Window win, WindowImage *image)
{
	int r, s;
	XWindowAttributes XWinAttribs;

	XGetWindowAttributes( dpy, win, &XWinAttribs );
	if( image->width != XWinAttribs.width || image->height != XWinAttribs.height )
	{
		_destroy_image( dpy, image);
		if( _create_image(dpy, win, image) == False )
		{
			XFreeGC(dpy, image->win_gc);
			image->win_gc = NULL;
			return;
		}
	}
	LOCK(glReadPixels_func);
	glReadPixels_no_lock(0, 0, image->width, image->height, GL_BGRA, GL_UNSIGNED_BYTE, image->rData);
	UNLOCK(glReadPixels_func);

	for(s = 0, r = image->height - 1; s < image->height; s++, r--)
	{ 
		memcpy(&image->imageData[image->bytes_per_line * s], &image->rData[image->bytes_per_line * r], image->bytes_per_line);
	}

	XShmPutImage(dpy, image->win, image->win_gc, image->xImage, 0, 0, 0, 0, image->width, image->height, False);
	XFlush(dpy);

	return;
}
#endif /*__CLIENT_WINDOW__*/
#endif


static int pagesize = 0;

static int debug_gl = 0;
static int debug_array_ptr = 0;
static int disable_optim = 0;
static int limit_fps = 0;

#ifndef WIN32
static struct sigaction old_action;

static void sigsegv_handler(int signum, siginfo_t* info, void* ptr)
{
	struct ucontext* context = (struct ucontext*)ptr;
#if defined(__i386__)
	unsigned char* eip_ptr = (unsigned char*)context->uc_mcontext.gregs[REG_EIP];
#elif defined(__x86_64__)
	unsigned char* eip_ptr = (unsigned char*)context->uc_mcontext.gregs[REG_RIP];
#else
#error "unsupported architecture"
#endif
	if (eip_ptr[0] == 0xCD && eip_ptr[1] == 0x99)
	{
#if defined(__i386__)
		context->uc_mcontext.gregs[REG_EIP] += 2;
#elif defined(__x86_64__)
		context->uc_mcontext.gregs[REG_RIP] += 2;
#else
#error "unsupported architecture"
#endif
	}
	else
	{
		log_gl("unhandled SIGSEGV\n");
		exit(-1); // FIXME
		if (old_action.sa_flags & SA_SIGINFO)
		{
			old_action.sa_sigaction(signum, info, ptr);
		}
		else
		{
			old_action.sa_handler(signum);
		}
	}
}
#endif

#ifdef TCP_COMMUNICATION_SUPPORT

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
static int sock;
#else
#include <windows.h>
#include <winbase.h>
#include <winuser.h>
#include <ws2tcpip.h>
SOCKET sock;
#endif


static void init_sockaddr (struct sockaddr_in *name,
		const char *hostname,
		uint16_t port)
{
	struct hostent *hostinfo;
	name->sin_family = AF_INET;
	name->sin_port = htons (port);
	name->sin_addr.s_addr  = inet_addr(hostname);
	if (name->sin_addr.s_addr != INADDR_NONE)
		return;
	hostinfo = gethostbyname (hostname);
	if (hostinfo == NULL)
	{
		log_gl("Unknown host %s.\n", hostname);
#ifdef WIN32
		char msg[512];
		sprintf(msg, "Unknown host %s", hostname);
		MessageBox(NULL, msg, "Unknown host", 0);
#endif
		exit (EXIT_FAILURE);
	}
	name->sin_addr = *(struct in_addr *) hostinfo->h_addr;
}

static int total_written = 0;
static void write_sock_data(void* data, int len)
{
	if (len && data)
	{
		int offset = 0;
		//if (debug_gl) log_gl("to write : %d\n", len);
		while(offset < len)
		{
#ifndef WIN32
			int nwritten = write(sock, data + offset, len - offset);
			if (nwritten == -1) 
#else
				int nwritten = send(sock, data + offset, len - offset, 0);
			if (nwritten == SOCKET_ERROR) 
#endif
			{
				if (errno == EINTR)
					continue;
				perror("write");
				assert(nwritten != -1);
			}
			offset += nwritten;
			total_written += nwritten;
			/*if (debug_gl)
			  log_gl("total written : %d\n", total_written);
			  if (debug_gl && offset < len) log_gl("remaining to write : %d\n", len - offset);*/
		}
	}
}

static void inline write_sock_int(int my_int)
{
	write_sock_data(&my_int, sizeof(int));
}

static void inline write_sock_short(short my_int)
{
	write_sock_data(&my_int, sizeof(short));
}

static void read_sock_data(void* data, int len)
{
	if (len && data)
	{
		int offset = 0;
		while(offset < len)
		{
#ifndef WIN32
			int nread = read(sock, data + offset, len - offset);
			if (nread == -1)
#else
				int nread = recv(sock, data + offset, len - offset, 0);
			if (nread == SOCKET_ERROR)
#endif
			{
				if (errno == EINTR)
					continue;
				perror("read");
				assert(nread != -1);
			}
			offset += nread;
		}
	}
}

static int inline read_sock_int()
{
	int ret;
	read_sock_data(&ret, sizeof(int));
	return ret;
}

static int call_opengl_tcp(int func_number, int pid, void* ret_string, void* _args, void* _args_size)
{
	int ret;
	Signature* signature = (Signature*)tab_opengl_calls[func_number];
	int nb_args = signature->nb_args;
	int i;
	long* args = (long*)_args;
	int* args_size = (int*)_args_size;
	int* args_type = signature->args_type;


	write_sock_short(func_number);
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
				{
					write_sock_int(args[i]);
					break;
				}

			case TYPE_ARRAY_DOUBLE:
			case TYPE_ARRAY_CHAR:
			case TYPE_ARRAY_INT:
			case TYPE_ARRAY_FLOAT:
			case TYPE_NULL_TERMINATED_STRING:
				{
					write_sock_int(args_size[i]);
					write_sock_data((void*)args[i], args_size[i]);
					break;
				}

CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
				{
					int size = compute_arg_length(get_err_file(), func_number, i, args);
					if (size < 0)
						log_gl("TYPE_ARRAY_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS : length = %d\n", size);
					else
						write_sock_data((void*)args[i], size);
					break;
				}

			case TYPE_DOUBLE:
CASE_IN_KNOWN_SIZE_POINTERS:
				{
					write_sock_data((void*)args[i], args_size[i]);
					break;
				}

			case TYPE_IN_IGNORED_POINTER:
				{
					break;
				}

CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
CASE_OUT_KNOWN_SIZE_POINTERS:
				{
					break;
				}

CASE_OUT_UNKNOWN_SIZE_POINTERS:
				{
					write_sock_int((args[i]) ? args_size[i] : 0);
					break;
				}

			default:
				{
					log_gl("(0) unexpected arg type %d at i=%d\n", args_type[i], i);
					exit(-1);
					break;
				}
		}
	}

	if (signature->has_out_parameters)
	{
		for(i=0;i<nb_args;i++)
		{
			switch(args_type[i])
			{
CASE_OUT_POINTERS:
				{
					read_sock_data((void*)args[i], args_size[i]);
					break;
				}

				default:
				{
					break;
				}
			}
		}
	}

	if (signature->ret_type == TYPE_CONST_CHAR)
	{
		int len = read_sock_int();
		read_sock_data(ret_string, len);
		ret = 0;
	}
	else if (signature->ret_type != TYPE_NONE)
	{
		ret = read_sock_int();
	}
	else
	{
		ret = 0;
	}

	return ret;
}
#endif

#ifdef WIN32
static EXCEPTION_DISPOSITION win32_sigsegv_handler(struct _EXCEPTION_RECORD *exception_record,
		void * EstablisherFrame,
		struct _CONTEXT *ContextRecord,
		void * DispatcherContext)
{
	/* If the exception is an access violation */ 
	if (exception_record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
			exception_record->NumberParameters >= 2) 
	{ 
		//int accessMode = (int)exception_record->ExceptionInformation[0]; 
		//void* adr = (void *)exception_record->ExceptionInformation[1]; 

		unsigned char* eip_ptr = (unsigned char*)ContextRecord->Eip;
		if (eip_ptr[0] == 0xCD && eip_ptr[1] == 0x99)
		{
			ContextRecord->Eip += 2;
			return ExceptionContinueExecution; 
		}
		else
		{
			log_gl("not handled exception : %X\n", (int)exception_record->ExceptionCode);
			fflush(get_err_file());
			return ExceptionContinueSearch;
		}
	}
	else
	{
		log_gl("not handled exception : %X\n", (int)exception_record->ExceptionCode);
		fflush(get_err_file());
		return ExceptionContinueSearch;
	}
} 
#endif

static int call_opengl_chardev(int func_number, int pid, void* ret_string, void *args, void* args_size)
{
	if( ! (func_number >= 0 && func_number < GL_N_CALLS) )
	{
		log_gl("func_number >= 0 && func_number < GL_N_CALLS failed\n");
		return 0;
	}

	Signature* signature = (Signature*)tab_opengl_calls[func_number];
	int nb_args = signature->nb_args;

	int ret;
	struct accel {
		int func_number;
		int pid;
		void *ret_string;
		void *args;
		void *args_size;
	};
	struct accel accelMode;

	accelMode.func_number = func_number;
	accelMode.pid = pid;
	accelMode.ret_string = ret_string;
	accelMode.args = args;
	accelMode.args_size = args_size;

	write(fd, &accelMode, nb_args);

	ret = read(fd, NULL, 1);

	return ret;    
}

static int call_opengl_qemu(int func_number, int pid, void* ret_string, void* args, void* args_size)
{
#if defined(__i386__)
	int ret;
#ifdef WIN32
	__asm__ ("pushl %0;pushl %%fs:0;movl %%esp,%%fs:0;" : : "g" (win32_sigsegv_handler));
#endif
	__asm__ ("push %ebx");
	__asm__ ("push %ecx");
	__asm__ ("push %edx");
	__asm__ ("push %esi");
	__asm__ ("mov %0, %%eax"::"m"(func_number));
	__asm__ ("mov %0, %%ebx"::"m"(pid));
	__asm__ ("mov %0, %%ecx"::"m"(ret_string));
	__asm__ ("mov %0, %%edx"::"m"(args));
	__asm__ ("mov %0, %%esi"::"m"(args_size));
	__asm__ ("int $0x99");
	__asm__ ("pop %esi");
	__asm__ ("pop %edx");
	__asm__ ("pop %ecx");
	__asm__ ("pop %ebx");
	__asm__ ("mov %%eax, %0"::"m"(ret));
#ifdef WIN32
	__asm__ ("movl (%%esp),%%ecx;movl %%ecx,%%fs:0;addl $8,%%esp;" : : : "%ecx");
#endif
	return ret;
#elif defined(__x86_64__)
	int ret;
	__asm__ ("push %rbx");
	__asm__ ("push %rcx");
	__asm__ ("push %rdx");
	__asm__ ("push %rsi");
	__asm__ ("mov %0, %%eax"::"m"(func_number));
	__asm__ ("mov %0, %%ebx"::"m"(pid));
	__asm__ ("mov %0, %%rcx"::"m"(ret_string));
	__asm__ ("mov %0, %%rdx"::"m"(args));
	__asm__ ("mov %0, %%rsi"::"m"(args_size));
	__asm__ ("int $0x99");
	__asm__ ("pop %rsi");
	__asm__ ("pop %rdx");
	__asm__ ("pop %rcx");
	__asm__ ("pop %rbx");
	__asm__ ("mov %%eax, %0"::"m"(ret));
	return ret;
#else
	fprintf(stderr, "unsupported architecture!\n");
	return 0;
#endif
}


#ifdef TCP_COMMUNICATION_SUPPORT
static int use_tcp_communication = 0;
static int call_opengl(int func_number, int pid, void* ret_string, void* args, void* args_size)
{
	if (!use_tcp_communication)
		return call_opengl_chardev(func_number, pid, ret_string, args, args_size);
	else
		return call_opengl_tcp(func_number, pid, ret_string, args, args_size);
}
#else
static int call_opengl(int func_number, int pid, void* ret_string, void* args, void* args_size)
{
	return call_opengl_qemu(func_number, pid, ret_string, args, args_size);
}
#endif


static void do_init()
{
#ifdef TCP_COMMUNICATION_SUPPORT
	if (use_tcp_communication)
	{
		struct sockaddr_in servername;
#ifdef WIN32
		WORD wVersionRequested;
		WSADATA WSAData;		/* Structure WSADATA d�finie dans winsock.h */
		int err;

		wVersionRequested = MAKEWORD(2, 0);	/* 1.1 Version voulut de Winsock */
		err = WSAStartup(wVersionRequested, &WSAData);	/* Appel de notre fonction */
		if (err != 0)
		{
			MessageBox(NULL, "WSAStartup failed", "WSAStartup failed", 0);
			exit(EXIT_FAILURE);
		}
#endif
		/* Create the socket. */
		sock = socket (AF_INET, SOCK_STREAM,
#ifndef WIN32
				IPPROTO_TCP
#else
				0
#endif
				);
		if (sock < 0)
		{
#ifdef WIN32
			MessageBox(NULL, "socket (client)", "socket (client)", 0);
#endif
			perror ("socket (client)");
			exit (EXIT_FAILURE);
		}

		/* Connect to the server. */
		int port;
		if (getenv("GL_SERVER_PORT"))
			port = atoi(getenv("GL_SERVER_PORT"));

		/* Get fixed host IP address and connect to server */
#ifdef USE_TCP_METHOD
		//char fixed_host_ip[] = "10.0.2.2";  /* HOST_QEMU_ADDRESS */
		//init_sockaddr (&servername, getenv("GL_SERVER") ? getenv("GL_SERVER") : &fixed_host_ip[0], port);

		FILE *fp;
		char port_buffer[32] = {0};
		char ip_buffer[32] = "10.0.2.2";

		/*
		   fp = fopen("/opt/home/opengl_ip.txt", "rt");
		   if( fp== NULL ) {
		   fprintf(stderr, "/opt/home/opengl_ip.txt file open error\n");
		   exit(EXIT_FAILURE);
		   }
		   fgets(ip_buffer, 32, fp);
		   fclose(fp);
		 */

		fp = fopen("/opt/home/sdb_port.txt", "rt");
		if( fp== NULL ) {
			fprintf(stderr, "/opt/home/sdb_port.txt file open error\n");
			exit(EXIT_FAILURE);
		}
		fgets(port_buffer, 32, fp);
		fclose(fp);

		port = atoi(port_buffer) + 4;
		init_sockaddr(&servername, getenv("GL_SERVER") ? getenv("GL_SERVER") : ip_buffer, port);;

#else
		init_sockaddr (&servername, getenv("GL_SERVER") ? getenv("GL_SERVER") : "localhost", port);
#endif

		int flag = 1;
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,(char *)&flag, sizeof(int)) != 0)
		{
#ifdef WIN32
			MessageBox(NULL, "setsockopt TCP_NODELAY", "setsockopt TCP_NODELAY", 0);
#endif
			perror("setsockopt TCP_NODELAY");
		}
		if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,(char *)&flag, sizeof(int)) != 0)
		{
#ifdef WIN32
			MessageBox(NULL, "setsockopt SO_KEEPALIVE", "setsockopt SO_KEEPALIVE", 0);
#endif
			perror("setsockopt SO_KEEPALIVE");
		}

		if (0 > connect (sock,
					(struct sockaddr *) &servername,
					sizeof (servername)))
		{
#ifdef WIN32
			MessageBox(NULL, "impossible to connect to server", "impossible to connect to server", 0);
#endif
			perror ("impossible to connect to server");
			exit (EXIT_FAILURE);
		}

		setenv("ENABLE_GL_BUFFERING", "1", 1);
		setenv("NO_MOVE", "1", 1);
	}
	else
#endif
	{
#ifndef WIN32
		struct sigaction action;
		action.sa_sigaction = sigsegv_handler;
		sigemptyset(&(action.sa_mask));
		action.sa_flags = SA_SIGINFO;
		sigaction(SIGSEGV, &action, &old_action);

		int parent_pid = getpid();
		int pid_fork = fork();
		if (pid_fork == 0)
		{
			/* Sorry. This is really ugly, not portable, etc...
			   This is the quickest way I've found
			   to make sure that someone notices that the main program has stopped
			   running and can warn the server */
			if (fork() != 0) exit(0);
			setsid();
			if (fork() != 0) exit(0);
			fcloseall();
			chdir("/");

			char processname[512];
			sprintf(processname, "/proc/%d", parent_pid);
			while(1)
			{
				struct stat buf;
				if (lstat(processname, &buf) < 0)
				{
					call_opengl(_exit_process_func, parent_pid, NULL, NULL, NULL);
					close(fd);
					exit(0);
				}
				sleep(1);
			}
		}
		else if (pid_fork > 0)
		{
			log_gl("go on...\n");
		}
		else
		{
			log_gl("fork failed\n");
			exit(-1);
		}
#endif
	}
	/*
#ifndef _WIN32
	if((fd = open(DEV_NAME, O_RDWR|O_NDELAY)) < 0)
#else
	if((fd = open(DEV_NAME, O_RDWR)) < 0 )
#endif
	{
		fprintf(stderr, "Device Open Error\n");
		return -1;
	}
	*/
}

static int try_to_put_into_phys_memory(void* addr, int len)
{
	if (addr == NULL || len == 0) return 0;
	int i;
	void* aligned_addr = (void*)((long)addr & (~(pagesize - 1)));
	int to_end_page = (long)aligned_addr + pagesize - (long)addr;
	char c = 0;
	if (aligned_addr != addr)
	{
		c += ((char*)addr)[0];
		if (len <= to_end_page)
		{
			return c;
		}
		len -= to_end_page;
		addr = aligned_addr + pagesize;
	}
	for(i=0;i<len;i+=pagesize)
	{
		c += ((char*)addr)[0];
		addr += pagesize;
	}
	return c;
}

#define RET_STRING_SIZE 32768
#define SIZE_BUFFER_COMMAND 65536*16

#define POINTER_TO_ARG(x)            (long)(void*)(x)
#define CHAR_TO_ARG(x)               (long)(x)
#define SHORT_TO_ARG(x)              (long)(x)
#define INT_TO_ARG(x)                (long)(x)
#define UNSIGNED_CHAR_TO_ARG(x)      (long)(x)
#define UNSIGNED_SHORT_TO_ARG(x)     (long)(x)
#define UNSIGNED_INT_TO_ARG(x)       (long)(x)
#define FLOAT_TO_ARG(x)              (long)*((int*)(void*)(&x))
#define DOUBLE_TO_ARG(x)             (long)(&x)

#ifdef WIN32
static int getpagesize()
{
	return 4096;
}

static void mlock(void* ptr, int size)
{
}

static void posix_memalign(void** pptr, int alignment, int size)
{
	*pptr = malloc(size);
}

#endif

static int disable_warning_for_gl_read_pixels = 0;

#ifndef WIN32
static Bool glXMakeCurrent_no_lock( Display *dpy, GLXDrawable drawable, GLXContext ctx);
static void glXSwapBuffers_no_lock( Display *dpy, GLXDrawable drawable );
#endif

static void glGetIntegerv_no_lock( GLenum pname, GLint *params );
static void glPixelStorei_no_lock( GLenum pname, GLint param );
static void glBindBufferARB_no_lock (GLenum target, GLuint buffer);
#ifndef __CLIENT_WINDOW__
static void glReadPixels_no_lock  ( GLint x, GLint y,
		GLsizei width, GLsizei height,
		GLenum format, GLenum type,
		GLvoid *pixels );
#endif                                    

#ifdef WIN32
typedef void* __GLXextFuncPtr;
#endif

static __GLXextFuncPtr glXGetProcAddress_no_lock(const GLubyte * name);

#define CHECK_PROC(x)
#define CHECK_PROC_WITH_RET(x)

/* Must only be called if the global lock has already been taken ! */
static void do_opengl_call_no_lock(int func_number, void* ret_ptr, long* args, int* args_size_opt)
{
	if( ! (func_number >= 0 && func_number < GL_N_CALLS) )
	{
		log_gl("func_number >= 0 && func_number < GL_N_CALLS failed\n");
		goto end_do_opengl_call;
	}

	Signature* signature = (Signature*)tab_opengl_calls[func_number];
	int nb_args = signature->nb_args;
	int* args_type = signature->args_type;
	int args_size[100] = {0,};
	int ret_int = 0;
	int i;

	static char* command_buffer = NULL;
	static int command_buffer_size = 0;
	static int last_command_buffer_size = -1;
	static char* ret_string = NULL;
	static int enable_gl_buffering = 0;
	static int last_current_thread = -1;
	static int exists_on_server_side[GL_N_CALLS];

	int current_thread = GET_CURRENT_THREAD();

	if (ret_string == NULL)
	{
		/* Sanity checks */
		assert(tab_args_type_length[TYPE_OUT_128UCHAR] == 128 * sizeof(char));
		assert(tab_args_type_length[TYPE_OUT_ARRAY_DOUBLE_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS] == sizeof(double));

		assert(sizeof(tab_args_type_length)/sizeof(tab_args_type_length[0]) == TYPE_LAST);

		memset(exists_on_server_side, 255, sizeof(exists_on_server_side));
		exists_on_server_side[glXGetProcAddress_fake_func] = 1;
		exists_on_server_side[glXGetProcAddress_global_fake_func] = 1;

		FILE* f = fopen("/tmp/.opengl_client", "rb");
		if (!f)
			f = fopen("opengl_client.txt", "rb");
		if (f)
		{
			char buffer[80];
			while (fgets(buffer, 80, f))
			{
				for(i=0;i<sizeof(interestingEnvVars)/sizeof(interestingEnvVars[0]);i++)
				{
					char tmp[256];
					strcpy(tmp, interestingEnvVars[i]);
					strcat(tmp, "=");
					if (strncmp(buffer, tmp, strlen(tmp)) == 0)
					{
						char* c= strdup(buffer+strlen(tmp));
						char* c2 = strchr(c, '\n');
						if (c2) *c2 = 0;
						setenv(interestingEnvVars[i], c,1);
						break;
					}
				}
			}
			fclose(f);
		}

		last_current_thread = current_thread;

#ifdef TCP_COMMUNICATION_SUPPORT
#ifdef USE_TCP_METHOD
		use_tcp_communication = getenv("USE_TCP_COMMUNICATION") ? getenv("USE_TCP_COMMUNICATION") : 1;
#else
		use_tcp_communication = getenv("USE_TCP_COMMUNICATION") != NULL;
#endif
#endif
		do_init();

		pagesize = getpagesize();
		debug_gl = getenv("DEBUG_GL") != NULL;
		debug_array_ptr = getenv("DEBUG_ARRAY_PTR") != NULL;
		disable_optim = getenv("DISABLE_OPTIM") != NULL;
		limit_fps = getenv("LIMIT_FPS") ? atoi(getenv("LIMIT_FPS")) : 0;

		posix_memalign((void**)(void*)&ret_string, pagesize, RET_STRING_SIZE);
		memset(ret_string, 0, RET_STRING_SIZE);
		mlock(ret_string, RET_STRING_SIZE);

		posix_memalign((void**)(void*)&command_buffer, pagesize, SIZE_BUFFER_COMMAND);
		mlock(command_buffer, SIZE_BUFFER_COMMAND);

		int init_ret = 0;
		long temp_args[] = { getenv("WRITE_GL") != NULL, POINTER_TO_ARG(&init_ret) };
		int temps_args_size[] = { 0, sizeof(int) };
		call_opengl(_init_func, getpid(), NULL, temp_args, temps_args_size);
		if (init_ret == 0)
		{
			log_gl("You maybe forgot to launch QEMU with -enable-gl argument.\n");
#ifdef TCP_COMMUNICATION_SUPPORT
			if (getenv("USE_TCP_COMMUNICATION") != NULL)
			{
				log_gl("Trying TCP/IP communication instead\n");
				use_tcp_communication = 1;
				do_init();
				call_opengl(_init_func, getpid(), NULL, temp_args, temps_args_size);
				if (init_ret == 0)
				{
					log_gl("exiting !\n");
					exit(-1);
				}
			}
			else
#endif
			{
				log_gl("exiting !\n");
				exit(-1);
			}
		}
		enable_gl_buffering = (init_ret == 2) || (getenv("ENABLE_GL_BUFFERING") != NULL);
	}
	if (exists_on_server_side[func_number] == -1)
	{
		if (strchr(tab_opengl_calls_name[func_number], '_'))
		{
			exists_on_server_side[func_number] = 1;
		}
		else
		{
			exists_on_server_side[func_number] = glXGetProcAddress_no_lock(tab_opengl_calls_name[func_number]) != NULL;
		}
		if (exists_on_server_side[func_number] == 0)
		{
			log_gl("Symbol %s not available in server libGL. Shouldn't have reach that point...\n",
					tab_opengl_calls_name[func_number]);
			goto end_do_opengl_call;
		}
	}
	else if (exists_on_server_side[func_number] == 0)
	{
		goto end_do_opengl_call;
	}

	GET_CURRENT_STATE();

#ifdef ENABLE_THREAD_SAFETY
	if (last_current_thread != current_thread)
	{
		last_current_thread = current_thread;
		if (debug_gl) log_gl("gl thread switch\n");
#ifndef WIN32
		glXMakeCurrent_no_lock(state->display, state->current_drawable, state->context);
#else
		// FIXME
#endif
	}
#endif

#ifndef WIN32
	if (func_number == glFlush_func && getenv("HACK_XGL"))
	{
		glXSwapBuffers_no_lock(state->display, state->current_drawable);
		goto end_do_opengl_call;
	}
	else if ((func_number == glDrawBuffer_func || func_number == glReadBuffer_func) && getenv("HACK_XGL"))
	{
		goto end_do_opengl_call;
	}
#endif

	if ((func_number >= glRasterPos2d_func && func_number <= glRasterPos4sv_func) ||
			(func_number >= glWindowPos2d_func && func_number <= glWindowPos3sv_func) ||
			(func_number >= glWindowPos2dARB_func && func_number <= glWindowPos3svARB_func) ||
			(func_number >= glWindowPos2dMESA_func && func_number <= glWindowPos4svMESA_func))

	{
		state->currentRasterPosKnown = 0;
	}

	int this_func_parameter_size = sizeof(short); /* func_number */

	/* Compress consecutive glEnableClientState calls */
	if (!disable_optim &&
			last_command_buffer_size != -1 &&
			func_number == glClientActiveTexture_func &&
			*(short*)(command_buffer + last_command_buffer_size) == glClientActiveTexture_func)
	{
		*(int*)(command_buffer + last_command_buffer_size + sizeof(short)) = args[0];
		goto end_do_opengl_call;
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
				{
					/*
					   args_size[i] = args_size_opt[i];
					   if(args_size[i] < 0)
					   {
					   log_gl("size < 0 : func=%s, param=%d\n", tab_opengl_calls_name[func_number], i);
					   goto end_do_opengl_call;
					   }	
					 */
					this_func_parameter_size += sizeof(int);
					break;
				}

CASE_IN_UNKNOWN_SIZE_POINTERS:
				{
					args_size[i] = args_size_opt[i];
					if (args_size[i] < 0)
					{
						log_gl("size < 0 : func=%s, param=%d\n", tab_opengl_calls_name[func_number], i);
						goto end_do_opengl_call;
					}
					try_to_put_into_phys_memory((void*)args[i], args_size[i]);
					this_func_parameter_size += sizeof(int) + args_size[i];
					break;
				}

			case TYPE_NULL_TERMINATED_STRING:
				{
					args_size[i] = strlen((const char*)args[i]) + 1;
					this_func_parameter_size += sizeof(int) + args_size[i];
					break;
				}

CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
				{
					args_size[i] = compute_arg_length(get_err_file(), func_number, i, args);
					this_func_parameter_size += args_size[i];
					break;
				}

			case TYPE_IN_IGNORED_POINTER:
				{
					args_size[i] = 0;
					break;
				}

CASE_OUT_UNKNOWN_SIZE_POINTERS:
				{
					args_size[i] = args_size_opt[i];
					if (args_size[i] < 0)
					{
						log_gl("size < 0 : func=%s, param=%d\n", tab_opengl_calls_name[func_number], i);
						goto end_do_opengl_call;
					}
					try_to_put_into_phys_memory((void*)args[i], args_size[i]);
					break;
				}

			case TYPE_DOUBLE:
CASE_KNOWN_SIZE_POINTERS:
				{
					args_size[i] = tab_args_type_length[args_type[i]];
					try_to_put_into_phys_memory((void*)args[i], args_size[i]);
					this_func_parameter_size += args_size[i];
					break;
				}

			default:
				{
					log_gl("(1) unexpected arg type %d at i=%d\n", args_type[i], i);
					exit(-1);
					break;
				}
		}
	}

	if (debug_gl) display_gl_call(get_err_file(), func_number, args, args_size);

	if (enable_gl_buffering && signature->ret_type == TYPE_NONE && signature->has_out_parameters == 0 &&
			func_number != _exit_process_func && !(func_number == glXSwapBuffers_func || func_number == glXDestroyWindow_func || func_number == glFlush_func || func_number == glFinish_func))
	{
		assert(ret_ptr == NULL);
		if (this_func_parameter_size + command_buffer_size >= SIZE_BUFFER_COMMAND)
		{
			if (debug_gl)
				log_gl("flush pending opengl calls...\n");
			try_to_put_into_phys_memory(command_buffer, command_buffer_size);
			call_opengl(_serialized_calls_func, getpid(), NULL, &command_buffer, &command_buffer_size);
			command_buffer_size = 0;
			last_command_buffer_size = -1;
		}

		if (this_func_parameter_size + command_buffer_size < SIZE_BUFFER_COMMAND)
		{
			/*if (debug_gl)
			  log_gl("serializable opengl call.\n");*/
			last_command_buffer_size = command_buffer_size;
			*(short*)(command_buffer + command_buffer_size) = func_number;
			command_buffer_size += sizeof(short);

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
						{
							*(int*)(command_buffer + command_buffer_size) = args[i];
							command_buffer_size += sizeof(int);
							break;
						}

CASE_IN_UNKNOWN_SIZE_POINTERS:
					case TYPE_NULL_TERMINATED_STRING:
						{
							*(int*)(command_buffer + command_buffer_size) = args_size[i];
							command_buffer_size += sizeof(int);

							if (args_size[i])
							{
								memcpy(command_buffer + command_buffer_size, (void*)args[i], args_size[i]);
								command_buffer_size += args_size[i];
							}
							break;
						}

CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
						{
							memcpy(command_buffer + command_buffer_size, (void*)args[i], args_size[i]);
							command_buffer_size += args_size[i];
							break;
						}

					case TYPE_IN_IGNORED_POINTER:
						{
							break;
						}

					case TYPE_DOUBLE:
CASE_IN_KNOWN_SIZE_POINTERS:
						{
							memcpy(command_buffer + command_buffer_size, (void*)args[i], args_size[i]);
							command_buffer_size += args_size[i];
							break;
						}

					default:
						{
							log_gl("(2) unexpected arg type %d at i=%d\n", args_type[i], i);
							exit(-1);
							break;
						}
				}
			}

			try_to_put_into_phys_memory(command_buffer, command_buffer_size);
			call_opengl(_serialized_calls_func, getpid(), NULL, &command_buffer, &command_buffer_size);
			command_buffer_size = 0;
			last_command_buffer_size = -1;
		}
		else
		{
			if (debug_gl)
				log_gl("too large opengl call.\n");
			call_opengl(func_number, getpid(), NULL, args, args_size);
		}
	}
	else
	{
		if (command_buffer_size)
		{
			if (debug_gl)
				log_gl("flush pending opengl calls...\n");
			try_to_put_into_phys_memory(command_buffer, command_buffer_size);
			call_opengl(_serialized_calls_func, getpid(), NULL, &command_buffer, &command_buffer_size);
			command_buffer_size = 0;
			last_command_buffer_size = -1;
		}

		//if (!(func_number == glXSwapBuffers_func || func_number == glFlush_func  || func_number == glFinish_func || (func_number == glReadPixels_func && disable_warning_for_gl_read_pixels)) && enable_gl_buffering)
		//  log_gl("synchronous opengl call : %s.\n", tab_opengl_calls_name[func_number]);

		if (signature->ret_type == TYPE_CONST_CHAR)
		{
			try_to_put_into_phys_memory(ret_string, RET_STRING_SIZE);
		}

#ifndef WIN32
		if (getenv("GET_IMG_FROM_SERVER") != NULL && func_number == glXSwapBuffers_func)
		{
		}
		else
#endif
			ret_int = call_opengl(func_number, getpid(), (signature->ret_type == TYPE_CONST_CHAR) ? ret_string : NULL, args, args_size);
		if (func_number == glXCreateContext_func)
		{
			if (debug_gl)
				log_gl("ret_int = %d\n", ret_int);
		}
		else if (func_number == glXSwapBuffers_func || func_number == glXDestroyWindow_func || func_number == glFinish_func || func_number == glFlush_func)
		{
#ifndef WIN32
			if (getenv("GET_IMG_FROM_SERVER"))
			{
				XWindowAttributes window_attributes_return;
				XGetWindowAttributes(state->display, state->current_drawable, &window_attributes_return);
				state->drawable_width = window_attributes_return.width;
				state->drawable_height = window_attributes_return.height;

				char* buf = malloc(4 * state->drawable_width * state->drawable_height);
				char* bufGL = malloc(4 * state->drawable_width * state->drawable_height);
				XImage* img = XCreateImage(state->display, 0, 24, ZPixmap, 0, buf, state->drawable_width, state->drawable_height, 8, 0);
				disable_warning_for_gl_read_pixels = 1;

				int pack_row_length, pack_alignment, pack_skip_rows, pack_skip_pixels;
				int pack_pbo = state->pixelPackBuffer;
				glGetIntegerv_no_lock(GL_PACK_ROW_LENGTH, &pack_row_length);
				glGetIntegerv_no_lock(GL_PACK_ALIGNMENT, &pack_alignment);
				glGetIntegerv_no_lock(GL_PACK_SKIP_ROWS, &pack_skip_rows);
				glGetIntegerv_no_lock(GL_PACK_SKIP_PIXELS, &pack_skip_pixels);
				glPixelStorei_no_lock(GL_PACK_ROW_LENGTH, 0);
				glPixelStorei_no_lock(GL_PACK_ALIGNMENT, 4);
				glPixelStorei_no_lock(GL_PACK_SKIP_ROWS, 0);
				glPixelStorei_no_lock(GL_PACK_SKIP_PIXELS, 0);
				if (pack_pbo)
					glBindBufferARB_no_lock(GL_PIXEL_PACK_BUFFER_EXT, 0);
				glReadPixels_no_lock(0, 0, state->drawable_width, state->drawable_height, GL_BGRA, GL_UNSIGNED_BYTE, bufGL);
				glPixelStorei_no_lock(GL_PACK_ROW_LENGTH, pack_row_length);
				glPixelStorei_no_lock(GL_PACK_ALIGNMENT, pack_alignment);
				glPixelStorei_no_lock(GL_PACK_SKIP_ROWS, pack_skip_rows);
				glPixelStorei_no_lock(GL_PACK_SKIP_PIXELS, pack_skip_pixels);
				if (pack_pbo)
					glBindBufferARB_no_lock(GL_PIXEL_PACK_BUFFER_EXT, pack_pbo);
				disable_warning_for_gl_read_pixels = 0;
				for(i=0;i<state->drawable_height;i++)
				{
					memcpy(buf + i * 4 * state->drawable_width,
							bufGL + (state->drawable_height-1-i) * 4 * state->drawable_width,
							4 * state->drawable_width);
				}
				free(bufGL);
				GC gc = DefaultGC(state->display, DefaultScreen(state->display));
				XPutImage(state->display, state->current_drawable, gc, img, 0, 0, 0, 0, state->drawable_width, state->drawable_height);
				XDestroyImage(img);
			}
			else
#endif
				call_opengl(_synchronize_func, getpid(), NULL, NULL, NULL);
		}
		if (signature->ret_type == TYPE_UNSIGNED_INT ||
				signature->ret_type == TYPE_INT)
		{
			*(int*)ret_ptr = ret_int;
		}
		else if (signature->ret_type == TYPE_UNSIGNED_CHAR ||
				signature->ret_type == TYPE_CHAR)
		{
			*(char*)ret_ptr = ret_int;
		}
		else if (signature->ret_type == TYPE_CONST_CHAR)
		{
			*(char**)(ret_ptr) = ret_string;
		}
	}

end_do_opengl_call:
	(void)0;
}

static inline void do_opengl_call(int func_number, void* ret_ptr, long* args, int* args_size_opt)
{
	LOCK(func_number);
	do_opengl_call_no_lock(func_number, ret_ptr, args, args_size_opt);
	UNLOCK(func_number);
}

#include "client_stub.c"

GLAPI void APIENTRY glPushAttrib(GLbitfield mask)
{
	GET_CURRENT_STATE();
	long args[] = { UNSIGNED_INT_TO_ARG(mask)};
	if (state->server_sp < MAX_SERVER_STATE_STACK_SIZE)
	{
		state->server_stack[state->server_sp].mask = mask;
		if (mask & GL_ENABLE_BIT)
		{
			state->server_stack[state->server_sp].linesmoothEnabled = state->current_server_state.linesmoothEnabled;
			state->server_stack[state->server_sp].lightingEnabled = state->current_server_state.lightingEnabled;
			state->server_stack[state->server_sp].texture2DEnabled = state->current_server_state.texture2DEnabled;
			state->server_stack[state->server_sp].blendEnabled = state->current_server_state.blendEnabled;
		}
		if (mask & GL_TRANSFORM_BIT)
		{
			state->server_stack[state->server_sp].matrixMode = state->current_server_state.matrixMode;
		}
		if (mask & GL_DEPTH_BUFFER_BIT)
		{
			state->server_stack[state->server_sp].depthFunc = state->current_server_state.depthFunc;
		}
		if (mask & GL_TEXTURE_BIT)
		{
			state->server_stack[state->server_sp].bindTexture2D = state->current_server_state.bindTexture2D;
			state->server_stack[state->server_sp].bindTextureRectangle = state->current_server_state.bindTextureRectangle;
		}
		if (mask & GL_FOG_BIT)
		{
			state->server_stack[state->server_sp].fog = state->current_server_state.fog;
		}
		state->server_sp++;
	}
	else
	{
		log_gl("server_sp > MAX_SERVER_STATE_STACK_SIZE\n");
	}
	do_opengl_call(glPushAttrib_func, NULL, args, NULL);
}

GLAPI void APIENTRY glPopAttrib()
{
	GET_CURRENT_STATE();
	if (state->server_sp > 0)
	{
		state->server_sp--;
		if (state->server_stack[state->server_sp].mask & GL_ENABLE_BIT)
		{
			state->current_server_state.linesmoothEnabled = state->server_stack[state->server_sp].linesmoothEnabled;
			state->current_server_state.lightingEnabled = state->server_stack[state->server_sp].lightingEnabled;
			state->current_server_state.texture2DEnabled = state->server_stack[state->server_sp].texture2DEnabled;
			state->current_server_state.blendEnabled = state->server_stack[state->server_sp].blendEnabled;
		}
		if (state->server_stack[state->server_sp].mask & GL_TRANSFORM_BIT)
		{
			state->current_server_state.matrixMode = state->server_stack[state->server_sp].matrixMode;
		}
		if (state->server_stack[state->server_sp].mask & GL_DEPTH_BUFFER_BIT)
		{
			state->current_server_state.depthFunc = state->server_stack[state->server_sp].depthFunc;
		}
		if (state->server_stack[state->server_sp].mask & GL_TEXTURE_BIT)
		{
			state->current_server_state.bindTexture2D = state->server_stack[state->server_sp].bindTexture2D;
			state->current_server_state.bindTextureRectangle = state->server_stack[state->server_sp].bindTextureRectangle;
		}
		if (state->server_stack[state->server_sp].mask & GL_FOG_BIT)
		{
			state->current_server_state.fog = state->server_stack[state->server_sp].fog;
		}
	}
	else
	{
		log_gl("server_sp <= 0\n");
	}
	do_opengl_call(glPopAttrib_func, NULL, NULL, NULL);
}

/* 'glIsEnabled'�� ���� static function �� */
static GLboolean APIENTRY _glIsEnabled( GLenum cap )
{
	GLboolean ret = 0;
	GET_CURRENT_STATE();
	if (!disable_optim)
	{
		if (cap == GL_LINE_SMOOTH)
		{
			return state->current_server_state.linesmoothEnabled;
		}
		else if (cap == GL_LIGHTING)
		{
			return state->current_server_state.lightingEnabled;
		}
		else if (cap == GL_TEXTURE_2D)
		{
			return state->current_server_state.texture2DEnabled;
		}
		else if (cap == GL_BLEND)
		{
			return state->current_server_state.blendEnabled;
		}
		else if (cap == GL_SCISSOR_TEST)
		{
			return state->current_server_state.scissorTestEnabled;
		}
		else if (cap == GL_VERTEX_PROGRAM_NV)
		{
			return state->current_server_state.vertexProgramEnabled;
		}
		else if (cap == GL_FOG)
		{
			return state->current_server_state.fogEnabled;
		}
	}
	long args[] = { INT_TO_ARG(cap) };
	do_opengl_call(glIsEnabled_func, &ret, args, NULL);
	return ret;
}

GLAPI GLboolean APIENTRY glIsEnabled( GLenum cap )
{
	GLboolean ret = 0;

	if (debug_gl) log_gl("glIsEnabled(0x%X)\n", cap);

	ret = _glIsEnabled(cap);

	if (debug_gl) log_gl("glIsEnabled(0x%X) = %d\n", cap, ret);
	return ret;
}

static ClientArray* _getArray(GLenum cap, int verbose)
{
	GET_CURRENT_STATE();
	switch(cap)
	{
		case GL_VERTEX_ARRAY: return &state->client_state.arrays.vertexArray;
		case GL_COLOR_ARRAY: return &state->client_state.arrays.colorArray;
		case GL_SECONDARY_COLOR_ARRAY: return &state->client_state.arrays.secondaryColorArray;
		case GL_NORMAL_ARRAY: return &state->client_state.arrays.normalArray;
		case GL_INDEX_ARRAY: return &state->client_state.arrays.indexArray;
		case GL_TEXTURE_COORD_ARRAY:
							 return &state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture];
		case GL_EDGE_FLAG_ARRAY: return &state->client_state.arrays.edgeFlagArray;
		case GL_WEIGHT_ARRAY_ARB: return &state->client_state.arrays.weightArray;
		case GL_MATRIX_INDEX_ARRAY_POINTER_ARB: return &state->client_state.arrays.matrixIndexArray;
		case GL_FOG_COORD_ARRAY: return &state->client_state.arrays.fogCoordArray;
		case GL_ELEMENT_ARRAY_ATI: return &state->client_state.arrays.elementArrayATI;
		default:
								   if (cap >= GL_VERTEX_ATTRIB_ARRAY0_NV && cap < GL_VERTEX_ATTRIB_ARRAY0_NV + MY_GL_MAX_VERTEX_ATTRIBS_NV)
									   return &state->client_state.arrays.vertexAttribArrayNV[cap - GL_VERTEX_ATTRIB_ARRAY0_NV];
								   if (verbose) log_gl("unhandled cap : %d\n", cap); return NULL;
	}
}


GLAPI void APIENTRY glEnable(GLenum cap)
{
	GET_CURRENT_STATE();
	/* Strange but some programs use glEnable(GL_VERTEX_ARRAY)
	   instead of glEnableClientState(GL_VERTEX_ARRAY) ...
	   I've discovered that while trying to make the spheres demo of chromium running
	   and mesa confirms it too */
	if (_getArray(cap, 0))
	{
		glEnableClientState(cap);
		return;
	}

	if (cap == GL_LINE_SMOOTH)
	{
		state->current_server_state.linesmoothEnabled = 1;
	}
	else if (cap == GL_LIGHTING)
	{
		state->current_server_state.lightingEnabled = 1;
	}
	else if (cap == GL_TEXTURE_2D)
	{
		state->current_server_state.texture2DEnabled = 1;
	}
	else if (cap == GL_BLEND)
	{
		state->current_server_state.blendEnabled = 1;
	}
	else if (cap == GL_SCISSOR_TEST)
	{
		state->current_server_state.scissorTestEnabled = 1;
	}
	else if (cap == GL_VERTEX_PROGRAM_NV)
	{
		state->current_server_state.vertexProgramEnabled = 1;
	}
	else if (cap == GL_FOG)
	{
		state->current_server_state.fogEnabled = 1;
	}
	long args[] = { INT_TO_ARG(cap)};
	do_opengl_call(glEnable_func, NULL, args, NULL);
}

GLAPI void APIENTRY glDisable(GLenum cap)
{
	GET_CURRENT_STATE();
	if (_getArray(cap, 0))
	{
		glDisableClientState(cap);
		return;
	}

	if (cap == GL_LINE_SMOOTH)
	{
		state->current_server_state.linesmoothEnabled = 0;
	}
	else if (cap == GL_LIGHTING)
	{
		state->current_server_state.lightingEnabled = 0;
	}
	else if (cap == GL_TEXTURE_2D)
	{
		state->current_server_state.texture2DEnabled = 0;
	}
	else if (cap == GL_BLEND)
	{
		state->current_server_state.blendEnabled = 0;
	}
	else if (cap == GL_SCISSOR_TEST)
	{
		state->current_server_state.scissorTestEnabled = 0;
	}
	else if (cap == GL_VERTEX_PROGRAM_NV)
	{
		state->current_server_state.vertexProgramEnabled = 0;
	}
	else if (cap == GL_FOG)
	{
		state->current_server_state.fogEnabled = 0;
	}
	long args[] = { INT_TO_ARG(cap)};
	do_opengl_call(glDisable_func, NULL, args, NULL);
}

#define GET_CAP_NAME(x) case x: return #x;
static const char* _getArrayName(GLenum cap)
{
	switch (cap)
	{
		GET_CAP_NAME(GL_VERTEX_ARRAY);
		GET_CAP_NAME(GL_COLOR_ARRAY);
		GET_CAP_NAME(GL_SECONDARY_COLOR_ARRAY);
		GET_CAP_NAME(GL_NORMAL_ARRAY);
		GET_CAP_NAME(GL_INDEX_ARRAY);
		GET_CAP_NAME(GL_TEXTURE_COORD_ARRAY);
		GET_CAP_NAME(GL_EDGE_FLAG_ARRAY);
		GET_CAP_NAME(GL_WEIGHT_ARRAY_ARB);
		GET_CAP_NAME(GL_MATRIX_INDEX_ARRAY_ARB);
		GET_CAP_NAME(GL_FOG_COORD_ARRAY);
		GET_CAP_NAME(GL_ELEMENT_ARRAY_ATI);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY0_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY1_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY2_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY3_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY4_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY5_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY6_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY7_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY8_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY9_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY10_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY11_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY12_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY13_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY14_NV);
		GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY15_NV);
	}
	return "unknown";
}

GLAPI void APIENTRY EXT_FUNC(glEnableVertexAttribArrayARB)(GLuint index)
{
	CHECK_PROC(glEnableVertexAttribArrayARB);
	GET_CURRENT_STATE();
	long args[] = { index };
	do_opengl_call(glEnableVertexAttribArrayARB_func, NULL, args, NULL);

	if (index < MY_GL_MAX_VERTEX_ATTRIBS_ARB)
	{
		state->client_state.arrays.vertexAttribArray[index].enabled = 1;
	}
	else
	{
		log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n");
	}
}

GLAPI void APIENTRY EXT_FUNC(glEnableVertexAttribArray)(GLuint index)
{
	CHECK_PROC(glEnableVertexAttribArray);
	GET_CURRENT_STATE();
	long args[] = { index };
	do_opengl_call(glEnableVertexAttribArray_func, NULL, args, NULL);

	if (index < MY_GL_MAX_VERTEX_ATTRIBS_ARB)
	{
		state->client_state.arrays.vertexAttribArray[index].enabled = 1;
	}
	else
	{
		log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n");
	}
}


GLAPI void APIENTRY EXT_FUNC(glDisableVertexAttribArrayARB)(GLuint index)
{
	CHECK_PROC(glDisableVertexAttribArrayARB);
	GET_CURRENT_STATE();
	long args[] = { index };
	do_opengl_call(glDisableVertexAttribArrayARB_func, NULL, args, NULL);

	if (index < MY_GL_MAX_VERTEX_ATTRIBS_ARB)
	{
		state->client_state.arrays.vertexAttribArray[index].enabled = 0;
	}
	else
	{
		log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n");
	}
}

GLAPI void APIENTRY EXT_FUNC(glDisableVertexAttribArray)(GLuint index)
{
	CHECK_PROC(glDisableVertexAttribArray);
	GET_CURRENT_STATE();
	long args[] = { index };
	do_opengl_call(glDisableVertexAttribArray_func, NULL, args, NULL);

	if (index < MY_GL_MAX_VERTEX_ATTRIBS_ARB)
	{
		state->client_state.arrays.vertexAttribArray[index].enabled = 0;
	}
	else
	{
		log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n");
	}
}

GLAPI void APIENTRY glEnableClientState(GLenum cap)
{
	if (cap == GL_VERTEX_ARRAY_RANGE_NV ||
			cap == GL_VERTEX_ARRAY_RANGE_WITHOUT_FLUSH_NV) return; /* FIXME */
	GET_CURRENT_STATE();
	ClientArray* array = _getArray(cap, 1);
	if (array == NULL) return;
	if (debug_array_ptr)
	{
		if (cap == GL_TEXTURE_COORD_ARRAY)
			log_gl("enable texture %d\n", state->client_state.clientActiveTexture);
		else
			log_gl("enable feature %s\n", _getArrayName(cap));
	}
	if ((!disable_optim) && array->enabled)
	{
		//log_gl("discard useless command\n");
		return;
	}
	array->enabled = 1;
	/*if ((!disable_optim) && cap == GL_TEXTURE_COORD_ARRAY)
	  {
	  long args[] = { UNSIGNED_INT_TO_ARG(state->client_state.clientActiveTexture + GL_TEXTURE0_ARB)};
	  do_opengl_call(glClientActiveTexture_func, NULL, args, NULL);
	  }*/
	long args[] = { UNSIGNED_INT_TO_ARG(cap)};
	do_opengl_call(glEnableClientState_func, NULL, args, NULL);
}

GLAPI void APIENTRY glDisableClientState(GLenum cap)
{
	if (cap == GL_VERTEX_ARRAY_RANGE_NV ||
			cap == GL_VERTEX_ARRAY_RANGE_WITHOUT_FLUSH_NV) return; /* FIXME */
	GET_CURRENT_STATE();
	ClientArray* array = _getArray(cap, 1);
	if (array == NULL) return;
	if (debug_array_ptr)
	{
		if (cap == GL_TEXTURE_COORD_ARRAY)
			log_gl("disable texture %d\n", state->client_state.clientActiveTexture);
		else
			log_gl("disable feature %s\n", _getArrayName(cap));
	}

	if ((!disable_optim) && array->enabled == 0)
	{
		//log_gl("discard useless command\n");
		return;
	}
	array->enabled = 0;
	/*if ((!disable_optim) && cap == GL_TEXTURE_COORD_ARRAY)
	  {
	  long args[] = { UNSIGNED_INT_TO_ARG(state->client_state.clientActiveTexture + GL_TEXTURE0_ARB)};
	  do_opengl_call(glClientActiveTexture_func, NULL, args, NULL);
	  }*/
	long args[] = { UNSIGNED_INT_TO_ARG(cap)};
	do_opengl_call(glDisableClientState_func, NULL, args, NULL);
}


GLAPI void APIENTRY glClientActiveTexture(GLenum texture)
{
	GET_CURRENT_STATE();

	if (disable_optim || state->client_state.clientActiveTexture != (int)texture - GL_TEXTURE0_ARB)
	{
		long args[] = { UNSIGNED_INT_TO_ARG(texture)};
		do_opengl_call(glClientActiveTexture_func, NULL, args, NULL);
	}

	state->client_state.clientActiveTexture = (int)texture - GL_TEXTURE0_ARB;

	assert(state->client_state.clientActiveTexture >= 0 &&
			state->client_state.clientActiveTexture < NB_MAX_TEXTURES);
}

GLAPI void APIENTRY glClientActiveTextureARB(GLenum texture)
{
	glClientActiveTexture(texture);
}

GLAPI void APIENTRY glActiveTextureARB(GLenum texture)
{
	GET_CURRENT_STATE();
	if (disable_optim || state->activeTexture != texture)
	{
		state->activeTexture = texture;

		long args[] = { INT_TO_ARG(texture) };
		do_opengl_call(glActiveTextureARB_func, NULL, args, NULL);
	}
}

GLAPI void APIENTRY glPushClientAttrib(GLbitfield mask)
{
	GET_CURRENT_STATE();
	long args[] = { UNSIGNED_INT_TO_ARG(mask)};
	if (state->client_state_sp < MAX_CLIENT_STATE_STACK_SIZE)
	{
		state->client_state_stack[state->client_state_sp].mask = mask;
		if (mask & GL_CLIENT_VERTEX_ARRAY_BIT)
		{
			state->client_state_stack[state->client_state_sp].arrays = state->client_state.arrays;
		}
		if (mask & GL_CLIENT_PIXEL_STORE_BIT)
		{
			state->client_state_stack[state->client_state_sp].pack = state->client_state.pack;
			state->client_state_stack[state->client_state_sp].unpack = state->client_state.unpack;
		}
		state->client_state_sp++;
	}
	else
	{
		log_gl("state->client_state_sp > MAX_CLIENT_STATE_STACK_SIZE\n");
	}
	do_opengl_call(glPushClientAttrib_func, NULL, args, NULL);
}

GLAPI void APIENTRY glPopClientAttrib()
{
	GET_CURRENT_STATE();
	if (state->client_state_sp > 0)
	{
		state->client_state_sp--;
		if (state->client_state_stack[state->client_state_sp].mask & GL_CLIENT_VERTEX_ARRAY_BIT)
		{
			state->client_state.arrays = state->client_state_stack[state->client_state_sp].arrays;
		}
		if (state->client_state_stack[state->client_state_sp].mask & GL_CLIENT_PIXEL_STORE_BIT)
		{
			state->client_state.pack = state->client_state_stack[state->client_state_sp].pack;
			state->client_state.unpack = state->client_state_stack[state->client_state_sp].unpack;
		}
	}
	else
	{
		log_gl("state->client_state_sp <= 0\n");
	}
	do_opengl_call(glPopClientAttrib_func, NULL, NULL, NULL);
}

static void _glPixelStore(GLenum pname, GLint param)
{
	GET_CURRENT_STATE();
	switch (pname)
	{
		case GL_PACK_SWAP_BYTES: state->client_state.pack.swapEndian = param != 0; break;
		case GL_PACK_LSB_FIRST: state->client_state.pack.lsbFirst = param != 0; break;
		case GL_PACK_ROW_LENGTH: if (param >= 0) state->client_state.pack.rowLength = param; break;
		case GL_PACK_IMAGE_HEIGHT: if (param >= 0) state->client_state.pack.imageHeight = param; break;
		case GL_PACK_SKIP_ROWS: if (param >= 0) state->client_state.pack.skipRows = param; break;
		case GL_PACK_SKIP_PIXELS: if (param >= 0) state->client_state.pack.skipPixels = param; break;
		case GL_PACK_SKIP_IMAGES: if (param >= 0) state->client_state.pack.skipImages = param; break;
		case GL_PACK_ALIGNMENT: if (param == 1 || param == 2 || param == 4 || param == 8)
									state->client_state.pack.alignment = param; break;
		case GL_UNPACK_SWAP_BYTES: state->client_state.unpack.swapEndian = param != 0; break;
		case GL_UNPACK_LSB_FIRST: state->client_state.unpack.lsbFirst = param != 0; break;
		case GL_UNPACK_ROW_LENGTH: if (param >= 0) state->client_state.unpack.rowLength = param; break;
		case GL_UNPACK_IMAGE_HEIGHT: if (param >= 0) state->client_state.unpack.imageHeight = param; break;
		case GL_UNPACK_SKIP_ROWS: if (param >= 0) state->client_state.unpack.skipRows = param; break;
		case GL_UNPACK_SKIP_PIXELS: if (param >= 0) state->client_state.unpack.skipPixels = param; break;
		case GL_UNPACK_SKIP_IMAGES: if (param >= 0) state->client_state.unpack.skipImages = param; break;
		case GL_UNPACK_ALIGNMENT: if (param == 1 || param == 2 || param == 4 || param == 8)
									  state->client_state.unpack.alignment = param; break;
		default: log_gl("unhandled pname %d\n", pname); break;
	}
}

GLAPI void APIENTRY glPixelStoref(GLenum pname, GLfloat param)
{
	long args[] = { UNSIGNED_INT_TO_ARG(pname), FLOAT_TO_ARG(param)};
	if (!(pname == GL_PACK_SKIP_PIXELS || pname == GL_PACK_SKIP_PIXELS || pname == GL_PACK_SKIP_IMAGES ||
				pname == GL_UNPACK_SKIP_PIXELS || pname == GL_UNPACK_SKIP_PIXELS || pname == GL_UNPACK_SKIP_IMAGES))
	{
		_glPixelStore(pname, (GLint)(param + 0.5));
		do_opengl_call(glPixelStoref_func, NULL, args, NULL);
	}
}

static void glPixelStorei_no_lock(GLenum pname, GLint param)
{
	_glPixelStore(pname, param);
	if (!(pname == GL_PACK_SKIP_PIXELS || pname == GL_PACK_SKIP_ROWS || pname == GL_PACK_SKIP_IMAGES ||
				pname == GL_UNPACK_SKIP_PIXELS || pname == GL_UNPACK_SKIP_ROWS || pname == GL_UNPACK_SKIP_IMAGES))
	{
		long args[] = { UNSIGNED_INT_TO_ARG(pname), INT_TO_ARG(param)};
		do_opengl_call_no_lock(glPixelStorei_func, NULL, args, NULL);
	}
}

GLAPI void APIENTRY glPixelStorei(GLenum pname, GLint param)
{
	LOCK(glPixelStorei_func);
	glPixelStorei_no_lock(pname, param);
	UNLOCK(glPixelStorei_func);
}

static int get_nb_composants_of_gl_get_constant_compare(const void* a, const void* b)
{
	GlGetConstant* constantA = (GlGetConstant*)a;
	GlGetConstant* constantB = (GlGetConstant*)b;
	return constantA->token - constantB->token;
}

static int get_size_get_boolean_integer_float_double_v(int func_number, int pname)
{
	GlGetConstant constant;
	GlGetConstant* found;
	constant.token = pname;
	found = bsearch(&constant, gl_get_constants,
			sizeof(gl_get_constants) / sizeof(GlGetConstant), sizeof(GlGetConstant),
			get_nb_composants_of_gl_get_constant_compare);
	if (found)
		return found->count;
	else
	{
		log_gl("unknown name for %s : %d\n", tab_opengl_calls_name[func_number], pname);
		log_gl("hoping that size is 1...\n");
		return 1;
	}
}


#define AFFECT_N(x, y, N) do { int _i; for(_i=0;_i<N;_i++) x[_i]=y[_i]; } while(0)

#define IS_VALID_MATRIX_MODE(mode) \
	((mode >= GL_MODELVIEW && mode <= GL_TEXTURE) || \
	 (mode == GL_MATRIX_PALETTE_ARB) || \
	 (mode == GL_MODELVIEW1_ARB) || \
	 (mode >= GL_MODELVIEW2_ARB && mode <= GL_MODELVIEW31_ARB))

#define MATRIX_MODE_TO_MATRIX_INDEX(mode) \
	((mode == GL_MODELVIEW) ? 0 : \
	 (mode == GL_PROJECTION) ? 1 : \
	 (mode == GL_TEXTURE) ? 2 + state->activeTexture - GL_TEXTURE0_ARB: \
	 (mode == GL_MATRIX_PALETTE_ARB) ? 2 + NB_MAX_TEXTURES : \
	 (mode == GL_MODELVIEW1_ARB) ? 3 + NB_MAX_TEXTURES : \
	 (mode >= GL_MODELVIEW2_ARB && mode <= GL_MODELVIEW31_ARB) ? 4 + NB_MAX_TEXTURES + mode - GL_MODELVIEW2_ARB : -1)

static int maxTextureSize = -1; /* optimization for ppracer */
static int maxTextureUnits = -1;

static int _glGetIntegerv(GLenum pname)
{
	int ret;
	long args[] = { INT_TO_ARG(pname), POINTER_TO_ARG(&ret) };
	int args_size[] = { 0, sizeof(int) };
	do_opengl_call_no_lock(glGetIntegerv_func, NULL, CHECK_ARGS(args, args_size));
	return ret;
}

#define glGetf(funcName, funcNumber, cType, typeBase) \
	static void CONCAT(funcName,_no_lock)( GLenum pname, cType *params ) \
{ \
	GET_CURRENT_STATE(); \
	switch (pname) \
	{ \
		case GL_VERTEX_ARRAY: *params = state->client_state.arrays.vertexArray.enabled; break; \
		case GL_NORMAL_ARRAY: *params = state->client_state.arrays.normalArray.enabled; break; \
		case GL_COLOR_ARRAY: *params = state->client_state.arrays.colorArray.enabled; break; \
		case GL_INDEX_ARRAY: *params = state->client_state.arrays.indexArray.enabled; break; \
		case GL_TEXTURE_COORD_ARRAY: *params = state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].enabled; break; \
		case GL_EDGE_FLAG_ARRAY: *params = state->client_state.arrays.edgeFlagArray.enabled; break; \
		case GL_SECONDARY_COLOR_ARRAY: *params = state->client_state.arrays.secondaryColorArray.enabled; break; \
		case GL_FOG_COORDINATE_ARRAY: *params = state->client_state.arrays.fogCoordArray.enabled; break; \
		case GL_WEIGHT_ARRAY_ARB: *params = state->client_state.arrays.weightArray.enabled; break; \
		case GL_VERTEX_ARRAY_SIZE: *params = state->client_state.arrays.vertexArray.size; break; \
		case GL_VERTEX_ARRAY_TYPE: *params = state->client_state.arrays.vertexArray.type; break; \
		case GL_VERTEX_ARRAY_STRIDE: *params = state->client_state.arrays.vertexArray.stride; break; \
		case GL_NORMAL_ARRAY_TYPE: *params = state->client_state.arrays.normalArray.type; break; \
		case GL_NORMAL_ARRAY_STRIDE: *params = state->client_state.arrays.normalArray.stride; break; \
		case GL_COLOR_ARRAY_SIZE: *params = state->client_state.arrays.colorArray.size; break; \
		case GL_COLOR_ARRAY_TYPE: *params = state->client_state.arrays.colorArray.type; break; \
		case GL_COLOR_ARRAY_STRIDE: *params = state->client_state.arrays.colorArray.stride; break; \
		case GL_INDEX_ARRAY_TYPE: *params = state->client_state.arrays.indexArray.type; break; \
		case GL_INDEX_ARRAY_STRIDE: *params = state->client_state.arrays.indexArray.stride; break; \
		case GL_TEXTURE_COORD_ARRAY_SIZE: *params = state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].size; break; \
		case GL_TEXTURE_COORD_ARRAY_TYPE: *params = state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].type; break; \
		case GL_TEXTURE_COORD_ARRAY_STRIDE: *params = state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].stride; break; \
		case GL_EDGE_FLAG_ARRAY_STRIDE: *params = state->client_state.arrays.edgeFlagArray.stride; break; \
		case GL_SECONDARY_COLOR_ARRAY_SIZE: *params = state->client_state.arrays.secondaryColorArray.size; break; \
		case GL_SECONDARY_COLOR_ARRAY_TYPE: *params = state->client_state.arrays.secondaryColorArray.type; break; \
		case GL_SECONDARY_COLOR_ARRAY_STRIDE: *params = state->client_state.arrays.secondaryColorArray.stride; break; \
		case GL_FOG_COORDINATE_ARRAY_TYPE: *params = state->client_state.arrays.fogCoordArray.type; break; \
		case GL_FOG_COORDINATE_ARRAY_POINTER: *params = state->client_state.arrays.fogCoordArray.stride; break; \
		case GL_WEIGHT_ARRAY_SIZE_ARB: *params = state->client_state.arrays.weightArray.size; break; \
		case GL_WEIGHT_ARRAY_TYPE_ARB: *params = state->client_state.arrays.weightArray.type; break; \
		case GL_WEIGHT_ARRAY_STRIDE_ARB: *params = state->client_state.arrays.weightArray.stride; break; \
		case GL_MATRIX_INDEX_ARRAY_SIZE_ARB: *params = state->client_state.arrays.matrixIndexArray.size; break; \
		case GL_MATRIX_INDEX_ARRAY_TYPE_ARB: *params = state->client_state.arrays.matrixIndexArray.type; break; \
		case GL_MATRIX_INDEX_ARRAY_STRIDE_ARB: *params = state->client_state.arrays.matrixIndexArray.stride; break; \
		case GL_VERTEX_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.vertexArray.vbo_name; break; \
		case GL_NORMAL_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.normalArray.vbo_name; break; \
		case GL_COLOR_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.colorArray.vbo_name; break; \
		case GL_INDEX_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.indexArray.vbo_name; break; \
		case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].vbo_name; break; \
		case GL_EDGE_FLAG_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.edgeFlagArray.vbo_name; break; \
		case GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.secondaryColorArray.vbo_name; break; \
		case GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.fogCoordArray.vbo_name; break; \
		case GL_WEIGHT_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.weightArray.vbo_name; break; \
		case GL_PACK_SWAP_BYTES: *params = state->client_state.pack.swapEndian; break; \
		case GL_PACK_LSB_FIRST: *params = state->client_state.pack.lsbFirst; break; \
		case GL_PACK_ROW_LENGTH: *params = state->client_state.pack.rowLength; break; \
		case GL_PACK_IMAGE_HEIGHT: *params = state->client_state.pack.imageHeight; break; \
		case GL_PACK_SKIP_ROWS: *params = state->client_state.pack.skipRows; break; \
		case GL_PACK_SKIP_PIXELS: *params = state->client_state.pack.skipPixels; break; \
		case GL_PACK_SKIP_IMAGES: *params = state->client_state.pack.skipImages; break; \
		case GL_PACK_ALIGNMENT: *params = state->client_state.pack.alignment; break; \
		case GL_UNPACK_SWAP_BYTES: *params = state->client_state.unpack.swapEndian; break; \
		case GL_UNPACK_LSB_FIRST: *params = state->client_state.unpack.lsbFirst; break; \
		case GL_UNPACK_ROW_LENGTH: *params = state->client_state.unpack.rowLength; break; \
		case GL_UNPACK_IMAGE_HEIGHT: *params = state->client_state.unpack.imageHeight; break; \
		case GL_UNPACK_SKIP_ROWS: *params = state->client_state.unpack.skipRows; break; \
		case GL_UNPACK_SKIP_PIXELS: *params = state->client_state.unpack.skipPixels; break; \
		case GL_UNPACK_SKIP_IMAGES: *params = state->client_state.unpack.skipImages; break; \
		case GL_UNPACK_ALIGNMENT: *params = state->client_state.unpack.alignment; break; \
		case GL_TEXTURE_2D_BINDING_EXT: *params = state->current_server_state.bindTexture2D; break; \
		case GL_TEXTURE_BINDING_RECTANGLE_ARB: *params = state->current_server_state.bindTextureRectangle; break; \
		case GL_VIEWPORT: params[0] = state->viewport.x; params[1] = state->viewport.y; params[2] = state->viewport.width; params[3] = state->viewport.height; break; \
		case GL_SCISSOR_BOX: params[0] = state->scissorbox.x; params[1] = state->scissorbox.y; params[2] = state->scissorbox.width; params[3] = state->scissorbox.height; break; \
		case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: *params = 16; break;\
		case GL_MATRIX_MODE: *params = state->current_server_state.matrixMode; break; \
		case GL_DEPTH_FUNC: *params = state->current_server_state.depthFunc; break; \
		case GL_FOG_MODE: *params = state->current_server_state.fog.mode; break; \
		case GL_FOG_DENSITY: *params = state->current_server_state.fog.density; break; \
		case GL_FOG_START: *params = state->current_server_state.fog.start; break; \
		case GL_FOG_END: *params = state->current_server_state.fog.end; break; \
		case GL_FOG_INDEX: *params = state->current_server_state.fog.index; break; \
		case GL_FOG_COLOR: AFFECT_N(params, state->current_server_state.fog.color, 4); break; \
		case GL_COMPRESSED_TEXTURE_FORMATS_ARB: \
												{ \
													long args[] = { INT_TO_ARG(pname), POINTER_TO_ARG(params) }; \
													int args_size[] = { 0, 0 }; \
													int nb_compressed_texture_formats = 0; \
													glGetIntegerv_no_lock(GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB, &nb_compressed_texture_formats); \
													args_size[1] = tab_args_type_length[typeBase] * nb_compressed_texture_formats; \
													do_opengl_call_no_lock(funcNumber, NULL, CHECK_ARGS(args, args_size)); \
													break; \
												} \
		case GL_ARRAY_BUFFER_BINDING: *params = state->arrayBuffer; break; \
		case GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB: *params = state->elementArrayBuffer; break; \
		case GL_MAX_TEXTURE_SIZE: if (maxTextureSize == -1) maxTextureSize = _glGetIntegerv(pname); *params = maxTextureSize; break; \
		case GL_MAX_TEXTURE_UNITS_ARB: if (maxTextureUnits == -1) maxTextureUnits = _glGetIntegerv(pname); *params = maxTextureUnits; break; \
		case GL_MODELVIEW_MATRIX: \
		case GL_PROJECTION_MATRIX: \
		case GL_TEXTURE_MATRIX: AFFECT_N(params, state->matrix[pname - GL_MODELVIEW_MATRIX].current.val, 16); break; \
		case GL_CURRENT_RASTER_POSITION: \
										 { \
											 if (!state->currentRasterPosKnown) \
											 { \
												 long args[] = { INT_TO_ARG(pname), POINTER_TO_ARG(state->currentRasterPos) }; \
												 int args_size[] = { 0, 4 * sizeof(float) }; \
												 log_gl("getting value 0x%X\n", pname); \
												 do_opengl_call_no_lock(glGetFloatv_func, NULL, CHECK_ARGS(args, args_size)); \
												 state->currentRasterPosKnown = 1; \
											 } \
											 AFFECT_N(params, state->currentRasterPos, 4); \
											 break; \
										 } \
		default: \
				 { \
					 long args[] = { INT_TO_ARG(pname), POINTER_TO_ARG(params) }; \
					 int args_size[] = { 0, 0 }; \
					 args_size[1] = tab_args_type_length[typeBase] * get_size_get_boolean_integer_float_double_v(funcNumber, pname); \
					 log_gl("getting value 0x%X\n", pname); \
					 do_opengl_call_no_lock(funcNumber, NULL, CHECK_ARGS(args, args_size)); \
					 if (typeBase == TYPE_INT) log_gl("val=%d\n", (int)*params); \
					 else if (typeBase == TYPE_FLOAT) log_gl("val=%f\n", (float)*params); \
				 } \
	} \
} \
GLAPI void APIENTRY funcName( GLenum pname, cType *params ) \
{ \
	LOCK(funcNumber); \
	CONCAT(funcName,_no_lock)(pname, params); \
	UNLOCK(funcNumber); \
}

glGetf(glGetBooleanv, glGetBooleanv_func, GLboolean, TYPE_CHAR);
glGetf(glGetIntegerv, glGetIntegerv_func, GLint, TYPE_INT);
glGetf(glGetFloatv, glGetFloatv_func, GLfloat, TYPE_FLOAT);
glGetf(glGetDoublev, glGetDoublev_func, GLdouble, TYPE_DOUBLE);

GLAPI void APIENTRY glDepthFunc(GLenum func)
{
	long args[] = { UNSIGNED_INT_TO_ARG(func)};
	GET_CURRENT_STATE();
	state->current_server_state.depthFunc = func;
	do_opengl_call(glDepthFunc_func, NULL, args, NULL);
}

GLAPI void APIENTRY glClipPlane(GLenum plane, const GLdouble * equation)
{
	GET_CURRENT_STATE();
	if (plane >= GL_CLIP_PLANE0 && plane < GL_CLIP_PLANE0 + N_CLIP_PLANES)
		memcpy(state->current_server_state.clipPlanes[plane-GL_CLIP_PLANE0], equation, 4 * sizeof(double));
	long args[] = { UNSIGNED_INT_TO_ARG(plane), POINTER_TO_ARG(equation)};
	do_opengl_call(glClipPlane_func, NULL, args, NULL);
}

GLAPI void APIENTRY glGetClipPlane(GLenum plane, GLdouble * equation)
{
	GET_CURRENT_STATE();
	if (plane >= GL_CLIP_PLANE0 && plane < GL_CLIP_PLANE0 + N_CLIP_PLANES)
	{
		memcpy(equation, state->current_server_state.clipPlanes[plane-GL_CLIP_PLANE0], 4 * sizeof(double));
		return;
	}
	long args[] = { UNSIGNED_INT_TO_ARG(plane), POINTER_TO_ARG(equation)};
	do_opengl_call(glGetClipPlane_func, NULL, args, NULL);
}


GLAPI void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	GET_CURRENT_STATE();
	long args[] = { INT_TO_ARG(x), INT_TO_ARG(y), INT_TO_ARG(width), INT_TO_ARG(height)};
	state->viewport.x = x;
	state->viewport.y = y;
	state->viewport.width = width;
	state->viewport.height = height;
	if (debug_gl) log_gl("viewport %d,%d,%d,%d\n", x, y, width, height);
	do_opengl_call(glViewport_func, NULL, args, NULL);
}

GLAPI void APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	GET_CURRENT_STATE();
	long args[] = { INT_TO_ARG(x), INT_TO_ARG(y), INT_TO_ARG(width), INT_TO_ARG(height)};
	state->scissorbox.x = x;
	state->scissorbox.y = y;
	state->scissorbox.width = width;
	state->scissorbox.height = height;
	do_opengl_call(glScissor_func, NULL, args, NULL);
}


/* Matrix optimization : openquartz */
#if 1
GLAPI void APIENTRY glMatrixMode(GLenum mode)
{
	GET_CURRENT_STATE();
	long args[] = { UNSIGNED_INT_TO_ARG(mode)};
	if (IS_VALID_MATRIX_MODE(mode))
		state->current_server_state.matrixMode = mode;
	do_opengl_call(glMatrixMode_func, NULL, args, NULL);
}

GLAPI void APIENTRY glPushMatrix()
{
	GET_CURRENT_STATE();
	int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
	if (index_mode >= 0)
	{
		if (state->matrix[index_mode].sp < MAX_MATRIX_STACK_SIZE)
		{
			memcpy(state->matrix[index_mode].stack[state->matrix[index_mode].sp].val,
					state->matrix[index_mode].current.val,
					16 * sizeof(double));
			state->matrix[index_mode].sp++;
		}
		else
		{
			log_gl("matrix[mode].sp >= MAX_MATRIX_STACK_SIZE\n");
		}
	}

	do_opengl_call(glPushMatrix_func, NULL, NULL, NULL);
}

GLAPI void APIENTRY glPopMatrix()
{
	GET_CURRENT_STATE();
	int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
	if (index_mode >= 0)
	{
		if (state->matrix[index_mode].sp > 0)
		{
			state->matrix[index_mode].sp--;
			memcpy(state->matrix[index_mode].current.val,
					state->matrix[index_mode].stack[state->matrix[index_mode].sp].val,
					16 * sizeof(double));
		}
		else
		{
			log_gl("matrix[mode].sp <= 0\n");
		}
	}

	do_opengl_call(glPopMatrix_func, NULL, NULL, NULL);
}

GLAPI void APIENTRY glLoadIdentity()
{
	GET_CURRENT_STATE();
	int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
	int j;
	if (index_mode >= 0)
	{
		for(j=0;j<16;j++)
		{
			state->matrix[index_mode].current.val[j] = (j == 0 || j == 5 || j == 10 || j == 15);
		}
	}
	do_opengl_call(glLoadIdentity_func, NULL, NULL, NULL);
}

static void _internal_glLoadMatrixd(const GLdouble matrix[16])
{
	GET_CURRENT_STATE();
	int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
	if (index_mode >= 0)
		memcpy(state->matrix[index_mode].current.val, matrix, 16 * sizeof(double));
}

static void _internal_glLoadMatrixf(const GLfloat matrix[16])
{
	GET_CURRENT_STATE();
	int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
	if (index_mode >= 0)
	{
		int i;
		for(i=0;i<16;i++)
			state->matrix[index_mode].current.val[i] = matrix[i];
	}
}

GLAPI void APIENTRY glLoadMatrixd(const GLdouble matrix[16])
{
	_internal_glLoadMatrixd(matrix);
	long args[] = { POINTER_TO_ARG(matrix)};
	do_opengl_call(glLoadMatrixd_func, NULL, args, NULL);
}

static void matrixfToMatrixd(const GLfloat matrixf[16], GLdouble matrix[16])
{
	int i;
	for(i=0;i<16;i++)
	{
		matrix[i] = matrixf[i];
	}
}

GLAPI void APIENTRY glLoadMatrixf(const GLfloat matrix[16])
{
	_internal_glLoadMatrixf(matrix);

	long args[] = { POINTER_TO_ARG(matrix)};
	do_opengl_call(glLoadMatrixf_func, NULL, args, NULL);
}

static void _internal_glMultMatrixd(const GLdouble matrix[16])
{
	GET_CURRENT_STATE();
	GLdouble destMatrix[16];
	int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
	if (index_mode >= 0)
	{
		GLdouble* oriMatrix = state->matrix[index_mode].current.val;

		/* t(C)=t(A.B)=t(B).t(A) */

		destMatrix[0*4+0] = matrix[0*4+0] * oriMatrix[0*4+0] +
			matrix[0*4+1] * oriMatrix[1*4+0] +
			matrix[0*4+2] * oriMatrix[2*4+0] +
			matrix[0*4+3] * oriMatrix[3*4+0];
		destMatrix[0*4+1] = matrix[0*4+0] * oriMatrix[0*4+1] +
			matrix[0*4+1] * oriMatrix[1*4+1] +
			matrix[0*4+2] * oriMatrix[2*4+1] +
			matrix[0*4+3] * oriMatrix[3*4+1];
		destMatrix[0*4+2] = matrix[0*4+0] * oriMatrix[0*4+2] +
			matrix[0*4+1] * oriMatrix[1*4+2] +
			matrix[0*4+2] * oriMatrix[2*4+2] +
			matrix[0*4+3] * oriMatrix[3*4+2];
		destMatrix[0*4+3] = matrix[0*4+0] * oriMatrix[0*4+3] +
			matrix[0*4+1] * oriMatrix[1*4+3] +
			matrix[0*4+2] * oriMatrix[2*4+3] +
			matrix[0*4+3] * oriMatrix[3*4+3];

		destMatrix[1*4+0] = matrix[1*4+0] * oriMatrix[0*4+0] +
			matrix[1*4+1] * oriMatrix[1*4+0] +
			matrix[1*4+2] * oriMatrix[2*4+0] +
			matrix[1*4+3] * oriMatrix[3*4+0];
		destMatrix[1*4+1] = matrix[1*4+0] * oriMatrix[0*4+1] +
			matrix[1*4+1] * oriMatrix[1*4+1] +
			matrix[1*4+2] * oriMatrix[2*4+1] +
			matrix[1*4+3] * oriMatrix[3*4+1];
		destMatrix[1*4+2] = matrix[1*4+0] * oriMatrix[0*4+2] +
			matrix[1*4+1] * oriMatrix[1*4+2] +
			matrix[1*4+2] * oriMatrix[2*4+2] +
			matrix[1*4+3] * oriMatrix[3*4+2];
		destMatrix[1*4+3] = matrix[1*4+0] * oriMatrix[0*4+3] +
			matrix[1*4+1] * oriMatrix[1*4+3] +
			matrix[1*4+2] * oriMatrix[2*4+3] +
			matrix[1*4+3] * oriMatrix[3*4+3];

		destMatrix[2*4+0] = matrix[2*4+0] * oriMatrix[0*4+0] +
			matrix[2*4+1] * oriMatrix[1*4+0] +
			matrix[2*4+2] * oriMatrix[2*4+0] +
			matrix[2*4+3] * oriMatrix[3*4+0];
		destMatrix[2*4+1] = matrix[2*4+0] * oriMatrix[0*4+1] +
			matrix[2*4+1] * oriMatrix[1*4+1] +
			matrix[2*4+2] * oriMatrix[2*4+1] +
			matrix[2*4+3] * oriMatrix[3*4+1];
		destMatrix[2*4+2] = matrix[2*4+0] * oriMatrix[0*4+2] +
			matrix[2*4+1] * oriMatrix[1*4+2] +
			matrix[2*4+2] * oriMatrix[2*4+2] +
			matrix[2*4+3] * oriMatrix[3*4+2];
		destMatrix[2*4+3] = matrix[2*4+0] * oriMatrix[0*4+3] +
			matrix[2*4+1] * oriMatrix[1*4+3] +
			matrix[2*4+2] * oriMatrix[2*4+3] +
			matrix[2*4+3] * oriMatrix[3*4+3];

		destMatrix[3*4+0] = matrix[3*4+0] * oriMatrix[0*4+0] +
			matrix[3*4+1] * oriMatrix[1*4+0] +
			matrix[3*4+2] * oriMatrix[2*4+0] +
			matrix[3*4+3] * oriMatrix[3*4+0];
		destMatrix[3*4+1] = matrix[3*4+0] * oriMatrix[0*4+1] +
			matrix[3*4+1] * oriMatrix[1*4+1] +
			matrix[3*4+2] * oriMatrix[2*4+1] +
			matrix[3*4+3] * oriMatrix[3*4+1];
		destMatrix[3*4+2] = matrix[3*4+0] * oriMatrix[0*4+2] +
			matrix[3*4+1] * oriMatrix[1*4+2] +
			matrix[3*4+2] * oriMatrix[2*4+2] +
			matrix[3*4+3] * oriMatrix[3*4+2];
		destMatrix[3*4+3] = matrix[3*4+0] * oriMatrix[0*4+3] +
			matrix[3*4+1] * oriMatrix[1*4+3] +
			matrix[3*4+2] * oriMatrix[2*4+3] +
			matrix[3*4+3] * oriMatrix[3*4+3];

		memcpy(oriMatrix, destMatrix, 16 * sizeof(double));
	}
}

GLAPI void APIENTRY glMultMatrixd(const GLdouble matrix[16])
{
	_internal_glMultMatrixd(matrix);

	long args[] = { POINTER_TO_ARG(matrix)};
	do_opengl_call(glMultMatrixd_func, NULL, args, NULL);
}

GLAPI void APIENTRY glMultMatrixf(const GLfloat matrix[16])
{
	GLdouble matrixd[16];
	matrixfToMatrixd(matrix, matrixd);
	_internal_glMultMatrixd(matrixd);

	long args[] = { POINTER_TO_ARG(matrix)};
	do_opengl_call(glMultMatrixf_func, NULL, args, NULL);
}

/**
 * Transpose a GLfloat matrix.
 *
 * \param to destination array.
 * \param from source array.
 */
	static void
_math_transposef( GLfloat to[16], const GLfloat from[16] )
{
	to[0] = from[0];
	to[1] = from[4];
	to[2] = from[8];
	to[3] = from[12];
	to[4] = from[1];
	to[5] = from[5];
	to[6] = from[9];
	to[7] = from[13];
	to[8] = from[2];
	to[9] = from[6];
	to[10] = from[10];
	to[11] = from[14];
	to[12] = from[3];
	to[13] = from[7];
	to[14] = from[11];
	to[15] = from[15];
}


/**
 * Transpose a GLdouble matrix.
 *
 * \param to destination array.
 * \param from source array.
 */
	static void
_math_transposed( GLdouble to[16], const GLdouble from[16] )
{
	to[0] = from[0];
	to[1] = from[4];
	to[2] = from[8];
	to[3] = from[12];
	to[4] = from[1];
	to[5] = from[5];
	to[6] = from[9];
	to[7] = from[13];
	to[8] = from[2];
	to[9] = from[6];
	to[10] = from[10];
	to[11] = from[14];
	to[12] = from[3];
	to[13] = from[7];
	to[14] = from[11];
	to[15] = from[15];
}

GLAPI void APIENTRY glLoadTransposeMatrixf (const GLfloat m[16])
{
	GLfloat dest[16];
	_math_transposef(dest, m);
	glLoadMatrixf(dest);
}

GLAPI void APIENTRY glLoadTransposeMatrixd (const GLdouble m[16])
{
	GLdouble dest[16];
	_math_transposed(dest, m);
	glLoadMatrixd(dest);
}

GLAPI void APIENTRY glMultTransposeMatrixf (const GLfloat m[16])
{
	GLfloat dest[16];
	_math_transposef(dest, m);
	glMultMatrixf(dest);
}

GLAPI void APIENTRY glMultTransposeMatrixd (const GLdouble m[16])
{
	GLdouble dest[16];
	_math_transposed(dest, m);
	glMultMatrixd(dest);
}

GLAPI void APIENTRY glLoadTransposeMatrixfARB (const GLfloat* m)
{
	GLfloat dest[16];
	_math_transposef(dest, m);
	glLoadMatrixf(dest);
}

GLAPI void APIENTRY glLoadTransposeMatrixdARB (const GLdouble* m)
{
	GLdouble dest[16];
	_math_transposed(dest, m);
	glLoadMatrixd(dest);
}

GLAPI void APIENTRY glMultTransposeMatrixfARB (const GLfloat* m)
{
	GLfloat dest[16];
	_math_transposef(dest, m);
	glMultMatrixf(dest);
}

GLAPI void APIENTRY glMultTransposeMatrixdARB (const GLdouble* m)
{
	GLdouble dest[16];
	_math_transposed(dest, m);
	glMultMatrixd(dest);
}

GLAPI void APIENTRY glOrtho( GLdouble left,
		GLdouble right,
		GLdouble bottom,
		GLdouble top,
		GLdouble near_val,
		GLdouble far_val)
{
	double tx, ty, tz;
	tx = -(right + left) / (right - left);
	ty = -(top + bottom) / (top - bottom);
	tz = -(far_val + near_val)   / (far_val - near_val);
	double matrix[16] = { 2/(right - left),      0,            0,          0,
		0,       2/(top-bottom),        0,          0,
		0,            0,  -2/(far_val - near_val),   0,
		tx,            ty,               tz,         1 };
	_internal_glMultMatrixd(matrix);

	long args[] = { DOUBLE_TO_ARG(left), DOUBLE_TO_ARG(right), DOUBLE_TO_ARG(bottom), DOUBLE_TO_ARG(top), DOUBLE_TO_ARG(near_val), DOUBLE_TO_ARG(far_val)};
	do_opengl_call(glOrtho_func, NULL, args, NULL);
}

GLAPI void APIENTRY glFrustum( GLdouble left,
		GLdouble right,
		GLdouble bottom,
		GLdouble top,
		GLdouble near_val,
		GLdouble far_val)
{
	double V1, V2, A, B, C, D;
	V1 = 2 * near_val / (right - left);
	V2 = 2 * near_val / (top - bottom);
	A = (right + left) / (right - left);
	B = (top + bottom) / (top - bottom);
	C = -(far_val + near_val)   / (far_val - near_val);
	D = -2 * far_val * near_val / (far_val - near_val);
	double matrix[16] = { V1, 0, 0, 0,
		0, V2, 0, 0,
		A,  B, C, -1,
		0,  0, D, 0};
	_internal_glMultMatrixd(matrix);

	long args[] = { DOUBLE_TO_ARG(left), DOUBLE_TO_ARG(right), DOUBLE_TO_ARG(bottom), DOUBLE_TO_ARG(top), DOUBLE_TO_ARG(near_val), DOUBLE_TO_ARG(far_val)};
	do_opengl_call(glFrustum_func, NULL, args, NULL);

}

static void _glRotate_internal(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
	double c = cos(angle / 180. * M_PI);
	double s = sin(angle / 180. * M_PI);
	if (x == 1 && y == 0 && z == 0)
	{
		GET_CURRENT_STATE();
		int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
		if (index_mode >= 0)
		{
			GLdouble* matrix = state->matrix[index_mode].current.val;
			double t, u;

			t = matrix[1*4+0];
			u = matrix[2*4+0];
			matrix[1*4+0] = c * t + s * u;
			matrix[2*4+0] = c * u - s * t;

			t = matrix[1*4+1];
			u = matrix[2*4+1];
			matrix[1*4+1] = c * t + s * u;
			matrix[2*4+1] = c * u - s * t;

			t = matrix[1*4+2];
			u = matrix[2*4+2];
			matrix[1*4+2] = c * t + s * u;
			matrix[2*4+2] = c * u - s * t;

			t = matrix[1*4+3];
			u = matrix[2*4+3];
			matrix[1*4+3] = c * t + s * u;
			matrix[2*4+3] = c * u - s * t;
		}
	}
	else if (x == 0 && y == 1 && z == 0)
	{
		GET_CURRENT_STATE();
		int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
		if (index_mode >= 0)
		{
			GLdouble* matrix = state->matrix[index_mode].current.val;
			double t, u;

			t = matrix[0*4+0];
			u = matrix[2*4+0];
			matrix[0*4+0] = c * t - s * u;
			matrix[2*4+0] = s * t + c * u;

			t = matrix[0*4+1];
			u = matrix[2*4+1];
			matrix[0*4+1] = c * t - s * u;
			matrix[2*4+1] = s * t + c * u;

			t = matrix[0*4+2];
			u = matrix[2*4+2];
			matrix[0*4+2] = c * t - s * u;
			matrix[2*4+2] = s * t + c * u;

			t = matrix[0*4+3];
			u = matrix[2*4+3];
			matrix[0*4+3] = c * t - s * u;
			matrix[2*4+3] = s * t + c * u;
		}
	}
	else if (x == 0 && y == 0 && z == 1)
	{
		GET_CURRENT_STATE();
		int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
		if (index_mode >= 0)
		{
			GLdouble* matrix = state->matrix[index_mode].current.val;
			double t, u;

			t = matrix[0*4+0];
			u = matrix[1*4+0];
			matrix[0*4+0] = c * t + s * u;
			matrix[1*4+0] = c * u - s * t;

			t = matrix[0*4+1];
			u = matrix[1*4+1];
			matrix[0*4+1] = c * t + s * u;
			matrix[1*4+1] = c * u - s * t;

			t = matrix[0*4+2];
			u = matrix[1*4+2];
			matrix[0*4+2] = c * t + s * u;
			matrix[1*4+2] = c * u - s * t;

			t = matrix[0*4+3];
			u = matrix[1*4+3];
			matrix[0*4+3] = c * t + s * u;
			matrix[1*4+3] = c * u - s * t;
		}
	}
	else
	{
		double sqrt_sum_sqr = sqrt(x*x+y*y+z*z);
		if (sqrt_sum_sqr < 1e-4) return;
		x /= sqrt_sum_sqr;
		y /= sqrt_sum_sqr;
		z /= sqrt_sum_sqr;
		double matrix[16] = { x*x*(1-c)+c, y*x*(1-c)+z*s, x*z*(1-c)-y*s, 0,
			x*y*(1-c)-z*s, y*y*(1-c)+c, y*z*(1-c)+x*s, 0,
			x*z*(1-c)+y*s, y*z*(1-c)-x*s, z*z*(1-c)+c, 0,
			0, 0, 0, 1};
		_internal_glMultMatrixd(matrix);
	}
}

GLAPI void APIENTRY glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
	_glRotate_internal(angle, x, y, z);

	long args[] = { DOUBLE_TO_ARG(angle), DOUBLE_TO_ARG(x), DOUBLE_TO_ARG(y), DOUBLE_TO_ARG(z)};
	do_opengl_call(glRotated_func, NULL, args, NULL);
}

GLAPI void APIENTRY glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	_glRotate_internal(angle, x, y, z);

	long args[] = { FLOAT_TO_ARG(angle), FLOAT_TO_ARG(x), FLOAT_TO_ARG(y), FLOAT_TO_ARG(z)};
	do_opengl_call(glRotatef_func, NULL, args, NULL);
}

static void _glScale_internal(double a, double b, double c)
{
	GET_CURRENT_STATE();
	int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
	if (index_mode >= 0)
	{
		GLdouble* matrix = state->matrix[index_mode].current.val;
		matrix[0*4+0] *= a;
		matrix[0*4+1] *= a;
		matrix[0*4+2] *= a;
		matrix[0*4+3] *= a;
		matrix[1*4+0] *= b;
		matrix[1*4+1] *= b;
		matrix[1*4+2] *= b;
		matrix[1*4+3] *= b;
		matrix[2*4+0] *= c;
		matrix[2*4+1] *= c;
		matrix[2*4+2] *= c;
		matrix[2*4+3] *= c;
	}
}

GLAPI void APIENTRY glScaled(GLdouble x, GLdouble y, GLdouble z)
{
	_glScale_internal(x, y, z);

	long args[] = { DOUBLE_TO_ARG(x), DOUBLE_TO_ARG(y), DOUBLE_TO_ARG(z)};
	do_opengl_call(glScaled_func, NULL, args, NULL);
}

GLAPI void APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	_glScale_internal(x, y, z);

	long args[] = { FLOAT_TO_ARG(x), FLOAT_TO_ARG(y), FLOAT_TO_ARG(z)};
	do_opengl_call(glScalef_func, NULL, args, NULL);
}

static void _glTranslate_internal(double a, double b, double c)
{
	GET_CURRENT_STATE();
	int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
	if (index_mode >= 0)
	{
		GLdouble* matrix = state->matrix[index_mode].current.val;

		matrix[3*4+0] += a * matrix[0*4+0] + b * matrix[1*4+0] + c * matrix[2*4+0];
		matrix[3*4+1] += a * matrix[0*4+1] + b * matrix[1*4+1] + c * matrix[2*4+1];
		matrix[3*4+2] += a * matrix[0*4+2] + b * matrix[1*4+2] + c * matrix[2*4+2];
		matrix[3*4+3] += a * matrix[0*4+3] + b * matrix[1*4+3] + c * matrix[2*4+3];
	}
}

GLAPI void APIENTRY glTranslated(GLdouble x, GLdouble y, GLdouble z)
{
	_glTranslate_internal(x, y, z);

	long args[] = { DOUBLE_TO_ARG(x), DOUBLE_TO_ARG(y), DOUBLE_TO_ARG(z)};
	do_opengl_call(glTranslated_func, NULL, args, NULL);
}

GLAPI void APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	_glTranslate_internal(x, y, z);

	long args[] = { FLOAT_TO_ARG(x), FLOAT_TO_ARG(y), FLOAT_TO_ARG(z)};
	do_opengl_call(glTranslatef_func, NULL, args, NULL);
}
#endif
/* End of matrix optimization */

static void glBindBufferARB_no_lock(GLenum target, GLuint buffer)
{
	CHECK_PROC(glBindBufferARB);
	GET_CURRENT_STATE();
	if (buffer >= 32768)
	{
		log_gl("buffer >= 32768\n");
		return;
	}
	long args[] = {INT_TO_ARG(target), INT_TO_ARG(buffer)};
	if (target == GL_ARRAY_BUFFER_ARB)
	{
		//log_gl("glBindBufferARB(GL_ARRAY_BUFFER,%d)\n", buffer);
		state->arrayBuffer = buffer;
	}
	else if (target == GL_ELEMENT_ARRAY_BUFFER_ARB)
	{
		//log_gl("glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,%d)\n", buffer);
		state->elementArrayBuffer = buffer;
	}
	else if (target == GL_PIXEL_UNPACK_BUFFER_EXT)
	{
		//log_gl("glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT,%d)\n", buffer);
		state->pixelUnpackBuffer = buffer;
	}
	else if (target == GL_PIXEL_PACK_BUFFER_EXT)
	{
		//log_gl("glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT,%d)\n", buffer);
		state->pixelPackBuffer = buffer;
	}
	do_opengl_call_no_lock(glBindBufferARB_func, NULL, args, NULL);
}

GLAPI void APIENTRY EXT_FUNC(glBindBufferARB) (GLenum target, GLuint buffer)
{
	LOCK(glBindBufferARB_func);
	glBindBufferARB_no_lock(target, buffer);
	UNLOCK(glBindBufferARB_func);
}

GLAPI void APIENTRY EXT_FUNC(glBindBuffer) (GLenum target, GLuint buffer)
{
	glBindBufferARB(target, buffer);
}

GLAPI void APIENTRY glGenBuffersARB (GLsizei n, GLuint * tab)
{
	CHECK_PROC(glGenBuffersARB);
	GET_CURRENT_STATE();
	if (n <= 0) { log_gl("n <= 0\n"); return; }
	alloc_range(state->bufferAllocator, n, tab);
	long args[] = { INT_TO_ARG(n) };
	do_opengl_call(glGenBuffersARB_fake_func, NULL, args, NULL);
}

GLAPI void APIENTRY glGenBuffers (GLsizei n, GLuint * tab)
{
	glGenBuffersARB(n, tab);
}

GLAPI void APIENTRY glDeleteBuffersARB (GLsizei n, const GLuint * tab)
{
	CHECK_PROC(glDeleteBuffersARB);
	GET_CURRENT_STATE();
	if (n <= 0) { log_gl("n <= 0\n"); return; }
	delete_range(state->bufferAllocator, n, tab);
	long args[] = { INT_TO_ARG(n), POINTER_TO_ARG(tab) };
	do_opengl_call(glDeleteBuffersARB_func, NULL, args, NULL);
}

GLAPI void APIENTRY glDeleteBuffers(GLsizei n, const GLuint * tab)
{
	glDeleteBuffersARB(n, tab);
}

static Buffer* _get_buffer(GLenum target)
{
	GET_CURRENT_STATE();
	if (target == GL_ARRAY_BUFFER_ARB)
	{
		if (state->arrayBuffer)
			return &state->arrayBuffers[state->arrayBuffer];
		else
			return NULL;
	}
	else if (target == GL_ELEMENT_ARRAY_BUFFER_ARB)
	{
		if (state->elementArrayBuffer)
			return &state->elementArrayBuffers[state->elementArrayBuffer];
		else
			return NULL;
	}
	else if (target == GL_PIXEL_UNPACK_BUFFER_EXT)
	{
		if (state->pixelUnpackBuffer)
			return &state->pixelUnpackBuffers[state->pixelUnpackBuffer];
		else
			return NULL;
	}
	else if (target == GL_PIXEL_PACK_BUFFER_EXT)
	{
		if (state->pixelPackBuffer)
			return &state->pixelPackBuffers[state->pixelPackBuffer];
		else
			return NULL;
	}
	else
	{
		return NULL;
	}
}

GLAPI GLenum APIENTRY glGetError()
{
	int ret = 0;
	if (disable_optim)
	{
		do_opengl_call(glGetError_func, &ret, NULL, NULL);
		log_gl("glGetError() = %d\n", ret);
	}
	else
		do_opengl_call(_glGetError_fake_func, NULL, NULL, NULL);
	return ret;
}

GLAPI void APIENTRY glBufferDataARB (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage)
{
	CHECK_PROC(glBufferDataARB);

	Buffer* buffer = _get_buffer(target);
	if (buffer)
	{
		buffer->usage = usage;
		buffer->size = size;
		buffer->ptr = realloc(buffer->ptr, size);
		if (data) memcpy(buffer->ptr, data, size);
	}
	else
	{
		fprintf(stderr, "unknown buffer/buffer target : %d\n", target);
	}
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(size), POINTER_TO_ARG(data), INT_TO_ARG(usage) };
	int args_size[] = { 0, 0, (data) ? size : 0, 0 };
	do_opengl_call(glBufferDataARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glBufferData (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage)
{
	glBufferDataARB(target, size, data, usage);
}

GLAPI void APIENTRY glBufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data)
{
	CHECK_PROC(glBufferSubDataARB);

	//log_gl("glBufferSubDataARB %d %d\n", offset, size);

	Buffer* buffer = _get_buffer(target);
	if (buffer)
	{
		assert(offset + size <= buffer->size);
		assert(buffer->ptr);
		memcpy(buffer->ptr + offset, data, size);
	}
	else
	{
		fprintf(stderr, "unknown buffer/buffer target : %d\n", target);
	}
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(offset), INT_TO_ARG(size), POINTER_TO_ARG(data) };
	int args_size[] = { 0, 0, 0, size };
	do_opengl_call(glBufferSubDataARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glBufferSubData(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data)
{
	glBufferSubDataARB(target, offset, size, data);
}

GLAPI void APIENTRY glGetBufferSubDataARB (GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid *data)
{
	CHECK_PROC(glGetBufferSubDataARB);

	Buffer* buffer = _get_buffer(target);
	if (!buffer) return;

	assert(offset + size <= buffer->size);
	assert(buffer->ptr);

	memcpy(data, buffer->ptr + offset, size);
}

GLAPI void APIENTRY glGetBufferSubData (GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid *data)
{
	glGetBufferSubDataARB(target, offset, size, data);
}

GLvoid* glMapBufferARB (GLenum target, GLenum access)
{
	CHECK_PROC_WITH_RET(glMapBufferARB);

	Buffer* buffer = _get_buffer(target);
	if (!buffer) return NULL;
	if (target == GL_PIXEL_PACK_BUFFER_EXT && (access == GL_READ_ONLY || access == GL_READ_WRITE))
	{
		int ret_int = 0;
		long args[] = { INT_TO_ARG(target), INT_TO_ARG(buffer->size), POINTER_TO_ARG(buffer->ptr) };
		int args_size[] = { 0, 0, buffer->size };
		do_opengl_call(_glMapBufferARB_fake_func, &ret_int, CHECK_ARGS(args, args_size));
		if (ret_int == 0)
			return NULL;
	}
	buffer->access = access;
	buffer->mapped = 1;
	return buffer->ptr;
}

GLvoid* glMapBuffer(GLenum target, GLenum access)
{
	return glMapBufferARB(target, access);
}

GLAPI void APIENTRY glGetBufferParameterivARB (GLenum target, GLenum pname, GLint *params)
{
	CHECK_PROC(glGetBufferParameterivARB);

	Buffer* buffer = _get_buffer(target);
	if (!buffer) return;

	switch (pname)
	{
		case GL_BUFFER_SIZE_ARB: *params = buffer->size; break;
		case GL_BUFFER_USAGE_ARB: *params = buffer->usage; break;
		case GL_BUFFER_ACCESS_ARB: *params = buffer->access; break;
		case GL_BUFFER_MAPPED_ARB: *params = buffer->mapped; break;
		default:
								   log_gl("unknown pname = 0x%X\n", pname);
	}
}

GLAPI void APIENTRY glGetBufferParameteriv (GLenum target, GLenum pname, GLint *params)
{
	glGetBufferParameterivARB(target, pname, params);
}

GLAPI void APIENTRY glGetBufferPointervARB (GLenum target, GLenum pname, GLvoid* *params)
{
	CHECK_PROC(glGetBufferPointervARB);
	if (pname != GL_BUFFER_MAP_POINTER_ARB)
	{
		log_gl("glGetBufferPointervARB : unknown buffer data pname : %x\n", pname);
		return;
	}
	Buffer* buffer = _get_buffer(target);
	if (!buffer) return;
	if (buffer->mapped) *params = buffer->ptr; else *params = NULL;
}

GLAPI void APIENTRY glGetBufferPointerv (GLenum target, GLenum pname, GLvoid* *params)
{
	glGetBufferPointervARB(target, pname, params);
}

GLAPI GLboolean APIENTRY glUnmapBufferARB(GLenum target)
{
	CHECK_PROC_WITH_RET(glUnmapBufferARB);
	Buffer* buffer = _get_buffer(target);
	if (!buffer) return 0;
	if (!buffer->mapped)
	{
		log_gl("unmapped buffer");
		return 0;
	}
	buffer->mapped = 0;
	if (buffer->access != GL_READ_ONLY)
	{
		glBufferSubDataARB(target, 0, buffer->size, buffer->ptr);
	}
	buffer->access = 0;
	return 1;
}

GLAPI GLboolean APIENTRY glUnmapBuffer(GLenum target)
{
	return glUnmapBufferARB(target);
}

GLAPI void GLAPIENTRY glNewList( GLuint list, GLenum mode )
{
	long args[] = { INT_TO_ARG(list), INT_TO_ARG(mode) };
	GET_CURRENT_STATE();
	alloc_value(state->listAllocator, list);
	do_opengl_call(glNewList_func, NULL, args, NULL);
}

GLAPI void GLAPIENTRY glDeleteLists( GLuint list, GLsizei range )
{
	long args[] = { INT_TO_ARG(list), INT_TO_ARG(range) };
	GET_CURRENT_STATE();
	delete_consecutive_values(state->listAllocator, list, range);
	do_opengl_call(glDeleteLists_func, NULL, args, NULL);
}

GLAPI GLuint GLAPIENTRY glGenLists( GLsizei range )
{
	GET_CURRENT_STATE();
	unsigned int firstValue = alloc_range(state->listAllocator, range, NULL);
	long args[] = { INT_TO_ARG(range) };
	do_opengl_call(glGenLists_fake_func, NULL, args, NULL);
	return firstValue;
}

GLAPI void APIENTRY glCallLists( GLsizei n,
		GLenum type,
		const GLvoid *lists )
{
	long args[] = { INT_TO_ARG(n), INT_TO_ARG(type), POINTER_TO_ARG(lists) };
	int args_size[] = { 0, 0, 0 };
	int size = n;
	if (n <= 0) { log_gl("n <= 0\n"); return; }
	switch(type)
	{
		case GL_BYTE:
		case GL_UNSIGNED_BYTE:
			break;

		case GL_SHORT:
		case GL_UNSIGNED_SHORT:
		case GL_2_BYTES:
			size *= 2;
			break;

		case GL_3_BYTES:
			size *= 3;
			break;

		case GL_INT:
		case GL_UNSIGNED_INT:
		case GL_FLOAT:
		case GL_4_BYTES:
			size *= 4;
			break;

		default:
			log_gl("unsupported type = %d\n", type);
			return;
	}
	args_size[2] = size;
	do_opengl_call(glCallLists_func, NULL, CHECK_ARGS(args, args_size));
}



static void removeUnwantedExtensions(char* ret)
{
	char* toBeRemoved = getenv("GL_REMOVE_EXTENSIONS");
	if (toBeRemoved == NULL) return;
	toBeRemoved = strdup(toBeRemoved);
	char* iterToBeRemoved = toBeRemoved;
	while(*iterToBeRemoved)
	{
		char* cSpace = strchr(iterToBeRemoved, ' ');
		char* cComma = strchr(iterToBeRemoved, ',');
		char* c = (cSpace && cComma) ? MIN(cSpace, cComma) : (cSpace) ? cSpace : (cComma) ? cComma : NULL;
		if (c != NULL)
		{
			*c = 0;
		}
		if (!(*iterToBeRemoved == ' ' || *iterToBeRemoved == ',' || *iterToBeRemoved == 0))
		{
			log_gl("Trying to remove : %s (%d)\n", iterToBeRemoved, *iterToBeRemoved);
			char* c2 = strstr(ret, iterToBeRemoved);
			while (c2)
			{
				memset(c2, 'X', strlen(iterToBeRemoved));
				c2 = strstr(c2 + strlen(iterToBeRemoved), iterToBeRemoved);
			}
		}
		if (c == NULL)
			break;
		iterToBeRemoved = c + 1;
	}
	free(toBeRemoved);
}

#ifndef WIN32
const char *glXQueryExtensionsString( Display *dpy, int screen )
{
	LOCK(glXQueryExtensionsString_func);
	static char* ret = NULL;
	if (ret == NULL)
	{
		long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(screen) };
		do_opengl_call_no_lock(glXQueryExtensionsString_func, &ret, args, NULL);
		ret = strdup(ret);
		removeUnwantedExtensions(ret);
	}
	UNLOCK(glXQueryExtensionsString_func);
	return ret;
}


typedef struct
{
	XVisualInfo* vis;
	int visualid;
	GLXFBConfig fbconfig;
} AssocVisualInfoVisualId;

#define MAX_SIZE_TAB_ASSOC_VISUALINFO_VISUALID  100
AssocVisualInfoVisualId tabAssocVisualInfoVisualId[MAX_SIZE_TAB_ASSOC_VISUALINFO_VISUALID];
int nEltTabAssocVisualInfoVisualId = 0;

static const char* _getAttribNameFromValue(int val)
{
	int i;
	static char buffer[80];
	for(i=0;i<N_REQUESTED_ATTRIBS;i++)
	{
		if (tabRequestedAttribsPair[i].val == val)
			return tabRequestedAttribsPair[i].name;
	}
	sprintf(buffer, "(unknown name = %d, 0x%X)", val, val);
	return buffer;
}

static int _compute_length_of_attrib_list_including_zero(const int* attribList, int booleanMustHaveValue)
{
	int i = 0;
	debug_gl = getenv("DEBUG_GL") != NULL;
	if (debug_gl) log_gl("attribList = \n");
	while(attribList[i])
	{
		if (booleanMustHaveValue ||
				!(attribList[i] == GLX_USE_GL ||
					attribList[i] == GLX_RGBA ||
					attribList[i] == GLX_DOUBLEBUFFER ||
					attribList[i] == GLX_STEREO))
		{
			if (debug_gl) log_gl("%s = %d\n", _getAttribNameFromValue(attribList[i]), attribList[i+1]);
			i+=2;
		}
		else
		{
			if (debug_gl) log_gl("%s\n", _getAttribNameFromValue(attribList[i]));
			i++;
		}
	}
	if (debug_gl) log_gl("\n");
	return i + 1;
}


XVisualInfo* glXChooseVisual( Display *dpy, int screen,
		int *attribList )
{
	XVisualInfo temp, *vis;
	long mask;
	int n;
	int i;

	int visualid = 0;
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(screen), POINTER_TO_ARG(attribList) };
	int args_size[] = { 0, 0, sizeof(int) * _compute_length_of_attrib_list_including_zero(attribList, 0) };
	do_opengl_call(glXChooseVisual_func, &visualid, CHECK_ARGS(args, args_size));

	if (visualid)
	{
		mask = VisualScreenMask | VisualDepthMask | VisualClassMask | VisualIDMask;
		temp.screen = screen;
		temp.depth = DefaultDepth(dpy,screen);
		temp.class = DefaultVisual(dpy,screen)->class;
		temp.visualid = DefaultVisual(dpy,screen)->visualid;

		vis = XGetVisualInfo( dpy, mask, &temp, &n );
		if (vis == NULL)
			log_gl("cannot get visual from client side\n");

		assert (nEltTabAssocVisualInfoVisualId < MAX_SIZE_TAB_ASSOC_VISUALINFO_VISUALID);
		for(i=0;i<nEltTabAssocVisualInfoVisualId;i++)
		{
			if (tabAssocVisualInfoVisualId[i].vis == vis) break;
		}
		if (i == nEltTabAssocVisualInfoVisualId)
			nEltTabAssocVisualInfoVisualId++;
		tabAssocVisualInfoVisualId[i].vis = vis;
		tabAssocVisualInfoVisualId[i].fbconfig = 0;
		tabAssocVisualInfoVisualId[i].visualid = visualid;
	}
	else
	{
		vis = NULL;
	}

	if (debug_gl) log_gl("glXChooseVisual returning vis %p (visualid=%d, 0x%X)\n", vis, visualid, visualid);

	return vis;
}

const char *glXQueryServerString( Display *dpy, int screen, int name )
{
	LOCK(glXQueryServerString_func);
	static char* glXQueryServerString_ret[100] = {NULL};
	assert(name >= 0 && name < 100);
	if (glXQueryServerString_ret[name] == NULL)
	{
		long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(screen), INT_TO_ARG(name) };
		do_opengl_call_no_lock(glXQueryServerString_func, &glXQueryServerString_ret[name], args, NULL);
		glXQueryServerString_ret[name] = strdup(glXQueryServerString_ret[name]);
		if (name == GLX_EXTENSIONS)
		{
			removeUnwantedExtensions(glXQueryServerString_ret[name]);
		}
	}
	UNLOCK(glXQueryServerString_func);
	return glXQueryServerString_ret[name];
}

const char *glXGetClientString( Display *dpy, int name )
{
	LOCK(glXGetClientString_func);
	static char* glXGetClientString_ret[100] = {NULL};
	assert(name >= 0 && name < 100);
	if (glXGetClientString_ret[name] == NULL)
	{
		long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(name) };
		do_opengl_call_no_lock(glXGetClientString_func, &glXGetClientString_ret[name], args, NULL);
		if (getenv("GLX_VENDOR") && name == GLX_VENDOR)
		{
			glXGetClientString_ret[name] = getenv("GLX_VENDOR");
		}
		else
			glXGetClientString_ret[name] = strdup(glXGetClientString_ret[name]);
		if (name == GLX_EXTENSIONS)
		{
			removeUnwantedExtensions(glXGetClientString_ret[name]);
		}
	}
	UNLOCK(glXGetClientString_func);
	return glXGetClientString_ret[name];
}

static void _create_context(GLXContext context, GLXContext shareList)
{
	int i;
	glstates = realloc(glstates, (nbGLStates+1) * sizeof(GLState*));
	glstates[nbGLStates] = new_gl_state();
	glstates[nbGLStates]->ref = 1;
	glstates[nbGLStates]->context = context;
	glstates[nbGLStates]->shareList = shareList;
	glstates[nbGLStates]->pbuffer = 0;
	glstates[nbGLStates]->viewport.width = 0;
	if (shareList)
	{
		for(i=0; i<nbGLStates; i++)
		{
			if (glstates[i]->context == shareList)
			{
				glstates[i]->ref++;
				glstates[nbGLStates]->textureAllocator = glstates[i]->textureAllocator;
				glstates[nbGLStates]->bufferAllocator = glstates[i]->bufferAllocator;
				glstates[nbGLStates]->listAllocator = glstates[i]->listAllocator;
				break;
			}
		}
		if (i == nbGLStates)
		{
			log_gl("unknown shareList %p\n", (void*)shareList);
		}
	}
	nbGLStates++;
}

static GLXFBConfig* glXChooseFBConfig_no_lock( Display *dpy, int screen,
		const int *attribList, int *nitems );
static XVisualInfo* glXGetVisualFromFBConfig_no_lock( Display *dpy, GLXFBConfig config );
static GLXPbuffer glXCreatePbuffer_no_lock(Display *dpy,
		GLXFBConfig config,
		const int *attribList);

GLXContext glXCreateContext( Display *dpy, XVisualInfo *vis,
		GLXContext shareList, Bool direct )
{
	LOCK(glXCreateContext_func);
	int isFbConfigVisual = 0;
	int i;
	int visualid = 0;

	for(i=0;i<nEltTabAssocVisualInfoVisualId;i++)
	{
		if (tabAssocVisualInfoVisualId[i].vis == vis)
		{
			if (tabAssocVisualInfoVisualId[i].fbconfig != NULL)
			{
				log_gl("isFbConfigVisual = 1\n");
				isFbConfigVisual = 1;
			}
			visualid = tabAssocVisualInfoVisualId[i].visualid;
			if (debug_gl) log_gl("found visualid %d corresponding to vis %p\n", visualid, vis);
			break;
		}
	}

	if (getenv("GET_IMG_FROM_SERVER"))
	{
		/* If the visual is already linked to a fbconfig, there's no use to create
		   a new pbuffer ! */
		if (!isFbConfigVisual)
		{
			int nitems;
			int attribs[] = {
				GLX_DOUBLEBUFFER,  True,
				GLX_RED_SIZE,      1,
				GLX_GREEN_SIZE,    1,
				GLX_BLUE_SIZE,     1,
				GLX_DEPTH_SIZE,    1,
				GLX_STENCIL_SIZE, 8,
				GLX_RENDER_TYPE,   GLX_RGBA_BIT,
				GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
				None
			};

			GLXFBConfig* fbconfig = glXChooseFBConfig_no_lock(dpy,
					DefaultScreen( dpy ),
					attribs,
					&nitems);
			if (NULL == fbconfig) {
				log_gl("Error: couldn't get fbconfig\n");
				exit(1);
			}

			XVisualInfo *visinfo = glXGetVisualFromFBConfig_no_lock(dpy, fbconfig[0]);
			GLXContext ctxt = NULL;
			long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(visinfo->visualid), INT_TO_ARG(shareList), INT_TO_ARG(direct) };
			do_opengl_call_no_lock(glXCreateContext_func, &ctxt, args, NULL);

			XFree(visinfo);

			if (ctxt)
			{
				_create_context(ctxt, shareList);

				int pbufAttrib[] = {
					GLX_PBUFFER_WIDTH,   1024,
					GLX_PBUFFER_HEIGHT,  1024,
					GLX_LARGEST_PBUFFER, GL_TRUE,
					None
				};

				glstates[nbGLStates-1]->isAssociatedToFBConfigVisual = isFbConfigVisual;
				glstates[nbGLStates-1]->pbuffer = glXCreatePbuffer_no_lock(dpy, fbconfig[0], pbufAttrib);
				assert(glstates[nbGLStates-1]->pbuffer);
			}

			XFree(fbconfig);

			goto end_of_create_context;
		}
	}

	GLXContext ctxt = NULL;
	if (i == nEltTabAssocVisualInfoVisualId)
	{
		visualid = vis->visualid;
		if (debug_gl) log_gl("not found vis %p in table, visualid=%d\n", vis, visualid);
	}
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(visualid), INT_TO_ARG(shareList), INT_TO_ARG(direct) };
	do_opengl_call_no_lock(glXCreateContext_func, &ctxt, args, NULL);

	if (ctxt)
	{
		_create_context(ctxt, shareList);
		glstates[nbGLStates-1]->isAssociatedToFBConfigVisual = isFbConfigVisual;
	}
end_of_create_context:
	UNLOCK(glXCreateContext_func);
	return ctxt;
}

GLXContext glXGetCurrentContext (void)
{
	GET_CURRENT_STATE();
	if (debug_gl) log_gl("glXGetCurrentContext() -> %p\n", state->context);
	return state->context;
}

GLXDrawable glXGetCurrentDrawable (void)
{
	GET_CURRENT_STATE();
	if (debug_gl) log_gl("glXGetCurrentDrawable() -> %p\n", (void*)state->current_drawable);
	return state->current_drawable;
}

static void _free_context(Display* dpy, int i, GLState* state)
{
	if (state->pbuffer) glXDestroyPbuffer(dpy, state->pbuffer);
	free(state->last_cursor.pixels);
	free(state->ownTextureAllocator.values);
	free(state->ownBufferAllocator.values);
	free(state->ownListAllocator.values);
	free(state);
	memmove(&state, &glstates[i+1], (nbGLStates-i-1) * sizeof(GLState*));
	nbGLStates--;
}

GLAPI void APIENTRY glXDestroyContext( Display *dpy, GLXContext ctx )
{
	int i;
	LOCK(glXDestroyContext_func);
	GET_CURRENT_STATE();
	for(i=0;i<nbGLStates;i++)
	{
		if (glstates[i]->context == ctx)
		{
			long args[] = { POINTER_TO_ARG(dpy), POINTER_TO_ARG(ctx) };
			do_opengl_call_no_lock(glXDestroyContext_func, NULL, args, NULL);
			if (ctx == state->context)
			{
				SET_CURRENT_STATE(NULL);
			}

			GLXContext shareList = glstates[i]->shareList;

			glstates[i]->ref --;
			if (glstates[i]->ref == 0)
			{
				_free_context(dpy, i, glstates[i]);
			}

			if (shareList)
			{
				for(i=0; i<nbGLStates; i++)
				{
					if (glstates[i]->context == shareList)
					{
						glstates[i]->ref--;
						if (glstates[i]->ref == 0)
						{
							_free_context(dpy, i, glstates[i]);
						}
						break;
					}
				}
			}
			break;
		}
	}
	UNLOCK(glXDestroyContext_func);
}

Bool glXQueryVersion( Display *dpy, int *maj, int *min )
{
	LOCK(glXQueryVersion_func);
	static Bool ret = -1;
	static int l_maj, l_min;
	if (ret == -1)
	{
		long args[] = { POINTER_TO_ARG(dpy), POINTER_TO_ARG(&l_maj), POINTER_TO_ARG(&l_min) };
		do_opengl_call_no_lock(glXQueryVersion_func, &ret, args, NULL);
	}
	if (maj) *maj = l_maj;
	if (min) *min = l_min;
	UNLOCK(glXQueryVersion_func);
	return ret;
}


static void _get_window_pos(Display *dpy, Window win, WindowPosStruct* pos)
{
	XWindowAttributes window_attributes_return;
	Window child;
	int x, y;
	Window root = DefaultRootWindow(dpy);
	XGetWindowAttributes(dpy, win, &window_attributes_return);
	XTranslateCoordinates(dpy, win, root, 0, 0, &x, &y, &child);
	/*printf("%d %d %d %d\n", x, y, window_attributes_return.width, window_attributes_return.height);*/
	pos->x = x;
	pos->y = y;
	pos->width = window_attributes_return.width;
	pos->height = window_attributes_return.height;
	pos->map_state = window_attributes_return.map_state;
}

#define MAX_PBUFFERS 100

/* Doit être appelé avec le lock */
static void _move_win_if_necessary(Display *dpy, Window win)
{
	GET_CURRENT_STATE();
	if ((int)win < MAX_PBUFFERS) /* FIXME */
	{
		return;
	}

	WindowPosStruct pos;
	_get_window_pos(dpy, win, &pos);
	if (memcmp(&pos, &state->oldPos, sizeof(state->oldPos)) != 0)
	{
		/* Host Window�� ��Ÿ���� �ʴ´�. if (pos.map_state != state->oldPos.map_state)
		   {
		   long args[] = { INT_TO_ARG(win), INT_TO_ARG(pos.map_state) };
		   do_opengl_call_no_lock(_changeWindowState_func, NULL, args, NULL);
		   }*/
		memcpy(&state->oldPos, &pos, sizeof(state->oldPos));
		long args[] = { INT_TO_ARG(win), POINTER_TO_ARG(&pos) };
		if (getenv("NO_MOVE"))
		{
			pos.x = 0;
			pos.y = 0;
		}
		do_opengl_call_no_lock(_moveResizeWindow_func, NULL, args, NULL);
	}
}

static Bool glXMakeCurrent_no_lock( Display *dpy, GLXDrawable drawable, GLXContext ctx)
{
	Bool ret = False;
	int i;
#if 0
	if (drawable == 0 && ctx == 0)
		return True;
	if (current_drawable == drawable && current_context == ctx) /* optimization */
		return True;
#endif
	GET_CURRENT_STATE();

	if (!(drawable < MAX_PBUFFERS)) /* FIXME */
	{
		for(i=0; i<nbGLStates; i++)
		{
			if (glstates[i]->context == ctx && glstates[i]->viewport.width == 0)
			{
				XWindowAttributes window_attributes_return;
				XGetWindowAttributes(dpy, drawable, &window_attributes_return);
				glstates[i]->viewport.width = window_attributes_return.width;
				glstates[i]->viewport.height = window_attributes_return.height;
				if (debug_gl)
					log_gl("drawable 0x%X dim : %d x %d\n", (int)drawable, window_attributes_return.width,
							window_attributes_return.height);
				break;
			}
		}
	}

	if (getenv("GET_IMG_FROM_SERVER") && ctx != NULL)
	{
		for(i=0; i<nbGLStates; i++)
		{
			if (glstates[i]->context == ctx)
			{
				if (glstates[i]->pbuffer)
				{
					long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(glstates[i]->pbuffer), INT_TO_ARG(ctx) };
					do_opengl_call_no_lock(glXMakeCurrent_func, NULL /*&ret*/, args, NULL);
					ret = True;
				}
				else
				{
					long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(drawable), INT_TO_ARG(ctx) };
					do_opengl_call_no_lock(glXMakeCurrent_func, NULL /*&ret*/, args, NULL);
					ret = True;
					_move_win_if_necessary(dpy, drawable);
				}
				break;
			}
		}
		if (i == nbGLStates)
		{
			log_gl("unknown context %p\n", ctx);
		}
	}
	else
	{
		//log_gl("glXMakeCurrent %d %d\n", drawable, ctx);
		long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(drawable), INT_TO_ARG(ctx) };
		do_opengl_call_no_lock(glXMakeCurrent_func, NULL /*&ret*/, args, NULL);
		ret = True;
		_move_win_if_necessary(dpy, drawable);
	}

	if (ret)
	{
		int i;
		if (ctx == 0)
		{
			state->context = NULL;
		}
		else
		{
			for(i=0; i<nbGLStates; i++)
			{
				if (glstates[i]->context == ctx)
				{
					state = glstates[i];
					SET_CURRENT_STATE(state);
					break;
				}
			}
			if (i == nbGLStates)
			{
				log_gl("cannot set current_gl_state\n");
			}
		}

		state->display = dpy;
		state->context = ctx;
		state->current_drawable = drawable;
		state->current_read_drawable = drawable;
	}
	return ret;
}

GLAPI Bool APIENTRY glXMakeCurrent( Display *dpy, GLXDrawable drawable, GLXContext ctx)
{
	Bool ret;
	LOCK(glXMakeCurrent_func);
	ret = glXMakeCurrent_no_lock(dpy, drawable, ctx);
	UNLOCK(glXMakeCurrent_func);
	return ret;
}

GLAPI void APIENTRY glXCopyContext( Display *dpy, GLXContext src, GLXContext dst, unsigned long mask )
{
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(src), INT_TO_ARG(dst), INT_TO_ARG(mask) };
	do_opengl_call(glXCopyContext_func, NULL, args, NULL);
}

GLAPI Bool APIENTRY glXIsDirect( Display *dpy, GLXContext ctx )
{
	Bool ret = False;
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(ctx) };
	do_opengl_call(glXIsDirect_func, &ret, args, NULL);
	return ret;
}

GLAPI int APIENTRY glXGetConfig( Display *dpy, XVisualInfo *vis,
		int attrib, int *value )
{
	int ret = 0;
	int i, j;
	if (vis == NULL || value == NULL) return 0;
	LOCK(glXGetConfig_func);

	int visualid = 0;
	for(i=0;i<nEltTabAssocVisualInfoVisualId;i++)
	{
		if (vis == tabAssocVisualInfoVisualId[i].vis)
		{
			visualid = tabAssocVisualInfoVisualId[i].visualid;
			if (debug_gl) log_gl("found visualid %d corresponding to vis %p\n", visualid, vis);
			break;
		}
	}
	if (i == nEltTabAssocVisualInfoVisualId)
	{
		if (debug_gl) log_gl("not found vis %p in table\n", vis);
		visualid = vis->visualid;
	}

	/* Optimization */
	for(i=0;i<nbConfigs;i++)
	{
		if (visualid == configs[i].visualid)
		{
			for(j=0;j<configs[i].nbAttribs;j++)
			{
				if (configs[i].attribs[j].attrib == attrib)
				{
					*value = configs[i].attribs[j].value;
					ret = configs[i].attribs[j].ret;
					if (debug_gl) log_gl("glXGetConfig(%s)=%d (%d)\n", _getAttribNameFromValue(attrib), *value, ret);
					goto end_of_glx_get_config;
				}
			}
			break;
		}
	}

	if (i < N_MAX_CONFIGS)
	{
		if (i == nbConfigs)
		{
			configs[i].visualid = visualid;
			configs[i].nbAttribs = 0;
			int tabGottenValues[N_REQUESTED_ATTRIBS];
			int tabGottenRes[N_REQUESTED_ATTRIBS];
			if (debug_gl) log_gl("glXGetConfig_extended visual=%p\n", vis);
			long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(visualid), INT_TO_ARG(N_REQUESTED_ATTRIBS),
				POINTER_TO_ARG(getTabRequestedAttribsInt()), POINTER_TO_ARG(tabGottenValues),
				POINTER_TO_ARG(tabGottenRes) };
			int args_size[] = {0, 0, 0, N_REQUESTED_ATTRIBS*sizeof(int), N_REQUESTED_ATTRIBS*sizeof(int),
				N_REQUESTED_ATTRIBS*sizeof(int) };
			do_opengl_call_no_lock(glXGetConfig_extended_func, NULL, CHECK_ARGS(args, args_size));

			int j;
			int found = 0;
			int  jDblBuffer = -1, jUseGL = -1;
			for(j=0;j<N_REQUESTED_ATTRIBS;j++)
			{
				if (GLX_USE_GL == tabRequestedAttribsPair[j].val)
					jUseGL = j;
				else if (GLX_DOUBLEBUFFER == tabRequestedAttribsPair[j].val)
					jDblBuffer = j;
				configs[i].attribs[j].attrib = tabRequestedAttribsPair[j].val;
				configs[i].attribs[j].value = tabGottenValues[j];
				configs[i].attribs[j].ret = tabGottenRes[j];
				configs[i].nbAttribs++;
				if (tabRequestedAttribsPair[j].val == attrib)
				{
					found = 1;
					*value = configs[i].attribs[j].value;
					ret = configs[i].attribs[j].ret;
					if (debug_gl) log_gl("glXGetConfig(%s)=%d (%d)\n", tabRequestedAttribsPair[j].name, *value, ret);
				}
			}

			if (getenv("DISABLE_DOUBLE_BUFFER"))
			{
				if (configs[i].attribs[jDblBuffer].value == 1)
				{
					if (attrib == GLX_USE_GL)
						*value = 0;
					configs[i].attribs[jUseGL].value = 0;
				}
			}

			nbConfigs++;
			if (found)
				goto end_of_glx_get_config;
		}

		{
			long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(visualid), INT_TO_ARG(attrib), POINTER_TO_ARG(value) };
			do_opengl_call_no_lock(glXGetConfig_func, &ret, args, NULL);
			if (debug_gl) log_gl("glXGetConfig visual=%p, attrib=%d -> %d\n", vis, attrib, *value);
			if (configs[i].nbAttribs < N_MAX_ATTRIBS)
			{
				configs[i].attribs[configs[i].nbAttribs].attrib = attrib;
				configs[i].attribs[configs[i].nbAttribs].value = *value;
				configs[i].attribs[configs[i].nbAttribs].ret = ret;
				configs[i].nbAttribs++;
			}
		}
	}
	else
	{
		long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(visualid), INT_TO_ARG(attrib), POINTER_TO_ARG(value) };
		do_opengl_call_no_lock(glXGetConfig_func, &ret, args, NULL);
		if (debug_gl) log_gl("glXGetConfig visual=%p, attrib=%d -> %d\n", vis, attrib, *value);
	}
end_of_glx_get_config:
	UNLOCK(glXGetConfig_func);
	return ret;
}

#ifndef __CLIENT_WINDOW__
/* Doit être appelé avec le lock */
static void _send_cursor(Display* dpy, Window win)
{
	GET_CURRENT_STATE();
	XFixesCursorImage* cursor = XFixesGetCursorImage(dpy);
	Window child_return, root_return;
	int root_x_return, root_y_return, win_x_return, win_y_return;
	unsigned int mask_return;
	XQueryPointer(dpy, win, &root_return, &child_return, &root_x_return, &root_y_return,
			&win_x_return, &win_y_return, &mask_return);
	cursor->x = win_x_return;
	cursor->y = win_y_return;


	if (cursor->width == state->last_cursor.width &&
			cursor->height == state->last_cursor.height &&
			cursor->xhot == state->last_cursor.xhot &&
			cursor->yhot == state->last_cursor.yhot &&
			memcmp(cursor->pixels, state->last_cursor.pixels, sizeof(long) * cursor->width * cursor->height) == 0)
	{
		if (!(cursor->x == state->last_cursor.x &&
					cursor->y == state->last_cursor.y))
		{
			long args[] = { INT_TO_ARG(cursor->x), INT_TO_ARG(cursor->y),
				INT_TO_ARG(cursor->width), INT_TO_ARG(cursor->height),
				INT_TO_ARG(cursor->xhot), INT_TO_ARG(cursor->yhot),
				0 };
			int args_size[] = { 0, 0, 0, 0, 0, 0, 0 };
			do_opengl_call_no_lock(_send_cursor_func, NULL, CHECK_ARGS(args, args_size));

			state->last_cursor.x = cursor->x;
			state->last_cursor.y = cursor->y;
		}
		XFree(cursor);
		return;
	}
	int* data;

	/* Fun stuff about the 'pixels' field of XFixesCursorImage. It's a long instead of an int */
	/* The interface chosen for serialization is an array of int */
	if (sizeof(long) != sizeof(int))
	{
		data = malloc(sizeof(int) * cursor->width * cursor->height);
		int i;
		for(i=0;i<cursor->width*cursor->height;i++)
		{
			data[i] = (int)cursor->pixels[i];
		}
	}
	else
	{
		data = (int*)cursor->pixels;
	}

	long args[] = { INT_TO_ARG(cursor->x), INT_TO_ARG(cursor->y),
		INT_TO_ARG(cursor->width), INT_TO_ARG(cursor->height),
		INT_TO_ARG(cursor->xhot), INT_TO_ARG(cursor->yhot),
		POINTER_TO_ARG(data) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, sizeof(int) * cursor->width * cursor->height };
	do_opengl_call_no_lock(_send_cursor_func, NULL, CHECK_ARGS(args, args_size));

	void* prev_ptr = state->last_cursor.pixels;
	memcpy(&state->last_cursor, cursor, sizeof(XFixesCursorImage));
	state->last_cursor.pixels = realloc(prev_ptr, sizeof(long) * cursor->width * cursor->height);
	memcpy(state->last_cursor.pixels, cursor->pixels, sizeof(long) * cursor->width * cursor->height);

	if (sizeof(long) != sizeof(int))
	{
		free(data);
	}
	XFree(cursor);
}
#endif

static void glXSwapBuffers_no_lock( Display *dpy, GLXDrawable drawable )
{
	//log_gl("glXSwapBuffers %d\n", drawable);
	int i;
#ifdef __CLIENT_WINDOW__
	WindowImage *image = NULL;
#else
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(drawable) };
	GET_CURRENT_STATE();
#endif

#ifndef __CLIENT_WINDOW__ /*Host window�� draw���� �ʴ´�.*/
	do_opengl_call_no_lock(glXSwapBuffers_func, NULL, args, NULL);
#endif

	if (getenv("GET_IMG_FROM_SERVER") == NULL)
	{
		_move_win_if_necessary(dpy, drawable);
	}

#ifndef __CLIENT_WINDOW__
	_send_cursor(dpy, drawable);

	if (limit_fps > 0)
	{
		if (state->last_swap_buffer_time.tv_sec != 0)
		{
			struct timeval current_time;
			gettimeofday(&current_time, NULL);
			int diff_time = (current_time.tv_sec - state->last_swap_buffer_time.tv_sec) * 1000 + (current_time.tv_usec - state->last_swap_buffer_time.tv_usec) / 1000;

			if (diff_time < 1000 / limit_fps)
			{
				usleep( (1000 / limit_fps - diff_time) * 900);
			}
		}
		gettimeofday(&state->last_swap_buffer_time, NULL);
	} 
#endif

#ifdef __CLIENT_WINDOW__
	for(i = 0; i < MAX_IMAGES; i++)
	{
		if( wImage[i].win == (Window)drawable )
		{
			image = &wImage[i];				
			break;
		}
	}

	if( image && image->win )
		_draw_image(dpy, image->win, image);
#endif

}

GLAPI void APIENTRY glXSwapBuffers( Display *dpy, GLXDrawable drawable )
{
	LOCK(glXSwapBuffers_func);
	glXSwapBuffers_no_lock(dpy, drawable);
	UNLOCK(glXSwapBuffers_func);
}

GLAPI Bool APIENTRY glXQueryExtension( Display *dpy, int *errorBase, int *eventBase )
{
	Bool ret;
	LOCK(glXQueryExtension_func);
	int fake_int;
	if (errorBase == NULL) errorBase = &fake_int;
	if (eventBase == NULL) eventBase = &fake_int;
	long args[] = { POINTER_TO_ARG(dpy), POINTER_TO_ARG(errorBase), POINTER_TO_ARG(eventBase) };
	do_opengl_call_no_lock(glXQueryExtension_func, &ret, args, NULL);
	UNLOCK(glXQueryExtension_func);
	return ret;
}

GLAPI void APIENTRY glXWaitGL (void)
{
	int ret;
	do_opengl_call(glXWaitGL_func, &ret, NULL, NULL);
}

GLAPI void APIENTRY glXWaitX (void)
{
	int ret;
	do_opengl_call(glXWaitX_func, &ret, NULL, NULL);
}

GLAPI Display* APIENTRY glXGetCurrentDisplay( void )
{
	GET_CURRENT_STATE();
	return state->display;
}

static GLXFBConfig* glXChooseFBConfig_no_lock( Display *dpy, int screen,
		const int *attribList, int *nitems )
{
	CHECK_PROC_WITH_RET(glXChooseFBConfig);
	GLXFBConfig* fbConfig = NULL;
	if (debug_gl) log_gl("glXChooseFBConfig\n");
	int i=0;
	int ret = 0;
	int emptyAttribList = None;
	if (attribList == NULL) attribList = &emptyAttribList;
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(screen), POINTER_TO_ARG(attribList), POINTER_TO_ARG(nitems) };
	int args_size[] = { 0, 0, sizeof(int) * _compute_length_of_attrib_list_including_zero(attribList, 1), 0 };
	do_opengl_call_no_lock(glXChooseFBConfig_func, &ret, args, args_size);
	if (debug_gl) log_gl("nitems = %d\n", *nitems);
	if (*nitems == 0)
		return NULL;
	fbConfig = malloc(sizeof(GLXFBConfig) * (*nitems));
	for(i=0;i<*nitems;i++)
	{
		fbConfig[i] = (GLXFBConfig)(long)(ret + i);
		if (debug_gl && (i == 0 || i == *nitems-1)) log_gl("config %d = %d\n", i, ret+i);
	}
	return fbConfig;
}

GLAPI GLXFBConfig* APIENTRY glXChooseFBConfig( Display *dpy, int screen,
		const int *attribList, int *nitems )
{
	GLXFBConfig* fbconfig;
	LOCK(glXChooseFBConfig_func);
	fbconfig = glXChooseFBConfig_no_lock(dpy, screen, attribList, nitems);
	UNLOCK(glXChooseFBConfig_func);
	return fbconfig;
}

GLAPI GLXFBConfigSGIX* APIENTRY glXChooseFBConfigSGIX( Display *dpy, int screen,
		const int *attribList, int *nitems )
{
	CHECK_PROC_WITH_RET(glXChooseFBConfigSGIX);
	GLXFBConfigSGIX* fbConfig = NULL;
	if (debug_gl) log_gl("glXChooseFBConfigSGIX\n");
	int i = 0;
	int ret = 0;
	int emptyAttribList = None;
	if (attribList == NULL) attribList = &emptyAttribList;
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(screen), POINTER_TO_ARG(attribList), POINTER_TO_ARG(nitems) };
	int args_size[] = { 0, 0, sizeof(int) * _compute_length_of_attrib_list_including_zero(attribList, 1), 0 };
	do_opengl_call(glXChooseFBConfigSGIX_func, &ret, args, args_size);
	if (debug_gl) log_gl("nitems = %d\n", *nitems);
	fbConfig = malloc(sizeof(GLXFBConfigSGIX) * (*nitems));
	for(i=0;i<*nitems;i++)
	{
		fbConfig[i] = (GLXFBConfig)(long)(ret + i);
		if (debug_gl && (i == 0 || i == *nitems-1)) log_gl("config %d = %d\n", i, ret+i);
	}
	return fbConfig;
}

GLAPI GLXFBConfig* APIENTRY glXGetFBConfigs( Display *dpy, int screen, int *nitems )
{
	CHECK_PROC_WITH_RET(glXGetFBConfigs);
	if (debug_gl) log_gl("glXGetFBConfigs\n");
	int i = 0;
	GLXFBConfig* fbConfig;
	int ret = 0;
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(screen), POINTER_TO_ARG(nitems) };
	do_opengl_call(glXGetFBConfigs_func, &ret, args, NULL);
	if (debug_gl) log_gl("nitems = %d\n", *nitems);
	fbConfig = malloc(sizeof(GLXFBConfig) * (*nitems));
	for(i=0;i<*nitems;i++)
	{
		fbConfig[i] = (GLXFBConfig)(long)(ret + i);
		if (debug_gl && (i == 0 || i == *nitems-1)) log_gl("config %d = %d\n", i, ret+i);
	}
	return fbConfig;
}


GLAPI int APIENTRY glXGetFBConfigAttrib(Display *dpy, GLXFBConfig config, int attrib, int *value)
{
	CHECK_PROC_WITH_RET(glXGetFBConfigAttrib);
	LOCK(glXGetFBConfigAttrib_func);
	int ret = 0;
	int i, j;

	/* Optimization */
	for(i=0;i<nbFBConfigs;i++)
	{
		if (config == fbconfigs[i].config)
		{
			for(j=0;j<fbconfigs[i].nbAttribs;j++)
			{
				if (fbconfigs[i].attribs[j].attrib == attrib)
				{
					*value = fbconfigs[i].attribs[j].value;
					ret = fbconfigs[i].attribs[j].ret;
					if (debug_gl)
					{
						log_gl("glXGetFBConfigAttrib(config=%p,%s)=%d (%d)\n", config,
								_getAttribNameFromValue(attrib), *value, ret);
					}
					goto end_of_glx_get_fb_config_attrib;
				}
			}
			break;
		}
	}

	if (i < N_MAX_CONFIGS)
	{
		if (i == nbFBConfigs)
		{
			fbconfigs[i].config = config;
			fbconfigs[i].nbAttribs = 0;
			int tabGottenValues[N_REQUESTED_ATTRIBS];
			int tabGottenRes[N_REQUESTED_ATTRIBS];
			if (debug_gl) log_gl("glXGetFBConfigAttrib_extended config=%p\n", config);
			long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(config), INT_TO_ARG(N_REQUESTED_ATTRIBS),
				POINTER_TO_ARG(getTabRequestedAttribsInt()), POINTER_TO_ARG(tabGottenValues),
				POINTER_TO_ARG(tabGottenRes) };
			int args_size[] = {0, 0, 0, N_REQUESTED_ATTRIBS*sizeof(int), N_REQUESTED_ATTRIBS*sizeof(int),
				N_REQUESTED_ATTRIBS*sizeof(int) };
			do_opengl_call_no_lock(glXGetFBConfigAttrib_extended_func, NULL, CHECK_ARGS(args, args_size));

			int j;
			int found = 0;
			for(j=0;j<N_REQUESTED_ATTRIBS;j++)
			{
				fbconfigs[i].attribs[j].attrib = tabRequestedAttribsPair[j].val;
				fbconfigs[i].attribs[j].value = tabGottenValues[j];
				fbconfigs[i].attribs[j].ret = tabGottenRes[j];
				fbconfigs[i].nbAttribs++;
				if (tabRequestedAttribsPair[j].val == attrib)
				{
					found = 1;
					*value = fbconfigs[i].attribs[j].value;
					ret = fbconfigs[i].attribs[j].ret;
					if (debug_gl) log_gl("glXGetFBConfigAttrib(config=%p, %s)=%d (%d)\n",
							config, tabRequestedAttribsPair[j].name, *value, ret);
				}
			}
			nbFBConfigs++;
			if (found)
				goto end_of_glx_get_fb_config_attrib;
		}

		{
			long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(config), INT_TO_ARG(attrib), POINTER_TO_ARG(value) };
			do_opengl_call_no_lock(glXGetFBConfigAttrib_func, &ret, args, NULL);
			if (debug_gl) log_gl("glXGetFBConfigAttrib config=%p, attrib=%d -> %d\n", config, attrib, *value);
			if (fbconfigs[i].nbAttribs < N_MAX_ATTRIBS)
			{
				fbconfigs[i].attribs[fbconfigs[i].nbAttribs].attrib = attrib;
				fbconfigs[i].attribs[fbconfigs[i].nbAttribs].value = *value;
				fbconfigs[i].attribs[fbconfigs[i].nbAttribs].ret = ret;
				fbconfigs[i].nbAttribs++;
			}
		}
	}
	else
	{
		long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(config), INT_TO_ARG(attrib), POINTER_TO_ARG(value) };
		do_opengl_call_no_lock(glXGetFBConfigAttrib_func, &ret, args, NULL);
		if (debug_gl) log_gl("glXGetFBConfigAttrib config=%p, attrib=%d -> %d\n", config, attrib, *value);
	}
end_of_glx_get_fb_config_attrib:
	UNLOCK(glXGetFBConfigAttrib_func);
	return ret;
}

GLAPI int APIENTRY glXGetFBConfigAttribSGIX(Display *dpy, GLXFBConfigSGIX config, int attribute, int *value)
{
	CHECK_PROC_WITH_RET(glXGetFBConfigAttribSGIX);
	int ret = 0;
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(config), INT_TO_ARG(attribute), POINTER_TO_ARG(value) };
	do_opengl_call(glXGetFBConfigAttribSGIX_func, &ret, args, NULL);
	if (debug_gl)
	{
		if (attribute < 0x20)
			log_gl("glXGetFBConfigAttribSGIX %p %d = %d\n", (void*)config, attribute, *value);
		else
			log_gl("glXGetFBConfigAttribSGIX %p 0x%X = %d\n", (void*)config, attribute, *value);
	}
	return ret;
}

GLAPI int APIENTRY glXQueryContext( Display *dpy, GLXContext ctx, int attribute, int *value )
{
	CHECK_PROC_WITH_RET(glXQueryContext);
	int ret = 0;
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(ctx), INT_TO_ARG(attribute), POINTER_TO_ARG(value) };
	do_opengl_call(glXQueryContext_func, &ret, args, NULL);
	return ret;
}

GLAPI void APIENTRY glXQueryDrawable( Display *dpy, GLXDrawable draw, int attribute, unsigned int *value )
{
	CHECK_PROC(glXQueryDrawable);
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(draw), INT_TO_ARG(attribute), POINTER_TO_ARG(value) };
	do_opengl_call(glXQueryDrawable_func, NULL, args, NULL);
}

GLAPI int APIENTRY glXQueryGLXPbufferSGIX( Display *dpy, GLXPbufferSGIX pbuf, int attribute, unsigned int *value )
{
	CHECK_PROC_WITH_RET(glXQueryGLXPbufferSGIX);
	int ret = 0;
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(pbuf), INT_TO_ARG(attribute), POINTER_TO_ARG(value) };
	do_opengl_call(glXQueryGLXPbufferSGIX_func, &ret, args, NULL);
	return ret;
}

static GLXPbuffer glXCreatePbuffer_no_lock(Display *dpy,
		GLXFBConfig config,
		const int *attribList)
{
	CHECK_PROC_WITH_RET(glXCreatePbuffer);
	if (debug_gl) log_gl("glXCreatePbuffer %p\n", (void*)config);

	GLXPbuffer pbuffer;
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(config), POINTER_TO_ARG(attribList) };
	int args_size[] = { 0, 0, sizeof(int) * _compute_length_of_attrib_list_including_zero(attribList, 1)};
	do_opengl_call_no_lock(glXCreatePbuffer_func, &pbuffer, args, args_size);

	return pbuffer;
}

GLAPI GLXPbuffer APIENTRY glXCreatePbuffer(Display *dpy,
		GLXFBConfig config,
		const int *attribList)
{
	GLXPbuffer pbuffer;
	LOCK(glXCreatePbuffer_func);
	pbuffer = glXCreatePbuffer_no_lock(dpy, config, attribList);
	UNLOCK(glXCreatePbuffer_func);
	return pbuffer;
}

GLAPI GLXPbufferSGIX APIENTRY glXCreateGLXPbufferSGIX( Display *dpy,
		GLXFBConfigSGIX config,
		unsigned int width,
		unsigned int height,
		int *attribList )
{
	CHECK_PROC_WITH_RET(glXCreateGLXPbufferSGIX);
	if (debug_gl) log_gl("glXCreateGLXPbufferSGIX %p\n", (void*)config);

	GLXPbufferSGIX pbuffer;
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(config), INT_TO_ARG(width), INT_TO_ARG(height), POINTER_TO_ARG(attribList) };
	int args_size[] = { 0, 0, 0, 0, sizeof(int) * _compute_length_of_attrib_list_including_zero(attribList, 1)};
	do_opengl_call(glXCreateGLXPbufferSGIX_func, &pbuffer, args, args_size);

	return pbuffer;
}

void glXBindTexImageATI(Display *dpy, GLXPbuffer pbuffer, int buffer)
{
	CHECK_PROC(glXBindTexImageATI);
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(pbuffer), INT_TO_ARG(buffer) };
	do_opengl_call(glXBindTexImageATI_func, NULL, args, NULL);
}

void glXReleaseTexImageATI(Display *dpy, GLXPbuffer pbuffer, int buffer)
{
	CHECK_PROC(glXReleaseTexImageATI);
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(pbuffer), INT_TO_ARG(buffer) };
	do_opengl_call(glXReleaseTexImageATI_func, NULL, args, NULL);
}

Bool glXBindTexImageARB(Display *dpy, GLXPbuffer pbuffer, int buffer)
{
	Bool ret = 0;
	CHECK_PROC_WITH_RET(glXBindTexImageARB);
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(pbuffer), INT_TO_ARG(buffer) };
	do_opengl_call(glXBindTexImageARB_func, &ret, args, NULL);
	return ret;
}

Bool glXReleaseTexImageARB(Display *dpy, GLXPbuffer pbuffer, int buffer)
{
	Bool ret = 0;
	CHECK_PROC_WITH_RET(glXReleaseTexImageARB);
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(pbuffer), INT_TO_ARG(buffer) };
	do_opengl_call(glXReleaseTexImageARB_func, &ret, args, NULL);
	return ret;
}

GLAPI void APIENTRY glXDestroyPbuffer(Display* dpy, GLXPbuffer pbuffer)
{
	CHECK_PROC(glXDestroyPbuffer);
	if (debug_gl) log_gl("glXDestroyPbuffer %d\n", (int)pbuffer);

	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(pbuffer) };
	do_opengl_call(glXDestroyPbuffer_func, NULL, args, NULL);
}

GLAPI void APIENTRY glXDestroyGLXPbufferSGIX(Display* dpy, GLXPbufferSGIX pbuffer)
{
	CHECK_PROC(glXDestroyGLXPbufferSGIX);
	if (debug_gl) log_gl("glXDestroyGLXPbufferSGIX %d\n", (int)pbuffer);

	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(pbuffer) };
	do_opengl_call(glXDestroyGLXPbufferSGIX_func, NULL, args, NULL);
}

static XVisualInfo* glXGetVisualFromFBConfig_no_lock( Display *dpy, GLXFBConfig config )
{
	CHECK_PROC_WITH_RET(glXGetVisualFromFBConfig);
	int screen = 0;

	if (debug_gl) log_gl("glXGetVisualFromFBConfig %p\n", (void*)config);

	XVisualInfo temp, *vis;
	long mask;
	int n;
	int i;

	mask = VisualScreenMask | VisualDepthMask | VisualClassMask;
	temp.screen = screen;
	temp.depth = DefaultDepth(dpy,screen);
	temp.class = DefaultVisual(dpy,screen)->class;
	temp.visualid = DefaultVisual(dpy,screen)->visualid;
	mask |= VisualIDMask;

	vis = XGetVisualInfo( dpy, mask, &temp, &n );

	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(config)};
	int visualid;
	do_opengl_call_no_lock(glXGetVisualFromFBConfig_func, &visualid, args, NULL);

	/*host vid�� tabAssocVisualInfoVisualId[]�� �����Ͽ� ��������� guest vi�� ����� �ʿ� ����*/ 
	/*vis->visualid = visualid;*/

	assert (nEltTabAssocVisualInfoVisualId < MAX_SIZE_TAB_ASSOC_VISUALINFO_VISUALID);
	for(i=0;i<nEltTabAssocVisualInfoVisualId;i++)
	{
		if (tabAssocVisualInfoVisualId[i].vis == vis) break;
	}
	if (i == nEltTabAssocVisualInfoVisualId)
		nEltTabAssocVisualInfoVisualId++;
	tabAssocVisualInfoVisualId[i].vis = vis;
	tabAssocVisualInfoVisualId[i].fbconfig = config;
	tabAssocVisualInfoVisualId[i].visualid = visualid;

	if (debug_gl) log_gl("glXGetVisualFromFBConfig returning vis %p (visualid=%d, 0x%X)\n", vis, visualid, visualid);

	return vis;
}

GLAPI XVisualInfo* APIENTRY glXGetVisualFromFBConfig( Display *dpy, GLXFBConfig config )
{
	XVisualInfo* vis;
	LOCK(glXGetVisualFromFBConfig_func);
	vis = glXGetVisualFromFBConfig_no_lock(dpy, config);
	UNLOCK(glXGetVisualFromFBConfig_func);
	return vis;
}

GLAPI GLXContext APIENTRY glXCreateNewContext(Display * dpy,
		GLXFBConfig  fbconfig,
		int  renderType,
		GLXContext  shareList,
		Bool  direct)
{
	CHECK_PROC_WITH_RET(glXCreateNewContext);
	LOCK(glXCreateNewContext_func);
	if (debug_gl) log_gl("glXCreateNewContext %p\n", (void*)fbconfig);

	GLXContext ctxt;
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(fbconfig), INT_TO_ARG(renderType), INT_TO_ARG(shareList),
		INT_TO_ARG(direct) };
	do_opengl_call_no_lock(glXCreateNewContext_func, &ctxt, args, NULL);
	if (ctxt)
	{
		_create_context(ctxt, shareList);
		if (getenv("GET_IMG_FROM_SERVER"))
		{
			int pbufAttrib[] = {
				GLX_PBUFFER_WIDTH,   1024,
				GLX_PBUFFER_HEIGHT,  1024,
				GLX_LARGEST_PBUFFER, GL_TRUE,
				None
			};
			glstates[nbGLStates-1]->pbuffer = glXCreatePbuffer(dpy, fbconfig, pbufAttrib);
			assert(glstates[nbGLStates-1]->pbuffer);
		}
	}
	UNLOCK(glXCreateNewContext_func);
	return ctxt;
}

GLAPI Bool APIENTRY glXMakeContextCurrent( Display *dpy, GLXDrawable draw,
		GLXDrawable read, GLXContext ctx )
{
	Bool ret;
	GET_CURRENT_STATE();
	if (draw != read)
	{
		static int first_time = 1;
		if (first_time)
		{
			first_time = 0;
			log_gl("using glXMakeCurrent instead of real glXMakeContextCurrent... may help some program work...\n");
		}
	}
	ret = glXMakeCurrent(dpy, draw, ctx);
	if (ret)
		state->current_read_drawable = read;
	return ret;
}

GLAPI GLXContext APIENTRY glXCreateContextWithConfigSGIX( Display *dpy,
		GLXFBConfigSGIX config,
		int renderType,
		GLXContext shareList,
		Bool direct )
{
	CHECK_PROC_WITH_RET(glXCreateContextWithConfigSGIX);
	if (debug_gl) log_gl("glXCreateContextWithConfigSGIX %p\n", (void*)config);

	GLXContext ctxt;
	long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(config), INT_TO_ARG(renderType), INT_TO_ARG(shareList),
		INT_TO_ARG(direct) };
	do_opengl_call(glXCreateContextWithConfigSGIX_func, &ctxt, args, NULL);
	return ctxt;
}

GLAPI GLXWindow APIENTRY glXCreateWindow( Display *dpy, GLXFBConfig config, Window win, const int *attribList )
{
	CHECK_PROC_WITH_RET(glXCreateWindow);
	/* do nothing. Not sure about this implementation. FIXME */

#ifdef __CLIENT_WINDOW__
	int i;
	WindowImage *image = NULL;

	for(i = 0; i < MAX_IMAGES; i++)
	{  
		if( wImage[i].win == 0 && image == NULL )
		{
			image = &wImage[i];				
			break;
		}
		else if( wImage[i].win == win )
		{
			image = &wImage[i];
			return (GLXWindow)win;
		}
	}

	if( image )
	{
		image->win_gc = XCreateGC(dpy, win, 0, NULL);
		if( image->win_gc == NULL )
		{
			log_gl("Winodow GC Create Fail \n");
			return (GLXWindow)win;
		}

		if( _create_image(dpy, win, image) == False )
		{
			XFreeGC(dpy, image->win_gc);
			image->win_gc = NULL;
			log_gl("Window Image Create Fail \n");
			return (GLXWindow)win;
		}
	}
#endif

	return (GLXWindow)win;
}

GLAPI void APIENTRY glXDestroyWindow( Display *dpy, GLXWindow window )
{
	CHECK_PROC(glXDestroyWindow);
	/* Destroy Sub-Window of Host OS */

#ifdef __CLIENT_WINDOW__
	int i;
	WindowImage *image = NULL;

	if (debug_gl) log_gl("glXDestroyWindow %d\n", (int)window);

	for(i = 0; i < MAX_IMAGES; i++)
	{
		if( wImage[i].win == (Window)window )
		{
			image = &wImage[i];				
			break;
		}
	}

	if( image && image->win)
	{
		_destroy_image(dpy, image);

		XFreeGC(dpy, image->win_gc);
		image->win_gc = NULL;
	}
#endif

	if ( window ) {
		long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(window) };
		do_opengl_call(glXDestroyWindow_func, NULL, args, NULL);
	}
}

GLAPI GLXPixmap APIENTRY glXCreateGLXPixmap( Display *dpy,
		XVisualInfo *vis,
		Pixmap pixmap )
{
	CHECK_PROC_WITH_RET(glXCreateGLXPixmap);
	/* FIXME */
	log_gl("glXCreateGLXPixmap : sorry, unsupported call and I don't really see how I could implement it...");
	return 0;
}

GLAPI void APIENTRY glXDestroyGLXPixmap( Display *dpy, GLXPixmap pixmap )
{
	CHECK_PROC(glXDestroyGLXPixmap);
	/* FIXME */
	log_gl("glXDestroyGLXPixmap : sorry, unsupported call and I don't really see how I could implement it...");
}

GLAPI GLXPixmap APIENTRY glXCreatePixmap( Display *dpy, GLXFBConfig config,
		Pixmap pixmap, const int *attribList )
{
	CHECK_PROC_WITH_RET(glXCreatePixmap);
	/* FIXME */
	log_gl("glXCreatePixmap : sorry, unsupported call and I don't really see how I could implement it...");
	return 0;
}

GLAPI void APIENTRY glXDestroyPixmap( Display *dpy, GLXPixmap pixmap )
{
	CHECK_PROC(glXDestroyPixmap);
	/* FIXME */
	log_gl("glXDestroyPixmap : sorry, unsupported call and I don't really see how I could implement it...");
}

GLAPI GLXDrawable APIENTRY glXGetCurrentReadDrawable( void )
{
	CHECK_PROC_WITH_RET(glXGetCurrentReadDrawable);
	GET_CURRENT_STATE();
	return state->current_read_drawable;
}

GLAPI void APIENTRY glXSelectEvent( Display *dpy, GLXDrawable drawable,
		unsigned long mask )
{
	CHECK_PROC(glXSelectEvent);
	log_gl("glXSelectEvent : sorry, unsupported call");
}

GLAPI void APIENTRY glXGetSelectedEvent( Display *dpy, GLXDrawable drawable,
		unsigned long *mask )
{
	CHECK_PROC(glXGetSelectedEvent);
	log_gl("glXGetSelectedEvent : sorry, unsupported call");
}


#include "opengl_client_xfonts.c"

GLAPI const char * APIENTRY EXT_FUNC(glXGetScreenDriver) (Display *dpy, int screen)
{
	static const char* ret = NULL;
	LOCK(glXGetScreenDriver_func);
	CHECK_PROC_WITH_RET(glXGetScreenDriver);
	if (ret == NULL)
	{
		long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(screen) };
		do_opengl_call_no_lock(glXGetScreenDriver_func, &ret, args, NULL);
		ret = strdup(ret);
	}
	UNLOCK(glXGetScreenDriver_func);
	return ret;
}

GLAPI const char * APIENTRY EXT_FUNC(glXGetDriverConfig) (const char *drivername)
{
	static const char* ret = NULL;
	CHECK_PROC_WITH_RET(glXGetDriverConfig);
	long args[] = { POINTER_TO_ARG(drivername) };
	if (ret) free((void*)ret);
	do_opengl_call(glXGetDriverConfig_func, &ret, args, NULL);
	ret = strdup(ret);
	return ret;
}

/* For googleearth */
static int counterSync = 0;

GLAPI int APIENTRY EXT_FUNC(glXWaitVideoSyncSGI) ( int divisor, int remainder, unsigned int *count )
{
	CHECK_PROC_WITH_RET(glXWaitVideoSyncSGI);
	//log_gl("glXWaitVideoSyncSGI %d %d\n", divisor, remainder);
	*count = counterSync++; // FIXME ?
	return 0;
}

GLAPI int APIENTRY EXT_FUNC(glXGetVideoSyncSGI)( unsigned int *count )
{
	CHECK_PROC_WITH_RET(glXGetVideoSyncSGI);
	//log_gl("glXGetVideoSyncSGI\n");
	*count = counterSync++; // FIXME ?
	return 0;
}

GLAPI int APIENTRY EXT_FUNC(glXSwapIntervalSGI) ( int interval )
{
	CHECK_PROC_WITH_RET(glXSwapIntervalSGI);
	long args[] = { INT_TO_ARG(interval) };
	int ret = 0;
	do_opengl_call(glXSwapIntervalSGI_func, &ret, args, NULL);
	//log_gl("glXSwapIntervalSGI(%d) = %d\n", interval, ret);
	return ret;
}

#endif

GLAPI const GLubyte * APIENTRY glGetString( GLenum name )
{
	int i;
	static GLubyte* glStrings[6] = {NULL};
	static const char* glGetStringsName[] = {
		"GL_VENDOR",
		"GL_RENDERER",
		"GL_VERSION",
		"GL_EXTENSIONS",
		"GL_SHADING_LANGUAGE_VERSION",
	};

	if (name >= GL_VENDOR && name <= GL_EXTENSIONS)
		i = name - GL_VENDOR;
	else if (name == GL_SHADING_LANGUAGE_VERSION)
		i = 4;
	else if (name == GL_PROGRAM_ERROR_STRING_NV)
		i = 5;
	else
	{
		log_gl("assert(name >= GL_VENDOR && name <= GL_EXTENSIONS || name == GL_SHADING_LANGUAGE_VERSION  || name == GL_PROGRAM_ERROR_STRING_NV)\n");
		return NULL;
	}
	LOCK(glGetString_func);
	if (glStrings[i] == NULL)
	{
		if (i <= 4 && getenv(glGetStringsName[i]))
		{
			glStrings[i] = getenv(glGetStringsName[i]);
		}
		else
		{
			long args[] = { INT_TO_ARG(name) };
			do_opengl_call_no_lock(glGetString_func, &glStrings[i], args, NULL);
		}

		log_gl("glGetString(0x%X) = %s\n", name, glStrings[i]);
		glStrings[name - GL_VENDOR] = strdup((char*)glStrings[i]);
		if (name == GL_EXTENSIONS)
		{
			removeUnwantedExtensions(glStrings[i]);
		}
	}
	UNLOCK(glGetString_func);
	return glStrings[i];
}

#define CASE_GL_PIXEL_MAP(x) case GL_PIXEL_MAP_##x: glGetIntegerv_no_lock(CONCAT(GL_PIXEL_MAP_##x,_SIZE), &value); return value;

static int get_glgetpixelmapv_size(int map)
{
	int value;
	switch (map)
	{
		LOCK(glGetIntegerv_func);
		CASE_GL_PIXEL_MAP(I_TO_I);
		CASE_GL_PIXEL_MAP(S_TO_S);
		CASE_GL_PIXEL_MAP(I_TO_R);
		CASE_GL_PIXEL_MAP(I_TO_G);
		CASE_GL_PIXEL_MAP(I_TO_B);
		CASE_GL_PIXEL_MAP(I_TO_A);
		CASE_GL_PIXEL_MAP(R_TO_R);
		CASE_GL_PIXEL_MAP(G_TO_G);
		CASE_GL_PIXEL_MAP(B_TO_B);
		CASE_GL_PIXEL_MAP(A_TO_A);
		UNLOCK(glGetIntegerv_func);
		default :
		{
			log_gl("unhandled map = %d\n", map);
			return 0;
		}
	}
}

GLAPI void APIENTRY glGetPixelMapfv( GLenum map, GLfloat *values )
{
	long args[] = { INT_TO_ARG(map), POINTER_TO_ARG(values) };
	int args_size[] = { 0, get_glgetpixelmapv_size(map) * sizeof(float) };
	if (args_size[1] == 0) return;
	do_opengl_call(glGetPixelMapfv_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glGetPixelMapuiv( GLenum map, GLuint *values )
{
	long args[] = { INT_TO_ARG(map), POINTER_TO_ARG(values) };
	int args_size[] = { 0, get_glgetpixelmapv_size(map) * sizeof(int) };
	if (args_size[1] == 0) return;
	do_opengl_call(glGetPixelMapuiv_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glGetPixelMapusv( GLenum map, GLushort *values )
{
	long args[] = { INT_TO_ARG(map), POINTER_TO_ARG(values) };
	int args_size[] = { 0, get_glgetpixelmapv_size(map) * sizeof(short) };
	if (args_size[1] == 0) return;
	do_opengl_call(glGetPixelMapusv_func, NULL, CHECK_ARGS(args, args_size));
}

static int glMap1_get_multiplier(GLenum target)
{
	switch (target)
	{
		case GL_MAP1_VERTEX_3:
		case GL_MAP1_NORMAL:
		case GL_MAP1_TEXTURE_COORD_3:
			return 3;
			break;

		case GL_MAP1_VERTEX_4:
		case GL_MAP1_COLOR_4:
		case GL_MAP1_TEXTURE_COORD_4:
			return 4;
			break;

		case GL_MAP1_INDEX:
		case GL_MAP1_TEXTURE_COORD_1:
			return 1;
			break;

		case GL_MAP1_TEXTURE_COORD_2:
			return 2;
			break;

		default:
			if (target >= GL_MAP1_VERTEX_ATTRIB0_4_NV && target <= GL_MAP1_VERTEX_ATTRIB15_4_NV)
				return 4;
			log_gl("unhandled target = %d\n", target);
			return 0;
	}
}


static int glMap2_get_multiplier(GLenum target)
{
	switch (target)
	{
		case GL_MAP2_VERTEX_3:
		case GL_MAP2_NORMAL:
		case GL_MAP2_TEXTURE_COORD_3:
			return 3;
			break;

		case GL_MAP2_VERTEX_4:
		case GL_MAP2_COLOR_4:
		case GL_MAP2_TEXTURE_COORD_4:
			return 4;
			break;

		case GL_MAP2_INDEX:
		case GL_MAP2_TEXTURE_COORD_1:
			return 1;
			break;

		case GL_MAP2_TEXTURE_COORD_2:
			return 2;
			break;

		default:
			if (target >= GL_MAP2_VERTEX_ATTRIB0_4_NV && target <= GL_MAP2_VERTEX_ATTRIB15_4_NV)
				return 4;
			log_gl("unhandled target = %d\n", target);
			return 0;
	}
}

static int get_dimensionnal_evaluator(GLenum target)
{
	switch(target)
	{
		case GL_MAP1_COLOR_4:
		case GL_MAP1_INDEX:
		case GL_MAP1_NORMAL:
		case GL_MAP1_TEXTURE_COORD_1:
		case GL_MAP1_TEXTURE_COORD_2:
		case GL_MAP1_TEXTURE_COORD_3:
		case GL_MAP1_TEXTURE_COORD_4:
		case GL_MAP1_VERTEX_3:
		case GL_MAP1_VERTEX_4:
			return 1;

		case GL_MAP2_COLOR_4:
		case GL_MAP2_INDEX:
		case GL_MAP2_NORMAL:
		case GL_MAP2_TEXTURE_COORD_1:
		case GL_MAP2_TEXTURE_COORD_2:
		case GL_MAP2_TEXTURE_COORD_3:
		case GL_MAP2_TEXTURE_COORD_4:
		case GL_MAP2_VERTEX_3:
		case GL_MAP2_VERTEX_4:
			return 2;

		default:
			log_gl("unhandled target %d\n", target);
			return 0;
	}
}

GLAPI void APIENTRY glMap1f( GLenum target,
		GLfloat u1,
		GLfloat u2,
		GLint stride,
		GLint order,
		const GLfloat *points )
{
	long args[] = { INT_TO_ARG(target), FLOAT_TO_ARG(u1), FLOAT_TO_ARG(u2),
		INT_TO_ARG(stride), INT_TO_ARG(order), POINTER_TO_ARG(points) };
	int args_size[] = { 0, 0, 0, 0, 0, 0 };
	int num_points = order;
	int multiplier = glMap1_get_multiplier(target);
	if (multiplier)
	{
		num_points *= multiplier;
		args_size[5] = num_points * sizeof(float);
		do_opengl_call(glMap1f_func, NULL, CHECK_ARGS(args, args_size));
	}
}

GLAPI void APIENTRY glMap1d( GLenum target,
		GLdouble u1,
		GLdouble u2,
		GLint stride,
		GLint order,
		const GLdouble *points )
{
	long args[] = { INT_TO_ARG(target), DOUBLE_TO_ARG(u1), DOUBLE_TO_ARG(u2),
		INT_TO_ARG(stride), INT_TO_ARG(order), POINTER_TO_ARG(points) };
	int args_size[] = { 0, 0, 0, 0, 0, 0 };
	int num_points = order;
	int multiplier = glMap1_get_multiplier(target);
	if (multiplier)
	{
		num_points *= multiplier;
		args_size[5] = num_points * sizeof(double);
		do_opengl_call(glMap1d_func, NULL,  CHECK_ARGS(args, args_size));
	}
}

GLAPI void APIENTRY glMap2f( GLenum target,
		GLfloat u1,
		GLfloat u2,
		GLint ustride,
		GLint uorder,
		GLfloat v1,
		GLfloat v2,
		GLint vstride,
		GLint vorder,
		const GLfloat *points )
{
	long args[] = { INT_TO_ARG(target),
		FLOAT_TO_ARG(u1), FLOAT_TO_ARG(u2),
		INT_TO_ARG(ustride), INT_TO_ARG(uorder),
		FLOAT_TO_ARG(v1), FLOAT_TO_ARG(v2),
		INT_TO_ARG(vstride), INT_TO_ARG(vorder),
		POINTER_TO_ARG(points) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int num_points = uorder * vorder;
	int multiplier = glMap2_get_multiplier(target);
	if (multiplier)
	{
		num_points *= multiplier;
		args_size[9] = num_points * sizeof(float);
		do_opengl_call(glMap2f_func, NULL, CHECK_ARGS(args, args_size));
	}
}


GLAPI void APIENTRY glMap2d( GLenum target,
		GLdouble u1,
		GLdouble u2,
		GLint ustride,
		GLint uorder,
		GLdouble v1,
		GLdouble v2,
		GLint vstride,
		GLint vorder,
		const GLdouble *points )
{
	long args[] = { INT_TO_ARG(target),
		DOUBLE_TO_ARG(u1), DOUBLE_TO_ARG(u2),
		INT_TO_ARG(ustride), INT_TO_ARG(uorder),
		DOUBLE_TO_ARG(v1), DOUBLE_TO_ARG(v2),
		INT_TO_ARG(vstride), INT_TO_ARG(vorder),
		POINTER_TO_ARG(points) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	int num_points = uorder * vorder;
	int multiplier = glMap2_get_multiplier(target);
	if (multiplier)
	{
		num_points *= multiplier;
		args_size[9] = num_points * sizeof(double);
		do_opengl_call(glMap2d_func, NULL, CHECK_ARGS(args, args_size));
	}
}

static int _glGetMapv_get_n_components( GLenum target, GLenum query)
{
	int dim = get_dimensionnal_evaluator(target);
	if (query == GL_COEFF)
	{
		int orders[2] = { 1, 1 };
		glGetMapiv(target, GL_ORDER, orders);
		return orders[0] * orders[1] * ((dim == 1) ? glMap1_get_multiplier(target) : glMap2_get_multiplier(target));
	}
	else if (query == GL_ORDER)
	{
		return dim;
	}
	else if (query == GL_DOMAIN)
	{
		return 2 * dim;
	}
	else
		return 0;
}


GLAPI void APIENTRY glGetMapdv( GLenum target, GLenum query, GLdouble *v )
{
	int dim = get_dimensionnal_evaluator(target);
	if (dim == 0) return;
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(query), POINTER_TO_ARG(v) };
	int args_size[] = { 0, 0, _glGetMapv_get_n_components(target, query) * sizeof(double) };
	do_opengl_call(glGetMapdv_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glGetMapfv( GLenum target, GLenum query, GLfloat *v )
{
	int dim = get_dimensionnal_evaluator(target);
	if (dim == 0) return;
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(query), POINTER_TO_ARG(v) };
	int args_size[] = { 0, 0, _glGetMapv_get_n_components(target, query) * sizeof(float) };
	do_opengl_call(glGetMapfv_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glGetMapiv( GLenum target, GLenum query, GLint *v )
{
	int dim = get_dimensionnal_evaluator(target);
	if (dim == 0) return;
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(query), POINTER_TO_ARG(v) };
	int args_size[] = { 0, 0, _glGetMapv_get_n_components(target, query) * sizeof(int) };
	do_opengl_call(glGetMapiv_func, NULL, CHECK_ARGS(args, args_size));
}



GLAPI void APIENTRY glBindTexture(GLenum target, GLuint texture)
{
	CHECK_PROC(glBindTexture);
	GET_CURRENT_STATE();
	alloc_value(state->textureAllocator, texture);
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(texture) };
	if (target == GL_TEXTURE_2D)
	{
		state->current_server_state.bindTexture2D = texture;
	}
	else if (target == GL_TEXTURE_RECTANGLE_ARB)
	{
		state->current_server_state.bindTextureRectangle = texture;
	}
	do_opengl_call(glBindTexture_func, NULL, args, NULL);
}

GLAPI void APIENTRY EXT_FUNC(glBindTextureEXT) (GLenum target, GLuint texture)
{
	glBindTexture(target, texture);
}


GLAPI void APIENTRY glGenTextures( GLsizei n, GLuint *textures )
{
	CHECK_PROC(glGenTextures);
	GET_CURRENT_STATE();
	if (n <= 0) { log_gl("n <= 0\n"); return; }
	alloc_range(state->textureAllocator, n, textures);
	long args[] = { n };
	do_opengl_call(glGenTextures_fake_func, NULL, args, NULL);
}

GLAPI void APIENTRY glGenTexturesEXT( GLsizei n, GLuint *textures )
{
	glGenTextures(n, textures);
}

GLAPI void APIENTRY glDeleteTextures ( GLsizei n, const GLuint *textures )
{
	CHECK_PROC(glDeleteTextures);
	GET_CURRENT_STATE();
	if (n <= 0) { log_gl("n <= 0\n"); return; }
	delete_range(state->textureAllocator, n, textures);
	long args[] = { INT_TO_ARG(n), POINTER_TO_ARG(textures) };
	do_opengl_call(glDeleteTextures_func, NULL, args, NULL);
}

GLAPI void APIENTRY glDeleteTexturesEXT ( GLsizei n, const GLuint *textures )
{
	glDeleteTextures(n, textures);
}

static int getTexImageTypeSizeSimple(int format, int type)
{
	switch (type)
	{
		case GL_UNSIGNED_BYTE:
		case GL_BYTE:
			return 1;

		case GL_UNSIGNED_SHORT:
		case GL_SHORT:
			return 2;

		case GL_UNSIGNED_INT:
		case GL_INT:
		case GL_UNSIGNED_INT_24_8_EXT:
		case GL_FLOAT:
			return 4;

		default:
			log_gl("unknown texture type %d for texture format %d\n", type, format);
			return 0;
	}
}

static int getTexImageFactorFromFormatAndType(int format, int type)
{
	switch (format)
	{
		case GL_COLOR_INDEX:
		case GL_RED:
		case GL_GREEN:
		case GL_BLUE:
		case GL_ALPHA:
		case GL_LUMINANCE:
		case GL_INTENSITY:
		case GL_DEPTH_COMPONENT:
		case GL_STENCIL_INDEX:
		case GL_DEPTH_STENCIL_EXT:
			return 1 * getTexImageTypeSizeSimple(format, type);
			break;

		case GL_LUMINANCE_ALPHA:
			return 2 * getTexImageTypeSizeSimple(format, type);
			break;

		case GL_YCBCR_MESA:
			{
				switch (type)
				{
					case GL_UNSIGNED_SHORT_8_8_MESA:
					case GL_UNSIGNED_SHORT_8_8_REV_MESA:
						return 2;

					default:
						log_gl("unknown texture type %d for texture format %d\n", type, format);
						return 0;
				}
			}

		case GL_RGB:
		case GL_BGR:
			{
				switch (type)
				{
					case GL_UNSIGNED_BYTE:
					case GL_BYTE:
						return 1 * 3;

					case GL_UNSIGNED_SHORT:
					case GL_SHORT:
						return 2 * 3;

					case GL_UNSIGNED_INT:
					case GL_INT:
					case GL_FLOAT:
						return 4 * 3;

					case GL_UNSIGNED_BYTE_3_3_2:
					case GL_UNSIGNED_BYTE_2_3_3_REV:
						return 1;

					case GL_UNSIGNED_SHORT_5_6_5:
					case GL_UNSIGNED_SHORT_5_6_5_REV:
					case GL_UNSIGNED_SHORT_8_8_MESA:
					case GL_UNSIGNED_SHORT_8_8_REV_MESA:
						return 2;

					default:
						log_gl("unknown texture type %d for texture format %d\n", type, format);
						return 0;
				}
			}

		case GL_RGBA:
		case GL_BGRA:
		case GL_ABGR_EXT:
			{
				switch (type)
				{
					case GL_UNSIGNED_BYTE:
					case GL_BYTE:
						return 1 * 4;

					case GL_UNSIGNED_SHORT:
					case GL_SHORT:
						return 2 * 4;

					case GL_UNSIGNED_INT:
					case GL_INT:
					case GL_FLOAT:
						return 4 * 4;

					case GL_UNSIGNED_SHORT_4_4_4_4:
					case GL_UNSIGNED_SHORT_4_4_4_4_REV:
					case GL_UNSIGNED_SHORT_5_5_5_1:
					case GL_UNSIGNED_SHORT_1_5_5_5_REV:
						return 2;

					case GL_UNSIGNED_INT_8_8_8_8:
					case GL_UNSIGNED_INT_8_8_8_8_REV:
					case GL_UNSIGNED_INT_10_10_10_2:
					case GL_UNSIGNED_INT_2_10_10_10_REV:
						return 4;

					default:
						log_gl("unknown texture type %d for texture format %d\n", type, format);
						return 0;
				}
			}

		default:
			log_gl("unknown texture format : %d\n", format);
			return 0;
	}
}

static void* _calcReadSize(int width, int height, int depth, GLenum format, GLenum type, void* pixels, int* p_size)
{
	int pack_row_length, pack_alignment, pack_skip_rows, pack_skip_pixels;

	LOCK(glGetIntegerv_func);
	glGetIntegerv_no_lock(GL_PACK_ROW_LENGTH, &pack_row_length);
	glGetIntegerv_no_lock(GL_PACK_ALIGNMENT, &pack_alignment);
	glGetIntegerv_no_lock(GL_PACK_SKIP_ROWS, &pack_skip_rows);
	glGetIntegerv_no_lock(GL_PACK_SKIP_PIXELS, &pack_skip_pixels);
	UNLOCK(glGetIntegerv_func);

	int w = (pack_row_length == 0) ? width : pack_row_length;
	int size = ((width * getTexImageFactorFromFormatAndType(format, type) + pack_alignment - 1) & (~(pack_alignment-1))) * depth;
	if (height >= 1)
		size += ((w * getTexImageFactorFromFormatAndType(format, type) + pack_alignment - 1) & (~(pack_alignment-1)))* (height-1)  * depth ;
	*p_size = size;

	pixels += (pack_skip_pixels + pack_skip_rows * w) * getTexImageFactorFromFormatAndType(format, type);

	return pixels;
}

static const void* _calcWriteSize(int width, int height, int depth, GLenum format, GLenum type, const void* pixels, int* p_size)
{
	int unpack_row_length, unpack_alignment, unpack_skip_rows, unpack_skip_pixels;

	LOCK(glGetIntegerv_func);
	glGetIntegerv_no_lock(GL_UNPACK_ROW_LENGTH, &unpack_row_length);
	glGetIntegerv_no_lock(GL_UNPACK_ALIGNMENT, &unpack_alignment);
	glGetIntegerv_no_lock(GL_UNPACK_SKIP_ROWS, &unpack_skip_rows);
	glGetIntegerv_no_lock(GL_UNPACK_SKIP_PIXELS, &unpack_skip_pixels);
	UNLOCK(glGetIntegerv_func);

	int w = (unpack_row_length == 0) ? width : unpack_row_length;
	int size = ((width * getTexImageFactorFromFormatAndType(format, type) + unpack_alignment - 1) & (~(unpack_alignment-1))) * depth;
	if (height >= 1)
		size += ((w * getTexImageFactorFromFormatAndType(format, type) + unpack_alignment - 1) & (~(unpack_alignment-1))) * (height-1) * depth;
	*p_size = size;

	pixels += (unpack_skip_pixels + unpack_skip_rows * w) * getTexImageFactorFromFormatAndType(format, type);

	return pixels;
}

GLAPI void APIENTRY glTexImage1D(GLenum target,
		GLint level,
		GLint internalFormat,
		GLsizei width,
		GLint border,
		GLenum format,
		GLenum type,
		const GLvoid *pixels )
{
	int size = 0;
	if (pixels)
		pixels = _calcWriteSize(width, 1, 1, format, type, pixels, &size);

	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(internalFormat),
		INT_TO_ARG(width), INT_TO_ARG(border), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, 0, (pixels == NULL) ? 0 : size };
	do_opengl_call(glTexImage1D_func, NULL, CHECK_ARGS(args, args_size));

}

GLAPI void APIENTRY glTexImage1DEXT(GLenum target,
		GLint level,
		GLint internalFormat,
		GLsizei width,
		GLint border,
		GLenum format,
		GLenum type,
		const GLvoid *pixels )
{
	glTexImage1D(target, level, internalFormat, width, border, format, type, pixels);
}

GLAPI GLint GLAPIENTRY gluBuild2DMipmaps (GLenum target, GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	int size = 0;
	pixels = _calcWriteSize(width, height, 1, format, type, pixels, &size);
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(internalFormat),
		INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, size };
	do_opengl_call(fake_gluBuild2DMipmaps_func, NULL, CHECK_ARGS(args, args_size));
	return 0;
}

GLAPI void APIENTRY glTexImage2D( GLenum target,
		GLint level,
		GLint internalFormat,
		GLsizei width,
		GLsizei height,
		GLint border,
		GLenum format,
		GLenum type,
		const GLvoid *pixels )
{
	GET_CURRENT_STATE();
	int i;
	int size = 0;
	if (pixels)
		pixels = _calcWriteSize(width, height, 1, format, type, pixels, &size);

	if (target == GL_TEXTURE_2D)
	{
		for(i=0;i<state->current_server_state.texture2DCacheDim;i++)
		{
			if (state->current_server_state.texture2DCache[i].texture == state->current_server_state.bindTexture2D &&
					state->current_server_state.texture2DCache[i].level == level)
			{
				state->current_server_state.texture2DCache[i].width = width;
				state->current_server_state.texture2DCache[i].height = height;
				break;
			}
		}
		if (i == state->current_server_state.texture2DCacheDim)
		{
			state->current_server_state.texture2DCache =
				realloc(state->current_server_state.texture2DCache, sizeof(Texture2DDim) * 
						(state->current_server_state.texture2DCacheDim + 1));
			i = state->current_server_state.texture2DCacheDim;
			state->current_server_state.texture2DCache[i].texture = state->current_server_state.bindTexture2D;
			state->current_server_state.texture2DCache[i].level = level;
			state->current_server_state.texture2DCache[i].width = width;
			state->current_server_state.texture2DCache[i].height = height;
			state->current_server_state.texture2DCacheDim++;
		}
	}
	else if (target == GL_PROXY_TEXTURE_2D_EXT)
	{
		for(i=0;i<state->current_server_state.textureProxy2DCacheDim;i++)
		{
			if (state->current_server_state.textureProxy2DCache[i].level == level)
			{
				state->current_server_state.textureProxy2DCache[i].width = width;
				state->current_server_state.textureProxy2DCache[i].height = height;
				break;
			}
		}
		if (i == state->current_server_state.textureProxy2DCacheDim)
		{
			state->current_server_state.textureProxy2DCache =
				realloc(state->current_server_state.textureProxy2DCache, sizeof(Texture2DDim) * 
						(state->current_server_state.textureProxy2DCacheDim + 1));
			i = state->current_server_state.textureProxy2DCacheDim;
			state->current_server_state.textureProxy2DCache[i].level = level;
			state->current_server_state.textureProxy2DCache[i].width = width;
			state->current_server_state.textureProxy2DCache[i].height = height;
			state->current_server_state.textureProxy2DCacheDim++;
		}
	}
	else if (target == GL_TEXTURE_RECTANGLE_ARB)
	{
		for(i=0;i<state->current_server_state.textureRectangleCacheDim;i++)
		{
			if (state->current_server_state.textureRectangleCache[i].texture == state->current_server_state.bindTextureRectangle &&
					state->current_server_state.textureRectangleCache[i].level == level)
			{
				state->current_server_state.textureRectangleCache[i].width = width;
				state->current_server_state.textureRectangleCache[i].height = height;
				break;
			}
		}
		if (i == state->current_server_state.textureRectangleCacheDim)
		{
			state->current_server_state.textureRectangleCache =
				realloc(state->current_server_state.textureRectangleCache, sizeof(Texture2DDim) * 
						(state->current_server_state.textureRectangleCacheDim + 1));
			i = state->current_server_state.textureRectangleCacheDim;
			state->current_server_state.textureRectangleCache[i].texture = state->current_server_state.bindTextureRectangle;
			state->current_server_state.textureRectangleCache[i].level = level;
			state->current_server_state.textureRectangleCache[i].width = width;
			state->current_server_state.textureRectangleCache[i].height = height;
			state->current_server_state.textureRectangleCacheDim++;
		}
	}
	else if (target == GL_PROXY_TEXTURE_RECTANGLE_ARB)
	{
		for(i=0;i<state->current_server_state.textureProxyRectangleCacheDim;i++)
		{
			if (state->current_server_state.textureProxyRectangleCache[i].level == level)
			{
				state->current_server_state.textureProxyRectangleCache[i].width = width;
				state->current_server_state.textureProxyRectangleCache[i].height = height;
				break;
			}
		}
		if (i == state->current_server_state.textureProxyRectangleCacheDim)
		{
			state->current_server_state.textureProxyRectangleCache =
				realloc(state->current_server_state.textureProxyRectangleCache, sizeof(Texture2DDim) * 
						(state->current_server_state.textureProxyRectangleCacheDim + 1));
			i = state->current_server_state.textureProxyRectangleCacheDim;
			state->current_server_state.textureProxyRectangleCache[i].level = level;
			state->current_server_state.textureProxyRectangleCache[i].width = width;
			state->current_server_state.textureProxyRectangleCache[i].height = height;
			state->current_server_state.textureProxyRectangleCacheDim++;
		}
	}


	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(internalFormat),
		INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(border), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, (pixels == NULL) ? 0 : size };
	do_opengl_call(glTexImage2D_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glTexImage2DEXT(GLenum target,
		GLint level,
		GLint internalFormat,
		GLsizei width,
		GLsizei height,
		GLint border,
		GLenum format,
		GLenum type,
		const GLvoid *pixels )
{
	glTexImage2D(target, level, internalFormat, width, height, border, format, type, pixels);
}

GLAPI void APIENTRY glTexImage3D( GLenum target,
		GLint level,
		GLint internalFormat,
		GLsizei width,
		GLsizei height,
		GLsizei depth,
		GLint border,
		GLenum format,
		GLenum type,
		const GLvoid *pixels )
{
	int size = 0;
	if (pixels)
		pixels = _calcWriteSize(width, height, depth, format, type, pixels, &size);

	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(internalFormat),
		INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(depth), INT_TO_ARG(border), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, (pixels == NULL) ? 0 : size };
	do_opengl_call(glTexImage3D_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glTexImage3DEXT)(GLenum target,
		GLint level,
		GLint internalFormat,
		GLsizei width,
		GLsizei height,
		GLsizei depth,
		GLint border,
		GLenum format,
		GLenum type,
		const GLvoid *pixels )
{
	CHECK_PROC(glTexImage3DEXT);
	glTexImage3D(target, level, internalFormat, width, height, depth, border, format, type, pixels);
}

GLAPI void APIENTRY glTexSubImage1D( GLenum target,
		GLint level,
		GLint xoffset,
		GLsizei width,
		GLenum format,
		GLenum type,
		const GLvoid *pixels )
{
	int size = 0;
	if (pixels)
		pixels = _calcWriteSize(width, 1, 1, format, type, pixels, &size);

	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(xoffset),
		INT_TO_ARG(width), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, size };
	do_opengl_call(glTexSubImage1D_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glTexSubImage1DEXT)( GLenum target,
		GLint level,
		GLint xoffset,
		GLsizei width,
		GLenum format,
		GLenum type,
		const GLvoid *pixels )
{
	CHECK_PROC(glTexSubImage1DEXT);
	glTexSubImage1D(target, level, xoffset, width, format, type, pixels);
}

GLAPI void APIENTRY glTexSubImage2D( GLenum target,
		GLint level,
		GLint xoffset,
		GLint yoffset,
		GLsizei width,
		GLsizei height,
		GLenum format,
		GLenum type,
		const GLvoid *pixels )
{
	int size = 0;
	if (pixels)
		pixels = _calcWriteSize(width, height, 1, format, type, pixels, &size);

	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(xoffset), INT_TO_ARG(yoffset),
		INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, size };
	do_opengl_call(glTexSubImage2D_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glTexSubImage2DEXT)( GLenum target,
		GLint level,
		GLint xoffset,
		GLint yoffset,
		GLsizei width,
		GLsizei height,
		GLenum format,
		GLenum type,
		const GLvoid *pixels )
{
	CHECK_PROC(glTexSubImage2DEXT);
	glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

GLAPI void APIENTRY glTexSubImage3D( GLenum target,
		GLint level,
		GLint xoffset,
		GLint yoffset,
		GLint zoffset,
		GLsizei width,
		GLsizei height,
		GLsizei depth,
		GLenum format,
		GLenum type,
		const GLvoid *pixels )
{
	int size = 0;
	if (pixels)
		pixels = _calcWriteSize(width, height, depth, format, type, pixels, &size);

	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(xoffset), INT_TO_ARG(yoffset), INT_TO_ARG(zoffset),
		INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(depth), INT_TO_ARG(format), INT_TO_ARG(type),
		POINTER_TO_ARG(pixels) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, size };
	do_opengl_call(glTexSubImage3D_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glTexSubImage3DEXT)( GLenum target,
		GLint level,
		GLint xoffset,
		GLint yoffset,
		GLint zoffset,
		GLsizei width,
		GLsizei height,
		GLsizei depth,
		GLenum format,
		GLenum type,
		const GLvoid *pixels )
{
	CHECK_PROC(glTexSubImage3DEXT);
	glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}
GLAPI void APIENTRY glSelectBuffer( GLsizei size, GLuint *buffer )
{
	if (size <= 0) return;
	GET_CURRENT_STATE();
	state->client_state.selectBufferSize = size;
	state->client_state.selectBufferPtr = buffer;
	long args[] = { INT_TO_ARG(size) };
	do_opengl_call(_glSelectBuffer_fake_func, NULL, args, NULL);
}

GLAPI void APIENTRY glFeedbackBuffer( GLsizei size, GLenum type, GLfloat *buffer )
{
	if (size <= 0) return;
	GET_CURRENT_STATE();
	state->client_state.feedbackBufferSize = size;
	state->client_state.feedbackBufferPtr = buffer;
	long args[] = { INT_TO_ARG(size), INT_TO_ARG(type) };
	do_opengl_call(_glFeedbackBuffer_fake_func, NULL, args, NULL);
}

GLAPI GLint APIENTRY glRenderMode(GLenum mode)
{
	GLint ret;
	GET_CURRENT_STATE();
	long args[] = { UNSIGNED_INT_TO_ARG(mode)};
	do_opengl_call(glRenderMode_func, &ret, args, NULL);
	if (mode == GL_SELECT && state->client_state.selectBufferPtr)
	{
		long args[] = { POINTER_TO_ARG(state->client_state.selectBufferPtr) };
		int args_size[] = { state->client_state.selectBufferSize * 4 };
		do_opengl_call(_glGetSelectBuffer_fake_func, NULL, CHECK_ARGS(args, args_size));
	}
	else if (mode == GL_FEEDBACK && state->client_state.selectBufferPtr)
	{
		long args[] = {  POINTER_TO_ARG(state->client_state.feedbackBufferPtr) };
		int args_size[] = { state->client_state.feedbackBufferSize * 4 };
		do_opengl_call(_glGetFeedbackBuffer_fake_func, NULL, CHECK_ARGS(args, args_size));
	}
	return ret;
}


GLAPI void APIENTRY EXT_FUNC(glGetCompressedTexImageARB)(GLenum target, GLint level, GLvoid *img)
{
	CHECK_PROC(glGetCompressedTexImageARB);

	int imageSize = 0;
	glGetTexLevelParameteriv(target, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB, &imageSize);

	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), POINTER_TO_ARG(img) };
	int args_size[] = { 0, 0, imageSize };
	do_opengl_call(glGetCompressedTexImageARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glGetCompressedTexImage)(GLenum target, GLint level, GLvoid *img)
{
	glGetCompressedTexImageARB(target, level, img);
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexImage1DARB)(GLenum target,
		GLint level,
		GLint internalFormat,
		GLsizei width,
		GLint border,
		GLsizei imageSize,
		const GLvoid * data)
{
	CHECK_PROC(glCompressedTexImage1DARB);
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(internalFormat),
		INT_TO_ARG(width), INT_TO_ARG(border), INT_TO_ARG(imageSize), POINTER_TO_ARG(data) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, imageSize };
	do_opengl_call(glCompressedTexImage1DARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexImage1D)(GLenum target,
		GLint level,
		GLenum internalFormat,
		GLsizei width,
		GLint border,
		GLsizei imageSize,
		const GLvoid * data)
{
	glCompressedTexImage1DARB(target, level, internalFormat, width, border, imageSize, data);
}


GLAPI void APIENTRY EXT_FUNC(glCompressedTexImage2DARB)(GLenum target,
		GLint level,
		GLint internalFormat,
		GLsizei width,
		GLsizei height,
		GLint border,
		GLsizei imageSize,
		const GLvoid * data)
{
	CHECK_PROC(glCompressedTexImage2DARB);
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(internalFormat),
		INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(border), INT_TO_ARG(imageSize), POINTER_TO_ARG(data) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, 0, imageSize };
	do_opengl_call(glCompressedTexImage2DARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexImage2D)(GLenum target,
		GLint level,
		GLenum internalFormat,
		GLsizei width,
		GLsizei height,
		GLint border,
		GLsizei imageSize,
		const GLvoid * data)
{
	glCompressedTexImage2DARB(target, level, internalFormat, width, height, border, imageSize, data);
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexImage3DARB)(GLenum target,
		GLint level,
		GLint internalFormat,
		GLsizei width,
		GLsizei height,
		GLsizei depth,
		GLint border,
		GLsizei imageSize,
		const GLvoid * data)
{
	CHECK_PROC(glCompressedTexImage3DARB);
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(internalFormat),
		INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(depth), INT_TO_ARG(border), INT_TO_ARG(imageSize), POINTER_TO_ARG(data) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, imageSize };
	do_opengl_call(glCompressedTexImage3DARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexImage3D)(GLenum target,
		GLint level,
		GLenum internalFormat,
		GLsizei width,
		GLsizei height,
		GLsizei depth,
		GLint border,
		GLsizei imageSize,
		const GLvoid * data)
{
	glCompressedTexImage3DARB(target, level, internalFormat, width, height, depth, border, imageSize, data);
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexSubImage1DARB)(GLenum target,
		GLint level,
		GLint xoffset,
		GLsizei width,
		GLenum format,
		GLsizei imageSize,
		const GLvoid * data)
{
	CHECK_PROC(glCompressedTexSubImage1DARB);
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(xoffset),
		INT_TO_ARG(width), INT_TO_ARG(format), INT_TO_ARG(imageSize), POINTER_TO_ARG(data) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, imageSize };
	do_opengl_call(glCompressedTexSubImage1DARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexSubImage1D)(GLenum target,
		GLint level,
		GLint xoffset,
		GLsizei width,
		GLenum format,
		GLsizei imageSize,
		const GLvoid * data)
{
	glCompressedTexSubImage1DARB(target, level, xoffset, width, format, imageSize, data);
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexSubImage2DARB)(GLenum target,
		GLint level,
		GLint xoffset,
		GLint yoffset,
		GLsizei width,
		GLsizei height,
		GLenum format,
		GLsizei imageSize,
		const GLvoid * data)
{
	CHECK_PROC(glCompressedTexSubImage2DARB);
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(xoffset), INT_TO_ARG(yoffset),
		INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(format), INT_TO_ARG(imageSize), POINTER_TO_ARG(data) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, imageSize };
	do_opengl_call(glCompressedTexSubImage2DARB_func, NULL, CHECK_ARGS(args, args_size));
}


GLAPI void APIENTRY EXT_FUNC(glCompressedTexSubImage2D)(GLenum target,
		GLint level,
		GLint xoffset,
		GLint yoffset,
		GLsizei width,
		GLsizei height,
		GLenum format,
		GLsizei imageSize,
		const GLvoid * data)
{
	glCompressedTexSubImage2DARB(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexSubImage3DARB)(GLenum target,
		GLint level,
		GLint xoffset,
		GLint yoffset,
		GLint zoffset,
		GLsizei width,
		GLsizei height,
		GLsizei depth,
		GLenum format,
		GLsizei imageSize,
		const GLvoid * data)
{
	CHECK_PROC(glCompressedTexSubImage3DARB);
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(xoffset), INT_TO_ARG(yoffset), INT_TO_ARG(zoffset),
		INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(depth), INT_TO_ARG(format), INT_TO_ARG(imageSize), POINTER_TO_ARG(data) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, imageSize };
	do_opengl_call(glCompressedTexSubImage3DARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexSubImage3D)(GLenum target,
		GLint level,
		GLint xoffset,
		GLint yoffset,
		GLint zoffset,
		GLsizei width,
		GLsizei height,
		GLsizei depth,
		GLenum format,
		GLsizei imageSize,
		const GLvoid * data)
{
	glCompressedTexSubImage3DARB(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}


GLAPI void APIENTRY glGetTexLevelParameteriv( GLenum target,
		GLint level,
		GLenum pname,
		GLint *params )
{
	int i;
	GET_CURRENT_STATE();

	if (target == GL_TEXTURE_2D && (pname == GL_TEXTURE_WIDTH || pname == GL_TEXTURE_HEIGHT))
	{
		for(i=0;i<state->current_server_state.texture2DCacheDim;i++)
		{
			if (state->current_server_state.texture2DCache[i].texture == state->current_server_state.bindTexture2D &&
					state->current_server_state.texture2DCache[i].level == level)
			{
				if (pname == GL_TEXTURE_WIDTH)
				{
					*params = state->current_server_state.texture2DCache[i].width;
					return;
				}
				if (pname == GL_TEXTURE_HEIGHT)
				{
					*params = state->current_server_state.texture2DCache[i].height;
					return;
				}
			}
		}
	}
	else if (target == GL_PROXY_TEXTURE_2D_EXT && (pname == GL_TEXTURE_WIDTH || pname == GL_TEXTURE_HEIGHT))
	{
		for(i=0;i<state->current_server_state.textureProxy2DCacheDim;i++)
		{
			if (state->current_server_state.textureProxy2DCache[i].level == level)
			{
				if (pname == GL_TEXTURE_WIDTH)
				{
					*params = state->current_server_state.textureProxy2DCache[i].width;
					return;
				}
				if (pname == GL_TEXTURE_HEIGHT)
				{
					*params = state->current_server_state.textureProxy2DCache[i].height;
					return;
				}
			}
		}
	}
	else if (target == GL_TEXTURE_RECTANGLE_ARB && (pname == GL_TEXTURE_WIDTH || pname == GL_TEXTURE_HEIGHT))
	{
		for(i=0;i<state->current_server_state.textureRectangleCacheDim;i++)
		{
			if (state->current_server_state.textureRectangleCache[i].texture == state->current_server_state.bindTextureRectangle &&
					state->current_server_state.textureRectangleCache[i].level == level)
			{
				if (pname == GL_TEXTURE_WIDTH)
				{
					*params = state->current_server_state.textureRectangleCache[i].width;
					return;
				}
				if (pname == GL_TEXTURE_HEIGHT)
				{
					*params = state->current_server_state.textureRectangleCache[i].height;
					return;
				}
			}
		}
	}
	else if (target == GL_PROXY_TEXTURE_RECTANGLE_ARB && (pname == GL_TEXTURE_WIDTH || pname == GL_TEXTURE_HEIGHT))
	{
		for(i=0;i<state->current_server_state.textureProxyRectangleCacheDim;i++)
		{
			if (state->current_server_state.textureProxyRectangleCache[i].level == level)
			{
				if (pname == GL_TEXTURE_WIDTH)
				{
					*params = state->current_server_state.textureProxyRectangleCache[i].width;
					return;
				}
				if (pname == GL_TEXTURE_HEIGHT)
				{
					*params = state->current_server_state.textureProxyRectangleCache[i].height;
					return;
				}
			}
		}
	}

	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(pname), POINTER_TO_ARG(params) };
	do_opengl_call(glGetTexLevelParameteriv_func, NULL, args, NULL);

}


GLAPI void APIENTRY glGetTexParameterfv( GLenum target, GLenum pname, GLfloat *params )
{
	if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT)
		*params = 1;
	else
	{
		int size = __glTexParameter_size(get_err_file(), pname);
		long args[] = { INT_TO_ARG(target), INT_TO_ARG(pname), POINTER_TO_ARG(params) };
		int args_size[] = { 0, 0, size * sizeof(GLfloat) };
		do_opengl_call(glGetTexParameterfv_func, NULL, CHECK_ARGS(args, args_size));
	}
}

GLAPI void APIENTRY glFogf(GLenum pname, GLfloat param)
{
	GET_CURRENT_STATE();
	if (pname == GL_FOG_MODE)
		state->current_server_state.fog.mode = param;
	else if (pname == GL_FOG_DENSITY)
		state->current_server_state.fog.density = param;
	else if (pname == GL_FOG_START)
		state->current_server_state.fog.start = param;
	else if (pname == GL_FOG_END)
		state->current_server_state.fog.end = param;
	else if (pname == GL_FOG_INDEX)
		state->current_server_state.fog.index = param;
	long args[] = { UNSIGNED_INT_TO_ARG(pname), FLOAT_TO_ARG(param)};
	do_opengl_call(glFogf_func, NULL, args, NULL);
}

GLAPI void APIENTRY glFogi(GLenum pname, GLint param)
{
	GET_CURRENT_STATE();
	if (pname == GL_FOG_MODE)
		state->current_server_state.fog.mode = param;
	else if (pname == GL_FOG_DENSITY)
		state->current_server_state.fog.density = param;
	else if (pname == GL_FOG_START)
		state->current_server_state.fog.start = param;
	else if (pname == GL_FOG_END)
		state->current_server_state.fog.end = param;
	else if (pname == GL_FOG_INDEX)
		state->current_server_state.fog.index = param;
	long args[] = { UNSIGNED_INT_TO_ARG(pname), INT_TO_ARG(param)};
	do_opengl_call(glFogi_func, NULL, args, NULL);
}


GLAPI void APIENTRY glFogfv( GLenum pname, const GLfloat *params )
{
	if (pname != GL_FOG_COLOR)
	{
		glFogf(pname, *params);
		return;
	}
	GET_CURRENT_STATE();
	long args[] = { INT_TO_ARG(pname), POINTER_TO_ARG(params) };
	memcpy(state->current_server_state.fog.color, params, 4 * sizeof(float));
	do_opengl_call(glFogfv_func, NULL, args, NULL);
}

GLAPI void APIENTRY glFogiv( GLenum pname, const GLint *params )
{
	if (pname != GL_FOG_COLOR)
	{
		glFogi(pname, *params);
		return;
	}
	long args[] = { INT_TO_ARG(pname), POINTER_TO_ARG(params) };
	do_opengl_call(glFogiv_func, NULL, args, NULL);
}

GLAPI void APIENTRY glRectdv( const GLdouble *v1, const GLdouble *v2 )
{
	glRectd(v1[0], v1[1], v2[0], v2[1]);
}
GLAPI void APIENTRY glRectfv( const GLfloat *v1, const GLfloat *v2 )
{
	glRectf(v1[0], v1[1], v2[0], v2[1]);
}
GLAPI void APIENTRY glRectiv( const GLint *v1, const GLint *v2 )
{
	glRecti(v1[0], v1[1], v2[0], v2[1]);
}
GLAPI void APIENTRY glRectsv( const GLshort *v1, const GLshort *v2 )
{
	glRects(v1[0], v1[1], v2[0], v2[1]);
}


GLAPI void APIENTRY glBitmap(GLsizei width,
		GLsizei height,
		GLfloat xorig,
		GLfloat yorig,
		GLfloat xmove,
		GLfloat ymove,
		const GLubyte *bitmap ) 
{
	GET_CURRENT_STATE();
	int unpack_alignment, unpack_row_length;
	LOCK(glGetIntegerv_func);
	glGetIntegerv_no_lock(GL_UNPACK_ROW_LENGTH, &unpack_row_length);
	glGetIntegerv_no_lock(GL_UNPACK_ALIGNMENT, &unpack_alignment);
	UNLOCK(glGetIntegerv_func);
	int w = (unpack_row_length == 0) ? width : unpack_row_length;
	int size = ((w + unpack_alignment - 1) & (~(unpack_alignment-1))) * height;
	long args[] = { INT_TO_ARG(width), INT_TO_ARG(height), FLOAT_TO_ARG(xorig), FLOAT_TO_ARG(yorig),
		FLOAT_TO_ARG(xmove), FLOAT_TO_ARG(ymove), POINTER_TO_ARG(bitmap) };
	int args_size[] = { 0, 0, 0, 0, 0, 0, size };
	do_opengl_call(glBitmap_func, NULL, CHECK_ARGS(args, args_size));

	state->currentRasterPos[0] += xmove;
	state->currentRasterPos[1] += ymove;
}


GLAPI void APIENTRY glGetTexImage( GLenum target,
		GLint level,
		GLenum format,
		GLenum type,
		GLvoid *pixels )
{
	int size = 0, width, height = 1, depth = 1;
	if (target == GL_PROXY_TEXTURE_1D || target == GL_PROXY_TEXTURE_2D || target == GL_PROXY_TEXTURE_3D)
	{
		log_gl("unhandled target : %d\n", target);
		return;
	}
	glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &width);
	if (target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE_ARB || target == GL_TEXTURE_3D)
		glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &height);
	if (target == GL_TEXTURE_3D)
		glGetTexLevelParameteriv(target, level, GL_TEXTURE_DEPTH, &depth);

	pixels = _calcReadSize(width, height, depth, format, type, pixels, &size);

	long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
	int args_size[] = { 0, 0, 0, 0, size };
	do_opengl_call(glGetTexImage_func, NULL, CHECK_ARGS(args, args_size));
}


static void glReadPixels_no_lock(GLint x,
		GLint y,
		GLsizei width,
		GLsizei height,
		GLenum format,
		GLenum type,
		GLvoid *pixels )
{
	GET_CURRENT_STATE();
	if (state->pixelPackBuffer)
	{
		int fake_ret;
		long args[] = { INT_TO_ARG(x), INT_TO_ARG(y), INT_TO_ARG(width), INT_TO_ARG(height),
			INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
		/* Make it synchronous, otherwise it floods server */
		do_opengl_call_no_lock(_glReadPixels_pbo_func, &fake_ret, args, NULL);
	}
	else
	{
		int size = 0;

		pixels = _calcReadSize(width, height, 1, format, type, pixels, &size);

		long args[] = { INT_TO_ARG(x), INT_TO_ARG(y), INT_TO_ARG(width), INT_TO_ARG(height),
			INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
		int args_size[] = { 0, 0, 0, 0, 0, 0, size };

		do_opengl_call_no_lock(glReadPixels_func, NULL, CHECK_ARGS(args, args_size));
	}
}
GLAPI void APIENTRY glReadPixels(GLint x,
		GLint y,
		GLsizei width,
		GLsizei height,
		GLenum format,
		GLenum type,
		GLvoid *pixels )
{
	LOCK(glReadPixels_func);
	glReadPixels_no_lock(x, y, width, height, format, type, pixels);
	UNLOCK(glReadPixels_func);
}

GLAPI void APIENTRY glDrawPixels(GLsizei width,
		GLsizei height,
		GLenum format,
		GLenum type,
		const GLvoid *pixels )
{
	GET_CURRENT_STATE();
	if (state->pixelUnpackBuffer)
	{
		long args[] = { INT_TO_ARG(width), INT_TO_ARG(height),
			INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
		do_opengl_call(_glDrawPixels_pbo_func, NULL, args, NULL);
	}
	else
	{
		int size = 0;

		pixels = _calcWriteSize(width, height, 1, format, type, pixels, &size);

		long args[] = { INT_TO_ARG(width), INT_TO_ARG(height),
			INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
		int args_size[] = { 0, 0, 0, 0, size };
		do_opengl_call(glDrawPixels_func, NULL, CHECK_ARGS(args, args_size));
	}
}

GLAPI void APIENTRY glInterleavedArrays( GLenum format,
		GLsizei stride,
		const GLvoid *ptr )
{
	CHECK_PROC(glInterleavedArrays);
	GET_CURRENT_STATE();
	if (debug_array_ptr)
		log_gl("glInterleavedArrays format=%d stride=%d ptr=%p\n",  format, stride, ptr);
	state->client_state.arrays.interleavedArrays.format = format;
	state->client_state.arrays.interleavedArrays.stride = stride;
	state->client_state.arrays.interleavedArrays.ptr = ptr;
}

GLAPI void APIENTRY glVertexPointer( GLint size,
		GLenum type,
		GLsizei stride,
		const GLvoid *ptr )
{
	CHECK_PROC(glVertexPointer);
	GET_CURRENT_STATE();

	state->client_state.arrays.vertexArray.vbo_name = state->arrayBuffer;
	if (state->client_state.arrays.vertexArray.vbo_name)
	{
		long args[] = { INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
		do_opengl_call(_glVertexPointer_buffer_func, NULL, args, NULL);
		return;
	}

	if (state->client_state.arrays.vertexArray.size == size &&
			state->client_state.arrays.vertexArray.type == type &&
			state->client_state.arrays.vertexArray.stride == stride &&
			state->client_state.arrays.vertexArray.ptr == ptr)
	{
	}
	else
	{
		if (debug_array_ptr)
			log_gl("glVertexPointer size=%d type=%d stride=%d ptr=%p\n",  size, type, stride, ptr);
		state->client_state.arrays.vertexArray.size = size;
		state->client_state.arrays.vertexArray.type = type;
		state->client_state.arrays.vertexArray.stride = stride;
		state->client_state.arrays.vertexArray.ptr = ptr;
	}
}

DEFINE_EXT(glVertexPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr), (size, type, stride, ptr));

GLAPI void APIENTRY glNormalPointer( GLenum type,
		GLsizei stride,
		const GLvoid *ptr )
{
	CHECK_PROC(glNormalPointer);
	GET_CURRENT_STATE();

	state->client_state.arrays.normalArray.vbo_name = state->arrayBuffer;
	if (state->client_state.arrays.normalArray.vbo_name)
	{
		long args[] = { INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
		do_opengl_call(_glNormalPointer_buffer_func, NULL, args, NULL);
		return;
	}

	if (state->client_state.arrays.normalArray.type == type &&
			state->client_state.arrays.normalArray.stride == stride &&
			state->client_state.arrays.normalArray.ptr == ptr)
	{
	}
	else
	{
		if (debug_array_ptr)
			log_gl("glNormalPointer type=%d stride=%d ptr=%p\n", type, stride, ptr);
		state->client_state.arrays.normalArray.size = 3;
		state->client_state.arrays.normalArray.type = type;
		state->client_state.arrays.normalArray.stride = stride;
		state->client_state.arrays.normalArray.ptr = ptr;
	}
}

DEFINE_EXT(glNormalPointer, (GLenum type, GLsizei stride, const GLvoid *ptr), (type, stride, ptr));

GLAPI void APIENTRY glIndexPointer( GLenum type,
		GLsizei stride,
		const GLvoid *ptr )
{
	CHECK_PROC(glIndexPointer);
	GET_CURRENT_STATE();

	state->client_state.arrays.indexArray.vbo_name = state->arrayBuffer;
	if (state->client_state.arrays.indexArray.vbo_name)
	{
		long args[] = { INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
		do_opengl_call(_glIndexPointer_buffer_func, NULL, args, NULL);
		return;
	}

	if (state->client_state.arrays.indexArray.type == type &&
			state->client_state.arrays.indexArray.stride == stride &&
			state->client_state.arrays.indexArray.ptr == ptr)
	{
	}
	else
	{
		if (debug_array_ptr)
			log_gl("glIndexPointer type=%d stride=%d ptr=%p\n", type, stride, ptr);
		state->client_state.arrays.indexArray.size = 1;
		state->client_state.arrays.indexArray.type = type;
		state->client_state.arrays.indexArray.stride = stride;
		state->client_state.arrays.indexArray.ptr = ptr;
	}
}

DEFINE_EXT(glIndexPointer, (GLenum type, GLsizei stride, const GLvoid *ptr), (type, stride, ptr));

GLAPI void APIENTRY glColorPointer( GLint size,
		GLenum type,
		GLsizei stride,
		const GLvoid *ptr )
{
	CHECK_PROC(glColorPointer);
	GET_CURRENT_STATE();

	state->client_state.arrays.colorArray.vbo_name = state->arrayBuffer;
	if (state->client_state.arrays.colorArray.vbo_name)
	{
		long args[] = { INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
		do_opengl_call(_glColorPointer_buffer_func, NULL, args, NULL);
		return;
	}

	if (state->client_state.arrays.colorArray.size == size &&
			state->client_state.arrays.colorArray.type == type &&
			state->client_state.arrays.colorArray.stride == stride &&
			state->client_state.arrays.colorArray.ptr == ptr)
	{
	}
	else
	{
		if (debug_array_ptr)
			log_gl("glColorPointer size=%d type=%d stride=%d ptr=%p\n", size, type, stride, ptr);
		state->client_state.arrays.colorArray.size = size;
		state->client_state.arrays.colorArray.type = type;
		state->client_state.arrays.colorArray.stride = stride;
		state->client_state.arrays.colorArray.ptr = ptr;
	}
}

DEFINE_EXT(glColorPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr), (size, type, stride, ptr));

GLAPI void APIENTRY glSecondaryColorPointer( GLint size,
		GLenum type,
		GLsizei stride,
		const GLvoid *ptr )
{
	CHECK_PROC(glSecondaryColorPointer);
	GET_CURRENT_STATE();

	state->client_state.arrays.secondaryColorArray.vbo_name = state->arrayBuffer;
	if (state->client_state.arrays.secondaryColorArray.vbo_name)
	{
		long args[] = { INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
		do_opengl_call(_glSecondaryColorPointer_buffer_func, NULL, args, NULL);
		return;
	}

	if (state->client_state.arrays.secondaryColorArray.size == size &&
			state->client_state.arrays.secondaryColorArray.type == type &&
			state->client_state.arrays.secondaryColorArray.stride == stride &&
			state->client_state.arrays.secondaryColorArray.ptr == ptr)
	{
	}
	else
	{
		if (debug_array_ptr)
			log_gl("glSecondaryColorPointer size=%d type=%d stride=%d ptr=%p\n", size, type, stride, ptr);
		state->client_state.arrays.secondaryColorArray.size = size;
		state->client_state.arrays.secondaryColorArray.type = type;
		state->client_state.arrays.secondaryColorArray.stride = stride;
		state->client_state.arrays.secondaryColorArray.ptr = ptr;
	}
}

DEFINE_EXT(glSecondaryColorPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr), (size, type, stride, ptr));

GLAPI void APIENTRY glTexCoordPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr )
{
	CHECK_PROC(glTexCoordPointer);
	GET_CURRENT_STATE();

	state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].vbo_name = state->arrayBuffer;
	if (state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].vbo_name)
	{
		long args[] = { INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
		do_opengl_call(_glTexCoordPointer_buffer_func, NULL, args, NULL);
		return;
	}

	if (state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].size == size &&
			state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].type == type &&
			state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].stride == stride &&
			state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].ptr == ptr)
	{
	}
	else
	{
		if (debug_array_ptr)
			log_gl("glTexCoordPointer[%d] size=%d type=%d stride=%d ptr=%p\n",
					state->client_state.clientActiveTexture, size, type, stride, ptr);
		state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].index =
			state->client_state.clientActiveTexture;
		state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].size = size;
		state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].type = type;
		state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].stride = stride;
		state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].ptr = ptr;
	}
}

DEFINE_EXT(glTexCoordPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr), (size, type, stride, ptr));

GLAPI void APIENTRY glEdgeFlagPointer( GLsizei stride, const GLvoid *ptr )
{
	CHECK_PROC(glEdgeFlagPointer);
	GET_CURRENT_STATE();

	state->client_state.arrays.edgeFlagArray.vbo_name = state->arrayBuffer;
	if (state->client_state.arrays.edgeFlagArray.vbo_name)
	{
		long args[] = { INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
		do_opengl_call(_glEdgeFlagPointer_buffer_func, NULL, args, NULL);
		return;
	}

	if (state->client_state.arrays.edgeFlagArray.stride == stride &&
			state->client_state.arrays.edgeFlagArray.ptr == ptr)
	{
	}
	else
	{
		if (debug_array_ptr)
			log_gl("edgeFlagArray stride=%d ptr=%p\n", stride, ptr);
		state->client_state.arrays.edgeFlagArray.size = 1;
		state->client_state.arrays.edgeFlagArray.type = GL_BYTE;
		state->client_state.arrays.edgeFlagArray.stride = stride;
		state->client_state.arrays.edgeFlagArray.ptr = ptr;
	}
}

DEFINE_EXT(glEdgeFlagPointer, (GLsizei stride, const GLvoid *ptr), (stride, ptr));

GLAPI void APIENTRY glFogCoordPointer(GLenum type, GLsizei stride, const GLvoid *ptr)
{
	CHECK_PROC(glFogCoordPointer);
	GET_CURRENT_STATE();

	state->client_state.arrays.fogCoordArray.vbo_name = state->arrayBuffer;
	if (state->client_state.arrays.fogCoordArray.vbo_name)
	{
		long args[] = { INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
		do_opengl_call(_glFogCoordPointer_buffer_func, NULL, args, NULL);
		return;
	}

	if (state->client_state.arrays.fogCoordArray.type == type &&
			state->client_state.arrays.fogCoordArray.stride == stride &&
			state->client_state.arrays.fogCoordArray.ptr == ptr)
	{
	}
	else
	{
		if (debug_array_ptr)
			log_gl("glFogCoordPointer type=%d stride=%d ptr=%p\n", type, stride, ptr);
		state->client_state.arrays.fogCoordArray.size = 1;
		state->client_state.arrays.fogCoordArray.type = type;
		state->client_state.arrays.fogCoordArray.stride = stride;
		state->client_state.arrays.fogCoordArray.ptr = ptr;
	}
}

DEFINE_EXT(glFogCoordPointer, (GLenum type, GLsizei stride, const GLvoid *ptr), (type, stride, ptr));


GLAPI void APIENTRY glWeightPointerARB (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
	CHECK_PROC(glWeightPointerARB);
	GET_CURRENT_STATE();

	state->client_state.arrays.weightArray.vbo_name = state->arrayBuffer;
	if (state->client_state.arrays.weightArray.vbo_name)
	{
		long args[] = { INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
		do_opengl_call(_glWeightPointerARB_buffer_func, NULL, args, NULL);
		return;
	}

	log_gl("glWeightPointerARB\n");
	fflush(get_err_file());
	if (debug_array_ptr)
		log_gl("weightArray size=%d type=%d stride=%d ptr=%p\n", size, type, stride, ptr);
	state->client_state.arrays.weightArray.size = size;
	state->client_state.arrays.weightArray.type = type;
	state->client_state.arrays.weightArray.stride = stride;
	state->client_state.arrays.weightArray.ptr = ptr;
}


GLAPI void APIENTRY glMatrixIndexPointerARB (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
	CHECK_PROC(glMatrixIndexPointerARB);
	GET_CURRENT_STATE();

	state->client_state.arrays.matrixIndexArray.vbo_name = state->arrayBuffer;
	if (state->client_state.arrays.matrixIndexArray.vbo_name)
	{
		long args[] = { INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
		do_opengl_call(_glMatrixIndexPointerARB_buffer_func, NULL, args, NULL);
		return;
	}

	log_gl("glMatrixIndexPointerARB\n");
	fflush(get_err_file());
	if (debug_array_ptr)
		log_gl("matrixIndexArray size=%d type=%d stride=%d ptr=%p\n", size, type, stride, ptr);
	state->client_state.arrays.matrixIndexArray.size = size;
	state->client_state.arrays.matrixIndexArray.type = type;
	state->client_state.arrays.matrixIndexArray.stride = stride;
	state->client_state.arrays.matrixIndexArray.ptr = ptr;
}

#define glMatrixIndexvARB(name, type) \
	GLAPI void APIENTRY name (GLint size, const type *indices) \
{ \
	CHECK_PROC(name); \
	long args[] = { INT_TO_ARG(size), POINTER_TO_ARG(indices) }; \
	int args_size[] = { 0, size * sizeof(type) }; \
	do_opengl_call(CONCAT(name, _func), NULL, CHECK_ARGS(args, args_size)); \
}

glMatrixIndexvARB(glMatrixIndexubvARB, GLubyte);
glMatrixIndexvARB(glMatrixIndexusvARB, GLushort);
glMatrixIndexvARB(glMatrixIndexuivARB, GLuint);


GLAPI void APIENTRY glVertexWeightfEXT (GLfloat weight)
{
	log_gl("glVertexWeightfEXT : deprecated API. unimplemented\n");
}

GLAPI void APIENTRY glVertexWeightfvEXT (const GLfloat *weight)
{
	log_gl("glVertexWeightfvEXT : deprecated API. unimplemented\n");
}

GLAPI void APIENTRY glVertexWeightPointerEXT (GLsizei size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	log_gl("glVertexWeightPointerEXT : deprecated API. unimplemented\n");
}



GLAPI void APIENTRY EXT_FUNC(glVertexAttribPointerARB)(GLuint index,
		GLint size,
		GLenum type,
		GLboolean normalized,
		GLsizei stride,
		const GLvoid *ptr)
{
	CHECK_PROC(glVertexAttribPointerARB);

	GET_CURRENT_STATE();
	if (index < MY_GL_MAX_VERTEX_ATTRIBS_ARB)
	{
		state->client_state.arrays.vertexAttribArray[index].vbo_name = state->arrayBuffer;
		if (state->client_state.arrays.vertexAttribArray[index].vbo_name)
		{
			long args[] = { INT_TO_ARG(index), INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(normalized), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
			do_opengl_call(_glVertexAttribPointerARB_buffer_func, NULL, args, NULL);
			return;
		}

		if (debug_array_ptr)
			log_gl("glVertexAttribPointerARB[%d] size=%d type=%d normalized=%d stride=%d ptr=%p\n",
					index, size, type, normalized, stride, ptr);
		state->client_state.arrays.vertexAttribArray[index].index = index;
		state->client_state.arrays.vertexAttribArray[index].size = size;
		state->client_state.arrays.vertexAttribArray[index].type = type;
		state->client_state.arrays.vertexAttribArray[index].normalized = normalized;
		state->client_state.arrays.vertexAttribArray[index].stride = stride;
		state->client_state.arrays.vertexAttribArray[index].ptr = ptr;
	}
	else
	{
		log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n");
	}
}

GLAPI void APIENTRY EXT_FUNC(glVertexAttribPointer)(GLuint index,
		GLint size,
		GLenum type,
		GLboolean normalized,
		GLsizei stride,
		const GLvoid *ptr)
{
	glVertexAttribPointerARB(index, size, type, normalized, stride, ptr);
}

GLAPI void APIENTRY EXT_FUNC(glVertexAttribPointerNV)(GLuint index,
		GLint size,
		GLenum type,
		GLsizei stride,
		const GLvoid *ptr)
{
	CHECK_PROC(glVertexAttribPointerNV);

	GET_CURRENT_STATE();
	if (index < MY_GL_MAX_VERTEX_ATTRIBS_NV)
	{
		if (debug_array_ptr)
			log_gl("glVertexAttribPointerNV[%d] size=%d type=%d stride=%d ptr=%p\n",
					index, size, type, stride, ptr);
		state->client_state.arrays.vertexAttribArrayNV[index].index = index;
		state->client_state.arrays.vertexAttribArrayNV[index].size = size;
		state->client_state.arrays.vertexAttribArrayNV[index].type = type;
		state->client_state.arrays.vertexAttribArrayNV[index].stride = stride;
		state->client_state.arrays.vertexAttribArrayNV[index].ptr = ptr;
	}
	else
	{
		log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_NV\n");
	}
}

GLAPI void APIENTRY EXT_FUNC(glElementPointerATI) (GLenum type, const GLvoid * ptr)
{
	CHECK_PROC(glElementPointerATI);
	GET_CURRENT_STATE();
	state->client_state.arrays.elementArrayATI.size = 1;
	state->client_state.arrays.elementArrayATI.type = type;
	state->client_state.arrays.elementArrayATI.stride = 0;
	state->client_state.arrays.elementArrayATI.ptr = ptr;
}

GLAPI void APIENTRY glGetPointerv( GLenum pname, void **params )
{
	GET_CURRENT_STATE();
	switch (pname)
	{
		case GL_COLOR_ARRAY_POINTER: *params = (void*)state->client_state.arrays.colorArray.ptr; break;
		case GL_SECONDARY_COLOR_ARRAY_POINTER: *params = (void*)state->client_state.arrays.secondaryColorArray.ptr; break;
		case GL_NORMAL_ARRAY_POINTER: *params = (void*)state->client_state.arrays.normalArray.ptr; break;
		case GL_INDEX_ARRAY_POINTER: *params = (void*)state->client_state.arrays.indexArray.ptr; break;
		case GL_TEXTURE_COORD_ARRAY_POINTER: *params = (void*)state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].ptr; break;
		case GL_VERTEX_ARRAY_POINTER: *params = (void*)state->client_state.arrays.vertexArray.ptr; break;
		case GL_EDGE_FLAG_ARRAY_POINTER: *params = (void*)state->client_state.arrays.edgeFlagArray.ptr; break;
		case GL_WEIGHT_ARRAY_POINTER_ARB: *params = (void*)state->client_state.arrays.weightArray.ptr; break;
		case GL_MATRIX_INDEX_ARRAY_POINTER_ARB: *params = (void*)state->client_state.arrays.matrixIndexArray.ptr; break;
		case GL_FOG_COORD_ARRAY_POINTER: *params = (void*)state->client_state.arrays.fogCoordArray.ptr; break;
		case GL_ELEMENT_ARRAY_POINTER_ATI: *params = (void*)state->client_state.arrays.elementArrayATI.ptr; break;
		case GL_SELECTION_BUFFER_POINTER: *params = (void*)state->client_state.selectBufferPtr; break;
		case GL_FEEDBACK_BUFFER_POINTER: *params = (void*)state->client_state.feedbackBufferPtr; break;
		default:
										 {
											 log_gl("not yet handled pname %d\n", pname);
											 *params = NULL;
										 }
	}
}

GLAPI void APIENTRY glGetPointervEXT( GLenum pname, void **params )
{
	glGetPointerv(pname, params);
}

GLAPI void APIENTRY EXT_FUNC(glGetVertexAttribPointervARB)(GLuint index,
		GLenum pname,
		GLvoid **pointer)
{
	GET_CURRENT_STATE();

	if (index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB)
	{
		log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n");
		return;
	}

	switch (pname)
	{
		case GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB:
			{
				*pointer = (void*)state->client_state.arrays.vertexAttribArray[index].ptr;
				break;
			}
		default:
			log_gl("glGetVertexAttribPointervARB : bad pname=0x%X", pname);
			break;
	}
}

GLAPI void APIENTRY EXT_FUNC(glGetVertexAttribPointerv)(GLuint index,
		GLenum pname,
		GLvoid **pointer)
{
	glGetVertexAttribPointervARB(index, pname, pointer);
}

GLAPI void APIENTRY EXT_FUNC(_glGetVertexAttribiv)(int func_number, GLuint index, GLenum pname, GLint *params)
{
	if (index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB)
	{
		log_gl("%s: index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n", tab_opengl_calls_name[func_number]);
		return;
	}

	GET_CURRENT_STATE();
	switch (pname)
	{
		case GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB:
			*params = state->client_state.arrays.vertexAttribArray[index].enabled;
			break;
		case GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB:
			*params = state->client_state.arrays.vertexAttribArray[index].size;
			break;
		case GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB:
			*params = state->client_state.arrays.vertexAttribArray[index].stride;
			break;
		case GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB:
			*params = state->client_state.arrays.vertexAttribArray[index].type;
			break;
		case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB:
			*params = state->client_state.arrays.vertexAttribArray[index].normalized;
			break;
		case GL_CURRENT_VERTEX_ATTRIB_ARB:
			*params = state->client_state.arrays.vertexAttribArray[index].vbo_name;
			break;
		default:
			log_gl("%s : bad pname=0x%X", tab_opengl_calls_name[func_number], pname);
			break;
	}
}

#define DEFINE_glGetVertexAttribv(funcName, type) \
	GLAPI void APIENTRY EXT_FUNC(funcName)(GLuint index, GLenum pname, type *params) \
{ \
	CHECK_PROC(funcName); \
	if (pname == GL_CURRENT_VERTEX_ATTRIB_ARB) \
	{ \
		long args[] = { UNSIGNED_INT_TO_ARG(index), UNSIGNED_INT_TO_ARG(pname), POINTER_TO_ARG(params)}; \
		do_opengl_call(CONCAT(funcName,_func), NULL, args, NULL); \
		return; \
	} \
	int i_params; \
	_glGetVertexAttribiv(CONCAT(funcName,_func), index, pname, &i_params); \
	*params = i_params; \
}

DEFINE_glGetVertexAttribv(glGetVertexAttribivARB, GLint);
DEFINE_glGetVertexAttribv(glGetVertexAttribiv, GLint);
DEFINE_glGetVertexAttribv(glGetVertexAttribfvARB, GLfloat);
DEFINE_glGetVertexAttribv(glGetVertexAttribfv, GLfloat);
DEFINE_glGetVertexAttribv(glGetVertexAttribdvARB, GLdouble);
DEFINE_glGetVertexAttribv(glGetVertexAttribdv, GLdouble);


GLAPI void APIENTRY EXT_FUNC(glGetVertexAttribPointervNV)(GLuint index,
		GLenum pname,
		GLvoid **pointer)
{
	CHECK_PROC(glGetVertexAttribPointervNV);
	GET_CURRENT_STATE();
	if (index >= MY_GL_MAX_VERTEX_ATTRIBS_NV)
	{
		log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_NV\n");
		return;
	}
	switch (pname)
	{
		case GL_ATTRIB_ARRAY_POINTER_NV:
			{
				*pointer = (void*)state->client_state.arrays.vertexAttribArrayNV[index].ptr;
				break;
			}
		default:
			log_gl("glGetVertexAttribPointervNV : bad pname=0x%X", pname);
			break;
	}
}

static int getGlTypeByteSize(int type)
{
	switch(type)
	{
		case GL_BYTE:
		case GL_UNSIGNED_BYTE:
			return 1;
			break;

		case GL_SHORT:
		case GL_UNSIGNED_SHORT:
			return 2;
			break;

		case GL_INT:
		case GL_UNSIGNED_INT:
		case GL_FLOAT:
			return 4;
			break;

		case GL_DOUBLE:
			return 8;
			break;

		default:
			log_gl("unsupported type = %d\n", type);
			return 0;
	}
}

static int getMulFactorFromPointerArray(ClientArray* array)
{
	if (array->stride)
		return array->stride;

	return getGlTypeByteSize(array->type) * array->size;
}

static void _glArraySend(GLState* state, const char* func_name, int func, ClientArray* array, int first, int last)
{
	if (array->ptr == NULL || array->vbo_name || array->enabled == 0) return;
	int offset = first * getMulFactorFromPointerArray(array);
	int size = (last - first + 1) * getMulFactorFromPointerArray(array);
	if (size == 0) return;

#if 0
	unsigned int crc = calc_checksum(array->ptr + offset, size, 0xFFFFFFFF);
	crc = calc_checksum(&offset, sizeof(int), crc);
	crc = calc_checksum(&size, sizeof(int), crc);
	crc = calc_checksum(&array->size, sizeof(int), crc);
	crc = calc_checksum(&array->type, sizeof(int), crc);

	if (crc == 0)
	{
		/*int i;
		  unsigned char* ptr = (unsigned char*)(array->ptr + offset);
		  for(i=0;i<size;i++)
		  {
		  log_gl("%d ", (int)ptr[i]);
		  }*/
		log_gl("strange : crc = 0\n");
	}

	if (crc == array->last_crc)
	{
		if (debug_array_ptr)
		{
			log_gl("%s : same crc. Saving %d bytes\n", func_name, size);
		}
		return;
	}

	array->last_crc = crc;
#endif

	if (debug_array_ptr)
	{
		unsigned int crc = calc_checksum(array->ptr + offset, size, 0xFFFFFFFF);
		crc = calc_checksum(&offset, sizeof(int), crc);
		crc = calc_checksum(&size, sizeof(int), crc);
		crc = calc_checksum(&array->size, sizeof(int), crc);
		crc = calc_checksum(&array->type, sizeof(int), crc);

		log_gl("%s sending %d bytes from %d : crc = %d\n", func_name, size, offset, crc);
	}

	int currentArrayBuffer = state->arrayBuffer;
	if (currentArrayBuffer)
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}

	switch(func)
	{
		case glEdgeFlagPointer_fake_func:
			{
				long args[] = { INT_TO_ARG(offset),
					INT_TO_ARG(array->stride),
					INT_TO_ARG(size),
					POINTER_TO_ARG(array->ptr + offset) };
				int args_size[] = { 0, 0, 0, size};
				do_opengl_call(func, NULL, CHECK_ARGS(args, args_size));
				break;
			}

		case glNormalPointer_fake_func:
		case glIndexPointer_fake_func:
		case glFogCoordPointer_fake_func:
			{
				long args[] = { INT_TO_ARG(offset),
					INT_TO_ARG(array->type),
					INT_TO_ARG(array->stride),
					INT_TO_ARG(size),
					POINTER_TO_ARG(array->ptr + offset) };
				int args_size[] = { 0, 0, 0, 0, size};
				do_opengl_call(func, NULL, CHECK_ARGS(args, args_size));
				break;
			}

		case glVertexPointer_fake_func:
		case glColorPointer_fake_func:
		case glSecondaryColorPointer_fake_func:
		case glWeightPointerARB_fake_func:
		case glMatrixIndexPointerARB_fake_func:
			{
				long args[] = { INT_TO_ARG(offset),
					INT_TO_ARG(array->size),
					INT_TO_ARG(array->type),
					INT_TO_ARG(array->stride),
					INT_TO_ARG(size),
					POINTER_TO_ARG(array->ptr + offset) };
				int args_size[] = { 0, 0, 0, 0, 0, size};
				do_opengl_call(func, NULL, CHECK_ARGS(args, args_size));
				break;
			}

		case glTexCoordPointer_fake_func:
		case glVertexAttribPointerNV_fake_func:
			{
				long args[] = { INT_TO_ARG(offset),
					INT_TO_ARG(array->index),
					INT_TO_ARG(array->size),
					INT_TO_ARG(array->type),
					INT_TO_ARG(array->stride),
					INT_TO_ARG(size),
					POINTER_TO_ARG(array->ptr + offset) };
				int args_size[] = { 0, 0, 0, 0, 0, 0, size};
				do_opengl_call(func, NULL, CHECK_ARGS(args, args_size));
				break;
			}

		case glVertexAttribPointerARB_fake_func:
			{
				long args[] = { INT_TO_ARG(offset),
					INT_TO_ARG(array->index),
					INT_TO_ARG(array->size),
					INT_TO_ARG(array->type),
					INT_TO_ARG(array->normalized),
					INT_TO_ARG(array->stride),
					INT_TO_ARG(size),
					POINTER_TO_ARG(array->ptr + offset) };
				int args_size[] = { 0, 0, 0, 0, 0, 0, 0, size};
				do_opengl_call(func, NULL, CHECK_ARGS(args, args_size));
				break;
			}

		case glVariantPointerEXT_fake_func:
			{
				long args[] = { INT_TO_ARG(offset),
					INT_TO_ARG(array->index),
					INT_TO_ARG(array->type),
					INT_TO_ARG(array->stride),
					INT_TO_ARG(size),
					POINTER_TO_ARG(array->ptr + offset) };
				int args_size[] = { 0, 0, 0, 0, 0, size};
				do_opengl_call(func, NULL, CHECK_ARGS(args, args_size));
				break;
			}

		default:
			log_gl("shoudln't reach that point\n");
			break;
	}

	if (currentArrayBuffer)
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, currentArrayBuffer);
	}
}


#define ARRAY_SEND_FUNC(x)  #x, CONCAT(x,_fake_func)

static int _calc_interleaved_arrays_stride(GLenum format)
{
	switch(format)
	{
		case GL_V2F: return 2 * sizeof(float); break;
		case GL_V3F: return 3 * sizeof(float); break;
		case GL_C4UB_V2F: return 4 * sizeof(char) + 2 * sizeof(float); break;
		case GL_C4UB_V3F: return 4 * sizeof(char) + 3 * sizeof(float); break;
		case GL_C3F_V3F: return 3 * sizeof(float) + 3 * sizeof(float); break;
		case GL_N3F_V3F: return 3 * sizeof(float) + 3 * sizeof(float); break;
		case GL_C4F_N3F_V3F: return 4 * sizeof(float) + 3 * sizeof(float) + 3 * sizeof(float); break;
		case GL_T2F_V3F: return 2 * sizeof(float) + 3 * sizeof(float); break;
		case GL_T4F_V4F: return 4 * sizeof(float) + 4 * sizeof(float); break;
		case GL_T2F_C4UB_V3F: return 2 * sizeof(float) + 4 * sizeof(char) + 3 * sizeof(float); break;
		case GL_T2F_C3F_V3F: return 2 * sizeof(float) + 3 * sizeof(float) + 3 * sizeof(float); break;
		case GL_T2F_N3F_V3F: return 2 * sizeof(float) + 3 * sizeof(float) + 3 * sizeof(float); break;
		case GL_T2F_C4F_N3F_V3F: return 2 * sizeof(float) + 4 * sizeof(float) + 3 * sizeof(float) + 3 * sizeof(float); break;
		case GL_T4F_C4F_N3F_V4F: return 4 * sizeof(float) + 4 * sizeof(float) + 3 * sizeof(float) + 4 * sizeof(float); break;
		default: log_gl("unknown interleaved array format : %d\n", format); return 0;
	}
}


enum
{
	TEXCOORD_FUNC,
	COLOR_FUNC,
	NORMAL_FUNC,
	INDEX_FUNC,
	VERTEX_FUNC
};

typedef void (*vector_func_type)(const void*);

typedef struct
{
	int type;
	int size;
	int typeFunc;
	vector_func_type vector_func;
} VectorFuncStruct;

VectorFuncStruct vectorFuncArray[] = 
{
	{ GL_DOUBLE, 1, TEXCOORD_FUNC, (vector_func_type)glTexCoord1dv },
	{ GL_DOUBLE, 2, TEXCOORD_FUNC, (vector_func_type)glTexCoord2dv },
	{ GL_DOUBLE, 3, TEXCOORD_FUNC, (vector_func_type)glTexCoord3dv },
	{ GL_DOUBLE, 4, TEXCOORD_FUNC, (vector_func_type)glTexCoord4dv },
	{ GL_FLOAT, 1, TEXCOORD_FUNC, (vector_func_type)glTexCoord1fv },
	{ GL_FLOAT, 2, TEXCOORD_FUNC, (vector_func_type)glTexCoord2fv },
	{ GL_FLOAT, 3, TEXCOORD_FUNC, (vector_func_type)glTexCoord3fv },
	{ GL_FLOAT, 4, TEXCOORD_FUNC, (vector_func_type)glTexCoord4fv },
	{ GL_INT, 1, TEXCOORD_FUNC, (vector_func_type)glTexCoord1iv },
	{ GL_INT, 2, TEXCOORD_FUNC, (vector_func_type)glTexCoord2iv },
	{ GL_INT, 3, TEXCOORD_FUNC, (vector_func_type)glTexCoord3iv },
	{ GL_INT, 4, TEXCOORD_FUNC, (vector_func_type)glTexCoord4iv },
	{ GL_SHORT, 1, TEXCOORD_FUNC, (vector_func_type)glTexCoord1sv },
	{ GL_SHORT, 2, TEXCOORD_FUNC, (vector_func_type)glTexCoord2sv },
	{ GL_SHORT, 3, TEXCOORD_FUNC, (vector_func_type)glTexCoord3sv },
	{ GL_SHORT, 4, TEXCOORD_FUNC, (vector_func_type)glTexCoord4sv },

	{ GL_BYTE, 3, COLOR_FUNC, (vector_func_type)glColor3bv },
	{ GL_DOUBLE, 3, COLOR_FUNC, (vector_func_type)glColor3dv },
	{ GL_FLOAT, 3, COLOR_FUNC, (vector_func_type)glColor3fv },
	{ GL_INT, 3, COLOR_FUNC, (vector_func_type)glColor3iv },
	{ GL_SHORT, 3, COLOR_FUNC, (vector_func_type)glColor3sv },
	{ GL_UNSIGNED_BYTE, 3, COLOR_FUNC, (vector_func_type)glColor3ubv },
	{ GL_UNSIGNED_INT, 3, COLOR_FUNC, (vector_func_type)glColor3uiv },
	{ GL_UNSIGNED_SHORT, 3, COLOR_FUNC, (vector_func_type)glColor3usv },
	{ GL_BYTE, 4, COLOR_FUNC, (vector_func_type)glColor4bv },
	{ GL_DOUBLE, 4, COLOR_FUNC, (vector_func_type)glColor4dv },
	{ GL_FLOAT, 4, COLOR_FUNC, (vector_func_type)glColor4fv },
	{ GL_INT, 4, COLOR_FUNC, (vector_func_type)glColor4iv },
	{ GL_SHORT, 4, COLOR_FUNC, (vector_func_type)glColor4sv },
	{ GL_UNSIGNED_BYTE, 4, COLOR_FUNC, (vector_func_type)glColor4ubv },
	{ GL_UNSIGNED_INT, 4, COLOR_FUNC, (vector_func_type)glColor4uiv },
	{ GL_UNSIGNED_SHORT, 4, COLOR_FUNC, (vector_func_type)glColor4usv },

	{ GL_BYTE, 3, NORMAL_FUNC, (vector_func_type)glNormal3bv },
	{ GL_DOUBLE, 3, NORMAL_FUNC, (vector_func_type)glNormal3dv },
	{ GL_FLOAT, 3, NORMAL_FUNC, (vector_func_type)glNormal3fv },
	{ GL_INT, 3, NORMAL_FUNC, (vector_func_type)glNormal3iv },
	{ GL_SHORT, 3, NORMAL_FUNC, (vector_func_type)glNormal3sv },

	{ GL_DOUBLE, 1, INDEX_FUNC, (vector_func_type)glIndexdv },
	{ GL_FLOAT, 1, INDEX_FUNC, (vector_func_type)glIndexfv },
	{ GL_INT, 1, INDEX_FUNC, (vector_func_type)glIndexiv },
	{ GL_SHORT, 1, INDEX_FUNC, (vector_func_type)glIndexsv },
	{ GL_UNSIGNED_BYTE, 1, INDEX_FUNC, (vector_func_type)glIndexubv },

	{ GL_DOUBLE, 2, VERTEX_FUNC, (vector_func_type)glVertex2dv },
	{ GL_FLOAT, 2, VERTEX_FUNC, (vector_func_type)glVertex2fv },
	{ GL_INT, 2, VERTEX_FUNC, (vector_func_type)glVertex2iv },
	{ GL_SHORT, 2, VERTEX_FUNC, (vector_func_type)glVertex2sv },
	{ GL_DOUBLE, 3, VERTEX_FUNC, (vector_func_type)glVertex3dv },
	{ GL_FLOAT, 3, VERTEX_FUNC, (vector_func_type)glVertex3fv },
	{ GL_INT, 3, VERTEX_FUNC, (vector_func_type)glVertex3iv },
	{ GL_SHORT, 4, VERTEX_FUNC, (vector_func_type)glVertex4sv },
	{ GL_DOUBLE, 4, VERTEX_FUNC, (vector_func_type)glVertex4dv },
	{ GL_FLOAT, 4, VERTEX_FUNC, (vector_func_type)glVertex4fv },
	{ GL_INT, 4, VERTEX_FUNC, (vector_func_type)glVertex4iv },
	{ GL_SHORT, 4, VERTEX_FUNC, (vector_func_type)glVertex4sv },
};

static vector_func_type _get_vector_func(int type, int size, int typeFunc)
{
	int i;
	for(i=0;i<sizeof(vectorFuncArray)/sizeof(vectorFuncArray[0]);i++)
	{
		if (vectorFuncArray[i].type == type &&
				vectorFuncArray[i].size == size &&
				vectorFuncArray[i].typeFunc == typeFunc)
			return vectorFuncArray[i].vector_func;
	}
	log_gl("can't find vector_func(type=%X, size=%d, typeFunc=%d)\n", type, size, typeFunc);
	return NULL;
}

static void _glElementArrayImmediate_one(ClientArray* array, int indice, int typeFunc)
{
	if (array->enabled)
	{
		vector_func_type vector_func =
			_get_vector_func(array->type, array->size, typeFunc);
		if (vector_func)
		{
			vector_func(array->ptr + getMulFactorFromPointerArray(array) * indice);
		}
	}
}

static void _glElementArrayImmediate(int indice)
{
	GET_CURRENT_STATE();

	if (state->client_state.arrays.interleavedArrays.ptr != NULL &&
			state->client_state.arrays.vertexArray.ptr == NULL)
	{
		GLboolean tflag, cflag, nflag;  /* enable/disable flags */
		GLint tcomps, ccomps, vcomps;   /* components per texcoord, color, vertex */
		GLenum ctype = 0;               /* color type */
		GLint coffset = 0, noffset = 0, voffset;/* color, normal, vertex offsets */
		const GLint toffset = 0;        /* always zero */
		GLint defstride;                /* default stride */
		GLint c, f;

		int stride = state->client_state.arrays.interleavedArrays.stride;

		f = sizeof(GLfloat);
		c = f * ((4 * sizeof(GLubyte) + (f - 1)) / f);

		switch (state->client_state.arrays.interleavedArrays.format) {
			case GL_V2F:
				tflag = GL_FALSE;  cflag = GL_FALSE;  nflag = GL_FALSE;
				tcomps = 0;  ccomps = 0;  vcomps = 2;
				voffset = 0;
				defstride = 2*f;
				break;
			case GL_V3F:
				tflag = GL_FALSE;  cflag = GL_FALSE;  nflag = GL_FALSE;
				tcomps = 0;  ccomps = 0;  vcomps = 3;
				voffset = 0;
				defstride = 3*f;
				break;
			case GL_C4UB_V2F:
				tflag = GL_FALSE;  cflag = GL_TRUE;  nflag = GL_FALSE;
				tcomps = 0;  ccomps = 4;  vcomps = 2;
				ctype = GL_UNSIGNED_BYTE;
				coffset = 0;
				voffset = c;
				defstride = c + 2*f;
				break;
			case GL_C4UB_V3F:
				tflag = GL_FALSE;  cflag = GL_TRUE;  nflag = GL_FALSE;
				tcomps = 0;  ccomps = 4;  vcomps = 3;
				ctype = GL_UNSIGNED_BYTE;
				coffset = 0;
				voffset = c;
				defstride = c + 3*f;
				break;
			case GL_C3F_V3F:
				tflag = GL_FALSE;  cflag = GL_TRUE;  nflag = GL_FALSE;
				tcomps = 0;  ccomps = 3;  vcomps = 3;
				ctype = GL_FLOAT;
				coffset = 0;
				voffset = 3*f;
				defstride = 6*f;
				break;
			case GL_N3F_V3F:
				tflag = GL_FALSE;  cflag = GL_FALSE;  nflag = GL_TRUE;
				tcomps = 0;  ccomps = 0;  vcomps = 3;
				noffset = 0;
				voffset = 3*f;
				defstride = 6*f;
				break;
			case GL_C4F_N3F_V3F:
				tflag = GL_FALSE;  cflag = GL_TRUE;  nflag = GL_TRUE;
				tcomps = 0;  ccomps = 4;  vcomps = 3;
				ctype = GL_FLOAT;
				coffset = 0;
				noffset = 4*f;
				voffset = 7*f;
				defstride = 10*f;
				break;
			case GL_T2F_V3F:
				tflag = GL_TRUE;  cflag = GL_FALSE;  nflag = GL_FALSE;
				tcomps = 2;  ccomps = 0;  vcomps = 3;
				voffset = 2*f;
				defstride = 5*f;
				break;
			case GL_T4F_V4F:
				tflag = GL_TRUE;  cflag = GL_FALSE;  nflag = GL_FALSE;
				tcomps = 4;  ccomps = 0;  vcomps = 4;
				voffset = 4*f;
				defstride = 8*f;
				break;
			case GL_T2F_C4UB_V3F:
				tflag = GL_TRUE;  cflag = GL_TRUE;  nflag = GL_FALSE;
				tcomps = 2;  ccomps = 4;  vcomps = 3;
				ctype = GL_UNSIGNED_BYTE;
				coffset = 2*f;
				voffset = c+2*f;
				defstride = c+5*f;
				break;
			case GL_T2F_C3F_V3F:
				tflag = GL_TRUE;  cflag = GL_TRUE;  nflag = GL_FALSE;
				tcomps = 2;  ccomps = 3;  vcomps = 3;
				ctype = GL_FLOAT;
				coffset = 2*f;
				voffset = 5*f;
				defstride = 8*f;
				break;
			case GL_T2F_N3F_V3F:
				tflag = GL_TRUE;  cflag = GL_FALSE;  nflag = GL_TRUE;
				tcomps = 2;  ccomps = 0;  vcomps = 3;
				noffset = 2*f;
				voffset = 5*f;
				defstride = 8*f;
				break;
			case GL_T2F_C4F_N3F_V3F:
				tflag = GL_TRUE;  cflag = GL_TRUE;  nflag = GL_TRUE;
				tcomps = 2;  ccomps = 4;  vcomps = 3;
				ctype = GL_FLOAT;
				coffset = 2*f;
				noffset = 6*f;
				voffset = 9*f;
				defstride = 12*f;
				break;
			case GL_T4F_C4F_N3F_V4F:
				tflag = GL_TRUE;  cflag = GL_TRUE;  nflag = GL_TRUE;
				tcomps = 4;  ccomps = 4;  vcomps = 4;
				ctype = GL_FLOAT;
				coffset = 4*f;
				noffset = 8*f;
				voffset = 11*f;
				defstride = 15*f;
				break;
			default:
				log_gl("unknown interleaved array format : %d\n", state->client_state.arrays.interleavedArrays.format);
				return;
		}

		if (stride==0) {
			stride = defstride;
		}

		const void* ptr = state->client_state.arrays.interleavedArrays.ptr + indice * stride;

		if (tflag)
		{
			if (tcomps == 2)
				glTexCoord2fv(ptr + toffset);
			else if (tcomps == 4)
				glTexCoord4fv(ptr + toffset);
			else
				assert(0);
		}

		if (cflag)
		{
			if (ctype == GL_FLOAT)
			{
				if (ccomps == 3)
					glColor3fv(ptr + coffset);
				else if (ccomps == 4)
					glColor4fv(ptr + coffset);
				else
					assert(0);
			}
			else if (ctype == GL_UNSIGNED_BYTE)
			{
				if (ccomps == 4)
					glColor4ubv(ptr + coffset);
				else
					assert(0);
			}
			else
				assert(0);
		}

		if (nflag)
			glNormal3fv(ptr + noffset);

		if (vcomps == 2)
			glVertex2fv(ptr + voffset);
		else if (vcomps == 3)
			glVertex3fv(ptr + voffset);
		else if (vcomps == 4)
			glVertex4fv(ptr + voffset);
		else
			assert(0);

		return;
	}

	int i;

	int prevActiveTexture = state->activeTexture;
	for(i=0;i<NB_MAX_TEXTURES;i++)
	{
		if (state->client_state.arrays.texCoordArray[i].enabled)
		{
			if (i != 0 || state->activeTexture != GL_TEXTURE0_ARB)
				glActiveTextureARB(GL_TEXTURE0_ARB + i);
			_glElementArrayImmediate_one(&state->client_state.arrays.texCoordArray[i], indice, TEXCOORD_FUNC);
		}
	}
	glActiveTextureARB(prevActiveTexture);

	_glElementArrayImmediate_one(&state->client_state.arrays.normalArray, indice, NORMAL_FUNC);
	_glElementArrayImmediate_one(&state->client_state.arrays.colorArray, indice, COLOR_FUNC);
	_glElementArrayImmediate_one(&state->client_state.arrays.indexArray, indice, INDEX_FUNC);
	_glElementArrayImmediate_one(&state->client_state.arrays.vertexArray, indice, VERTEX_FUNC);
}

static void _glArraysSend(int first, int last)
{
	GET_CURRENT_STATE();
	if (debug_array_ptr)
	{
		log_gl("_glArraysSend from %d to %d\n", first, last);
	}


	int startIndiceTextureToDealtWith = 0;
	int i;
	int nbElts = 1 + last;

	if (_glIsEnabled(GL_VERTEX_PROGRAM_NV))
	{
		int vertexProgramNV = 0;
		for(i=0;i<MY_GL_MAX_VERTEX_ATTRIBS_NV;i++)
		{
			ClientArray* array = &state->client_state.arrays.vertexAttribArrayNV[i];
			if (!(array->ptr == NULL || array->enabled == 0))
			{
				_glArraySend(state, ARRAY_SEND_FUNC(glVertexAttribPointerNV), array, first, last);
				vertexProgramNV = 1;
			}
		}
		if (vertexProgramNV)
			return;
	}

	if (state->client_state.arrays.interleavedArrays.ptr != NULL &&
			state->client_state.arrays.vertexArray.ptr == NULL)
	{
		int stride;
		if (state->client_state.arrays.interleavedArrays.stride)
			stride = state->client_state.arrays.interleavedArrays.stride;
		else
			stride = _calc_interleaved_arrays_stride(state->client_state.arrays.interleavedArrays.format);
		if (stride == 0) return;
		int offset = stride * first;
		int size = (last - first + 1) * stride;
		long args[] = { offset,
			state->client_state.arrays.interleavedArrays.format,
			state->client_state.arrays.interleavedArrays.stride,
			size,
			POINTER_TO_ARG(state->client_state.arrays.interleavedArrays.ptr + offset) };
		int args_size[] = { 0, 0, 0, 0, size };
		do_opengl_call(glInterleavedArrays_fake_func, NULL, CHECK_ARGS(args, args_size));
		return;
	}

	int normalArrayOffset = (long)state->client_state.arrays.normalArray.ptr -
		(long)state->client_state.arrays.vertexArray.ptr;
	int colorArrayOffset = (long)state->client_state.arrays.colorArray.ptr -
		(long)state->client_state.arrays.vertexArray.ptr;
	int texCoord0PointerOffset = (long)state->client_state.arrays.texCoordArray[0].ptr -
		(long)state->client_state.arrays.vertexArray.ptr;
	int texCoord1PointerOffset = (long)state->client_state.arrays.texCoordArray[1].ptr -
		(long)state->client_state.arrays.vertexArray.ptr;
	int texCoord2PointerOffset = (long)state->client_state.arrays.texCoordArray[2].ptr -
		(long)state->client_state.arrays.vertexArray.ptr;

	if (state->client_state.arrays.vertexArray.vbo_name ||
			state->client_state.arrays.colorArray.vbo_name ||
			state->client_state.arrays.normalArray.vbo_name ||
			state->client_state.arrays.texCoordArray[0].vbo_name ||
			state->client_state.arrays.texCoordArray[1].vbo_name ||
			state->client_state.arrays.texCoordArray[2].vbo_name)
	{
		_glArraySend(state, ARRAY_SEND_FUNC(glVertexPointer), &state->client_state.arrays.vertexArray, first, last);
		_glArraySend(state, ARRAY_SEND_FUNC(glNormalPointer), &state->client_state.arrays.normalArray, first, last);
		_glArraySend(state, ARRAY_SEND_FUNC(glColorPointer), &state->client_state.arrays.colorArray, first, last);
	}
	else
		if (state->client_state.arrays.vertexArray.enabled &&
				state->client_state.arrays.normalArray.enabled == 0 &&
				state->client_state.arrays.colorArray.enabled &&
				state->client_state.arrays.texCoordArray[0].enabled &&
				state->client_state.arrays.vertexArray.ptr != NULL &&
				state->client_state.arrays.vertexArray.stride != 0 &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.colorArray.stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
				colorArrayOffset >= 0 &&
				colorArrayOffset < state->client_state.arrays.vertexArray.stride &&
				texCoord0PointerOffset >= 0 &&
				texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride)
		{
			/* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
			int offset = first * state->client_state.arrays.vertexArray.stride;
			int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

			if (debug_array_ptr)
			{
				log_gl("glVertexColorTexCoord0PointerInterlaced_fake_func sending %d bytes from %d\n",
						data_size,
						offset);
			}
			long args[] = { INT_TO_ARG(offset),
				INT_TO_ARG(state->client_state.arrays.vertexArray.size),
				INT_TO_ARG(state->client_state.arrays.vertexArray.type),
				INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
				INT_TO_ARG(colorArrayOffset),
				INT_TO_ARG(state->client_state.arrays.colorArray.size),
				INT_TO_ARG(state->client_state.arrays.colorArray.type),
				INT_TO_ARG(texCoord0PointerOffset),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
				INT_TO_ARG(data_size),
				POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
			int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
			do_opengl_call(glVertexColorTexCoord0PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

			startIndiceTextureToDealtWith = 1;
		}
		else if (state->client_state.arrays.vertexArray.enabled &&
				state->client_state.arrays.normalArray.enabled &&
				state->client_state.arrays.colorArray.enabled &&
				state->client_state.arrays.texCoordArray[0].enabled &&
				state->client_state.arrays.texCoordArray[1].enabled &&
				state->client_state.arrays.texCoordArray[2].enabled &&
				state->client_state.arrays.vertexArray.ptr != NULL &&
				state->client_state.arrays.vertexArray.stride != 0 &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[1].stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[2].stride &&
				normalArrayOffset >= 0 &&
				normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
				colorArrayOffset >= 0 &&
				colorArrayOffset < state->client_state.arrays.vertexArray.stride &&
				texCoord0PointerOffset >= 0 &&
				texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride &&
				texCoord1PointerOffset >= 0 &&
				texCoord1PointerOffset < state->client_state.arrays.vertexArray.stride &&
				texCoord2PointerOffset >= 0 &&
				texCoord2PointerOffset < state->client_state.arrays.vertexArray.stride)
				{
					/* For unreal tournament 4 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
					int offset = first * state->client_state.arrays.vertexArray.stride;
					int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

					if (debug_array_ptr)
					{
						log_gl("glVertexNormalColorTexCoord012PointerInterlaced_fake_func sending %d bytes from %d\n",
								data_size,
								offset);
					}
					long args[] = { INT_TO_ARG(offset),
						INT_TO_ARG(state->client_state.arrays.vertexArray.size),
						INT_TO_ARG(state->client_state.arrays.vertexArray.type),
						INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
						INT_TO_ARG(normalArrayOffset),
						INT_TO_ARG(state->client_state.arrays.normalArray.type),
						INT_TO_ARG(colorArrayOffset),
						INT_TO_ARG(state->client_state.arrays.colorArray.size),
						INT_TO_ARG(state->client_state.arrays.colorArray.type),
						INT_TO_ARG(texCoord0PointerOffset),
						INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
						INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
						INT_TO_ARG(texCoord1PointerOffset),
						INT_TO_ARG(state->client_state.arrays.texCoordArray[1].size),
						INT_TO_ARG(state->client_state.arrays.texCoordArray[1].type),
						INT_TO_ARG(texCoord2PointerOffset),
						INT_TO_ARG(state->client_state.arrays.texCoordArray[2].size),
						INT_TO_ARG(state->client_state.arrays.texCoordArray[2].type),
						INT_TO_ARG(data_size),
						POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
					int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
					do_opengl_call(glVertexNormalColorTexCoord012PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

					startIndiceTextureToDealtWith = 3;
				}
		else if (state->client_state.arrays.vertexArray.enabled &&
				state->client_state.arrays.normalArray.enabled &&
				state->client_state.arrays.colorArray.enabled &&
				state->client_state.arrays.texCoordArray[0].enabled &&
				state->client_state.arrays.texCoordArray[1].enabled &&
				state->client_state.arrays.vertexArray.ptr != NULL &&
				state->client_state.arrays.vertexArray.stride != 0 &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[1].stride &&
				normalArrayOffset >= 0 &&
				normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
				colorArrayOffset >= 0 &&
				colorArrayOffset < state->client_state.arrays.vertexArray.stride &&
				texCoord0PointerOffset >= 0 &&
				texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride &&
				texCoord1PointerOffset >= 0 &&
				texCoord1PointerOffset < state->client_state.arrays.vertexArray.stride)
		{
			/* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
			int offset = first * state->client_state.arrays.vertexArray.stride;
			int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

			if (debug_array_ptr)
			{
				log_gl("glVertexNormalColorTexCoord01PointerInterlaced_fake_func sending %d bytes from %d\n",
						data_size,
						offset);
			}
			long args[] = { INT_TO_ARG(offset),
				INT_TO_ARG(state->client_state.arrays.vertexArray.size),
				INT_TO_ARG(state->client_state.arrays.vertexArray.type),
				INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
				INT_TO_ARG(normalArrayOffset),
				INT_TO_ARG(state->client_state.arrays.normalArray.type),
				INT_TO_ARG(colorArrayOffset),
				INT_TO_ARG(state->client_state.arrays.colorArray.size),
				INT_TO_ARG(state->client_state.arrays.colorArray.type),
				INT_TO_ARG(texCoord0PointerOffset),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
				INT_TO_ARG(texCoord1PointerOffset),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[1].size),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[1].type),
				INT_TO_ARG(data_size),
				POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
			int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
			do_opengl_call(glVertexNormalColorTexCoord01PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

			startIndiceTextureToDealtWith = 2;
		}
		else if (state->client_state.arrays.vertexArray.enabled &&
				state->client_state.arrays.normalArray.enabled &&
				state->client_state.arrays.colorArray.enabled == 0 &&
				state->client_state.arrays.texCoordArray[0].enabled &&
				state->client_state.arrays.texCoordArray[1].enabled &&
				state->client_state.arrays.texCoordArray[2].enabled &&
				state->client_state.arrays.vertexArray.ptr != NULL &&
				state->client_state.arrays.vertexArray.stride != 0 &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[1].stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[2].stride &&
				normalArrayOffset >= 0 &&
				normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
				texCoord0PointerOffset >= 0 &&
				texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride &&
				texCoord1PointerOffset >= 0 &&
				texCoord1PointerOffset < state->client_state.arrays.vertexArray.stride &&
				texCoord2PointerOffset >= 0 &&
				texCoord2PointerOffset < state->client_state.arrays.vertexArray.stride)
		{
			/* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
			int offset = first * state->client_state.arrays.vertexArray.stride;
			int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

			if (debug_array_ptr)
			{
				log_gl("glVertexNormalTexCoord012PointerInterlaced_fake_func sending %d bytes from %d\n",
						data_size,
						offset);
			}
			long args[] = { INT_TO_ARG(offset),
				INT_TO_ARG(state->client_state.arrays.vertexArray.size),
				INT_TO_ARG(state->client_state.arrays.vertexArray.type),
				INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
				INT_TO_ARG(normalArrayOffset),
				INT_TO_ARG(state->client_state.arrays.normalArray.type),
				INT_TO_ARG(texCoord0PointerOffset),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
				INT_TO_ARG(texCoord1PointerOffset),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[1].size),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[1].type),
				INT_TO_ARG(texCoord2PointerOffset),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[2].size),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[2].type),
				INT_TO_ARG(data_size),
				POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
			int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
			do_opengl_call(glVertexNormalTexCoord012PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

			startIndiceTextureToDealtWith = 3;
		}
		else if (state->client_state.arrays.vertexArray.enabled &&
				state->client_state.arrays.normalArray.enabled &&
				state->client_state.arrays.colorArray.enabled == 0 &&
				state->client_state.arrays.texCoordArray[0].enabled &&
				state->client_state.arrays.texCoordArray[1].enabled &&
				state->client_state.arrays.vertexArray.ptr != NULL &&
				state->client_state.arrays.vertexArray.stride != 0 &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[1].stride &&
				normalArrayOffset >= 0 &&
				normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
				texCoord0PointerOffset >= 0 &&
				texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride &&
				texCoord1PointerOffset >= 0 &&
				texCoord1PointerOffset < state->client_state.arrays.vertexArray.stride)
		{
			/* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
			int offset = first * state->client_state.arrays.vertexArray.stride;
			int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

			if (debug_array_ptr)
			{
				log_gl("glVertexNormalTexCoord01PointerInterlaced_fake_func sending %d bytes from %d\n",
						data_size,
						offset);
			}
			long args[] = { INT_TO_ARG(offset),
				INT_TO_ARG(state->client_state.arrays.vertexArray.size),
				INT_TO_ARG(state->client_state.arrays.vertexArray.type),
				INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
				INT_TO_ARG(normalArrayOffset),
				INT_TO_ARG(state->client_state.arrays.normalArray.type),
				INT_TO_ARG(texCoord0PointerOffset),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
				INT_TO_ARG(texCoord1PointerOffset),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[1].size),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[1].type),
				INT_TO_ARG(data_size),
				POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
			int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
			do_opengl_call(glVertexNormalTexCoord01PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

			startIndiceTextureToDealtWith = 2;
		}
		else if (state->client_state.arrays.vertexArray.enabled &&
				state->client_state.arrays.normalArray.enabled &&
				state->client_state.arrays.colorArray.enabled == 0 &&
				state->client_state.arrays.texCoordArray[0].enabled &&
				state->client_state.arrays.vertexArray.ptr != NULL &&
				state->client_state.arrays.vertexArray.stride != 0 &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
				normalArrayOffset >= 0 &&
				normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
				texCoord0PointerOffset >= 0 &&
				texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride)
		{
			/* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
			int offset = first * state->client_state.arrays.vertexArray.stride;
			int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

			if (debug_array_ptr)
			{
				log_gl("glVertexNormalTexCoord0PointerInterlaced_fake_func sending %d bytes from %d\n",
						data_size,
						offset);
			}
			long args[] = { INT_TO_ARG(offset),
				INT_TO_ARG(state->client_state.arrays.vertexArray.size),
				INT_TO_ARG(state->client_state.arrays.vertexArray.type),
				INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
				INT_TO_ARG(normalArrayOffset),
				INT_TO_ARG(state->client_state.arrays.normalArray.type),
				INT_TO_ARG(texCoord0PointerOffset),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
				INT_TO_ARG(data_size),
				POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
			int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
			do_opengl_call(glVertexNormalTexCoord0PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

			startIndiceTextureToDealtWith = 1;
		}
		else if (state->client_state.arrays.vertexArray.enabled &&
				state->client_state.arrays.normalArray.enabled &&
				state->client_state.arrays.colorArray.enabled &&
				state->client_state.arrays.texCoordArray[0].enabled &&
				state->client_state.arrays.vertexArray.ptr != NULL &&
				state->client_state.arrays.vertexArray.stride != 0 &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.colorArray.stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
				normalArrayOffset >= 0 &&
				normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
				colorArrayOffset >= 0 &&
				colorArrayOffset < state->client_state.arrays.vertexArray.stride &&
				texCoord0PointerOffset >= 0 &&
				texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride)
		{
			/* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
			int offset = first * state->client_state.arrays.vertexArray.stride;
			int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

			if (debug_array_ptr)
			{
				log_gl("glVertexNormalColorTexCoord0PointerInterlaced_fake_func sending %d bytes from %d\n",
						data_size,
						offset);
			}
			long args[] = { INT_TO_ARG(offset),
				INT_TO_ARG(state->client_state.arrays.vertexArray.size),
				INT_TO_ARG(state->client_state.arrays.vertexArray.type),
				INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
				INT_TO_ARG(normalArrayOffset),
				INT_TO_ARG(state->client_state.arrays.normalArray.type),
				INT_TO_ARG(colorArrayOffset),
				INT_TO_ARG(state->client_state.arrays.colorArray.size),
				INT_TO_ARG(state->client_state.arrays.colorArray.type),
				INT_TO_ARG(texCoord0PointerOffset),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
				INT_TO_ARG(data_size),
				POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
			int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
			do_opengl_call(glVertexNormalColorTexCoord0PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

			startIndiceTextureToDealtWith = 1;
		}
		else if (state->client_state.arrays.vertexArray.enabled &&
				state->client_state.arrays.normalArray.enabled &&
				state->client_state.arrays.vertexArray.ptr != NULL &&
				getMulFactorFromPointerArray(&state->client_state.arrays.vertexArray) ==
				getMulFactorFromPointerArray(&state->client_state.arrays.normalArray) &&
				state->client_state.arrays.vertexArray.ptr == state->client_state.arrays.normalArray.ptr)
		{
			/* Special optimization for earth3d */
			int data_size = nbElts * getMulFactorFromPointerArray(&state->client_state.arrays.vertexArray);

			if (debug_array_ptr)
			{
				log_gl("glVertexAndNormalPointer_fake_func sending %d bytes from %d\n",
						data_size,
						0);
			}

			long args[] = { INT_TO_ARG(state->client_state.arrays.vertexArray.size),
				INT_TO_ARG(state->client_state.arrays.vertexArray.type),
				INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
				INT_TO_ARG(state->client_state.arrays.normalArray.type),
				INT_TO_ARG(state->client_state.arrays.normalArray.stride),
				INT_TO_ARG(data_size),
				POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr) };
			int args_size[] = { 0, 0, 0, 0, 0, 0, data_size };
			do_opengl_call(glVertexAndNormalPointer_fake_func, NULL, CHECK_ARGS(args, args_size));
		}

		else if (state->client_state.arrays.vertexArray.enabled &&
				state->client_state.arrays.normalArray.enabled &&
				/*state->client_state.arrays.colorArray.enabled && */
				state->client_state.arrays.vertexArray.ptr != NULL &&
				state->client_state.arrays.vertexArray.stride != 0 &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.colorArray.stride &&
				normalArrayOffset >= 0 &&
				normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
				colorArrayOffset >= 0 &&
				colorArrayOffset < state->client_state.arrays.vertexArray.stride)
		{
			/* Special optimization for tuxracer */
			/*
			   glEnableClientState(GL_VERTEX_ARRAY);
			   glVertexPointer( 3, GL_FLOAT, STRIDE_GL_ARRAY, vnc_array );

			   glEnableClientState(GL_NORMAL_ARRAY);
			   glNormalPointer( GL_FLOAT, STRIDE_GL_ARRAY, 
			   vnc_array + 4*sizeof(GLfloat) );

			   glEnableClientState(GL_COLOR_ARRAY);
			   glColorPointer( 4, GL_UNSIGNED_BYTE, STRIDE_GL_ARRAY, 
			   vnc_array + 8*sizeof(GLfloat) );
			 */
			int offset = first * state->client_state.arrays.vertexArray.stride;
			int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

			if (debug_array_ptr)
			{
				log_gl("glVertexNormalColorPointerInterlaced_fake_func sending %d bytes from %d\n",
						data_size,
						offset);
			}
			long args[] = { INT_TO_ARG(offset),
				INT_TO_ARG(state->client_state.arrays.vertexArray.size),
				INT_TO_ARG(state->client_state.arrays.vertexArray.type),
				INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
				INT_TO_ARG(normalArrayOffset),
				INT_TO_ARG(state->client_state.arrays.normalArray.type),
				INT_TO_ARG(colorArrayOffset),
				INT_TO_ARG(state->client_state.arrays.colorArray.size),
				INT_TO_ARG(state->client_state.arrays.colorArray.type),
				INT_TO_ARG(data_size),
				POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
			int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				data_size};
			do_opengl_call(glVertexNormalColorPointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));
		}
		else if (state->client_state.arrays.vertexArray.enabled &&
				state->client_state.arrays.normalArray.enabled &&
				state->client_state.arrays.vertexArray.ptr != NULL &&
				state->client_state.arrays.vertexArray.stride != 0 &&
				state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
				normalArrayOffset >= 0 &&
				normalArrayOffset < state->client_state.arrays.vertexArray.stride)
		{
			/* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
			int offset = first * state->client_state.arrays.vertexArray.stride;
			int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

			if (debug_array_ptr)
			{
				log_gl("glVertexNormalPointerInterlaced_fake_func sending %d bytes from %d\n",
						data_size,
						offset);
			}
			long args[] = { INT_TO_ARG(offset),
				INT_TO_ARG(state->client_state.arrays.vertexArray.size),
				INT_TO_ARG(state->client_state.arrays.vertexArray.type),
				INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
				INT_TO_ARG(normalArrayOffset),
				INT_TO_ARG(state->client_state.arrays.normalArray.type),
				INT_TO_ARG(data_size),
				POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
			int args_size[] = { 0, 0, 0, 0, 0, 0, 0, data_size};
			do_opengl_call(glVertexNormalPointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

			_glArraySend(state, ARRAY_SEND_FUNC(glColorPointer), &state->client_state.arrays.colorArray, first, last);
		}
		else
		{
			_glArraySend(state, ARRAY_SEND_FUNC(glVertexPointer), &state->client_state.arrays.vertexArray, first, last);
			_glArraySend(state, ARRAY_SEND_FUNC(glNormalPointer), &state->client_state.arrays.normalArray, first, last);
			_glArraySend(state, ARRAY_SEND_FUNC(glColorPointer), &state->client_state.arrays.colorArray, first, last);
		}

	_glArraySend(state, ARRAY_SEND_FUNC(glSecondaryColorPointer), &state->client_state.arrays.secondaryColorArray, first, last);
	_glArraySend(state, ARRAY_SEND_FUNC(glEdgeFlagPointer), &state->client_state.arrays.edgeFlagArray, first, last);
	_glArraySend(state, ARRAY_SEND_FUNC(glIndexPointer), &state->client_state.arrays.indexArray, first, last);

	for(i=0;i<MY_GL_MAX_VERTEX_ATTRIBS_ARB;i++)
	{
		_glArraySend(state, ARRAY_SEND_FUNC(glVertexAttribPointerARB), &state->client_state.arrays.vertexAttribArray[i], first, last);
	}
	for(i=0;i<MY_GL_MAX_VARIANT_POINTER_EXT;i++)
	{
		_glArraySend(state, ARRAY_SEND_FUNC(glVariantPointerEXT), &state->client_state.arrays.variantPointer[i], first, last);
	}

	if (startIndiceTextureToDealtWith == 0 &&
			state->client_state.arrays.texCoordArray[0].vbo_name == 0 &&
			state->client_state.arrays.texCoordArray[1].vbo_name == 0 &&
			state->client_state.arrays.texCoordArray[2].vbo_name == 0 &&
			state->client_state.arrays.texCoordArray[0].enabled &&
			state->client_state.arrays.texCoordArray[1].enabled &&
			state->client_state.arrays.texCoordArray[2].enabled &&
			state->client_state.arrays.texCoordArray[0].ptr &&
			state->client_state.arrays.texCoordArray[0].ptr == state->client_state.arrays.texCoordArray[1].ptr &&
			state->client_state.arrays.texCoordArray[0].size == state->client_state.arrays.texCoordArray[1].size &&
			state->client_state.arrays.texCoordArray[0].type == state->client_state.arrays.texCoordArray[1].type &&
			state->client_state.arrays.texCoordArray[0].stride == state->client_state.arrays.texCoordArray[1].stride &&
			state->client_state.arrays.texCoordArray[0].ptr == state->client_state.arrays.texCoordArray[2].ptr &&
			state->client_state.arrays.texCoordArray[0].size == state->client_state.arrays.texCoordArray[2].size &&
			state->client_state.arrays.texCoordArray[0].type == state->client_state.arrays.texCoordArray[2].type &&
			state->client_state.arrays.texCoordArray[0].stride == state->client_state.arrays.texCoordArray[2].stride)
	{
		/* For unreal tournament 4 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
		int data_size = nbElts * getMulFactorFromPointerArray(&state->client_state.arrays.texCoordArray[0]);
		if (debug_array_ptr)
		{
			log_gl("glTexCoordPointer012_fake_func sending %d bytes from %d\n",
					data_size,
					0);
		}

		long args[] = { INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
			INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
			INT_TO_ARG(state->client_state.arrays.texCoordArray[0].stride),
			INT_TO_ARG(data_size),
			POINTER_TO_ARG(state->client_state.arrays.texCoordArray[0].ptr) };
		int args_size[] = { 0, 0, 0, 0, data_size };
		do_opengl_call(glTexCoordPointer012_fake_func, NULL, CHECK_ARGS(args, args_size));

		startIndiceTextureToDealtWith = 3;
	}
	else
		if (startIndiceTextureToDealtWith == 0 &&
				state->client_state.arrays.texCoordArray[0].vbo_name == 0 &&
				state->client_state.arrays.texCoordArray[1].vbo_name == 0 &&
				state->client_state.arrays.texCoordArray[0].enabled &&
				state->client_state.arrays.texCoordArray[1].enabled &&
				state->client_state.arrays.texCoordArray[0].ptr &&
				state->client_state.arrays.texCoordArray[0].ptr == state->client_state.arrays.texCoordArray[1].ptr &&
				state->client_state.arrays.texCoordArray[0].size == state->client_state.arrays.texCoordArray[1].size &&
				state->client_state.arrays.texCoordArray[0].type == state->client_state.arrays.texCoordArray[1].type &&
				state->client_state.arrays.texCoordArray[0].stride == state->client_state.arrays.texCoordArray[1].stride)
		{
			/* Special optimization for earth3d */
			int data_size = nbElts * getMulFactorFromPointerArray(&state->client_state.arrays.texCoordArray[0]);
			if (debug_array_ptr)
			{
				log_gl("glTexCoordPointer01_fake_func sending %d bytes from %d\n",
						data_size,
						0);
			}

			long args[] = { INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
				INT_TO_ARG(state->client_state.arrays.texCoordArray[0].stride),
				INT_TO_ARG(data_size),
				POINTER_TO_ARG(state->client_state.arrays.texCoordArray[0].ptr) };
			int args_size[] = { 0, 0, 0, 0, data_size };
			do_opengl_call(glTexCoordPointer01_fake_func, NULL, CHECK_ARGS(args, args_size));

			startIndiceTextureToDealtWith = 2;
		}

	for(i=startIndiceTextureToDealtWith;i<NB_MAX_TEXTURES;i++)
	{
		_glArraySend(state, ARRAY_SEND_FUNC(glTexCoordPointer), &state->client_state.arrays.texCoordArray[i], first, last);
	}

	_glArraySend(state, ARRAY_SEND_FUNC(glWeightPointerARB), &state->client_state.arrays.weightArray, first, last);
	_glArraySend(state, ARRAY_SEND_FUNC(glMatrixIndexPointerARB), &state->client_state.arrays.matrixIndexArray, first, last);
	_glArraySend(state, ARRAY_SEND_FUNC(glFogCoordPointer), &state->client_state.arrays.fogCoordArray, first, last);
}

#define ASSERT_COND(cond, val)  do { if (!(val)) { state->lastGlError = val; return; } } while(0)
#define CLEAR_ERROR_STATE()  do { state->lastGlError = 0; } while(0)


GLAPI void APIENTRY EXT_FUNC(glLockArraysEXT) (GLint first, GLsizei count)
{
	/* Quite difficult to be correctly implemented IMHO for a doubtful performance gain */
	/* You need to send data for enabled arrays at this moment, but then for */
	/* the glDrawArrays, glDrawElements, etc... you need to check if enough data has been sent */
	/* For ex consider the following code :
	   glVertexPointer(.....);
	   glColorPointer(......);
	   glNormalPointer(.....);
	   glEnableClientState(GL_VERTEX_ARRAY);
	   glLockArraysEXT(0, 16);                  (1)
	   glEnableClientState(GL_COLOR_ARRAY);
	   glEnableClientState(GL_NORMAL_ARRAY);
	   glDrawArrays(GL_TRIANGLE, 0, 20);        (2)
	   glUnlockArraysEXT();
	 */
	/* At (1), you need to send 16 vertices, but at (2) you need to send the */
	/* 4 following ones, or either the whole 20... */
	CHECK_PROC(glLockArraysEXT);
	GET_CURRENT_STATE();
	ASSERT_COND(first >= 0, GL_INVALID_VALUE);
	ASSERT_COND(count > 0, GL_INVALID_VALUE);
	ASSERT_COND(state->isBetweenLockArrays == 0, GL_INVALID_OPERATION);
	ASSERT_COND(state->isBetweenBegin == 0, GL_INVALID_OPERATION);

	if (debug_array_ptr)
	{
		log_gl("glLockArraysEXT(%d,%d)\n", first, count);
	}

	state->isBetweenLockArrays = 1;

	state->locked_first = first;
	state->locked_count = count;

	CLEAR_ERROR_STATE();
}

GLAPI void APIENTRY EXT_FUNC(glUnlockArraysEXT) ()
{
	CHECK_PROC(glUnlockArraysEXT);
	GET_CURRENT_STATE();
	ASSERT_COND(state->isBetweenLockArrays == 1, GL_INVALID_OPERATION);
	ASSERT_COND(state->isBetweenBegin == 0, GL_INVALID_OPERATION);

	if (debug_array_ptr)
	{
		log_gl("glUnlockArraysEXT()\n");
	}

	state->isBetweenLockArrays = 0;

	CLEAR_ERROR_STATE();
}

GLAPI void APIENTRY glArrayElement( GLint i )
{
	Buffer* buffer = _get_buffer(GL_ARRAY_BUFFER_ARB);
	if (buffer)
	{
		long args[] = { INT_TO_ARG(i) };
		do_opengl_call(glArrayElement_func, NULL, args, NULL);
	}
	else
	{
		_glElementArrayImmediate(i);
	}
}

GLAPI void APIENTRY glDrawArrays( GLenum mode,
		GLint first,
		GLsizei count )
{
	if (debug_array_ptr) log_gl("glDrawArrays(%d,%d,%d)\n", mode, first, count);

	_glArraysSend(first, first + count - 1);

	long args[] = { INT_TO_ARG(mode), INT_TO_ARG(first), INT_TO_ARG(count) };
	do_opengl_call(glDrawArrays_func, NULL, args, NULL);
}

static int _isTuxRacer(GLState* state)
{
	int i;
	ClientArrays* arrays = &state->client_state.arrays;
	if (arrays->vertexArray.vbo_name ||
			arrays->normalArray.vbo_name ||
			arrays->colorArray.vbo_name ||
			arrays->vertexArray.enabled == 0 ||
			arrays->normalArray.enabled == 0)
		return 0;

	int normalArrayOffset = (long)arrays->normalArray.ptr - (long)arrays->vertexArray.ptr;
	int colorArrayOffset = (long)arrays->colorArray.ptr - (long)arrays->vertexArray.ptr;

	if (!(arrays->vertexArray.ptr != NULL &&
				arrays->vertexArray.stride != 0 &&
				arrays->vertexArray.stride == arrays->normalArray.stride &&
				arrays->vertexArray.stride == arrays->colorArray.stride &&
				normalArrayOffset >= 0 &&
				normalArrayOffset < arrays->vertexArray.stride &&
				colorArrayOffset >= 0 &&
				colorArrayOffset < arrays->vertexArray.stride))
		return 0;

	if (!(arrays->vertexArray.type == GL_FLOAT && arrays->vertexArray.size == 3 &&
				arrays->normalArray.type == GL_FLOAT &&
				arrays->colorArray.type == GL_UNSIGNED_BYTE && arrays->colorArray.size == 4))
		return 0;

	if (arrays->secondaryColorArray.enabled ||
			arrays->indexArray.enabled ||
			arrays->edgeFlagArray.enabled ||
			arrays->weightArray.enabled ||
			arrays->matrixIndexArray.enabled ||
			arrays->fogCoordArray.enabled)
		return 0;

	for(i=0;i<NB_MAX_TEXTURES;i++)
	{
		if (arrays->texCoordArray[i].enabled)
			return 0;
	}
	for(i=0;i<MY_GL_MAX_VERTEX_ATTRIBS_ARB;i++)
	{
		if (arrays->vertexAttribArray[i].enabled)
			return 0;
	}
	return 1;
}

static int _check_if_enabled_non_vbo_array()
{
	int i;
	GET_CURRENT_STATE();
	ClientArrays* arrays = &state->client_state.arrays;

	if (arrays->vertexArray.vbo_name == 0 && arrays->vertexArray.enabled)
		return 1;
	if (arrays->normalArray.vbo_name == 0 && arrays->normalArray.enabled)
		return 1;
	if (arrays->colorArray.vbo_name == 0 && arrays->colorArray.enabled)
		return 1;
	if (arrays->secondaryColorArray.vbo_name == 0 && arrays->secondaryColorArray.enabled)
		return 1;
	if (arrays->indexArray.vbo_name == 0 && arrays->indexArray.enabled)
		return 1;
	if (arrays->edgeFlagArray.vbo_name == 0 && arrays->edgeFlagArray.enabled)
		return 1;
	if (arrays->weightArray.vbo_name == 0 && arrays->weightArray.enabled)
		return 1;
	if (arrays->matrixIndexArray.vbo_name == 0 && arrays->matrixIndexArray.enabled)
		return 1;
	if (arrays->fogCoordArray.vbo_name == 0 && arrays->fogCoordArray.enabled)
		return 1;
	for(i=0;i<NB_MAX_TEXTURES;i++)
	{
		if (arrays->texCoordArray[i].vbo_name == 0 && arrays->texCoordArray[i].enabled)
			return 1;
	}
	for(i=0;i<MY_GL_MAX_VERTEX_ATTRIBS_ARB;i++)
	{
		if (arrays->vertexAttribArray[i].vbo_name == 0 && arrays->vertexAttribArray[i].enabled)
			return 1;
	}
	for(i=0;i<MY_GL_MAX_VARIANT_POINTER_EXT;i++)
	{
		if (arrays->variantPointer[i].vbo_name == 0 && arrays->variantPointer[i].enabled)
			return 1;
	}

	return 0;
}

GLAPI void APIENTRY glDrawElements( GLenum mode,
		GLsizei count,
		GLenum type,
		const GLvoid *indices )
{
	CHECK_PROC(glDrawElements);
	int i;
	int size = count;
	//Buffer* bufferArray;
	Buffer* bufferElement;

	if (debug_array_ptr) log_gl("glDrawElements(%d,%d,%d,%p)\n", mode, count, type, indices);

	if (count == 0) return;

	//bufferArray = _get_buffer(GL_ARRAY_BUFFER_ARB);
	bufferElement = _get_buffer(GL_ELEMENT_ARRAY_BUFFER_ARB);
	if (bufferElement)
	{
		if (_check_if_enabled_non_vbo_array())
		{
			log_gl("sorry : unsupported : glDrawElements in EBO with a non VBO array enabled\n");
			return;
		}
		long args[] = { INT_TO_ARG(mode), INT_TO_ARG(count), INT_TO_ARG(type), POINTER_TO_ARG(indices) };
		do_opengl_call(_glDrawElements_buffer_func, NULL, args, NULL);
		return;
	}


	int minIndice = 0;
	int maxIndice = -1;

	switch(type)
	{
		case GL_UNSIGNED_BYTE:
			{
				//if (!bufferArray)
				{
					unsigned char* tab_indices = (unsigned char*)indices;
					minIndice = tab_indices[0];
					maxIndice = tab_indices[0];
					for(i=1;i<count;i++)
					{
						if (tab_indices[i] < minIndice) minIndice = tab_indices[i];
						if (tab_indices[i] > maxIndice) maxIndice = tab_indices[i];
					}
				}
				break;
			}

		case GL_UNSIGNED_SHORT:
			{
				size *= 2;
				//if (!bufferArray)
				{
					unsigned short* tab_indices = (unsigned short*)indices;
					minIndice = tab_indices[0];
					maxIndice = tab_indices[0];
					for(i=1;i<count;i++)
					{
						if (tab_indices[i] < minIndice) minIndice = tab_indices[i];
						if (tab_indices[i] > maxIndice) maxIndice = tab_indices[i];
					}
				}
				break;
			}

		case GL_UNSIGNED_INT:
			{
				size *= 4;
				//if (!bufferArray)
				{
					unsigned int* tab_indices = (unsigned int*)indices;
					minIndice = tab_indices[0];
					maxIndice = tab_indices[0];
					for(i=1;i<count;i++)
					{
						if (tab_indices[i] < minIndice) minIndice = tab_indices[i];
						if (tab_indices[i] > maxIndice) maxIndice = tab_indices[i];
					}
					//log_gl("maxIndice = %d\n", maxIndice);
					if (maxIndice > 100 * 1024 * 1024)
					{
						log_gl("too big indice : %d\n",maxIndice);
						return;
					}
				}
				break;
			}

		default:
			log_gl("unsupported type = %d\n", type);
			return;
	}

	GET_CURRENT_STATE();
	if (_isTuxRacer(state) && type == GL_UNSIGNED_INT)
	{
		/* Special optimization for tuxracer */
		/*
		   glEnableClientState(GL_VERTEX_ARRAY);
		   glVertexPointer( 3, GL_FLOAT, STRIDE_GL_ARRAY, vnc_array );

		   glEnableClientState(GL_NORMAL_ARRAY);
		   glNormalPointer( GL_FLOAT, STRIDE_GL_ARRAY, 
		   vnc_array + 4*sizeof(GLfloat) );

		   glEnableClientState(GL_COLOR_ARRAY);
		   glColorPointer( 4, GL_UNSIGNED_BYTE, STRIDE_GL_ARRAY, 
		   vnc_array + 8*sizeof(GLfloat) );
		 */
		ClientArrays* arrays = &state->client_state.arrays;
		int stride = arrays->vertexArray.stride;
		int bufferSize = (maxIndice - minIndice + 1) * stride + sizeof(int) * count;
		int eltSize = 6 * sizeof(float) + ((arrays->colorArray.enabled) ? 4 * sizeof(unsigned char) : 0);
		int singleCommandSize = count * eltSize;
		if (count < bufferSize)
		{
			unsigned int* tab_indices = (unsigned int*)indices;
			const char* vertexArray = (const char*)arrays->vertexArray.ptr;
			int normalArrayOffset = (long)arrays->normalArray.ptr - (long)arrays->vertexArray.ptr;
			int colorArrayOffset = (long)arrays->colorArray.ptr - (long)arrays->vertexArray.ptr;
			state->tuxRacerBuffer = realloc(state->tuxRacerBuffer, singleCommandSize);

			for(i=0;i<count;i++)
			{
				int ind = tab_indices[i];
				memcpy(&state->tuxRacerBuffer[i * eltSize], &vertexArray[ind * stride], 3 * sizeof(float));
				memcpy(&state->tuxRacerBuffer[i * eltSize + 3 * sizeof(float)],
						&vertexArray[ind * stride + normalArrayOffset], 3 * sizeof(float));
				if (arrays->colorArray.enabled)
					memcpy(&state->tuxRacerBuffer[i * eltSize + 6 * sizeof(float)],
							&vertexArray[ind * stride + colorArrayOffset], 4 * sizeof(unsigned char));
			}

			long args[] = { INT_TO_ARG(mode), INT_TO_ARG(count), INT_TO_ARG(arrays->colorArray.enabled), POINTER_TO_ARG(state->tuxRacerBuffer) };
			int args_size[] = { 0, 0, 0, singleCommandSize} ;
			do_opengl_call(glTuxRacerDrawElements_fake_func, NULL, CHECK_ARGS(args, args_size));
			return;
		}
	}
	_glArraysSend(minIndice, maxIndice);

	long args[] = { INT_TO_ARG(mode), INT_TO_ARG(count), INT_TO_ARG(type), POINTER_TO_ARG(indices) };
	int args_size[] = { 0, 0, 0, (indices) ? size : 0 } ;
	do_opengl_call(glDrawElements_func, NULL, CHECK_ARGS(args, args_size));
}


GLAPI void APIENTRY glDrawRangeElements( GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices )
{
	CHECK_PROC(glDrawRangeElements);
	int size = count;
	Buffer* bufferElement;

	if (debug_array_ptr) log_gl("glDrawRangeElements(0x%X, %d, %d, %d, %d, %p)\n", mode, start, end, count, type, indices);

	if (count == 0) return;

	/* Yes : try to send regular arrays even if GL_ELEMENT_ARRAY_BUFFER_ARB is enabled.
	   This is necessary for nexuiz 2.3 where in GL_ELEMENT_ARRAY_BUFFER_ARB some arrays are regular array pointers,
	   and some others are stored in VBO... */
	_glArraysSend(start, end);

	bufferElement = _get_buffer(GL_ELEMENT_ARRAY_BUFFER_ARB);
	if (bufferElement)
	{
		long args[] = { INT_TO_ARG(mode), INT_TO_ARG(start), INT_TO_ARG(end), INT_TO_ARG(count), INT_TO_ARG(type), POINTER_TO_ARG(indices) };
		do_opengl_call(_glDrawRangeElements_buffer_func, NULL, args, NULL);
		return;
	}
#if 0
	int i;
	switch(type)
	{
		case GL_UNSIGNED_BYTE:
			{
				unsigned char* tab_indices = (unsigned char*)indices;
				for(i=0;i<count;i++)
				{
					if (tab_indices[i] < start || tab_indices[i] > end)
					{
						log_gl("indice out of bounds at offset %d\n", i);
					}
				}
				break;
			}

		case GL_UNSIGNED_SHORT:
			{
				size *= 2;
				unsigned short* tab_indices = (unsigned short*)indices;
				for(i=0;i<count;i++)
				{
					if (tab_indices[i] < start || tab_indices[i] > end)
					{
						log_gl("indice out of bounds at offset %d\n", i);
					}
				}
				break;
			}

		case GL_UNSIGNED_INT:
			{
				size *= 4;
				unsigned int* tab_indices = (unsigned int*)indices;
				for(i=0;i<count;i++)
				{
					if (tab_indices[i] < start || tab_indices[i] > end)
					{
						log_gl("indice out of bounds at offset %d\n", i);
					}
				}
				break;
			}

		default:
			log_gl("unsupported type = %d\n", type);
			return;
	}
#else
	switch(type)
	{
		case GL_UNSIGNED_BYTE:
			{
			}

		case GL_UNSIGNED_SHORT:
			{
				size *= 2;
				break;
			}

		case GL_UNSIGNED_INT:
			{
				size *= 4;
				break;
			}

		default:
			log_gl("unsupported type = %d\n", type);
			return;
	}
#endif
	long args[] = { INT_TO_ARG(mode), INT_TO_ARG(start), INT_TO_ARG(end),
		INT_TO_ARG(count), INT_TO_ARG(type), POINTER_TO_ARG(indices) };
	int args_size[] = { 0, 0, 0, 0, 0, size } ;
	do_opengl_call(glDrawRangeElements_func, NULL, CHECK_ARGS(args, args_size));

}

DEFINE_EXT(glDrawRangeElements, ( GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices ),
		(mode, start, end, count, type, indices) )


GLAPI void APIENTRY glMultiDrawArrays( GLenum mode, GLint *first,
		GLsizei *count, GLsizei primcount )
{
	GLint i;

	for (i = 0; i < primcount; i++) {
		if (count[i] > 0) {
			glDrawArrays(mode, first[i], count[i]);
		}
	}
}

DEFINE_EXT(glMultiDrawArrays, ( GLenum mode, GLint *first,
			GLsizei *count, GLsizei primcount ),
		(mode, first, count, primcount) )

GLAPI void APIENTRY glMultiDrawElements( GLenum mode, const GLsizei *count, GLenum type,
		const GLvoid **indices, GLsizei primcount )
{
	GLint i;
	CHECK_PROC(glMultiDrawElements);

	Buffer* bufferElement = _get_buffer(GL_ELEMENT_ARRAY_BUFFER_ARB);
	if (bufferElement && primcount > 1)
	{
		if (_check_if_enabled_non_vbo_array())
		{
			log_gl("sorry : unsupported : glMultiDrawElements in EBO with a non VBO array enabled\n");
			return;
		}
		long args[] = { INT_TO_ARG(mode), POINTER_TO_ARG(count), INT_TO_ARG(type),
			POINTER_TO_ARG(indices), INT_TO_ARG(primcount) };
		int args_size[] = { 0, primcount * sizeof(int), 0, primcount * sizeof(void*), 0 };
		do_opengl_call(_glMultiDrawElements_buffer_func, NULL, args, args_size);
		return;
	}

	for (i = 0; i < primcount; i++) {
		if (count[i] > 0) {
			glDrawElements(mode, count[i], type, indices[i]);
		}
	}
}

DEFINE_EXT(glMultiDrawElements, ( GLenum mode, const GLsizei *count, GLenum type,
			const GLvoid **indices, GLsizei primcount ), 
		(mode, count, type, indices, primcount) )


#if 0
GLAPI void APIENTRY glArrayObjectATI (GLenum array, GLint size, GLenum type, GLsizei stride, GLuint buffer, GLuint offset)
{
	CHECK_PROC(glArrayObjectATI);
	//log_gl("glArrayObjectATI(array=%p, size=%d, type=%p, stride=%d, buffer=%d, offset=%d)\n", array, size, type, stride, buffer, offset);
	long args[] = { UNSIGNED_INT_TO_ARG(array), INT_TO_ARG(size), UNSIGNED_INT_TO_ARG(type), INT_TO_ARG(stride), UNSIGNED_INT_TO_ARG(buffer), UNSIGNED_INT_TO_ARG(offset)};
	do_opengl_call(glArrayObjectATI_func, NULL, args, NULL);
}
#endif

GLAPI void APIENTRY glDrawElementArrayATI (GLenum mode, GLsizei count)
{
	CHECK_PROC(glDrawElementArrayATI);
	GET_CURRENT_STATE();

	if (state->client_state.arrays.elementArrayATI.enabled &&
			state->client_state.arrays.elementArrayATI.ptr != NULL)
	{
		int size = count * getMulFactorFromPointerArray(&state->client_state.arrays.elementArrayATI);
		long args[] = { INT_TO_ARG(state->client_state.arrays.elementArrayATI.type),
			INT_TO_ARG(size),
			INT_TO_ARG(state->client_state.arrays.elementArrayATI.ptr) };
		int args_size[] = {0, 0, size};
		do_opengl_call(glElementPointerATI_fake_func, NULL, CHECK_ARGS(args, args_size));
	}

	long args[] = { INT_TO_ARG(mode), INT_TO_ARG(count) };
	do_opengl_call(glDrawElementArrayATI_func, NULL, args, NULL);
}

GLAPI void APIENTRY glDrawRangeElementArrayATI (GLenum mode, GLuint start, GLuint end, GLsizei count)
{
	CHECK_PROC(glDrawRangeElementArrayATI);
	GET_CURRENT_STATE();

	if (state->client_state.arrays.elementArrayATI.enabled &&
			state->client_state.arrays.elementArrayATI.ptr != NULL)
	{
		int size = count * getMulFactorFromPointerArray(&state->client_state.arrays.elementArrayATI);
		long args[] = { INT_TO_ARG(state->client_state.arrays.elementArrayATI.type),
			INT_TO_ARG(size),
			INT_TO_ARG(state->client_state.arrays.elementArrayATI.ptr) };
		int args_size[] = {0, 0, size};
		do_opengl_call(glElementPointerATI_fake_func, NULL, CHECK_ARGS(args, args_size));
	}

	long args[] = { INT_TO_ARG(mode), INT_TO_ARG(start), INT_TO_ARG(end), INT_TO_ARG(count) };
	do_opengl_call(glDrawRangeElementArrayATI_func, NULL, args, NULL);
}

GLAPI GLuint APIENTRY glGenSymbolsEXT(GLenum datatype, GLenum storagetype, GLenum range, GLuint components)
{
	GLuint id = 0;
	CHECK_PROC_WITH_RET(glGenSymbolsEXT);
	GET_CURRENT_STATE();
	long args[] = { datatype, storagetype, range, components };
	do_opengl_call(glGenSymbolsEXT_func, &id, args, NULL);
	if (id != 0)
	{
		state->symbols.tab = realloc(state->symbols.tab, (state->symbols.count+1) * sizeof(Symbol));
		state->symbols.tab[state->symbols.count].id = id;
		state->symbols.tab[state->symbols.count].datatype = datatype;
		state->symbols.tab[state->symbols.count].components = components;
		state->symbols.count++;
	}
	return id;
}

static int get_vertex_shader_var_nb_composants(GLuint id)
{
	GET_CURRENT_STATE();
	int i;
	for(i=0;i<state->symbols.count;i++)
	{
		if (id >= state->symbols.tab[i].id && id < state->symbols.tab[i].id + state->symbols.tab[i].components)
		{
			int size = 0;

			if (state->symbols.tab[i].datatype == GL_SCALAR_EXT)
				size = 1;
			else if (state->symbols.tab[i].datatype == GL_VECTOR_EXT)
				size = 4;
			else if (state->symbols.tab[i].datatype == GL_MATRIX_EXT)
				size = 16;
			else
			{
				log_gl("unknown datatype %d\n", state->symbols.tab[i].datatype);
				return 0;
			}

			return size;
		}
	}
	log_gl("unknown id %d\n", id);
	return 0;
}

GLAPI void APIENTRY glSetLocalConstantEXT (GLuint id, GLenum type, const GLvoid *addr)
{
	CHECK_PROC(glSetLocalConstantEXT);
	int size = get_vertex_shader_var_nb_composants(id) * getGlTypeByteSize(type);
	if (size)
	{
		long args[] = { id, type, POINTER_TO_ARG(addr) };
		int args_size[] = { 0, 0, size };

		do_opengl_call(glSetLocalConstantEXT_func, NULL, CHECK_ARGS(args, args_size));
	}
}

GLAPI void APIENTRY glSetInvariantEXT (GLuint id, GLenum type, const GLvoid *addr)
{
	CHECK_PROC(glSetInvariantEXT);
	int size = get_vertex_shader_var_nb_composants(id) * getGlTypeByteSize(type);
	if (size)
	{
		long args[] = { id, type, POINTER_TO_ARG(addr) };
		int args_size[] = { 0, 0, size };

		do_opengl_call(glSetInvariantEXT_func, NULL, CHECK_ARGS(args, args_size));
	}
}

#define glVariantGeneric(func_name, gltype) \
	GLAPI void APIENTRY func_name (GLuint id, const gltype * addr)\
{\
	CHECK_PROC(func_name); \
	int size = get_vertex_shader_var_nb_composants(id) * sizeof(gltype); \
	if (size) \
	{ \
		long args[] = { id, POINTER_TO_ARG(addr) }; \
		int args_size[] = { 0, size }; \
		do_opengl_call(CONCAT(func_name,_func), NULL, CHECK_ARGS(args, args_size)); \
	} \
}

glVariantGeneric(glVariantbvEXT, GLbyte);
glVariantGeneric(glVariantsvEXT, GLshort);
glVariantGeneric(glVariantivEXT, GLint);
glVariantGeneric(glVariantfvEXT, GLfloat);
glVariantGeneric(glVariantdvEXT, GLdouble);
glVariantGeneric(glVariantubvEXT, GLubyte);
glVariantGeneric(glVariantusvEXT, GLushort);
glVariantGeneric(glVariantuivEXT, GLuint);

GLAPI void APIENTRY glEnableVariantClientStateEXT (GLuint id)
{
	CHECK_PROC(glEnableVariantClientStateEXT);
	long args[] = { id };
	do_opengl_call(glEnableVariantClientStateEXT_func, NULL, args, NULL);
	if (id < MY_GL_MAX_VARIANT_POINTER_EXT)
	{
		GET_CURRENT_STATE();
		state->client_state.arrays.variantPointer[id].enabled = 1;
	}
}

GLAPI void APIENTRY glDisableVariantClientStateEXT (GLuint id)
{
	CHECK_PROC(glDisableVariantClientStateEXT);
	long args[] = { id };
	do_opengl_call(glDisableVariantClientStateEXT_func, NULL, args, NULL);
	if (id < MY_GL_MAX_VARIANT_POINTER_EXT)
	{
		GET_CURRENT_STATE();
		state->client_state.arrays.variantPointer[id].enabled = 0;
	}
}

GLAPI void APIENTRY glVariantPointerEXT (GLuint id, GLenum type, GLuint stride, const GLvoid *ptr)
{
	CHECK_PROC(glVariantPointerEXT);

	GET_CURRENT_STATE();
	if (id < MY_GL_MAX_VARIANT_POINTER_EXT)
	{
		state->client_state.arrays.variantPointer[id].vbo_name = state->arrayBuffer;
		if (state->client_state.arrays.variantPointer[id].vbo_name)
		{
			long args[] = { INT_TO_ARG(id), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
			do_opengl_call(_glVariantPointerEXT_buffer_func, NULL, args, NULL);
			return;
		}

		if (debug_array_ptr)
			log_gl("glVariantPointerEXT[%d] type=%dstride=%d ptr=%p\n",
					id, type, stride, ptr);
		state->client_state.arrays.variantPointer[id].index = id;
		state->client_state.arrays.variantPointer[id].size = 4;
		state->client_state.arrays.variantPointer[id].type = type;
		state->client_state.arrays.variantPointer[id].stride = stride;
		state->client_state.arrays.variantPointer[id].ptr = ptr;
	}
	else
	{
		log_gl("id >= MY_GL_MAX_VARIANT_POINTER_EXT\n");
	}
}

#define glGetVariantGeneric(func_name, gltype) \
	GLAPI void APIENTRY func_name (GLuint id, GLenum name, gltype* addr)\
{\
	CHECK_PROC(func_name); \
	int size = (name == GL_VARIANT_VALUE_EXT) ? get_vertex_shader_var_nb_composants(id) * sizeof(gltype) : sizeof(gltype); \
	if (size) \
	{ \
		long args[] = { id, name, POINTER_TO_ARG(addr) }; \
		int args_size[] = { 0, 0, size }; \
		do_opengl_call(CONCAT(func_name,_func), NULL, CHECK_ARGS(args, args_size)); \
	} \
}

glGetVariantGeneric(glGetVariantBooleanvEXT, GLboolean);
glGetVariantGeneric(glGetVariantIntegervEXT, GLint);
glGetVariantGeneric(glGetVariantFloatvEXT, GLfloat);

GLAPI void APIENTRY glGetVariantPointervEXT (GLuint id, GLenum name, GLvoid* *addr)
{
	CHECK_PROC(glGetVariantPointervEXT);

	GET_CURRENT_STATE();
	if (id < MY_GL_MAX_VARIANT_POINTER_EXT)
	{
		if (name == GL_VARIANT_ARRAY_POINTER_EXT)
			*addr = (void*)state->client_state.arrays.variantPointer[id].ptr;
	}
	else
	{
		log_gl("id >= MY_GL_MAX_VARIANT_POINTER_EXT\n");
	}
}

#define glGetInvariantGeneric(func_name, gltype) \
	GLAPI void APIENTRY func_name (GLuint id, GLenum name, gltype* addr)\
{\
	CHECK_PROC(func_name); \
	int size = (name == GL_INVARIANT_VALUE_EXT) ? get_vertex_shader_var_nb_composants(id) * sizeof(gltype) : sizeof(gltype); \
	if (size) \
	{ \
		long args[] = { id, name, POINTER_TO_ARG(addr) }; \
		int args_size[] = { 0, 0, size }; \
		do_opengl_call(CONCAT(func_name,_func), NULL, CHECK_ARGS(args, args_size)); \
	} \
}

glGetInvariantGeneric(glGetInvariantBooleanvEXT, GLboolean);
glGetInvariantGeneric(glGetInvariantIntegervEXT, GLint);
glGetInvariantGeneric(glGetInvariantFloatvEXT, GLfloat);

#define glGetLocalConstantGeneric(func_name, gltype) \
	GLAPI void APIENTRY func_name (GLuint id, GLenum name, gltype* addr)\
{\
	CHECK_PROC(func_name); \
	int size = (name == GL_LOCAL_CONSTANT_VALUE_EXT) ? get_vertex_shader_var_nb_composants(id) * sizeof(gltype) : sizeof(gltype); \
	if (size) \
	{ \
		long args[] = { id, name, POINTER_TO_ARG(addr) }; \
		int args_size[] = { 0, 0, size }; \
		do_opengl_call(CONCAT(func_name,_func), NULL, CHECK_ARGS(args, args_size)); \
	} \
}

glGetLocalConstantGeneric(glGetLocalConstantBooleanvEXT, GLboolean);
glGetLocalConstantGeneric(glGetLocalConstantIntegervEXT, GLint);
glGetLocalConstantGeneric(glGetLocalConstantFloatvEXT, GLfloat);


static void _glShaderSource(int func_number, GLhandleARB handle, GLsizei size, const GLcharARB** tab_prog, const GLint* tab_length)
{
	int i;
	int* my_tab_length;
	int total_length = 0;
	int acc_length = 0;
	GLcharARB* all_progs;

	if (size <= 0 || tab_prog == NULL)
	{
		log_gl("size <= 0 || tab_prog == NULL\n");
		return;
	}
	my_tab_length = malloc(sizeof(int) * size);
	for(i=0;i<size;i++)
	{
		if (tab_prog[i] == NULL)
		{
			log_gl("tab_prog[%d] == NULL\n", i);
			free(my_tab_length);
			return ;
		}
		my_tab_length[i] = (tab_length && tab_length[i]) ? tab_length[i] : strlen(tab_prog[i]);
		total_length += my_tab_length[i];
	}
	all_progs = malloc(total_length+1);
	for(i=0;i<size;i++)
	{
		char* str_tmp = all_progs + acc_length;
		memcpy(str_tmp, tab_prog[i], my_tab_length[i]);
		str_tmp[my_tab_length[i]] = 0;
		if (debug_gl) log_gl("glShaderSource[%d] : %s\n", i, str_tmp);
		char* version_ptr = strstr(str_tmp, "#version");
		if (version_ptr && version_ptr != str_tmp)
		{
			/* ATI driver won't be happy if "#version" is not at beginning of program */
			/* Necessary for "Danger from the Deep 0.3.0" */
			int offset = version_ptr - str_tmp;
			char* eol = strchr(version_ptr, '\n');
			if (eol)
			{
				int len = eol - version_ptr + 1;
				memcpy(str_tmp, tab_prog[i] + offset, len);
				memcpy(str_tmp + len, tab_prog[i], offset);
			}
		}
		acc_length += my_tab_length[i];
	}
	long args[] = { INT_TO_ARG(handle), INT_TO_ARG(size), POINTER_TO_ARG(all_progs), POINTER_TO_ARG(my_tab_length) } ;
	int args_size[] = { 0, 0, total_length, sizeof(int) * size };
	do_opengl_call(func_number, NULL, CHECK_ARGS(args, args_size));
	free(my_tab_length);
	free(all_progs);
}


GLAPI void APIENTRY EXT_FUNC(glShaderSourceARB) (GLhandleARB handle, GLsizei size, const GLcharARB** tab_prog, const GLint* tab_length)
{
	CHECK_PROC(glShaderSourceARB);
	_glShaderSource(glShaderSourceARB_fake_func, handle, size, tab_prog, tab_length);
}

GLAPI void APIENTRY EXT_FUNC(glShaderSource) (GLhandleARB handle, GLsizei size, const GLcharARB** tab_prog, const GLint* tab_length)
{
	CHECK_PROC(glShaderSource);
	_glShaderSource(glShaderSource_fake_func, handle, size, tab_prog, tab_length);
}

GLAPI void APIENTRY EXT_FUNC(glGetProgramInfoLog)(GLuint program,
		GLsizei maxLength,
		GLsizei *length,
		GLchar *infoLog)
{
	CHECK_PROC(glGetProgramInfoLog);
	int fake_length;
	if (length == NULL) length = &fake_length;
	long args[] = { INT_TO_ARG(program), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(infoLog) };
	int args_size[] = { 0, 0, sizeof(int), maxLength };
	do_opengl_call(glGetProgramInfoLog_func, NULL, CHECK_ARGS(args, args_size));
	log_gl("glGetProgramInfoLog: %s\n", infoLog);
}


GLAPI void APIENTRY EXT_FUNC(glGetProgramStringARB) (GLenum target, GLenum pname, GLvoid *string)
{
	int size = 0;
	CHECK_PROC(glGetProgramStringARB);
	glGetProgramivARB(target, GL_PROGRAM_LENGTH_ARB, &size);
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(pname), POINTER_TO_ARG(string) };
	int args_size[] = { 0, 0, size };
	do_opengl_call(glGetProgramStringARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glGetProgramStringNV) (GLenum target, GLenum pname, GLvoid *string)
{
	int size = 0;
	CHECK_PROC(glGetProgramStringNV);
	glGetProgramivNV(target, GL_PROGRAM_LENGTH_NV, &size);
	long args[] = { INT_TO_ARG(target), INT_TO_ARG(pname), POINTER_TO_ARG(string) };
	int args_size[] = { 0, 0, size };
	do_opengl_call(glGetProgramStringNV_func, NULL, CHECK_ARGS(args, args_size));
}



GLAPI void APIENTRY EXT_FUNC(glGetInfoLogARB)(GLhandleARB object,
		GLsizei maxLength,
		GLsizei *length,
		GLcharARB *infoLog)
{
	CHECK_PROC(glGetInfoLogARB);
	int fake_length;
	if (length == NULL) length = &fake_length;
	long args[] = { INT_TO_ARG(object), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(infoLog) };
	/*int size = 0;
	  glGetObjectParameterARBiv(object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &size);*/
	int args_size[] = { 0, 0, sizeof(int), maxLength };
	do_opengl_call(glGetInfoLogARB_func, NULL, CHECK_ARGS(args, args_size));
	log_gl("glGetInfoLogARB : %s\n", infoLog);
}

GLAPI void APIENTRY EXT_FUNC(glGetAttachedObjectsARB)(GLhandleARB program,
		GLsizei maxCount,
		GLsizei *count,
		GLhandleARB *objects)
{
	CHECK_PROC(glGetAttachedObjectsARB);
	int fake_count;
	if (count == NULL) count = &fake_count;
	long args[] = { INT_TO_ARG(program), INT_TO_ARG(maxCount), POINTER_TO_ARG(count), POINTER_TO_ARG(objects) };
	/*int size = 0;
	  glGetObjectParameterARBiv(object, GL_OBJECT_ATTACHED_OBJECTS_ARB, &size);*/
	int args_size[] = { 0, 0, sizeof(int), maxCount * sizeof(int) };
	do_opengl_call(glGetAttachedObjectsARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glGetAttachedShaders)(GLuint program,
		GLsizei maxCount,
		GLsizei *count,
		GLuint *shaders)
{
	CHECK_PROC(glGetAttachedShaders);
	int fake_count;
	if (count == NULL) count = &fake_count;
	long args[] = { INT_TO_ARG(program), INT_TO_ARG(maxCount), POINTER_TO_ARG(count), POINTER_TO_ARG(shaders) };
	int args_size[] = { 0, 0, sizeof(int), maxCount * sizeof(int) };
	do_opengl_call(glGetAttachedShaders_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI GLint EXT_FUNC(glGetUniformLocationARB) (GLuint program, const GLcharARB *txt)
{
	int i;
	int ret = -1;
	GET_CURRENT_STATE();
	for(i=0;i<state->countUniformLocations;i++)
	{
		if (state->uniformLocations[i].program == program &&
				strcmp(state->uniformLocations[i].txt, txt) == 0)
		{
			return state->uniformLocations[i].location;
		}
	}

	CHECK_PROC_WITH_RET(glGetUniformLocationARB);
	long args[] = { INT_TO_ARG(program), POINTER_TO_ARG(txt) } ;
	do_opengl_call(glGetUniformLocationARB_func, &ret, args, NULL);

	state->uniformLocations = realloc(state->uniformLocations, sizeof(UniformLocation) * (state->countUniformLocations+1));
	state->uniformLocations[state->countUniformLocations].program = program;
	state->uniformLocations[state->countUniformLocations].txt = strdup(txt);
	state->uniformLocations[state->countUniformLocations].location = ret;
	state->countUniformLocations++;

	return ret;
}

GLAPI GLint EXT_FUNC(glGetUniformLocation) (GLuint program, const GLcharARB *txt)
{
	int i;
	int ret = -1;
	GET_CURRENT_STATE();
	for(i=0;i<state->countUniformLocations;i++)
	{
		if (state->uniformLocations[i].program == program &&
				strcmp(state->uniformLocations[i].txt, txt) == 0)
		{
			return state->uniformLocations[i].location;
		}
	}

	CHECK_PROC_WITH_RET(glGetUniformLocation);
	long args[] = { INT_TO_ARG(program), POINTER_TO_ARG(txt) } ;
	do_opengl_call(glGetUniformLocation_func, &ret, args, NULL);

	state->uniformLocations = realloc(state->uniformLocations, sizeof(UniformLocation) * (state->countUniformLocations+1));
	state->uniformLocations[state->countUniformLocations].program = program;
	state->uniformLocations[state->countUniformLocations].txt = strdup(txt);
	state->uniformLocations[state->countUniformLocations].location = ret;
	state->countUniformLocations++;

	return ret;
}


GLAPI void APIENTRY EXT_FUNC(glGetActiveUniformARB)(GLuint program,
		GLuint index,
		GLsizei maxLength,
		GLsizei *length,
		GLint *size,
		GLenum *type,
		GLcharARB *name)
{
	CHECK_PROC(glGetActiveUniformARB);
	int fake_length;
	if (length == NULL) length = &fake_length;
	long args[] = { INT_TO_ARG(program), INT_TO_ARG(index), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(size), POINTER_TO_ARG(type), POINTER_TO_ARG(name) };
	int args_size[] = { 0, 0, 0, sizeof(int), sizeof(int), sizeof(int), maxLength };
	do_opengl_call(glGetActiveUniformARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glGetActiveUniform)(GLuint program,
		GLuint index,
		GLsizei maxLength,
		GLsizei *length,
		GLint *size,
		GLenum *type,
		GLcharARB *name)
{
	CHECK_PROC(glGetActiveUniform);
	int fake_length;
	if (length == NULL) length = &fake_length;
	long args[] = { INT_TO_ARG(program), INT_TO_ARG(index), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(size), POINTER_TO_ARG(type), POINTER_TO_ARG(name) };
	int args_size[] = { 0, 0, 0, sizeof(int), sizeof(int), sizeof(int), maxLength };
	do_opengl_call(glGetActiveUniform_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glGetActiveVaryingNV)(GLuint program,
		GLuint index,
		GLsizei bufSize,
		GLsizei *length,
		GLsizei *size,
		GLenum *type,
		GLchar *name)
{
	CHECK_PROC(glGetActiveVaryingNV);
	int fake_length;
	if (length == NULL) length = &fake_length;
	long args[] = { INT_TO_ARG(program), INT_TO_ARG(index), INT_TO_ARG(bufSize), POINTER_TO_ARG(length), POINTER_TO_ARG(size), POINTER_TO_ARG(type), POINTER_TO_ARG(name) };
	int args_size[] = { 0, 0, 0, sizeof(int), sizeof(int), sizeof(int), bufSize };
	do_opengl_call(glGetActiveVaryingNV_func, NULL, CHECK_ARGS(args, args_size));
}

static int _get_size_of_gl_uniform_variables(GLenum type)
{
	switch(type)
	{
		case GL_FLOAT:       return sizeof(float);
		case GL_FLOAT_VEC2:  return 2*sizeof(float);
		case GL_FLOAT_VEC3:  return 3*sizeof(float);
		case GL_FLOAT_VEC4:  return 4*sizeof(float);
		case GL_INT:         return sizeof(int);
		case GL_INT_VEC2:    return 2*sizeof(int);
		case GL_INT_VEC3:    return 3*sizeof(int);
		case GL_INT_VEC4:    return 4*sizeof(int);
		case GL_BOOL:        return sizeof(int);
		case GL_BOOL_VEC2:   return 2*sizeof(int);
		case GL_BOOL_VEC3:   return 3*sizeof(int);
		case GL_BOOL_VEC4:   return 4*sizeof(int);
		case GL_FLOAT_MAT2:  return 2*2*sizeof(float);
		case GL_FLOAT_MAT3:  return 3*3*sizeof(float);
		case GL_FLOAT_MAT4:  return 4*4*sizeof(float);
		case GL_FLOAT_MAT2x3:return 2*3*sizeof(float);
		case GL_FLOAT_MAT2x4:return 2*4*sizeof(float);
		case GL_FLOAT_MAT3x2:return 3*2*sizeof(float);
		case GL_FLOAT_MAT3x4:return 3*4*sizeof(float);
		case GL_FLOAT_MAT4x2:return 4*2*sizeof(float);
		case GL_FLOAT_MAT4x3:return 4*3*sizeof(float);
		case GL_SAMPLER_1D:
		case GL_SAMPLER_2D:
		case GL_SAMPLER_3D:
		case GL_SAMPLER_CUBE:
		case GL_SAMPLER_1D_SHADOW:
		case GL_SAMPLER_2D_SHADOW:
							 return sizeof(int);

		default:
							 log_gl("unknown type for a uniform variable : %X\n", type);
							 return 0;
	}
}

static void _gl_get_uniform(PFNGLGETPROGRAMIVPROC getProgramiv,
		PFNGLGETACTIVEUNIFORMPROC getActiveUniform,
		int func_number,
		GLuint program,
		GLint location,
		void* params)
{
	GET_CURRENT_STATE();
	int nActiveUniforms = 0;
	int nameMaxLength = 0;
	int i;

	getProgramiv(program, GL_ACTIVE_UNIFORMS, &nActiveUniforms);
	getProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &nameMaxLength);

	char* name = malloc(nameMaxLength+1);
	char* uniformName = NULL;
	for(i=0;i<state->countUniformLocations;i++)
	{
		if (state->uniformLocations[i].program == program &&
				state->uniformLocations[i].location == location)
		{
			uniformName = state->uniformLocations[i].txt;
			break;
		}
	}
	if (uniformName == NULL)
	{
		log_gl("unknown uniform location : %d\n", location);
		return;
	}
	char* uniformName2 = NULL;
	if (strchr(uniformName, '[') == 0)
	{
		uniformName2 = malloc(strlen(uniformName) + 3 + 1);
		strcpy(uniformName2, uniformName);
		strcat(uniformName2, "[0]");
	}
	/*log_gl("nActiveUniforms=%d\n", nActiveUniforms);*/
	for(i=0;i<nActiveUniforms;i++)
	{
		int actualLength, size, type;
		int index = (i == 0 && location < nActiveUniforms) ? location : (i == location) ? 0 : i;
		getActiveUniform(program, index, nameMaxLength, &actualLength, &size, &type, name);
		/*log_gl("[%d] %s\n", i, name);*/
		if (strcmp(name, uniformName) == 0 || (uniformName2 && strcmp(name, uniformName2) == 0))
		{
			long args[] = { INT_TO_ARG(program), INT_TO_ARG(location), POINTER_TO_ARG(params) };
			int args_size[] = { 0, 0, size * _get_size_of_gl_uniform_variables(type)  };
			do_opengl_call(func_number, NULL, CHECK_ARGS(args, args_size));
			break;
		}
	}
	if (i == nActiveUniforms)
	{
		log_gl("sorry : I can't retrieve %s in the list of active uniforms\n", uniformName);
	}
	free(uniformName2);
	free(name);
}

GLAPI void APIENTRY EXT_FUNC(glGetUniformfvARB)(GLuint program,
		GLint location,
		GLfloat *params)
{
	CHECK_PROC(glGetUniformfvARB);
	_gl_get_uniform(glGetProgramivARB, glGetActiveUniformARB, glGetUniformfvARB_func, program, location, params);
}

GLAPI void APIENTRY EXT_FUNC(glGetUniformfv)(GLuint program,
		GLint location,
		GLfloat *params)
{
	CHECK_PROC(glGetUniformfv);
	_gl_get_uniform(glGetProgramiv, glGetActiveUniform, glGetUniformfv_func, program, location, params);
}


GLAPI void APIENTRY EXT_FUNC(glGetUniformivARB)(GLuint program,
		GLint location,
		GLint *params)
{
	CHECK_PROC(glGetUniformivARB);
	_gl_get_uniform(glGetProgramivARB, glGetActiveUniformARB, glGetUniformivARB_func, program, location, params);
}

GLAPI void APIENTRY EXT_FUNC(glGetUniformuivEXT)(GLuint program,
		GLint location,
		GLuint *params)
{
	CHECK_PROC(glGetUniformuivEXT);
	_gl_get_uniform(glGetProgramivARB, glGetActiveUniformARB, glGetUniformuivEXT_func, program, location, params);
}

GLAPI void APIENTRY EXT_FUNC(glGetUniformiv)(GLuint program,
		GLint location,
		GLint *params)
{
	CHECK_PROC(glGetUniformiv);
	_gl_get_uniform(glGetProgramiv, glGetActiveUniform, glGetUniformiv_func, program, location, params);
}

GLAPI void APIENTRY EXT_FUNC(glGetShaderSourceARB)(GLuint shader,
		GLsizei maxLength,
		GLsizei *length,
		GLcharARB *source)
{
	CHECK_PROC(glGetShaderSourceARB);
	int fake_length;
	if (length == NULL) length = &fake_length;
	long args[] = { INT_TO_ARG(shader), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(source) };
	int args_size[] = { 0, 0, sizeof(int), maxLength };
	do_opengl_call(glGetShaderSourceARB_func, NULL, CHECK_ARGS(args, args_size));
	log_gl("glGetShaderSourceARB : %s\n", source);
}

GLAPI void APIENTRY EXT_FUNC(glGetShaderSource)(GLuint shader,
		GLsizei maxLength,
		GLsizei *length,
		GLcharARB *source)
{
	CHECK_PROC(glGetShaderSource);
	int fake_length;
	if (length == NULL) length = &fake_length;
	long args[] = { INT_TO_ARG(shader), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(source) };
	int args_size[] = { 0, 0, sizeof(int), maxLength };
	do_opengl_call(glGetShaderSource_func, NULL, CHECK_ARGS(args, args_size));
	log_gl("glGetShaderSource : %s\n", source);
}


GLAPI void APIENTRY EXT_FUNC(glGetShaderInfoLog)(GLuint shader,
		GLsizei maxLength,
		GLsizei *length,
		GLchar *infoLog)
{
	CHECK_PROC(glGetShaderInfoLog);
	int fake_length;
	if (length == NULL) length = &fake_length;
	long args[] = { INT_TO_ARG(shader), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(infoLog) };
	int args_size[] = { 0, 0, sizeof(int), maxLength };
	do_opengl_call(glGetShaderInfoLog_func, NULL, CHECK_ARGS(args, args_size));
	log_gl("glGetShaderInfoLog: %s\n", infoLog);
}


static ObjectBufferATI* _new_object_buffer_ATI()
{
	GET_CURRENT_STATE();
	int i;
	for(i=0;i<32768;i++)
	{
		if (state->objectBuffersATI[i].bufferid == 0)
		{
			memset(&state->objectBuffersATI[i], 0, sizeof(ObjectBufferATI));
			return &state->objectBuffersATI[i];
		}
	}
	return NULL;
}

static ObjectBufferATI* _find_object_buffer_ATI_from_id(GLuint buffer)
{
	GET_CURRENT_STATE();
	int i;
	for(i=0;i<32768;i++)
	{
		if (state->objectBuffersATI[i].bufferid == buffer)
		{
			return &state->objectBuffersATI[i];
		}
	}
	return NULL;
}

static void _free_object_buffer_ATI(ObjectBufferATI* objectBufferATI)
{
	if (objectBufferATI == NULL) return;

	if (objectBufferATI->ptr)
		free(objectBufferATI->ptr);
	objectBufferATI->ptr = NULL;
	if (objectBufferATI->ptrMapped)
		free(objectBufferATI->ptrMapped);
	objectBufferATI->ptrMapped = NULL;
	if (objectBufferATI->ptrUpdatedWhileMapped)
		free(objectBufferATI->ptrUpdatedWhileMapped);
	objectBufferATI->ptrUpdatedWhileMapped = NULL;
	if (objectBufferATI->updatedRangesAfterMapping.ranges)
		free(objectBufferATI->updatedRangesAfterMapping.ranges);
	objectBufferATI->updatedRangesAfterMapping.ranges = NULL;
	objectBufferATI->updatedRangesAfterMapping.nb = 0;
	objectBufferATI->bufferid = 0;
	objectBufferATI->size = 0;
}

GLAPI GLuint APIENTRY EXT_FUNC(glNewObjectBufferATI) (GLsizei size, const GLvoid *pointer, GLenum usage)
{
	int buffer = 0;
	CHECK_PROC_WITH_RET(glNewObjectBufferATI);
	long args[] = { INT_TO_ARG(size), POINTER_TO_ARG(pointer), INT_TO_ARG(usage) };
	int args_size[] = { 0, (pointer) ? size : 0, 0 };
	do_opengl_call(glNewObjectBufferATI_func, &buffer, CHECK_ARGS(args, args_size));
	//log_gl("glNewObjectBufferATI(%d,%p) --> %d\n", size, pointer, buffer);

	if (buffer != 0)
	{
		ObjectBufferATI* objectBufferATI = _new_object_buffer_ATI();
		if (objectBufferATI)
		{
			objectBufferATI->bufferid = buffer;
			objectBufferATI->size = size;
			objectBufferATI->ptr = malloc(size);
			objectBufferATI->ptrMapped = NULL;
			if (pointer)
				memcpy(objectBufferATI->ptr, pointer, size);
		}
	}

	return buffer;
}

GLAPI void APIENTRY EXT_FUNC(glFreeObjectBufferATI) (GLuint buffer)
{
	CHECK_PROC(glFreeObjectBufferATI);
	long args[] = { UNSIGNED_INT_TO_ARG(buffer)};
	do_opengl_call(glFreeObjectBufferATI_func, NULL, args, NULL);
	_free_object_buffer_ATI(_find_object_buffer_ATI_from_id(buffer));
}

static void _add_int_range_to_ranges(IntSetRanges* ranges, int start, int length)
{
	int i,j;
	for(i=0;i<ranges->nb;i++)
	{
		IntRange* range = &ranges->ranges[i];
		if (start <= range->start)
		{
			if (start + length < range->start)
			{
				if (ranges->nb == ranges->maxNb)
					ranges->ranges = realloc(ranges->ranges, sizeof(IntRange) * (ranges->nb+1));
				memmove(&ranges->ranges[i+1], &ranges->ranges[i], sizeof(IntRange) * (ranges->nb - i));
				ranges->nb++;
				if (ranges->nb > ranges->maxNb)
					ranges->maxNb = ranges->nb;
				ranges->ranges[i].start = start;
				ranges->ranges[i].length = length;
				return;
			}
			else if (start + length <= range->start + range->length)
			{
				range->length = range->start + range->length - start;
				range->start = start;
				return;
			}
			else
			{
				j = i + 1;
				range->start = start;
				range->length = start + length - range->start;
				while(j < ranges->nb && start + length >= ranges->ranges[j].start)
				{
					if (start + length <= ranges->ranges[j].start + ranges->ranges[j].length)
					{
						range->length = ranges->ranges[j].start + ranges->ranges[j].length - range->start;
						j++;
						break;
					}
					j++;
				}
				if (i+1<j && j < ranges->nb)
					memmove(&ranges->ranges[i+1], &ranges->ranges[j], sizeof(IntRange) * (j - (i + 1)));
				ranges->nb -= j - (i+1);
				return;
			}
		}
		else
		{
			if (start > range->start + range->length)
			{
				continue;
			}
			else if (start + length <= range->start + range->length)
			{
				return;
			}
			else
			{
				j = i + 1;
				range->length = start + length - range->start;
				while(j < ranges->nb && start + length >= ranges->ranges[j].start)
				{
					if (start + length <= ranges->ranges[j].start + ranges->ranges[j].length)
					{
						range->length = ranges->ranges[j].start + ranges->ranges[j].length - range->start;
						j++;
						break;
					}
					j++;
				}
				if (i+1<j && j < ranges->nb)
					memmove(&ranges->ranges[i+1], &ranges->ranges[j], sizeof(IntRange) * (j - (i + 1)));
				ranges->nb -= j - (i+1);
				return;
			}
		}
	}
	if (ranges->nb == ranges->maxNb)
		ranges->ranges = realloc(ranges->ranges, sizeof(IntRange) * (ranges->nb+1));
	ranges->ranges[ranges->nb].start = start;
	ranges->ranges[ranges->nb].length = length;
	ranges->nb++;
	if (ranges->nb > ranges->maxNb)
		ranges->maxNb = ranges->nb;
	return;
}

static IntSetRanges _get_empty_ranges(IntSetRanges* inRanges, int start, int length)
{
	IntSetRanges outRanges = {0};
	int i;
	int end = start+length;
	int last_end = 0x80000000;
	for(i=0;i<=inRanges->nb;i++)
	{
		int cur_start, cur_end;
		if (i == inRanges->nb)
		{
			cur_start = cur_end = 0x7FFFFFFF;
		}
		else
		{
			IntRange* range = &inRanges->ranges[i];
			cur_start = range->start;
			cur_end = range->start + range->length;
		}

		/* [last_end,cur_start[ inter [start,end[ */
		if ((last_end >= start && last_end < end) || (start >= last_end && start < cur_start))
		{
			outRanges.ranges = realloc(outRanges.ranges, sizeof(IntRange) * (outRanges.nb+1));
			outRanges.ranges[outRanges.nb].start = MAX(start,last_end);
			outRanges.ranges[outRanges.nb].length = MIN(end,cur_start) - outRanges.ranges[outRanges.nb].start;
			outRanges.nb++;
		}

		last_end = cur_end;
	}
	return outRanges;
}

GLAPI void APIENTRY EXT_FUNC(glUpdateObjectBufferATI) (GLuint buffer, GLuint offset, GLsizei size, const GLvoid *pointer, GLenum preserve)
{
	CHECK_PROC(glUpdateObjectBufferATI);
	//log_gl("glUpdateObjectBufferATI(%d, %d, %d)\n", buffer, offset, size);
	long args[] = { INT_TO_ARG(buffer), INT_TO_ARG(offset), INT_TO_ARG(size), POINTER_TO_ARG(pointer), INT_TO_ARG(preserve) };
	int args_size[] = { 0, 0, 0, size, 0 };
	do_opengl_call(glUpdateObjectBufferATI_func, NULL, CHECK_ARGS(args, args_size));
	ObjectBufferATI* objectBufferATI = _find_object_buffer_ATI_from_id(buffer);
	if (objectBufferATI)
	{
		if (offset >= 0 && offset + size <= objectBufferATI->size)
		{
			if (objectBufferATI->ptrMapped)
			{
				log_gl("you shouldn't call glUpdateObjectBufferATI after glMapObjectBufferATI. we're emulating ATI fglrx (strange) behaviour\n");
				objectBufferATI->updatedRangesAfterMapping.nb = 0;
				_add_int_range_to_ranges(&objectBufferATI->updatedRangesAfterMapping, offset, size);
				objectBufferATI->ptrUpdatedWhileMapped = realloc(objectBufferATI->ptrUpdatedWhileMapped, size);
				memcpy(objectBufferATI->ptrUpdatedWhileMapped, pointer, size);
			}
			else
			{
				memcpy(objectBufferATI->ptr + offset, pointer, size);
			}
		}
		else
		{
			log_gl("offset >= 0 && offset + size <= state->objectBuffersATI[i].size failed\n");
		}
	}
}

GLAPI GLvoid* APIENTRY EXT_FUNC(glMapObjectBufferATI) (GLuint buffer)
{
	CHECK_PROC_WITH_RET(glMapObjectBufferATI);
	//log_gl("glMapObjectBufferATI(%d)\n", buffer);
	ObjectBufferATI* objectBufferATI = _find_object_buffer_ATI_from_id(buffer);
	if (objectBufferATI)
	{
		if (objectBufferATI->ptrMapped == NULL)
		{
			objectBufferATI->ptrMapped = malloc(objectBufferATI->size);
			memcpy(objectBufferATI->ptrMapped,
					objectBufferATI->ptr,
					objectBufferATI->size);
			return objectBufferATI->ptrMapped;
		}
		else
			return NULL;
	}
	else
		return NULL;
}

GLAPI void APIENTRY EXT_FUNC(glUnmapObjectBufferATI) (GLuint buffer)
{
	CHECK_PROC(glUnmapObjectBufferATI);
	//log_gl("glUnmapObjectBufferATI(%d)\n", buffer);
	ObjectBufferATI* objectBufferATI = _find_object_buffer_ATI_from_id(buffer);
	if (objectBufferATI)
	{
		if (objectBufferATI->ptrMapped)
		{
			IntSetRanges outRanges = _get_empty_ranges(&objectBufferATI->updatedRangesAfterMapping, 0, objectBufferATI->size);
			int i;
			void* ptrMapped = objectBufferATI->ptrMapped;
			if (objectBufferATI->ptrUpdatedWhileMapped)
			{
				assert(objectBufferATI->updatedRangesAfterMapping.nb == 1);
				memcpy(objectBufferATI->ptr + objectBufferATI->updatedRangesAfterMapping.ranges[0].start,
						objectBufferATI->ptrUpdatedWhileMapped,
						objectBufferATI->updatedRangesAfterMapping.ranges[0].length);
				free(objectBufferATI->ptrUpdatedWhileMapped);
				objectBufferATI->ptrUpdatedWhileMapped = NULL;
			}
			objectBufferATI->updatedRangesAfterMapping.nb = 0;
			objectBufferATI->ptrMapped = NULL;
			for(i=0;i<outRanges.nb;i++)
			{
				glUpdateObjectBufferATI(buffer, outRanges.ranges[i].start, outRanges.ranges[i].length,
						ptrMapped, GL_DISCARD_ATI);
			}
			free(outRanges.ranges);
			free(ptrMapped);
		}
	}
}

GLAPI void APIENTRY EXT_FUNC(glGetActiveAttribARB)(GLhandleARB program,
		GLuint index,
		GLsizei maxLength,
		GLsizei *length,
		GLint *size,
		GLenum *type,
		GLcharARB *name)
{
	CHECK_PROC(glGetActiveAttribARB);
	int fake_length;
	if (length == NULL) length = &fake_length;
	long args[] = { INT_TO_ARG(program), INT_TO_ARG(index), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(size), POINTER_TO_ARG(type), POINTER_TO_ARG(name) };
	int args_size[] = { 0, 0, 0, sizeof(int), sizeof(int), sizeof(int), maxLength };
	do_opengl_call(glGetActiveAttribARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glGetActiveAttrib)(GLhandleARB program,
		GLuint index,
		GLsizei maxLength,
		GLsizei *length,
		GLint *size,
		GLenum *type,
		GLcharARB *name)
{
	CHECK_PROC(glGetActiveAttrib);
	int fake_length;
	if (length == NULL) length = &fake_length;
	long args[] = { INT_TO_ARG(program), INT_TO_ARG(index), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(size), POINTER_TO_ARG(type), POINTER_TO_ARG(name) };
	int args_size[] = { 0, 0, 0, sizeof(int), sizeof(int), sizeof(int), maxLength };
	do_opengl_call(glGetActiveAttrib_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI GLint APIENTRY EXT_FUNC(glGetAttribLocationARB)(GLhandleARB program,
		const GLcharARB *name)
{
	CHECK_PROC_WITH_RET(glGetAttribLocationARB);
	int ret = 0;
	long args[] = { INT_TO_ARG(program), POINTER_TO_ARG(name) };
	do_opengl_call(glGetAttribLocationARB_func, &ret, args, NULL);
	return ret;
}

GLAPI GLint APIENTRY EXT_FUNC(glGetAttribLocation)(GLhandleARB program,
		const GLcharARB *name)
{
	CHECK_PROC_WITH_RET(glGetAttribLocation);
	int ret = 0;
	long args[] = { INT_TO_ARG(program), POINTER_TO_ARG(name) };
	do_opengl_call(glGetAttribLocation_func, &ret, args, NULL);
	return ret;
}

GLAPI void APIENTRY glGetDetailTexFuncSGIS (GLenum target, GLfloat *points)
{
	int npoints = 0;
	CHECK_PROC(glGetDetailTexFuncSGIS);
	glGetTexParameteriv(target, GL_DETAIL_TEXTURE_FUNC_POINTS_SGIS, &npoints);
	long args[] = { INT_TO_ARG(target), POINTER_TO_ARG(points) };
	int args_size[] = { 0, 2 * npoints * sizeof(float) };
	do_opengl_call(glGetDetailTexFuncSGIS_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glGetSharpenTexFuncSGIS (GLenum target, GLfloat *points)
{
	int npoints = 0;
	CHECK_PROC(glGetSharpenTexFuncSGIS);
	glGetTexParameteriv(target, GL_SHARPEN_TEXTURE_FUNC_POINTS_SGIS, &npoints);
	long args[] = { INT_TO_ARG(target), POINTER_TO_ARG(points) };
	int args_size[] = { 0, 2 * npoints * sizeof(float) };
	do_opengl_call(glGetSharpenTexFuncSGIS_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glColorTable (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *table)
{
	NOT_IMPLEMENTED(glColorTable);
}

GLAPI void APIENTRY glColorTableEXT (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *table)
{
	NOT_IMPLEMENTED(glColorTableEXT);
}


GLAPI void APIENTRY glColorSubTable (GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid *data)
{
	NOT_IMPLEMENTED(glColorSubTable);
}

GLAPI void APIENTRY glColorSubTableEXT (GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid *data)
{
	NOT_IMPLEMENTED(glColorSubTableEXT);
}


GLAPI void APIENTRY glGetColorTable (GLenum target, GLenum format, GLenum type, GLvoid *table)
{
	NOT_IMPLEMENTED(glGetColorTable);
}

GLAPI void APIENTRY glGetColorTableEXT (GLenum target, GLenum format, GLenum type, GLvoid *table)
{
	NOT_IMPLEMENTED(glGetColorTableEXT);
}


GLAPI void APIENTRY glConvolutionFilter1D (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *image)
{
	NOT_IMPLEMENTED(glConvolutionFilter1D);
}

GLAPI void APIENTRY glConvolutionFilter1DEXT (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *image)
{
	NOT_IMPLEMENTED(glConvolutionFilter1DEXT);
}

GLAPI void APIENTRY glConvolutionFilter2D (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *image)
{
	NOT_IMPLEMENTED(glConvolutionFilter2D);
}

GLAPI void APIENTRY glConvolutionFilter2DEXT (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *image)
{
	NOT_IMPLEMENTED(glConvolutionFilter2DEXT);
}


GLAPI void APIENTRY glGetConvolutionFilter (GLenum target, GLenum format, GLenum type, GLvoid *image)
{
	NOT_IMPLEMENTED(glGetConvolutionFilter);
}

GLAPI void APIENTRY glGetConvolutionFilterEXT (GLenum target, GLenum format, GLenum type, GLvoid *image)
{
	NOT_IMPLEMENTED(glGetConvolutionFilterEXT);
}

GLAPI void APIENTRY glGetSeparableFilter (GLenum target, GLenum format, GLenum type, GLvoid *row, GLvoid *column, GLvoid *span)
{
	NOT_IMPLEMENTED(glGetSeparableFilter);
}

GLAPI void APIENTRY glGetSeparableFilterEXT (GLenum target, GLenum format, GLenum type, GLvoid *row, GLvoid *column, GLvoid *span)
{
	NOT_IMPLEMENTED(glGetSeparableFilterEXT);
}

GLAPI void APIENTRY glSeparableFilter2D (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *row, const GLvoid *column)
{
	NOT_IMPLEMENTED(glSeparableFilter2D);
}

GLAPI void APIENTRY glSeparableFilter2DEXT (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *row, const GLvoid *column)
{
	NOT_IMPLEMENTED(glSeparableFilter2DEXT);
}


GLAPI void APIENTRY glGetHistogram (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values)
{
	NOT_IMPLEMENTED(glGetHistogram);
}

GLAPI void APIENTRY glGetHistogramEXT (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values)
{
	NOT_IMPLEMENTED(glGetHistogramEXT);
}


GLAPI void APIENTRY glGetMinmax (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values)
{
	NOT_IMPLEMENTED(glGetMinmax);
}

GLAPI void APIENTRY glGetMinmaxEXT (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values)
{
	NOT_IMPLEMENTED(glGetMinmaxEXT);
}

GLAPI void* APIENTRY glXAllocateMemoryNV(GLsizei size, GLfloat readfreq, GLfloat writefreq, GLfloat priority)
{
	return malloc(size);
}

GLAPI void APIENTRY glXFreeMemoryNV(GLvoid *pointer)
{
	free(pointer);
}

GLAPI void APIENTRY glPixelDataRangeNV (GLenum target, GLsizei length, GLvoid *pointer)
{
	CHECK_PROC(glPixelDataRangeNV);
	/* do nothing is a possible implementation... */
}

GLAPI void APIENTRY glFlushPixelDataRangeNV (GLenum target)
{
	CHECK_PROC(glFlushPixelDataRangeNV);
	/* do nothing is a possible implementation... */
}

GLAPI void APIENTRY glVertexArrayRangeNV (GLsizei size, const GLvoid *ptr)
{
	CHECK_PROC(glVertexArrayRangeNV);
	/* do nothing is a possible implementation... */
}

GLAPI void APIENTRY glFlushVertexArrayRangeNV (void)
{
	CHECK_PROC(glFlushVertexArrayRangeNV);
	/* do nothing is a possible implementation... */
}


static unsigned int str_hash (const void* v)
{
	/* 31 bit hash function */
	const signed char *p = v;
	unsigned int h = *p;

	if (h)
		for (p += 1; *p != '\0'; p++)
			h = (h << 5) - h + *p;

	return h;
}

static const char* global_glXGetProcAddress_request =
{
	"glAccum\0"
		"glActiveStencilFaceEXT\0"
		"glActiveTexture\0"
		"glActiveTextureARB\0"
		"glActiveVaryingNV\0"
		"glAddSwapHintRectWIN\0"
		"glAlphaFragmentOp1ATI\0"
		"glAlphaFragmentOp2ATI\0"
		"glAlphaFragmentOp3ATI\0"
		"glAlphaFunc\0"
		"glApplyTextureEXT\0"
		"glAreProgramsResidentNV\0"
		"glAreTexturesResident\0"
		"glAreTexturesResidentEXT\0"
		"glArrayElement\0"
		"glArrayElementEXT\0"
		"glArrayObjectATI\0"
		"glAsyncMarkerSGIX\0"
		"glAttachObjectARB\0"
		"glAttachShader\0"
		"glBegin\0"
		"glBeginConditionalRenderNVX\0"
		"glBeginDefineVisibilityQueryATI\0"
		"glBeginFragmentShaderATI\0"
		"glBeginOcclusionQuery\0"
		"glBeginOcclusionQueryNV\0"
		"glBeginQuery\0"
		"glBeginQueryARB\0"
		"glBeginSceneEXT\0"
		"glBeginTransformFeedbackNV\0"
		"glBeginUseVisibilityQueryATI\0"
		"glBeginVertexShaderEXT\0"
		"glBindArraySetARB\0"
		"glBindAttribLocation\0"
		"glBindAttribLocationARB\0"
		"glBindBuffer\0"
		"glBindBufferARB\0"
		"glBindBufferBaseNV\0"
		"glBindBufferOffsetNV\0"
		"glBindBufferRangeNV\0"
		"glBindFragDataLocationEXT\0"
		"glBindFragmentShaderATI\0"
		"glBindFramebufferEXT\0"
		"glBindLightParameterEXT\0"
		"glBindMaterialParameterEXT\0"
		"glBindParameterEXT\0"
		"glBindProgramARB\0"
		"glBindProgramNV\0"
		"glBindRenderbufferEXT\0"
		"glBindTexGenParameterEXT\0"
		"glBindTexture\0"
		"glBindTextureEXT\0"
		"glBindTextureUnitParameterEXT\0"
		"glBindVertexArrayAPPLE\0"
		"glBindVertexShaderEXT\0"
		"glBinormal3bEXT\0"
		"glBinormal3bvEXT\0"
		"glBinormal3dEXT\0"
		"glBinormal3dvEXT\0"
		"glBinormal3fEXT\0"
		"glBinormal3fvEXT\0"
		"glBinormal3iEXT\0"
		"glBinormal3ivEXT\0"
		"glBinormal3sEXT\0"
		"glBinormal3svEXT\0"
		"glBinormalArrayEXT\0"
		"glBitmap\0"
		"glBlendColor\0"
		"glBlendColorEXT\0"
		"glBlendEquation\0"
		"glBlendEquationEXT\0"
		"glBlendEquationSeparate\0"
		"glBlendEquationSeparateATI\0"
		"glBlendEquationSeparateEXT\0"
		"glBlendFunc\0"
		"glBlendFuncSeparate\0"
		"glBlendFuncSeparateEXT\0"
		"glBlendFuncSeparateINGR\0"
		"glBlitFramebufferEXT\0"
		"glBufferData\0"
		"glBufferDataARB\0"
		"glBufferParameteriAPPLE\0"
		"glBufferRegionEnabled\0"
		"glBufferRegionEnabledEXT\0"
		"glBufferSubData\0"
		"glBufferSubDataARB\0"
		"glCallList\0"
		"glCallLists\0"
		"glCheckFramebufferStatusEXT\0"
		"glClampColorARB\0"
		"glClear\0"
		"glClearAccum\0"
		"glClearColor\0"
		"glClearColorIiEXT\0"
		"glClearColorIuiEXT\0"
		"glClearDepth\0"
		"glClearDepthdNV\0"
		"glClearIndex\0"
		"glClearStencil\0"
		"glClientActiveTexture\0"
		"glClientActiveTextureARB\0"
		"glClientActiveVertexStreamATI\0"
		"glClipPlane\0"
		"glColor3b\0"
		"glColor3bv\0"
		"glColor3d\0"
		"glColor3dv\0"
		"glColor3f\0"
		"glColor3fv\0"
		"glColor3fVertex3fSUN\0"
		"glColor3fVertex3fvSUN\0"
		"glColor3hNV\0"
		"glColor3hvNV\0"
		"glColor3i\0"
		"glColor3iv\0"
		"glColor3s\0"
		"glColor3sv\0"
		"glColor3ub\0"
		"glColor3ubv\0"
		"glColor3ui\0"
		"glColor3uiv\0"
		"glColor3us\0"
		"glColor3usv\0"
		"glColor4b\0"
		"glColor4bv\0"
		"glColor4d\0"
		"glColor4dv\0"
		"glColor4f\0"
		"glColor4fNormal3fVertex3fSUN\0"
		"glColor4fNormal3fVertex3fvSUN\0"
		"glColor4fv\0"
		"glColor4hNV\0"
		"glColor4hvNV\0"
		"glColor4i\0"
		"glColor4iv\0"
		"glColor4s\0"
		"glColor4sv\0"
		"glColor4ub\0"
		"glColor4ubv\0"
		"glColor4ubVertex2fSUN\0"
		"glColor4ubVertex2fvSUN\0"
		"glColor4ubVertex3fSUN\0"
		"glColor4ubVertex3fvSUN\0"
		"glColor4ui\0"
		"glColor4uiv\0"
		"glColor4us\0"
		"glColor4usv\0"
		"glColorFragmentOp1ATI\0"
		"glColorFragmentOp2ATI\0"
		"glColorFragmentOp3ATI\0"
		"glColorMask\0"
		"glColorMaskIndexedEXT\0"
		"glColorMaterial\0"
		"glColorPointer\0"
		"glColorPointerEXT\0"
		"glColorPointerListIBM\0"
		"glColorPointervINTEL\0"
		"glColorSubTable\0"
		"glColorSubTableEXT\0"
		"glColorTable\0"
		"glColorTableEXT\0"
		"glColorTableParameterfv\0"
		"glColorTableParameterfvSGI\0"
		"glColorTableParameteriv\0"
		"glColorTableParameterivSGI\0"
		"glColorTableSGI\0"
		"glCombinerInputNV\0"
		"glCombinerOutputNV\0"
		"glCombinerParameterfNV\0"
		"glCombinerParameterfvNV\0"
		"glCombinerParameteriNV\0"
		"glCombinerParameterivNV\0"
		"glCombinerStageParameterfvNV\0"
		"glCompileShader\0"
		"glCompileShaderARB\0"
		"glCompressedTexImage1D\0"
		"glCompressedTexImage1DARB\0"
		"glCompressedTexImage2D\0"
		"glCompressedTexImage2DARB\0"
		"glCompressedTexImage3D\0"
		"glCompressedTexImage3DARB\0"
		"glCompressedTexSubImage1D\0"
		"glCompressedTexSubImage1DARB\0"
		"glCompressedTexSubImage2D\0"
		"glCompressedTexSubImage2DARB\0"
		"glCompressedTexSubImage3D\0"
		"glCompressedTexSubImage3DARB\0"
		"glConvolutionFilter1D\0"
		"glConvolutionFilter1DEXT\0"
		"glConvolutionFilter2D\0"
		"glConvolutionFilter2DEXT\0"
		"glConvolutionParameterf\0"
		"glConvolutionParameterfEXT\0"
		"glConvolutionParameterfv\0"
		"glConvolutionParameterfvEXT\0"
		"glConvolutionParameteri\0"
		"glConvolutionParameteriEXT\0"
		"glConvolutionParameteriv\0"
		"glConvolutionParameterivEXT\0"
		"glCopyColorSubTable\0"
		"glCopyColorSubTableEXT\0"
		"glCopyColorTable\0"
		"glCopyColorTableSGI\0"
		"glCopyConvolutionFilter1D\0"
		"glCopyConvolutionFilter1DEXT\0"
		"glCopyConvolutionFilter2D\0"
		"glCopyConvolutionFilter2DEXT\0"
		"glCopyPixels\0"
		"glCopyTexImage1D\0"
		"glCopyTexImage1DEXT\0"
		"glCopyTexImage2D\0"
		"glCopyTexImage2DEXT\0"
		"glCopyTexSubImage1D\0"
		"glCopyTexSubImage1DEXT\0"
		"glCopyTexSubImage2D\0"
		"glCopyTexSubImage2DEXT\0"
		"glCopyTexSubImage3D\0"
		"glCopyTexSubImage3DEXT\0"
		"glCreateProgram\0"
		"glCreateProgramObjectARB\0"
		"glCreateShader\0"
		"glCreateShaderObjectARB\0"
		"glCullFace\0"
		"glCullParameterdvEXT\0"
		"glCullParameterfvEXT\0"
		"glCurrentPaletteMatrixARB\0"
		"glDeformationMap3dSGIX\0"
		"glDeformationMap3fSGIX\0"
		"glDeformSGIX\0"
		"glDeleteArraySetsARB\0"
		"glDeleteAsyncMarkersSGIX\0"
		"glDeleteBufferRegion\0"
		"glDeleteBufferRegionEXT\0"
		"glDeleteBuffers\0"
		"glDeleteBuffersARB\0"
		"glDeleteFencesAPPLE\0"
		"glDeleteFencesNV\0"
		"glDeleteFragmentShaderATI\0"
		"glDeleteFramebuffersEXT\0"
		"glDeleteLists\0"
		"glDeleteObjectARB\0"
		"glDeleteObjectBufferATI\0"
		"glDeleteOcclusionQueries\0"
		"glDeleteOcclusionQueriesNV\0"
		"glDeleteProgram\0"
		"glDeleteProgramsARB\0"
		"glDeleteProgramsNV\0"
		"glDeleteQueries\0"
		"glDeleteQueriesARB\0"
		"glDeleteRenderbuffersEXT\0"
		"glDeleteShader\0"
		"glDeleteTextures\0"
		"glDeleteTexturesEXT\0"
		"glDeleteVertexArraysAPPLE\0"
		"glDeleteVertexShaderEXT\0"
		"glDeleteVisibilityQueriesATI\0"
		"glDepthBoundsdNV\0"
		"glDepthBoundsEXT\0"
		"glDepthFunc\0"
		"glDepthMask\0"
		"glDepthRange\0"
		"glDepthRangedNV\0"
		"glDetachObjectARB\0"
		"glDetachShader\0"
		"glDetailTexFuncSGIS\0"
		"glDisable\0"
		"glDisableClientState\0"
		"glDisableIndexedEXT\0"
		"glDisableVariantClientStateEXT\0"
		"glDisableVertexAttribAPPLE\0"
		"glDisableVertexAttribArray\0"
		"glDisableVertexAttribArrayARB\0"
		"glDrawArrays\0"
		"glDrawArraysEXT\0"
		"glDrawArraysInstancedEXT\0"
		"glDrawBuffer\0"
		"glDrawBufferRegion\0"
		"glDrawBufferRegionEXT\0"
		"glDrawBuffers\0"
		"glDrawBuffersARB\0"
		"glDrawBuffersATI\0"
		"glDrawElementArrayAPPLE\0"
		"glDrawElementArrayATI\0"
		"glDrawElements\0"
		"glDrawElementsFGL\0"
		"glDrawElementsInstancedEXT\0"
		"glDrawMeshArraysSUN\0"
		"glDrawMeshNV\0"
		"glDrawPixels\0"
		"glDrawRangeElementArrayAPPLE\0"
		"glDrawRangeElementArrayATI\0"
		"glDrawRangeElements\0"
		"glDrawRangeElementsEXT\0"
		"glDrawWireTrianglesFGL\0"
		"glEdgeFlag\0"
		"glEdgeFlagPointer\0"
		"glEdgeFlagPointerEXT\0"
		"glEdgeFlagPointerListIBM\0"
		"glEdgeFlagv\0"
		"glElementPointerAPPLE\0"
		"glElementPointerATI\0"
		"glEnable\0"
		"glEnableClientState\0"
		"glEnableIndexedEXT\0"
		"glEnableVariantClientStateEXT\0"
		"glEnableVertexAttribAPPLE\0"
		"glEnableVertexAttribArray\0"
		"glEnableVertexAttribArrayARB\0"
		"glEnd\0"
		"glEndConditionalRenderNVX\0"
		"glEndDefineVisibilityQueryATI\0"
		"glEndFragmentShaderATI\0"
		"glEndList\0"
		"glEndOcclusionQuery\0"
		"glEndOcclusionQueryNV\0"
		"glEndQuery\0"
		"glEndQueryARB\0"
		"glEndSceneEXT\0"
		"glEndTransformFeedbackNV\0"
		"glEndUseVisibilityQueryATI\0"
		"glEndVertexShaderEXT\0"
		"glEvalCoord1d\0"
		"glEvalCoord1dv\0"
		"glEvalCoord1f\0"
		"glEvalCoord1fv\0"
		"glEvalCoord2d\0"
		"glEvalCoord2dv\0"
		"glEvalCoord2f\0"
		"glEvalCoord2fv\0"
		"glEvalMapsNV\0"
		"glEvalMesh1\0"
		"glEvalMesh2\0"
		"glEvalPoint1\0"
		"glEvalPoint2\0"
		"glExecuteProgramNV\0"
		"glExtractComponentEXT\0"
		"glFeedbackBuffer\0"
		"glFinalCombinerInputNV\0"
		"glFinish\0"
		"glFinishAsyncSGIX\0"
		"glFinishFenceAPPLE\0"
		"glFinishFenceNV\0"
		"glFinishObjectAPPLE\0"
		"glFinishRenderAPPLE\0"
		"glFinishTextureSUNX\0"
		"glFlush\0"
		"glFlushMappedBufferRangeAPPLE\0"
		"glFlushPixelDataRangeNV\0"
		"glFlushRasterSGIX\0"
		"glFlushRenderAPPLE\0"
		"glFlushVertexArrayRangeAPPLE\0"
		"glFlushVertexArrayRangeNV\0"
		"glFogCoordd\0"
		"glFogCoorddEXT\0"
		"glFogCoorddv\0"
		"glFogCoorddvEXT\0"
		"glFogCoordf\0"
		"glFogCoordfEXT\0"
		"glFogCoordfv\0"
		"glFogCoordfvEXT\0"
		"glFogCoordhNV\0"
		"glFogCoordhvNV\0"
		"glFogCoordPointer\0"
		"glFogCoordPointerEXT\0"
		"glFogCoordPointerListIBM\0"
		"glFogf\0"
		"glFogFuncSGIS\0"
		"glFogfv\0"
		"glFogi\0"
		"glFogiv\0"
		"glFragmentColorMaterialEXT\0"
		"glFragmentColorMaterialSGIX\0"
		"glFragmentLightfEXT\0"
		"glFragmentLightfSGIX\0"
		"glFragmentLightfvEXT\0"
		"glFragmentLightfvSGIX\0"
		"glFragmentLightiEXT\0"
		"glFragmentLightiSGIX\0"
		"glFragmentLightivEXT\0"
		"glFragmentLightivSGIX\0"
		"glFragmentLightModelfEXT\0"
		"glFragmentLightModelfSGIX\0"
		"glFragmentLightModelfvEXT\0"
		"glFragmentLightModelfvSGIX\0"
		"glFragmentLightModeliEXT\0"
		"glFragmentLightModeliSGIX\0"
		"glFragmentLightModelivEXT\0"
		"glFragmentLightModelivSGIX\0"
		"glFragmentMaterialfEXT\0"
		"glFragmentMaterialfSGIX\0"
		"glFragmentMaterialfvEXT\0"
		"glFragmentMaterialfvSGIX\0"
		"glFragmentMaterialiEXT\0"
		"glFragmentMaterialiSGIX\0"
		"glFragmentMaterialivEXT\0"
		"glFragmentMaterialivSGIX\0"
		"glFramebufferRenderbufferEXT\0"
		"glFramebufferTexture1DEXT\0"
		"glFramebufferTexture2DEXT\0"
		"glFramebufferTexture3DEXT\0"
		"glFramebufferTextureEXT\0"
		"glFramebufferTextureFaceEXT\0"
		"glFramebufferTextureLayerEXT\0"
		"glFrameZoomSGIX\0"
		"glFreeObjectBufferATI\0"
		"glFrontFace\0"
		"glFrustum\0"
		"glGenArraySetsARB\0"
		"glGenAsyncMarkersSGIX\0"
		"glGenBuffers\0"
		"glGenBuffersARB\0"
		"glGenerateMipmapEXT\0"
		"glGenFencesAPPLE\0"
		"glGenFencesNV\0"
		"glGenFragmentShadersATI\0"
		"glGenFramebuffersEXT\0"
		"glGenLists\0"
		"glGenOcclusionQueries\0"
		"glGenOcclusionQueriesNV\0"
		"glGenProgramsARB\0"
		"glGenProgramsNV\0"
		"glGenQueries\0"
		"glGenQueriesARB\0"
		"glGenRenderbuffersEXT\0"
		"glGenSymbolsEXT\0"
		"glGenTextures\0"
		"glGenTexturesEXT\0"
		"glGenVertexArraysAPPLE\0"
		"glGenVertexShadersEXT\0"
		"glGenVisibilityQueriesATI\0"
		"glGetActiveAttrib\0"
		"glGetActiveAttribARB\0"
		"glGetActiveUniform\0"
		"glGetActiveUniformARB\0"
		"glGetActiveVaryingNV\0"
		"glGetArrayObjectfvATI\0"
		"glGetArrayObjectivATI\0"
		"glGetAttachedObjectsARB\0"
		"glGetAttachedShaders\0"
		"glGetAttribLocation\0"
		"glGetAttribLocationARB\0"
		"glGetBooleanIndexedvEXT\0"
		"glGetBooleanv\0"
		"glGetBufferParameteriv\0"
		"glGetBufferParameterivARB\0"
		"glGetBufferPointerv\0"
		"glGetBufferPointervARB\0"
		"glGetBufferSubData\0"
		"glGetBufferSubDataARB\0"
		"glGetClipPlane\0"
		"glGetColorTable\0"
		"glGetColorTableEXT\0"
		"glGetColorTableParameterfv\0"
		"glGetColorTableParameterfvEXT\0"
		"glGetColorTableParameterfvSGI\0"
		"glGetColorTableParameteriv\0"
		"glGetColorTableParameterivEXT\0"
		"glGetColorTableParameterivSGI\0"
		"glGetColorTableSGI\0"
		"glGetCombinerInputParameterfvNV\0"
		"glGetCombinerInputParameterivNV\0"
		"glGetCombinerOutputParameterfvNV\0"
		"glGetCombinerOutputParameterivNV\0"
		"glGetCombinerStageParameterfvNV\0"
		"glGetCompressedTexImage\0"
		"glGetCompressedTexImageARB\0"
		"glGetConvolutionFilter\0"
		"glGetConvolutionFilterEXT\0"
		"glGetConvolutionParameterfv\0"
		"glGetConvolutionParameterfvEXT\0"
		"glGetConvolutionParameteriv\0"
		"glGetConvolutionParameterivEXT\0"
		"glGetDetailTexFuncSGIS\0"
		"glGetDoublev\0"
		"glGetError\0"
		"glGetFenceivNV\0"
		"glGetFinalCombinerInputParameterfvNV\0"
		"glGetFinalCombinerInputParameterivNV\0"
		"glGetFloatv\0"
		"glGetFogFuncSGIS\0"
		"glGetFragDataLocationEXT\0"
		"glGetFragmentLightfvEXT\0"
		"glGetFragmentLightfvSGIX\0"
		"glGetFragmentLightivEXT\0"
		"glGetFragmentLightivSGIX\0"
		"glGetFragmentMaterialfvEXT\0"
		"glGetFragmentMaterialfvSGIX\0"
		"glGetFragmentMaterialivEXT\0"
		"glGetFragmentMaterialivSGIX\0"
		"glGetFramebufferAttachmentParameterivEXT\0"
		"glGetHandleARB\0"
		"glGetHistogram\0"
		"glGetHistogramEXT\0"
		"glGetHistogramParameterfv\0"
		"glGetHistogramParameterfvEXT\0"
		"glGetHistogramParameteriv\0"
		"glGetHistogramParameterivEXT\0"
		"glGetImageTransformParameterfvHP\0"
		"glGetImageTransformParameterivHP\0"
		"glGetInfoLogARB\0"
		"glGetInstrumentsSGIX\0"
		"glGetIntegerIndexedvEXT\0"
		"glGetIntegerv\0"
		"glGetInvariantBooleanvEXT\0"
		"glGetInvariantFloatvEXT\0"
		"glGetInvariantIntegervEXT\0"
		"glGetLightfv\0"
		"glGetLightiv\0"
		"glGetListParameterfvSGIX\0"
		"glGetListParameterivSGIX\0"
		"glGetLocalConstantBooleanvEXT\0"
		"glGetLocalConstantFloatvEXT\0"
		"glGetLocalConstantIntegervEXT\0"
		"glGetMapAttribParameterfvNV\0"
		"glGetMapAttribParameterivNV\0"
		"glGetMapControlPointsNV\0"
		"glGetMapdv\0"
		"glGetMapfv\0"
		"glGetMapiv\0"
		"glGetMapParameterfvNV\0"
		"glGetMapParameterivNV\0"
		"glGetMaterialfv\0"
		"glGetMaterialiv\0"
		"glGetMinmax\0"
		"glGetMinmaxEXT\0"
		"glGetMinmaxParameterfv\0"
		"glGetMinmaxParameterfvEXT\0"
		"glGetMinmaxParameteriv\0"
		"glGetMinmaxParameterivEXT\0"
		"glGetObjectBufferfvATI\0"
		"glGetObjectBufferivATI\0"
		"glGetObjectParameterfvARB\0"
		"glGetObjectParameterivARB\0"
		"glGetOcclusionQueryiv\0"
		"glGetOcclusionQueryivNV\0"
		"glGetOcclusionQueryuiv\0"
		"glGetOcclusionQueryuivNV\0"
		"glGetPixelMapfv\0"
		"glGetPixelMapuiv\0"
		"glGetPixelMapusv\0"
		"glGetPixelTexGenParameterfvSGIS\0"
		"glGetPixelTexGenParameterivSGIS\0"
		"glGetPixelTransformParameterfvEXT\0"
		"glGetPixelTransformParameterivEXT\0"
		"glGetPointerv\0"
		"glGetPointervEXT\0"
		"glGetPolygonStipple\0"
		"glGetProgramEnvParameterdvARB\0"
		"glGetProgramEnvParameterfvARB\0"
		"glGetProgramEnvParameterIivNV\0"
		"glGetProgramEnvParameterIuivNV\0"
		"glGetProgramInfoLog\0"
		"glGetProgramiv\0"
		"glGetProgramivARB\0"
		"glGetProgramivNV\0"
		"glGetProgramLocalParameterdvARB\0"
		"glGetProgramLocalParameterfvARB\0"
		"glGetProgramLocalParameterIivNV\0"
		"glGetProgramLocalParameterIuivNV\0"
		"glGetProgramNamedParameterdvNV\0"
		"glGetProgramNamedParameterfvNV\0"
		"glGetProgramParameterdvNV\0"
		"glGetProgramParameterfvNV\0"
		"glGetProgramRegisterfvMESA\0"
		"glGetProgramStringARB\0"
		"glGetProgramStringNV\0"
		"glGetQueryiv\0"
		"glGetQueryivARB\0"
		"glGetQueryObjecti64vEXT\0"
		"glGetQueryObjectiv\0"
		"glGetQueryObjectivARB\0"
		"glGetQueryObjectui64vEXT\0"
		"glGetQueryObjectuiv\0"
		"glGetQueryObjectuivARB\0"
		"glGetRenderbufferParameterivEXT\0"
		"glGetSeparableFilter\0"
		"glGetSeparableFilterEXT\0"
		"glGetShaderInfoLog\0"
		"glGetShaderiv\0"
		"glGetShaderSource\0"
		"glGetShaderSourceARB\0"
		"glGetSharpenTexFuncSGIS\0"
		"glGetString\0"
		"glGetTexBumpParameterfvATI\0"
		"glGetTexBumpParameterivATI\0"
		"glGetTexEnvfv\0"
		"glGetTexEnviv\0"
		"glGetTexFilterFuncSGIS\0"
		"glGetTexGendv\0"
		"glGetTexGenfv\0"
		"glGetTexGeniv\0"
		"glGetTexImage\0"
		"glGetTexLevelParameterfv\0"
		"glGetTexLevelParameteriv\0"
		"glGetTexParameterfv\0"
		"glGetTexParameterIivEXT\0"
		"glGetTexParameterIuivEXT\0"
		"glGetTexParameteriv\0"
		"glGetTexParameterPointervAPPLE\0"
		"glGetTrackMatrixivNV\0"
		"glGetTransformFeedbackVaryingNV\0"
		"glGetUniformBufferSizeEXT\0"
		"glGetUniformfv\0"
		"glGetUniformfvARB\0"
		"glGetUniformiv\0"
		"glGetUniformivARB\0"
		"glGetUniformLocation\0"
		"glGetUniformLocationARB\0"
		"glGetUniformOffsetEXT\0"
		"glGetUniformuivEXT\0"
		"glGetVariantArrayObjectfvATI\0"
		"glGetVariantArrayObjectivATI\0"
		"glGetVariantBooleanvEXT\0"
		"glGetVariantFloatvEXT\0"
		"glGetVariantIntegervEXT\0"
		"glGetVariantPointervEXT\0"
		"glGetVaryingLocationNV\0"
		"glGetVertexAttribArrayObjectfvATI\0"
		"glGetVertexAttribArrayObjectivATI\0"
		"glGetVertexAttribdv\0"
		"glGetVertexAttribdvARB\0"
		"glGetVertexAttribdvNV\0"
		"glGetVertexAttribfv\0"
		"glGetVertexAttribfvARB\0"
		"glGetVertexAttribfvNV\0"
		"glGetVertexAttribIivEXT\0"
		"glGetVertexAttribIuivEXT\0"
		"glGetVertexAttribiv\0"
		"glGetVertexAttribivARB\0"
		"glGetVertexAttribivNV\0"
		"glGetVertexAttribPointerv\0"
		"glGetVertexAttribPointervARB\0"
		"glGetVertexAttribPointervNV\0"
		"glGlobalAlphaFactorbSUN\0"
		"glGlobalAlphaFactordSUN\0"
		"glGlobalAlphaFactorfSUN\0"
		"glGlobalAlphaFactoriSUN\0"
		"glGlobalAlphaFactorsSUN\0"
		"glGlobalAlphaFactorubSUN\0"
		"glGlobalAlphaFactoruiSUN\0"
		"glGlobalAlphaFactorusSUN\0"
		"glHint\0"
		"glHintPGI\0"
		"glHistogram\0"
		"glHistogramEXT\0"
		"glIglooInterfaceSGIX\0"
		"glImageTransformParameterfHP\0"
		"glImageTransformParameterfvHP\0"
		"glImageTransformParameteriHP\0"
		"glImageTransformParameterivHP\0"
		"glIndexd\0"
		"glIndexdv\0"
		"glIndexf\0"
		"glIndexFuncEXT\0"
		"glIndexfv\0"
		"glIndexi\0"
		"glIndexiv\0"
		"glIndexMask\0"
		"glIndexMaterialEXT\0"
		"glIndexPointer\0"
		"glIndexPointerEXT\0"
		"glIndexPointerListIBM\0"
		"glIndexs\0"
		"glIndexsv\0"
		"glIndexub\0"
		"glIndexubv\0"
		"glInitNames\0"
		"glInsertComponentEXT\0"
		"glInstrumentsBufferSGIX\0"
		"glInterleavedArrays\0"
		"glIsArraySetARB\0"
		"glIsAsyncMarkerSGIX\0"
		"glIsBuffer\0"
		"glIsBufferARB\0"
		"glIsEnabled\0"
		"glIsEnabledIndexedEXT\0"
		"glIsFenceAPPLE\0"
		"glIsFenceNV\0"
		"glIsFramebufferEXT\0"
		"glIsList\0"
		"glIsObjectBufferATI\0"
		"glIsOcclusionQuery\0"
		"_glIsOcclusionQueryNV\0"
		"glIsOcclusionQueryNV\0"
		"glIsProgram\0"
		"glIsProgramARB\0"
		"glIsProgramNV\0"
		"glIsQuery\0"
		"glIsQueryARB\0"
		"glIsRenderbufferEXT\0"
		"glIsShader\0"
		"glIsTexture\0"
		"glIsTextureEXT\0"
		"glIsVariantEnabledEXT\0"
		"glIsVertexArrayAPPLE\0"
		"glIsVertexAttribEnabledAPPLE\0"
		"glLightEnviEXT\0"
		"glLightEnviSGIX\0"
		"glLightf\0"
		"glLightfv\0"
		"glLighti\0"
		"glLightiv\0"
		"glLightModelf\0"
		"glLightModelfv\0"
		"glLightModeli\0"
		"glLightModeliv\0"
		"glLineStipple\0"
		"glLineWidth\0"
		"glLinkProgram\0"
		"glLinkProgramARB\0"
		"glListBase\0"
		"glListParameterfSGIX\0"
		"glListParameterfvSGIX\0"
		"glListParameteriSGIX\0"
		"glListParameterivSGIX\0"
		"glLoadIdentity\0"
		"glLoadIdentityDeformationMapSGIX\0"
		"glLoadMatrixd\0"
		"glLoadMatrixf\0"
		"glLoadName\0"
		"glLoadProgramNV\0"
		"glLoadTransposeMatrixd\0"
		"glLoadTransposeMatrixdARB\0"
		"glLoadTransposeMatrixf\0"
		"glLoadTransposeMatrixfARB\0"
		"glLockArraysEXT\0"
		"glLogicOp\0"
		"glMap1d\0"
		"glMap1f\0"
		"glMap2d\0"
		"glMap2f\0"
		"glMapBuffer\0"
		"glMapBufferARB\0"
		"glMapControlPointsNV\0"
		"glMapGrid1d\0"
		"glMapGrid1f\0"
		"glMapGrid2d\0"
		"glMapGrid2f\0"
		"glMapObjectBufferATI\0"
		"glMapParameterfvNV\0"
		"glMapParameterivNV\0"
		"glMapTexture3DATI\0"
		"glMapVertexAttrib1dAPPLE\0"
		"glMapVertexAttrib1fAPPLE\0"
		"glMapVertexAttrib2dAPPLE\0"
		"glMapVertexAttrib2fAPPLE\0"
		"glMaterialf\0"
		"glMaterialfv\0"
		"glMateriali\0"
		"glMaterialiv\0"
		"glMatrixIndexPointerARB\0"
		"glMatrixIndexubvARB\0"
		"glMatrixIndexuivARB\0"
		"glMatrixIndexusvARB\0"
		"glMatrixMode\0"
		"glMinmax\0"
		"glMinmaxEXT\0"
		"glMultiDrawArrays\0"
		"glMultiDrawArraysEXT\0"
		"glMultiDrawElementArrayAPPLE\0"
		"glMultiDrawElements\0"
		"glMultiDrawElementsEXT\0"
		"glMultiDrawRangeElementArrayAPPLE\0"
		"glMultiModeDrawArraysIBM\0"
		"glMultiModeDrawElementsIBM\0"
		"glMultiTexCoord1d\0"
		"glMultiTexCoord1dARB\0"
		"glMultiTexCoord1dSGIS\0"
		"glMultiTexCoord1dv\0"
		"glMultiTexCoord1dvARB\0"
		"glMultiTexCoord1dvSGIS\0"
		"glMultiTexCoord1f\0"
		"glMultiTexCoord1fARB\0"
		"glMultiTexCoord1fSGIS\0"
		"glMultiTexCoord1fv\0"
		"glMultiTexCoord1fvARB\0"
		"glMultiTexCoord1fvSGIS\0"
		"glMultiTexCoord1hNV\0"
		"glMultiTexCoord1hvNV\0"
		"glMultiTexCoord1i\0"
		"glMultiTexCoord1iARB\0"
		"glMultiTexCoord1iSGIS\0"
		"glMultiTexCoord1iv\0"
		"glMultiTexCoord1ivARB\0"
		"glMultiTexCoord1ivSGIS\0"
		"glMultiTexCoord1s\0"
		"glMultiTexCoord1sARB\0"
		"glMultiTexCoord1sSGIS\0"
		"glMultiTexCoord1sv\0"
		"glMultiTexCoord1svARB\0"
		"glMultiTexCoord1svSGIS\0"
		"glMultiTexCoord2d\0"
		"glMultiTexCoord2dARB\0"
		"glMultiTexCoord2dSGIS\0"
		"glMultiTexCoord2dv\0"
		"glMultiTexCoord2dvARB\0"
		"glMultiTexCoord2dvSGIS\0"
		"glMultiTexCoord2f\0"
		"glMultiTexCoord2fARB\0"
		"glMultiTexCoord2fSGIS\0"
		"glMultiTexCoord2fv\0"
		"glMultiTexCoord2fvARB\0"
		"glMultiTexCoord2fvSGIS\0"
		"glMultiTexCoord2hNV\0"
		"glMultiTexCoord2hvNV\0"
		"glMultiTexCoord2i\0"
		"glMultiTexCoord2iARB\0"
		"glMultiTexCoord2iSGIS\0"
		"glMultiTexCoord2iv\0"
		"glMultiTexCoord2ivARB\0"
		"glMultiTexCoord2ivSGIS\0"
		"glMultiTexCoord2s\0"
		"glMultiTexCoord2sARB\0"
		"glMultiTexCoord2sSGIS\0"
		"glMultiTexCoord2sv\0"
		"glMultiTexCoord2svARB\0"
		"glMultiTexCoord2svSGIS\0"
		"glMultiTexCoord3d\0"
		"glMultiTexCoord3dARB\0"
		"glMultiTexCoord3dSGIS\0"
		"glMultiTexCoord3dv\0"
		"glMultiTexCoord3dvARB\0"
		"glMultiTexCoord3dvSGIS\0"
		"glMultiTexCoord3f\0"
		"glMultiTexCoord3fARB\0"
		"glMultiTexCoord3fSGIS\0"
		"glMultiTexCoord3fv\0"
		"glMultiTexCoord3fvARB\0"
		"glMultiTexCoord3fvSGIS\0"
		"glMultiTexCoord3hNV\0"
		"glMultiTexCoord3hvNV\0"
		"glMultiTexCoord3i\0"
		"glMultiTexCoord3iARB\0"
		"glMultiTexCoord3iSGIS\0"
		"glMultiTexCoord3iv\0"
		"glMultiTexCoord3ivARB\0"
		"glMultiTexCoord3ivSGIS\0"
		"glMultiTexCoord3s\0"
		"glMultiTexCoord3sARB\0"
		"glMultiTexCoord3sSGIS\0"
		"glMultiTexCoord3sv\0"
		"glMultiTexCoord3svARB\0"
		"glMultiTexCoord3svSGIS\0"
		"glMultiTexCoord4d\0"
		"glMultiTexCoord4dARB\0"
		"glMultiTexCoord4dSGIS\0"
		"glMultiTexCoord4dv\0"
		"glMultiTexCoord4dvARB\0"
		"glMultiTexCoord4dvSGIS\0"
		"glMultiTexCoord4f\0"
		"glMultiTexCoord4fARB\0"
		"glMultiTexCoord4fSGIS\0"
		"glMultiTexCoord4fv\0"
		"glMultiTexCoord4fvARB\0"
		"glMultiTexCoord4fvSGIS\0"
		"glMultiTexCoord4hNV\0"
		"glMultiTexCoord4hvNV\0"
		"glMultiTexCoord4i\0"
		"glMultiTexCoord4iARB\0"
		"glMultiTexCoord4iSGIS\0"
		"glMultiTexCoord4iv\0"
		"glMultiTexCoord4ivARB\0"
		"glMultiTexCoord4ivSGIS\0"
		"glMultiTexCoord4s\0"
		"glMultiTexCoord4sARB\0"
		"glMultiTexCoord4sSGIS\0"
		"glMultiTexCoord4sv\0"
		"glMultiTexCoord4svARB\0"
		"glMultiTexCoord4svSGIS\0"
		"glMultiTexCoordPointerSGIS\0"
		"glMultMatrixd\0"
		"glMultMatrixf\0"
		"glMultTransposeMatrixd\0"
		"glMultTransposeMatrixdARB\0"
		"glMultTransposeMatrixf\0"
		"glMultTransposeMatrixfARB\0"
		"glNewBufferRegion\0"
		"glNewBufferRegionEXT\0"
		"glNewList\0"
		"glNewObjectBufferATI\0"
		"glNormal3b\0"
		"glNormal3bv\0"
		"glNormal3d\0"
		"glNormal3dv\0"
		"glNormal3f\0"
		"glNormal3fv\0"
		"glNormal3fVertex3fSUN\0"
		"glNormal3fVertex3fvSUN\0"
		"glNormal3hNV\0"
		"glNormal3hvNV\0"
		"glNormal3i\0"
		"glNormal3iv\0"
		"glNormal3s\0"
		"glNormal3sv\0"
		"glNormalPointer\0"
		"glNormalPointerEXT\0"
		"glNormalPointerListIBM\0"
		"glNormalPointervINTEL\0"
		"glNormalStream3bATI\0"
		"glNormalStream3bvATI\0"
		"glNormalStream3dATI\0"
		"glNormalStream3dvATI\0"
		"glNormalStream3fATI\0"
		"glNormalStream3fvATI\0"
		"glNormalStream3iATI\0"
		"glNormalStream3ivATI\0"
		"glNormalStream3sATI\0"
		"glNormalStream3svATI\0"
		"glOrtho\0"
		"glPassTexCoordATI\0"
		"glPassThrough\0"
		"glPixelDataRangeNV\0"
		"glPixelMapfv\0"
		"glPixelMapuiv\0"
		"glPixelMapusv\0"
		"glPixelStoref\0"
		"glPixelStorei\0"
		"glPixelTexGenParameterfSGIS\0"
		"glPixelTexGenParameterfvSGIS\0"
		"glPixelTexGenParameteriSGIS\0"
		"glPixelTexGenParameterivSGIS\0"
		"glPixelTexGenSGIX\0"
		"glPixelTransferf\0"
		"glPixelTransferi\0"
		"glPixelTransformParameterfEXT\0"
		"glPixelTransformParameterfvEXT\0"
		"glPixelTransformParameteriEXT\0"
		"glPixelTransformParameterivEXT\0"
		"glPixelZoom\0"
		"glPNTrianglesfATI\0"
		"glPNTrianglesiATI\0"
		"glPointParameterf\0"
		"glPointParameterfARB\0"
		"glPointParameterfEXT\0"
		"glPointParameterfSGIS\0"
		"glPointParameterfv\0"
		"glPointParameterfvARB\0"
		"glPointParameterfvEXT\0"
		"glPointParameterfvSGIS\0"
		"glPointParameteri\0"
		"glPointParameteriEXT\0"
		"glPointParameteriNV\0"
		"glPointParameteriv\0"
		"glPointParameterivEXT\0"
		"glPointParameterivNV\0"
		"glPointSize\0"
		"glPollAsyncSGIX\0"
		"glPollInstrumentsSGIX\0"
		"glPolygonMode\0"
		"glPolygonOffset\0"
		"glPolygonOffsetEXT\0"
		"glPolygonStipple\0"
		"glPopAttrib\0"
		"glPopClientAttrib\0"
		"glPopMatrix\0"
		"glPopName\0"
		"glPrimitiveRestartIndexNV\0"
		"glPrimitiveRestartNV\0"
		"glPrioritizeTextures\0"
		"glPrioritizeTexturesEXT\0"
		"glProgramBufferParametersfvNV\0"
		"glProgramBufferParametersIivNV\0"
		"glProgramBufferParametersIuivNV\0"
		"glProgramCallbackMESA\0"
		"glProgramEnvParameter4dARB\0"
		"glProgramEnvParameter4dvARB\0"
		"glProgramEnvParameter4fARB\0"
		"glProgramEnvParameter4fvARB\0"
		"glProgramEnvParameterI4iNV\0"
		"glProgramEnvParameterI4ivNV\0"
		"glProgramEnvParameterI4uiNV\0"
		"glProgramEnvParameterI4uivNV\0"
		"glProgramEnvParameters4fvEXT\0"
		"glProgramEnvParametersI4ivNV\0"
		"glProgramEnvParametersI4uivNV\0"
		"glProgramLocalParameter4dARB\0"
		"glProgramLocalParameter4dvARB\0"
		"glProgramLocalParameter4fARB\0"
		"glProgramLocalParameter4fvARB\0"
		"glProgramLocalParameterI4iNV\0"
		"glProgramLocalParameterI4ivNV\0"
		"glProgramLocalParameterI4uiNV\0"
		"glProgramLocalParameterI4uivNV\0"
		"glProgramLocalParameters4fvEXT\0"
		"glProgramLocalParametersI4ivNV\0"
		"glProgramLocalParametersI4uivNV\0"
		"glProgramNamedParameter4dNV\0"
		"glProgramNamedParameter4dvNV\0"
		"glProgramNamedParameter4fNV\0"
		"glProgramNamedParameter4fvNV\0"
		"glProgramParameter4dNV\0"
		"glProgramParameter4dvNV\0"
		"glProgramParameter4fNV\0"
		"glProgramParameter4fvNV\0"
		"glProgramParameteriEXT\0"
		"glProgramParameters4dvNV\0"
		"glProgramParameters4fvNV\0"
		"glProgramStringARB\0"
		"glProgramVertexLimitNV\0"
		"glPushAttrib\0"
		"glPushClientAttrib\0"
		"glPushMatrix\0"
		"glPushName\0"
		"glRasterPos2d\0"
		"glRasterPos2dv\0"
		"glRasterPos2f\0"
		"glRasterPos2fv\0"
		"glRasterPos2i\0"
		"glRasterPos2iv\0"
		"glRasterPos2s\0"
		"glRasterPos2sv\0"
		"glRasterPos3d\0"
		"glRasterPos3dv\0"
		"glRasterPos3f\0"
		"glRasterPos3fv\0"
		"glRasterPos3i\0"
		"glRasterPos3iv\0"
		"glRasterPos3s\0"
		"glRasterPos3sv\0"
		"glRasterPos4d\0"
		"glRasterPos4dv\0"
		"glRasterPos4f\0"
		"glRasterPos4fv\0"
		"glRasterPos4i\0"
		"glRasterPos4iv\0"
		"glRasterPos4s\0"
		"glRasterPos4sv\0"
		"glReadBuffer\0"
		"glReadBufferRegion\0"
		"glReadBufferRegionEXT\0"
		"glReadInstrumentsSGIX\0"
		"glReadPixels\0"
		"glReadVideoPixelsSUN\0"
		"glRectd\0"
		"glRectdv\0"
		"glRectf\0"
		"glRectfv\0"
		"glRecti\0"
		"glRectiv\0"
		"glRects\0"
		"glRectsv\0"
		"glReferencePlaneSGIX\0"
		"glRenderbufferStorageEXT\0"
		"glRenderbufferStorageMultisampleCoverageNV\0"
		"glRenderbufferStorageMultisampleEXT\0"
		"glRenderMode\0"
		"glReplacementCodePointerSUN\0"
		"glReplacementCodeubSUN\0"
		"glReplacementCodeubvSUN\0"
		"glReplacementCodeuiColor3fVertex3fSUN\0"
		"glReplacementCodeuiColor3fVertex3fvSUN\0"
		"glReplacementCodeuiColor4fNormal3fVertex3fSUN\0"
		"glReplacementCodeuiColor4fNormal3fVertex3fvSUN\0"
		"glReplacementCodeuiColor4ubVertex3fSUN\0"
		"glReplacementCodeuiColor4ubVertex3fvSUN\0"
		"glReplacementCodeuiNormal3fVertex3fSUN\0"
		"glReplacementCodeuiNormal3fVertex3fvSUN\0"
		"glReplacementCodeuiSUN\0"
		"glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN\0"
		"glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN\0"
		"glReplacementCodeuiTexCoord2fNormal3fVertex3fSUN\0"
		"glReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN\0"
		"glReplacementCodeuiTexCoord2fVertex3fSUN\0"
		"glReplacementCodeuiTexCoord2fVertex3fvSUN\0"
		"glReplacementCodeuiVertex3fSUN\0"
		"glReplacementCodeuiVertex3fvSUN\0"
		"glReplacementCodeuivSUN\0"
		"glReplacementCodeusSUN\0"
		"glReplacementCodeusvSUN\0"
		"glRequestResidentProgramsNV\0"
		"glResetHistogram\0"
		"glResetHistogramEXT\0"
		"glResetMinmax\0"
		"glResetMinmaxEXT\0"
		"glResizeBuffersMESA\0"
		"glRotated\0"
		"glRotatef\0"
		"glSampleCoverage\0"
		"glSampleCoverageARB\0"
		"glSampleMapATI\0"
		"glSampleMaskEXT\0"
		"glSampleMaskSGIS\0"
		"glSamplePassARB\0"
		"glSamplePatternEXT\0"
		"glSamplePatternSGIS\0"
		"glScaled\0"
		"glScalef\0"
		"glScissor\0"
		"glSecondaryColor3b\0"
		"glSecondaryColor3bEXT\0"
		"glSecondaryColor3bv\0"
		"glSecondaryColor3bvEXT\0"
		"glSecondaryColor3d\0"
		"glSecondaryColor3dEXT\0"
		"glSecondaryColor3dv\0"
		"glSecondaryColor3dvEXT\0"
		"glSecondaryColor3f\0"
		"glSecondaryColor3fEXT\0"
		"glSecondaryColor3fv\0"
		"glSecondaryColor3fvEXT\0"
		"glSecondaryColor3hNV\0"
		"glSecondaryColor3hvNV\0"
		"glSecondaryColor3i\0"
		"glSecondaryColor3iEXT\0"
		"glSecondaryColor3iv\0"
		"glSecondaryColor3ivEXT\0"
		"glSecondaryColor3s\0"
		"glSecondaryColor3sEXT\0"
		"glSecondaryColor3sv\0"
		"glSecondaryColor3svEXT\0"
		"glSecondaryColor3ub\0"
		"glSecondaryColor3ubEXT\0"
		"glSecondaryColor3ubv\0"
		"glSecondaryColor3ubvEXT\0"
		"glSecondaryColor3ui\0"
		"glSecondaryColor3uiEXT\0"
		"glSecondaryColor3uiv\0"
		"glSecondaryColor3uivEXT\0"
		"glSecondaryColor3us\0"
		"glSecondaryColor3usEXT\0"
		"glSecondaryColor3usv\0"
		"glSecondaryColor3usvEXT\0"
		"glSecondaryColorPointer\0"
		"glSecondaryColorPointerEXT\0"
		"glSecondaryColorPointerListIBM\0"
		"glSelectBuffer\0"
		"glSelectTextureCoordSetSGIS\0"
		"glSelectTextureSGIS\0"
		"glSelectTextureTransformSGIS\0"
		"glSeparableFilter2D\0"
		"glSeparableFilter2DEXT\0"
		"glSetFenceAPPLE\0"
		"glSetFenceNV\0"
		"glSetFragmentShaderConstantATI\0"
		"glSetInvariantEXT\0"
		"glSetLocalConstantEXT\0"
		"glShadeModel\0"
		"glShaderOp1EXT\0"
		"glShaderOp2EXT\0"
		"glShaderOp3EXT\0"
		"glShaderSource\0"
		"glShaderSourceARB\0"
		"glSharpenTexFuncSGIS\0"
		"glSpriteParameterfSGIX\0"
		"glSpriteParameterfvSGIX\0"
		"glSpriteParameteriSGIX\0"
		"glSpriteParameterivSGIX\0"
		"glStartInstrumentsSGIX\0"
		"glStencilClearTagEXT\0"
		"glStencilFunc\0"
		"glStencilFuncSeparate\0"
		"glStencilFuncSeparateATI\0"
		"glStencilMask\0"
		"glStencilMaskSeparate\0"
		"glStencilOp\0"
		"glStencilOpSeparate\0"
		"glStencilOpSeparateATI\0"
		"glStopInstrumentsSGIX\0"
		"glStringMarkerGREMEDY\0"
		"glSwapAPPLE\0"
		"glSwizzleEXT\0"
		"glTagSampleBufferSGIX\0"
		"glTangent3bEXT\0"
		"glTangent3bvEXT\0"
		"glTangent3dEXT\0"
		"glTangent3dvEXT\0"
		"glTangent3fEXT\0"
		"glTangent3fvEXT\0"
		"glTangent3iEXT\0"
		"glTangent3ivEXT\0"
		"glTangent3sEXT\0"
		"glTangent3svEXT\0"
		"glTangentPointerEXT\0"
		"glTbufferMask3DFX\0"
		"glTestFenceAPPLE\0"
		"glTestFenceNV\0"
		"glTestObjectAPPLE\0"
		"glTexBufferEXT\0"
		"glTexBumpParameterfvATI\0"
		"glTexBumpParameterivATI\0"
		"glTexCoord1d\0"
		"glTexCoord1dv\0"
		"glTexCoord1f\0"
		"glTexCoord1fv\0"
		"glTexCoord1hNV\0"
		"glTexCoord1hvNV\0"
		"glTexCoord1i\0"
		"glTexCoord1iv\0"
		"glTexCoord1s\0"
		"glTexCoord1sv\0"
		"glTexCoord2d\0"
		"glTexCoord2dv\0"
		"glTexCoord2f\0"
		"glTexCoord2fColor3fVertex3fSUN\0"
		"glTexCoord2fColor3fVertex3fvSUN\0"
		"glTexCoord2fColor4fNormal3fVertex3fSUN\0"
		"glTexCoord2fColor4fNormal3fVertex3fvSUN\0"
		"glTexCoord2fColor4ubVertex3fSUN\0"
		"glTexCoord2fColor4ubVertex3fvSUN\0"
		"glTexCoord2fNormal3fVertex3fSUN\0"
		"glTexCoord2fNormal3fVertex3fvSUN\0"
		"glTexCoord2fv\0"
		"glTexCoord2fVertex3fSUN\0"
		"glTexCoord2fVertex3fvSUN\0"
		"glTexCoord2hNV\0"
		"glTexCoord2hvNV\0"
		"glTexCoord2i\0"
		"glTexCoord2iv\0"
		"glTexCoord2s\0"
		"glTexCoord2sv\0"
		"glTexCoord3d\0"
		"glTexCoord3dv\0"
		"glTexCoord3f\0"
		"glTexCoord3fv\0"
		"glTexCoord3hNV\0"
		"glTexCoord3hvNV\0"
		"glTexCoord3i\0"
		"glTexCoord3iv\0"
		"glTexCoord3s\0"
		"glTexCoord3sv\0"
		"glTexCoord4d\0"
		"glTexCoord4dv\0"
		"glTexCoord4f\0"
		"glTexCoord4fColor4fNormal3fVertex4fSUN\0"
		"glTexCoord4fColor4fNormal3fVertex4fvSUN\0"
		"glTexCoord4fv\0"
		"glTexCoord4fVertex4fSUN\0"
		"glTexCoord4fVertex4fvSUN\0"
		"glTexCoord4hNV\0"
		"glTexCoord4hvNV\0"
		"glTexCoord4i\0"
		"glTexCoord4iv\0"
		"glTexCoord4s\0"
		"glTexCoord4sv\0"
		"glTexCoordPointer\0"
		"glTexCoordPointerEXT\0"
		"glTexCoordPointerListIBM\0"
		"glTexCoordPointervINTEL\0"
		"glTexEnvf\0"
		"glTexEnvfv\0"
		"glTexEnvi\0"
		"glTexEnviv\0"
		"glTexFilterFuncSGIS\0"
		"glTexGend\0"
		"glTexGendv\0"
		"glTexGenf\0"
		"glTexGenfv\0"
		"glTexGeni\0"
		"glTexGeniv\0"
		"glTexImage1D\0"
		"glTexImage2D\0"
		"glTexImage3D\0"
		"glTexImage3DEXT\0"
		"glTexImage4DSGIS\0"
		"glTexParameterf\0"
		"glTexParameterfv\0"
		"glTexParameteri\0"
		"glTexParameterIivEXT\0"
		"glTexParameterIuivEXT\0"
		"glTexParameteriv\0"
		"glTexScissorFuncINTEL\0"
		"glTexScissorINTEL\0"
		"glTexSubImage1D\0"
		"glTexSubImage1DEXT\0"
		"glTexSubImage2D\0"
		"glTexSubImage2DEXT\0"
		"glTexSubImage3D\0"
		"glTexSubImage3DEXT\0"
		"glTexSubImage4DSGIS\0"
		"glTextureColorMaskSGIS\0"
		"glTextureFogSGIX\0"
		"glTextureLightEXT\0"
		"glTextureMaterialEXT\0"
		"glTextureNormalEXT\0"
		"glTextureRangeAPPLE\0"
		"glTrackMatrixNV\0"
		"glTransformFeedbackAttribsNV\0"
		"glTransformFeedbackVaryingsNV\0"
		"glTranslated\0"
		"glTranslatef\0"
		"glUniform1f\0"
		"glUniform1fARB\0"
		"glUniform1fv\0"
		"glUniform1fvARB\0"
		"glUniform1i\0"
		"glUniform1iARB\0"
		"glUniform1iv\0"
		"glUniform1ivARB\0"
		"glUniform1uiEXT\0"
		"glUniform1uivEXT\0"
		"glUniform2f\0"
		"glUniform2fARB\0"
		"glUniform2fv\0"
		"glUniform2fvARB\0"
		"glUniform2i\0"
		"glUniform2iARB\0"
		"glUniform2iv\0"
		"glUniform2ivARB\0"
		"glUniform2uiEXT\0"
		"glUniform2uivEXT\0"
		"glUniform3f\0"
		"glUniform3fARB\0"
		"glUniform3fv\0"
		"glUniform3fvARB\0"
		"glUniform3i\0"
		"glUniform3iARB\0"
		"glUniform3iv\0"
		"glUniform3ivARB\0"
		"glUniform3uiEXT\0"
		"glUniform3uivEXT\0"
		"glUniform4f\0"
		"glUniform4fARB\0"
		"glUniform4fv\0"
		"glUniform4fvARB\0"
		"glUniform4i\0"
		"glUniform4iARB\0"
		"glUniform4iv\0"
		"glUniform4ivARB\0"
		"glUniform4uiEXT\0"
		"glUniform4uivEXT\0"
		"glUniformBufferEXT\0"
		"glUniformMatrix2fv\0"
		"glUniformMatrix2fvARB\0"
		"glUniformMatrix2x3fv\0"
		"glUniformMatrix2x4fv\0"
		"glUniformMatrix3fv\0"
		"glUniformMatrix3fvARB\0"
		"glUniformMatrix3x2fv\0"
		"glUniformMatrix3x4fv\0"
		"glUniformMatrix4fv\0"
		"glUniformMatrix4fvARB\0"
		"glUniformMatrix4x2fv\0"
		"glUniformMatrix4x3fv\0"
		"glUnlockArraysEXT\0"
		"glUnmapBuffer\0"
		"glUnmapBufferARB\0"
		"glUnmapObjectBufferATI\0"
		"glUnmapTexture3DATI\0"
		"glUpdateObjectBufferATI\0"
		"glUseProgram\0"
		"glUseProgramObjectARB\0"
		"glValidateProgram\0"
		"glValidateProgramARB\0"
		"glValidBackBufferHintAutodesk\0"
		"glVariantArrayObjectATI\0"
		"glVariantbvEXT\0"
		"glVariantdvEXT\0"
		"glVariantfvEXT\0"
		"glVariantivEXT\0"
		"glVariantPointerEXT\0"
		"glVariantsvEXT\0"
		"glVariantubvEXT\0"
		"glVariantuivEXT\0"
		"glVariantusvEXT\0"
		"glVertex2d\0"
		"glVertex2dv\0"
		"glVertex2f\0"
		"glVertex2fv\0"
		"glVertex2hNV\0"
		"glVertex2hvNV\0"
		"glVertex2i\0"
		"glVertex2iv\0"
		"glVertex2s\0"
		"glVertex2sv\0"
		"glVertex3d\0"
		"glVertex3dv\0"
		"glVertex3f\0"
		"glVertex3fv\0"
		"glVertex3hNV\0"
		"glVertex3hvNV\0"
		"glVertex3i\0"
		"glVertex3iv\0"
		"glVertex3s\0"
		"glVertex3sv\0"
		"glVertex4d\0"
		"glVertex4dv\0"
		"glVertex4f\0"
		"glVertex4fv\0"
		"glVertex4hNV\0"
		"glVertex4hvNV\0"
		"glVertex4i\0"
		"glVertex4iv\0"
		"glVertex4s\0"
		"glVertex4sv\0"
		"glVertexArrayParameteriAPPLE\0"
		"glVertexArrayRangeAPPLE\0"
		"glVertexArrayRangeNV\0"
		"glVertexAttrib1d\0"
		"glVertexAttrib1dARB\0"
		"glVertexAttrib1dNV\0"
		"glVertexAttrib1dv\0"
		"glVertexAttrib1dvARB\0"
		"glVertexAttrib1dvNV\0"
		"glVertexAttrib1f\0"
		"glVertexAttrib1fARB\0"
		"glVertexAttrib1fNV\0"
		"glVertexAttrib1fv\0"
		"glVertexAttrib1fvARB\0"
		"glVertexAttrib1fvNV\0"
		"glVertexAttrib1hNV\0"
		"glVertexAttrib1hvNV\0"
		"glVertexAttrib1s\0"
		"glVertexAttrib1sARB\0"
		"glVertexAttrib1sNV\0"
		"glVertexAttrib1sv\0"
		"glVertexAttrib1svARB\0"
		"glVertexAttrib1svNV\0"
		"glVertexAttrib2d\0"
		"glVertexAttrib2dARB\0"
		"glVertexAttrib2dNV\0"
		"glVertexAttrib2dv\0"
		"glVertexAttrib2dvARB\0"
		"glVertexAttrib2dvNV\0"
		"glVertexAttrib2f\0"
		"glVertexAttrib2fARB\0"
		"glVertexAttrib2fNV\0"
		"glVertexAttrib2fv\0"
		"glVertexAttrib2fvARB\0"
		"glVertexAttrib2fvNV\0"
		"glVertexAttrib2hNV\0"
		"glVertexAttrib2hvNV\0"
		"glVertexAttrib2s\0"
		"glVertexAttrib2sARB\0"
		"glVertexAttrib2sNV\0"
		"glVertexAttrib2sv\0"
		"glVertexAttrib2svARB\0"
		"glVertexAttrib2svNV\0"
		"glVertexAttrib3d\0"
		"glVertexAttrib3dARB\0"
		"glVertexAttrib3dNV\0"
		"glVertexAttrib3dv\0"
		"glVertexAttrib3dvARB\0"
		"glVertexAttrib3dvNV\0"
		"glVertexAttrib3f\0"
		"glVertexAttrib3fARB\0"
		"glVertexAttrib3fNV\0"
		"glVertexAttrib3fv\0"
		"glVertexAttrib3fvARB\0"
		"glVertexAttrib3fvNV\0"
		"glVertexAttrib3hNV\0"
		"glVertexAttrib3hvNV\0"
		"glVertexAttrib3s\0"
		"glVertexAttrib3sARB\0"
		"glVertexAttrib3sNV\0"
		"glVertexAttrib3sv\0"
		"glVertexAttrib3svARB\0"
		"glVertexAttrib3svNV\0"
		"glVertexAttrib4bv\0"
		"glVertexAttrib4bvARB\0"
		"glVertexAttrib4d\0"
		"glVertexAttrib4dARB\0"
		"glVertexAttrib4dNV\0"
		"glVertexAttrib4dv\0"
		"glVertexAttrib4dvARB\0"
		"glVertexAttrib4dvNV\0"
		"glVertexAttrib4f\0"
		"glVertexAttrib4fARB\0"
		"glVertexAttrib4fNV\0"
		"glVertexAttrib4fv\0"
		"glVertexAttrib4fvARB\0"
		"glVertexAttrib4fvNV\0"
		"glVertexAttrib4hNV\0"
		"glVertexAttrib4hvNV\0"
		"glVertexAttrib4iv\0"
		"glVertexAttrib4ivARB\0"
		"glVertexAttrib4Nbv\0"
		"glVertexAttrib4NbvARB\0"
		"glVertexAttrib4Niv\0"
		"glVertexAttrib4NivARB\0"
		"glVertexAttrib4Nsv\0"
		"glVertexAttrib4NsvARB\0"
		"glVertexAttrib4Nub\0"
		"glVertexAttrib4NubARB\0"
		"glVertexAttrib4Nubv\0"
		"glVertexAttrib4NubvARB\0"
		"glVertexAttrib4Nuiv\0"
		"glVertexAttrib4NuivARB\0"
		"glVertexAttrib4Nusv\0"
		"glVertexAttrib4NusvARB\0"
		"glVertexAttrib4s\0"
		"glVertexAttrib4sARB\0"
		"glVertexAttrib4sNV\0"
		"glVertexAttrib4sv\0"
		"glVertexAttrib4svARB\0"
		"glVertexAttrib4svNV\0"
		"glVertexAttrib4ubNV\0"
		"glVertexAttrib4ubv\0"
		"glVertexAttrib4ubvARB\0"
		"glVertexAttrib4ubvNV\0"
		"glVertexAttrib4uiv\0"
		"glVertexAttrib4uivARB\0"
		"glVertexAttrib4usv\0"
		"glVertexAttrib4usvARB\0"
		"glVertexAttribArrayObjectATI\0"
		"glVertexAttribI1iEXT\0"
		"glVertexAttribI1ivEXT\0"
		"glVertexAttribI1uiEXT\0"
		"glVertexAttribI1uivEXT\0"
		"glVertexAttribI2iEXT\0"
		"glVertexAttribI2ivEXT\0"
		"glVertexAttribI2uiEXT\0"
		"glVertexAttribI2uivEXT\0"
		"glVertexAttribI3iEXT\0"
		"glVertexAttribI3ivEXT\0"
		"glVertexAttribI3uiEXT\0"
		"glVertexAttribI3uivEXT\0"
		"glVertexAttribI4bvEXT\0"
		"glVertexAttribI4iEXT\0"
		"glVertexAttribI4ivEXT\0"
		"glVertexAttribI4svEXT\0"
		"glVertexAttribI4ubvEXT\0"
		"glVertexAttribI4uiEXT\0"
		"glVertexAttribI4uivEXT\0"
		"glVertexAttribI4usvEXT\0"
		"glVertexAttribIPointerEXT\0"
		"glVertexAttribPointer\0"
		"glVertexAttribPointerARB\0"
		"glVertexAttribPointerNV\0"
		"glVertexAttribs1dvNV\0"
		"glVertexAttribs1fvNV\0"
		"glVertexAttribs1hvNV\0"
		"glVertexAttribs1svNV\0"
		"glVertexAttribs2dvNV\0"
		"glVertexAttribs2fvNV\0"
		"glVertexAttribs2hvNV\0"
		"glVertexAttribs2svNV\0"
		"glVertexAttribs3dvNV\0"
		"glVertexAttribs3fvNV\0"
		"glVertexAttribs3hvNV\0"
		"glVertexAttribs3svNV\0"
		"glVertexAttribs4dvNV\0"
		"glVertexAttribs4fvNV\0"
		"glVertexAttribs4hvNV\0"
		"glVertexAttribs4svNV\0"
		"glVertexAttribs4ubvNV\0"
		"glVertexBlendARB\0"
		"glVertexBlendEnvfATI\0"
		"glVertexBlendEnviATI\0"
		"glVertexPointer\0"
		"glVertexPointerEXT\0"
		"glVertexPointerListIBM\0"
		"glVertexPointervINTEL\0"
		"glVertexStream1dATI\0"
		"glVertexStream1dvATI\0"
		"glVertexStream1fATI\0"
		"glVertexStream1fvATI\0"
		"glVertexStream1iATI\0"
		"glVertexStream1ivATI\0"
		"glVertexStream1sATI\0"
		"glVertexStream1svATI\0"
		"glVertexStream2dATI\0"
		"glVertexStream2dvATI\0"
		"glVertexStream2fATI\0"
		"glVertexStream2fvATI\0"
		"glVertexStream2iATI\0"
		"glVertexStream2ivATI\0"
		"glVertexStream2sATI\0"
		"glVertexStream2svATI\0"
		"glVertexStream3dATI\0"
		"glVertexStream3dvATI\0"
		"glVertexStream3fATI\0"
		"glVertexStream3fvATI\0"
		"glVertexStream3iATI\0"
		"glVertexStream3ivATI\0"
		"glVertexStream3sATI\0"
		"glVertexStream3svATI\0"
		"glVertexStream4dATI\0"
		"glVertexStream4dvATI\0"
		"glVertexStream4fATI\0"
		"glVertexStream4fvATI\0"
		"glVertexStream4iATI\0"
		"glVertexStream4ivATI\0"
		"glVertexStream4sATI\0"
		"glVertexStream4svATI\0"
		"glVertexWeightfEXT\0"
		"glVertexWeightfvEXT\0"
		"glVertexWeighthNV\0"
		"glVertexWeighthvNV\0"
		"glVertexWeightPointerEXT\0"
		"glViewport\0"
		"glWeightbvARB\0"
		"glWeightdvARB\0"
		"glWeightfvARB\0"
		"glWeightivARB\0"
		"glWeightPointerARB\0"
		"glWeightsvARB\0"
		"glWeightubvARB\0"
		"glWeightuivARB\0"
		"glWeightusvARB\0"
		"glWindowBackBufferHintAutodesk\0"
		"glWindowPos2d\0"
		"glWindowPos2dARB\0"
		"glWindowPos2dMESA\0"
		"glWindowPos2dv\0"
		"glWindowPos2dvARB\0"
		"glWindowPos2dvMESA\0"
		"glWindowPos2f\0"
		"glWindowPos2fARB\0"
		"glWindowPos2fMESA\0"
		"glWindowPos2fv\0"
		"glWindowPos2fvARB\0"
		"glWindowPos2fvMESA\0"
		"glWindowPos2i\0"
		"glWindowPos2iARB\0"
		"glWindowPos2iMESA\0"
		"glWindowPos2iv\0"
		"glWindowPos2ivARB\0"
		"glWindowPos2ivMESA\0"
		"glWindowPos2s\0"
		"glWindowPos2sARB\0"
		"glWindowPos2sMESA\0"
		"glWindowPos2sv\0"
		"glWindowPos2svARB\0"
		"glWindowPos2svMESA\0"
		"glWindowPos3d\0"
		"glWindowPos3dARB\0"
		"glWindowPos3dMESA\0"
		"glWindowPos3dv\0"
		"glWindowPos3dvARB\0"
		"glWindowPos3dvMESA\0"
		"glWindowPos3f\0"
		"glWindowPos3fARB\0"
		"glWindowPos3fMESA\0"
		"glWindowPos3fv\0"
		"glWindowPos3fvARB\0"
		"glWindowPos3fvMESA\0"
		"glWindowPos3i\0"
		"glWindowPos3iARB\0"
		"glWindowPos3iMESA\0"
		"glWindowPos3iv\0"
		"glWindowPos3ivARB\0"
		"glWindowPos3ivMESA\0"
		"glWindowPos3s\0"
		"glWindowPos3sARB\0"
		"glWindowPos3sMESA\0"
		"glWindowPos3sv\0"
		"glWindowPos3svARB\0"
		"glWindowPos3svMESA\0"
		"glWindowPos4dMESA\0"
		"glWindowPos4dvMESA\0"
		"glWindowPos4fMESA\0"
		"glWindowPos4fvMESA\0"
		"glWindowPos4iMESA\0"
		"glWindowPos4ivMESA\0"
		"glWindowPos4sMESA\0"
		"glWindowPos4svMESA\0"
		"glWriteMaskEXT\0"
#ifndef WIN32
		"glXAllocateMemoryMESA\0"
		"glXAllocateMemoryNV\0"
		"glXBindChannelToWindowSGIX\0"
		"glXBindHyperpipeSGIX\0"
		"glXBindSwapBarrierNV\0"
		"glXBindSwapBarrierSGIX\0"
		"glXBindTexImageARB\0"
		"glXBindTexImageATI\0"
		"glXBindTexImageEXT\0"
		"glXBindVideoImageNV\0"
		"glXChannelRectSGIX\0"
		"glXChannelRectSyncSGIX\0"
		"glXChooseFBConfig\0"
		"glXChooseFBConfigSGIX\0"
		"glXChooseVisual\0"
		"glXCopyContext\0"
		"glXCopySubBufferMESA\0"
		"glXCreateContext\0"
		"glXCreateContextWithConfigSGIX\0"
		"glXCreateGLXPbufferSGIX\0"
		"glXCreateGLXPixmap\0"
		"glXCreateGLXPixmapMESA\0"
		"glXCreateGLXPixmapWithConfigSGIX\0"
		"glXCreateNewContext\0"
		"glXCreatePbuffer\0"
		"glXCreatePixmap\0"
		"glXCreateWindow\0"
		"glXCushionSGI\0"
		"glXDestroyContext\0"
		"glXDestroyGLXPbufferSGIX\0"
		"glXDestroyGLXPixmap\0"
		"glXDestroyHyperpipeConfigSGIX\0"
		"glXDestroyPbuffer\0"
		"glXDestroyPixmap\0"
		"glXDestroyWindow\0"
		"glXDrawableAttribARB\0"
		"glXDrawableAttribATI\0"
		"glXFreeContextEXT\0"
		"glXFreeMemoryMESA\0"
		"glXFreeMemoryNV\0"
		"glXGetAGPOffsetMESA\0"
		"glXGetClientString\0"
		"glXGetConfig\0"
		"glXGetContextIDEXT\0"
		"glXGetCurrentContext\0"
		"glXGetCurrentDisplay\0"
		"glXGetCurrentDisplayEXT\0"
		"glXGetCurrentDrawable\0"
		"glXGetCurrentReadDrawable\0"
		"glXGetCurrentReadDrawableSGI\0"
		"glXGetDriverConfig\0"
		"glXGetFBConfigAttrib\0"
		"glXGetFBConfigAttribSGIX\0"
		"glXGetFBConfigFromVisualSGIX\0"
		"glXGetFBConfigs\0"
		"glXGetMemoryOffsetMESA\0"
		"glXGetMscRateOML\0"
		"glXGetProcAddress\0"
		"glXGetProcAddressARB\0"
		"glXGetRefreshRateSGI\0"
		"glXGetScreenDriver\0"
		"glXGetSelectedEvent\0"
		"glXGetSelectedEventSGIX\0"
		"glXGetSyncValuesOML\0"
		"glXGetTransparentIndexSUN\0"
		"glXGetVideoDeviceNV\0"
		"glXGetVideoInfoNV\0"
		"glXGetVideoResizeSUN\0"
		"glXGetVideoSyncSGI\0"
		"glXGetVisualFromFBConfig\0"
		"glXGetVisualFromFBConfigSGIX\0"
		"glXHyperpipeAttribSGIX\0"
		"glXHyperpipeConfigSGIX\0"
		"glXImportContextEXT\0"
		"glXIsDirect\0"
		"glXJoinSwapGroupNV\0"
		"glXJoinSwapGroupSGIX\0"
		"glXMakeContextCurrent\0"
		"glXMakeCurrent\0"
		"glXMakeCurrentReadSGI\0"
		"glXQueryChannelDeltasSGIX\0"
		"glXQueryChannelRectSGIX\0"
		"glXQueryContext\0"
		"glXQueryContextInfoEXT\0"
		"glXQueryDrawable\0"
		"glXQueryExtension\0"
		"glXQueryExtensionsString\0"
		"glXQueryFrameCountNV\0"
		"glXQueryGLXPbufferSGIX\0"
		"glXQueryHyperpipeAttribSGIX\0"
		"glXQueryHyperpipeBestAttribSGIX\0"
		"glXQueryHyperpipeConfigSGIX\0"
		"glXQueryHyperpipeNetworkSGIX\0"
		"glXQueryMaxSwapBarriersSGIX\0"
		"glXQueryMaxSwapGroupsNV\0"
		"glXQueryServerString\0"
		"glXQuerySwapGroupNV\0"
		"glXQueryVersion\0"
		"glXReleaseBuffersMESA\0"
		"glXReleaseTexImageARB\0"
		"glXReleaseTexImageATI\0"
		"glXReleaseTexImageEXT\0"
		"glXReleaseVideoDeviceNV\0"
		"glXReleaseVideoImageNV\0"
		"glXResetFrameCountNV\0"
		"glXSelectEvent\0"
		"glXSelectEventSGIX\0"
		"glXSendPbufferToVideoNV\0"
		"glXSet3DfxModeMESA\0"
		"glXSwapBuffers\0"
		"glXSwapBuffersMscOML\0"
		"glXSwapIntervalSGI\0"
		"glXUseXFont\0"
		"glXVideoResizeSUN\0"
		"glXWaitForMscOML\0"
		"glXWaitForSbcOML\0"
		"glXWaitGL\0"
		"glXWaitVideoSyncSGI\0"
		"glXWaitX\0"
#endif
		"\0"
};

typedef struct
{
	unsigned int hash;
	char* name;
	__GLXextFuncPtr func;
	int found_on_client_side;
	int found_on_server_side;
	int alreadyAsked;
} AssocProcAdress;

#ifdef PROVIDE_STUB_IMPLEMENTATION
static void _glStubImplementation()
{
	log_gl("This function is a stub !!!\n");
}
#endif

static __GLXextFuncPtr glXGetProcAddress_no_lock(const GLubyte * _name)
{
	static int nbElts = 0;
	static int tabSize = 0;
	static AssocProcAdress* tab_assoc = NULL;
	static void* handle = NULL;
	__GLXextFuncPtr ret = NULL;

	const char* name = (const char*)_name;
	log_gl("looking for \"%s\",\n", name);
	int i;

	if (name == NULL)
	{
		goto end_of_glx_get_proc_address;
	}

#ifdef WIN32
	if (strcmp(name, "wglGetExtensionsStringARB") == 0 ||
			strcmp(name, "wglGetExtensionsStringEXT") == 0)
	{
		ret = wglGetExtensionsStringARB;
		goto end_of_glx_get_proc_address;
	}
#endif
	if (tabSize == 0)
	{
		tabSize = 2000;
		tab_assoc = calloc(tabSize, sizeof(AssocProcAdress));

#ifndef WIN32
		handle = dlopen(getenv("REAL_LIBGL") ? getenv("REAL_LIBGL") : "libGL.so" ,RTLD_LAZY);
		if (!handle) {
			log_gl("%s\n", dlerror());
			exit(1);
		}
#else
		handle = (void *)LoadLibrary("opengl32.dll");
		if (!handle) {
			log_gl("can't load opengl32.dll\n");
			exit(1);
		}
#endif

		{
			log_gl("global_glXGetProcAddress request\n");
			int sizeOfString = 0;
			int nbRequestElts = 0;
			int i = 0;
			while(1)
			{
				if (global_glXGetProcAddress_request[i] == '\0')
				{
					nbRequestElts++;
					if (global_glXGetProcAddress_request[i + 1] == '\0')
					{
						sizeOfString = i + 1;
						break;
					}
				}
				i++;
			}
			log_gl("nbRequestElts=%d\n", nbRequestElts);
			char* result = (char*)malloc(nbRequestElts);
			long args[] = { INT_TO_ARG(nbRequestElts), POINTER_TO_ARG(global_glXGetProcAddress_request), POINTER_TO_ARG(result) };
			int args_size[] = { 0, sizeOfString, nbRequestElts };
			do_opengl_call_no_lock(glXGetProcAddress_global_fake_func, NULL, CHECK_ARGS(args, args_size));
			int offset = 0;
			for(i=0; i<nbRequestElts;i++)
			{
				const char* funcName = global_glXGetProcAddress_request + offset;
#ifndef WIN32
				void* func = dlsym(handle, funcName);
#else
				void* func = GetProcAddress(handle, funcName);
#endif
#ifdef PROVIDE_STUB_IMPLEMENTATION
				if (func == NULL)
					func = _glStubImplementation;
#endif
				if (result[i] && func == NULL)
					log_gl("%s %d %d\n", funcName, result[i], func != NULL);

				if (result[i] == 0)
				{
					if (nbElts < tabSize)
					{
						int hash = str_hash(funcName);
						tab_assoc[nbElts].alreadyAsked = 0;
						tab_assoc[nbElts].found_on_server_side = 0;
						tab_assoc[nbElts].found_on_client_side = func != NULL;
						tab_assoc[nbElts].hash = hash;
						tab_assoc[nbElts].name = strdup(funcName);
						tab_assoc[nbElts].func = NULL;
						nbElts ++;
					}
				}
				else
				{
					if (nbElts < tabSize)
					{
						int hash = str_hash(funcName);
						tab_assoc[nbElts].alreadyAsked = 0;
						tab_assoc[nbElts].found_on_server_side = 1;
						tab_assoc[nbElts].found_on_client_side = func != NULL;
						tab_assoc[nbElts].hash = hash;
						tab_assoc[nbElts].name = strdup(funcName);
						tab_assoc[nbElts].func = func;
						nbElts ++;
					}
				}
				offset += strlen(funcName) + 1;
			}
			free(result);
			ret = glXGetProcAddress_no_lock(_name);
			goto end_of_glx_get_proc_address;
		}
	}

	int hash = str_hash(name);
	for(i=0;i<nbElts;i++)
	{
		if (tab_assoc[i].hash == hash && strcmp(tab_assoc[i].name, name) == 0)
		{
			if (tab_assoc[i].alreadyAsked == 0)
			{
				tab_assoc[i].alreadyAsked = 1;
				if (tab_assoc[i].found_on_server_side == 0 && tab_assoc[i].found_on_client_side == 0)
				{
					log_gl("not found name on server and client side = %s\n", name);
				}
				else if (tab_assoc[i].found_on_server_side == 0)
				{
					log_gl("not found name on server side = %s\n", name);
				}
				else if (tab_assoc[i].found_on_client_side == 0)
				{
					log_gl("not found name on client side = %s\n", name);
				}
			}
			ret = (__GLXextFuncPtr)tab_assoc[i].func;
			goto end_of_glx_get_proc_address;
		}
	}
	log_gl("looking for \"%s\",\n", name);
	int ret_call = 0;
	long args[] = { INT_TO_ARG(name) };
	do_opengl_call_no_lock(glXGetProcAddress_fake_func, &ret_call, args, NULL);
#ifndef WIN32
	void* func = dlsym(handle, name);
#else
	void* func = GetProcAddress(handle, name);
#endif
#ifdef PROVIDE_STUB_IMPLEMENTATION
	if (func == NULL)
		func = _glStubImplementation;
#endif
	if (ret_call == 0)
	{
		if (nbElts < tabSize)
		{
			tab_assoc[nbElts].alreadyAsked = 1;
			tab_assoc[nbElts].found_on_server_side = 0;
			tab_assoc[nbElts].found_on_client_side = func != NULL;
			tab_assoc[nbElts].hash = hash;
			tab_assoc[nbElts].name = strdup(name);
			tab_assoc[nbElts].func = NULL;
			nbElts ++;
		}
		if (func == NULL)
		{
			log_gl("not found name on server and client side = %s\n", name);
		}
		else
		{
			log_gl("not found name on server side = %s\n", name);
		}
		ret = NULL;
		goto end_of_glx_get_proc_address;
	}
	else
	{
		if (func == NULL)
		{
			log_gl("not found name on client side = %s\n", name);
		}
		if (nbElts < tabSize)
		{
			tab_assoc[nbElts].alreadyAsked = 1;
			tab_assoc[nbElts].found_on_server_side = 1;
			tab_assoc[nbElts].found_on_client_side = func != NULL;
			tab_assoc[nbElts].hash = hash;
			tab_assoc[nbElts].name = strdup(name);
			tab_assoc[nbElts].func = func;
			nbElts ++;
		}
		ret = func;
		goto end_of_glx_get_proc_address;
	}
end_of_glx_get_proc_address:
	return ret;
}

__GLXextFuncPtr glXGetProcAddress(const GLubyte * name)
{
	__GLXextFuncPtr ret;
	LOCK(glXGetProcAddress_fake_func);
	ret = glXGetProcAddress_no_lock(name);
	UNLOCK(glXGetProcAddress_fake_func);
	return ret;
}

__GLXextFuncPtr glXGetProcAddressARB (const GLubyte * name)
{
	return glXGetProcAddress(name);
}

#ifdef WIN32

WINGDIAPI PROC WINAPI wglGetProcAddress(LPCSTR _name)
{
	return glXGetProcAddress(_name);
}

WINGDIAPI PROC WINAPI wglGetDefaultProcAddress(LPCSTR _name)
{
	return glXGetProcAddress(_name);
}

WINGDIAPI int WINAPI wglChoosePixelFormat(HDC hdc,CONST PIXELFORMATDESCRIPTOR* b)
{
	log_gl("wglChoosePixelFormat : stub\n");
	return 1; /* FIXME ? */
}

WINGDIAPI BOOL  WINAPI wglCopyContext           (HGLRC a, HGLRC ctxt, UINT c)
{
	log_gl("wglCopyContext : stub\n");
	return 0;
}

/*
   static void __attribute__ ((constructor)) gl_init (void)
   {
   MessageBox(NULL, "gl_init", "gl_init", 0);
   }*/

WINGDIAPI HGLRC WINAPI wglCreateContext         (HDC hdc)
{
	HGLRC ret;
	long args[] = { POINTER_TO_ARG(/*dpy*/0), INT_TO_ARG(/*visualid*/0), INT_TO_ARG(/*shareList*/0), INT_TO_ARG(/*direct*/1) };
	do_opengl_call(glXCreateContext_func, &ret, args, NULL);
	/*MessageBox(NULL, "wglCreateContext", "wglCreateContext", 0);*/
	return ret;
}

WINGDIAPI HGLRC WINAPI wglCreateLayerContext    (HDC hdc, int b)
{
	log_gl("wglCreateLayerContext : stub\n");
	return 0;
}

WINGDIAPI BOOL  WINAPI wglDeleteContext         (HGLRC a)
{
	long args[] = { POINTER_TO_ARG(/*dpy*/0), POINTER_TO_ARG(a) };
	do_opengl_call(glXDestroyContext_func, NULL, args, NULL);

	/* FIXME ! */
	call_opengl(_exit_process_func, getpid(), NULL, NULL, NULL);
	close(fd);

	return 1;
}

WINGDIAPI BOOL  WINAPI wglDescribeLayerPlane    (HDC hdc, int b, int c, UINT d, LPLAYERPLANEDESCRIPTOR e)
{
	log_gl("wglDescribeLayerPlane : stub\n");
	return 0;
}

WINGDIAPI int   WINAPI wglDescribePixelFormat   (HDC hdc, int b, UINT c, LPPIXELFORMATDESCRIPTOR d)
{
	log_gl("wglDescribePixelFormat : stub\n");
	return 0;
}

WINGDIAPI HGLRC WINAPI wglGetCurrentContext     (void)
{
	log_gl("wglGetCurrentContext : stub\n");
	return 0;
}
WINGDIAPI HDC   WINAPI wglGetCurrentDC          (void)
{
	log_gl("wglGetCurrentDC : stub\n");
	return 0;
}

WINGDIAPI int   WINAPI wglGetLayerPaletteEntries(HDC hdc, int b, int c, int d, COLORREF *e)
{
	log_gl("wglGetLayerPaletteEntries : stub\n");
	return 0;
}

WINGDIAPI int   WINAPI wglGetPixelFormat        (HDC hdc)
{
	log_gl("wglGetPixelFormat : stub\n");
	return 0;
}

#define IsViewable         2

static RECT oldRect = {0, 0, 0, 0};

static void _move_win_if_necessary(HDC hdc)
{
	HWND hwnd = WindowFromDC(hdc);
	if (hwnd)
	{
		RECT rect;
		if (GetWindowRect(hwnd, &rect) == 0)
		{
			log_gl("arg. cannot retrieve window size\n");
			return;
		}
		if (oldRect.left != rect.left || oldRect.top != rect.top ||
				oldRect.right != rect.right || oldRect.bottom != rect.bottom)
		{
			{
				long args[] = { INT_TO_ARG(hdc), INT_TO_ARG(IsViewable) }; /* FIXME */
				do_opengl_call(_changeWindowState_func, NULL, args, NULL);
			}
			{
				int standardizedPos[] = { rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top };
				long args[] = { INT_TO_ARG(hdc), POINTER_TO_ARG(standardizedPos)};
				do_opengl_call(_moveResizeWindow_func, NULL, args, NULL);
			}

			memcpy(&oldRect, &rect, sizeof(rect));
		}
	}
	else
	{
		log_gl("arg. cannot retrieve window from DC\n");
	}
}

WINGDIAPI BOOL  WINAPI wglMakeCurrent(HDC hdc, HGLRC ctxt)
{
	long args[] = { POINTER_TO_ARG(/*dpy*/0), INT_TO_ARG(/*drawable*/hdc), INT_TO_ARG(/*ctx*/ctxt) };
	do_opengl_call(glXMakeCurrent_func, NULL, args, NULL);
	if (hdc != NULL)
		_move_win_if_necessary(hdc);
	return 1;
}

WINGDIAPI BOOL  WINAPI wglRealizeLayerPalette   (HDC hdc, int b, BOOL c)
{
	log_gl("wglRealizeLayerPalette : stub\n");
	return 0;
}

WINGDIAPI int   WINAPI wglSetLayerPaletteEntries(HDC hdc, int b, int c, int d, CONST COLORREF *e)
{
	log_gl("wglSetLayerPaletteEntries : stub\n");
	return 0;
}

WINGDIAPI BOOL  WINAPI wglSetPixelFormat        (HDC hdc, int b, CONST PIXELFORMATDESCRIPTOR *c)
{
	log_gl("wglSetPixelFormat : stub\n");
	return 1; /* FIXME ? */
}

WINGDIAPI BOOL  WINAPI wglShareLists            (HGLRC a, HGLRC ctxt)
{
	log_gl("wglShareLists : stub\n");
	return 0;
}

WINGDIAPI BOOL  WINAPI wglSwapBuffers           (HDC hdc)
{
	_move_win_if_necessary(hdc);
	long args[] = { POINTER_TO_ARG(/*dpy*/0), INT_TO_ARG(/*drawable*/hdc) };
	do_opengl_call(glXSwapBuffers_func, NULL, args, NULL);
	return 1;
}

WINGDIAPI BOOL  WINAPI wglSwapLayerBuffers      (HDC hdc, UINT b)
{
	log_gl("wglSwapLayerBuffers : stub\n");
	return 0;
}

WINGDIAPI BOOL  WINAPI wglUseFontBitmapsA       (HDC hdc, DWORD b, DWORD c, DWORD d)
{
	log_gl("wglUseFontBitmapsA : stub\n");
	return 0;
}

WINGDIAPI BOOL  WINAPI wglUseFontBitmapsW       (HDC hdc, DWORD b, DWORD c, DWORD d)
{
	log_gl("wglUseFontBitmapsW : stub\n");
	return 0;
}

WINGDIAPI BOOL  WINAPI wglUseFontOutlinesA      (HDC hdc, DWORD b, DWORD c, DWORD d, FLOAT e, FLOAT f, int g, LPGLYPHMETRICSFLOAT h)
{
	log_gl("wglUseFontOutlinesA : stub\n");
	return 0;
}

WINGDIAPI BOOL  WINAPI wglUseFontOutlinesW      (HDC hdc, DWORD b, DWORD c, DWORD d, FLOAT e, FLOAT f, int g, LPGLYPHMETRICSFLOAT h)
{
	log_gl("wglUseFontOutlinesW : stub\n");
	return 0;
}

#define GLX_EXTENSIONS 		3

WINGDIAPI const char* WINAPI wglGetExtensionsStringARB(HDC hdc)
{
	int i;
	char* glxClientExtensions = NULL;
	char* glxServerExtensions = NULL;
	char* glExtensions = NULL;
	static char* extensions = NULL;
	if (extensions)
		return extensions;

	{
		long args[] = { POINTER_TO_ARG(/*dpy*/0), INT_TO_ARG(GLX_EXTENSIONS) };
		do_opengl_call(glXGetClientString_func, &glxClientExtensions, args, NULL);
		glxClientExtensions = strdup(glxClientExtensions);
	}
	{
		long args[] = { POINTER_TO_ARG(/*dpy*/0), INT_TO_ARG(/*screen*/0), INT_TO_ARG(GLX_EXTENSIONS) };
		do_opengl_call(glXQueryServerString_func, &glxServerExtensions, args, NULL);
		glxServerExtensions = strdup(glxServerExtensions);
	}
	glExtensions = glGetString(GL_EXTENSIONS);

	extensions = malloc(strlen("WGL_ARB_extensions_string") + 1 +
			strlen(glxClientExtensions) + 1 +
			strlen(glxServerExtensions) + 1 +
			strlen(glExtensions) + 1);
	strcpy(extensions, "WGL_ARB_extensions_string ");
	strcat(extensions, glxClientExtensions);
	strcat(extensions, " ");
	strcat(extensions, glxServerExtensions);
	strcat(extensions, " ");
	strcat(extensions, glExtensions);

	free(glxClientExtensions);
	free(glxServerExtensions);

	i = 0;
	while(extensions[i])
	{
		if (strncmp(extensions + i, "GLX", 3) == 0)
		{
			extensions[i] = 'W';
			extensions[i+1] = 'G';
			extensions[i+2] = 'L';
		}
		i++;
	}

	return extensions;
}


#endif
