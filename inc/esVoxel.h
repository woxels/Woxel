/*
--------------------------------------------------
    James William Fletcher (github.com/mrbid)
         & Test_User 	   (notabug.org/test_user)
        	August 2023

	esVoxel.h v5.0 (glsl ray-tracing)
--------------------------------------------------

    A pretty good color converter: https://www.easyrgb.com/en/convert.php

    This version uses ray tracing in an OpenGL 1.0 shader to render a volume of voxels.

    Requires:
        - vec.h: https://gist.github.com/mrbid/77a92019e1ab8b86109bf103166bd04e
        - mat.h: https://gist.github.com/mrbid/cbc69ec9d99b0fda44204975fcbeae7c

*/

#ifndef AUX_H
#define AUX_H

#ifndef _WIN32
	#include <unistd.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>

#include "vec.h"
#include "mat.h"

#pragma GCC diagnostic ignored "-Wunused-result"

//*************************************
// VOXEL STRUCTURE
//*************************************
typedef struct
{
    GLuint vid; // vertex buff id
    GLuint iid;	// index buff id
    GLuint tid;	// uv/st texcoord buff id
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
GLuint esReLoadTextureA(const GLuint w, const GLuint h, const unsigned char* data, const GLuint linear)
{
    GLuint textureId;
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

		"ray_dir = forward + (up*(position.y * scale.y)) + (right*(position.x * scale.x));\n"

		"screen_pos = vec2((position.x * 0.5) + 0.5, ((-position.y) * 0.5) + 0.5);\n"
		"gl_Position = vec4(position, 1.0, 1.0);\n"
	"}\n";

const GLchar* f0 =
	"#version 100\n"
	"precision highp float;\n"

	"varying vec3 ray_dir;\n"
	"varying vec2 screen_pos;\n"

	"uniform sampler2D voxels;\n" // no dynamic array indexing? fine, dynamic not-array indexing it is
	"uniform sampler2D hud;\n"

	"uniform vec3 look_pos;\n" // for where to start

	"vec4 voxel_at(float z, float y, float x)\n"
	"{\n"
		"x = floor(x + 0.5);\n"
		"y = floor(y + 0.5);\n"
		"z = floor(z + 0.5);\n"

		"float ysplithigh = floor(y / 16.0);\n" // can inverse 16.0 as well v
		"float ysplitlow = y - (ysplithigh * 16.0);\n"

		"vec2 index;\n"
		"index.x = ((x * 8.0) + ysplithigh) / 1024.0;\n" // TODO: rm the division
		"index.y = ((ysplitlow * 128.0) + z) / 2048.0;\n" // ^

		"return texture2D(voxels, index);\n"
	"}\n"

	"void ray()\n"
	"{\n"
		"vec3 pos = look_pos;\n"
		"vec4 maxdist;\n"
		"int index;\n"

		"vec3 dir = vec3("
			"ray_dir.x >= 0.0 ? 1.0 : -1.0,"
			"ray_dir.y >= 0.0 ? 1.0 : -1.0,"
			"ray_dir.z >= 0.0 ? 1.0 : -1.0"
		");\n"

		"vec3 dir2 = vec3("
			"dir.x * 0.5,"
			"dir.y * 0.5,"
			"dir.z * 0.5"
		");\n"

		"vec3 dist_per = vec3("
			"dir.x / ray_dir.x,"
			"dir.y / ray_dir.y,"
			"dir.z / ray_dir.z"
		");\n"

		"vec3 dist_remaining = vec3(" // maybe move initialization to an else in this next section
			"(((dir.x + 1.0) * 0.5) - ((pos.x + 0.5) - floor(pos.x  + 0.5))) / ray_dir.x,"
			"(((dir.y + 1.0) * 0.5) - ((pos.y + 0.5) - floor(pos.y  + 0.5))) / ray_dir.y,"
			"(((dir.z + 1.0) * 0.5) - ((pos.z + 0.5) - floor(pos.z  + 0.5))) / ray_dir.z"
		");\n"

		"if (pos.x < -0.5 || pos.x > 127.5 || pos.y < -0.5 || pos.y > 127.5 || pos.z < -0.5 || pos.z > 127.5) {\n"
			"if (pos.x < -0.5) {\n"
				"maxdist.x = -((pos.x + 0.5) / ray_dir.x);\n"
				"if (maxdist.x < 0.0) {\n"
					"gl_FragColor = vec4(screen_pos, 0.5, 1.0);\n"
					"return;\n"
				"}\n"
			"} else if (pos.x > 127.5) {\n"
				"maxdist.x = -((pos.x - 127.5) / ray_dir.x);\n"
				"if (maxdist.x < 0.0) {\n"
					"gl_FragColor = vec4(screen_pos, 0.5, 1.0);\n"
					"return;\n"
				"}\n"
			"} else {\n"
				"maxdist.x = 0.0;\n"
			"}\n"

			"if (pos.y < -0.5) {\n"
				"maxdist.y = -((pos.y + 0.5) / ray_dir.y);\n"
				"if (maxdist.y < 0.0) {\n"
					"gl_FragColor = vec4(screen_pos, 0.5, 1.0);\n"
					"return;\n"
				"}\n"
			"} else if (pos.y > 127.5) {\n"
				"maxdist.y = -((pos.y - 127.5) / ray_dir.y);\n"
				"if (maxdist.y < 0.0) {\n"
					"gl_FragColor = vec4(screen_pos, 0.5, 1.0);\n"
					"return;\n"
				"}\n"
			"} else {\n"
				"maxdist.y = 0.0;\n"
			"}\n"

			"if (pos.z < -0.5) {\n"
				"maxdist.z = -((pos.z + 0.5) / ray_dir.z);\n"
				"if (maxdist.z < 0.0) {\n"
					"gl_FragColor = vec4(screen_pos, 0.5, 1.0);\n"
					"return;\n"
				"}\n"
			"} else if (pos.z > 127.5) {\n"
				"maxdist.z = -((pos.z - 127.5) / ray_dir.z);\n"
				"if (maxdist.z < 0.0) {\n"
					"gl_FragColor = vec4(screen_pos, 0.5, 1.0);\n"
					"return;\n"
				"}\n"
			"} else {\n"
				"maxdist.z = 0.0;\n"
			"}\n"

			"maxdist.w = maxdist.x;\n"
			"if (maxdist.w < maxdist.y) {\n"
				"maxdist.w = maxdist.y;\n"
			"}\n"
			"if (maxdist.w < maxdist.z) {\n"
				"maxdist.w = maxdist.z;\n"
			"}\n"

			"pos.x += maxdist.w * ray_dir.x;\n"
			"pos.y += maxdist.w * ray_dir.y;\n"
			"pos.z += maxdist.w * ray_dir.z;\n"

			"dist_remaining = vec3("
				"(((dir.x + 1.0) * 0.5) - ((pos.x + 0.5) - floor(pos.x  + 0.5))) / ray_dir.x,"
				"(((dir.y + 1.0) * 0.5) - ((pos.y + 0.5) - floor(pos.y  + 0.5))) / ray_dir.y,"
				"(((dir.z + 1.0) * 0.5) - ((pos.z + 0.5) - floor(pos.z  + 0.5))) / ray_dir.z"
			");\n"

			"if (maxdist.w == maxdist.x) {\n"
				"if (pos.y < -0.5 || pos.y > 127.5 || pos.z < -0.5 || pos.z > 127.5) {\n" // edge cases, literally, might have issues with float inaccuracies, but they should be nearly impossible to reach...
					"gl_FragColor = vec4(screen_pos, 0.0, 1.0);\n"
					"return;\n"
				"}\n"

				"dist_remaining.x = dist_per.x;\n"

				"vec4 color = voxel_at(pos.x + dir2.x, pos.y, pos.z);\n"
				"if (color.a != 0.0) {\n"
					"gl_FragColor = color;\n"
					"return;\n"
				"}\n"
			"} else if (maxdist.w == maxdist.y) {\n"
				"if (pos.x < -0.5 || pos.x > 127.5 || pos.z < -0.5 || pos.z > 127.5) {\n"
					"gl_FragColor = vec4(screen_pos, 0.0, 1.0);\n"
					"return;\n"
				"}\n"

				"dist_remaining.y = dist_per.y;\n"

				"vec4 color = voxel_at(pos.x, pos.y + dir2.y, pos.z);\n"
				"if (color.a != 0.0) {\n"
					"gl_FragColor = color;\n"
					"return;\n"
				"}\n"
			"} else {\n"
				"if (pos.x < -0.5 || pos.x > 127.5 || pos.y < -0.5 || pos.y > 127.5) {\n"
					"gl_FragColor = vec4(screen_pos, 0.0, 1.0);\n"
					"return;\n"
				"}\n"

				"dist_remaining.z = dist_per.z;\n"

				"vec4 color = voxel_at(pos.x, pos.y, pos.z + dir2.z);\n"
				"if (color.a != 0.0) {\n"
					"gl_FragColor = color;\n"
					"return;\n"
				"}\n"
			"}\n"
		"}\n"



		"vec3 checkpos;\n"
		"float multiplier;\n"
		"for(int i = 0; i < 512; i++){\n"
			"if (dist_remaining.x < dist_remaining.y && dist_remaining.x < dist_remaining.z) {\n"
				"pos.x += ray_dir.x * dist_remaining.x;\n"
				"pos.y += ray_dir.y * dist_remaining.x;\n"
				"pos.z += ray_dir.z * dist_remaining.x;\n"

				"dist_remaining.y -= dist_remaining.x;\n"
				"dist_remaining.z -= dist_remaining.x;\n"

				"dist_remaining.x = dist_per.x;\n"

				"if (pos.x + dir.x > 127.7 || pos.x + dir.x < -0.7) {\n"
					"gl_FragColor = vec4(screen_pos, 1.0, 1.0) * max((1.0-(distance(look_pos, pos) * 0.002590674)), 0.6);\n"
					"return;\n"
				"}\n"

				"checkpos = vec3(pos.x + dir2.x, pos.y, pos.z);\n"
				"multiplier = 0.9;\n"
			"} else if (dist_remaining.y < dist_remaining.z) {\n"
				"pos.x += ray_dir.x * dist_remaining.y;\n"
				"pos.y += ray_dir.y * dist_remaining.y;\n"
				"pos.z += ray_dir.z * dist_remaining.y;\n"

				"dist_remaining.x -= dist_remaining.y;\n"
				"dist_remaining.z -= dist_remaining.y;\n"

				"dist_remaining.y = dist_per.y;\n"

				"if (pos.y + dir.y > 127.7 || pos.y + dir.y < -0.7) {\n"
					"gl_FragColor = vec4(screen_pos, 1.0, 1.0) * max((1.0-(distance(look_pos, pos) * 0.002590674)), 0.6);\n"
					"return;\n"
				"}\n"

				"checkpos = vec3(pos.x, pos.y + dir2.y, pos.z);\n"
				"multiplier = 1.0;\n"
			"} else {\n"
				"pos.x += ray_dir.x * dist_remaining.z;\n"
				"pos.y += ray_dir.y * dist_remaining.z;\n"
				"pos.z += ray_dir.z * dist_remaining.z;\n"

				"dist_remaining.x -= dist_remaining.z;\n"
				"dist_remaining.y -= dist_remaining.z;\n"

				"dist_remaining.z = dist_per.z;\n"

				"if (pos.z + dir.z > 127.7 || pos.z + dir.z < -0.7) {\n"
					"gl_FragColor = vec4(screen_pos, 1.0, 1.0) * max((1.0-(distance(look_pos, pos) * 0.002590674)), 0.6);\n"
					"return;\n"
				"}\n"

				"checkpos = vec3(pos.x, pos.y, pos.z + dir2.z);\n"
				"multiplier = 0.8;\n"
			"}\n"

			"vec4 color = voxel_at(checkpos.x, checkpos.y, checkpos.z);\n"
			"if (color.a != 0.0) {\n"
				"gl_FragColor = color * multiplier;\n"
				"return;\n"
			"}\n"
		"}\n"
	"}\n"

	"void main()\n"
	"{\n"
		"vec4 color = texture2D(hud, screen_pos);\n"
		"if (color.a != 1.0) {\n"
			"ray();\n"
			"gl_FragColor.rgb = mix(gl_FragColor.rgb, color.rgb, color.a);\n"
		"} else {\n"
			"gl_FragColor = color;\n"
		"}\n"
	"}\n";

///

GLuint shdHud;
GLint  shdHud_position;
GLint  shdHud_hud;
GLint  shdHud_look_pos;
GLint  shdHud_scale;
GLint  shdHud_view;
GLint  shdHud_voxels;

GLuint shdVoxel;
GLint  shdVoxel_projection;
GLint  shdVoxel_view;
GLint  shdVoxel_position;
GLint  shdVoxel_voxel;

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
        GLchar message[1024*1024];
        glGetShaderInfoLog(vertexShader, 1024*1024, &log_length, message);
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
        GLchar message[1024*1024];
        glGetShaderInfoLog(fragmentShader, 1024*1024, &log_length, message);
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
