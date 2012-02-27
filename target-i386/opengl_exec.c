/*
 *  Host-side implementation of GL/GLX API
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


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void init_process_tab(void);

#ifdef _WIN32

#include <windows.h>

//#define _MK_DBG_

#define IsViewable 2
#define False 0
#define True 1


#define GL_GLEXT_PROTOTYPES
#include "opengl_func.h"
#include "opengl_utils.h"
#include "mesa_glx.h"
#include "mesa_glxext.h"
#include "mesa_glu.h"
#include "mesa_mipmap.c"

typedef struct
{
	HGLRC context;
	void *backBuffer;
	int width;
	int height;
	int colorFormat;
	unsigned int colorBits;
} PbufferInfo;

typedef struct
{
	void* key;
	void* value;
	void* hWnd; /* Surface?? À§?? Window Handle*/
	void* cDC; /*Context?? À§?? Dummy Window Handle*/
} Assoc;

#define MAX_HANDLED_PROCESS 100
#define MAX_ASSOC_SIZE 100
#define MAX_FBCONFIG 10
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

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
	GLbitfield     mask;
	int            activeTextureIndex;
} ClientState;

#define MAX_CLIENT_STATE_STACK_SIZE 16
#define NB_MAX_TEXTURES 16
#define MY_GL_MAX_VERTEX_ATTRIBS_ARB 16
#define MY_GL_MAX_VERTEX_ATTRIBS_NV 16
#define MY_GL_MAX_VARIANT_POINTER_EXT 16

typedef struct
{
	int ref;
	int fake_ctxt;
	int fake_shareList;
	HGLRC context;
	HDC drawable;

	void* vertexPointer;
	void* normalPointer;
	void* colorPointer;
	void* secondaryColorPointer;
	void* indexPointer;
	void* texCoordPointer[NB_MAX_TEXTURES];
	void* edgeFlagPointer;
	void* vertexAttribPointer[MY_GL_MAX_VERTEX_ATTRIBS_ARB];
	void* vertexAttribPointerNV[MY_GL_MAX_VERTEX_ATTRIBS_NV];
	void* weightPointer;
	void* matrixIndexPointer;
	void* fogCoordPointer;
	void* variantPointerEXT[MY_GL_MAX_VARIANT_POINTER_EXT];
	void* interleavedArrays;
	void* elementPointerATI;

	int vertexPointerSize;
	int normalPointerSize;
	int colorPointerSize;
	int secondaryColorPointerSize;
	int indexPointerSize;
	int texCoordPointerSize[NB_MAX_TEXTURES];
	int edgeFlagPointerSize;
	int vertexAttribPointerSize[MY_GL_MAX_VERTEX_ATTRIBS_ARB];
	int vertexAttribPointerNVSize[MY_GL_MAX_VERTEX_ATTRIBS_NV];
	int weightPointerSize;
	int matrixIndexPointerSize;
	int fogCoordPointerSize;
	int variantPointerEXTSize[MY_GL_MAX_VARIANT_POINTER_EXT];
	int interleavedArraysSize;
	int elementPointerATISize;

	int selectBufferSize;
	void* selectBufferPtr;
	int feedbackBufferSize;
	void* feedbackBufferPtr;

	ClientState clientStateStack[MAX_CLIENT_STATE_STACK_SIZE];
	int clientStateSp;
	int activeTextureIndex;

	unsigned int ownTabTextures[32768];
	unsigned int* tabTextures;
	RangeAllocator ownTextureAllocator;
	RangeAllocator* textureAllocator;

	unsigned int ownTabBuffers[32768];
	unsigned int* tabBuffers;
	RangeAllocator ownBufferAllocator;
	RangeAllocator* bufferAllocator;

	unsigned int ownTabLists[32768];
	unsigned int* tabLists;
	RangeAllocator ownListAllocator;
	RangeAllocator* listAllocator;

#ifdef SYSTEMATIC_ERROR_CHECK
	int last_error;
#endif
} GLState;

typedef struct
{
	PIXELFORMATDESCRIPTOR pfd;
	unsigned int visualID;
} WGLFBConfig;

typedef struct
{
	int internal_num;
	int process_id;
	int instr_counter;

	int x, y, width, height;
	WindowPosStruct currentDrawablePos;

	int next_available_context_number;
	int next_available_pbuffer_number;

	int nbGLStates;
	GLState default_state;
	GLState** glstates;
	GLState* current_state;

	int nfbconfig;
	WGLFBConfig* fbconfigs[MAX_FBCONFIG];
	int fbconfigs_max[MAX_FBCONFIG];
	int nfbconfig_total;

	Assoc association_fakecontext_glxcontext[MAX_ASSOC_SIZE];
	Assoc association_fakepbuffer_pbuffer[MAX_ASSOC_SIZE];
	Assoc association_clientdrawable_serverdrawable[MAX_ASSOC_SIZE];
	Assoc association_fakecontext_visual[MAX_ASSOC_SIZE];
} ProcessStruct;

int last_process_id = 0;


typedef struct
{
	int attribListLength;
	int* attribList;
	unsigned int visualID;
	/*PIXELFORMATDESCRIPTOR* visInfo;*/
} AssocAttribListVisual;

#define ARG_TO_CHAR(x)                (char)(x)
#define ARG_TO_UNSIGNED_CHAR(x)       (unsigned char)(x)
#define ARG_TO_SHORT(x)               (short)(x)
#define ARG_TO_UNSIGNED_SHORT(x)      (unsigned short)(x)
#define ARG_TO_INT(x)                 (int)(x)
#define ARG_TO_UNSIGNED_INT(x)        (unsigned int)(x)
#define ARG_TO_FLOAT(x)               (*(float*)&(x))
#define ARG_TO_DOUBLE(x)              (*(double*)(x))

#define GET_EXT_PTR(type, funcname, args_decl) \
	static int detect_##funcname = 0; \
static type(*ptr_func_##funcname)args_decl = NULL; \
if (detect_##funcname == 0) \
{ \
	detect_##funcname = 1; \
	ptr_func_##funcname = (type(*)args_decl)wglGetProcAddress((const GLubyte*)#funcname); \
	assert (ptr_func_##funcname); \
}

#define GET_EXT_PTR_NO_FAIL(type, funcname, args_decl) \
	static int detect_##funcname = 0; \
static type(*ptr_func_##funcname)args_decl = NULL; \
if (detect_##funcname == 0) \
{ \
	detect_##funcname = 1; \
	ptr_func_##funcname = (type(*)args_decl)wglGetProcAddress((const GLubyte*)#funcname); \
}


#include "server_stub.c"

static void* get_glu_ptr(const char* name)
{
	static void* handle = (void*)-1;
	if (handle == (void*)-1)
	{
#ifndef WIN32
		handle = dlopen("libGLU.so" ,RTLD_LAZY);
		if (!handle) {
			fprintf (stderr, "can't load libGLU.so : %s\n", dlerror());
		}
#else
		handle = (void *)LoadLibrary("glu32.dll");
		if (!handle) {
			fprintf (stderr, "can't load glu32.dll\n");
		}
#endif
	}
	if (handle)
	{
#ifndef WIN32
		return dlsym(handle, name);
#else
		return GetProcAddress(handle, name);
#endif
	}
	return NULL;
}



#define GET_GLU_PTR(type, funcname, args_decl) \
	static int detect_##funcname = 0; \
static type(*ptr_func_##funcname)args_decl = NULL; \
if (detect_##funcname == 0) \
{ \
	detect_##funcname = 1; \
	ptr_func_##funcname = (type(*)args_decl)get_glu_ptr(#funcname); \
}


static void _get_window_pos(HWND hWnd, WindowPosStruct* pos)
{
	RECT rcWindow;

	GetWindowRect(hWnd, &rcWindow);

	pos->x = rcWindow.left;
	pos->y = rcWindow.top;

	pos->width = rcWindow.right - rcWindow.left;
	pos->height = rcWindow.bottom - rcWindow.top;

	pos->map_state = IsViewable;
}


int display_function_call = 0;

static void init_gl_state(GLState* state)
{
	state->textureAllocator = &state->ownTextureAllocator;
	state->tabTextures = state->ownTabTextures;
	state->bufferAllocator = &state->ownBufferAllocator;
	state->tabBuffers = state->ownTabBuffers;
	state->listAllocator = &state->ownListAllocator;
	state->tabLists = state->ownTabLists;
}


static void destroy_gl_state(GLState* state)
{
	int i;
	if (state->vertexPointer) free(state->vertexPointer);
	if (state->normalPointer) free(state->normalPointer);
	if (state->indexPointer) free(state->indexPointer);
	if (state->colorPointer) free(state->colorPointer);
	if (state->secondaryColorPointer) free(state->secondaryColorPointer);
	for(i=0;i<NB_MAX_TEXTURES;i++)
	{
		if (state->texCoordPointer[i]) free(state->texCoordPointer[i]);
	}
	for(i=0;i<MY_GL_MAX_VERTEX_ATTRIBS_ARB;i++)
	{
		if (state->vertexAttribPointer[i]) free(state->vertexAttribPointer[i]);
	}
	for(i=0;i<MY_GL_MAX_VERTEX_ATTRIBS_NV;i++)
	{
		if (state->vertexAttribPointerNV[i]) free(state->vertexAttribPointerNV[i]);
	}
	if (state->weightPointer) free(state->weightPointer);
	if (state->matrixIndexPointer) free(state->matrixIndexPointer);
	if (state->fogCoordPointer) free(state->fogCoordPointer);
	for(i=0;i<MY_GL_MAX_VARIANT_POINTER_EXT;i++)
	{
		if (state->variantPointerEXT[i]) free(state->variantPointerEXT[i]);
	}
	if (state->interleavedArrays) free(state->interleavedArrays);
	if (state->elementPointerATI) free(state->elementPointerATI);
}

static void _create_context(ProcessStruct* process, HGLRC ctxt, int fake_ctxt, HGLRC shareList, int fake_shareList)
{
	process->glstates = realloc(process->glstates, (process->nbGLStates+1)*sizeof(GLState*));
	process->glstates[process->nbGLStates] = malloc(sizeof(GLState));
	memset(process->glstates[process->nbGLStates], 0, sizeof(GLState));
	process->glstates[process->nbGLStates]->ref = 1;
	process->glstates[process->nbGLStates]->context = ctxt;
	process->glstates[process->nbGLStates]->fake_ctxt = fake_ctxt;
	process->glstates[process->nbGLStates]->fake_shareList = fake_shareList;
	init_gl_state(process->glstates[process->nbGLStates]);
	if (shareList && fake_shareList)
	{
		int i;
		for(i=0;i<process->nbGLStates;i++)
		{
			if (process->glstates[i]->fake_ctxt == fake_shareList)
			{
				process->glstates[i]->ref ++;
				process->glstates[process->nbGLStates]->textureAllocator =
					process->glstates[i]->textureAllocator;
				process->glstates[process->nbGLStates]->tabTextures =
					process->glstates[i]->tabTextures;
				process->glstates[process->nbGLStates]->bufferAllocator =
					process->glstates[i]->bufferAllocator;
				process->glstates[process->nbGLStates]->tabBuffers =
					process->glstates[i]->tabBuffers;
				process->glstates[process->nbGLStates]->listAllocator =
					process->glstates[i]->listAllocator;
				process->glstates[process->nbGLStates]->tabLists =
					process->glstates[i]->tabLists;
				break;
			}
		}
	}
	process->nbGLStates++;
}

WGLFBConfig* get_pfDescriptor(ProcessStruct* process, int client_pfd)
{
	int i;
	int nbtotal = 0;
	for(i=0;i<process->nfbconfig;i++)
	{
		assert(client_pfd >= 1 + nbtotal);
		if (client_pfd <= nbtotal + process->fbconfigs_max[i])
		{
			return (WGLFBConfig*) (&process->fbconfigs[i][client_pfd-1 - nbtotal]);
		}
		nbtotal += process->fbconfigs_max[i];
	}
	return 0;
}

int get_pfdAttrib(PIXELFORMATDESCRIPTOR *ppfd, int attrib, int *value)
{
	int answer = 0;

	switch(attrib)
	{
		case GLX_FBCONFIG_ID:
			answer = (int) ppfd;
			break;

		case GLX_AUX_BUFFERS:
			answer = ppfd->cAuxBuffers;
			break;

		case GLX_STEREO:
			answer = (int)False;
			break;

		case GLX_ACCUM_RED_SIZE:
			answer = ppfd->cAccumRedBits;
			break;

		case GLX_ACCUM_GREEN_SIZE:
			answer = ppfd->cAccumGreenBits;
			break;

		case GLX_ACCUM_BLUE_SIZE:
			answer = ppfd->cAccumBlueBits;
			break;

		case GLX_ACCUM_ALPHA_SIZE:
			answer = ppfd->cAccumAlphaBits;
			break;

		case GLX_RENDER_TYPE:
			answer = (ppfd->iPixelType == PFD_TYPE_RGBA) ? GLX_RGBA_BIT : GLX_COLOR_INDEX_BIT;
			break;

		case GLX_RGBA:
		case GLX_RGBA_TYPE:
			answer = (ppfd->iPixelType == PFD_TYPE_RGBA) ? (int)True : (int)False;
			break;

		case GLX_COLOR_INDEX_TYPE:
			answer = (ppfd->iPixelType == PFD_TYPE_COLORINDEX) ? (int)True : (int)False;
			break;

		case GLX_USE_GL:
			answer = ((ppfd->dwFlags & PFD_SUPPORT_OPENGL) != 0) ? (int)True : (int)False;
			break;

		case GLX_DOUBLEBUFFER:
			answer = ((ppfd->dwFlags & PFD_DOUBLEBUFFER) != 0) ? (int)True : (int)False;
			break;

		case GLX_DRAWABLE_TYPE:
			answer =  GLX_WINDOW_BIT | GLX_PIXMAP_BIT | GLX_PBUFFER_BIT;
			break;

#ifdef GLX_CONFIG_CAVEAT
		case GLX_CONFIG_CAVEAT:
			answer = GLX_NONE;
			break;

		case GLX_SLOW_CONFIG:
		case GLX_NON_CONFORMANT_CONFIG:
			answer = (int)False;
			break;
#endif /*GLX_CONFIG_CAVEAT not support*/

		case GLX_X_RENDERABLE:
			answer = 1;
			break;

		case GLX_BUFFER_SIZE:
			answer = ppfd->cColorBits;
			break;

		case GLX_RED_SIZE:
			answer = ppfd->cRedBits;
			break;

		case GLX_GREEN_SIZE:
			answer = ppfd->cGreenBits;
			break;

		case GLX_BLUE_SIZE:
			answer = ppfd->cBlueBits;
			break;

		case GLX_ALPHA_SIZE:
			answer = ppfd->cAlphaBits;
			break;

		case GLX_DEPTH_SIZE:
			answer = ppfd->cDepthBits;
			break;

		case GLX_STENCIL_SIZE:
			answer = ppfd->cStencilBits;
			break;

		case GLX_LEVEL:
			answer = 0;
			break;

		case GLX_MAX_PBUFFER_WIDTH:
			answer = 480;
			break;

		case GLX_MAX_PBUFFER_HEIGHT:
			answer = 800;
			break;

		case GLX_MAX_PBUFFER_PIXELS:
			answer = 384000;
			break;

		case GLX_VISUAL_ID:
			answer = 0;
			break;

#ifdef GLX_X_VISUAL_TYPE
		case GLX_X_VISUAL_TYPE:
			answer = GLX_NONE;
			break;

		case GLX_TRUE_COLOR:
		case GLX_DIRECT_COLOR:
		case GLX_PSEUDO_COLOR:
		case GLX_STATIC_COLOR:
		case GLX_GRAY_SCALE:
		case GLX_STATIC_GRAY:
			answer = False;
			break;
#endif /*GLX_X_VISUAL_TYPE not support*/

		case GLX_SAMPLE_BUFFERS:
		case GLX_SAMPLES:
			answer = 0;
			break;

#ifdef GLX_TRANSPARENT_TYPE
		case GLX_TRANSPARENT_TYPE:
			answer = GLX_NONE;
			break;

		case GLX_TRANSPARENT_INDEX_VALUE:
		case GLX_TRANSPARENT_RED_VALUE:
		case GLX_TRANSPARENT_GREEN_VALUE:
		case GLX_TRANSPARENT_ALPHA_VALUE:
		case GLX_TRANSPARENT_BLUE_VALUE:
			answer = GLX_DONT_CARE;
			break;

		case GLX_TRANSPARENT_RGB:
		case GLX_TRANSPARENT_INDEX:
			answer = (int)False;
			break;
#endif  /*GLX_X_VISUAL_TYPE not support*/

		case GLX_FLOAT_COMPONENTS_NV:
		case GLX_PRESERVED_CONTENTS:
			answer = (int)False;
			break;

		default:
			answer = GLX_BAD_ATTRIBUTE;

	}

	*value = answer;

	return 0;
}

void* get_association_fakecontext_glxcontext(ProcessStruct* process, void* fakecontext)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_glxcontext[i].key != NULL;i++)
	{
		if (process->association_fakecontext_glxcontext[i].key == fakecontext)
			return process->association_fakecontext_glxcontext[i].value;
	}
	return NULL;
}

void set_association_fakecontext_glxcontext(ProcessStruct* process, void* fakecontext, void* glxcontext)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_glxcontext[i].key != NULL;i++)
	{
		if (process->association_fakecontext_glxcontext[i].key == fakecontext)
		{
			break;
		}
	}
	if (i < MAX_ASSOC_SIZE)
	{
		process->association_fakecontext_glxcontext[i].key = fakecontext;
		process->association_fakecontext_glxcontext[i].value = glxcontext;
	}
	else
	{
		fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
	}
}

void get_association_fakecontext_glxwnd(ProcessStruct* process, void* fakecontext, HWND rWnd, HDC rDC)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_glxcontext[i].key != NULL;i++)
	{
		if (process->association_fakecontext_glxcontext[i].key == fakecontext)
		{
			rWnd = (HWND)process->association_fakecontext_glxcontext[i].hWnd;
			rDC = (HDC)process->association_fakecontext_glxcontext[i].cDC;
			return;
		}
	}

	rWnd = NULL;
	rDC = NULL;
}

void set_association_fakecontext_glxwnd(ProcessStruct* process, void* fakecontext, void* glxwnd, void *glxhdc)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_glxcontext[i].key != NULL;i++)
	{
		if (process->association_fakecontext_glxcontext[i].key == fakecontext)
		{
			break;
		}
	}
	if (i < MAX_ASSOC_SIZE)
	{
		process->association_fakecontext_glxcontext[i].key = fakecontext;
		process->association_fakecontext_glxcontext[i].hWnd = glxwnd;
		process->association_fakecontext_glxcontext[i].cDC = glxhdc;
	}
	else
	{
		fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
	}
}

void unset_association_fakecontext_glxcontext(ProcessStruct* process, void* fakecontext)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_glxcontext[i].key != NULL;i++)
	{
		if (process->association_fakecontext_glxcontext[i].key == fakecontext)
		{
			memmove(&process->association_fakecontext_glxcontext[i],
					&process->association_fakecontext_glxcontext[i+1],
					sizeof(Assoc) * (MAX_ASSOC_SIZE - 1 - i));
			return;
		}
	}
}

unsigned int get_association_fakecontext_visualid(ProcessStruct* process, void* fakecontext)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_visual[i].key != NULL;i++)
	{
		if (process->association_fakecontext_visual[i].key == fakecontext)
			return (unsigned int)process->association_fakecontext_visual[i].value;
	}
	return 0;
}

void set_association_fakecontext_visualid(ProcessStruct* process, void* fakecontext, unsigned int visualid)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_visual[i].key != NULL;i++)
	{
		if (process->association_fakecontext_visual[i].key == fakecontext)
		{
			break;
		}
	}
	if (i < MAX_ASSOC_SIZE)
	{
		process->association_fakecontext_visual[i].key = fakecontext;
		process->association_fakecontext_visual[i].value = (void *)(long)visualid;
	}
	else
	{
		fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
	}
}

void unset_association_fakecontext_visualid(ProcessStruct* process, void* fakecontext)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_visual[i].key != NULL;i++)
	{
		if (process->association_fakecontext_visual[i].key == fakecontext)
		{
			memmove(&process->association_fakecontext_visual[i],
					&process->association_fakecontext_visual[i+1],
					sizeof(Assoc) * (MAX_ASSOC_SIZE - 1 - i));
			return;
		}
	}
}

HWND get_association_clientdrawable_serverwnd(ProcessStruct* process, HDC clientdrawable)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_clientdrawable_serverdrawable[i].key != NULL; i++)
	{
		if (process->association_clientdrawable_serverdrawable[i].key == (void*)clientdrawable)
			return (HWND)process->association_clientdrawable_serverdrawable[i].hWnd;
	}
	return (HWND)0;
}

HDC get_association_clientdrawable_serverdrawable(ProcessStruct* process, HDC clientdrawable)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_clientdrawable_serverdrawable[i].key != NULL;i++)
	{
		if (process->association_clientdrawable_serverdrawable[i].key == (void*)clientdrawable)
			return (HDC)process->association_clientdrawable_serverdrawable[i].value;
	}
	return (HDC)0;
}

void set_association_clientdrawable_serverwnd(ProcessStruct* process, void* clientdrawable, void* serverwnd)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_clientdrawable_serverdrawable[i].key != NULL;i++)
	{
		if (process->association_clientdrawable_serverdrawable[i].key == clientdrawable)
		{
			break;
		}
	}
	if (i < MAX_ASSOC_SIZE)
	{
		process->association_clientdrawable_serverdrawable[i].key = clientdrawable;
		process->association_clientdrawable_serverdrawable[i].hWnd = serverwnd;
	}
	else
	{
		fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
	}
}

void set_association_clientdrawable_serverdrawable(ProcessStruct* process, void* clientdrawable, void* serverdrawable)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_clientdrawable_serverdrawable[i].key != NULL;i++)
	{
		if (process->association_clientdrawable_serverdrawable[i].key == clientdrawable)
		{
			break;
		}
	}
	if (i < MAX_ASSOC_SIZE)
	{
		process->association_clientdrawable_serverdrawable[i].key = clientdrawable;
		process->association_clientdrawable_serverdrawable[i].value = serverdrawable;
	}
	else
	{
		fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
	}
}

void unset_association_clientdrawable_serverdrawable(ProcessStruct* process, void* clientdrawable)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_clientdrawable_serverdrawable[i].key != NULL;i++)
	{
		if (process->association_clientdrawable_serverdrawable[i].key == clientdrawable)
		{
			memmove(&process->association_clientdrawable_serverdrawable[i],
					&process->association_clientdrawable_serverdrawable[i+1],
					sizeof(Assoc) * (MAX_ASSOC_SIZE - 1 - i));
			return;
		}
	}
}

HWND get_association_fakepbuffer_pbufferwnd(ProcessStruct* process, void* fakepbuffer)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakepbuffer_pbuffer[i].key != NULL; i++)
	{
		if (process->association_fakepbuffer_pbuffer[i].key == fakepbuffer)
			return (HWND)process->association_fakepbuffer_pbuffer[i].hWnd;
	}
	return (HWND)0;
}

void set_association_fakepbuffer_pbufferwnd(ProcessStruct* process, void* fakepbuffer, void* pbufferwnd)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakepbuffer_pbuffer[i].key != NULL;i++)
	{
		if (process->association_fakepbuffer_pbuffer[i].key == fakepbuffer)
		{
			break;
		}
	}
	if (i < MAX_ASSOC_SIZE)
	{
		process->association_fakepbuffer_pbuffer[i].key = fakepbuffer;
		process->association_fakepbuffer_pbuffer[i].hWnd = pbufferwnd;
	}
	else
	{
		fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
	}
}

PbufferInfo *get_association_fakepbuffer_pbinfo(ProcessStruct* process, void* fakepbuffer)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakepbuffer_pbuffer[i].key != NULL; i++)
	{
		if (process->association_fakepbuffer_pbuffer[i].key == fakepbuffer)
			return (PbufferInfo *)process->association_fakepbuffer_pbuffer[i].cDC;
	}
	return (PbufferInfo *)0;
}

void set_association_fakepbuffer_pbinfo(ProcessStruct* process, void* fakepbuffer, void* pbuffer_info)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakepbuffer_pbuffer[i].key != NULL;i++)
	{
		if (process->association_fakepbuffer_pbuffer[i].key == fakepbuffer)
		{
			break;
		}
	}
	if (i < MAX_ASSOC_SIZE)
	{
		process->association_fakepbuffer_pbuffer[i].key = fakepbuffer;
		process->association_fakepbuffer_pbuffer[i].cDC= pbuffer_info;
	}
	else
	{
		fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
	}
}

HDC get_association_fakepbuffer_pbuffer(ProcessStruct* process, void* fakepbuffer)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakepbuffer_pbuffer[i].key != NULL;i++)
	{
		if (process->association_fakepbuffer_pbuffer[i].key == fakepbuffer)
			return (HDC)process->association_fakepbuffer_pbuffer[i].value;
	}
	return (HDC)NULL;
}

void set_association_fakepbuffer_pbuffer(ProcessStruct* process, void* fakepbuffer, void* pbuffer)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakepbuffer_pbuffer[i].key != NULL;i++)
	{
		if (process->association_fakepbuffer_pbuffer[i].key == fakepbuffer)
		{
			break;
		}
	}
	if (i < MAX_ASSOC_SIZE)
	{
		process->association_fakepbuffer_pbuffer[i].key = fakepbuffer;
		process->association_fakepbuffer_pbuffer[i].value = pbuffer;
	}
	else
	{
		fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
	}
}

void unset_association_fakepbuffer_pbuffer(ProcessStruct* process, void* fakepbuffer)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakepbuffer_pbuffer[i].key != NULL;i++)
	{
		if (process->association_fakepbuffer_pbuffer[i].key == fakepbuffer)
		{
			memmove(&process->association_fakepbuffer_pbuffer[i],
					&process->association_fakepbuffer_pbuffer[i+1],
					sizeof(Assoc) * (MAX_ASSOC_SIZE - 1 - i));
			return;
		}
	}
}

static int get_visual_info_from_visual_id( OGLS_Conn *pConn, int visualid, PIXELFORMATDESCRIPTOR* rpfd)
{
	int i;
	if( 0 < visualid )
	{
		AssocAttribListVisual *tabAssocAttribListVisual = (AssocAttribListVisual *)pConn->tabAssocAttribListVisual ;
		for(i=0;i<pConn->nTabAssocAttribListVisual;i++)
		{
			if ( tabAssocAttribListVisual[i].visualID == visualid)
			{
				DescribePixelFormat((HDC)pConn->Display, (visualid -1), sizeof( PIXELFORMATDESCRIPTOR), rpfd);
				return 1;
			}
		}
	}

	return 0;
}

static int get_default_visual(Display dpy, PIXELFORMATDESCRIPTOR* rpfd)
{
	HDC hdc;
	hdc = (HDC)dpy;
	int n;

	if ((n = DescribePixelFormat(hdc, 0, 0, NULL)) > 0)
	{
		int i;
		for (i = 0; i < n; i++)
		{
			DescribePixelFormat(hdc, i, sizeof( PIXELFORMATDESCRIPTOR), rpfd);
			if (!(rpfd->dwFlags & PFD_SUPPORT_OPENGL)) continue;
			if (!(rpfd->dwFlags & PFD_DRAW_TO_WINDOW)) continue;
			if (!(rpfd->dwFlags & PFD_DOUBLEBUFFER)) continue;
			if (rpfd->iPixelType != PFD_TYPE_RGBA) continue;
			if (rpfd->cRedBits < 8) continue;
			if (rpfd->cGreenBits < 8) continue;
			if (rpfd->cBlueBits < 8) continue;
			if (rpfd->cAlphaBits < 8) continue;
			if (rpfd->cDepthBits < 16) continue;
			if (rpfd->cStencilBits < 8) continue;

			break;
		}

		if( i < n ) return i + 1;
	}

	return 0;
}

/* Surface?? 위?? Window ????*/
static HDC create_swindow(OGLS_Conn *pConn, HDC clientdrawable, ProcessStruct* process, int x, int y, int width, int height)
{
	RECT rect;
	HWND hWnd;
	HDC hDC;
	WNDCLASS    wc;
	LPSTR lpszClassWindowSurface ="WindowSurface";

	wc.style		= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	= (WNDPROC)DefWindowProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= 0;
	wc.hIcon		= LoadIcon(NULL,IDI_APPLICATION);
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground	= NULL;
	wc.lpszMenuName	= NULL;
	wc.lpszClassName	= lpszClassWindowSurface;

	RegisterClass(&wc);

	rect.left   = x;
	rect.top    = y;
	rect.right  = width;
	rect.bottom = height;
	AdjustWindowRect(&rect, (WS_OVERLAPPED |WS_POPUP |WS_SYSMENU), False);

	hWnd = CreateWindow(lpszClassWindowSurface, lpszClassWindowSurface,
			(WS_OVERLAPPED |WS_POPUP |WS_SYSMENU  ),
			rect.left, rect.top,  rect.right-rect.left, rect.bottom-rect.top,
			NULL, (HMENU)NULL, NULL, NULL);

	hDC = GetDC(hWnd);
	if(hDC)
	{
		pConn->active_win = hDC;
		ShowWindow(hWnd,SW_HIDE);
		set_association_clientdrawable_serverwnd(process, (void *) clientdrawable, (void *) hWnd);
	}

	return hDC;
}

/* pbuffer?? 위?? Window ????*/
static HDC create_pbwindow(void *fakepbuffer, ProcessStruct* process, int x, int y, int width, int height)
{
	RECT rect;
	HWND hWnd;
	HDC hDC;
	WNDCLASS    wc;
	LPSTR lpszClassWindowSurface ="PBuffer";

	wc.style		= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	= (WNDPROC)DefWindowProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= 0;
	wc.hIcon		= LoadIcon(NULL,IDI_APPLICATION);
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground	= NULL;
	wc.lpszMenuName	= NULL;
	wc.lpszClassName	= lpszClassWindowSurface;

	RegisterClass(&wc);

	rect.left   = x;
	rect.top    = y;
	rect.right  = width;
	rect.bottom = height;
	AdjustWindowRect(&rect, (WS_OVERLAPPED |WS_POPUP |WS_SYSMENU), False);

	hWnd = CreateWindow(lpszClassWindowSurface, lpszClassWindowSurface,
			(WS_OVERLAPPED |WS_POPUP |WS_SYSMENU  ),
			rect.left, rect.top,  rect.right-rect.left, rect.bottom-rect.top,
			NULL, (HMENU)NULL, NULL, NULL);

	hDC = GetDC(hWnd);
	if(hDC)
	{
		ShowWindow(hWnd,SW_HIDE);
		set_association_fakepbuffer_pbufferwnd(process, fakepbuffer, (void *) hWnd);
	}

	return hDC;
}

/* context?? À§?? Dummy Window ????*/
static HDC create_cwindow(int fake_ctx, ProcessStruct* process, int x, int y, int width, int height)
{
	RECT rect;
	HWND hWnd;
	HDC hDC;
	WNDCLASS    wc;
	LPSTR lpszClassWindowSurface ="Context";

	wc.style		= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	= (WNDPROC)DefWindowProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= 0;
	wc.hIcon		= LoadIcon(NULL,IDI_APPLICATION);
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground	= NULL;
	wc.lpszMenuName	= NULL;
	wc.lpszClassName	= lpszClassWindowSurface;

	RegisterClass(&wc);

	rect.left   = x;
	rect.top    = y;
	rect.right  = width;
	rect.bottom = height;
	AdjustWindowRect(&rect, (WS_OVERLAPPED |WS_POPUP |WS_SYSMENU), False);

	hWnd = CreateWindow(lpszClassWindowSurface, lpszClassWindowSurface,
			(WS_OVERLAPPED |WS_POPUP |WS_SYSMENU  ),
			rect.left, rect.top,  rect.right-rect.left, rect.bottom-rect.top,
			NULL, (HMENU)NULL, NULL, NULL);

	hDC = GetDC(hWnd);
	if(hDC)
	{
		ShowWindow(hWnd,SW_HIDE);
		set_association_fakecontext_glxwnd(process, (void *) (long)fake_ctx, (void *)hWnd, (void *)hDC);
	}

	return hDC;
}

static void destroy_glwindow(OGLS_Conn *pConn, HWND hWnd, HDC hDC )
{
	ReleaseDC( hWnd, hDC );
	DestroyWindow(hWnd);

	/*PostQuitMessage(0);*/

	if( pConn->active_win == hDC)
		pConn->active_win = 0;
}

static int get_server_texture(ProcessStruct* process, unsigned int client_texture)
{
	unsigned int server_texture = 0;
	if (client_texture < 32768)
	{
		server_texture = process->current_state->tabTextures[client_texture];
	}
	else
	{
		fprintf(stderr, "invalid texture name %d\n", client_texture);
	}
	return server_texture;
}

static int get_server_list(ProcessStruct* process, unsigned int client_list)
{
	unsigned int server_list = 0;
	if (client_list < 32768)
	{
		server_list = process->current_state->tabLists[client_list];
	}
	else
	{
		fprintf(stderr, "invalid list name %d\n", client_list);
	}
	return server_list;
}

static int get_server_buffer(ProcessStruct* process, unsigned int client_buffer)
{
	unsigned int server_buffer = 0;
	if (client_buffer < 32768)
	{
		server_buffer = process->current_state->tabBuffers[client_buffer];
	}
	else
	{
		fprintf(stderr, "invalid buffer name %d\n", client_buffer);
	}
	return server_buffer;
}


static void do_glClientActiveTextureARB(int texture)
{
	GET_EXT_PTR_NO_FAIL(void, glClientActiveTextureARB, (int));
	if (ptr_func_glClientActiveTextureARB)
	{
		ptr_func_glClientActiveTextureARB(texture);
	}
}


/*
 * Translate the nth element of list from type to GLuint.
 */
	static GLuint
translate_id(GLsizei n, GLenum type, const GLvoid * list)
{
	GLbyte *bptr;
	GLubyte *ubptr;
	GLshort *sptr;
	GLushort *usptr;
	GLint *iptr;
	GLuint *uiptr;
	GLfloat *fptr;

	switch (type) {
		case GL_BYTE:
			bptr = (GLbyte *) list;
			return (GLuint) *(bptr + n);
		case GL_UNSIGNED_BYTE:
			ubptr = (GLubyte *) list;
			return (GLuint) *(ubptr + n);
		case GL_SHORT:
			sptr = (GLshort *) list;
			return (GLuint) *(sptr + n);
		case GL_UNSIGNED_SHORT:
			usptr = (GLushort *) list;
			return (GLuint) *(usptr + n);
		case GL_INT:
			iptr = (GLint *) list;
			return (GLuint) *(iptr + n);
		case GL_UNSIGNED_INT:
			uiptr = (GLuint *) list;
			return (GLuint) *(uiptr + n);
		case GL_FLOAT:
			fptr = (GLfloat *) list;
			return (GLuint) *(fptr + n);
		case GL_2_BYTES:
			ubptr = ((GLubyte *) list) + 2 * n;
			return (GLuint) *ubptr * 256 + (GLuint) * (ubptr + 1);
		case GL_3_BYTES:
			ubptr = ((GLubyte *) list) + 3 * n;
			return (GLuint) * ubptr * 65536
				+ (GLuint) *(ubptr + 1) * 256 + (GLuint) * (ubptr + 2);
		case GL_4_BYTES:
			ubptr = ((GLubyte *) list) + 4 * n;
			return (GLuint) *ubptr * 16777216
				+ (GLuint) *(ubptr + 1) * 65536
				+ (GLuint) *(ubptr + 2) * 256 + (GLuint) * (ubptr + 3);
		default:
			return 0;
	}
}


static const char *opengl_strtok(const char *s, int *n, char **saveptr, char *prevbuf)
{
	char *start;
	char *ret;
	char *p;
	int retlen;
    static const char *delim = " \t\n\r/";

	if (prevbuf)
		free(prevbuf);

    if (s) {
        *saveptr = s;
    } else {
        if (!(*saveptr) || !(*n))
            return NULL;
        s = *saveptr;
    }

    for (; *n && strchr(delim, *s); s++, (*n)--) {
        if (*s == '/' && *n > 1) {
            if (s[1] == '/') {
                do {
                    s++, (*n)--;
                } while (*n > 1 && s[1] != '\n' && s[1] != '\r');
            } else if (s[1] == '*') {
                do {
                    s++, (*n)--;
                } while (*n > 2 && (s[1] != '*' || s[2] != '/'));
                s++, (*n)--;
            }
        }
    }

   	start = s;
    for (; *n && *s && !strchr(delim, *s); s++, (*n)--);
	if (*n > 0) 
		s++, (*n)--;

	*saveptr = s;

	retlen = s - start;
	ret = malloc(retlen + 1);
	p = ret;

	while (retlen > 0) {
        if (*start == '/' && retlen > 1) {
            if (start[1] == '/') {
                do {
                    start++, retlen--;
                } while (retlen > 1 && start[1] != '\n' && start[1] != '\r');
				start++, retlen--;
				continue;
            } else if (start[1] == '*') {
                do {
                    start++, retlen--;
                } while (retlen > 2 && (start[1] != '*' || start[2] != '/'));
                start += 3, retlen -= 3;
				continue;
            }
        }
		*(p++) = *(start++), retlen--;
	}
	
	*p = 0;
	return ret;
}

static char *do_eglShaderPatch(const char *source, int length, int *patched_len)
{
	char *saveptr = NULL;
	char *sp;
	char *p = NULL;

    if (!length) 
        length = strlen(source);
    
    *patched_len = 0;
    int patched_size = length;
    char *patched = malloc(patched_size + 1);

    if (!patched) 
        return NULL;

    p = opengl_strtok(source, &length, &saveptr, NULL);
    for (; p; p = opengl_strtok(0, &length, &saveptr, p)) {
        if (!strncmp(p, "lowp", 4) || !strncmp(p, "mediump", 7) || !strncmp(p, "highp", 5)) {
            continue;
        } else if (!strncmp(p, "precision", 9)) {
            while ((p = opengl_strtok(0, &length, &saveptr, p)) && !strchr(p, ';'));
        } else {
            if (!strncmp(p, "gl_MaxVertexUniformVectors", 26)) {
                p = "(gl_MaxVertexUniformComponents / 4)";
            } else if (!strncmp(p, "gl_MaxFragmentUniformVectors", 28)) {
                p = "(gl_MaxFragmentUniformComponents / 4)";
            } else if (!strncmp(p, "gl_MaxVaryingVectors", 20)) {
                p = "(gl_MaxVaryingFloats / 4)";
            }

            int new_len = strlen(p);
            if (*patched_len + new_len > patched_size) {
                patched_size *= 2;
                patched = realloc(patched, patched_size + 1);

                if (!patched) 
                    return NULL;
            }

            memcpy(patched + *patched_len, p, new_len);
            *patched_len += new_len;
        }     
    }

    patched[*patched_len] = 0;
    /* check that we don't leave dummy preprocessor lines */
    for (sp = patched; *sp;) {
        for (; *sp == ' ' || *sp == '\t'; sp++);
        if (!strncmp(sp, "#define", 7)) {
            for (p = sp + 7; *p == ' ' || *p == '\t'; p++);
            if (*p == '\n' || *p == '\r' || *p == '/') {
                memset(sp, 0x20, 7);
            }
        }
        for (; *sp && *sp != '\n' && *sp != '\r'; sp++);
        for (; *sp == '\n' || *sp == '\r'; sp++);
    }
    return patched;
}

static int 
shadersrc_gles_to_gl(GLsizei count, const char** string, char **s, const GLint* length, GLint *l)
{
	int i;

	for(i = 0; i < count; ++i) {
		GLint len;
		if(length) {
			len = length[i];
			if (len < 0) 
				len = string[i] ? strlen(string[i]) : 0;
		} else
			len = string[i] ? strlen(string[i]) : 0;

		if(string[i]) {
			s[i] = do_eglShaderPatch(string[i], len, &l[i]);
			if(!s[i]) {
				while(i)
					free(s[--i]);

				free(l);
				free(s);
				return -1;
			}
		} else {
			s[i] = NULL;
			l[i] = 0;
		}
	}
	
	return 0;
}


int do_function_call(OGLS_Conn *pConn, int func_number, int pid, long* args, char* ret_string)
{
	char ret_char = 0;
	int ret_int = 0;
	const char* ret_str = NULL;
	int iProcess;
	ProcessStruct* process = NULL;
	ProcessStruct *processTab = (ProcessStruct *) pConn->processTab;

	for(iProcess=0;iProcess<MAX_HANDLED_PROCESS;iProcess++)
	{
		ProcessStruct *processTab = (ProcessStruct *) pConn->processTab;
		if (processTab[iProcess].process_id == pid)
		{
			process = &processTab[iProcess];
			break;
		}
		else if (processTab[iProcess].process_id == 0)
		{
			process = &processTab[iProcess];
			memset(process, 0, sizeof(ProcessStruct));
			process->process_id = pid;
			process->internal_num = pConn->last_assigned_internal_num++;
			init_gl_state(&process->default_state);
			process->current_state = &process->default_state;
			break;
		}
	}
	if (process == NULL)
	{
		fprintf(stderr, "Too many processes !\n");
		return 0;
	}

	if (process->internal_num != pConn->last_active_internal_num)
	{
		wglMakeCurrent(process->current_state->drawable, process->current_state->context);
		pConn->last_active_internal_num = process->internal_num;
	}

	process->instr_counter++;
	if (display_function_call) fprintf(stderr, "[%d]> %s\n", process->instr_counter, tab_opengl_calls_name[func_number]);

#if defined( _MK_DBG_ )
	//printf(" %s Function(%d) \n", tab_opengl_calls_name[func_number], func_number);
#endif

	switch (func_number)
	{
		case _init_func:
			{
				*(int*)args[1] = 1;
				break;
			}

		case _synchronize_func:
			{
				ret_int = 1;
				break;
			}

		case _exit_process_func:
			{
				int i;
				HWND hWnd;
				HDC hDC;
				HGLRC hRC;
				PbufferInfo *pb_info;

				for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_glxcontext[i].key != NULL;i++)
				{
					hRC = process->association_fakecontext_glxcontext[i].value;
					hWnd = (HWND) process->association_fakecontext_glxcontext[i].hWnd;
					hDC = (HDC) process->association_fakecontext_glxcontext[i].cDC;

					fprintf(stderr, "Destroy context corresponding to fake_context = %ld\n",
							(long)process->association_fakecontext_glxcontext[i].key);

					wglDeleteContext(hRC);
					ReleaseDC(hWnd, hDC);
					DestroyWindow(hWnd);
				}

				for(i=0;i < MAX_ASSOC_SIZE && process->association_fakepbuffer_pbuffer[i].key != NULL;i++)
				{
					hWnd = (HWND) process->association_fakepbuffer_pbuffer[i].hWnd;
					hDC = (HDC)process->association_fakepbuffer_pbuffer[i].value;
					pb_info = (PbufferInfo *) process->association_fakepbuffer_pbuffer[i].cDC;

					fprintf(stderr, "Destroy pbuffer corresponding to client_drawable = %p\n",
							process->association_fakepbuffer_pbuffer[i].key);

					free(pb_info->backBuffer);
					free(pb_info);

					ReleaseDC(hWnd, hDC);
					DestroyWindow(hWnd);
				}

				for(i=0;i < MAX_ASSOC_SIZE && process->association_clientdrawable_serverdrawable[i].key != NULL; i++)
				{
					hWnd = (HWND) process->association_clientdrawable_serverdrawable[i].hWnd;
					hDC = (HDC)process->association_clientdrawable_serverdrawable[i].value;

					fprintf(stderr, "Destroy window corresponding to client_drawable = %p\n",
							process->association_clientdrawable_serverdrawable[i].key);

					ReleaseDC(hWnd, hDC);
					DestroyWindow(hWnd);
				}

				for(i=0;i<process->nfbconfig;i++)
				{
					free(process->fbconfigs[i]);
				}

				for(i=0;i<process->nbGLStates;i++)
				{
					destroy_gl_state(process->glstates[i]);
					free(process->glstates[i]);
				}

				destroy_gl_state(&process->default_state);
				free(process->glstates);

				pConn->active_win = 0;

				memmove(&processTab[iProcess], &processTab[iProcess+1], (MAX_HANDLED_PROCESS - 1 - iProcess) * sizeof(ProcessStruct));

				last_process_id = 0;

				break;
			}

		case _changeWindowState_func:
			{
				HDC client_drawable = (HDC) args[0];
				if (display_function_call) fprintf(stderr, "client_drawable=%p\n", (void*)client_drawable);

				HWND hWnd = get_association_clientdrawable_serverwnd(process, client_drawable);
				if (hWnd && (args[1] == IsViewable))
				{
					ShowWindow(hWnd, SW_SHOW);
				}

				break;
			}

		case _moveResizeWindow_func:
			{
				int* params = (int*)args[1];
				HDC client_drawable = (HDC)args[0];
				if (display_function_call) fprintf(stderr, "client_drawable=%p\n", (void*)client_drawable);

				HWND hWnd = get_association_clientdrawable_serverwnd(process, client_drawable);
				if (hWnd)
				{
					WindowPosStruct pos;
					_get_window_pos(hWnd, &pos);
					if (!(params[0] == pos.x && params[1] == pos.y && params[2] == pos.width && params[3] == pos.height))
					{
						int redim = !(params[2] == pos.width && params[3] == pos.height);
						pConn->active_win_x = params[0];
						pConn->active_win_y = params[1];

						//fprintf(stderr, "old x=%d y=%d width=%d height=%d\n", pos.x, pos.y, pos.width, pos.height);
						//fprintf(stderr, "new x=%d y=%d width=%d height=%d\n", params[0], params[1], params[2], params[3]);

						if (redim)
						{
							MoveWindow(hWnd, params[0], params[1], params[2], params[3], False);
							_get_window_pos(hWnd, &pos);
							process->currentDrawablePos = pos;

							glViewport(0, 0, pos.width, pos.height);
						}
					}
				}
				break;
			}

		case _send_cursor_func:
			{
				fprintf(stderr, "_send_cursor_func not support \n");
				break;
			}

		case glXWaitGL_func:
			{
				ret_int = 0;
				break;
			}

		case glXWaitX_func:
			{
				ret_int = 0;
				break;
			}

		case glXGetFBConfigs_func:
			{
				ret_int = 0;

				fprintf(stderr, "glXGetFBConfigs not support \n");

				/*TODO*/
				/*TODO*/
				break;
			}


		case glXChooseFBConfig_func:
		case glXChooseFBConfigSGIX_func:
			{
				HDC         hdc;
				WGLFBConfig *fbConfigs, *c;
				int n;

				if (process->nfbconfig == MAX_FBCONFIG)
				{
					*(int*)args[3] = 0;
					ret_int = 0;
				}
				else
				{
					hdc = (HDC) pConn->Display;

					if ((n = DescribePixelFormat(hdc, 0, 0, NULL)) > 0)
					{
						int i, j;
						PIXELFORMATDESCRIPTOR pfd;
						for (j = i = 0; i < n; i++)
						{
							DescribePixelFormat(hdc, i, sizeof pfd, &pfd);
							if (!(pfd.dwFlags & PFD_SUPPORT_OPENGL)) continue;
							if (!(pfd.dwFlags & PFD_DRAW_TO_WINDOW)) continue;
							if (!(pfd.dwFlags & PFD_DOUBLEBUFFER)) continue;
							if (pfd.iPixelType != PFD_TYPE_RGBA) continue;
							j++;
						}

						c = fbConfigs = (WGLFBConfig *)malloc(sizeof(WGLFBConfig) * j);

						for (i = 0; i < n; i++) {
							DescribePixelFormat(hdc, i, sizeof pfd, &pfd);
							if (!(pfd.dwFlags & PFD_SUPPORT_OPENGL)) continue;
							if (!(pfd.dwFlags & PFD_DRAW_TO_WINDOW)) continue;
							if (!(pfd.dwFlags & PFD_DOUBLEBUFFER)) continue;
							if (pfd.iPixelType != PFD_TYPE_RGBA) continue;

							c->visualID = i + 1;
							memcpy(&c->pfd, &pfd, sizeof(PIXELFORMATDESCRIPTOR));
#if defined( _MK_DBG_ )   /* by    19.Nov.2009 */
							printf("Choose Config:0x%p VisualID : %d\n ",c, c->visualID);
#endif
							c++;
						}

						*(int*)args[3] = j;

						if (fbConfigs)
						{
							process->fbconfigs[process->nfbconfig] = fbConfigs;
							process->fbconfigs_max[process->nfbconfig] = *(int*)args[3];
							process->nfbconfig++;
							ret_int = 1 + process->nfbconfig_total;
							process->nfbconfig_total += process->fbconfigs_max[process->nfbconfig];

#if defined( _MK_DBG_ )
							printf(" DescribePixelFormat Num : %d \n", j);
#endif
						}
						else
						{
							ret_int = 0;
							*(int*)args[3] = 0;
						}
					}
					else
					{
						ret_int = 0;
						*(int*)args[3] = 0;
						printf(" DescribePixelFormat - NULL \n");
					}

				}

				break;
			}

		case glXGetFBConfigAttrib_func:
		case glXGetFBConfigAttribSGIX_func:
			{
				int client_pfd = args[1];
				WGLFBConfig *fbConfig = get_pfDescriptor(process, client_pfd);

				ret_int = 0;

				if (fbConfig)
					ret_int = get_pfdAttrib(&fbConfig->pfd, args[2], (int*)args[3]);

				break;
			}

		case glXGetFBConfigAttrib_extended_func:
			{
				int client_pfd = args[1];
				int n = args[2];
				int i;
				int* attribs = (int*)args[3];
				int* values = (int*)args[4];
				int* res = (int*)args[5];
				WGLFBConfig *fbConfig = get_pfDescriptor(process, client_pfd);

				for(i=0;i<n;i++)
				{
					if (fbConfig)
					{
						res[i] = get_pfdAttrib(&fbConfig->pfd, attribs[i], &values[i]);
					}
					else
					{
						res[i] = 0;
					}
				}
				break;
			}

		case glXGetVisualFromFBConfig_func:
			{
				int client_pfd = args[1];
				int i;
				unsigned int visualid;
				WGLFBConfig *fbConfig = get_pfDescriptor(process, client_pfd);

				ret_int = 0;

				if (fbConfig)
				{
					visualid = fbConfig->visualID;
					ret_int = visualid;

					AssocAttribListVisual *tabAssocAttribListVisual = (AssocAttribListVisual *)pConn->tabAssocAttribListVisual ;
					for( i = 0; i < pConn->nTabAssocAttribListVisual && (tabAssocAttribListVisual[i].visualID != visualid); i++ );

					if( i >= pConn->nTabAssocAttribListVisual )
					{
						pConn->tabAssocAttribListVisual = tabAssocAttribListVisual =
							realloc(tabAssocAttribListVisual, sizeof(AssocAttribListVisual) * (pConn->nTabAssocAttribListVisual+1));

						tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].attribListLength = 0;
						tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].attribList = NULL;
						tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].visualID = visualid;
						pConn->nTabAssocAttribListVisual++;
					}

					if (display_function_call) fprintf(stderr, "visualid = %d\n", ret_int);
				}

				break;
			}

		case glXCreateNewContext_func:
			{
				int client_pfd = args[1];
				WGLFBConfig *fbConfig = get_pfDescriptor(process, client_pfd);
				ret_int = 0;

#if defined( _MK_DBG_ )
				printf(" Config 0x%p - client_pfd %d \n", fbConfig, client_pfd);
#endif

				if (fbConfig)
				{
					int fake_shareList = args[3];
					HGLRC ctxt;
					HGLRC shareList = get_association_fakecontext_glxcontext(process, (void*)(long)fake_shareList);
					int fake_ctxt = process->next_available_context_number + 1;
					HDC hdc = create_cwindow(fake_ctxt, process, 0, 0, 10, 10);

					if(SetPixelFormat(hdc, fbConfig->visualID - 1, NULL))
					{
						ctxt = wglCreateContext (hdc);
#if defined( _MK_DBG_ )
						printf("HGLRC 0x%p = wglCreateNewContext(HDC 0x%p) visualID:%d fake_ctx:%d\n", ctxt, hdc, fbConfig->visualID, fake_ctxt);
#endif

						if(ctxt)
						{
							process->next_available_context_number ++;
							set_association_fakecontext_visualid(process, (void*)(long)fake_ctxt, fbConfig->visualID);
							set_association_fakecontext_glxcontext(process, (void*)(long)fake_ctxt, ctxt);
							_create_context(process, ctxt, fake_ctxt, shareList, fake_shareList);
							ret_int = fake_ctxt;
						}
					}

					if( 0 == ret_int )
					{
						int i;

						for(i=0; i < MAX_ASSOC_SIZE; i++)
						{
							if (process->association_fakecontext_glxcontext[i].key == (void *) (long)fake_ctxt)
							{
								process->association_fakecontext_glxcontext[i].key = NULL;
								process->association_fakecontext_glxcontext[i].value = NULL;
								process->association_fakecontext_glxcontext[i].hWnd = NULL;
								process->association_fakecontext_glxcontext[i].cDC= NULL;
								break;
							}
						}
						printf("wglCreateContext Fail - HDC: 0x%p, visual ID: %d,  Error : %d \n", hdc, fbConfig->visualID, (unsigned int)GetLastError());
					}
				}

				break;
			}

		case glXCreateContext_func:
			{
				int visualid = (int)args[1];
				int fake_shareList = (int)args[2];
				PIXELFORMATDESCRIPTOR pfd;
				HGLRC shareList = get_association_fakecontext_glxcontext(process, (void*)(long)fake_shareList);
				HGLRC ctxt = 0;
				int fake_ctxt = process->next_available_context_number + 1;
				HDC hdc = create_cwindow(fake_ctxt, process, 0, 0, 10, 10);

				ret_int = 0;

				if (1 || display_function_call) fprintf(stderr, "visualid=%d, fake_shareList=%d\n", visualid, fake_shareList);

				if(get_visual_info_from_visual_id(pConn, visualid, &pfd))
				{
					SetPixelFormat(hdc, visualid - 1, &pfd);
					ctxt = wglCreateContext (hdc);
				}
				else
				{
					if((visualid = get_default_visual(pConn->Display, &pfd)))
					{
						SetPixelFormat(hdc, visualid - 1, &pfd);
						ctxt = wglCreateContext (hdc);
					}
				}

				printf("HGLRC 0x%p = wglCreateContext(HDC 0x%p) visualID:%d \n", ctxt, hdc, visualid);

				if (ctxt)
				{
					process->next_available_context_number ++;
					set_association_fakecontext_glxcontext(process, (void*)(long)fake_ctxt, ctxt);
					set_association_fakecontext_visualid(process, (void*)(long)fake_ctxt, visualid);
					_create_context(process, ctxt, fake_ctxt, shareList, fake_shareList);
					ret_int = fake_ctxt;
				}

				if( 0 == ret_int )
				{
					int i;

					for(i=0; i < MAX_ASSOC_SIZE; i++)
					{
						if (process->association_fakecontext_glxcontext[i].key == (void *) (long)fake_ctxt)
						{
							process->association_fakecontext_glxcontext[i].key = NULL;
							process->association_fakecontext_glxcontext[i].value = NULL;
							process->association_fakecontext_glxcontext[i].hWnd = NULL;
							process->association_fakecontext_glxcontext[i].cDC= NULL;
							break;
						}
					}
					printf("wglCreateContext Fail - HDC: 0x%p, visual ID: %d,  Error : %d \n", hdc, visualid, (unsigned int)GetLastError());
				}

				break;
			}

		case glXMakeCurrent_func:
			{
				int i;
				HDC client_drawable = (HDC)args[1];
				HDC drawable = 0;
				PIXELFORMATDESCRIPTOR pfd;
				int fake_ctxt = args[2];
				unsigned int visualid = 0;

				if (display_function_call) fprintf(stderr, "client_drawable=%p fake_ctx=%d\n", (void*)client_drawable, fake_ctxt);

				if (client_drawable == 0 && fake_ctxt == 0)
				{
					ret_int = wglMakeCurrent(NULL, NULL);
					ret_int = 1;
					process->current_state = &process->default_state;
				}
				else if ((drawable = (HDC)get_association_fakepbuffer_pbuffer(process, (void*)client_drawable)))
				{
					HGLRC ctxt = (HGLRC)get_association_fakecontext_glxcontext(process, (void*)(long)fake_ctxt);
					PbufferInfo *pb_info = get_association_fakepbuffer_pbinfo(process, (void*)client_drawable);

					if (ctxt == NULL)
					{
						fprintf(stderr, "invalid fake_ctxt (%d) (*)!\n", fake_ctxt);
						ret_int = 0;
					}
					else
					{
						if( pb_info )
						{
							pb_info->context = ctxt;
						}

						visualid = get_association_fakecontext_visualid(process, (void*)(long)fake_ctxt);
						if(visualid)
						{
							/*DescribePixelFormat((HDC)dpy, (visualid -1), sizeof( PIXELFORMATDESCRIPTOR), &pfd);*/

							if(SetPixelFormat(drawable, visualid - 1, &pfd))
							{
								ret_int = wglMakeCurrent(drawable, ctxt);
#if defined( _MK_DBG_ )
								printf("%d = wglMakeCurrentPBuffer(HDC 0x%p, HGLRC 0x%p) - visualID:%d client_drawable:0x%p\n", ret_int, drawable, ctxt, visualid, client_drawable);
#endif
							}
							else
							{
								fprintf(stderr, "SetPixelFormat Error......\n");
								ret_int = 0;
							}

						}
						else
						{
							ret_int = 0;
						}
					}
				}
				else
				{
					HGLRC ctxt = (HGLRC)get_association_fakecontext_glxcontext(process, (void*)(long)fake_ctxt);
					if (ctxt == NULL)
					{
						fprintf(stderr, "invalid fake_ctxt (%d)!\n", fake_ctxt);
						ret_int = 0;
					}
					else
					{
						visualid = get_association_fakecontext_visualid(process, (void*)(long)fake_ctxt);

						if( visualid == 0 )
						{
							visualid = get_default_visual(pConn->Display, &pfd);
						}

						drawable = get_association_clientdrawable_serverdrawable(process, client_drawable);
						if (drawable == 0)
						{

							drawable = create_swindow(pConn, client_drawable, process, 0, 0, 480, 800);
							set_association_clientdrawable_serverdrawable(process, (void*)client_drawable, (void*)drawable);
						}

						/*DescribePixelFormat((HDC)dpy, (visualid -1), sizeof( PIXELFORMATDESCRIPTOR), &pfd);*/
						if(SetPixelFormat(drawable, visualid - 1, &pfd))
						{
							ret_int = wglMakeCurrent(drawable, ctxt);
#if defined( _MK_DBG_ )
							printf("%d = wglMakeCurrent(HDC 0x%p, HGLRC 0x%p) - visualID:%d client_drawable:0x%p\n", ret_int, drawable, ctxt, visualid, client_drawable);
#endif
						}
						else
						{
							fprintf(stderr, "SetPixelFormat Error......\n");
							ret_int = 0;
						}
					}
				}

				if (ret_int)
				{
					for(i=0;i<process->nbGLStates;i++)
					{
						if (process->glstates[i]->fake_ctxt == fake_ctxt)
						{
							process->current_state = process->glstates[i]; /* HACK !!! REMOVE */
							process->current_state->drawable = drawable;
							break;
						}
					}
				}
				break;
			}

		case glXGetConfig_func:
			{
				int visualid = args[1];
				PIXELFORMATDESCRIPTOR pfd;

				if (visualid)
				{
					if( get_visual_info_from_visual_id(pConn, visualid, &pfd))
					{
						ret_int = get_pfdAttrib(&pfd, args[2], (int*)args[3]);
					}
					else
					{
						if( get_default_visual(pConn->Display, &pfd))
							ret_int = get_pfdAttrib(&pfd, args[2], (int*)args[3]);
						else
							ret_int = GLX_BAD_VISUAL;
					}
				}
				else
				{
					if( get_default_visual(pConn->Display, &pfd))
						ret_int = get_pfdAttrib(&pfd, args[2], (int*)args[3]);
					else
						ret_int = GLX_BAD_VISUAL;
				}

				break;
			}

		case glXChooseVisual_func:
			{
				PIXELFORMATDESCRIPTOR pfd;
				unsigned int visualid = 0;
				AssocAttribListVisual *tabAssocAttribListVisual = (AssocAttribListVisual *)pConn->tabAssocAttribListVisual ;

				int i;

				if ((int*)args[2] == NULL)
					ret_int = 0;

				visualid = get_default_visual(pConn->Display, &pfd);

				for( i = 0; i < pConn->nTabAssocAttribListVisual && (tabAssocAttribListVisual[i].visualID != visualid); i++ );

				if( i >= pConn->nTabAssocAttribListVisual ) {
					pConn->tabAssocAttribListVisual = tabAssocAttribListVisual =
						realloc(tabAssocAttribListVisual, sizeof(AssocAttribListVisual) * (pConn->nTabAssocAttribListVisual+1));

					tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].attribListLength = 0;
					tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].attribList = NULL;
					tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].visualID = i;
					pConn->nTabAssocAttribListVisual++;
				}

				ret_int = visualid;
				break;
			}

		case glXDestroyWindow_func:
			{
				int i;
				HDC client_drawable = (HDC)args[1];
				HDC drawable = get_association_clientdrawable_serverdrawable(process, client_drawable);
				HWND hWnd = get_association_clientdrawable_serverwnd( process, client_drawable);

				if (display_function_call) fprintf(stderr, "client_drawable=%p\n", (void*)client_drawable);

				if( drawable && hWnd )
				{
					destroy_glwindow(pConn, hWnd, drawable);

#if defined( _MK_DBG_ )
					printf("DestoryWindw( HWND 0x%p, HDC 0x%p) Client Drawable 0x%p\n", hWnd, drawable, client_drawable);
#endif

					unset_association_clientdrawable_serverdrawable(process, (void *) (long)client_drawable);

					for(i=0;i<process->nbGLStates;i++)
					{
						if (process->glstates[i]->drawable == drawable)
						{
							process->glstates[i]->drawable = 0;
						}
					}

					if( process->current_state->drawable == drawable )
						process->current_state = &process->default_state;
				}

				break;
			}


		case glXDestroyContext_func:
			{
				int fake_ctxt = (int)args[1];
				HWND hWnd = NULL;
				HDC hdc = NULL;
				HGLRC ctxt = (HGLRC)get_association_fakecontext_glxcontext(process, (void*)(long)fake_ctxt);

				if (display_function_call) fprintf(stderr, "fake_ctxt=%d\n", fake_ctxt);

				if (ctxt == NULL)
				{
					fprintf(stderr, "invalid fake_ctxt (%p) !\n", (void*)(long)fake_ctxt);
				}
				else
				{
					int i;
					for(i=0;i<process->nbGLStates;i++)
					{
						if (process->glstates[i]->fake_ctxt == fake_ctxt)
						{
							if (ctxt == process->current_state->context)
								process->current_state = &process->default_state;

							int fake_shareList = process->glstates[i]->fake_shareList;
							process->glstates[i]->ref --;
							if (process->glstates[i]->ref == 0)
							{
								fprintf(stderr, "destroy_gl_state fake_ctxt = %d\n", process->glstates[i]->fake_ctxt);
								destroy_gl_state(process->glstates[i]);
								free(process->glstates[i]);
								memmove(&process->glstates[i], &process->glstates[i+1], (process->nbGLStates-i-1) * sizeof(GLState*));
								process->nbGLStates--;
							}

							if (fake_shareList)
							{
								for(i=0;i<process->nbGLStates;i++)
								{
									if (process->glstates[i]->fake_ctxt == fake_shareList)
									{
										process->glstates[i]->ref --;
										if (process->glstates[i]->ref == 0)
										{
											fprintf(stderr, "destroy_gl_state fake_ctxt = %d\n", process->glstates[i]->fake_ctxt);
											destroy_gl_state(process->glstates[i]);
											free(process->glstates[i]);
											memmove(&process->glstates[i], &process->glstates[i+1], (process->nbGLStates-i-1) * sizeof(GLState*));
											process->nbGLStates--;
										}
										break;
									}
								}
							}

							wglDeleteContext(ctxt);

							get_association_fakecontext_glxwnd(process, (void*)(long)fake_ctxt, hWnd, hdc);
							destroy_glwindow(pConn, hWnd, hdc);

							unset_association_fakecontext_glxcontext(process, (void*)(long)fake_ctxt);
							unset_association_fakecontext_visualid(process, (void*)(long)fake_ctxt);

#if defined( _MK_DBG_ )
							printf("Destory Context Window ( WHND 0x%p, HRC 0x%p)", hWnd, hdc);
							printf("wglDestroyContext(HGLRC 0x%p) - fake context : %d\n", ctxt, fake_ctxt);
#endif
							break;
						}
					}
				}
				break;
			}

		case glXGetProcAddress_fake_func:
			{
				char *fun_name = (char*)args[0];
				if (display_function_call) fprintf(stderr, "%s\n", fun_name);

				ret_int = 1;
				break;
			}

		case glXGetProcAddress_global_fake_func:
			{
				int nbElts = args[0];
				char* huge_buffer = (char*)args[1];
				char* result = (char*)args[2];
				int i;
				for(i=0;i<nbElts;i++)
				{
					int len = strlen(huge_buffer);

					result[i] = 1;
					huge_buffer += len + 1;
				}
				break;
			}

		case glXSwapBuffers_func:
			{
				HDC client_drawable = (HDC)args[1];
				if (display_function_call) fprintf(stderr, "client_drawable=%p\n", (void*)client_drawable);

				HDC drawable = get_association_clientdrawable_serverdrawable(process, client_drawable);
				if (drawable == 0)
				{
					fprintf(stderr, "unknown client_drawable (%p) !\n", (void*)client_drawable);
				}
				else
				{
#if defined( _MK_DBG_ )   /* by    20.Nov.2009 */
					printf("SwapBuffers( HDC 0x%p)\n", drawable);
#endif
					SwapBuffers(drawable);
				}
				break;
			}

		case glXQueryVersion_func:
			{
				int *pMajorVersion = (int *)args[1];
				int *pMinorVersion = (int *)args[2];
				int rVersion;

				rVersion = GetVersion();

				*pMajorVersion = (rVersion & 0x0000FF00) >> 8;
				*pMinorVersion = rVersion & 0x000000FF;


#if defined( _MK_DBG_ )   /* by    20.Nov.2009 */
				printf(" Major Version : %d - Minor Version : %d \n",*pMajorVersion, *pMinorVersion);
#endif
				break;
			}

		case glXQueryExtensionsString_func:
			{

				fprintf(stderr, "glXQueryExtensionsString not support (Current WIN32 System)\n");
				ret_str = NULL;
				break;
			}

		case glXGetClientString_func:
			{
				static char *ventor = "support WGL of Microsoft Corporation";
				ret_str = ventor;//glXGetClientString(dpy, args[1]);
				break;
			}

		case glXQueryServerString_func:
			{
				static char *ventor = "support WGL of Microsoft Corporation";
				ret_str = ventor; //glXQueryServerString(dpy, 0, args[2]);
				break;
			}

		case glXGetScreenDriver_func:
			{
				fprintf(stderr, "glXGetScreenDriver not support (Current WIN32 System)\n");
				ret_str = NULL;
				break;
			}

		case glXGetDriverConfig_func:
			{
				fprintf(stderr, "glXGetDriverConfig not support (Current WIN32 System)\n");
				ret_str = NULL;
				break;
			}

		case glXCreatePbuffer_func:
			{
				int client_fbconfig = args[1];
				int pb_width = 0, pb_height = 0;
				int *attrib_list = (int*)args[2];
				int fake_pbuffer = process->next_available_pbuffer_number + 1;
				HDC pbuffer;
				WGLFBConfig *fbconfig = get_pfDescriptor(process, client_fbconfig);

				ret_int = 0;

				while (attrib_list && *attrib_list != GLX_NONE) {
					switch (*attrib_list++) {
						case GLX_WIDTH:
							pb_width = *attrib_list++;
							break;

						case GLX_HEIGHT:
							pb_height = *attrib_list++;
							break;

						default:
							break;
					}
				}

				if(fbconfig)
				{
					pbuffer = create_pbwindow((void *) ( fake_pbuffer), process, 0, 0, pb_width, pb_height);

#if defined( _MK_DBG_ )
					printf(" pbuffer 0x%p = Create_pbwindow( fakebuffer : %d , width : %d, height : %d\n", pbuffer, fake_pbuffer, pb_width, pb_height);
#endif

					if (pbuffer)
					{
						PbufferInfo *pb_info = (PbufferInfo *)malloc( sizeof(PbufferInfo) );
						memset((void *) pb_info, 0, sizeof(PbufferInfo));

						process->next_available_pbuffer_number ++;
						set_association_fakepbuffer_pbuffer(process, (void*)(long)fake_pbuffer, (void*)pbuffer);
						ret_int = fake_pbuffer;

						pb_info->width = pb_width;
						pb_info->height = pb_height;
						pb_info->colorFormat = GLX_RGBA;
						get_pfdAttrib(&fbconfig->pfd, GLX_BUFFER_SIZE, &pb_info->colorBits);

						pb_info->backBuffer = malloc( pb_width * pb_height * sizeof(int));
						set_association_fakepbuffer_pbinfo( process, (void *)(long)fake_pbuffer, (void *) pb_info );
					}
				}

				break;
			}

		case glXCreateGLXPbufferSGIX_func:
			{
				int client_fbconfig = args[1];
				int pb_width = 0, pb_height = 0;
				int fake_pbuffer = process->next_available_pbuffer_number + 1;
				HDC pbuffer;
				WGLFBConfig *fbconfig = get_pfDescriptor(process, client_fbconfig);

				ret_int = 0;

				pb_width = args[2];
				pb_height = args[3];

				if(fbconfig)
				{
					pbuffer = create_pbwindow((void *) ( fake_pbuffer), process, 0, 0, pb_width, pb_height);
#if defined( _MK_DBG_ )
					printf(" pbuffer 0x%p = Create_pbwindow( fakebuffer : %d , width : %d, height : %d\n", pbuffer, fake_pbuffer, pb_width, pb_height);
#endif
					if (pbuffer)
					{
						PbufferInfo *pb_info = (PbufferInfo *)malloc( sizeof(PbufferInfo) );
						memset((void *) pb_info, 0, sizeof(PbufferInfo));

						process->next_available_pbuffer_number ++;
						set_association_fakepbuffer_pbuffer(process, (void*)(long)fake_pbuffer, (void*)pbuffer);
						ret_int = fake_pbuffer;

						pb_info->width = pb_width;
						pb_info->height = pb_height;
						pb_info->colorFormat = GLX_RGBA;
						get_pfdAttrib(&fbconfig->pfd, GLX_BUFFER_SIZE, &pb_info->colorBits);

						pb_info->backBuffer = malloc( pb_width * pb_height * sizeof(int));
						set_association_fakepbuffer_pbinfo( process, (void *)(long)fake_pbuffer, (void *) pb_info );
					}
				}

				break;
			}

		case glXDestroyPbuffer_func:
		case glXDestroyGLXPbufferSGIX_func:
			{
				HDC fakepbuffer = (HDC)args[1];
				HDC pbuffer = get_association_fakepbuffer_pbuffer(process, (void *)fakepbuffer);
				HWND hWnd = get_association_fakepbuffer_pbufferwnd( process, (void *)fakepbuffer);
				PbufferInfo *pb_info = get_association_fakepbuffer_pbinfo(process, (void *) fakepbuffer);

				if( pbuffer && hWnd )
				{
					if(pb_info)
					{
						free(pb_info->backBuffer);
						free(pb_info);
					}

					destroy_glwindow(pConn, hWnd, pbuffer);

#if defined( _MK_DBG_ )
					printf("DestoryPbuffer( HWND 0x%p, HDC 0x%p) fake pbuffer 0x%p\n", hWnd, pbuffer, fakepbuffer);
#endif

					unset_association_fakepbuffer_pbuffer(process, (void *) (long)fakepbuffer);
				}

				break;
			}

		case glXBindTexImageARB_func:
		case glXBindTexImageATI_func:
			{
				HGLRC pb_context, cur_context;
				HDC pb_drawable, cur_drawable;
				int fake_pbuffer = (int)args[1];
				PbufferInfo *pb_info = get_association_fakepbuffer_pbinfo(process, (void *) (long) fake_pbuffer);

				ret_int = 0;

				if( pb_info )
				{

					pb_context = pb_info->context;
					pb_drawable =get_association_fakepbuffer_pbuffer(process, (void *) (long) fake_pbuffer);

					if( pb_context && pb_drawable )
					{
						cur_context = wglGetCurrentContext( );
						cur_drawable = wglGetCurrentDC( );

						wglMakeCurrent(pb_drawable, pb_context);
						glReadPixels(0, 0, pb_info->width, pb_info->height, pb_info->colorFormat, GL_UNSIGNED_BYTE, pb_info->backBuffer);
						wglMakeCurrent(cur_drawable, cur_context);

						glTexImage2D(GL_TEXTURE_2D, 0, pb_info->colorFormat, pb_info->width, pb_info->height,
								0, pb_info->colorFormat, GL_UNSIGNED_BYTE, pb_info->backBuffer);
						ret_int = 1;
					}
					else
					{
						fprintf(stderr, "glXBindTexImage Error - pbuffer Null...\n");
					}

				}
				else
				{
					fprintf(stderr, "glXBindTexImage Error - pbuffer information Null...\n");
				}


				/*TODO*/
				/*TODO*/
				break;
			}

		case glXReleaseTexImageARB_func:
		case glXReleaseTexImageATI_func:
			{
				int fake_pbuffer = (int)args[1];
				HDC pbuffer = get_association_fakepbuffer_pbuffer(process, (void*)fake_pbuffer);
				PbufferInfo *pb_info = get_association_fakepbuffer_pbinfo(process, (void*)fake_pbuffer);

				ret_int = 0;

				if ( pbuffer )
				{
					if(pb_info)
					{
						glTexImage2D(GL_TEXTURE_2D, 0, pb_info->colorFormat, pb_info->width, pb_info->height, 0, pb_info->colorFormat, GL_UNSIGNED_BYTE, NULL);
						ret_int = 1;
					}
					else
					{
						fprintf(stderr, "glXReleaseTexImageARB : invalid fake_pbuffer (%d) !\n", (int)fake_pbuffer);
					}
				}
				else
				{
					fprintf(stderr, "glXReleaseTexImageARB : invalid fake_pbuffer (%d) !\n", (int)fake_pbuffer);
				}

				break;
			}

		case glXQueryContext_func:
			{
				ret_int = 0;
				fprintf(stderr, "glXCreateGLXPbufferSGIX not support (Current WIN32 System)\n");
				break;
			}

		case glXQueryDrawable_func:
			{
				fprintf(stderr, "glXQueryDrawable not support \n");

				/*TODO*/
				/*TODO*/
				break;
			}

		case glXQueryGLXPbufferSGIX_func:
			{
				ret_int = 0;

				fprintf(stderr, "glXCreateGLXPbufferSGIX not support (Current WIN32 System)\n");
				break;
			}

		case glXCreateContextWithConfigSGIX_func:
			{
				ret_int = 0;

				fprintf(stderr, "glXCreateContextWithConfigSGIX not support (Current WIN32 System)\n");
				break;
			}

		case glXSwapIntervalSGI_func:
			{
				ret_int = 0;

				fprintf(stderr, "glXSwapIntervalSGI not support (Current WIN32 System)\n");
				break;
			}

		case glXCopyContext_func:
			{
				HGLRC fake_src_ctxt = (HGLRC)args[1];
				HGLRC fake_dst_ctxt = (HGLRC)args[2];

				if (display_function_call) fprintf(stderr, "fake_src_ctxt=%p, fake_dst_ctxt=%p\n", fake_src_ctxt, fake_dst_ctxt);

				fprintf(stderr, "glXCopyContext not support (Current WIN32 System)\n");
				break;
			}

		case glXIsDirect_func:
			{
				ret_char = False;
				fprintf(stderr, "glXCopyContext not support (Current WIN32 System)\n");
				break;
			}

		case glGetString_func:
			{
				ret_str = (char*)glGetString(args[0]);
				break;
			}

			/* Begin of texture stuff */
		case glBindTexture_func:
		case glBindTextureEXT_func:
			{
				int target = args[0];
				unsigned int client_texture = args[1];
				unsigned int server_texture;

				if (client_texture == 0)
				{
					glBindTexture(target, 0);
				}
				else
				{
					alloc_value(process->current_state->textureAllocator, client_texture);
					server_texture = process->current_state->tabTextures[client_texture];
					if (server_texture == 0)
					{
						glGenTextures(1, &server_texture);
						process->current_state->tabTextures[client_texture] = server_texture;
					}
					glBindTexture(target, server_texture);
				}
				break;
			}

		case glGenTextures_fake_func:
			{
				//GET_EXT_PTR(void, glGenTextures, (GLsizei n, GLuint *textures));
				int i;
				int n = args[0];
				unsigned int* clientTabTextures = malloc(n * sizeof(int));
				unsigned int* serverTabTextures = malloc(n * sizeof(int));

				alloc_range(process->current_state->textureAllocator, n, clientTabTextures);

				glGenTextures(n, serverTabTextures);
				for(i=0;i<n;i++)
				{
					process->current_state->tabTextures[clientTabTextures[i]] = serverTabTextures[i];
				}

				free(clientTabTextures);
				free(serverTabTextures);
				break;
			}

		case glDeleteTextures_func:
			{
				//GET_EXT_PTR(void, glDeleteTextures, (GLsizei n, const GLuint *textures));
				int i;
				int n = args[0];
				unsigned int* clientTabTextures = (unsigned int*)args[1];

				delete_range(process->current_state->textureAllocator, n, clientTabTextures);

				unsigned int* serverTabTextures = malloc(n * sizeof(int));
				for(i=0;i<n;i++)
				{
					serverTabTextures[i] = get_server_texture(process, clientTabTextures[i]);
				}
				glDeleteTextures(n, serverTabTextures);
				for(i=0;i<n;i++)
				{
					process->current_state->tabTextures[clientTabTextures[i]] = 0;
				}
				free(serverTabTextures);
				break;
			}

		case glPrioritizeTextures_func:
			{
				GET_EXT_PTR(void, glPrioritizeTextures, (GLsizei n, const GLuint *textures, const GLclampf *priorities));

				int i;
				int n = args[0];
				unsigned int* textures = (unsigned int*)args[1];
				for(i=0;i<n;i++)
				{
					textures[i] = get_server_texture(process, textures[i]);
				}
				ptr_func_glPrioritizeTextures(n, textures, (const GLclampf*)args[2]);
				break;
			}

		case glAreTexturesResident_func:
			{
				GET_EXT_PTR(void, glAreTexturesResident, (GLsizei n, const GLuint *textures, GLboolean *residences));
				int i;
				int n = args[0];
				unsigned int* textures = (unsigned int*)args[1];
				for(i=0;i<n;i++)
				{
					textures[i] = get_server_texture(process, textures[i]);
				}
				ptr_func_glAreTexturesResident(n, textures, (GLboolean*)args[2]);
				break;
			}

		case glIsTexture_func:
		case glIsTextureEXT_func:
			{
				GET_EXT_PTR(GLboolean, glIsTexture, (GLuint texture ));
				unsigned int client_texture = args[0];
				unsigned int server_texture = get_server_texture(process, client_texture);
				if (server_texture)
					ret_char = ptr_func_glIsTexture(server_texture);
				else
					ret_char = 0;
				break;
			}

		case glFramebufferTexture1DEXT_func:
			{
				GET_EXT_PTR(void, glFramebufferTexture1DEXT, (int, int, int, int, int));
				unsigned int client_texture = args[3];
				unsigned int server_texture = get_server_texture(process, client_texture);
				if (server_texture)
					ptr_func_glFramebufferTexture1DEXT(args[0], args[1], args[2], server_texture, args[4]);
				break;
			}

		case glFramebufferTexture2DEXT_func:
			{
				GET_EXT_PTR(void, glFramebufferTexture2DEXT, (int, int, int, int, int));
				unsigned int client_texture = args[3];
				unsigned int server_texture = get_server_texture(process, client_texture);
				if (server_texture)
					ptr_func_glFramebufferTexture2DEXT(args[0], args[1], args[2], server_texture, args[4]);
				break;
			}

		case glFramebufferTexture3DEXT_func:
			{
				GET_EXT_PTR(void, glFramebufferTexture3DEXT, (int, int, int, int, int, int));
				unsigned int client_texture = args[3];
				unsigned int server_texture = get_server_texture(process, client_texture);
				if (server_texture)
					ptr_func_glFramebufferTexture3DEXT(args[0], args[1], args[2], server_texture, args[4], args[5]);
				break;
			}
			/* End of texture stuff */

			/* Begin of list stuff */
		case glIsList_func:
			{
				unsigned int client_list = args[0];
				unsigned int server_list = get_server_list(process, client_list);
				if (server_list)
					ret_char = glIsList(server_list);
				else
					ret_char = 0;
				break;
			}

		case glDeleteLists_func:
			{
				int i;
				unsigned int first_client = args[0];
				int n = args[1];

				unsigned int first_server = get_server_list(process, first_client);
				for(i=0;i<n;i++)
				{
					if (get_server_list(process, first_client + i) != first_server + i)
						break;
				}
				if (i == n)
				{
					glDeleteLists(first_server, n);
				}
				else
				{
					for(i=0;i<n;i++)
					{
						glDeleteLists(get_server_list(process, first_client + i), 1);
					}
				}

				for(i=0;i<n;i++)
				{
					process->current_state->tabLists[first_client + i] = 0;
				}
				delete_consecutive_values(process->current_state->listAllocator, first_client, n);
				break;
			}

		case glGenLists_fake_func:
			{
				int i;
				int n = args[0];
				unsigned int server_first = glGenLists(n);
				if (server_first)
				{
					unsigned int client_first = alloc_range(process->current_state->listAllocator, n, NULL);
					for(i=0;i<n;i++)
					{
						process->current_state->tabLists[client_first + i] = server_first + i;
					}
				}
				break;
			}

		case glNewList_func:
			{
				unsigned int client_list = args[0];
				int mode = args[1];
				alloc_value(process->current_state->listAllocator, client_list);
				unsigned int server_list = get_server_list(process, client_list);
				if (server_list == 0)
				{
					server_list = glGenLists(1);
					process->current_state->tabLists[client_list] = server_list;
				}
				glNewList(server_list, mode);
				break;
			}

		case glCallList_func:
			{
				unsigned int client_list = args[0];
				unsigned int server_list = get_server_list(process, client_list);
				glCallList(server_list);
				break;
			}

		case glCallLists_func:
			{
				int i;
				int n = args[0];
				int type = args[1];
				const GLvoid* lists = (const GLvoid*)args[2];
				int* new_lists = malloc(sizeof(int) * n);
				for(i=0;i<n;i++)
				{
					new_lists[i] = get_server_list(process, translate_id(i, type, lists));
				}
				glCallLists(n, GL_UNSIGNED_INT, new_lists);
				free(new_lists);
				break;
			}
			/* End of list stuff */

			/* Begin of buffer stuff */
		case glBindBufferARB_func:
			{
				GET_EXT_PTR(void, glBindBufferARB, (int,int));
				int target = args[0];
				unsigned int client_buffer = args[1];
				unsigned int server_buffer;

				if (client_buffer == 0)
				{
					ptr_func_glBindBufferARB(target, 0);
				}
				else
				{
					server_buffer = get_server_buffer(process, client_buffer);
					ptr_func_glBindBufferARB(target, server_buffer);
				}
				break;
			}

		case glGenBuffersARB_fake_func:
			{
				GET_EXT_PTR(void, glGenBuffersARB, (int,unsigned int*));
				int i;
				int n = args[0];
				unsigned int* clientTabBuffers = malloc(n * sizeof(int));
				unsigned int* serverTabBuffers = malloc(n * sizeof(int));

				alloc_range(process->current_state->bufferAllocator, n, clientTabBuffers);

				ptr_func_glGenBuffersARB(n, serverTabBuffers);
				for(i=0;i<n;i++)
				{
					process->current_state->tabBuffers[clientTabBuffers[i]] = serverTabBuffers[i];
				}

				free(clientTabBuffers);
				free(serverTabBuffers);
				break;
			}


		case glDeleteBuffersARB_func:
			{
				GET_EXT_PTR(void, glDeleteBuffersARB, (int,int*));
				int i;
				int n = args[0];
				unsigned int* clientTabBuffers = (unsigned int*)args[1];

				delete_range(process->current_state->bufferAllocator, n, clientTabBuffers);

				int* serverTabBuffers = malloc(n * sizeof(int));
				for(i=0;i<n;i++)
				{
					serverTabBuffers[i] = get_server_buffer(process, clientTabBuffers[i]);
				}
				ptr_func_glDeleteBuffersARB(n, serverTabBuffers);
				for(i=0;i<n;i++)
				{
					process->current_state->tabBuffers[clientTabBuffers[i]] = 0;
				}
				free(serverTabBuffers);
				break;
			}

		case glIsBufferARB_func:
			{
				GET_EXT_PTR(int, glIsBufferARB, (int));
				unsigned int client_buffer = args[0];
				unsigned int server_buffer = get_server_buffer(process, client_buffer);
				if (server_buffer)
					ret_int = ptr_func_glIsBufferARB(server_buffer);
				else
					ret_int = 0;
				break;
			}
			/* Endo of buffer stuff */

		case glShaderSourceARB_fake_func:
			{
				GET_EXT_PTR(void, glShaderSourceARB, (int,int,char**,void*));
				int size = args[1];
				int i;
				int acc_length = 0;
				GLcharARB** tab_prog = malloc(size * sizeof(GLcharARB*));
				int* tab_length = (int*)args[3];
				for(i=0;i<size;i++)
				{
					tab_prog[i] = ((GLcharARB*)args[2]) + acc_length;
					acc_length += tab_length[i];
				}
				ptr_func_glShaderSourceARB(args[0], args[1], tab_prog, tab_length);
				free(tab_prog);
				break;
			}

		case glShaderSource_fake_func:
			{
				GET_EXT_PTR(void, glShaderSource, (int,int,char**,void*));
				int size = args[1];
				int i;
				int acc_length = 0;
				GLcharARB** tab_prog = malloc(size * sizeof(GLcharARB*));
				int* tab_length = (int*)args[3];

				char **tab_prog_new;
				GLint *tab_length_new;

			   tab_prog_new = malloc(args[1]* sizeof(char*));
			   tab_length_new = malloc(args[1]* sizeof(GLint));

			   memset(tab_prog_new, 0, args[1] * sizeof(char*));
			   memset(tab_length_new, 0, args[1] * sizeof(GLint));


				for(i=0;i<size;i++)
				{
					tab_prog[i] = ((GLcharARB*)args[2]) + acc_length;
					acc_length += tab_length[i];
				}
				
				shadersrc_gles_to_gl(args[1], tab_prog, tab_prog_new, tab_length, tab_length_new);
				
				if (!tab_prog_new || !tab_length_new)
					break;

				ptr_func_glShaderSource(args[0], args[1], tab_prog_new, tab_length_new);

				for (i = 0; i < args[1]; i++)
					free(tab_prog_new[i]);
				free(tab_prog_new);
				free(tab_length_new);

				free(tab_prog);
				break;
			}

		case glVertexPointer_fake_func:
			{
				int offset = args[0];
				int size = args[1];
				int type = args[2];
				int stride = args[3];
				int bytes_size = args[4];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, (void*)args[5], bytes_size);
				/*fprintf(stderr, "glVertexPointer_fake_func size=%d, type=%d, stride=%d, byte_size=%d\n",
				  size, type, stride, bytes_size);*/
				glVertexPointer(size, type, stride, process->current_state->vertexPointer);
				break;
			}

		case glNormalPointer_fake_func:
			{
				int offset = args[0];
				int type = args[1];
				int stride = args[2];
				int bytes_size = args[3];
				process->current_state->normalPointerSize = MAX(process->current_state->normalPointerSize, offset + bytes_size);
				process->current_state->normalPointer = realloc(process->current_state->normalPointer, process->current_state->normalPointerSize);
				memcpy(process->current_state->normalPointer + offset, (void*)args[4], bytes_size);
				//fprintf(stderr, "glNormalPointer_fake_func type=%d, stride=%d, byte_size=%d\n", type, stride, bytes_size);
				glNormalPointer(type, stride, process->current_state->normalPointer);
				break;
			}

		case glIndexPointer_fake_func:
			{
				int offset = args[0];
				int type = args[1];
				int stride = args[2];
				int bytes_size = args[3];
				process->current_state->indexPointerSize = MAX(process->current_state->indexPointerSize, offset + bytes_size);
				process->current_state->indexPointer = realloc(process->current_state->indexPointer, process->current_state->indexPointerSize);
				memcpy(process->current_state->indexPointer + offset, (void*)args[4], bytes_size);
				//fprintf(stderr, "glIndexPointer_fake_func type=%d, stride=%d, byte_size=%d\n", type, stride, bytes_size);
				glIndexPointer(type, stride, process->current_state->indexPointer);
				break;
			}

		case glEdgeFlagPointer_fake_func:
			{
				int offset = args[0];
				int stride = args[1];
				int bytes_size = args[2];
				process->current_state->edgeFlagPointerSize = MAX(process->current_state->edgeFlagPointerSize, offset + bytes_size);
				process->current_state->edgeFlagPointer = realloc(process->current_state->edgeFlagPointer, process->current_state->edgeFlagPointerSize);
				memcpy(process->current_state->edgeFlagPointer + offset, (void*)args[3], bytes_size );
				//fprintf(stderr, "glEdgeFlagPointer_fake_func stride = %d, bytes_size=%d\n", stride, bytes_size);
				glEdgeFlagPointer(stride, process->current_state->edgeFlagPointer);
				break;
			}

		case glVertexAttribPointerARB_fake_func:
			{
				GET_EXT_PTR(void, glVertexAttribPointerARB, (int,int,int,int,int,void*));
				int offset = args[0];
				int index = args[1];
				int size = args[2];
				int type = args[3];
				int normalized = args[4];
				int stride = args[5];
				int bytes_size = args[6];
				process->current_state->vertexAttribPointerSize[index] = MAX(process->current_state->vertexAttribPointerSize[index], offset + bytes_size);
				process->current_state->vertexAttribPointer[index] = realloc(process->current_state->vertexAttribPointer[index],
						process->current_state->vertexAttribPointerSize[index]);
				memcpy(process->current_state->vertexAttribPointer[index] + offset, (void*)args[7], bytes_size);
				ptr_func_glVertexAttribPointerARB(index, size, type, normalized, stride,
						process->current_state->vertexAttribPointer[index]);
				break;
			}

		case glVertexAttribPointerNV_fake_func:
			{
				GET_EXT_PTR(void, glVertexAttribPointerNV, (int,int,int,int,void*));
				int offset = args[0];
				int index = args[1];
				int size = args[2];
				int type = args[3];
				int stride = args[4];
				int bytes_size = args[5];
				process->current_state->vertexAttribPointerNVSize[index] = MAX(process->current_state->vertexAttribPointerNVSize[index], offset + bytes_size);
				process->current_state->vertexAttribPointerNV[index] = realloc(process->current_state->vertexAttribPointerNV[index],
						process->current_state->vertexAttribPointerNVSize[index]);
				memcpy(process->current_state->vertexAttribPointerNV[index] + offset, (void*)args[6], bytes_size);
				ptr_func_glVertexAttribPointerNV(index, size, type, stride,
						process->current_state->vertexAttribPointerNV[index]);
				break;
			}

		case glColorPointer_fake_func:
			{
				int offset = args[0];
				int size = args[1];
				int type = args[2];
				int stride = args[3];
				int bytes_size = args[4];
				process->current_state->colorPointerSize = MAX(process->current_state->colorPointerSize, offset + bytes_size);
				process->current_state->colorPointer = realloc(process->current_state->colorPointer, process->current_state->colorPointerSize);
				memcpy(process->current_state->colorPointer + offset, (void*)args[5], bytes_size);
				//fprintf(stderr, "glColorPointer_fake_func bytes_size = %d\n", bytes_size);
				glColorPointer(size, type, stride, process->current_state->colorPointer);

				break;
			}

		case glSecondaryColorPointer_fake_func:
			{
				GET_EXT_PTR(void, glSecondaryColorPointer, (int,int,int,void*));
				int offset = args[0];
				int size = args[1];
				int type = args[2];
				int stride = args[3];
				int bytes_size = args[4];
				process->current_state->secondaryColorPointerSize = MAX(process->current_state->secondaryColorPointerSize, offset + bytes_size);
				process->current_state->secondaryColorPointer = realloc(process->current_state->secondaryColorPointer, process->current_state->secondaryColorPointerSize);
				memcpy(process->current_state->secondaryColorPointer + offset, (void*)args[5], bytes_size);
				//fprintf(stderr, "glSecondaryColorPointer_fake_func bytes_size = %d\n", bytes_size);
				ptr_func_glSecondaryColorPointer(size, type, stride, process->current_state->secondaryColorPointer);

				break;
			}

		case glPushClientAttrib_func:
			{
				int mask = args[0];
				if (process->current_state->clientStateSp < MAX_CLIENT_STATE_STACK_SIZE)
				{
					process->current_state->clientStateStack[process->current_state->clientStateSp].mask = mask;
					if (mask & GL_CLIENT_VERTEX_ARRAY_BIT)
					{
						process->current_state->clientStateStack[process->current_state->clientStateSp].activeTextureIndex =
							process->current_state->activeTextureIndex;
					}
					process->current_state->clientStateSp++;
				}
				glPushClientAttrib(mask);
				break;
			}

		case glPopClientAttrib_func:
			{
				if (process->current_state->clientStateSp > 0)
				{
					process->current_state->clientStateSp--;
					if (process->current_state->clientStateStack[process->current_state->clientStateSp].mask & GL_CLIENT_VERTEX_ARRAY_BIT)
					{
						process->current_state->activeTextureIndex =
							process->current_state->clientStateStack[process->current_state->clientStateSp].activeTextureIndex;
					}
				}
				glPopClientAttrib();
				break;
			}

		case glClientActiveTexture_func:
		case glClientActiveTextureARB_func:
			{
				int activeTexture = args[0];
				process->current_state->activeTextureIndex = activeTexture - GL_TEXTURE0_ARB;
				do_glClientActiveTextureARB(activeTexture);
				break;
			}

		case glTexCoordPointer_fake_func:
			{
				int offset = args[0];
				int index = args[1];
				int size = args[2];
				int type = args[3];
				int stride = args[4];
				int bytes_size = args[5];
				process->current_state->texCoordPointerSize[index] = MAX(process->current_state->texCoordPointerSize[index], offset + bytes_size);
				process->current_state->texCoordPointer[index] = realloc(process->current_state->texCoordPointer[index], process->current_state->texCoordPointerSize[index]);
				memcpy(process->current_state->texCoordPointer[index] + offset, (void*)args[6], bytes_size);
				/*fprintf(stderr, "glTexCoordPointer_fake_func size=%d, type=%d, stride=%d, byte_size=%d\n",
				  size, type, stride, bytes_size);*/
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + index);
				glTexCoordPointer(size, type, stride, process->current_state->texCoordPointer[index]);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glWeightPointerARB_fake_func:
			{
				GET_EXT_PTR(void, glWeightPointerARB, (int,int,int,void*));
				int offset = args[0];
				int size = args[1];
				int type = args[2];
				int stride = args[3];
				int bytes_size = args[4];
				process->current_state->weightPointerSize = MAX(process->current_state->weightPointerSize, offset + bytes_size);
				process->current_state->weightPointer = realloc(process->current_state->weightPointer, process->current_state->weightPointerSize);
				memcpy(process->current_state->weightPointer + offset, (void*)args[5], bytes_size);
				/*fprintf(stderr, "glWeightPointerARB_fake_func size=%d, type=%d, stride=%d, byte_size=%d\n",
				  size, type, stride, bytes_size);*/
				ptr_func_glWeightPointerARB(size, type, stride, process->current_state->weightPointer);
				break;
			}

		case glMatrixIndexPointerARB_fake_func:
			{
				GET_EXT_PTR(void, glMatrixIndexPointerARB, (int,int,int,void*));
				int offset = args[0];
				int size = args[1];
				int type = args[2];
				int stride = args[3];
				int bytes_size = args[4];
				process->current_state->matrixIndexPointerSize = MAX(process->current_state->matrixIndexPointerSize, offset + bytes_size);
				process->current_state->matrixIndexPointer = realloc(process->current_state->matrixIndexPointer, process->current_state->matrixIndexPointerSize);
				memcpy(process->current_state->matrixIndexPointer + offset, (void*)args[5], bytes_size);
				/*fprintf(stderr, "glMatrixIndexPointerARB_fake_func size=%d, type=%d, stride=%d, byte_size=%d\n",
				  size, type, stride, bytes_size);*/
				ptr_func_glMatrixIndexPointerARB(size, type, stride, process->current_state->matrixIndexPointer);
				break;
			}

		case glFogCoordPointer_fake_func:
			{
				GET_EXT_PTR(void, glFogCoordPointer, (int,int,void*));
				int offset = args[0];
				int type = args[1];
				int stride = args[2];
				int bytes_size = args[3];
				process->current_state->fogCoordPointerSize = MAX(process->current_state->fogCoordPointerSize, offset + bytes_size);
				process->current_state->fogCoordPointer = realloc(process->current_state->fogCoordPointer, process->current_state->fogCoordPointerSize);
				memcpy(process->current_state->fogCoordPointer + offset, (void*)args[4], bytes_size);
				//fprintf(stderr, "glFogCoordPointer_fake_func type=%d, stride=%d, byte_size=%d\n", type, stride, bytes_size);
				ptr_func_glFogCoordPointer(type, stride, process->current_state->fogCoordPointer);
				break;
			}

		case glVariantPointerEXT_fake_func:
			{
				GET_EXT_PTR(void, glVariantPointerEXT, (int,int,int,void*));
				int offset = args[0];
				int id = args[1];
				int type = args[2];
				int stride = args[3];
				int bytes_size = args[4];
				process->current_state->variantPointerEXTSize[id] = MAX(process->current_state->variantPointerEXTSize[id], offset + bytes_size);
				process->current_state->variantPointerEXT[id] = realloc(process->current_state->variantPointerEXT[id], process->current_state->variantPointerEXTSize[id]);
				memcpy(process->current_state->variantPointerEXT[id] + offset, (void*)args[5], bytes_size);
				//fprintf(stderr, "glVariantPointerEXT_fake_func[%d] type=%d, stride=%d, byte_size=%d\n", id, type, stride, bytes_size);
				ptr_func_glVariantPointerEXT(id, type, stride, process->current_state->variantPointerEXT[id]);
				break;
			}

		case glInterleavedArrays_fake_func:
			{
				GET_EXT_PTR(void, glInterleavedArrays, (int,int,void*));
				int offset = args[0];
				int format = args[1];
				int stride = args[2];
				int bytes_size = args[3];
				process->current_state->interleavedArraysSize = MAX(process->current_state->interleavedArraysSize, offset + bytes_size);
				process->current_state->interleavedArrays = realloc(process->current_state->interleavedArrays, process->current_state->interleavedArraysSize);
				memcpy(process->current_state->interleavedArrays + offset, (void*)args[4], bytes_size);
				//fprintf(stderr, "glInterleavedArrays_fake_func format=%d, stride=%d, byte_size=%d\n", format, stride, bytes_size);
				ptr_func_glInterleavedArrays(format, stride, process->current_state->interleavedArrays);
				break;
			}

		case glElementPointerATI_fake_func:
			{
				GET_EXT_PTR(void, glElementPointerATI, (int,void*));
				int type = args[0];
				int bytes_size = args[1];
				process->current_state->elementPointerATISize = bytes_size;
				process->current_state->elementPointerATI = realloc(process->current_state->elementPointerATI, process->current_state->elementPointerATISize);
				memcpy(process->current_state->elementPointerATI, (void*)args[2], bytes_size);
				//fprintf(stderr, "glElementPointerATI_fake_func type=%d, byte_size=%d\n", type, bytes_size);
				ptr_func_glElementPointerATI(type, process->current_state->elementPointerATI);
				break;
			}

		case glTexCoordPointer01_fake_func:
			{
				int size = args[0];
				int type = args[1];
				int stride = args[2];
				int bytes_size = args[3];
				process->current_state->texCoordPointerSize[0] = bytes_size;
				process->current_state->texCoordPointer[0] = realloc(process->current_state->texCoordPointer[0], bytes_size);
				memcpy(process->current_state->texCoordPointer[0], (void*)args[4], bytes_size);
				/*fprintf(stderr, "glTexCoordPointer01_fake_func size=%d, type=%d, stride=%d, byte_size=%d\n",
				  size, type, stride, bytes_size);*/
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(size, type, stride, process->current_state->texCoordPointer[0]);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
				glTexCoordPointer(size, type, stride, process->current_state->texCoordPointer[0]);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glTexCoordPointer012_fake_func:
			{
				int size = args[0];
				int type = args[1];
				int stride = args[2];
				int bytes_size = args[3];
				process->current_state->texCoordPointerSize[0] = bytes_size;
				process->current_state->texCoordPointer[0] = realloc(process->current_state->texCoordPointer[0], bytes_size);
				memcpy(process->current_state->texCoordPointer[0], (void*)args[4], bytes_size);
				/*fprintf(stderr, "glTexCoordPointer012_fake_func size=%d, type=%d, stride=%d, byte_size=%d\n",
				  size, type, stride, bytes_size);*/
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(size, type, stride, process->current_state->texCoordPointer[0]);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
				glTexCoordPointer(size, type, stride, process->current_state->texCoordPointer[0]);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 2);
				glTexCoordPointer(size, type, stride, process->current_state->texCoordPointer[0]);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glVertexAndNormalPointer_fake_func:
			{
				int vertexPointerSize = args[0];
				int vertexPointerType = args[1];
				int vertexPointerStride = args[2];
				int normalPointerType = args[3];
				int normalPointerStride = args[4];
				int bytes_size = args[5];
				void* ptr = (void*)args[6];
				process->current_state->vertexPointerSize = bytes_size;
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, bytes_size);
				memcpy(process->current_state->vertexPointer, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, vertexPointerStride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, normalPointerStride, process->current_state->vertexPointer);
				break;
			}

		case glVertexNormalPointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset= args[i++];
				int normalPointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				break;
			}

		case glTuxRacerDrawElements_fake_func:
			{
				int mode = args[0];
				int count = args[1];
				int isColorEnabled = args[2];
				void* ptr = (void*)args[3];
				int stride = 6 * sizeof(float) + ((isColorEnabled) ? 4 * sizeof(unsigned char) : 0);
				glVertexPointer( 3, GL_FLOAT, stride, ptr);
				glNormalPointer( GL_FLOAT, stride, ptr + 3 * sizeof(float));
				if (isColorEnabled)
					glColorPointer( 4, GL_UNSIGNED_BYTE, stride, ptr + 6 * sizeof(float));
				glDrawArrays(mode, 0, count);
				break;
			}

		case glVertexNormalColorPointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset= args[i++];
				int normalPointerType = args[i++];
				int colorPointerOffset = args[i++];
				int colorPointerSize = args[i++];
				int colorPointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				glColorPointer(colorPointerSize, colorPointerType, stride, process->current_state->vertexPointer + colorPointerOffset);
				break;
			}

		case glVertexColorTexCoord0PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int colorPointerOffset = args[i++];
				int colorPointerSize = args[i++];
				int colorPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glColorPointer(colorPointerSize, colorPointerType, stride, process->current_state->vertexPointer + colorPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glVertexNormalTexCoord0PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset = args[i++];
				int normalPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glVertexNormalTexCoord01PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset = args[i++];
				int normalPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int texCoord1PointerOffset = args[i++];
				int texCoord1PointerSize = args[i++];
				int texCoord1PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
				glTexCoordPointer(texCoord1PointerSize, texCoord1PointerType, stride, process->current_state->vertexPointer + texCoord1PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glVertexNormalTexCoord012PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset = args[i++];
				int normalPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int texCoord1PointerOffset = args[i++];
				int texCoord1PointerSize = args[i++];
				int texCoord1PointerType = args[i++];
				int texCoord2PointerOffset = args[i++];
				int texCoord2PointerSize = args[i++];
				int texCoord2PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
				glTexCoordPointer(texCoord1PointerSize, texCoord1PointerType, stride, process->current_state->vertexPointer + texCoord1PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 2);
				glTexCoordPointer(texCoord2PointerSize, texCoord2PointerType, stride, process->current_state->vertexPointer + texCoord2PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glVertexNormalColorTexCoord0PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset = args[i++];
				int normalPointerType = args[i++];
				int colorPointerOffset = args[i++];
				int colorPointerSize = args[i++];
				int colorPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				glColorPointer(colorPointerSize, colorPointerType, stride, process->current_state->vertexPointer + colorPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glVertexNormalColorTexCoord01PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset = args[i++];
				int normalPointerType = args[i++];
				int colorPointerOffset = args[i++];
				int colorPointerSize = args[i++];
				int colorPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int texCoord1PointerOffset = args[i++];
				int texCoord1PointerSize = args[i++];
				int texCoord1PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				glColorPointer(colorPointerSize, colorPointerType, stride, process->current_state->vertexPointer + colorPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
				glTexCoordPointer(texCoord1PointerSize, texCoord1PointerType, stride, process->current_state->vertexPointer + texCoord1PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glVertexNormalColorTexCoord012PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset = args[i++];
				int normalPointerType = args[i++];
				int colorPointerOffset = args[i++];
				int colorPointerSize = args[i++];
				int colorPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int texCoord1PointerOffset = args[i++];
				int texCoord1PointerSize = args[i++];
				int texCoord1PointerType = args[i++];
				int texCoord2PointerOffset = args[i++];
				int texCoord2PointerSize = args[i++];
				int texCoord2PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				glColorPointer(colorPointerSize, colorPointerType, stride, process->current_state->vertexPointer + colorPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
				glTexCoordPointer(texCoord1PointerSize, texCoord1PointerType, stride, process->current_state->vertexPointer + texCoord1PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 2);
				glTexCoordPointer(texCoord2PointerSize, texCoord2PointerType, stride, process->current_state->vertexPointer + texCoord2PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case _glVertexPointer_buffer_func:
			{
				glVertexPointer(args[0],args[1],args[2],(void*)args[3]);
				break;
			}

		case _glNormalPointer_buffer_func:
			{
				glNormalPointer(args[0],args[1],(void*)args[2]);
				break;
			}

		case _glColorPointer_buffer_func:
			{
				glColorPointer(args[0],args[1],args[2],(void*)args[3]);
				break;
			}

		case _glSecondaryColorPointer_buffer_func:
			{
				GET_EXT_PTR(void, glSecondaryColorPointer, (int,int,int,void*));
				ptr_func_glSecondaryColorPointer(args[0],args[1],args[2],(void*)args[3]);
				break;
			}

		case _glIndexPointer_buffer_func:
			{
				glIndexPointer(args[0],args[1],(void*)args[2]);
				break;
			}

		case _glTexCoordPointer_buffer_func:
			{
				glTexCoordPointer(args[0],args[1],args[2],(void*)args[3]);
				break;
			}

		case _glEdgeFlagPointer_buffer_func:
			{
				glEdgeFlagPointer(args[0],(void*)args[1]);
				break;
			}

		case _glVertexAttribPointerARB_buffer_func:
			{
				GET_EXT_PTR(void, glVertexAttribPointerARB, (int,int,int,int,int,void*));
				ptr_func_glVertexAttribPointerARB(args[0], args[1], args[2], args[3], args[4], (void*)args[5]);
				break;
			}

		case _glWeightPointerARB_buffer_func:
			{
				GET_EXT_PTR(void, glWeightPointerARB, (int,int,int,void*));
				ptr_func_glWeightPointerARB(args[0], args[1], args[2], (void*)args[3]);
				break;
			}

		case _glMatrixIndexPointerARB_buffer_func:
			{
				GET_EXT_PTR(void, glMatrixIndexPointerARB, (int,int,int,void*));
				ptr_func_glMatrixIndexPointerARB(args[0], args[1], args[2], (void*)args[3]);
				break;
			}

		case _glFogCoordPointer_buffer_func:
			{
				GET_EXT_PTR(void, glFogCoordPointer, (int,int,void*));
				ptr_func_glFogCoordPointer(args[0], args[1], (void*)args[2]);
				break;
			}

		case _glVariantPointerEXT_buffer_func:
			{
				GET_EXT_PTR(void, glVariantPointerEXT, (int, int,int,void*));
				ptr_func_glVariantPointerEXT(args[0], args[1], args[2], (void*)args[3]);
				break;
			}

		case _glDrawElements_buffer_func:
			{
				glDrawElements(args[0],args[1],args[2],(void*)args[3]);
				break;
			}

		case _glDrawRangeElements_buffer_func:
			{
				GET_EXT_PTR(void, glDrawRangeElements, ( GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices ));
				ptr_func_glDrawRangeElements(args[0],args[1],args[2],args[3],args[4],(void*)args[5]);
				break;
			}

		case _glMultiDrawElements_buffer_func:
			{
				GET_EXT_PTR(void, glMultiDrawElements, (int,int*,int,void**, int));
				ptr_func_glMultiDrawElements(args[0],(int*)args[1],args[2],(void**)args[3],args[4]);
				break;
			}

		case _glGetError_fake_func:
			{
				break;
			}

		case glGetIntegerv_func:
			{
				glGetIntegerv(args[0], (int*)args[1]);
				break;
			}

		case _glReadPixels_pbo_func:
			{
				glReadPixels(ARG_TO_INT(args[0]), ARG_TO_INT(args[1]), ARG_TO_INT(args[2]), ARG_TO_INT(args[3]), ARG_TO_UNSIGNED_INT(args[4]), ARG_TO_UNSIGNED_INT(args[5]), (void*)(args[6]));
				break;
			}

		case _glDrawPixels_pbo_func:
			{
				glDrawPixels(ARG_TO_INT(args[0]), ARG_TO_INT(args[1]), ARG_TO_UNSIGNED_INT(args[2]), ARG_TO_UNSIGNED_INT(args[3]), (const void*)(args[4]));
				break;
			}

		case _glMapBufferARB_fake_func:
			{
				GET_EXT_PTR(GLvoid*, glMapBufferARB, (GLenum, GLenum));
				GET_EXT_PTR(GLboolean, glUnmapBufferARB, (GLenum));
				int target = args[0];
				int size = args[1];
				void* dst_ptr = (void*)args[2];
				void* src_ptr = ptr_func_glMapBufferARB(target, GL_READ_ONLY);
				if (src_ptr)
				{
					memcpy(dst_ptr, src_ptr, size);
					ret_int = ptr_func_glUnmapBufferARB(target);
				}
				else
				{
					ret_int = 0;
				}
				break;
			}

		case fake_gluBuild2DMipmaps_func:
			{
				GET_GLU_PTR(GLint, gluBuild2DMipmaps, (GLenum arg_0, GLint arg_1, GLsizei arg_2, GLsizei arg_3, GLenum arg_4, GLenum arg_5, const GLvoid * arg_6));
				if (ptr_func_gluBuild2DMipmaps == NULL)
					ptr_func_gluBuild2DMipmaps = mesa_gluBuild2DMipmaps;
				ptr_func_gluBuild2DMipmaps(ARG_TO_UNSIGNED_INT(args[0]), ARG_TO_INT(args[1]), ARG_TO_INT(args[2]), ARG_TO_INT(args[3]), ARG_TO_UNSIGNED_INT(args[4]), ARG_TO_UNSIGNED_INT(args[5]), (const void*)(args[6]));
				break;
			}

		case _glSelectBuffer_fake_func:
			{
				process->current_state->selectBufferSize = args[0] * 4;
				process->current_state->selectBufferPtr = realloc(process->current_state->selectBufferPtr, process->current_state->selectBufferSize);
				glSelectBuffer(args[0], process->current_state->selectBufferPtr);
				break;
			}

		case _glGetSelectBuffer_fake_func:
			{
				void* ptr = (void*)args[0];
				memcpy(ptr, process->current_state->selectBufferPtr, process->current_state->selectBufferSize);
				break;
			}

		case _glFeedbackBuffer_fake_func:
			{
				process->current_state->feedbackBufferSize = args[0] * 4;
				process->current_state->feedbackBufferPtr = realloc(process->current_state->feedbackBufferPtr, process->current_state->feedbackBufferSize);
				glFeedbackBuffer(args[0], args[1], process->current_state->feedbackBufferPtr);
				break;
			}

		case _glGetFeedbackBuffer_fake_func:
			{
				void* ptr = (void*)args[0];
				memcpy(ptr, process->current_state->feedbackBufferPtr, process->current_state->feedbackBufferSize);
				break;
			}

		case glGetError_func:
			{
				ret_int = glGetError();
				break;
			}

		case glNewObjectBufferATI_func:
			{
				GET_EXT_PTR(int, glNewObjectBufferATI, (int,void*, int));
				ret_int = ptr_func_glNewObjectBufferATI(args[0], (void*)args[1], args[2]);
				break;
			}

		default:
			execute_func(func_number, args, &ret_int, &ret_char);
			break;
	}

	Signature* signature = (Signature*)tab_opengl_calls[func_number];
	int ret_type = signature->ret_type;
	switch(ret_type)
	{
		case TYPE_NONE:
			break;

		case TYPE_CHAR:
		case TYPE_UNSIGNED_CHAR:
			ret_int = ret_char;
			break;

		case TYPE_INT:
		case TYPE_UNSIGNED_INT:
			break;

		case TYPE_CONST_CHAR:
			{
				strncpy(ret_string, (ret_str) ? ret_str : "", 32768);
				break;
			}

		default:
			fprintf(stderr, "unexpected ret type : %d\n", ret_type);
			exit(-1);
			break;
	}

	return ret_int;
}

#else

#include <arpa/inet.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES

#include "opengl_func.h"
#include "opengl_utils.h"
#include "mesa_gl.h"
#include "mesa_glx.h"
#include "mesa_glu.h"
#include "mesa_mipmap.c"

//#define SYSTEMATIC_ERROR_CHECK

#define GET_EXT_PTR(type, funcname, args_decl) \
	static int detect_##funcname = 0; \
static type(*ptr_func_##funcname)args_decl = NULL; \
if (detect_##funcname == 0) \
{ \
	detect_##funcname = 1; \
	ptr_func_##funcname = (type(*)args_decl)glXGetProcAddressARB((const GLubyte*)#funcname); \
	assert (ptr_func_##funcname); \
}

#define GET_EXT_PTR_NO_FAIL(type, funcname, args_decl) \
	static int detect_##funcname = 0; \
static type(*ptr_func_##funcname)args_decl = NULL; \
if (detect_##funcname == 0) \
{ \
	detect_##funcname = 1; \
	ptr_func_##funcname = (type(*)args_decl)glXGetProcAddressARB((const GLubyte*)#funcname); \
}

#ifndef WIN32
#include <dlfcn.h>
#endif

static void* get_glu_ptr(const char* name)
{
	static void* handle = (void*)-1;
	if (handle == (void*)-1)
	{
#ifndef WIN32
		handle = dlopen("libGLU.so" ,RTLD_LAZY);
		if (!handle) {
			fprintf (stderr, "can't load libGLU.so : %s\n", dlerror());
		}
#else
		handle = (void *)LoadLibrary("glu32.dll");
		if (!handle) {
			fprintf (stderr, "can't load glu32.dll\n");
		}
#endif
	}
	if (handle)
	{
#ifndef WIN32
		return dlsym(handle, name);
#else
		return GetProcAddress(handle, name);
#endif
	}
	return NULL;
}

#define GET_GLU_PTR(type, funcname, args_decl) \
	static int detect_##funcname = 0; \
static type(*ptr_func_##funcname)args_decl = NULL; \
if (detect_##funcname == 0) \
{ \
	detect_##funcname = 1; \
	ptr_func_##funcname = (type(*)args_decl)get_glu_ptr(#funcname); \
}

int display_function_call = 0;

static const int defaultAttribList[] = {
	GLX_RGBA,
	GLX_RED_SIZE, 1,
	GLX_GREEN_SIZE, 1,
	GLX_BLUE_SIZE, 1,
	GLX_DOUBLEBUFFER,
	None
};

static XVisualInfo* get_default_visual(Display* dpy)
{
	fprintf(stderr, "get_default_visual\n");
	static XVisualInfo* vis = NULL;
	XVisualInfo theTemplate;
	int numVisuals;
	if (vis) return vis;
	/*if (vis == NULL)
	  vis = glXChooseVisual(dpy, 0, (int*)defaultAttribList);*/
	theTemplate.screen = 0;
	vis = XGetVisualInfo(dpy, VisualScreenMask, &theTemplate, &numVisuals);
	return vis;
}

void opengl_exec_set_parent_window(OGLS_Conn *pConn, Window _parent_window)
{
	pConn->parent_dpy = pConn->Display;
	pConn->qemu_parent_window = _parent_window;
	if (pConn->active_win)
	{
		XReparentWindow(pConn->Display, pConn->active_win, _parent_window, pConn->active_win_x, pConn->active_win_y);
	}
}

static Window create_window(OGLS_Conn *pConn, Window local_parent_window, XVisualInfo* vis, const char *name,
		int x, int y, int width, int height)
{
	int scrnum;
	XSetWindowAttributes attr = {0};
	unsigned long mask;
	Window root;
	Window win;

	scrnum = DefaultScreen( pConn->Display );
	root = RootWindow( pConn->Display, scrnum );

	/* window attributes */
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap( pConn->Display, root, vis->visual, AllocNone);
	attr.event_mask = StructureNotifyMask | ExposureMask /*| KeyPressMask*/;
	attr.save_under = True;
	//if (local_parent_window == NULL && qemu_parent_window == NULL)
	attr.override_redirect = False;
	//else
	//  attr.override_redirect = True;
	attr.cursor = None;
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask | CWOverrideRedirect | CWSaveUnder ;

	if (local_parent_window)
	{
		win = XCreateWindow( pConn->Display, local_parent_window, 0, 0, width, height,
				0, vis->depth, InputOutput,
				vis->visual, mask, &attr );
	}
	else if (pConn->qemu_parent_window)
	{
		win = XCreateWindow( pConn->Display, pConn->qemu_parent_window, 0, 0, width, height,
				0, vis->depth, InputOutput,
				vis->visual, mask, &attr );
	}
	else
	{
		win = XCreateWindow( pConn->Display, root, 0, 0, width, height,
				0, vis->depth, InputOutput,
				vis->visual, mask, &attr );
	}

	/* set hints and properties */
	{
		XSizeHints sizehints;
		sizehints.x = x;
		sizehints.y = y;
		sizehints.width  = width;
		sizehints.height = height;
		sizehints.flags = USSize | USPosition;
		XSetWMNormalHints(pConn->Display, win, &sizehints);
		XSetStandardProperties(pConn->Display, win, name, name,
				None, (char **)NULL, 0, &sizehints);
	}

	/* Host Window?? ?琉??? ?苛쨈?. if( win )
	   XMapWindow(pConn->Display, win);*/

	XSync(pConn->Display, 0);

	/*
	   int loop = 1;
	   while (loop) {
	   while (XPending(pConn->Display) > 0) {
	   XEvent event;
	   XNextEvent(pConn->Display, &event);
	   switch (event.type) {
	   case CreateNotify:
	   {
	   if (((XCreateWindowEvent*)&event)->window == win)
	   {
	   loop = 0;
	   }
	   break;
	   }
	   }
	   }
	   }*/

	pConn->active_win = win;

	return win;
}

static void destroy_window(OGLS_Conn *pConn, Window win )
{
	/*int i;*/

	XDestroyWindow(pConn->Display, win);

	XSync(pConn->Display, 0);
	/*int loop = 1;
	  while (loop) {
	  while (XPending(pConn->Display) > 0) {
	  XEvent event;
	  XNextEvent(pConn->Display, &event);
	  switch (event.type) {
	  case DestroyNotify:
	  {
	  if (((XDestroyWindowEvent*)&event)->window == win)
	  {
	  loop = 0;
	  }
	  break;
	  }
	  }
	  }
	  }*/

	if( pConn->active_win == win)
		pConn->active_win = 0;

}


typedef struct
{
	void* key;
	void* value;
} Assoc;


#define MAX_HANDLED_PROCESS 100
#define MAX_ASSOC_SIZE 100

#define MAX_FBCONFIG 10


#define MAX(a, b) (((a) > (b)) ? (a) : (b))


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
	GLbitfield     mask;
	int            activeTextureIndex;
} ClientState;

#define MAX_CLIENT_STATE_STACK_SIZE 16

typedef struct
{
	int ref;
	int fake_ctxt;
	int fake_shareList;
	GLXContext context;
	GLXDrawable drawable;

	void* vertexPointer;
	void* normalPointer;
	void* colorPointer;
	void* secondaryColorPointer;
	void* indexPointer;
	void* texCoordPointer[NB_MAX_TEXTURES];
	void* edgeFlagPointer;
	void* vertexAttribPointer[MY_GL_MAX_VERTEX_ATTRIBS_ARB];
	void* vertexAttribPointerNV[MY_GL_MAX_VERTEX_ATTRIBS_NV];
	void* weightPointer;
	void* matrixIndexPointer;
	void* fogCoordPointer;
	void* variantPointerEXT[MY_GL_MAX_VARIANT_POINTER_EXT];
	void* interleavedArrays;
	void* elementPointerATI;

	int vertexPointerSize;
	int normalPointerSize;
	int colorPointerSize;
	int secondaryColorPointerSize;
	int indexPointerSize;
	int texCoordPointerSize[NB_MAX_TEXTURES];
	int edgeFlagPointerSize;
	int vertexAttribPointerSize[MY_GL_MAX_VERTEX_ATTRIBS_ARB];
	int vertexAttribPointerNVSize[MY_GL_MAX_VERTEX_ATTRIBS_NV];
	int weightPointerSize;
	int matrixIndexPointerSize;
	int fogCoordPointerSize;
	int variantPointerEXTSize[MY_GL_MAX_VARIANT_POINTER_EXT];
	int interleavedArraysSize;
	int elementPointerATISize;

	int selectBufferSize;
	void* selectBufferPtr;
	int feedbackBufferSize;
	void* feedbackBufferPtr;

	ClientState clientStateStack[MAX_CLIENT_STATE_STACK_SIZE];
	int clientStateSp;
	int activeTextureIndex;

	unsigned int ownTabTextures[32768];
	unsigned int* tabTextures;
	RangeAllocator ownTextureAllocator;
	RangeAllocator* textureAllocator;

	unsigned int ownTabBuffers[32768];
	unsigned int* tabBuffers;
	RangeAllocator ownBufferAllocator;
	RangeAllocator* bufferAllocator;

	unsigned int ownTabLists[32768];
	unsigned int* tabLists;
	RangeAllocator ownListAllocator;
	RangeAllocator* listAllocator;

#ifdef SYSTEMATIC_ERROR_CHECK
	int last_error;
#endif
} GLState;

typedef struct
{
	int internal_num;
	int process_id;
	int instr_counter;

	int x, y, width, height;
	WindowPosStruct currentDrawablePos;

	int next_available_context_number;
	int next_available_pbuffer_number;

	int nbGLStates;
	GLState default_state;
	GLState** glstates;
	GLState* current_state;

	int nfbconfig;
	GLXFBConfig* fbconfigs[MAX_FBCONFIG];
	int fbconfigs_max[MAX_FBCONFIG];
	int nfbconfig_total;

	Assoc association_fakecontext_glxcontext[MAX_ASSOC_SIZE];
	Assoc association_fakepbuffer_pbuffer[MAX_ASSOC_SIZE];
	Assoc association_clientdrawable_serverdrawable[MAX_ASSOC_SIZE];
	Assoc association_fakecontext_visual[MAX_ASSOC_SIZE];
} ProcessStruct;

int last_process_id = 0;


#define ARG_TO_CHAR(x)                (char)(x)
#define ARG_TO_UNSIGNED_CHAR(x)       (unsigned char)(x)
#define ARG_TO_SHORT(x)               (short)(x)
#define ARG_TO_UNSIGNED_SHORT(x)      (unsigned short)(x)
#define ARG_TO_INT(x)                 (int)(x)
#define ARG_TO_UNSIGNED_INT(x)        (unsigned int)(x)
#define ARG_TO_FLOAT(x)               (*(float*)&(x))
#define ARG_TO_DOUBLE(x)              (*(double*)(x))

#include "server_stub.c"

/* ---- */

static void* get_association_fakecontext_glxcontext(ProcessStruct* process, void* fakecontext)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_glxcontext[i].key != NULL;i++)
	{
		if (process->association_fakecontext_glxcontext[i].key == fakecontext)
			return process->association_fakecontext_glxcontext[i].value;
	}
	return NULL;
}

static void set_association_fakecontext_glxcontext(ProcessStruct* process, void* fakecontext, void* glxcontext)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_glxcontext[i].key != NULL;i++)
	{
		if (process->association_fakecontext_glxcontext[i].key == fakecontext)
		{
			break;
		}
	}
	if (i < MAX_ASSOC_SIZE)
	{
		process->association_fakecontext_glxcontext[i].key = fakecontext;
		process->association_fakecontext_glxcontext[i].value = glxcontext;
	}
	else
	{
		fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
	}
}

static void unset_association_fakecontext_glxcontext(ProcessStruct* process, void* fakecontext)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_glxcontext[i].key != NULL;i++)
	{
		if (process->association_fakecontext_glxcontext[i].key == fakecontext)
		{
			memmove(&process->association_fakecontext_glxcontext[i],
					&process->association_fakecontext_glxcontext[i+1],
					sizeof(Assoc) * (MAX_ASSOC_SIZE - 1 - i));
			return;
		}
	}
}

/* ---- */

static void* get_association_fakecontext_visual(ProcessStruct* process, void* fakecontext)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_visual[i].key != NULL;i++)
	{
		if (process->association_fakecontext_visual[i].key == fakecontext)
			return process->association_fakecontext_visual[i].value;
	}
	return NULL;
}

static void set_association_fakecontext_visual(ProcessStruct* process, void* fakecontext, void* visual)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_visual[i].key != NULL;i++)
	{
		if (process->association_fakecontext_visual[i].key == fakecontext)
		{
			break;
		}
	}
	if (i < MAX_ASSOC_SIZE)
	{
		process->association_fakecontext_visual[i].key = fakecontext;
		process->association_fakecontext_visual[i].value = visual;
	}
	else
	{
		fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
	}
}

/* ---- */

static void* get_association_fakepbuffer_pbuffer(ProcessStruct* process, void* fakepbuffer)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakepbuffer_pbuffer[i].key != NULL;i++)
	{
		if (process->association_fakepbuffer_pbuffer[i].key == fakepbuffer)
			return process->association_fakepbuffer_pbuffer[i].value;
	}
	return NULL;
}
static void set_association_fakepbuffer_pbuffer(ProcessStruct* process, void* fakepbuffer, void* pbuffer)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakepbuffer_pbuffer[i].key != NULL;i++)
	{
		if (process->association_fakepbuffer_pbuffer[i].key == fakepbuffer)
		{
			break;
		}
	}
	if (i < MAX_ASSOC_SIZE)
	{
		process->association_fakepbuffer_pbuffer[i].key = fakepbuffer;
		process->association_fakepbuffer_pbuffer[i].value = pbuffer;
	}
	else
	{
		fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
	}
}
static void unset_association_fakepbuffer_pbuffer(ProcessStruct* process, void* fakepbuffer)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_fakepbuffer_pbuffer[i].key != NULL;i++)
	{
		if (process->association_fakepbuffer_pbuffer[i].key == fakepbuffer)
		{
			memmove(&process->association_fakepbuffer_pbuffer[i],
					&process->association_fakepbuffer_pbuffer[i+1],
					sizeof(Assoc) * (MAX_ASSOC_SIZE - 1 - i));
			return;
		}
	}
}

/* ---- */


static GLXDrawable get_association_clientdrawable_serverdrawable(ProcessStruct* process, GLXDrawable clientdrawable)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_clientdrawable_serverdrawable[i].key != NULL;i++)
	{
		if (process->association_clientdrawable_serverdrawable[i].key == (void*)clientdrawable)
			return (GLXDrawable)process->association_clientdrawable_serverdrawable[i].value;
	}
	return (GLXDrawable)0;
}

#if 0
static void* get_association_serverdrawable_clientdrawable(ProcessStruct* process, GLXDrawable serverdrawable)
{
	int i;
	for(i=0;i < MAX_ASSOC_SIZE && process->association_clientdrawable_serverdrawable[i].key != NULL;i++)
	{
		if ((GLXDrawable)process->association_clientdrawable_serverdrawable[i].value == serverdrawable)
			return process->association_clientdrawable_serverdrawable[i].key;
	}
	return NULL;
}
#endif

static void set_association_clientdrawable_serverdrawable(ProcessStruct* process, void* clientdrawable, void* serverdrawable)
{
	int i;
	for(i=0;process->association_clientdrawable_serverdrawable[i].key != NULL;i++)
	{
		if (process->association_clientdrawable_serverdrawable[i].key == clientdrawable)
		{
			break;
		}
	}
	if (i < MAX_ASSOC_SIZE)
	{
		process->association_clientdrawable_serverdrawable[i].key = clientdrawable;
		process->association_clientdrawable_serverdrawable[i].value = serverdrawable;
	}
	else
	{
		fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
	}
}

static void _get_window_pos(Display *dpy, Window win, WindowPosStruct* pos)
{
	XWindowAttributes window_attributes_return;
	Window child;
	int x, y;
	Window root = DefaultRootWindow(dpy);
	XGetWindowAttributes(dpy, win, &window_attributes_return);
	XTranslateCoordinates(dpy, win, root, 0, 0, &x, &y, &child);
	/*printf("%d %d %d %d\n", x, y,
	  window_attributes_return.width, window_attributes_return.height);*/
	pos->x = x;
	pos->y = y;
	pos->width = window_attributes_return.width;
	pos->height = window_attributes_return.height;
	pos->map_state = window_attributes_return.map_state;
}

static int is_gl_vendor_ati(Display* dpy)
{
	static int is_gl_vendor_ati_flag = 0;
	static int has_init = 0;
	if (has_init == 0)
	{
		has_init = 1;
		is_gl_vendor_ati_flag = (strncmp(glXGetClientString(dpy, GLX_VENDOR), "ATI", 3) == 0);
	}
	return is_gl_vendor_ati_flag;
}

static int get_server_texture(ProcessStruct* process, unsigned int client_texture)
{
	unsigned int server_texture = 0;
	if (client_texture < 32768)
	{
		server_texture = process->current_state->tabTextures[client_texture];
	}
	else
	{
		fprintf(stderr, "invalid texture name %d\n", client_texture);
	}
	return server_texture;
}

static int get_server_buffer(ProcessStruct* process, unsigned int client_buffer)
{
	unsigned int server_buffer = 0;
	if (client_buffer < 32768)
	{
		server_buffer = process->current_state->tabBuffers[client_buffer];
	}
	else
	{
		fprintf(stderr, "invalid buffer name %d\n", client_buffer);
	}
	return server_buffer;
}


static int get_server_list(ProcessStruct* process, unsigned int client_list)
{
	unsigned int server_list = 0;
	if (client_list < 32768)
	{
		server_list = process->current_state->tabLists[client_list];
	}
	else
	{
		fprintf(stderr, "invalid list name %d\n", client_list);
	}
	return server_list;
}

static GLXFBConfig get_fbconfig(ProcessStruct* process, int client_fbconfig)
{
	int i;
	int nbtotal = 0;
	for(i=0;i<process->nfbconfig;i++)
	{
		assert(client_fbconfig >= 1 + nbtotal);
		if (client_fbconfig <= nbtotal + process->fbconfigs_max[i])
		{
			return process->fbconfigs[i][client_fbconfig-1 - nbtotal];
		}
		nbtotal += process->fbconfigs_max[i];
	}
	return 0;
}

typedef struct
{
	int attribListLength;
	int* attribList;
	XVisualInfo* visInfo;
} AssocAttribListVisual;

static int _compute_length_of_attrib_list_including_zero(const int* attribList, int booleanMustHaveValue)
{
	int i = 0;
	while(attribList[i])
	{
		if (booleanMustHaveValue ||
				!(attribList[i] == GLX_USE_GL ||
					attribList[i] == GLX_RGBA ||
					attribList[i] == GLX_DOUBLEBUFFER ||
					attribList[i] == GLX_STEREO))
		{
			i+=2;
		}
		else
		{
			i++;
		}
	}
	return i + 1;
}

static int glXChooseVisualFunc( OGLS_Conn *pConn, const int* _attribList)
{
	AssocAttribListVisual *tabAssocAttribListVisual = (AssocAttribListVisual *)pConn->tabAssocAttribListVisual ;

	if (_attribList == NULL)
		return 0;
	int attribListLength = _compute_length_of_attrib_list_including_zero(_attribList, 0);
	int i;

	int* attribList = malloc(sizeof(int) * attribListLength);
	memcpy(attribList, _attribList, sizeof(int) * attribListLength);

	i = 0;
	while(attribList[i])
	{
		if (!(attribList[i] == GLX_USE_GL ||
					attribList[i] == GLX_RGBA ||
					attribList[i] == GLX_DOUBLEBUFFER ||
					attribList[i] == GLX_STEREO))
		{
			if (attribList[i] == GLX_SAMPLE_BUFFERS && attribList[i+1] != 0 && getenv("DISABLE_SAMPLE_BUFFERS"))
			{
				fprintf(stderr, "Disabling GLX_SAMPLE_BUFFERS\n");
				attribList[i+1] = 0;
			}
			i+=2;
		}
		else
		{
			i++;
		}
	}

	for(i=0;i<pConn->nTabAssocAttribListVisual;i++)
	{
		if (tabAssocAttribListVisual[i].attribListLength == attribListLength &&
				memcmp(tabAssocAttribListVisual[i].attribList, attribList, attribListLength * sizeof(int)) == 0)
		{
			free(attribList);
			return (tabAssocAttribListVisual[i].visInfo) ? tabAssocAttribListVisual[i].visInfo->visualid : 0;
		}
	}
	XVisualInfo* visInfo = glXChooseVisual(pConn->Display, 0, attribList);
	pConn->tabAssocAttribListVisual = tabAssocAttribListVisual =
		realloc(tabAssocAttribListVisual, sizeof(AssocAttribListVisual) * (pConn->nTabAssocAttribListVisual+1));
	tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].attribListLength = attribListLength;
	tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].attribList = (int*)malloc(sizeof(int) * attribListLength);
	memcpy(tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].attribList, attribList, sizeof(int) * attribListLength);
	tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].visInfo = visInfo;
	pConn->nTabAssocAttribListVisual++;
	free(attribList);
	return (visInfo) ? visInfo->visualid : 0;
}

static XVisualInfo* get_visual_info_from_visual_id( OGLS_Conn *pConn, int visualid)
{
	int i, n;
	XVisualInfo template;
	XVisualInfo* visInfo;

	AssocAttribListVisual *tabAssocAttribListVisual = (AssocAttribListVisual *)pConn->tabAssocAttribListVisual ;

	for(i=0;i<pConn->nTabAssocAttribListVisual;i++)
	{
		if (tabAssocAttribListVisual[i].visInfo &&
				tabAssocAttribListVisual[i].visInfo->visualid == visualid)
		{
			return tabAssocAttribListVisual[i].visInfo;
		}
	}
	template.visualid = visualid;
	visInfo = XGetVisualInfo(pConn->Display, VisualIDMask, &template, &n);
	pConn->tabAssocAttribListVisual = tabAssocAttribListVisual =
		realloc(tabAssocAttribListVisual, sizeof(AssocAttribListVisual) * (pConn->nTabAssocAttribListVisual+1));
	tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].attribListLength = 0;
	tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].attribList = NULL;
	tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].visInfo = visInfo;
	pConn->nTabAssocAttribListVisual++;
	return visInfo;
}

typedef struct
{
	int x;
	int y;
	int width;
	int height;
	int xhot;
	int yhot;
	int* pixels;
} ClientCursor;

static ClientCursor client_cursor = { 0 };

static void do_glClientActiveTextureARB(int texture)
{
	GET_EXT_PTR_NO_FAIL(void, glClientActiveTextureARB, (int));
	if (ptr_func_glClientActiveTextureARB)
	{
		ptr_func_glClientActiveTextureARB(texture);
	}
}

static void do_glActiveTextureARB(int texture)
{
	GET_EXT_PTR_NO_FAIL(void, glActiveTextureARB, (int));
	if (ptr_func_glActiveTextureARB)
	{
		ptr_func_glActiveTextureARB(texture);
	}
}

static void do_glUseProgramObjectARB(GLhandleARB programObj)
{
	GET_EXT_PTR_NO_FAIL(void, glUseProgramObjectARB, (GLhandleARB));
	if (ptr_func_glUseProgramObjectARB)
	{
		ptr_func_glUseProgramObjectARB(programObj);
	}
}

static void destroy_gl_state(GLState* state)
{
	int i;
	if (state->vertexPointer) free(state->vertexPointer);
	if (state->normalPointer) free(state->normalPointer);
	if (state->indexPointer) free(state->indexPointer);
	if (state->colorPointer) free(state->colorPointer);
	if (state->secondaryColorPointer) free(state->secondaryColorPointer);
	for(i=0;i<NB_MAX_TEXTURES;i++)
	{
		if (state->texCoordPointer[i]) free(state->texCoordPointer[i]);
	}
	for(i=0;i<MY_GL_MAX_VERTEX_ATTRIBS_ARB;i++)
	{
		if (state->vertexAttribPointer[i]) free(state->vertexAttribPointer[i]);
	}
	for(i=0;i<MY_GL_MAX_VERTEX_ATTRIBS_NV;i++)
	{
		if (state->vertexAttribPointerNV[i]) free(state->vertexAttribPointerNV[i]);
	}
	if (state->weightPointer) free(state->weightPointer);
	if (state->matrixIndexPointer) free(state->matrixIndexPointer);
	if (state->fogCoordPointer) free(state->fogCoordPointer);
	for(i=0;i<MY_GL_MAX_VARIANT_POINTER_EXT;i++)
	{
		if (state->variantPointerEXT[i]) free(state->variantPointerEXT[i]);
	}
	if (state->interleavedArrays) free(state->interleavedArrays);
	if (state->elementPointerATI) free(state->elementPointerATI);
}

static void init_gl_state(GLState* state)
{
	state->textureAllocator = &state->ownTextureAllocator;
	state->tabTextures = state->ownTabTextures;
	state->bufferAllocator = &state->ownBufferAllocator;
	state->tabBuffers = state->ownTabBuffers;
	state->listAllocator = &state->ownListAllocator;
	state->tabLists = state->ownTabLists;
}

/*
 * Translate the nth element of list from type to GLuint.
 */
	static GLuint
translate_id(GLsizei n, GLenum type, const GLvoid * list)
{
	GLbyte *bptr;
	GLubyte *ubptr;
	GLshort *sptr;
	GLushort *usptr;
	GLint *iptr;
	GLuint *uiptr;
	GLfloat *fptr;

	switch (type) {
		case GL_BYTE:
			bptr = (GLbyte *) list;
			return (GLuint) *(bptr + n);
		case GL_UNSIGNED_BYTE:
			ubptr = (GLubyte *) list;
			return (GLuint) *(ubptr + n);
		case GL_SHORT:
			sptr = (GLshort *) list;
			return (GLuint) *(sptr + n);
		case GL_UNSIGNED_SHORT:
			usptr = (GLushort *) list;
			return (GLuint) *(usptr + n);
		case GL_INT:
			iptr = (GLint *) list;
			return (GLuint) *(iptr + n);
		case GL_UNSIGNED_INT:
			uiptr = (GLuint *) list;
			return (GLuint) *(uiptr + n);
		case GL_FLOAT:
			fptr = (GLfloat *) list;
			return (GLuint) *(fptr + n);
		case GL_2_BYTES:
			ubptr = ((GLubyte *) list) + 2 * n;
			return (GLuint) *ubptr * 256 + (GLuint) * (ubptr + 1);
		case GL_3_BYTES:
			ubptr = ((GLubyte *) list) + 3 * n;
			return (GLuint) * ubptr * 65536
				+ (GLuint) *(ubptr + 1) * 256 + (GLuint) * (ubptr + 2);
		case GL_4_BYTES:
			ubptr = ((GLubyte *) list) + 4 * n;
			return (GLuint) *ubptr * 16777216
				+ (GLuint) *(ubptr + 1) * 65536
				+ (GLuint) *(ubptr + 2) * 256 + (GLuint) * (ubptr + 3);
		default:
			return 0;
	}
}

static void _create_context(ProcessStruct* process, GLXContext ctxt, int fake_ctxt, GLXContext shareList, int fake_shareList)
{
	process->glstates = realloc(process->glstates, (process->nbGLStates+1)*sizeof(GLState*));
	process->glstates[process->nbGLStates] = malloc(sizeof(GLState));
	memset(process->glstates[process->nbGLStates], 0, sizeof(GLState));
	process->glstates[process->nbGLStates]->ref = 1;
	process->glstates[process->nbGLStates]->context = ctxt;
	process->glstates[process->nbGLStates]->fake_ctxt = fake_ctxt;
	process->glstates[process->nbGLStates]->fake_shareList = fake_shareList;
	init_gl_state(process->glstates[process->nbGLStates]);
	if (shareList && fake_shareList)
	{
		int i;
		for(i=0;i<process->nbGLStates;i++)
		{
			if (process->glstates[i]->fake_ctxt == fake_shareList)
			{
				process->glstates[i]->ref ++;
				process->glstates[process->nbGLStates]->textureAllocator =
					process->glstates[i]->textureAllocator;
				process->glstates[process->nbGLStates]->tabTextures =
					process->glstates[i]->tabTextures;
				process->glstates[process->nbGLStates]->bufferAllocator =
					process->glstates[i]->bufferAllocator;
				process->glstates[process->nbGLStates]->tabBuffers =
					process->glstates[i]->tabBuffers;
				process->glstates[process->nbGLStates]->listAllocator =
					process->glstates[i]->listAllocator;
				process->glstates[process->nbGLStates]->tabLists =
					process->glstates[i]->tabLists;
				break;
			}
		}
	}
	process->nbGLStates++;
}

static const char *opengl_strtok(const char *s, int *n, char **saveptr, char *prevbuf)
{
	char *start;
	char *ret;
	char *p;
	int retlen;
    static const char *delim = " \t\n\r/";

	if (prevbuf)
		free(prevbuf);

    if (s) {
        *saveptr = s;
    } else {
        if (!(*saveptr) || !(*n))
            return NULL;
        s = *saveptr;
    }

    for (; *n && strchr(delim, *s); s++, (*n)--) {
        if (*s == '/' && *n > 1) {
            if (s[1] == '/') {
                do {
                    s++, (*n)--;
                } while (*n > 1 && s[1] != '\n' && s[1] != '\r');
            } else if (s[1] == '*') {
                do {
                    s++, (*n)--;
                } while (*n > 2 && (s[1] != '*' || s[2] != '/'));
                s++, (*n)--;
            }
        }
    }

   	start = s;
    for (; *n && *s && !strchr(delim, *s); s++, (*n)--);
	if (*n > 0) 
		s++, (*n)--;

	*saveptr = s;

	retlen = s - start;
	ret = malloc(retlen + 1);
	p = ret;

	while (retlen > 0) {
        if (*start == '/' && retlen > 1) {
            if (start[1] == '/') {
                do {
                    start++, retlen--;
                } while (retlen > 1 && start[1] != '\n' && start[1] != '\r');
				start++, retlen--;
				continue;
            } else if (start[1] == '*') {
                do {
                    start++, retlen--;
                } while (retlen > 2 && (start[1] != '*' || start[2] != '/'));
                start += 3, retlen -= 3;
				continue;
            }
        }
		*(p++) = *(start++), retlen--;
	}
	
	*p = 0;
	return ret;
}

static char *do_eglShaderPatch(const char *source, int length, int *patched_len)
{
	char *saveptr = NULL;
	char *sp;
	char *p = NULL;

    if (!length) 
        length = strlen(source);
    
    *patched_len = 0;
    int patched_size = length;
    char *patched = malloc(patched_size + 1);

    if (!patched) 
        return NULL;

    p = opengl_strtok(source, &length, &saveptr, NULL);
    for (; p; p = opengl_strtok(0, &length, &saveptr, p)) {
        if (!strncmp(p, "lowp", 4) || !strncmp(p, "mediump", 7) || !strncmp(p, "highp", 5)) {
            continue;
        } else if (!strncmp(p, "precision", 9)) {
            while ((p = opengl_strtok(0, &length, &saveptr, p)) && !strchr(p, ';'));
        } else {
            if (!strncmp(p, "gl_MaxVertexUniformVectors", 26)) {
                p = "(gl_MaxVertexUniformComponents / 4)";
            } else if (!strncmp(p, "gl_MaxFragmentUniformVectors", 28)) {
                p = "(gl_MaxFragmentUniformComponents / 4)";
            } else if (!strncmp(p, "gl_MaxVaryingVectors", 20)) {
                p = "(gl_MaxVaryingFloats / 4)";
            }

            int new_len = strlen(p);
            if (*patched_len + new_len > patched_size) {
                patched_size *= 2;
                patched = realloc(patched, patched_size + 1);

                if (!patched) 
                    return NULL;
            }

            memcpy(patched + *patched_len, p, new_len);
            *patched_len += new_len;
        }     
    }

    patched[*patched_len] = 0;
    /* check that we don't leave dummy preprocessor lines */
    for (sp = patched; *sp;) {
        for (; *sp == ' ' || *sp == '\t'; sp++);
        if (!strncmp(sp, "#define", 7)) {
            for (p = sp + 7; *p == ' ' || *p == '\t'; p++);
            if (*p == '\n' || *p == '\r' || *p == '/') {
                memset(sp, 0x20, 7);
            }
        }
        for (; *sp && *sp != '\n' && *sp != '\r'; sp++);
        for (; *sp == '\n' || *sp == '\r'; sp++);
    }
    return patched;
}

static int 
shadersrc_gles_to_gl(GLsizei count, const char** string, char **s, const GLint* length, GLint *l)
{
	int i;

	for(i = 0; i < count; ++i) {
		GLint len;
		if(length) {
			len = length[i];
			if (len < 0) 
				len = string[i] ? strlen(string[i]) : 0;
		} else
			len = string[i] ? strlen(string[i]) : 0;

		if(string[i]) {
			s[i] = do_eglShaderPatch(string[i], len, &l[i]);
			if(!s[i]) {
				while(i)
					free(s[--i]);

				free(l);
				free(s);
				return -1;
			}
		} else {
			s[i] = NULL;
			l[i] = 0;
		}
	}
	
	return 0;
}

int do_function_call(OGLS_Conn *pConn, int func_number, int pid, long* args, char* ret_string)
{
	char ret_char = 0;
	int ret_int = 0;
	const char* ret_str = NULL;
	int iProcess;
	ProcessStruct* process = NULL;
	ProcessStruct *processTab = (ProcessStruct *) pConn->processTab;

	if (pConn->parent_dpy)
	{
		pConn->Display = pConn->parent_dpy;
	}

	for(iProcess=0;iProcess<MAX_HANDLED_PROCESS;iProcess++)
	{
		ProcessStruct *processTab = (ProcessStruct *) pConn->processTab;
		if (processTab[iProcess].process_id == pid)
		{
			process = &processTab[iProcess];
			break;
		}
		else if (processTab[iProcess].process_id == 0)
		{
			process = &processTab[iProcess];
			memset(process, 0, sizeof(ProcessStruct));
			process->process_id = pid;
			process->internal_num = pConn->last_assigned_internal_num++;
			init_gl_state(&process->default_state);
			process->current_state = &process->default_state;
			break;
		}
	}
	if (process == NULL)
	{
		fprintf(stderr, "Too many processes !\n");
		return 0;
	}
	if (process->internal_num != pConn->last_active_internal_num)
	{
		glXMakeCurrent(pConn->Display, process->current_state->drawable, process->current_state->context);
		pConn->last_active_internal_num = process->internal_num;
	}

	process->instr_counter++;

	if (display_function_call) fprintf(stderr, "[%d]> %s\n", process->instr_counter, tab_opengl_calls_name[func_number]);
	//fprintf(stderr, "[%d]> %s\n", process->instr_counter, tab_opengl_calls_name[func_number]);

	switch (func_number)
	{
		case _init_func:
			{
				*(int*)args[1] = 1;
				break;
			}

		case _synchronize_func:
			{
				ret_int = 1;
				break;
			}

		case _exit_process_func:
			{
				int i;

				for(i=0;i < MAX_ASSOC_SIZE && process->association_fakecontext_glxcontext[i].key != NULL;i++)
				{
					GLXContext ctxt = process->association_fakecontext_glxcontext[i].value;
					//fprintf(stderr, "Destroy context corresponding to fake_context = %ld\n",
					//		(long)process->association_fakecontext_glxcontext[i].key);
					glXDestroyContext(pConn->Display, ctxt);
				}

				GET_EXT_PTR(void, glXDestroyPbuffer, (Display*, GLXPbuffer));
				for(i=0;i < MAX_ASSOC_SIZE && process->association_fakepbuffer_pbuffer[i].key != NULL;i++)
				{
					GLXPbuffer pbuffer = (GLXPbuffer)process->association_fakepbuffer_pbuffer[i].value;
					fprintf(stderr, "Destroy pbuffer corresponding to fake_pbuffer = %ld\n",
							(long)process->association_fakepbuffer_pbuffer[i].key);
					if (!is_gl_vendor_ati(pConn->Display))
						ptr_func_glXDestroyPbuffer(pConn->Display, pbuffer);
				}

				for(i=0;i < MAX_ASSOC_SIZE && process->association_clientdrawable_serverdrawable[i].key != NULL;i++)
				{
					//fprintf(stderr, "Destroy window corresponding to client_drawable = %p\n",
					//		process->association_clientdrawable_serverdrawable[i].key);

					Window win = (Window)process->association_clientdrawable_serverdrawable[i].value;
					XDestroyWindow(pConn->Display, win);

					int loop = 1;
					while (loop) {
						while (XPending(pConn->Display) > 0) {
							XEvent event;
							XNextEvent(pConn->Display, &event);
							switch (event.type) {
								case DestroyNotify:
									{
										if (((XDestroyWindowEvent*)&event)->window == win)
										{
											loop = 0;
										}
										break;
									}
							}
						}
					}
				}

				for(i=0;i<process->nbGLStates;i++)
				{
					destroy_gl_state(process->glstates[i]);
					free(process->glstates[i]);
				}
				destroy_gl_state(&process->default_state);
				free(process->glstates);

				pConn->active_win = 0;

				memmove(&processTab[iProcess], &processTab[iProcess+1], (MAX_HANDLED_PROCESS - 1 - iProcess) * sizeof(ProcessStruct));

				last_process_id = 0;

				break;
			}

		case _changeWindowState_func:
			{
				GLXDrawable client_drawable = args[0];
				if (display_function_call) fprintf(stderr, "client_drawable=%p\n", (void*)client_drawable);

				GLXDrawable drawable = get_association_clientdrawable_serverdrawable(process, client_drawable);
				if (drawable)
				{
					if (args[1] == IsViewable)
					{
						WindowPosStruct pos;
						_get_window_pos(pConn->Display, drawable, &pos);
						if (pos.map_state != args[1])
						{
							XMapWindow(pConn->Display, drawable);

							int loop = 0; //1;
							while (loop) {
								while (XPending(pConn->Display) > 0) {
									XEvent event;
									XNextEvent(pConn->Display, &event);
									switch (event.type) {
										case ConfigureNotify:
											{
												if (((XConfigureEvent*)&event)->window == drawable)
												{
													loop = 0;
												}
												break;
											}
									}
								}
							}
						}
					}
				}

				break;
			}

		case _moveResizeWindow_func:
			{
				int* params = (int*)args[1];
				GLXDrawable client_drawable = args[0];
				if (display_function_call) fprintf(stderr, "client_drawable=%p\n", (void*)client_drawable);

				GLXDrawable drawable = get_association_clientdrawable_serverdrawable(process, client_drawable);
				if (drawable)
				{
					WindowPosStruct pos;
					_get_window_pos(pConn->Display, drawable, &pos);
					if (!(params[0] == pos.x && params[1] == pos.y && params[2] == pos.width && params[3] == pos.height))
					{
						int redim = !(params[2] == pos.width && params[3] == pos.height);
						pConn->active_win_x = params[0];
						pConn->active_win_y = params[1];

						/*
						   fprintf(stderr, "old x=%d y=%d width=%d height=%d\n", pos.x, pos.y, pos.width, pos.height);
						   fprintf(stderr, "new x=%d y=%d width=%d height=%d\n", params[0], params[1], params[2], params[3]);
						 */

						XMoveResizeWindow(pConn->Display, drawable, params[0], params[1], params[2], params[3]);
						_get_window_pos(pConn->Display, drawable, &pos);
						process->currentDrawablePos = pos;
						//if (getenv("FORCE_GL_VIEWPORT"))
						if (redim)
							glViewport(0, 0, params[2], params[3]);
						int loop = 0; //1;
						while (loop) {
							while (XPending(pConn->Display) > 0) {
								XEvent event;
								XNextEvent(pConn->Display, &event);
								switch (event.type) {
									case ConfigureNotify:
										{
											if (((XConfigureEvent*)&event)->window == drawable)
											{
												loop = 0;
											}
											break;
										}
								}
							}
						}
					}
				}
				break;
			}

		case _send_cursor_func:
			{
				int x = args[0];
				int y = args[1];
				int width = args[2];
				int height = args[3];
				int xhot = args[4];
				int yhot = args[5];
				int* pixels = (int*)args[6];
				client_cursor.x = x;
				client_cursor.y = y;
				client_cursor.width = width;
				client_cursor.height = height;
				client_cursor.xhot = xhot;
				client_cursor.yhot = yhot;
				if (pixels)
				{
					client_cursor.pixels = realloc(client_cursor.pixels, client_cursor.width * client_cursor.height * sizeof(int));
					memcpy(client_cursor.pixels, pixels, client_cursor.width * client_cursor.height * sizeof(int));
				}
				//int in_window = (x >= 0 && y >= 0 &&
				//x < process->currentDrawablePos.width &&
				//y < process->currentDrawablePos.height);
				//fprintf(stderr, "cursor at %d %d   (%s)\n", x, y, (in_window) ? "in window" : "not in window");
				break;
			}

		case glXWaitGL_func:
			{
				glXWaitGL();
				ret_int = 0;
				break;
			}

		case glXWaitX_func:
			{
				glXWaitX();
				ret_int = 0;
				break;
			}

		case glXChooseVisual_func:
			{
				ret_int = glXChooseVisualFunc(pConn, (int*)args[2]);
				break;
			}

		case glXQueryExtensionsString_func:
			{
				ret_str = glXQueryExtensionsString(pConn->Display, 0);
				break;
			}

		case glXQueryServerString_func:
			{
				ret_str = glXQueryServerString(pConn->Display, 0, args[2]);
				break;
			}

		case glXGetClientString_func:
			{
				ret_str = glXGetClientString(pConn->Display, args[1]);
				break;
			}

		case glXGetScreenDriver_func:
			{
				GET_EXT_PTR(const char*, glXGetScreenDriver, (Display* , int ));
				ret_str = ptr_func_glXGetScreenDriver(pConn->Display, 0);
				break;
			}

		case glXGetDriverConfig_func:
			{
				GET_EXT_PTR(const char*, glXGetDriverConfig, (const char* ));
				ret_str = ptr_func_glXGetDriverConfig((const char*)args[0]);
				break;
			}

		case glXCreateContext_func:
			{
				int visualid = (int)args[1];
				int fake_shareList = (int)args[2];
				if (1 || display_function_call) fprintf(stderr, "visualid=%d, fake_shareList=%d\n", visualid, fake_shareList);

				GLXContext shareList = get_association_fakecontext_glxcontext(process, (void*)(long)fake_shareList);
				XVisualInfo* vis = get_visual_info_from_visual_id(pConn, visualid);
				GLXContext ctxt;
				if (vis)
				{
					ctxt = glXCreateContext(pConn->Display, vis, shareList, args[3]);
				}
				else
				{
					vis = get_default_visual(pConn->Display);
					int saved_visualid = vis->visualid;
					vis->visualid = (visualid) ? visualid : saved_visualid;
					ctxt = glXCreateContext(pConn->Display, vis, shareList, args[3]);
					vis->visualid = saved_visualid;
				}

				if (ctxt)
				{
					process->next_available_context_number ++;
					int fake_ctxt = process->next_available_context_number;
					set_association_fakecontext_visual(process, (void*)(long)fake_ctxt, vis);
					set_association_fakecontext_glxcontext(process, (void*)(long)fake_ctxt, ctxt);
					ret_int = fake_ctxt;

					_create_context(process, ctxt, fake_ctxt, shareList, fake_shareList);
				}
				else
				{
					ret_int = 0;
				}

				break;
			}


		case glXCreateNewContext_func:
			{
				GET_EXT_PTR(GLXContext, glXCreateNewContext, (Display*, GLXFBConfig, int, GLXContext, int));
				GET_EXT_PTR(XVisualInfo*, glXGetVisualFromFBConfig, (Display*, GLXFBConfig));
				int client_fbconfig = args[1];
				XVisualInfo* vis = NULL;

				ret_int = 0;
				GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);
				if (fbconfig)
				{
					int fake_shareList = args[3];
					GLXContext shareList = get_association_fakecontext_glxcontext(process, (void*)(long)fake_shareList);
					process->next_available_context_number ++;
					int fake_ctxt = process->next_available_context_number;
					GLXContext ctxt = ptr_func_glXCreateNewContext(pConn->Display, fbconfig, args[2], shareList, args[4]);
					set_association_fakecontext_glxcontext(process, (void*)(long)fake_ctxt, ctxt);

					vis = ptr_func_glXGetVisualFromFBConfig(pConn->Display, fbconfig); /* visual info ????*/
					set_association_fakecontext_visual(process, (void *)(long)fake_ctxt, vis);

					ret_int = fake_ctxt;

					_create_context(process, ctxt, fake_ctxt, shareList, fake_shareList);
				}
				break;
			}

		case glXCopyContext_func:
			{
				GLXContext fake_src_ctxt = (GLXContext)args[1];
				GLXContext fake_dst_ctxt = (GLXContext)args[2];
				GLXContext src_ctxt;
				GLXContext dst_ctxt;
				if (display_function_call) fprintf(stderr, "fake_src_ctxt=%p, fake_dst_ctxt=%p\n", fake_src_ctxt, fake_dst_ctxt);

				if ((src_ctxt = get_association_fakecontext_glxcontext(process, fake_src_ctxt)) == NULL)
				{
					fprintf(stderr, "invalid fake_src_ctxt (%p) !\n", fake_src_ctxt);
				}
				else if ((dst_ctxt = get_association_fakecontext_glxcontext(process, fake_dst_ctxt)) == NULL)
				{
					fprintf(stderr, "invalid fake_dst_ctxt (%p) !\n", fake_dst_ctxt);
				}
				else
				{
					glXCopyContext(pConn->Display, src_ctxt, dst_ctxt, args[3]);
				}
				break;
			}

		case glXDestroyContext_func:
			{
				int fake_ctxt = (int)args[1];
				if (display_function_call) fprintf(stderr, "fake_ctxt=%d\n", fake_ctxt);

				GLXContext ctxt = get_association_fakecontext_glxcontext(process, (void*)(long)fake_ctxt);
				if (ctxt == NULL)
				{
					fprintf(stderr, "invalid fake_ctxt (%p) !\n", (void*)(long)fake_ctxt);
				}
				else
				{
					int i;
					for(i=0;i<process->nbGLStates;i++)
					{
						if (process->glstates[i]->fake_ctxt == fake_ctxt)
						{
							if (ctxt == process->current_state->context)
								process->current_state = &process->default_state;

							int fake_shareList = process->glstates[i]->fake_shareList;
							process->glstates[i]->ref --;
							if (process->glstates[i]->ref == 0)
							{
								fprintf(stderr, "destroy_gl_state fake_ctxt = %d\n", process->glstates[i]->fake_ctxt);
								destroy_gl_state(process->glstates[i]);
								free(process->glstates[i]);
								memmove(&process->glstates[i], &process->glstates[i+1], (process->nbGLStates-i-1) * sizeof(GLState*));
								process->nbGLStates--;
							}

							if (fake_shareList)
							{
								for(i=0;i<process->nbGLStates;i++)
								{
									if (process->glstates[i]->fake_ctxt == fake_shareList)
									{
										process->glstates[i]->ref --;
										if (process->glstates[i]->ref == 0)
										{
											fprintf(stderr, "destroy_gl_state fake_ctxt = %d\n", process->glstates[i]->fake_ctxt);
											destroy_gl_state(process->glstates[i]);
											free(process->glstates[i]);
											memmove(&process->glstates[i], &process->glstates[i+1], (process->nbGLStates-i-1) * sizeof(GLState*));
											process->nbGLStates--;
										}
										break;
									}
								}
							}

							glXDestroyContext(pConn->Display, ctxt);
							unset_association_fakecontext_glxcontext(process, (void*)(long)fake_ctxt);

							break;
						}
					}
				}
				break;
			}

		case glXQueryVersion_func:
			{
				ret_int = glXQueryVersion(pConn->Display, (int*)args[1], (int*)args[2]);
				break;
			}

		case glGetString_func:
			{
				ret_str = (char*)glGetString(args[0]);
				break;
			}

		case glXMakeCurrent_func:
			{
				int i;
				GLXDrawable client_drawable = (GLXDrawable)args[1];
				GLXDrawable drawable = 0;
				int fake_ctxt = args[2];
				if (display_function_call) fprintf(stderr, "client_drawable=%p fake_ctx=%d\n", (void*)client_drawable, fake_ctxt);

				if (client_drawable == 0 && fake_ctxt == 0)
				{
					ret_int = glXMakeCurrent(pConn->Display, 0, NULL);
					process->current_state = &process->default_state;
				}
				else if ((drawable = (GLXDrawable)get_association_fakepbuffer_pbuffer(process, (void*)client_drawable)))
				{
					GLXContext ctxt = get_association_fakecontext_glxcontext(process, (void*)(long)fake_ctxt);
					if (ctxt == NULL)
					{
						fprintf(stderr, "invalid fake_ctxt (%d) (*)!\n", fake_ctxt);
						ret_int = 0;
					}
					else
					{
						ret_int = glXMakeCurrent(pConn->Display, drawable, ctxt);
					}
				}
				else
				{
					GLXContext ctxt = get_association_fakecontext_glxcontext(process, (void*)(long)fake_ctxt);
					if (ctxt == NULL)
					{
						fprintf(stderr, "invalid fake_ctxt (%d)!\n", fake_ctxt);
						ret_int = 0;
					}
					else
					{
						drawable = get_association_clientdrawable_serverdrawable(process, client_drawable);
						if (drawable == 0)
						{
							XVisualInfo* vis = (XVisualInfo*)get_association_fakecontext_visual(process, (void*)(long)fake_ctxt);
							if (vis == NULL)
								vis = get_default_visual(pConn->Display);
							/*if (pConn->local_connection)
							  drawable = client_drawable;
							  else*/
							{
								if (/*pConn->local_connection &&*/ client_drawable == RootWindow(pConn->Display, 0))
								{
									drawable = client_drawable;
								}
								else
								{
									drawable = create_window(pConn, (pConn->local_connection) ? (Window)client_drawable : 0, vis, "", 0, 0, 480, 800);/* Default Window 크?綬�Simulator 크???? ?????磯?.*/
								}
								//fprintf(stderr, "creating window\n");
							}
							set_association_clientdrawable_serverdrawable(process, (void*)client_drawable, (void*)drawable);
						}

						ret_int = glXMakeCurrent(pConn->Display, drawable, ctxt);
					}
				}

				if (ret_int)
				{
					for(i=0;i<process->nbGLStates;i++)
					{
						if (process->glstates[i]->fake_ctxt == fake_ctxt)
						{
							process->current_state = process->glstates[i]; /* HACK !!! REMOVE */
							process->current_state->drawable = drawable;
							break;
						}
					}
				}
				break;
			}

		case glXSwapBuffers_func:
			{
				GLXDrawable client_drawable = args[1];
				if (display_function_call) fprintf(stderr, "client_drawable=%p\n", (void*)client_drawable);

				GLXDrawable drawable = get_association_clientdrawable_serverdrawable(process, client_drawable);
				if (drawable == 0)
				{
					fprintf(stderr, "unknown client_drawable (%p) !\n", (void*)client_drawable);
				}
				else
				{
					if (client_cursor.pixels && pConn->local_connection == 0)
					{
						glPushAttrib(GL_ALL_ATTRIB_BITS);
						glPushClientAttrib(GL_ALL_ATTRIB_BITS);

						glMatrixMode(GL_PROJECTION);
						glPushMatrix();
						glLoadIdentity();
						glOrtho(0,process->currentDrawablePos.width,process->currentDrawablePos.height,0,-1,1);
						glMatrixMode(GL_MODELVIEW);
						glPushMatrix();
						glLoadIdentity();
						glPixelZoom(1,-1); 

						glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
						glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
						glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
						glPixelStorei(GL_UNPACK_LSB_FIRST, 0);
						glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
						glShadeModel(GL_SMOOTH);

						glEnable(GL_BLEND);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) ;
						glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

						int i,numUnits;
						glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &numUnits);
						for(i=0;i<numUnits;i++)
						{
							do_glActiveTextureARB(GL_TEXTURE0_ARB + i);
							glDisable(GL_TEXTURE_1D);
							glDisable(GL_TEXTURE_2D);
							glDisable(GL_TEXTURE_3D);
						}
						glDisable(GL_ALPHA_TEST);
						glDisable(GL_DEPTH_TEST);
						glDisable(GL_STENCIL_TEST);
						glDisable(GL_SCISSOR_TEST);
						glDisable(GL_FRAGMENT_PROGRAM_ARB);
						glDisable(GL_VERTEX_PROGRAM_ARB);
						do_glUseProgramObjectARB (0);

						//memset(client_cursor.pixels, 255, client_cursor.width * client_cursor.height * sizeof(int));

						glRasterPos2d(client_cursor.x - client_cursor.xhot,
								client_cursor.y - client_cursor.yhot);
						glDrawPixels(client_cursor.width, client_cursor.height,
								GL_BGRA, GL_UNSIGNED_BYTE,
								client_cursor.pixels);

						glMatrixMode(GL_MODELVIEW);
						glPopMatrix();

						glMatrixMode(GL_PROJECTION);
						glPopMatrix();

						glPopClientAttrib();
						glPopAttrib();
					}

					glXSwapBuffers(pConn->Display, drawable);
				}
				break;
			}

		case glXIsDirect_func:
			{
				GLXContext fake_ctxt = (GLXContext)args[1];
				if (display_function_call) fprintf(stderr, "fake_ctx=%p\n", fake_ctxt);
				GLXContext ctxt = get_association_fakecontext_glxcontext(process, fake_ctxt);
				if (ctxt == NULL)
				{
					fprintf(stderr, "invalid fake_ctxt (%p) !\n", fake_ctxt);
					ret_char = False;
				}
				else
				{
					ret_char = glXIsDirect(pConn->Display, ctxt);
				}
				break;
			}

		case glXGetConfig_func:
			{
				int visualid = args[1];
				XVisualInfo* vis = NULL;
				if (visualid)
					vis = get_visual_info_from_visual_id(pConn, visualid);
				if (vis == NULL)
					vis = get_default_visual(pConn->Display);
				ret_int = glXGetConfig(pConn->Display, vis, args[2], (int*)args[3]);
				break;
			}

		case glXGetConfig_extended_func:
			{
				int visualid = args[1];
				int n = args[2];
				int i;
				XVisualInfo* vis = NULL;
				int* attribs = (int*)args[3];
				int* values = (int*)args[4];
				int* res = (int*)args[5];

				if (visualid)
					vis = get_visual_info_from_visual_id(pConn, visualid);
				if (vis == NULL)
					vis = get_default_visual(pConn->Display);

				for(i=0;i<n;i++)
				{
					res[i] = glXGetConfig(pConn->Display, vis, attribs[i], &values[i]);
				}
				break;
			}

		case glXUseXFont_func:
			{
				/* implementation is client-side only :-) */
				break;
			}


		case glXQueryExtension_func:
			{
				ret_int = glXQueryExtension(pConn->Display, (int*)args[1], (int*)args[2]);
				break;
			}

		case glXChooseFBConfig_func:
			{
				GET_EXT_PTR(GLXFBConfig*, glXChooseFBConfig, (Display*, int, int*, int*));
				if (process->nfbconfig == MAX_FBCONFIG)
				{
					*(int*)args[3] = 0;
					ret_int = 0;
				}
				else
				{
					GLXFBConfig* fbconfigs = ptr_func_glXChooseFBConfig(pConn->Display, args[1], (int*)args[2], (int*)args[3]);
					if (fbconfigs)
					{
						process->fbconfigs[process->nfbconfig] = fbconfigs;
						process->fbconfigs_max[process->nfbconfig] = *(int*)args[3];
						process->nfbconfig++;
						ret_int = 1 + process->nfbconfig_total;
						process->nfbconfig_total += process->fbconfigs_max[process->nfbconfig];
					}
					else
					{
						ret_int = 0;
					}
				}
				break;
			}

		case glXChooseFBConfigSGIX_func:
			{
				GET_EXT_PTR(GLXFBConfigSGIX*, glXChooseFBConfigSGIX, (Display*, int, int*, int*));
				if (process->nfbconfig == MAX_FBCONFIG)
				{
					*(int*)args[3] = 0;
					ret_int = 0;
				}
				else
				{
					GLXFBConfigSGIX* fbconfigs = ptr_func_glXChooseFBConfigSGIX(pConn->Display, args[1], (int*)args[2], (int*)args[3]);
					if (fbconfigs)
					{
						process->fbconfigs[process->nfbconfig] = fbconfigs;
						process->fbconfigs_max[process->nfbconfig] = *(int*)args[3];
						process->nfbconfig++;
						ret_int = 1 + process->nfbconfig_total;
						process->nfbconfig_total += process->fbconfigs_max[process->nfbconfig];
					}
					else
					{
						ret_int = 0;
					}
				}
				break;
			}

		case glXGetFBConfigs_func:
			{
				GET_EXT_PTR(GLXFBConfig*, glXGetFBConfigs, (Display*, int, int*));
				if (process->nfbconfig == MAX_FBCONFIG)
				{
					*(int*)args[2] = 0;
					ret_int = 0;
				}
				else
				{
					GLXFBConfig* fbconfigs = ptr_func_glXGetFBConfigs(pConn->Display, args[1], (int*)args[2]);
					if (fbconfigs)
					{
						process->fbconfigs[process->nfbconfig] = fbconfigs;
						process->fbconfigs_max[process->nfbconfig] = *(int*)args[2];
						process->nfbconfig++;
						ret_int = 1 + process->nfbconfig_total;
						process->nfbconfig_total += process->fbconfigs_max[process->nfbconfig];
					}
					else
					{
						ret_int = 0;
					}
				}
				break;
			}

		case glXDestroyWindow_func:
			{
				GLXDrawable client_drawable = args[1];
				if (display_function_call) fprintf(stderr, "client_drawable=%p\n", (void*)client_drawable);

				GLXDrawable drawable = get_association_clientdrawable_serverdrawable(process, client_drawable);

				destroy_window(pConn, (Window)drawable);

				int i;
				for(i=0;process->association_clientdrawable_serverdrawable[i].key != NULL;i++)
				{
					if (process->association_clientdrawable_serverdrawable[i].key == (void *) client_drawable)
					{
						process->association_clientdrawable_serverdrawable[i].key = NULL;
						process->association_clientdrawable_serverdrawable[i].value = NULL;
					}
				}

				for(i=0;i<process->nbGLStates;i++)
				{
					if (process->glstates[i]->drawable == drawable)
					{
						process->glstates[i]->drawable = 0;
					}
				}

				if( process->current_state->drawable == drawable )
					process->current_state = &process->default_state;

				break;
			}


		case glXCreatePbuffer_func:
			{
				GET_EXT_PTR(GLXPbuffer, glXCreatePbuffer, (Display*, GLXFBConfig, int*));
				int client_fbconfig = args[1];
				ret_int = 0;
				GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);
				if (fbconfig)
				{
					GLXPbuffer pbuffer = ptr_func_glXCreatePbuffer(pConn->Display, fbconfig, (int*)args[2]);
					fprintf(stderr, "glXCreatePbuffer --> %X\n", (int)pbuffer);
					if (pbuffer)
					{
						process->next_available_pbuffer_number ++;
						int fake_pbuffer = process->next_available_pbuffer_number;
						set_association_fakepbuffer_pbuffer(process, (void*)(long)fake_pbuffer, (void*)pbuffer);
						fprintf(stderr, "set_association_fakepbuffer_pbuffer(%d, %X)\n", fake_pbuffer, (int)pbuffer);
						ret_int = fake_pbuffer;
					}
				}
				break;
			}

		case glXCreateGLXPbufferSGIX_func:
			{
				GET_EXT_PTR(GLXPbufferSGIX, glXCreateGLXPbufferSGIX, (Display*, GLXFBConfig, int, int, int*));
				int client_fbconfig = args[1];
				ret_int = 0;
				GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);
				if (fbconfig)
				{
					GLXPbufferSGIX pbuffer = ptr_func_glXCreateGLXPbufferSGIX(pConn->Display, fbconfig, args[2], args[3], (int*)args[4]);
					if (pbuffer)
					{
						process->next_available_pbuffer_number ++;
						int fake_pbuffer = process->next_available_pbuffer_number;
						set_association_fakepbuffer_pbuffer(process, (void*)(long)fake_pbuffer, (void*)pbuffer);
						ret_int = fake_pbuffer;
					}
				}
				break;
			}

		case glXDestroyPbuffer_func:
			{
				GET_EXT_PTR(void, glXDestroyPbuffer, (Display*, GLXPbuffer));
				GLXPbuffer fake_pbuffer = (GLXPbuffer)args[1];
				if (display_function_call) fprintf(stderr, "fake_pbuffer=%d\n", (int)fake_pbuffer);

				GLXPbuffer pbuffer = (GLXPbuffer)get_association_fakepbuffer_pbuffer(process, (void*)fake_pbuffer);
				if (pbuffer == 0)
				{
					fprintf(stderr, "invalid fake_pbuffer (%d) !\n", (int)fake_pbuffer);
				}
				else
				{
					if (!is_gl_vendor_ati(pConn->Display))
						ptr_func_glXDestroyPbuffer(pConn->Display, pbuffer);
					unset_association_fakepbuffer_pbuffer(process, (void*)fake_pbuffer);
				}
				break;
			}

		case glXDestroyGLXPbufferSGIX_func:
			{
				GET_EXT_PTR(void, glXDestroyGLXPbufferSGIX, (Display*, GLXPbuffer));
				GLXPbuffer fake_pbuffer = (GLXPbuffer)args[1];
				if (display_function_call) fprintf(stderr, "fake_pbuffer=%d\n", (int)fake_pbuffer);

				GLXPbuffer pbuffer = (GLXPbuffer)get_association_fakepbuffer_pbuffer(process, (void*)fake_pbuffer);
				if (pbuffer == 0)
				{
					fprintf(stderr, "invalid fake_pbuffer (%d) !\n", (int)fake_pbuffer);
				}
				else
				{
					if (!is_gl_vendor_ati(pConn->Display))
						ptr_func_glXDestroyGLXPbufferSGIX(pConn->Display, pbuffer);
					unset_association_fakepbuffer_pbuffer(process, (void*)fake_pbuffer);
				}
				break;
			}

		case glXBindTexImageATI_func:
			{
				GET_EXT_PTR(void, glXBindTexImageATI, (Display*, GLXPbuffer, int));
				GLXPbuffer fake_pbuffer = (GLXPbuffer)args[1];
				if (display_function_call) fprintf(stderr, "fake_pbuffer=%d\n", (int)fake_pbuffer);

				GLXPbuffer pbuffer = (GLXPbuffer)get_association_fakepbuffer_pbuffer(process, (void*)fake_pbuffer);
				if (pbuffer == 0)
				{
					fprintf(stderr, "glXBindTexImageATI : invalid fake_pbuffer (%d) !\n", (int)fake_pbuffer);
				}
				else
				{
					ptr_func_glXBindTexImageATI(pConn->Display, pbuffer, args[2]);
				}
				break;
			}

		case glXReleaseTexImageATI_func:
			{
				GET_EXT_PTR(void, glXReleaseTexImageATI, (Display*, GLXPbuffer, int));
				GLXPbuffer fake_pbuffer = (GLXPbuffer)args[1];
				if (display_function_call) fprintf(stderr, "fake_pbuffer=%d\n", (int)fake_pbuffer);

				GLXPbuffer pbuffer = (GLXPbuffer)get_association_fakepbuffer_pbuffer(process, (void*)fake_pbuffer);
				if (pbuffer == 0)
				{
					fprintf(stderr, "glXReleaseTexImageATI : invalid fake_pbuffer (%d) !\n", (int)fake_pbuffer);
				}
				else
				{
					ptr_func_glXReleaseTexImageATI(pConn->Display, pbuffer, args[2]);
				}
				break;
			}

		case glXBindTexImageARB_func:
			{
				GET_EXT_PTR(Bool, glXBindTexImageARB, (Display*, GLXPbuffer, int));
				GLXPbuffer fake_pbuffer = (GLXPbuffer)args[1];
				if (display_function_call) fprintf(stderr, "fake_pbuffer=%d\n", (int)fake_pbuffer);

				GLXPbuffer pbuffer = (GLXPbuffer)get_association_fakepbuffer_pbuffer(process, (void*)fake_pbuffer);
				if (pbuffer == 0)
				{
					fprintf(stderr, "glXBindTexImageARB : invalid fake_pbuffer (%d) !\n", (int)fake_pbuffer);
					ret_int = 0;
				}
				else
				{
					ret_int = ptr_func_glXBindTexImageARB(pConn->Display, pbuffer, args[2]);
				}
				break;
			}

		case glXReleaseTexImageARB_func:
			{
				GET_EXT_PTR(Bool, glXReleaseTexImageARB, (Display*, GLXPbuffer, int));
				GLXPbuffer fake_pbuffer = (GLXPbuffer)args[1];
				if (display_function_call) fprintf(stderr, "fake_pbuffer=%d\n", (int)fake_pbuffer);

				GLXPbuffer pbuffer = (GLXPbuffer)get_association_fakepbuffer_pbuffer(process, (void*)fake_pbuffer);
				if (pbuffer == 0)
				{
					fprintf(stderr, "glXReleaseTexImageARB : invalid fake_pbuffer (%d) !\n", (int)fake_pbuffer);
					ret_int = 0;
				}
				else
				{
					ret_int = ptr_func_glXReleaseTexImageARB(pConn->Display, pbuffer, args[2]);
				}
				break;
			}

		case glXGetFBConfigAttrib_func:
			{
				GET_EXT_PTR(int, glXGetFBConfigAttrib, (Display*, GLXFBConfig, int, int*));
				int client_fbconfig = args[1];
				ret_int = 0;
				GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);
				if (fbconfig)
					ret_int = ptr_func_glXGetFBConfigAttrib(pConn->Display, fbconfig, args[2], (int*)args[3]);
				break;
			}

		case glXGetFBConfigAttrib_extended_func:
			{
				GET_EXT_PTR(int, glXGetFBConfigAttrib, (Display*, GLXFBConfig, int, int*));
				int client_fbconfig = args[1];
				int n = args[2];
				int i;
				int* attribs = (int*)args[3];
				int* values = (int*)args[4];
				int* res = (int*)args[5];
				GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);

				for(i=0;i<n;i++)
				{
					if (fbconfig)
					{
						res[i] = ptr_func_glXGetFBConfigAttrib(pConn->Display, fbconfig, attribs[i], &values[i]);
					}
					else
					{
						res[i] = 0;
					}
				}
				break;
			}

		case glXGetFBConfigAttribSGIX_func:
			{
				GET_EXT_PTR(int, glXGetFBConfigAttribSGIX, (Display*, GLXFBConfigSGIX, int, int*));
				int client_fbconfig = args[1];
				ret_int = 0;
				GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);
				if (fbconfig)
					ret_int = ptr_func_glXGetFBConfigAttribSGIX(pConn->Display, (GLXFBConfigSGIX)fbconfig, args[2], (int*)args[3]);
				break;
			}

		case glXQueryContext_func:
			{
				GET_EXT_PTR(int, glXQueryContext, (Display*, GLXContext, int, int*));
				GLXContext fake_ctxt = (GLXContext)args[1];
				if (display_function_call) fprintf(stderr, "fake_ctx=%p\n", fake_ctxt);
				GLXContext ctxt = get_association_fakecontext_glxcontext(process, fake_ctxt);
				if (ctxt == NULL)
				{
					fprintf(stderr, "invalid fake_ctxt (%p) !\n", fake_ctxt);
					ret_int = 0;
				}
				else
				{
					ret_int = ptr_func_glXQueryContext(pConn->Display, ctxt, args[2], (int*)args[3]);
				}
				break;
			}

		case glXQueryDrawable_func:
			{
				GET_EXT_PTR(void, glXQueryDrawable, (Display*, GLXDrawable, int, int*));
				GLXDrawable client_drawable = (GLXDrawable)args[1];
				GLXDrawable drawable = get_association_clientdrawable_serverdrawable(process, client_drawable);
				if (display_function_call) fprintf(stderr, "client_drawable=%d\n", (int)client_drawable);
				if (drawable == 0)
				{
					fprintf(stderr, "invalid client_drawable (%d) !\n", (int)client_drawable);
				}
				else
				{
					ptr_func_glXQueryDrawable(pConn->Display, drawable, args[2], (int*)args[3]);
				}
				break;
			}

		case glXQueryGLXPbufferSGIX_func:
			{
				GET_EXT_PTR(int, glXQueryGLXPbufferSGIX, (Display*, GLXFBConfigSGIX, int, int*));
				int client_fbconfig = args[1];
				ret_int = 0;
				GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);
				if (fbconfig)
					ret_int = ptr_func_glXQueryGLXPbufferSGIX(pConn->Display, (GLXFBConfigSGIX)fbconfig, args[2], (int*)args[3]);
				break;
			}

		case glXCreateContextWithConfigSGIX_func:
			{
				GET_EXT_PTR(GLXContext, glXCreateContextWithConfigSGIX, (Display*, GLXFBConfigSGIX, int, GLXContext, int));
				int client_fbconfig = args[1];
				ret_int = 0;
				GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);
				if (fbconfig)
				{
					GLXContext shareList = get_association_fakecontext_glxcontext(process, (void*)args[3]);
					process->next_available_context_number ++;
					int fake_ctxt = process->next_available_context_number;
					GLXContext ctxt = ptr_func_glXCreateContextWithConfigSGIX(pConn->Display, (GLXFBConfigSGIX)fbconfig, args[2], shareList, args[4]);
					set_association_fakecontext_glxcontext(process, (void*)(long)fake_ctxt, ctxt);
					ret_int = fake_ctxt;
				}
				break;
			}

		case glXGetVisualFromFBConfig_func:
			{
				GET_EXT_PTR(XVisualInfo*, glXGetVisualFromFBConfig, (Display*, GLXFBConfig));
				int client_fbconfig = args[1];
				ret_int = 0;
				GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);
				if (fbconfig)
				{
					AssocAttribListVisual *tabAssocAttribListVisual = (AssocAttribListVisual *)pConn->tabAssocAttribListVisual ;
					XVisualInfo* vis = ptr_func_glXGetVisualFromFBConfig(pConn->Display, fbconfig);
					ret_int = (vis) ? vis->visualid : 0;
					if (vis)
					{
						pConn->tabAssocAttribListVisual = tabAssocAttribListVisual =
							realloc(tabAssocAttribListVisual, sizeof(AssocAttribListVisual) * (pConn->nTabAssocAttribListVisual+1));
						tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].attribListLength = 0;
						tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].attribList = NULL;
						tabAssocAttribListVisual[pConn->nTabAssocAttribListVisual].visInfo = vis;
						pConn->nTabAssocAttribListVisual++;
						pConn->tabAssocAttribListVisual = tabAssocAttribListVisual;
					}
					if (display_function_call) fprintf(stderr, "visualid = %d\n", ret_int);
				}
				break;
			}

		case glXSwapIntervalSGI_func:
			{
				GET_EXT_PTR(int, glXSwapIntervalSGI, (int));
				ret_int = ptr_func_glXSwapIntervalSGI(args[0]);
				break;
			}

		case glXGetProcAddress_fake_func:
			{
				if (display_function_call) fprintf(stderr, "%s\n", (char*)args[0]);
				ret_int = glXGetProcAddressARB((const GLubyte*)args[0]) != NULL;
				break;
			}

		case glXGetProcAddress_global_fake_func:
			{
				int nbElts = args[0];
				char* huge_buffer = (char*)args[1];
				char* result = (char*)args[2];
				int i;

				for(i=0;i<nbElts;i++)
				{
					int len = strlen(huge_buffer);
					result[i] = glXGetProcAddressARB((const GLubyte*)huge_buffer) != NULL;
					huge_buffer += len + 1;
				}
				break;
			}

			/* Begin of texture stuff */
		case glBindTexture_func:
		case glBindTextureEXT_func:
			{
				int target = args[0];
				unsigned int client_texture = args[1];
				unsigned int server_texture;

				if (client_texture == 0)
				{
					glBindTexture(target, 0);
				}
				else
				{
					alloc_value(process->current_state->textureAllocator, client_texture);
					server_texture = process->current_state->tabTextures[client_texture];
					if (server_texture == 0)
					{
						glGenTextures(1, &server_texture);
						process->current_state->tabTextures[client_texture] = server_texture;
					}
					glBindTexture(target, server_texture);
				}
				break;
			}

		case glGenTextures_fake_func:
			{
				GET_EXT_PTR(void, glGenTextures, (GLsizei n, GLuint *textures));
				int i;
				int n = args[0];
				unsigned int* clientTabTextures = malloc(n * sizeof(int));
				unsigned int* serverTabTextures = malloc(n * sizeof(int));

				alloc_range(process->current_state->textureAllocator, n, clientTabTextures);

				ptr_func_glGenTextures(n, serverTabTextures);
				for(i=0;i<n;i++)
				{
					process->current_state->tabTextures[clientTabTextures[i]] = serverTabTextures[i];
				}

				free(clientTabTextures);
				free(serverTabTextures);
				break;
			}


		case glDeleteTextures_func:
			{
				GET_EXT_PTR(void, glDeleteTextures, (GLsizei n, const GLuint *textures));
				int i;
				int n = args[0];
				unsigned int* clientTabTextures = (unsigned int*)args[1];

				delete_range(process->current_state->textureAllocator, n, clientTabTextures);

				unsigned int* serverTabTextures = malloc(n * sizeof(int));
				for(i=0;i<n;i++)
				{
					serverTabTextures[i] = get_server_texture(process, clientTabTextures[i]);
				}
				ptr_func_glDeleteTextures(n, serverTabTextures);
				for(i=0;i<n;i++)
				{
					process->current_state->tabTextures[clientTabTextures[i]] = 0;
				}
				free(serverTabTextures);
				break;
			}

		case glPrioritizeTextures_func:
			{
				GET_EXT_PTR(void, glPrioritizeTextures, (GLsizei n, const GLuint *textures, const GLclampf *priorities));

				int i;
				int n = args[0];
				unsigned int* textures = (unsigned int*)args[1];
				for(i=0;i<n;i++)
				{
					textures[i] = get_server_texture(process, textures[i]);
				}
				ptr_func_glPrioritizeTextures(n, textures, (const GLclampf*)args[2]);
				break;
			}

		case glAreTexturesResident_func:
			{
				GET_EXT_PTR(void, glAreTexturesResident, (GLsizei n, const GLuint *textures, GLboolean *residences));
				int i;
				int n = args[0];
				unsigned int* textures = (unsigned int*)args[1];
				for(i=0;i<n;i++)
				{
					textures[i] = get_server_texture(process, textures[i]);
				}
				ptr_func_glAreTexturesResident(n, textures, (GLboolean*)args[2]);
				break;
			}

		case glIsTexture_func:
		case glIsTextureEXT_func:
			{
				GET_EXT_PTR(GLboolean, glIsTexture, (GLuint texture ));
				unsigned int client_texture = args[0];
				unsigned int server_texture = get_server_texture(process, client_texture);
				if (server_texture)
					ret_char = ptr_func_glIsTexture(server_texture);
				else
					ret_char = 0;
				break;
			}

		case glFramebufferTexture1DEXT_func:
			{
				GET_EXT_PTR(void, glFramebufferTexture1DEXT, (int, int, int, int, int));
				unsigned int client_texture = args[3];
				unsigned int server_texture = get_server_texture(process, client_texture);
				if (server_texture)
					ptr_func_glFramebufferTexture1DEXT(args[0], args[1], args[2], server_texture, args[4]);
				break;
			}

		case glFramebufferTexture2DEXT_func:
			{
				GET_EXT_PTR(void, glFramebufferTexture2DEXT, (int, int, int, int, int));
				unsigned int client_texture = args[3];
				unsigned int server_texture = get_server_texture(process, client_texture);
				if (server_texture)
					ptr_func_glFramebufferTexture2DEXT(args[0], args[1], args[2], server_texture, args[4]);
				break;
			}

		case glFramebufferTexture3DEXT_func:
			{
				GET_EXT_PTR(void, glFramebufferTexture3DEXT, (int, int, int, int, int, int));
				unsigned int client_texture = args[3];
				unsigned int server_texture = get_server_texture(process, client_texture);
				if (server_texture)
					ptr_func_glFramebufferTexture3DEXT(args[0], args[1], args[2], server_texture, args[4], args[5]);
				break;
			}
			/* End of texture stuff */

			/* Begin of list stuff */ 
		case glIsList_func:
			{
				unsigned int client_list = args[0];
				unsigned int server_list = get_server_list(process, client_list);
				if (server_list)
					ret_char = glIsList(server_list);
				else
					ret_char = 0;
				break;
			}

		case glDeleteLists_func:
			{
				int i;
				unsigned int first_client = args[0];
				int n = args[1];

				unsigned int first_server = get_server_list(process, first_client);
				for(i=0;i<n;i++)
				{
					if (get_server_list(process, first_client + i) != first_server + i)
						break;
				}
				if (i == n)
				{
					glDeleteLists(first_server, n);
				}
				else
				{
					for(i=0;i<n;i++)
					{
						glDeleteLists(get_server_list(process, first_client + i), 1);
					}
				}

				for(i=0;i<n;i++)
				{
					process->current_state->tabLists[first_client + i] = 0;
				}
				delete_consecutive_values(process->current_state->listAllocator, first_client, n);
				break;
			}

		case glGenLists_fake_func:
			{
				int i;
				int n = args[0];
				unsigned int server_first = glGenLists(n);
				if (server_first)
				{
					unsigned int client_first = alloc_range(process->current_state->listAllocator, n, NULL);
					for(i=0;i<n;i++)
					{
						process->current_state->tabLists[client_first + i] = server_first + i;
					}
				}
				break;
			}

		case glNewList_func:
			{
				unsigned int client_list = args[0];
				int mode = args[1];
				alloc_value(process->current_state->listAllocator, client_list);
				unsigned int server_list = get_server_list(process, client_list);
				if (server_list == 0)
				{
					server_list = glGenLists(1);
					process->current_state->tabLists[client_list] = server_list;
				}
				glNewList(server_list, mode);
				break;
			}

		case glCallList_func:
			{
				unsigned int client_list = args[0];
				unsigned int server_list = get_server_list(process, client_list);
				glCallList(server_list);
				break;
			}

		case glCallLists_func:
			{
				int i;
				int n = args[0];
				int type = args[1];
				const GLvoid* lists = (const GLvoid*)args[2];
				int* new_lists = malloc(sizeof(int) * n);
				for(i=0;i<n;i++)
				{
					new_lists[i] = get_server_list(process, translate_id(i, type, lists));
				}
				glCallLists(n, GL_UNSIGNED_INT, new_lists);
				free(new_lists);
				break;
			}


			/* End of list stuff */

			/* Begin of buffer stuff */
		case glBindBufferARB_func:
			{
				GET_EXT_PTR(void, glBindBufferARB, (int,int));
				int target = args[0];
				unsigned int client_buffer = args[1];
				unsigned int server_buffer;

				if (client_buffer == 0)
				{
					ptr_func_glBindBufferARB(target, 0);
				}
				else
				{
					server_buffer = get_server_buffer(process, client_buffer);
					ptr_func_glBindBufferARB(target, server_buffer);
				}
				break;
			}

		case glGenBuffersARB_fake_func:
			{
				GET_EXT_PTR(void, glGenBuffersARB, (int,unsigned int*));
				int i;
				int n = args[0];
				unsigned int* clientTabBuffers = malloc(n * sizeof(int));
				unsigned int* serverTabBuffers = malloc(n * sizeof(int));

				alloc_range(process->current_state->bufferAllocator, n, clientTabBuffers);

				ptr_func_glGenBuffersARB(n, serverTabBuffers);
				for(i=0;i<n;i++)
				{
					process->current_state->tabBuffers[clientTabBuffers[i]] = serverTabBuffers[i];
				}

				free(clientTabBuffers);
				free(serverTabBuffers);
				break;
			}


		case glDeleteBuffersARB_func:
			{
				GET_EXT_PTR(void, glDeleteBuffersARB, (int,int*));
				int i;
				int n = args[0];
				unsigned int* clientTabBuffers = (unsigned int*)args[1];

				delete_range(process->current_state->bufferAllocator, n, clientTabBuffers);

				int* serverTabBuffers = malloc(n * sizeof(int));
				for(i=0;i<n;i++)
				{
					serverTabBuffers[i] = get_server_buffer(process, clientTabBuffers[i]);
				}
				ptr_func_glDeleteBuffersARB(n, serverTabBuffers);
				for(i=0;i<n;i++)
				{
					process->current_state->tabBuffers[clientTabBuffers[i]] = 0;
				}
				free(serverTabBuffers);
				break;
			}

		case glIsBufferARB_func:
			{
				GET_EXT_PTR(int, glIsBufferARB, (int));
				unsigned int client_buffer = args[0];
				unsigned int server_buffer = get_server_buffer(process, client_buffer);
				if (server_buffer)
					ret_int = ptr_func_glIsBufferARB(server_buffer);
				else
					ret_int = 0;
				break;
			}

			/* Endo of buffer stuff */

		case glShaderSourceARB_fake_func:
			{
				GET_EXT_PTR(void, glShaderSourceARB, (int,int,char**,void*));
				int size = args[1];
				int i;
				int acc_length = 0;
				GLcharARB** tab_prog = malloc(size * sizeof(GLcharARB*));
				int* tab_length = (int*)args[3];
				for(i=0;i<size;i++)
				{
					tab_prog[i] = ((GLcharARB*)args[2]) + acc_length;
					acc_length += tab_length[i];
				}
				ptr_func_glShaderSourceARB(args[0], args[1], tab_prog, tab_length);
				free(tab_prog);
				break;
			}

		case glShaderSource_fake_func:
			{
				GET_EXT_PTR(void, glShaderSource, (int,int,char**,void*));
				int size = args[1];
				int i;
				int acc_length = 0;
				GLcharARB** tab_prog = malloc(size * sizeof(GLcharARB*));
				int* tab_length = (int*)args[3];

				char **tab_prog_new;
				GLint *tab_length_new;

			   tab_prog_new = malloc(args[1]* sizeof(char*));
			   tab_length_new = malloc(args[1]* sizeof(GLint));

			   memset(tab_prog_new, 0, args[1] * sizeof(char*));
			   memset(tab_length_new, 0, args[1] * sizeof(GLint));


				for(i=0;i<size;i++)
				{
					tab_prog[i] = ((GLcharARB*)args[2]) + acc_length;
					acc_length += tab_length[i];
				}
				
				shadersrc_gles_to_gl(args[1], tab_prog, tab_prog_new, tab_length, tab_length_new);
				
				if (!tab_prog_new || !tab_length_new)
					break;

				ptr_func_glShaderSource(args[0], args[1], tab_prog_new, tab_length_new);

				for (i = 0; i < args[1]; i++)
					free(tab_prog_new[i]);
				free(tab_prog_new);
				free(tab_length_new);

				free(tab_prog);
				break;
			}

		case glVertexPointer_fake_func:
			{
				int offset = args[0];
				int size = args[1];
				int type = args[2];
				int stride = args[3];
				int bytes_size = args[4];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, (void*)args[5], bytes_size);
				/*fprintf(stderr, "glVertexPointer_fake_func size=%d, type=%d, stride=%d, byte_size=%d\n",
				  size, type, stride, bytes_size);*/
				glVertexPointer(size, type, stride, process->current_state->vertexPointer);
				break;
			}

		case glNormalPointer_fake_func:
			{
				int offset = args[0];
				int type = args[1];
				int stride = args[2];
				int bytes_size = args[3];
				process->current_state->normalPointerSize = MAX(process->current_state->normalPointerSize, offset + bytes_size);
				process->current_state->normalPointer = realloc(process->current_state->normalPointer, process->current_state->normalPointerSize);
				memcpy(process->current_state->normalPointer + offset, (void*)args[4], bytes_size);
				//fprintf(stderr, "glNormalPointer_fake_func type=%d, stride=%d, byte_size=%d\n", type, stride, bytes_size);
				glNormalPointer(type, stride, process->current_state->normalPointer);
				break;
			}

		case glIndexPointer_fake_func:
			{
				int offset = args[0];
				int type = args[1];
				int stride = args[2];
				int bytes_size = args[3];
				process->current_state->indexPointerSize = MAX(process->current_state->indexPointerSize, offset + bytes_size);
				process->current_state->indexPointer = realloc(process->current_state->indexPointer, process->current_state->indexPointerSize);
				memcpy(process->current_state->indexPointer + offset, (void*)args[4], bytes_size);
				//fprintf(stderr, "glIndexPointer_fake_func type=%d, stride=%d, byte_size=%d\n", type, stride, bytes_size);
				glIndexPointer(type, stride, process->current_state->indexPointer);
				break;
			}

		case glEdgeFlagPointer_fake_func:
			{
				int offset = args[0];
				int stride = args[1];
				int bytes_size = args[2];
				process->current_state->edgeFlagPointerSize = MAX(process->current_state->edgeFlagPointerSize, offset + bytes_size);
				process->current_state->edgeFlagPointer = realloc(process->current_state->edgeFlagPointer, process->current_state->edgeFlagPointerSize);
				memcpy(process->current_state->edgeFlagPointer + offset, (void*)args[3], bytes_size );
				//fprintf(stderr, "glEdgeFlagPointer_fake_func stride = %d, bytes_size=%d\n", stride, bytes_size);
				glEdgeFlagPointer(stride, process->current_state->edgeFlagPointer);
				break;
			}

		case glVertexAttribPointerARB_fake_func:
			{
				GET_EXT_PTR(void, glVertexAttribPointerARB, (int,int,int,int,int,void*));
				int offset = args[0];
				int index = args[1];
				int size = args[2];
				int type = args[3];
				int normalized = args[4];
				int stride = args[5];
				int bytes_size = args[6];
				process->current_state->vertexAttribPointerSize[index] = MAX(process->current_state->vertexAttribPointerSize[index], offset + bytes_size);
				process->current_state->vertexAttribPointer[index] = realloc(process->current_state->vertexAttribPointer[index],
						process->current_state->vertexAttribPointerSize[index]);
				memcpy(process->current_state->vertexAttribPointer[index] + offset, (void*)args[7], bytes_size);
				ptr_func_glVertexAttribPointerARB(index, size, type, normalized, stride,
						process->current_state->vertexAttribPointer[index]);
				break;
			}

		case glVertexAttribPointerNV_fake_func:
			{
				GET_EXT_PTR(void, glVertexAttribPointerNV, (int,int,int,int,void*));
				int offset = args[0];
				int index = args[1];
				int size = args[2];
				int type = args[3];
				int stride = args[4];
				int bytes_size = args[5];
				process->current_state->vertexAttribPointerNVSize[index] = MAX(process->current_state->vertexAttribPointerNVSize[index], offset + bytes_size);
				process->current_state->vertexAttribPointerNV[index] = realloc(process->current_state->vertexAttribPointerNV[index],
						process->current_state->vertexAttribPointerNVSize[index]);
				memcpy(process->current_state->vertexAttribPointerNV[index] + offset, (void*)args[6], bytes_size);
				ptr_func_glVertexAttribPointerNV(index, size, type, stride,
						process->current_state->vertexAttribPointerNV[index]);
				break;
			}

		case glColorPointer_fake_func:
			{
				int offset = args[0];
				int size = args[1];
				int type = args[2];
				int stride = args[3];
				int bytes_size = args[4];
				process->current_state->colorPointerSize = MAX(process->current_state->colorPointerSize, offset + bytes_size);
				process->current_state->colorPointer = realloc(process->current_state->colorPointer, process->current_state->colorPointerSize);
				memcpy(process->current_state->colorPointer + offset, (void*)args[5], bytes_size);
				//fprintf(stderr, "glColorPointer_fake_func bytes_size = %d\n", bytes_size);
				glColorPointer(size, type, stride, process->current_state->colorPointer);

				break;
			}

		case glSecondaryColorPointer_fake_func:
			{
				GET_EXT_PTR(void, glSecondaryColorPointer, (int,int,int,void*));
				int offset = args[0];
				int size = args[1];
				int type = args[2];
				int stride = args[3];
				int bytes_size = args[4];
				process->current_state->secondaryColorPointerSize = MAX(process->current_state->secondaryColorPointerSize, offset + bytes_size);
				process->current_state->secondaryColorPointer = realloc(process->current_state->secondaryColorPointer, process->current_state->secondaryColorPointerSize);
				memcpy(process->current_state->secondaryColorPointer + offset, (void*)args[5], bytes_size);
				//fprintf(stderr, "glSecondaryColorPointer_fake_func bytes_size = %d\n", bytes_size);
				ptr_func_glSecondaryColorPointer(size, type, stride, process->current_state->secondaryColorPointer);

				break;
			}

		case glPushClientAttrib_func:
			{
				int mask = args[0];
				if (process->current_state->clientStateSp < MAX_CLIENT_STATE_STACK_SIZE)
				{
					process->current_state->clientStateStack[process->current_state->clientStateSp].mask = mask;
					if (mask & GL_CLIENT_VERTEX_ARRAY_BIT)
					{
						process->current_state->clientStateStack[process->current_state->clientStateSp].activeTextureIndex =
							process->current_state->activeTextureIndex;
					}
					process->current_state->clientStateSp++;
				}
				glPushClientAttrib(mask);
				break;
			}

		case glPopClientAttrib_func:
			{
				if (process->current_state->clientStateSp > 0)
				{
					process->current_state->clientStateSp--;
					if (process->current_state->clientStateStack[process->current_state->clientStateSp].mask & GL_CLIENT_VERTEX_ARRAY_BIT)
					{
						process->current_state->activeTextureIndex =
							process->current_state->clientStateStack[process->current_state->clientStateSp].activeTextureIndex;
					}
				}
				glPopClientAttrib();
				break;
			}

		case glClientActiveTexture_func:
		case glClientActiveTextureARB_func:
			{
				int activeTexture = args[0];
				process->current_state->activeTextureIndex = activeTexture - GL_TEXTURE0_ARB;
				do_glClientActiveTextureARB(activeTexture);
				break;
			}

		case glTexCoordPointer_fake_func:
			{
				int offset = args[0];
				int index = args[1];
				int size = args[2];
				int type = args[3];
				int stride = args[4];
				int bytes_size = args[5];
				process->current_state->texCoordPointerSize[index] = MAX(process->current_state->texCoordPointerSize[index], offset + bytes_size);
				process->current_state->texCoordPointer[index] = realloc(process->current_state->texCoordPointer[index], process->current_state->texCoordPointerSize[index]);
				memcpy(process->current_state->texCoordPointer[index] + offset, (void*)args[6], bytes_size);
				/*fprintf(stderr, "glTexCoordPointer_fake_func size=%d, type=%d, stride=%d, byte_size=%d\n",
				  size, type, stride, bytes_size);*/
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + index);
				glTexCoordPointer(size, type, stride, process->current_state->texCoordPointer[index]);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glWeightPointerARB_fake_func:
			{
				GET_EXT_PTR(void, glWeightPointerARB, (int,int,int,void*));
				int offset = args[0];
				int size = args[1];
				int type = args[2];
				int stride = args[3];
				int bytes_size = args[4];
				process->current_state->weightPointerSize = MAX(process->current_state->weightPointerSize, offset + bytes_size);
				process->current_state->weightPointer = realloc(process->current_state->weightPointer, process->current_state->weightPointerSize);
				memcpy(process->current_state->weightPointer + offset, (void*)args[5], bytes_size);
				/*fprintf(stderr, "glWeightPointerARB_fake_func size=%d, type=%d, stride=%d, byte_size=%d\n",
				  size, type, stride, bytes_size);*/
				ptr_func_glWeightPointerARB(size, type, stride, process->current_state->weightPointer);
				break;
			}

		case glMatrixIndexPointerARB_fake_func:
			{
				GET_EXT_PTR(void, glMatrixIndexPointerARB, (int,int,int,void*));
				int offset = args[0];
				int size = args[1];
				int type = args[2];
				int stride = args[3];
				int bytes_size = args[4];
				process->current_state->matrixIndexPointerSize = MAX(process->current_state->matrixIndexPointerSize, offset + bytes_size);
				process->current_state->matrixIndexPointer = realloc(process->current_state->matrixIndexPointer, process->current_state->matrixIndexPointerSize);
				memcpy(process->current_state->matrixIndexPointer + offset, (void*)args[5], bytes_size);
				/*fprintf(stderr, "glMatrixIndexPointerARB_fake_func size=%d, type=%d, stride=%d, byte_size=%d\n",
				  size, type, stride, bytes_size);*/
				ptr_func_glMatrixIndexPointerARB(size, type, stride, process->current_state->matrixIndexPointer);
				break;
			}

		case glFogCoordPointer_fake_func:
			{
				GET_EXT_PTR(void, glFogCoordPointer, (int,int,void*));
				int offset = args[0];
				int type = args[1];
				int stride = args[2];
				int bytes_size = args[3];
				process->current_state->fogCoordPointerSize = MAX(process->current_state->fogCoordPointerSize, offset + bytes_size);
				process->current_state->fogCoordPointer = realloc(process->current_state->fogCoordPointer, process->current_state->fogCoordPointerSize);
				memcpy(process->current_state->fogCoordPointer + offset, (void*)args[4], bytes_size);
				//fprintf(stderr, "glFogCoordPointer_fake_func type=%d, stride=%d, byte_size=%d\n", type, stride, bytes_size);
				ptr_func_glFogCoordPointer(type, stride, process->current_state->fogCoordPointer);
				break;
			}

		case glVariantPointerEXT_fake_func:
			{
				GET_EXT_PTR(void, glVariantPointerEXT, (int,int,int,void*));
				int offset = args[0];
				int id = args[1];
				int type = args[2];
				int stride = args[3];
				int bytes_size = args[4];
				process->current_state->variantPointerEXTSize[id] = MAX(process->current_state->variantPointerEXTSize[id], offset + bytes_size);
				process->current_state->variantPointerEXT[id] = realloc(process->current_state->variantPointerEXT[id], process->current_state->variantPointerEXTSize[id]);
				memcpy(process->current_state->variantPointerEXT[id] + offset, (void*)args[5], bytes_size);
				//fprintf(stderr, "glVariantPointerEXT_fake_func[%d] type=%d, stride=%d, byte_size=%d\n", id, type, stride, bytes_size);
				ptr_func_glVariantPointerEXT(id, type, stride, process->current_state->variantPointerEXT[id]);
				break;
			}

		case glInterleavedArrays_fake_func:
			{
				GET_EXT_PTR(void, glInterleavedArrays, (int,int,void*));
				int offset = args[0];
				int format = args[1];
				int stride = args[2];
				int bytes_size = args[3];
				process->current_state->interleavedArraysSize = MAX(process->current_state->interleavedArraysSize, offset + bytes_size);
				process->current_state->interleavedArrays = realloc(process->current_state->interleavedArrays, process->current_state->interleavedArraysSize);
				memcpy(process->current_state->interleavedArrays + offset, (void*)args[4], bytes_size);
				//fprintf(stderr, "glInterleavedArrays_fake_func format=%d, stride=%d, byte_size=%d\n", format, stride, bytes_size);
				ptr_func_glInterleavedArrays(format, stride, process->current_state->interleavedArrays);
				break;
			}

		case glElementPointerATI_fake_func:
			{
				GET_EXT_PTR(void, glElementPointerATI, (int,void*));
				int type = args[0];
				int bytes_size = args[1];
				process->current_state->elementPointerATISize = bytes_size;
				process->current_state->elementPointerATI = realloc(process->current_state->elementPointerATI, process->current_state->elementPointerATISize);
				memcpy(process->current_state->elementPointerATI, (void*)args[2], bytes_size);
				//fprintf(stderr, "glElementPointerATI_fake_func type=%d, byte_size=%d\n", type, bytes_size);
				ptr_func_glElementPointerATI(type, process->current_state->elementPointerATI);
				break;
			}

		case glTexCoordPointer01_fake_func:
			{
				int size = args[0];
				int type = args[1];
				int stride = args[2];
				int bytes_size = args[3];
				process->current_state->texCoordPointerSize[0] = bytes_size;
				process->current_state->texCoordPointer[0] = realloc(process->current_state->texCoordPointer[0], bytes_size);
				memcpy(process->current_state->texCoordPointer[0], (void*)args[4], bytes_size);
				/*fprintf(stderr, "glTexCoordPointer01_fake_func size=%d, type=%d, stride=%d, byte_size=%d\n",
				  size, type, stride, bytes_size);*/
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(size, type, stride, process->current_state->texCoordPointer[0]);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
				glTexCoordPointer(size, type, stride, process->current_state->texCoordPointer[0]);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glTexCoordPointer012_fake_func:
			{
				int size = args[0];
				int type = args[1];
				int stride = args[2];
				int bytes_size = args[3];
				process->current_state->texCoordPointerSize[0] = bytes_size;
				process->current_state->texCoordPointer[0] = realloc(process->current_state->texCoordPointer[0], bytes_size);
				memcpy(process->current_state->texCoordPointer[0], (void*)args[4], bytes_size);
				/*fprintf(stderr, "glTexCoordPointer012_fake_func size=%d, type=%d, stride=%d, byte_size=%d\n",
				  size, type, stride, bytes_size);*/
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(size, type, stride, process->current_state->texCoordPointer[0]);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
				glTexCoordPointer(size, type, stride, process->current_state->texCoordPointer[0]);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 2);
				glTexCoordPointer(size, type, stride, process->current_state->texCoordPointer[0]);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}


		case glVertexAndNormalPointer_fake_func:
			{
				int vertexPointerSize = args[0];
				int vertexPointerType = args[1];
				int vertexPointerStride = args[2];
				int normalPointerType = args[3];
				int normalPointerStride = args[4];
				int bytes_size = args[5];
				void* ptr = (void*)args[6];
				process->current_state->vertexPointerSize = bytes_size;
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, bytes_size);
				memcpy(process->current_state->vertexPointer, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, vertexPointerStride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, normalPointerStride, process->current_state->vertexPointer);
				break;
			}

		case glVertexNormalPointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset= args[i++];
				int normalPointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				break;
			}

		case glTuxRacerDrawElements_fake_func:
			{
				int mode = args[0];
				int count = args[1];
				int isColorEnabled = args[2];
				void* ptr = (void*)args[3];
				int stride = 6 * sizeof(float) + ((isColorEnabled) ? 4 * sizeof(unsigned char) : 0);
				glVertexPointer( 3, GL_FLOAT, stride, ptr);
				glNormalPointer( GL_FLOAT, stride, ptr + 3 * sizeof(float));
				if (isColorEnabled)
					glColorPointer( 4, GL_UNSIGNED_BYTE, stride, ptr + 6 * sizeof(float));
				glDrawArrays(mode, 0, count);
				break;
			}

		case glVertexNormalColorPointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset= args[i++];
				int normalPointerType = args[i++];
				int colorPointerOffset = args[i++];
				int colorPointerSize = args[i++];
				int colorPointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				glColorPointer(colorPointerSize, colorPointerType, stride, process->current_state->vertexPointer + colorPointerOffset);
				break;
			}

		case glVertexColorTexCoord0PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int colorPointerOffset = args[i++];
				int colorPointerSize = args[i++];
				int colorPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glColorPointer(colorPointerSize, colorPointerType, stride, process->current_state->vertexPointer + colorPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glVertexNormalTexCoord0PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset = args[i++];
				int normalPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glVertexNormalTexCoord01PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset = args[i++];
				int normalPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int texCoord1PointerOffset = args[i++];
				int texCoord1PointerSize = args[i++];
				int texCoord1PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
				glTexCoordPointer(texCoord1PointerSize, texCoord1PointerType, stride, process->current_state->vertexPointer + texCoord1PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glVertexNormalTexCoord012PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset = args[i++];
				int normalPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int texCoord1PointerOffset = args[i++];
				int texCoord1PointerSize = args[i++];
				int texCoord1PointerType = args[i++];
				int texCoord2PointerOffset = args[i++];
				int texCoord2PointerSize = args[i++];
				int texCoord2PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
				glTexCoordPointer(texCoord1PointerSize, texCoord1PointerType, stride, process->current_state->vertexPointer + texCoord1PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 2);
				glTexCoordPointer(texCoord2PointerSize, texCoord2PointerType, stride, process->current_state->vertexPointer + texCoord2PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glVertexNormalColorTexCoord0PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset = args[i++];
				int normalPointerType = args[i++];
				int colorPointerOffset = args[i++];
				int colorPointerSize = args[i++];
				int colorPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				glColorPointer(colorPointerSize, colorPointerType, stride, process->current_state->vertexPointer + colorPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glVertexNormalColorTexCoord01PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset = args[i++];
				int normalPointerType = args[i++];
				int colorPointerOffset = args[i++];
				int colorPointerSize = args[i++];
				int colorPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int texCoord1PointerOffset = args[i++];
				int texCoord1PointerSize = args[i++];
				int texCoord1PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				glColorPointer(colorPointerSize, colorPointerType, stride, process->current_state->vertexPointer + colorPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
				glTexCoordPointer(texCoord1PointerSize, texCoord1PointerType, stride, process->current_state->vertexPointer + texCoord1PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case glVertexNormalColorTexCoord012PointerInterlaced_fake_func:
			{
				int i = 0;
				int offset = args[i++];
				int vertexPointerSize = args[i++];
				int vertexPointerType = args[i++];
				int stride = args[i++];
				int normalPointerOffset = args[i++];
				int normalPointerType = args[i++];
				int colorPointerOffset = args[i++];
				int colorPointerSize = args[i++];
				int colorPointerType = args[i++];
				int texCoord0PointerOffset = args[i++];
				int texCoord0PointerSize = args[i++];
				int texCoord0PointerType = args[i++];
				int texCoord1PointerOffset = args[i++];
				int texCoord1PointerSize = args[i++];
				int texCoord1PointerType = args[i++];
				int texCoord2PointerOffset = args[i++];
				int texCoord2PointerSize = args[i++];
				int texCoord2PointerType = args[i++];
				int bytes_size = args[i++];
				void* ptr = (void*)args[i++];
				process->current_state->vertexPointerSize = MAX(process->current_state->vertexPointerSize, offset + bytes_size);
				process->current_state->vertexPointer = realloc(process->current_state->vertexPointer, process->current_state->vertexPointerSize);
				memcpy(process->current_state->vertexPointer + offset, ptr, bytes_size);
				glVertexPointer(vertexPointerSize, vertexPointerType, stride, process->current_state->vertexPointer);
				glNormalPointer(normalPointerType, stride, process->current_state->vertexPointer + normalPointerOffset);
				glColorPointer(colorPointerSize, colorPointerType, stride, process->current_state->vertexPointer + colorPointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
				glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType, stride, process->current_state->vertexPointer + texCoord0PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
				glTexCoordPointer(texCoord1PointerSize, texCoord1PointerType, stride, process->current_state->vertexPointer + texCoord1PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 2);
				glTexCoordPointer(texCoord2PointerSize, texCoord2PointerType, stride, process->current_state->vertexPointer + texCoord2PointerOffset);
				do_glClientActiveTextureARB(GL_TEXTURE0_ARB + process->current_state->activeTextureIndex);
				break;
			}

		case _glVertexPointer_buffer_func:
			{
				glVertexPointer(args[0],args[1],args[2],(void*)args[3]);
				break;
			}

		case _glNormalPointer_buffer_func:
			{
				glNormalPointer(args[0],args[1],(void*)args[2]);
				break;
			}

		case _glColorPointer_buffer_func:
			{
				glColorPointer(args[0],args[1],args[2],(void*)args[3]);
				break;
			}

		case _glSecondaryColorPointer_buffer_func:
			{
				GET_EXT_PTR(void, glSecondaryColorPointer, (int,int,int,void*));
				ptr_func_glSecondaryColorPointer(args[0],args[1],args[2],(void*)args[3]);
				break;
			}

		case _glIndexPointer_buffer_func:
			{
				glIndexPointer(args[0],args[1],(void*)args[2]);
				break;
			}

		case _glTexCoordPointer_buffer_func:
			{
				glTexCoordPointer(args[0],args[1],args[2],(void*)args[3]);
				break;
			}

		case _glEdgeFlagPointer_buffer_func:
			{
				glEdgeFlagPointer(args[0],(void*)args[1]);
				break;
			}

		case _glVertexAttribPointerARB_buffer_func:
			{
				GET_EXT_PTR(void, glVertexAttribPointerARB, (int,int,int,int,int,void*));
				ptr_func_glVertexAttribPointerARB(args[0], args[1], args[2], args[3], args[4], (void*)args[5]);
				break;
			}

		case _glWeightPointerARB_buffer_func:
			{
				GET_EXT_PTR(void, glWeightPointerARB, (int,int,int,void*));
				ptr_func_glWeightPointerARB(args[0], args[1], args[2], (void*)args[3]);
				break;
			}

		case _glMatrixIndexPointerARB_buffer_func:
			{
				GET_EXT_PTR(void, glMatrixIndexPointerARB, (int,int,int,void*));
				ptr_func_glMatrixIndexPointerARB(args[0], args[1], args[2], (void*)args[3]);
				break;
			}

		case _glFogCoordPointer_buffer_func:
			{
				GET_EXT_PTR(void, glFogCoordPointer, (int,int,void*));
				ptr_func_glFogCoordPointer(args[0], args[1], (void*)args[2]);
				break;
			}

		case _glVariantPointerEXT_buffer_func:
			{
				GET_EXT_PTR(void, glVariantPointerEXT, (int, int,int,void*));
				ptr_func_glVariantPointerEXT(args[0], args[1], args[2], (void*)args[3]);
				break;
			}

		case _glDrawElements_buffer_func:
			{
				glDrawElements(args[0],args[1],args[2],(void*)args[3]);
				break;
			}

		case _glDrawRangeElements_buffer_func:
			{
				glDrawRangeElements(args[0],args[1],args[2],args[3],args[4],(void*)args[5]);
				break;
			}

		case _glMultiDrawElements_buffer_func:
			{
				GET_EXT_PTR(void, glMultiDrawElements, (int,int*,int,void**, int));
				ptr_func_glMultiDrawElements(args[0],(int*)args[1],args[2],(void**)args[3],args[4]);
				break;
			}

		case _glGetError_fake_func:
			{
				break;
			}

		case glGetIntegerv_func:
			{
				glGetIntegerv(args[0], (int*)args[1]);
				//fprintf(stderr,"glGetIntegerv(%X)=%d\n", (int)args[0], *(int*)args[1]);
				break;
			}

		case _glReadPixels_pbo_func:
			{
				glReadPixels(ARG_TO_INT(args[0]), ARG_TO_INT(args[1]), ARG_TO_INT(args[2]), ARG_TO_INT(args[3]), ARG_TO_UNSIGNED_INT(args[4]), ARG_TO_UNSIGNED_INT(args[5]), (void*)(args[6]));
				break;
			}

		case _glDrawPixels_pbo_func:
			{
				glDrawPixels(ARG_TO_INT(args[0]), ARG_TO_INT(args[1]), ARG_TO_UNSIGNED_INT(args[2]), ARG_TO_UNSIGNED_INT(args[3]), (const void*)(args[4]));
				break;
			}

		case _glMapBufferARB_fake_func:
			{
				GET_EXT_PTR(GLvoid*, glMapBufferARB, (GLenum, GLenum));
				GET_EXT_PTR(GLboolean, glUnmapBufferARB, (GLenum));
				int target = args[0];
				int size = args[1];
				void* dst_ptr = (void*)args[2];
				void* src_ptr = ptr_func_glMapBufferARB(target, GL_READ_ONLY);
				if (src_ptr)
				{
					memcpy(dst_ptr, src_ptr, size);
					ret_int = ptr_func_glUnmapBufferARB(target);
				}
				else
				{
					ret_int = 0;
				}
				break;
			}

		case fake_gluBuild2DMipmaps_func:
			{
				GET_GLU_PTR(GLint, gluBuild2DMipmaps, (GLenum arg_0, GLint arg_1, GLsizei arg_2, GLsizei arg_3, GLenum arg_4, GLenum arg_5, const GLvoid * arg_6));
				if (ptr_func_gluBuild2DMipmaps == NULL)
					ptr_func_gluBuild2DMipmaps = mesa_gluBuild2DMipmaps;
				ptr_func_gluBuild2DMipmaps(ARG_TO_UNSIGNED_INT(args[0]), ARG_TO_INT(args[1]), ARG_TO_INT(args[2]), ARG_TO_INT(args[3]), ARG_TO_UNSIGNED_INT(args[4]), ARG_TO_UNSIGNED_INT(args[5]), (const void*)(args[6]));
				break;
			}

		case _glSelectBuffer_fake_func:
			{
				process->current_state->selectBufferSize = args[0] * 4;
				process->current_state->selectBufferPtr = realloc(process->current_state->selectBufferPtr, process->current_state->selectBufferSize);
				glSelectBuffer(args[0], process->current_state->selectBufferPtr);
				break;
			}

		case _glGetSelectBuffer_fake_func:
			{
				void* ptr = (void*)args[0];
				memcpy(ptr, process->current_state->selectBufferPtr, process->current_state->selectBufferSize);
				break;
			}

		case _glFeedbackBuffer_fake_func:
			{
				process->current_state->feedbackBufferSize = args[0] * 4;
				process->current_state->feedbackBufferPtr = realloc(process->current_state->feedbackBufferPtr, process->current_state->feedbackBufferSize);
				glFeedbackBuffer(args[0], args[1], process->current_state->feedbackBufferPtr);
				break;
			}

		case _glGetFeedbackBuffer_fake_func:
			{
				void* ptr = (void*)args[0];
				memcpy(ptr, process->current_state->feedbackBufferPtr, process->current_state->feedbackBufferSize);
				break;
			}

			/*
			   case glEnableClientState_func:
			   {
			   if (display_function_call) fprintf(stderr, "cap : %s\n", nameArrays[args[0] - GL_VERTEX_ARRAY]);
			   glEnableClientState(args[0]);
			   break;
			   }

			   case glDisableClientState_func:
			   {
			   if (display_function_call) fprintf(stderr, "cap : %s\n", nameArrays[args[0] - GL_VERTEX_ARRAY]);
			   glDisableClientState(args[0]);
			   break;
			   }

			   case glClientActiveTexture_func:
			   case glClientActiveTextureARB_func:
			   {
			   if (display_function_call) fprintf(stderr, "client activeTexture %d\n", args[0] - GL_TEXTURE0_ARB);
			   glClientActiveTextureARB(args[0]);
			   break;
			   }

			   case glActiveTextureARB_func:
			   {
			   if (display_function_call) fprintf(stderr, "server activeTexture %d\n", args[0] - GL_TEXTURE0_ARB);
			   glActiveTextureARB(args[0]);
			   break;
			   }

			   case glLockArraysEXT_func:
			   break;

			   case glUnlockArraysEXT_func:
			   break;

			   case glArrayElement_func:
			   {
			   glArrayElement(args[0]);
			   break;
			   }

			   case glDrawArrays_func:
			   {
			   glDrawArrays(args[0],args[1],args[2]);
			   break;
			   }

			   case glDrawElements_func:
			   {
			   glDrawElements(args[0],args[1],args[2],(void*)args[3]);
			   break;
			   }

			   case glDrawRangeElements_func:
			   {
			   glDrawRangeElements(args[0],args[1],args[2],args[3],args[4],(void*)args[5]);
			   break;
			   }
			 */

		case glGetError_func:
			{
#ifdef SYSTEMATIC_ERROR_CHECK
				ret_int = process->current_state->last_error;
#else
				ret_int = glGetError();
#endif
				break;
			}

		case glNewObjectBufferATI_func:
			{
				GET_EXT_PTR(int, glNewObjectBufferATI, (int,void*, int));
				ret_int = ptr_func_glNewObjectBufferATI(args[0], (void*)args[1], args[2]);
				break;
			}

		default:
			execute_func(func_number, args, &ret_int, &ret_char);
			break;
	}

#ifdef SYSTEMATIC_ERROR_CHECK
	if (func_number == glGetError_func)
	{
		process->current_state->last_error = 0;
	}
	else
	{
		process->current_state->last_error = glGetError();
		if (process->current_state->last_error != 0)
		{
			printf("error %s 0x%X\n",  tab_opengl_calls_name[func_number], process->current_state->last_error);
		}
	}
#endif

	Signature* signature = (Signature*)tab_opengl_calls[func_number];
	int ret_type = signature->ret_type;
	//int nb_args = signature->nb_args;
	switch(ret_type)
	{
		case TYPE_NONE:
			break;

		case TYPE_CHAR:
		case TYPE_UNSIGNED_CHAR:
			ret_int = ret_char;
			break;

		case TYPE_INT:
		case TYPE_UNSIGNED_INT:
			break;

		case TYPE_CONST_CHAR:
			{
				strncpy(ret_string, (ret_str) ? ret_str : "", 32768);
				break;
			}

		default:
			fprintf(stderr, "unexpected ret type : %d\n", ret_type);
			exit(-1);
			break;
	}

	if (display_function_call) fprintf(stderr, "[%d]< %s\n", process->instr_counter, tab_opengl_calls_name[func_number]);

	return ret_int;
}
#endif

void create_process_tab( OGLS_Conn *pConn )
{
	if (pConn == NULL) {
		fprintf(stderr, "create_process_tab: pConn is NULL.\n");
		return;
	}
	pConn->processTab = malloc( sizeof(ProcessStruct)*MAX_HANDLED_PROCESS );
	if( !pConn->processTab )
	{
		perror( "init_process_tab" );
		return ;
	}

	memset(pConn->processTab, 0, sizeof(ProcessStruct)*MAX_HANDLED_PROCESS );
}

void remove_process_tab( OGLS_Conn *pConn )
{
	if( !pConn->processTab ) return ;
	free( pConn->processTab );
	pConn->processTab = NULL;
}
