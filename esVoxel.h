/*
--------------------------------------------------
    James William Fletcher (github.com/mrbid)
         & Test_User 	   (notabug.org/test_user)
        	August 2023

	esVoxel.h v5.1 (glsl ray-tracing)
--------------------------------------------------

    A pretty good color converter: https://www.easyrgb.com/en/convert.php

    This version uses ray tracing in an OpenGL ES 2.0 shader to render a volume of voxels.

    v5.1:
        - Safe DDA (no NaN on axis-aligned rays, no float == face test)
        - Texel-center voxel lookups + reciprocal UV math
        - AABB slab entry instead of the unrolled outside-volume branch
        - esReLoadTextureA updates voxelmap via glTexSubImage2D

    vec/mat used by Woxel are inlined below.

*/

#ifndef AUX_H
#define AUX_H

#ifndef _WIN32
	#include <unistd.h>
#endif

#include <math.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>

typedef struct { float x,y,z,w; } vec;
typedef struct { float m[4][4]; } mat;

static void vAdd(vec* r, const vec v1, const vec v2)
{
    r->x = v1.x + v2.x; r->y = v1.y + v2.y; r->z = v1.z + v2.z;
}
static void vSub(vec* r, const vec v1, const vec v2)
{
    r->x = v1.x - v2.x; r->y = v1.y - v2.y; r->z = v1.z - v2.z;
}
static void vMulS(vec* r, const vec v1, const float s)
{
    r->x = v1.x * s; r->y = v1.y * s; r->z = v1.z * s;
}
static void vInv(vec* v)
{
    v->x = -v->x; v->y = -v->y; v->z = -v->z;
}
static void vNorm(vec* v)
{
    const float len = 1.f/sqrtf(v->x*v->x + v->y*v->y + v->z*v->z);
    v->x *= len; v->y *= len; v->z *= len;
}

static void mIdent(mat *m)
{
    memset(m, 0x0, sizeof(mat));
    m->m[0][0] = m->m[1][1] = m->m[2][2] = m->m[3][3] = 1.0f;
}
static void mMul(mat *r, const mat *a, const mat *b)
{
    mat tmp;
    for(int i = 0; i < 4; i++)
    {
        tmp.m[i][0] = (a->m[i][0]*b->m[0][0]) + (a->m[i][1]*b->m[1][0]) + (a->m[i][2]*b->m[2][0]) + (a->m[i][3]*b->m[3][0]);
        tmp.m[i][1] = (a->m[i][0]*b->m[0][1]) + (a->m[i][1]*b->m[1][1]) + (a->m[i][2]*b->m[2][1]) + (a->m[i][3]*b->m[3][1]);
        tmp.m[i][2] = (a->m[i][0]*b->m[0][2]) + (a->m[i][1]*b->m[1][2]) + (a->m[i][2]*b->m[2][2]) + (a->m[i][3]*b->m[3][2]);
        tmp.m[i][3] = (a->m[i][0]*b->m[0][3]) + (a->m[i][1]*b->m[1][3]) + (a->m[i][2]*b->m[2][3]) + (a->m[i][3]*b->m[3][3]);
    }
    memcpy(r, &tmp, sizeof(mat));
}
static void mSetRotY(mat *r, const float radians)
{
    const float s = sinf(radians);
    const float c = cosf(radians);
    r->m[0][0] = 1.f;
    r->m[0][1] = 0.f;
    r->m[0][2] = 0.f;
    r->m[0][3] = 0.f;
    r->m[1][0] = 0.f;
    r->m[1][1] = c;
    r->m[1][2] = -s;
    r->m[1][3] = 0.f;
    r->m[2][0] = 0.f;
    r->m[2][1] = s;
    r->m[2][2] = c;
    r->m[2][3] = 0.f;
}
static void mRotZ(mat *r, const float radians)
{
    const float s = sinf(radians);
    const float c = cosf(radians);
    const mat t = { c, -s, 0.f, 0.f,
                    s, c, 0.f, 0.f,
                    0.f, 0.f, 1.f, 0.f,
                    0.f, 0.f, 0.f, 1.f };
    mMul(r, &t, r);
}
static void mGetViewX(vec *r, const mat matrix)
{
    r->x = -matrix.m[0][0]; r->y = -matrix.m[1][0]; r->z = -matrix.m[2][0];
}
static void mGetViewY(vec *r, const mat matrix)
{
    r->x = -matrix.m[0][1]; r->y = -matrix.m[1][1]; r->z = -matrix.m[2][1];
}
static void mGetViewZ(vec *r, const mat matrix)
{
    r->x = -matrix.m[0][2]; r->y = -matrix.m[1][2]; r->z = -matrix.m[2][2];
}

#pragma GCC diagnostic ignored "-Wunused-result"

//*************************************
// VOXEL STRUCTURE
//*************************************
typedef struct
{
    GLuint vid; // vertex buff id
    GLuint iid;	// index buff id
} ESModel;
const GLfloat hud_vertices[] = {-1,1, -1,-1, 1,1, 1,-1,};
const GLubyte hud_indices[] = {0,1,2,2,3,1};
const GLsizeiptr hud_numind = 6;
ESModel mdlPlane;
SDL_Surface* sVoxel;
GLuint voxelmap;
SDL_Surface* sHud;
GLuint hudmap;

//*************************************
// SHADER
//*************************************
void makeHud();
void shadeHud(GLint* position, GLint* hud, GLint* look_pos, GLint* scale, GLint* view, GLint* voxels);
void flipHud()
{
    glBindTexture(GL_TEXTURE_2D, hudmap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sHud->w, sHud->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, sHud->pixels);
}

//*************************************
// UTILITY CODE
//*************************************
void esBind(const GLenum target, GLuint* buffer, const void* data, const GLsizeiptr datalen, const GLenum usage)
{
    glGenBuffers(1, buffer);
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
GLuint esReLoadTextureA(const GLuint w, const GLuint h, const unsigned char* data, const GLuint linear)
{
    /* Same signature as v5.0 so main.c does not need changes.
       The old body bound an uninitialised GLuint. Use the existing
       voxelmap and replace texels in-place instead of reallocating. */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, voxelmap);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)w, (GLsizei)h, GL_RGBA, GL_UNSIGNED_BYTE, data);
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
    return voxelmap;
}

//*************************************
// SHADER CODE
//*************************************
const GLchar* v0 =
	"#version 100\n"
	"attribute vec2 position;\n"
	"uniform vec2 scale;\n"
	"uniform vec3 view[3];\n"
	"varying vec3 ray_dir;\n"
	"varying vec2 screen_pos;\n"
	"void main()\n"
	"{\n"
		"vec3 forward = view[2];\n"
		"vec3 up      = view[1];\n"
		"vec3 right   = view[0];\n"
		"ray_dir = forward + (up * (position.y * scale.y)) + (right * (position.x * scale.x));\n"
		"screen_pos = vec2((position.x * 0.5) + 0.5, ((-position.y) * 0.5) + 0.5);\n"
		"gl_Position = vec4(position, 0.0, 1.0);\n"
	"}\n";

const GLchar* f0 =
	"#version 100\n"
	"precision highp float;\n"
	"varying vec3 ray_dir;\n"
	"varying vec2 screen_pos;\n"
	"uniform sampler2D voxels;\n"
	"uniform sampler2D hud;\n"
	"uniform vec3 look_pos;\n"

	/* Packing matches PTI(x,y,z) = z*16384 + y*128 + x
	   dumped into a 1024x2048 texture as texel(tx,ty) = voxels[tx*2048 + ty]
	   tx = z*8 + floor(y/16),  ty = (y%16)*128 + x */
	"vec4 voxel_at(vec3 p)\n"
	"{\n"
		"p = floor(p + 0.5);\n"
		"float yhi = floor(p.y * 0.0625);\n"
		"float ylo = p.y - yhi * 16.0;\n"
		"vec2 uv;\n"
		"uv.x = (p.z * 8.0 + yhi + 0.5) * 0.0009765625;\n"
		"uv.y = (ylo * 128.0 + p.x + 0.5) * 0.00048828125;\n"
		"return texture2D(voxels, uv);\n"
	"}\n"

	/* Far faces of the 128^3 canvas — this is the “background cube”. */
	"vec4 volume_bg(vec3 pos, float blue)\n"
	"{\n"
		"return vec4(screen_pos, blue, 1.0) * max(1.0 - distance(look_pos, pos) * 0.002590674, 0.6);\n"
	"}\n"

	"vec4 trace(vec3 ro, vec3 rd)\n"
	"{\n"
		"rd.x = abs(rd.x) < 1.0e-8 ? (rd.x < 0.0 ? -1.0e-8 : 1.0e-8) : rd.x;\n"
		"rd.y = abs(rd.y) < 1.0e-8 ? (rd.y < 0.0 ? -1.0e-8 : 1.0e-8) : rd.y;\n"
		"rd.z = abs(rd.z) < 1.0e-8 ? (rd.z < 0.0 ? -1.0e-8 : 1.0e-8) : rd.z;\n"

		"vec3 s = vec3(rd.x >= 0.0 ? 1.0 : -1.0, rd.y >= 0.0 ? 1.0 : -1.0, rd.z >= 0.0 ? 1.0 : -1.0);\n"
		"vec3 delta = abs(1.0 / rd);\n"
		"vec3 halfs = s * 0.5;\n"
		"vec3 pos = ro;\n"
		"vec3 side;\n"
		"vec4 albedo;\n"
		"float face = 1.0;\n"

		/* Slab vs AABB [-0.5, 127.5]. Same bounds the original unrolled. */
		"if (pos.x < -0.5 || pos.x > 127.5 || pos.y < -0.5 || pos.y > 127.5 || pos.z < -0.5 || pos.z > 127.5) {\n"
			"vec3 t1 = (-0.5 - pos) / rd;\n"
			"vec3 t2 = (127.5 - pos) / rd;\n"
			"vec3 tsm = min(t1, t2);\n"
			"vec3 tlg = max(t1, t2);\n"
			"float tenter = max(max(tsm.x, tsm.y), tsm.z);\n"
			"float texit  = min(min(tlg.x, tlg.y), tlg.z);\n"
			"if (texit < 0.0 || tenter > texit || tenter < 0.0) {\n"
				"return vec4(screen_pos, 0.5, 1.0);\n"
			"}\n"
			"pos += rd * tenter;\n"

			/* Which plane we entered on — sample that first interior cell. */
			"vec3 ad = abs(vec3(tenter) - tsm);\n"
			"side = ((s * 0.5 + 0.5) - (pos + 0.5 - floor(pos + 0.5))) / rd;\n"
			"if (ad.x <= ad.y && ad.x <= ad.z) {\n"
				"if (pos.y < -0.5 || pos.y > 127.5 || pos.z < -0.5 || pos.z > 127.5) {\n"
					"return vec4(screen_pos, 0.0, 1.0);\n"
				"}\n"
				"side.x = delta.x;\n"
				"face = 0.9;\n"
				"albedo = voxel_at(vec3(pos.x + halfs.x, pos.y, pos.z));\n"
			"} else if (ad.y <= ad.z) {\n"
				"if (pos.x < -0.5 || pos.x > 127.5 || pos.z < -0.5 || pos.z > 127.5) {\n"
					"return vec4(screen_pos, 0.0, 1.0);\n"
				"}\n"
				"side.y = delta.y;\n"
				"face = 1.0;\n"
				"albedo = voxel_at(vec3(pos.x, pos.y + halfs.y, pos.z));\n"
			"} else {\n"
				"if (pos.x < -0.5 || pos.x > 127.5 || pos.y < -0.5 || pos.y > 127.5) {\n"
					"return vec4(screen_pos, 0.0, 1.0);\n"
				"}\n"
				"side.z = delta.z;\n"
				"face = 0.8;\n"
				"albedo = voxel_at(vec3(pos.x, pos.y, pos.z + halfs.z));\n"
			"}\n"
			"if (albedo.a != 0.0) {\n"
				"return albedo * face;\n"
			"}\n"
		"} else {\n"
			"side = ((s * 0.5 + 0.5) - (pos + 0.5 - floor(pos + 0.5))) / rd;\n"
		"}\n"

		"for (int i = 0; i < 512; i++) {\n"
			"if (side.x < side.y && side.x < side.z) {\n"
				"pos += rd * side.x;\n"
				"side.y -= side.x;\n"
				"side.z -= side.x;\n"
				"side.x = delta.x;\n"
				"face = 0.9;\n"
				"if (pos.x + s.x > 127.7 || pos.x + s.x < -0.7) {\n"
					"return volume_bg(pos, 1.0);\n"
				"}\n"
				"albedo = voxel_at(vec3(pos.x + halfs.x, pos.y, pos.z));\n"
			"} else if (side.y < side.z) {\n"
				"pos += rd * side.y;\n"
				"side.x -= side.y;\n"
				"side.z -= side.y;\n"
				"side.y = delta.y;\n"
				"face = 1.0;\n"
				"if (pos.y + s.y > 127.7 || pos.y + s.y < -0.7) {\n"
					"return volume_bg(pos, 1.0);\n"
				"}\n"
				"albedo = voxel_at(vec3(pos.x, pos.y + halfs.y, pos.z));\n"
			"} else {\n"
				"pos += rd * side.z;\n"
				"side.x -= side.z;\n"
				"side.y -= side.z;\n"
				"side.z = delta.z;\n"
				"face = 0.8;\n"
				"if (pos.z + s.z > 127.7 || pos.z + s.z < -0.7) {\n"
					"return volume_bg(pos, 1.0);\n"
				"}\n"
				"albedo = voxel_at(vec3(pos.x, pos.y, pos.z + halfs.z));\n"
			"}\n"
			"if (albedo.a != 0.0) {\n"
				"return albedo * face;\n"
			"}\n"
		"}\n"
		"return volume_bg(pos, 1.0);\n"
	"}\n"

	"void main()\n"
	"{\n"
		"vec4 overlay = texture2D(hud, screen_pos);\n"
		"if (overlay.a == 1.0) {\n"
			"gl_FragColor = overlay;\n"
			"return;\n"
		"}\n"
		"gl_FragColor = trace(look_pos, ray_dir);\n"
		"gl_FragColor.rgb = mix(gl_FragColor.rgb, overlay.rgb, overlay.a);\n"
	"}\n";

///

GLuint shdHud;
GLint  shdHud_position;
GLint  shdHud_hud;
GLint  shdHud_look_pos;
GLint  shdHud_scale;
GLint  shdHud_view;
GLint  shdHud_voxels;

// leftover from the old raster voxel shader
// GLuint shdVoxel;
// GLint  shdVoxel_projection;
// GLint  shdVoxel_view;
// GLint  shdVoxel_position;
// GLint  shdVoxel_voxel;

//

void makeHud()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &v0, NULL);
    glCompileShader(vertexShader);

#ifdef __linux__
    GLint compiled;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE)
    {
        GLsizei log_length = 0;
        GLchar message[4096];
        glGetShaderInfoLog(vertexShader, 4096, &log_length, message);
        write(2, "Vertex error: ", 14);
        write(2, message, log_length);
        write(2, "\r\n", 2);
        exit(1);
    }
#endif

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &f0, NULL);
    glCompileShader(fragmentShader);

#ifdef __linux__
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE)
    {
        GLsizei log_length = 0;
        GLchar message[4096];
        glGetShaderInfoLog(fragmentShader, 4096, &log_length, message);
        write(2, "Fragment error: ", 16);
        write(2, message, log_length);
        write(2, "\r\n", 2);
        exit(1);
    }
#endif

    shdHud = glCreateProgram();
        glAttachShader(shdHud, vertexShader);
        glAttachShader(shdHud, fragmentShader);
    glLinkProgram(shdHud);
	glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

#ifdef __linux__
    GLint linked;
    glGetProgramiv(shdHud, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE)
    {
        GLsizei log_length = 0;
        GLchar message[4096];
        glGetProgramInfoLog(shdHud, 4096, &log_length, message);
        write(2, "Link error: ", 12);
        write(2, message, log_length);
        write(2, "\r\n", 2);
        exit(1);
    }
#endif

    shdHud_position   = glGetAttribLocation(shdHud,  "position");
    //<><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
    shdHud_voxels    = glGetUniformLocation(shdHud, "voxels");
    shdHud_look_pos   = glGetUniformLocation(shdHud, "look_pos");
    shdHud_scale      = glGetUniformLocation(shdHud, "scale");
    shdHud_view       = glGetUniformLocation(shdHud, "view");
    shdHud_hud       = glGetUniformLocation(shdHud, "hud");

    esBind(GL_ARRAY_BUFFER, &mdlPlane.vid, &hud_vertices, sizeof(hud_vertices), GL_STATIC_DRAW);
    esBind(GL_ELEMENT_ARRAY_BUFFER, &mdlPlane.iid, &hud_indices, sizeof(hud_indices), GL_STATIC_DRAW);
}
void shadeHud(GLint* position, GLint* hud, GLint* look_pos, GLint* scale, GLint* view, GLint* voxels)
{
    *position = shdHud_position;
    *hud = shdHud_hud;
    *look_pos = shdHud_look_pos;
    *scale = shdHud_scale;
    *view = shdHud_view;
    *voxels = shdHud_voxels;
    glUseProgram(shdHud);
}

//

#endif
