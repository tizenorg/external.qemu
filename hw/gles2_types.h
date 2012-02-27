/* Copyright (c) 2009-2010 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) any later version of the License.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GLES2_TYPES_H_
#define GLES2_TYPES_H_

// Automatically create the prototype and function definition.
#define GLES2_CB(FUNC) \
    void gles2_##FUNC##_cb(gles2_State *s, \
        gles2_decode_t *d, gles2_Client *c); \
    void gles2_##FUNC##_cb(gles2_State *s, \
        gles2_decode_t *d, gles2_Client *c)

// Sizes of primitive types in the ABI.
#define GLES2_HTYPE_byte uint8_t
#define GLES2_HTYPE_word uint16_t
#define GLES2_HTYPE_dword uint32_t
#define GLES2_HTYPE_float float
#define GLES2_HTYPE_handle uint32_t

// Defines shorthands for handling types.
#define GLES2_TYPE(TYPE, SIZE) \
    typedef GLES2_HTYPE_##SIZE TYPE; \
    static inline void gles2_ret_##TYPE(gles2_State *s, TYPE value) \
    { gles2_ret_##SIZE(s, value); } \
    static inline void gles2_put_##TYPE(gles2_State *s, target_ulong va, TYPE value) \
    { gles2_put_##SIZE(s, va, value); } \
    static inline TYPE gles2_get_##TYPE(gles2_State *s, target_ulong va) \
    { return (TYPE)gles2_get_##SIZE(s, va); } \
    static inline TYPE gles2_arg_##TYPE(gles2_State *s, gles2_decode_t *d) \
    { return (TYPE)gles2_arg_##SIZE(s, d); }

// Bunch of expansions of previous macro to ease things up.
GLES2_TYPE(Tptr, dword)
GLES2_TYPE(TEGLBoolean, dword)
GLES2_TYPE(TEGLenum, dword)
GLES2_TYPE(TEGLint, dword)
GLES2_TYPE(TEGLDisplay, handle)
GLES2_TYPE(TEGLConfig, handle)
GLES2_TYPE(TEGLContext, handle)
GLES2_TYPE(TEGLSurface, handle)

GLES2_TYPE(TGLclampf, float)
GLES2_TYPE(TGLbitfield, dword)
GLES2_TYPE(TGLboolean, byte)
GLES2_TYPE(TGLint, dword)
GLES2_TYPE(TGLuint, dword)
GLES2_TYPE(TGLushort, word)
GLES2_TYPE(TGLubyte, byte)
GLES2_TYPE(TGLenum, dword)
GLES2_TYPE(TGLsizei, dword)
GLES2_TYPE(TGLfloat, float)
GLES2_TYPE(TGLfixed, dword)
GLES2_TYPE(TGLclampx, dword)


// Just one more macro for even less typing.
#define GLES2_ARG(TYPE, NAME) \
    TYPE NAME = gles2_arg_##TYPE(s, d)

//        pthread_cond_signal(&s->cond_xcode);


// Host to guest vertex array copy.
struct gles2_Array
{
    GLuint indx;          // Parameter of the call.
    GLint size;           // --''--
    GLenum type;          // --''--
    GLboolean normalized; // --''--
    GLsizei stride;       // --''--
    GLsizei real_stride;       // --''--
    Tptr tptr;            // Pointer in the guest memory.
    void* ptr;            // Pointer in the host memory.

    void (*apply) (struct gles2_Array *va);

    GLboolean enabled;    // State.
};

unsigned gles1_glGetCount(TGLenum pname);
void checkGLESError(void);

#endif /* GLES2_TYPES_H_ */
