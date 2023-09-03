/*
--------------------------------------------------
    James William Fletcher (github.com/mrbid)
        August 2023
--------------------------------------------------
    C & SDL / OpenGL ES2 / GLSL ES
    Colour Converter: https://www.easyrgb.com
*/

#pragma GCC diagnostic ignored "-Wtrigraphs"

#include <time.h>
#include <zlib.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>

#ifdef __linux__
    #include <sys/time.h>
    #include <locale.h>
#endif

#include "esVoxel.h"

#define uint GLuint
#define sint GLint
#define uchar unsigned char

// render state id's
GLint projection_id;
GLint view_id;
GLint position_id;
GLint voxel_id;
GLint texcoord_id;
GLint sampler_id;

// render state matrices
mat projection;
mat view;

//*************************************
// globals
//*************************************
const char appTitle[] = "Woxel"; // or loxel?
const char appVersion[] = "v1.0";
char openTitle[256]="Untitled";
char *basedir, *appdir;
SDL_Window* wnd;
SDL_GLContext glc;
SDL_Surface* s_icon = NULL;
int mx=0, my=0, lx=0, ly=0, md=0;
int winw = 1024, winh = 768;
int winw2 = 512, winh2 = 384;
float ww, wh;
float aspect, t = 0.f;
uint g_fps = 0;
uint ks[10] = {0};      // keystate
uint focus_mouse = 0;   // mouse lock
uint showhud = 1;       // hud visibility
float vdist = 4096.f;   // view distance
vec ipp;                // inverse player position
vec look_dir;           // camera look direction
int lray = 0;           // pointed at node index
float ptt = 0.f;        // place timing trigger (for repeat place)
float dtt = 0.f;        // delete timing trigger (for repeat delete)
float rtt = 0.f;        // replace timing trigger (for repeat replace)
float rrsp = 0.3f;      // repeat replace speed
uint fks = 0;           // F-Key state (fast mode toggle)
float bigc = 0.f;       // big cursor start time
Uint32 sclr = 0;        // selected color
uint load_state = 0;    // loaded from appdir or custom path?
uint mirror = 0;        // mirror brush state
vec ghp;                // global ray hit position

//*************************************
// game state functions
//*************************************
// game data (for fast save and load)
#define max_voxels 2097152 // 2.097 million
typedef struct
{
    vec pp;     // player position
    vec pb;     // place block pos
    float sens; // mouse sensitivity
    float xrot; // camera x-axis rotation
    float yrot; // camera y-axis rotation
    float st;   // selected color
    float ms;   // player move speed
    float cms;  // custom move speed (high)
    float lms;  // custom move speed (low)
    uchar plock;// pitchlock on/off toggle
    uint colors[39]; // color palette (7 system, 32 user)
    uchar voxels[max_voxels]; // x,y,z,w (w = color_id)
}
game_state;
game_state g;
// point to index & vice-versa
uint PTI(const uchar x, const uchar y, const uchar z) // 0-128
{
    return (z * 16384) + (y * 128) + x;
}
// vec ITP(const float i)
// {
//     const float z = i * 6.1035156E-5f; // i / 16384.f;
//     const float a = floorf(z);
//     const float b = (z-floorf(z)) * 128.f;
//     const float c = (b-floorf(b)) * 128.f;
//     return (vec){roundf(c),roundf(b),roundf(a)};
// }
void defaultState(const uint type)
{
    g.sens = 0.003f;
    g.xrot = 0.f;
    g.yrot = 1.57f;
    g.pp = (vec){-64.f, 130.f, -64.f};
    if(type == 0){g.ms = 37.2f;}
    g.st = 8.f;
    g.pb = (vec){0.f, 0.f, 0.f, -1.f};
    if(type == 0){g.lms = 37.2f, g.cms = 74.4f;}
    g.plock = 0;
}
uint placedVoxels()
{
    uint c = 0;
    for(uint i = 0; i < max_voxels; i++)
        if(g.voxels[i] != 0){c++;}
    return c;
}
uint isInBounds(const vec p)
{
    if(p.x < -0.5f || p.y < -0.5f || p.z < -0.5f || p.x > 127.5f || p.y > 127.5f || p.z > 127.5f){return 0;}
    return 1;
}
// uint forceInBounds(vec p)
// {
//     if(p.x < -0.5f){p.x = -0.5f;}
//     if(p.y < -0.5f){p.y = -0.5f;}
//     if(p.z < -0.5f){p.z = -0.5f;}
//     if(p.x > 127.5f){p.x = 127.5f;}
//     if(p.y > 127.5f){p.y = 127.5f;}
//     if(p.z > 127.5f){p.z = 127.5f;}
//     return 1;
// }
uint PTIB(const uchar x, const uchar y, const uchar z) // 0-128
{
    uint r = (z * 16384) + (y * 128) + x;
    if(r > max_voxels-1){r = max_voxels-1;}
    return r;
}

//*************************************
// ray functions
//*************************************
int ray(vec* hit_pos, vec* hit_vec, const vec start_pos) // the look vector is a global
{
    // might need exclude conditions for obviously bogus rays to avoid those 2048 steps
    vec inc;
    vMulS(&inc, look_dir, 0.015625f); // 0.0625f
    int hit = -1;
    vec rp = start_pos;
    for(uint i = 0; i < 8192; i++) // 2048
    {
        vAdd(&rp, rp, inc);
        if(isInBounds(rp) == 0){continue;} // break;
        vec rb;
        rb.x = roundf(rp.x);
        rb.y = roundf(rp.y);
        rb.z = roundf(rp.z);
        const uint vi = PTI(rb.x, rb.y, rb.z);
        //printf("ray: %u: %f %f %f\n", vi, rb.x, rb.y, rb.z);
        if(g.voxels[vi] != 0)
        {
            *hit_vec = (vec){rp.x-rb.x, rp.y-rb.y, rp.z-rb.z};
            *hit_pos = (vec){rb.x, rb.y, rb.z};
            hit = vi;
            break;
        }
        if(hit > -1){break;}
    }
    return hit;
}
void traceViewPath(const uint face)
{
    g.pb.w = -1.f; // pre-set as failed
    vec rp;
    lray = ray(&ghp, &rp, ipp);
    //printf("tVP: %u: %f %f %f - %f %f %f\n", lray, ghp.x, ghp.y, ghp.z, ipp.x, ipp.y, ipp.z);
    if(lray > -1 && face == 1)
    {
        vNorm(&rp);
        vec diff = rp;
        rp = ghp;

        vec fd = diff;
        fd.x = fabsf(diff.x);
        fd.y = fabsf(diff.y);
        fd.z = fabsf(diff.z);
        if     (fd.x > fd.y && fd.x > fd.z){diff.y = 0.f, diff.z = 0.f;}
        else if(fd.y > fd.x && fd.y > fd.z){diff.x = 0.f, diff.z = 0.f;}
        else if(fd.z > fd.x && fd.z > fd.y){diff.x = 0.f, diff.y = 0.f;}
        diff.x = roundf(diff.x);
        diff.y = roundf(diff.y);
        diff.z = roundf(diff.z);

        rp.x += diff.x;
        rp.y += diff.y;
        rp.z += diff.z;

        if(vSumAbs(diff) == 1.f)
        {
            g.pb = rp;
            g.pb.w = 1.f; // success
        }
    }
}

//*************************************
// utility functions
//*************************************
void timestamp(char* ts){const time_t tt = time(0);strftime(ts, 16, "%H:%M:%S", localtime(&tt));}
float fTime(){return ((float)SDL_GetTicks())*0.001f;}
#ifdef __linux__
    uint64_t microtime()
    {
        struct timeval tv;
        struct timezone tz;
        memset(&tz, 0, sizeof(struct timezone));
        gettimeofday(&tv, &tz);
        return 1000000 * tv.tv_sec + tv.tv_usec;
    }
#endif
void loadColors(const char* file)
{
    memset(g.colors, 0, 39*sizeof(uint));
    g.colors[0] = 16448250;
    g.colors[1] = 16711680;
    g.colors[2] = 4194304;
    g.colors[3] = 65280;
    g.colors[4] = 16384;
    g.colors[5] = 255;
    g.colors[6] = 64;
    FILE* f = fopen(file, "r");
    if(f)
    {
        uint lino = 0;
        char line[8];
        while(fgets(line, 8, f) != NULL)
        {
            uint val;
            if(sscanf(line, "#%x", &val) == 1)
            {
                g.colors[7+lino] = val;
                lino++;
                if(lino > 31){break;}
            }
            else if(sscanf(line, "%x", &val) == 1)
            {
                g.colors[7+lino] = val;
                lino++;
                if(lino > 31){break;}
            }
        }
        fclose(f);
        char tmp[16];
        timestamp(tmp);
        printf("[%s] Custom color palette applied to project \"%s\".\n", tmp, openTitle);
    }
}

//*************************************
// save and load functions
//*************************************
void saveState(const char* name, const char* fne, const uint fs)
{
#ifdef __linux__
    setlocale(LC_NUMERIC, "");
    const uint64_t st = microtime();
#endif
    char file[256];
    sprintf(file, "%s%s.wox.gz%s", appdir, name, fne);
    if(fs == 0){sprintf(file, "%s%s.wox.gz%s", appdir, name, fne);}
    else{sprintf(file, "%s", name);}
    gzFile f = gzopen(file, "wb9hR");
    if(f != Z_NULL)
    {
        const size_t ws = sizeof(game_state);
        if(gzwrite(f, &g, ws) != ws)
        {
            char tmp[16];
            timestamp(tmp);
            printf("[%s] Save corrupted.\n", tmp);
        }
        gzclose(f);
        char tmp[16];
        timestamp(tmp);
#ifndef __linux__
        printf("[%s] Saved %'u voxels.\n", tmp, placedVoxels());
#else
        printf("[%s] Saved %'u voxels. (%'lu μs)\n", tmp, placedVoxels(), microtime()-st);
#endif
    }
}
uint loadState(const char* name, const uint fs)
{
#ifdef __linux__
    setlocale(LC_NUMERIC, "");
    const uint64_t st = microtime();
#endif
    char file[1024];
    if(fs == 0){sprintf(file, "%s%s.wox.gz", appdir, name);}
    else{sprintf(file, "%s", name);}
    gzFile f = gzopen(file, "rb");
    if(f != Z_NULL)
    {
        int gr = gzread(f, &g, sizeof(game_state));
        gzclose(f);
        fks = (g.ms == g.cms); // update F-Key State
        char tmp[16];
        timestamp(tmp);
#ifndef __linux__
        printf("[%s] Loaded %u voxels\n", tmp, placedVoxels());
#else
        printf("[%s] Loaded %'u voxels. (%'lu μs)\n", tmp, placedVoxels(), microtime()-st);
#endif
        return 1;
    }
    return 0;
}

// voxel face rendering for ply
uint fw_mx(FILE* f, float x, float y, float z, uchar r, uchar g, uchar b)
{
    x -= 64.f, y -= 64.f;
    const float s = 0.5f;
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y-s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y+s, z-s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y-s, z-s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y-s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y+s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y+s, z-s, r, g, b);
    return 4;
}
uint fw_px(FILE* f, float x, float y, float z, uchar r, uchar g, uchar b)
{
    x -= 64.f, y -= 64.f;
    const float s = 0.5f;
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y+s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y-s, z-s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y+s, z-s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y+s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y-s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y-s, z-s, r, g, b);
    return 4;
}
//
uint fw_my(FILE* f, float x, float y, float z, uchar r, uchar g, uchar b)
{
    x -= 64.f, y -= 64.f;
    const float s = 0.5f;
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y-s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y-s, z-s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y-s, z-s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y-s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y-s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y-s, z-s, r, g, b);
    return 4;
}
uint fw_py(FILE* f, float x, float y, float z, uchar r, uchar g, uchar b)
{
    x -= 64.f, y -= 64.f;
    const float s = 0.5f;
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y+s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y+s, z-s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y+s, z-s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y+s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y+s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y+s, z-s, r, g, b);
    return 4;
}
//
uint fw_mz(FILE* f, float x, float y, float z, uchar r, uchar g, uchar b)
{
    x -= 64.f, y -= 64.f;
    const float s = 0.5f;
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y+s, z-s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y-s, z-s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y+s, z-s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y+s, z-s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y-s, z-s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y-s, z-s, r, g, b);
    return 4;
}
uint fw_pz(FILE* f, float x, float y, float z, uchar r, uchar g, uchar b)
{
    x -= 64.f, y -= 64.f;
    const float s = 0.5f;
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y+s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y-s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y+s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y+s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x-s, y-s, z+s, r, g, b);
    fprintf(f, "%g %g %g -1 0 0 %u %u %u\n", x+s, y-s, z+s, r, g, b);
    return 4;
}

//*************************************
// more utility functions
//*************************************
void printAttrib(SDL_GLattr attr, char* name)
{
    int i;
    SDL_GL_GetAttribute(attr, &i);
    printf("%s: %i\n", name, i);
}
SDL_Surface* SDL_RGBA32Surface(Uint32 w, Uint32 h)
{
    return SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
}
void drawHud(const uint type);
void doPerspective()
{
    glViewport(0, 0, winw, winh);
    SDL_FreeSurface(sHud);
    sHud = SDL_RGBA32Surface(winw, winh);
    drawHud(0);
    hudmap = esLoadTextureA(winw, winh, sHud->pixels, 0);
    ww = (float)winw;
    wh = (float)winh;
    mIdent(&projection);
    mPerspective(&projection, 60.0f, ww / wh, 1.0f, vdist);
    glUniformMatrix4fv(projection_id, 1, GL_FALSE, (float*)&projection.m[0][0]);
}
uint insideFrustum(const float x, const float y, const float z)
{
    const float xm = x+g.pp.x, ym = y+g.pp.y, zm = z+g.pp.z;
    return (xm*look_dir.x) + (ym*look_dir.y) + (zm*look_dir.z) > 0.f; // check the angle
}
SDL_Surface* surfaceFromData(const Uint32* data, Uint32 w, Uint32 h)
{
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    memcpy(s->pixels, data, s->pitch*h);
    return s;
}
Uint32 getpixel(const SDL_Surface *surface, Uint32 x, Uint32 y)
{
    const Uint8 *p = (Uint8*)surface->pixels + y * surface->pitch + x * surface->format->BytesPerPixel;
    return *(Uint32*)p;
}
void setpixel(SDL_Surface *surface, Uint32 x, Uint32 y, Uint32 pix)
{
    const Uint8* pixel = (Uint8*)surface->pixels + (y * surface->pitch) + (x * surface->format->BytesPerPixel);
    *((Uint32*)pixel) = pix;
}
void replaceColour(SDL_Surface* surf, SDL_Rect r, Uint32 old_color, Uint32 new_color)
{
    const Uint32 max_y = r.y+r.h;
    const Uint32 max_x = r.x+r.w;
    for(Uint32 y = r.y; y < max_y; ++y)
        for(Uint32 x = r.x; x < max_x; ++x)
            if(getpixel(surf, x, y) == old_color){setpixel(surf, x, y, new_color);}
}
void updateSelectColor()
{
    const uint tu = g.colors[(uint)g.st-1];
    sclr = SDL_MapRGB(sHud->format, (tu & 0x00FF0000) >> 16,
                                    (tu & 0x0000FF00) >> 8,
                                     tu & 0x000000FF);
}

//*************************************
// Simple Font
//*************************************
int drawText(SDL_Surface* o, const char* s, Uint32 x, Uint32 y, Uint8 colour)
{
    static const Uint8 font_map[] = {255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,0,0,0,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,255,255,255,255,255,0,0,0,0,255,255,255,0,255,0,0,0,0,0,255,0,0,0,0,0,255,255,0,0,0,0,0,255,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,0,0,255,255,255,255,0,0,255,0,0,0,0,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,0,255,255,0,0,0,0,0,255,0,0,0,0,255,0,0,0,0,0,0,255,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,255,255,255,255,0,0,255,255,0,0,255,0,0,255,255,255,0,0,255,255,255,0,0,255,255,255,255,255,0,0,255,255,255,0,0,255,0,0,255,255,255,255,0,0,0,0,255,0,0,255,0,0,255,255,255,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,255,0,0,255,255,255,255,255,255,0,0,255,255,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,255,255,255,255,0,0,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,255,0,0,255,255,0,0,0,0,255,255,0,0,255,255,255,0,0,255,255,0,0,255,255,255,0,0,255,255,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,255,0,0,255,0,0,255,0,0,255,255,0,0,0,0,255,255,255,255,0,0,255,255,255,0,0,0,0,255,255,255,0,0,255,255,255,0,0,255,255,255,255,255,0,0,255,255,255,0,0,255,0,0,255,255,255,255,0,0,0,0,0,0,255,255,0,0,255,255,255,0,0,0,0,255,0,0,0,0,0,0,0,0,255,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,255,0,0,255,255,255,255,255,255,0,0,255,255,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,255,255,0,0,255,0,0,0,0,255,255,0,0,0,0,255,255,255,255,0,0,0,255,0,0,0,0,255,0,0,0,0,0,255,255,0,0,0,0,255,0,0,0,0,0,255,0,0,0,0,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,255,0,0,0,255,0,0,0,0,0,255,255,0,0,0,0,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,0,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,0,0,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,255,255,0,0,255,0,0,255,255,255,255,255,0,0,255,255,255,255,0,0,255,255,0,0,0,255,255,0,0,255,255,255,0,0,255,255,255,255,255,255,255,0,0,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,0,255,255,255,255,255,255,255,255,0,0,255,0,0,255,0,0,0,0,0,255,0,0,255,255,255,255,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,255,0,0,255,255,255,255,0,0,0,0,0,255,255,255,0,0,255,255,255,0,255,0,0,0,0,255,0,0,0,255,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,255,0,0,0,0,0,255,255,255,0,0,255,255,0,0,255,255,255,0,0,255,0,0,0,0,255,255,0,0,255,0,0,255,0,0,255,255,255,0,0,255,255,255,0,0,0,0,255,255,255,0,0,0,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,0,0,0,0,255,0,0,255,0,0,0,0,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,0,0,0,0,255,255,255,0,0,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,0,0,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,255,255,255,0,0,0,0,255,255,0,0,255,0,0,255,255,255,255,0,0,255,255,255,0,0,0,255,255,0,255,0,0,255,255,0,0,0,0,255,0,0,0,0,0,255,255,255,255,0,0,255,255,0,0,0,0,255,0,0,255,255,0,0,0,0,255,255,255,255,255,0,255,255,255,255,255,255,255,255,0,0,255,0,0,255,0,0,255,255,0,0,0,0,255,255,255,255,0,0,255,255,255,0,0,0,0,255,255,255,0,0,255,255,255,0,0,255,255,0,0,0,0,0,255,255,255,0,0,255,0,0,255,255,255,255,0,0,0,0,0,255,255,255,0,0,255,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,0,0,255,255,255,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,255,0,0,255,0,0,0,0,255,255,0,0,0,0,0,0,0,0,255,255,255,0,0,255,255,255,255,0,0,255,255,255,0,0,0,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,0,0,255,255,0,0,255,255,0,0,255,0,0,0,0,255,0,0,255,0,0,255,0,0,255,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,255,0,0,255,255,0,0,255,0,0,255,255,255,0,0,255,255,255,255,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,255,255,0,0,255,255,255,0,0,255,0,0,255,255,0,0,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,0,0,255,255,255,0,0,0,0,255,255,255,0,0,255,255,255,0,0,255,255,255,0,0,0,0,255,255,255,0,0,255,0,0,255,255,255,255,0,0,0,0,0,0,255,255,0,0,255,255,255,0,255,255,255,255,255,255,0,0,0,255,255,255,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,0,0,255,255,255,0,0,0,0,255,0,0,255,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,0,0,255,0,0,0,0,255,255,0,0,0,255,255,0,0,0,255,255,0,0,0,0,255,255,255,0,0,255,255,0,0,0,255,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,255,255,0,0,0,0,255,255,255,255,0,0,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,0,0,0,0,255,0,0,0,0,0,0,0,0,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,0,0,255,255,0,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,0,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,255,0,0,255,255,0,0,255,0,0,255,255,255,0,0,255,255,255,0,0,255,255,255,0,0,0,0,255,255,255,0,0,255,0,0,255,255,255,255,0,0,0,0,255,0,0,255,0,0,255,255,255,0,255,255,255,255,255,255,0,0,0,255,255,255,255,0,0,0,255,255,255,0,0,0,0,255,255,255,255,0,0,255,255,255,0,0,0,0,255,255,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,0,0,255,255,0,0,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,255,255,0,0,0,0,255,255,255,255,0,0,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,0,0,0,0,255,0,0,255,0,0,0,0,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,0,0,255,255,0,0,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,0,0,255,255,0,0,255,0,0,255,0,0,255,255,255,255,0,0,255,255,0,0,255,255,255,0,0,255,0,0,255,255,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,0,0,0,0,0,0,255,255,255,0,255,255,255,255,255,255,255,0,0,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,255,0,0,255,255,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,255,255,255,255,0,255,0,0,0,0,0,255,0,0,255,255,255,255,255,0,0,0,0,0,255,0,0,255,255,255,0,0,0,0,0,0,0,255,255,255,0,0,255,255,255,0,0,0,0,0,255,255,255,0,0,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,255,0,0,0,0,0,255,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,255,255,0,0,255,0,0,0,0,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,255,255,0,0,0,255,0,0,0,0,0,255,255,0,0,255,255,255,0,0,255,255,0,0,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,255,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,255,255,255,255,0,0,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,255,0,0,0,0,255,255,255,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};
    static const Uint32 m = 1;
    static SDL_Surface* font_black = NULL;
    static SDL_Surface* font_white = NULL;
    static SDL_Surface* font_aqua = NULL;
    static SDL_Surface* font_gold = NULL;
    static SDL_Surface* font_cia = NULL;
    // static SDL_Surface* font_red = NULL;
    // static SDL_Surface* font_green = NULL;
    // static SDL_Surface* font_blue = NULL;
    if(font_black == NULL)
    {
        font_black = SDL_RGBA32Surface(380, 11);
        for(int y = 0; y < font_black->h; y++)
        {
            for(int x = 0; x < font_black->w; x++)
            {
                const Uint8 c = font_map[(y*font_black->w)+x];
                setpixel(font_black, x, y, SDL_MapRGBA(font_black->format, c, c, c, 255-c));
            }
        }
        font_white = SDL_RGBA32Surface(380, 11);
        SDL_BlitSurface(font_black, &font_black->clip_rect, font_white, &font_white->clip_rect);
        replaceColour(font_white, font_white->clip_rect, 0xFF000000, 0xFFFFFFFF);
        font_aqua = SDL_RGBA32Surface(380, 11);
        SDL_BlitSurface(font_black, &font_black->clip_rect, font_aqua, &font_aqua->clip_rect);
        replaceColour(font_aqua, font_aqua->clip_rect, 0xFF000000, 0xFFFFFF00);
        font_gold = SDL_RGBA32Surface(380, 11);
        SDL_BlitSurface(font_black, &font_black->clip_rect, font_gold, &font_gold->clip_rect);
        replaceColour(font_gold, font_gold->clip_rect, 0xFF000000, 0xFF00BFFF);
        font_cia = SDL_RGBA32Surface(380, 11);
        SDL_BlitSurface(font_black, &font_black->clip_rect, font_cia, &font_cia->clip_rect);
        replaceColour(font_cia, font_cia->clip_rect, 0xFF000000, 0xFF97C920);
        // #20c997(0xFF97C920) #00FF41(0xFF41FF00) #61d97c(0xFF7CD961) #51d4e9(0xFFE9D451)
        // font_red = SDL_RGBA32Surface(380, 11);
        // SDL_BlitSurface(font_black, &font_black->clip_rect, font_red, &font_red->clip_rect);
        // replaceColour(font_red, font_red->clip_rect, 0xFF000000, 0xFF0000FF);
        // font_green = SDL_RGBA32Surface(380, 11);
        // SDL_BlitSurface(font_black, &font_black->clip_rect, font_green, &font_green->clip_rect);
        // replaceColour(font_green, font_green->clip_rect, 0xFF000000, 0xFF00FF00);
        // font_blue = SDL_RGBA32Surface(380, 11);
        // SDL_BlitSurface(font_black, &font_black->clip_rect, font_blue, &font_blue->clip_rect);
        // replaceColour(font_blue, font_blue->clip_rect, 0xFF000000, 0xFFFF0000);


    }
    if(s[0] == '*' && s[1] == 'K') // signal cleanup
    {
        SDL_FreeSurface(font_black);
        SDL_FreeSurface(font_white);
        SDL_FreeSurface(font_aqua);
        SDL_FreeSurface(font_gold);
        SDL_FreeSurface(font_cia);
        // SDL_FreeSurface(font_red);
        // SDL_FreeSurface(font_green);
        // SDL_FreeSurface(font_blue);
        font_black = NULL;
        return 0;
    }
    SDL_Surface* font = font_black;
    if(     colour == 1){font = font_white;}
    else if(colour == 2){font = font_aqua;}
    else if(colour == 3){font = font_gold;}
    else if(colour == 4){font = font_cia;}
    // else if(colour == 5){font = font_red;}
    // else if(colour == 6){font = font_green;}
    // else if(colour == 7){font = font_blue;}
    SDL_Rect dr = {x, y, 0, 0};
    const Uint32 len = strlen(s);
    for(Uint32 i = 0; i < len; i++)
    {
             if(s[i] == 'A'){SDL_BlitSurface(font, &(SDL_Rect){0,0,7,9}, o, &dr); dr.x += 7+m;}
        else if(s[i] == 'B'){SDL_BlitSurface(font, &(SDL_Rect){7,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'C'){SDL_BlitSurface(font, &(SDL_Rect){13,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'D'){SDL_BlitSurface(font, &(SDL_Rect){19,0,7,9}, o, &dr); dr.x += 7+m;}
        else if(s[i] == 'E'){SDL_BlitSurface(font, &(SDL_Rect){26,0,5,9}, o, &dr); dr.x += 5+m;}
        else if(s[i] == 'F'){SDL_BlitSurface(font, &(SDL_Rect){31,0,5,9}, o, &dr); dr.x += 5+m;}
        else if(s[i] == 'G'){SDL_BlitSurface(font, &(SDL_Rect){36,0,7,9}, o, &dr); dr.x += 7+m;}
        else if(s[i] == 'H'){SDL_BlitSurface(font, &(SDL_Rect){43,0,7,9}, o, &dr); dr.x += 7+m;}
        else if(s[i] == 'I'){SDL_BlitSurface(font, &(SDL_Rect){50,0,4,9}, o, &dr); dr.x += 4+m;}
        else if(s[i] == 'J'){SDL_BlitSurface(font, &(SDL_Rect){54,0,5,9}, o, &dr); dr.x += 5+m;}
        else if(s[i] == 'K'){SDL_BlitSurface(font, &(SDL_Rect){59,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'L'){SDL_BlitSurface(font, &(SDL_Rect){65,0,5,9}, o, &dr); dr.x += 5+m;}
        else if(s[i] == 'M'){SDL_BlitSurface(font, &(SDL_Rect){70,0,9,9}, o, &dr); dr.x += 9+m;}
        else if(s[i] == 'N'){SDL_BlitSurface(font, &(SDL_Rect){79,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'O'){SDL_BlitSurface(font, &(SDL_Rect){85,0,7,9}, o, &dr); dr.x += 7+m;}
        else if(s[i] == 'P'){SDL_BlitSurface(font, &(SDL_Rect){92,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'Q'){SDL_BlitSurface(font, &(SDL_Rect){98,0,7,11}, o, &dr); dr.x += 7+m;}
        else if(s[i] == 'R'){SDL_BlitSurface(font, &(SDL_Rect){105,0,7,9}, o, &dr); dr.x += 7+m;}
        else if(s[i] == 'S'){SDL_BlitSurface(font, &(SDL_Rect){112,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'T'){SDL_BlitSurface(font, &(SDL_Rect){118,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'U'){SDL_BlitSurface(font, &(SDL_Rect){124,0,7,9}, o, &dr); dr.x += 7+m;}
        else if(s[i] == 'V'){SDL_BlitSurface(font, &(SDL_Rect){131,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'W'){SDL_BlitSurface(font, &(SDL_Rect){137,0,10,9}, o, &dr); dr.x += 10+m;}
        else if(s[i] == 'X'){SDL_BlitSurface(font, &(SDL_Rect){147,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'Y'){SDL_BlitSurface(font, &(SDL_Rect){153,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'Z'){SDL_BlitSurface(font, &(SDL_Rect){159,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'a'){SDL_BlitSurface(font, &(SDL_Rect){165,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'b'){SDL_BlitSurface(font, &(SDL_Rect){171,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'c'){SDL_BlitSurface(font, &(SDL_Rect){177,0,5,9}, o, &dr); dr.x += 5+m;}
        else if(s[i] == 'd'){SDL_BlitSurface(font, &(SDL_Rect){182,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'e'){SDL_BlitSurface(font, &(SDL_Rect){188,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'f'){SDL_BlitSurface(font, &(SDL_Rect){194,0,4,9}, o, &dr); dr.x += 3+m;}
        else if(s[i] == 'g'){SDL_BlitSurface(font, &(SDL_Rect){198,0,6,11}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'h'){SDL_BlitSurface(font, &(SDL_Rect){204,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'i'){SDL_BlitSurface(font, &(SDL_Rect){210,0,2,9}, o, &dr); dr.x += 2+m;}
        else if(s[i] == 'j'){SDL_BlitSurface(font, &(SDL_Rect){212,0,3,11}, o, &dr); dr.x += 3+m;}
        else if(s[i] == 'k'){SDL_BlitSurface(font, &(SDL_Rect){215,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'l'){SDL_BlitSurface(font, &(SDL_Rect){221,0,2,9}, o, &dr); dr.x += 2+m;}
        else if(s[i] == 'm'){SDL_BlitSurface(font, &(SDL_Rect){223,0,10,9}, o, &dr); dr.x += 10+m;}
        else if(s[i] == 'n'){SDL_BlitSurface(font, &(SDL_Rect){233,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'o'){SDL_BlitSurface(font, &(SDL_Rect){239,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'p'){SDL_BlitSurface(font, &(SDL_Rect){245,0,6,11}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'q'){SDL_BlitSurface(font, &(SDL_Rect){251,0,6,11}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'r'){SDL_BlitSurface(font, &(SDL_Rect){257,0,4,9}, o, &dr); dr.x += 4+m;}
        else if(s[i] == 's'){SDL_BlitSurface(font, &(SDL_Rect){261,0,5,9}, o, &dr); dr.x += 5+m;}
        else if(s[i] == 't'){SDL_BlitSurface(font, &(SDL_Rect){266,0,4,9}, o, &dr); dr.x += 4+m;}
        else if(s[i] == 'u'){SDL_BlitSurface(font, &(SDL_Rect){270,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'v'){SDL_BlitSurface(font, &(SDL_Rect){276,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'w'){SDL_BlitSurface(font, &(SDL_Rect){282,0,8,9}, o, &dr); dr.x += 8+m;}
        else if(s[i] == 'x'){SDL_BlitSurface(font, &(SDL_Rect){290,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'y'){SDL_BlitSurface(font, &(SDL_Rect){296,0,6,11}, o, &dr); dr.x += 6+m;}
        else if(s[i] == 'z'){SDL_BlitSurface(font, &(SDL_Rect){302,0,5,9}, o, &dr); dr.x += 5+m;}
        else if(s[i] == '0'){SDL_BlitSurface(font, &(SDL_Rect){307,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == '1'){SDL_BlitSurface(font, &(SDL_Rect){313,0,4,9}, o, &dr); dr.x += 4+m;}
        else if(s[i] == '2'){SDL_BlitSurface(font, &(SDL_Rect){317,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == '3'){SDL_BlitSurface(font, &(SDL_Rect){323,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == '4'){SDL_BlitSurface(font, &(SDL_Rect){329,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == '5'){SDL_BlitSurface(font, &(SDL_Rect){335,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == '6'){SDL_BlitSurface(font, &(SDL_Rect){341,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == '7'){SDL_BlitSurface(font, &(SDL_Rect){347,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == '8'){SDL_BlitSurface(font, &(SDL_Rect){353,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == '9'){SDL_BlitSurface(font, &(SDL_Rect){359,0,6,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == ':'){SDL_BlitSurface(font, &(SDL_Rect){365,0,2,9}, o, &dr); dr.x += 2+m;}
        else if(s[i] == '.'){SDL_BlitSurface(font, &(SDL_Rect){367,0,2,9}, o, &dr); dr.x += 2+m;}
        else if(s[i] == '+'){SDL_BlitSurface(font, &(SDL_Rect){369,0,7,9}, o, &dr); dr.x += 7+m;}
        else if(s[i] == '-'){dr.x++; SDL_BlitSurface(font, &(SDL_Rect){376,0,4,9}, o, &dr); dr.x += 6+m;}
        else if(s[i] == ' '){dr.x += 2+m;}
    }
    return dr.x;
}
int lenText(const char* s)
{
    int x = 0;
    static const int m = 1;
    const Uint32 len = strlen(s);
    for(Uint32 i = 0; i < len; i++)
    {
             if(s[i] == 'A'){x += 7+m;}
        else if(s[i] == 'B'){x += 6+m;}
        else if(s[i] == 'C'){x += 6+m;}
        else if(s[i] == 'D'){x += 7+m;}
        else if(s[i] == 'E'){x += 5+m;}
        else if(s[i] == 'F'){x += 5+m;}
        else if(s[i] == 'G'){x += 7+m;}
        else if(s[i] == 'H'){x += 7+m;}
        else if(s[i] == 'I'){x += 4+m;}
        else if(s[i] == 'J'){x += 5+m;}
        else if(s[i] == 'K'){x += 6+m;}
        else if(s[i] == 'L'){x += 5+m;}
        else if(s[i] == 'M'){x += 9+m;}
        else if(s[i] == 'N'){x += 6+m;}
        else if(s[i] == 'O'){x += 7+m;}
        else if(s[i] == 'P'){x += 6+m;}
        else if(s[i] == 'Q'){x += 7+m;}
        else if(s[i] == 'R'){x += 7+m;}
        else if(s[i] == 'S'){x += 6+m;}
        else if(s[i] == 'T'){x += 6+m;}
        else if(s[i] == 'U'){x += 7+m;}
        else if(s[i] == 'V'){x += 6+m;}
        else if(s[i] == 'W'){x += 10+m;}
        else if(s[i] == 'X'){x += 6+m;}
        else if(s[i] == 'Y'){x += 6+m;}
        else if(s[i] == 'Z'){x += 6+m;}
        else if(s[i] == 'a'){x += 6+m;}
        else if(s[i] == 'b'){x += 6+m;}
        else if(s[i] == 'c'){x += 5+m;}
        else if(s[i] == 'd'){x += 6+m;}
        else if(s[i] == 'e'){x += 6+m;}
        else if(s[i] == 'f'){x += 3+m;}
        else if(s[i] == 'g'){x += 6+m;}
        else if(s[i] == 'h'){x += 6+m;}
        else if(s[i] == 'i'){x += 2+m;}
        else if(s[i] == 'j'){x += 3+m;}
        else if(s[i] == 'k'){x += 6+m;}
        else if(s[i] == 'l'){x += 2+m;}
        else if(s[i] == 'm'){x += 10+m;}
        else if(s[i] == 'n'){x += 6+m;}
        else if(s[i] == 'o'){x += 6+m;}
        else if(s[i] == 'p'){x += 6+m;}
        else if(s[i] == 'q'){x += 6+m;}
        else if(s[i] == 'r'){x += 4+m;}
        else if(s[i] == 's'){x += 5+m;}
        else if(s[i] == 't'){x += 4+m;}
        else if(s[i] == 'u'){x += 6+m;}
        else if(s[i] == 'v'){x += 6+m;}
        else if(s[i] == 'w'){x += 8+m;}
        else if(s[i] == 'x'){x += 6+m;}
        else if(s[i] == 'y'){x += 6+m;}
        else if(s[i] == 'z'){x += 5+m;}
        else if(s[i] == '0'){x += 6+m;}
        else if(s[i] == '1'){x += 4+m;}
        else if(s[i] == '2'){x += 6+m;}
        else if(s[i] == '3'){x += 6+m;}
        else if(s[i] == '4'){x += 6+m;}
        else if(s[i] == '5'){x += 6+m;}
        else if(s[i] == '6'){x += 6+m;}
        else if(s[i] == '7'){x += 6+m;}
        else if(s[i] == '8'){x += 6+m;}
        else if(s[i] == '9'){x += 6+m;}
        else if(s[i] == ':'){x += 2+m;}
        else if(s[i] == '.'){x += 2+m;}
        else if(s[i] == '+'){x += 7+m;}
        else if(s[i] == '-'){x += 7+m;}
        else if(s[i] == ' '){x += 2+m;}
    }
    return x;
}

//*************************************
// assets
//*************************************
const unsigned char icon[] = { // 16, 16, 4
  "\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000\000"
  "\000\000\"\070)\071f\000\000\000\"\377\377\377\000\377\377\377\000\377\377\377\000\377\377"
  "\377\000\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377"
  "\000\377\377\377\000\377\377\377\000\227o\231\231\235t\237\231\264\205\267\314"
  "\323\234\326\356\277\216\302\335\260\202\262\273\227o\231\210iMjU\377\377"
  "\377\000\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377"
  "\000\377\377\377\000\235t\237\273\347\253\350\377\347\253\350\377\365\255\354"
  "\377\360\252\347\377\356\255\355\377\347\253\350\377\356\255\355\377\264"
  "\205\267\356I\066J\063\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377"
  "\000#\031#\021\235t\237\252\347\253\350\377\345\247\344\377\336\247\346\377\213"
  "\236\342\377\251\240\341\377\317\245\345\377\336\247\346\377\345\247\344"
  "\377\351\254\353\377\277\216\302\377\017\013\017\"\377\377\377\000\377\377\377"
  "\000\377\377\377\000C\062Df\323\234\326\377\347\253\350\377\360\252\347\377\201"
  "\233\337\377,\215\330\377\061\221\333\377:\227\337\377S\226\335\377\326\246"
  "\345\377\356\255\355\377\306\223\307\356\377\377\377\000\377\377\377\000\377"
  "\377\377\000\377\377\377\000\017\013\017\063\305\224\313\356\351\254\353\377\345"
  "\247\344\377\326\246\345\377\277\232\334\377\232\215\321\377\210\222\345"
  "\377\221\227\335\377\356\255\355\377\356\255\355\377\224\202\273\356\000\000"
  "\000\"\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000n\205\277\314"
  "\365\255\354\377\341\245\342\377\347\253\350\377\345\247\344\377\365\255"
  "\354\377\360\252\347\377\355\247\345\377\347\253\350\377\341\245\342\377"
  "\245\201\351\377\223r\324\377\203g\276\335oW\240\231\060&ED\377\377\377\000"
  "Eh\237\273\272\246\351\377\336\247\346\377\360\252\347\377\360\252\347\377"
  "\351\254\353\377\345\247\344\377\317\245\345\377\351\254\353\377\276\231"
  "\346\377\241\177\362\377\246\202\360\377\253\206\366\377\234y\340\377'\037"
  "\071f\377\377\377\000\060Cx\231B\224\340\377P\230\337\377s\235\342\377\235\234"
  "\336\377\317\245\345\377\317\245\345\377H\234\345\377Y\234\344\377`\231\333"
  "\377\223z\325\377\234y\340\377\253\206\366\377\213m\310\335\000\000\000\021\377"
  "\377\377\000)T\200wHw\302\377L\202\315\377M\224\342\377:\227\337\377H\234\345"
  "\377P\230\337\377G~\310\377C\206\321\377S\221\311\377\223z\325\377\223r\324"
  "\377\253\206\366\377|a\263\273\377\377\377\000\377\377\377\000\067bxDQz\275\377"
  "S\\\256\377\256\177\357\377\202{\332\377E}\306\377G~\310\377K\\\252\377D"
  "Z\245\377xs\321\377\263\206\372\377\246\202\360\377\246\202\360\377kT\233"
  "\210\377\377\377\000\377\377\377\000DME\"T\225\312\377B\214\331\377o\213\344"
  "\377|}\332\377K\\\252\377Ja\256\377B\214\331\377C\206\321\377p\210\323\377"
  "\225n\320\356\237|\345\377\242~\351\377M=pU\377\377\377\000\377\377\377\000\377"
  "\377\377\000R~\236\252t\277\371\377Q\244\344\377B\224\340\377B\214\331\377"
  "J\214\325\377m\270\363\377a\257\360\377Q\207\242\252\000\000\000\021Q@vfgQ\226\252"
  "'\037\071\"\377\377\377\000\377\377\377\000\377\377\377\000Ii|Ux\255\315\335\206"
  "\304\352\377\204\310\366\377m\270\363\377a\257\360\377}\265\325\377\200\272"
  "\336\377\065M[D\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000\377"
  "\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000\065M[D"
  "a\215\247\210}\265\325\335u\252\312\335\000\000\000\"-BOD\377\377\377\000\377\377"
  "\377\000\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377"
  "\000\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000"
  "\000\000\000\021\017\026\032D\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377"
  "\000\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000",
};

