/*
 *  Parse the "get.c" from mesa source tree to generate "glgetv_cst.h"
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

// gcc -Wall parse_mesa_get_c.c -o parse_mesa_get_c -I. && ./parse_mesa_get_c

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mesa_gl.h"
#include "mesa_glext.h"

/* #include "mesa_enums.c" */

int gl_lookup_enum_by_name(const char* name)
{
  FILE* f;
  char buffer[256];
  char template1[256];
  char template2[256];
  int i;
  sprintf(template1, "#define %s\t", name);
  sprintf(template2, "#define %s ", name);
  for(i=0;i<2;i++)
  {
    if (i == 0)
      f = fopen("mesa_gl.h", "r");
    else
      f = fopen("mesa_glext.h", "r");
    while(fgets(buffer, 256, f))
    {
      if (strstr(buffer, template1) || strstr(buffer, template2))
      {
        char* c = strstr(buffer, "0x");
        assert(c);
        int ret;
        ret = strtol(c, NULL, 16);
        return ret;
      }
    }
    fclose(f);
  }

  return -1;
}

typedef struct
{
  int value;
  char* name;
  int nb_elts;
} Token;

int compare_func(Token* a, Token* b)
{
  return a->value - b->value;
}

typedef struct
{
  int i;
  char* str;
} Cpl;
#define CPL(x) {x, #x}

Cpl constantsOneVal[] =
{
  CPL(GL_MAX_GENERAL_COMBINERS_NV),
  CPL(GL_MAX_VERTEX_SHADER_INSTRUCTIONS_EXT),
  CPL(GL_MAX_VERTEX_SHADER_VARIANTS_EXT),
  CPL(GL_MAX_VERTEX_SHADER_INVARIANTS_EXT),
  CPL(GL_MAX_VERTEX_SHADER_LOCAL_CONSTANTS_EXT),
  CPL(GL_MAX_VERTEX_SHADER_LOCALS_EXT),
  CPL(GL_MAX_OPTIMIZED_VERTEX_SHADER_INSTRUCTIONS_EXT),
  CPL(GL_MAX_OPTIMIZED_VERTEX_SHADER_VARIANTS_EXT),
  CPL(GL_MAX_OPTIMIZED_VERTEX_SHADER_INVARIANTS_EXT),
  CPL(GL_MAX_OPTIMIZED_VERTEX_SHADER_LOCAL_CONSTANTS_EXT),
  CPL(GL_MAX_OPTIMIZED_VERTEX_SHADER_LOCAL_CONSTANTS_EXT),
};
#define NB_CONSTANTS_ONE_VAL  11

int main(int argc, char* argv[])
{
  FILE* f = fopen("mesa_get.c", "r");
  char buffer[256];
  int state = 0;
  char name[256];
  int count = 0;
  char template[256];
  Token tokens[1000];
  int ntoken = 0;
  int i;
  FILE* outf = fopen("glgetv_cst.h", "w");

  fprintf(outf, "/* This is a generated file. Do not edit !*/\n");
  fprintf(outf, "typedef struct {\n");
  fprintf(outf, "  GLuint count;\n");
  fprintf(outf, "  GLenum token;\n");
  fprintf(outf, "  const char *name;\n");
  fprintf(outf, "} GlGetConstant ;\n");
  fprintf(outf, "static const GlGetConstant gl_get_constants[] = {\n");

  while(fgets(buffer, 256, f))
  {
    if (strstr(buffer, "_mesa_GetBooleanv"))
    {
      state = 1;
    }
    else if (state == 1)
    {
      if (strstr(buffer, "_mesa_GetFloatv"))
      {
        break;
      }
      else if (strstr(buffer, "case GL_"))
      {
        strcpy(name, strstr(buffer, "case GL_") + 5);
        *strstr(name, ":") = 0;
        count = 0;
        strcpy(template, "params[0]");
      }
      else if (strstr(buffer, template))
      {
        count ++;
        sprintf(template, "params[%d]", count);
      }
      else if (strstr(buffer, "break"))
      {
        if (count > 0)
        {
          int gl_lookup = gl_lookup_enum_by_name(name);
          /*
          int mesa_lookup = _mesa_lookup_enum_by_name(name);
          if (mesa_lookup != -1)
          {
            if(mesa_lookup != gl_lookup)
            {
              fprintf(stderr, "wrong : %s %d %d !\n", name, mesa_lookup, gl_lookup);
              exit(-1);
            }
          }*/
          if (gl_lookup == -1)
          {
            fprintf(stderr, "not found in includes : %s\n", name);
            //fprintf(outf, "/*  { %d, unknown value, \"%s\" },*/\n", count, name);
          }
          else
          {
            //fprintf(outf, "  { %d, 0x%04X, \"%s\" },\n", count, gl_lookup, name);
            tokens[ntoken].value = gl_lookup;
            tokens[ntoken].nb_elts = count;
            tokens[ntoken].name = strdup(name);
            ntoken++;
          }
          /*fprintf(outf, "#ifdef %s\n", name);
          fprintf(outf, "  { %d, MAKE_TOKEN_NAME(%s) },\n", count, name);
          fprintf(outf, "#endif\n");*/
        }
        else
          fprintf(stderr, "not recognized : %s\n", name);
      }
    }
  }

  for(i=0;i<NB_CONSTANTS_ONE_VAL;i++)
  {
    tokens[ntoken].value = constantsOneVal[i].i;
    tokens[ntoken].nb_elts = 1;
    tokens[ntoken].name = strdup(constantsOneVal[i].str);
    ntoken++;
  }

  tokens[ntoken].value = GL_SPRITE_MODE_SGIX;
  tokens[ntoken].nb_elts = 1;
  tokens[ntoken].name = strdup("GL_SPRITE_MODE_SGIX");
  ntoken++;

  tokens[ntoken].value = GL_SPRITE_AXIS_SGIX ;
  tokens[ntoken].nb_elts = 3;
  tokens[ntoken].name = strdup("GL_SPRITE_AXIS_SGIX ");
  ntoken++;

  tokens[ntoken].value = GL_SPRITE_TRANSLATION_SGIX;
  tokens[ntoken].nb_elts = 3;
  tokens[ntoken].name = strdup("GL_SPRITE_TRANSLATION_SGIX");
  ntoken++;

  tokens[ntoken].value = GL_REFERENCE_PLANE_EQUATION_SGIX;
  tokens[ntoken].nb_elts = 4;
  tokens[ntoken].name = strdup("GL_REFERENCE_PLANE_EQUATION_SGIX");
  ntoken++;

  qsort(tokens, ntoken, sizeof(Token), compare_func);
  for(i=0;i<ntoken;i++)
  {
    fprintf(outf, "  { %d, 0x%04X, \"%s\" },\n", tokens[i].nb_elts, tokens[i].value, tokens[i].name);
  }
  fprintf(outf, "};\n");
  fclose(f);
  fclose(outf);
  return 0;
}
