/*
--------------------------------------------------
    James William Fletcher (github.com/mrbid)
        August 2023 - esVoxel.h v4.0
--------------------------------------------------

    A pretty good color converter: https://www.easyrgb.com/en/convert.php

    I was going to supply position and color on two Uniform1f's calls and
    execute the color Uniform1f using the painters algorithm on the limited
    32 color palette. This way less bandwidth would be used in the heavy
    per-voxel drawcall loop.

    The problem is this increases code complexity and the project performance
    gain is probably not worth while. I would need to track each color node
    placed in it's own array by it's main array index. This would require
    dynamic array realloc or pre-allocating 128^3 length arrays for each
    maximum possible 32 colors. That's 268.43 MB.

    I don't think it's in anyones interest for me to go to that extreme,
    the performance benefit is unlikely to be felt by the user and thus
    makes the code more complicated without good reason.

    Requires:
        - vec.h: https://gist.github.com/mrbid/77a92019e1ab8b86109bf103166bd04e
        - mat.h: https://gist.github.com/mrbid/cbc69ec9d99b0fda44204975fcbeae7c
    
*/

#ifndef AUX_H
#define AUX_H

#include "vec.h"
#include "mat.h"

//*************************************
// VOXEL STRUCTURE
//*************************************
typedef struct
{
    GLuint vid; // vertex buff id
    GLuint iid;	// index buff id
    GLuint tid;	// uv/st texcoord buff id
} ESModel;
const GLfloat voxel_vertices[] = {-0.5,0.5,0.5,0.5,-0.5,0.5,0.5,0.5,0.5,0.5,-0.5,0.5,-0.5,-0.5,-0.5,0.5,-0.5,-0.5,-0.5,-0.5,0.5,-0.5,0.5,-0.5,-0.5,-0.5,-0.5,0.5,0.5,-0.5,-0.5,-0.5,-0.5,-0.5,0.5,-0.5,0.5,0.5,0.5,0.5,-0.5,-0.5,0.5,0.5,-0.5,-0.5,0.5,0.5,0.5,0.5,-0.5,-0.5,0.5,-0.5,-0.5,-0.5,0.5,-0.5,-0.5,0.5,-0.5,0.5,0.5,0.5,-0.5,-0.5,0.5,-0.5,0.5,0.5,0.5,0.5,0.5,0.5,-0.5};
const GLubyte voxel_indices[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,0,18,1,3,19,4,6,20,7,9,21,10,12,22,13,15,23,24};
const GLsizeiptr voxel_numind = 36;
ESModel mdlVoxel;

const GLfloat hud_vertices[] = {-1,1,0,1,-1,0,1,1,0,-1,-1,0};
const GLfloat hud_uvmap[] = {0,0, 1,1, 1,0, 0,1};
const GLubyte hud_indices[] = {0,1,2,0,3,1};
const GLsizeiptr hud_numind = 6;
ESModel mdlPlane;
SDL_Surface* sHud;
GLuint hudmap;

//*************************************
// SHADER
//*************************************
void makeHud();
void shadeHud(GLint* position, GLint* texcoord, GLint* sampler);
void flipHud()
{
    glBindTexture(GL_TEXTURE_2D, hudmap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sHud->w, sHud->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, sHud->pixels);
}

void makeVoxel();
void shadeVoxel(GLint* projection, GLint* view, GLint* position, GLint* voxel);

//*************************************
// UTILITY CODE
//*************************************
GLuint esRand(const GLuint min, const GLuint max)
{
    return (rand()%(max+1-min))+min;
}
GLfloat esRandFloat(const GLfloat min, const GLfloat max)
{
    static GLfloat rrndmax = 1.f/(GLfloat)RAND_MAX;
    return (((GLfloat)rand()) * rrndmax) * (max-min) + min;
}
void esBind(const GLenum target, GLuint* buffer, const void* data, const GLsizeiptr datalen, const GLenum usage)
{
    glGenBuffers(1, buffer);
    glBindBuffer(target, *buffer);
    glBufferData(target, datalen, data, usage);
}
void esRebind(const GLenum target, GLuint* buffer, const void* data, const GLsizeiptr datalen, const GLenum usage)
{
    glBindBuffer(target, *buffer);
    glBufferData(target, datalen, data, usage);
}
GLuint esLoadTextureA(const GLuint w, const GLuint h, const unsigned char* data, const GLuint linear)
{
    GLuint textureId;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if(linear == 0)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    return textureId;
}

//*************************************
// SHADER CODE
//*************************************
const GLchar* v0 =
    "#version 100\n"
    "attribute vec4 position;\n"
    "attribute vec2 texcoord;\n"
    "varying vec2 vtc;\n"
    "void main()\n"
    "{\n"
        "vtc = texcoord;\n"
        "gl_Position = position;\n"
    "}\n";

const GLchar* f0 =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 vtc;\n"
    "uniform sampler2D tex;\n"
    "void main()\n"
    "{\n"
        "gl_FragColor = texture2D(tex, vtc);\n"
    "}\n";

///

const GLchar* v1 =
    "#version 100\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "uniform vec2 voxel;\n"
    "attribute vec4 position;\n"
    "varying vec3 color;\n"
    "varying vec3 vp;\n"
    "varying float vpl;\n"
    "void main()\n"
    "{\n"
        "vec3 nc;\n" // https://stackoverflow.com/a/12553149
        "nc.r = floor(voxel.y * 0.00001525878);\n"
        "nc.g = floor((voxel.y - nc.r * 65536.0) * 0.00390625);\n"
        "nc.b = floor(voxel.y - nc.r * 65536.0 - nc.g * 256.0);\n"
        "nc.r *= 0.00390625;\n"
        "nc.g *= 0.00390625;\n"
        "nc.b *= 0.00390625;\n"
        "float w = voxel.x * 0.000061035156;\n"
        "float z = floor(w);\n"
        "float y = fract(w) * 128.0;\n"
        "float x = fract(y) * 128.0;\n"
        "mat4 model;\n"
        "model[0] = vec4(1.0, 0.0, 0.0, 0.0);\n"
        "model[1] = vec4(0.0, 1.0, 0.0, 0.0);\n"
        "model[2] = vec4(0.0, 0.0, 1.0, 0.0);\n"
        "model[3] = vec4(floor(x), floor(y), floor(z), 1.0);\n"
        "mat4 modelview = view * model;\n"
        "vec4 vertPos4 = modelview * position;\n"
        "vec3 vertPos = vertPos4.xyz / vertPos4.w;\n"
        "color = nc * clamp(1.0-(length(vertPos)*0.001), 0.0, 1.0);\n"
        "vp = position.xyz;\n"
        "gl_Position = projection * vertPos4;\n"
    "}\n";

const GLchar* f1 =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec3 color;\n"
    "varying vec3 vp;\n"
    "void main()\n"
    "{\n"
        //"gl_FragColor = vec4(color, 1.0);\n"
        //"gl_FragColor = vec4(color * (1.0-(length(vp)*0.2)), 1.0);\n"
        "gl_FragColor = vec4(color * clamp(smoothstep(1.0, 0.7, length(vp)), 0.90, 1.0), 1.0);\n"
    "}\n";

//

GLuint shdHud;
GLint  shdHud_position;
GLint  shdHud_texcoord;
GLint  shdHud_sampler;

GLuint shdVoxel;
GLint  shdVoxel_projection;
GLint  shdVoxel_view;
GLint  shdVoxel_position;
GLint  shdVoxel_voxel;

//

void makeVoxel()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &v1, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &f1, NULL);
    glCompileShader(fragmentShader);

    shdVoxel = glCreateProgram();
        glAttachShader(shdVoxel, vertexShader);
        glAttachShader(shdVoxel, fragmentShader);
    glLinkProgram(shdVoxel);

    shdVoxel_position   = glGetAttribLocation(shdVoxel,  "position");
    //<><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
    shdVoxel_projection = glGetUniformLocation(shdVoxel, "projection");
    shdVoxel_view       = glGetUniformLocation(shdVoxel, "view");
    shdVoxel_voxel      = glGetUniformLocation(shdVoxel, "voxel");

    esBind(GL_ARRAY_BUFFER,         &mdlVoxel.vid, voxel_vertices, sizeof(voxel_vertices), GL_STATIC_DRAW);
    esBind(GL_ELEMENT_ARRAY_BUFFER, &mdlVoxel.iid, voxel_indices,  sizeof(voxel_indices),  GL_STATIC_DRAW);
}
void shadeVoxel(GLint* projection, GLint* view, GLint* position, GLint* voxel)
{
    *projection = shdVoxel_projection;
    *view = shdVoxel_view;
    *position = shdVoxel_position;
    *voxel = shdVoxel_voxel;
    glUseProgram(shdVoxel);
}

//

void makeHud()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &v0, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &f0, NULL);
    glCompileShader(fragmentShader);

    shdHud = glCreateProgram();
        glAttachShader(shdHud, vertexShader);
        glAttachShader(shdHud, fragmentShader);
    glLinkProgram(shdHud);

    shdHud_position   = glGetAttribLocation(shdHud,  "position");
    shdHud_texcoord   = glGetAttribLocation(shdHud,  "texcoord");
    //<><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
    shdHud_sampler    = glGetUniformLocation(shdHud, "tex");

    esBind(GL_ARRAY_BUFFER, &mdlPlane.vid, &hud_vertices, sizeof(hud_vertices), GL_STATIC_DRAW);
    esBind(GL_ELEMENT_ARRAY_BUFFER, &mdlPlane.iid, &hud_indices, sizeof(hud_indices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlPlane.tid, &hud_uvmap, sizeof(hud_uvmap), GL_STATIC_DRAW);
}
void shadeHud(GLint* position, GLint* texcoord, GLint* sampler)
{
    *position = shdHud_position;
    *texcoord = shdHud_texcoord;
    *sampler = shdHud_sampler;
    glUseProgram(shdHud);
}

//

#endif
