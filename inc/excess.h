/*
--------------------------------------------------
    James William Fletcher (github.com/mrbid)
        August 2023
--------------------------------------------------
    C & SDL / OpenGL ES2 / GLSL ES
    Colour Converter: https://www.easyrgb.com
*/

#pragma GCC diagnostic ignored "-Wtrigraphs"
#define forceinline __attribute__((always_inline)) inline

#include <time.h>
#include <zlib.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>

#ifdef __linux__
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
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
forceinline uint PTI(const uchar x, const uchar y, const uchar z) // 0-128
{
    return (z * 16384) + (y * 128) + x;
}
vec ITP(const float i)
{
    const float z = i * 6.1035156E-5f; // i / 16384.f;
    const float a = floorf(z);
    const float b = (z-floorf(z)) * 128.f;
    const float c = (b-floorf(b)) * 128.f;
    return (vec){roundf(c),roundf(b),roundf(a)};
}
void defaultState(const uint type)
{
    g.sens = 0.003f;
    g.xrot = 0.f;
    g.yrot = 1.57f;
    g.pp = (vec){-64.f, 130.f, -64.f};
    if(type == 0)
    {
        g.ms = 37.2f;
    }
    g.st = 8.f;
    g.pb = (vec){0.f, 0.f, 0.f, -1.f};
    if(type == 0)
    {
        g.lms = 37.2f;
        g.cms = 74.4f;
    }
    g.plock = 0;
}
uint placedVoxels()
{
    uint c = 0;
    for(uint i = 0; i < max_voxels; i++)
        if(g.voxels[i] != 0){c++;}
    return c;
}
forceinline uint isInBounds(const vec p)
{
    if(p.x < 0.f || p.y < 0.f || p.z < 0.f || p.x > 127.5f || p.y > 127.5f || p.z > 127.5f){return 0;}
    return 1;
}

//*************************************
// ray functions
//*************************************
#define RAY_DEPTH 2048
#define RAY_STEP 0.0625f
int ray(vec* hit_pos, vec* hit_vec, const uint depth, const float stepsize, const vec start_pos)
{
    // might need exclude conditions for obviously bogus rays to avoid those 2048 steps
    vec inc;
    vMulS(&inc, look_dir, stepsize);
    int hit = -1;
    vec rp = start_pos;
    for(uint i = 0; i < depth; i++)
    {
        vAdd(&rp, rp, inc);
        if(isInBounds(rp) == 0){continue;} // break;
        vec rb;
        rb.x = roundf(rp.x);
        rb.y = roundf(rp.y);
        rb.z = roundf(rp.z);
        const uint vi = PTI(rb.x, rb.y, rb.z);
        //printf("%u: %f %f %f\n", vi, rb.x, rb.y, rb.z);
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
    vec hp;
    lray = ray(&hp, &rp, RAY_DEPTH, RAY_STEP, ipp);
    //printf("TVP: %u: %f %f %f - %f %f %f\n", lray, hp.x, hp.y, hp.z, ipp.x, ipp.y, ipp.z);
    if(lray > -1 && face == 1)
    {
        vNorm(&rp);
        vec diff = rp;
        rp = hp;

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
forceinline float fTime(){return ((float)SDL_GetTicks())*0.001f;}
#ifdef __linux__
    uint64_t microtime()
    {
        struct timeval tv;
        struct timezone tz;
        memset(&tz, 0, sizeof(struct timezone));
        gettimeofday(&tv, &tz);
        return 1000000 * tv.tv_sec + tv.tv_usec;
    }
    uint dirExist(const char* dir)
    {
        struct stat st;
        return (stat(dir, &st) == 0 && S_ISDIR(st.st_mode));
    }
    uint fileExist(const char* file)
    {
        struct stat st;
        return (stat(file, &st) == 0);
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
                // uchar r = (val & 0xFF0000) >> 16;
                // uchar gc = (val & 0x00FF00) >> 8;
                // uchar b = (val & 0x0000FF);
                // printf("%u: %u %u %u\n", val, r,gc,b);
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
SDL_Surface* surfaceFromData(const Uint32* data, Uint32 w, Uint32 h)
{
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    memcpy(s->pixels, data, s->pitch*h);
    return s;
}
forceinline Uint32 getpixel(const SDL_Surface *surface, Uint32 x, Uint32 y)
{
    const Uint8 *p = (Uint8*)surface->pixels + y * surface->pitch + x * surface->format->BytesPerPixel;
    return *(Uint32*)p;
}
forceinline void setpixel(SDL_Surface *surface, Uint32 x, Uint32 y, Uint32 pix)
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
  "\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000\000\000\000\035\000\000\000\307"
  "\000\000\000\327\000\000\000\242\000\000\000\215\000\000\000\346\000\000\000\260\377\377\377\000\377\377"
  "\377\000\377\377\377\000\000\000\000\247\000\000\000\213\377\377\377\000\377\377\377\000\377"
  "\377\377\000\377\377\377\000\000\000\000d\000\000\000\366\000\000\000\377\000\000\000\377\000\000\000\377\000"
  "\000\000\377\000\000\000\370\000\000\000\340\000\000\000\314\000\000\000\222\000\000\000\365\000\000\000\331\000\000"
  "\000\025\000\000\000\222\000\000\000\006\000\000\000\000\000\000\000l\000\000\000\365\063\063\063\377\261\261\261"
  "\376\313\313\313\354\263\263\263\336\222\222\222\354\214\214\214\367\177"
  "\177\177\372bbb\365!!!\376\000\000\000\375\000\000\000\233\000\000\000\375\000\000\000\204\000\000\000"
  "\331\036\036\036\377\211\211\211\377\264\264\264\370```\354\031\031\031\355\060"
  "\060\060\337YYY\276\242\242\242\236\353\353\353\257\377\377\377\362kkk\365"
  "\000\000\000\367\000\000\000\347\000\000\000\377\025\025\025\376iii\377\261\261\261\375yyy\347"
  "@\005\005\377\233\000\000\377\271\000\000\377y\000\000\377%\000\000\377fff\340\346\346\346\334"
  "\360\360\360\244\000\000\000\362\000\000\000\346\000\000\000\271\000\000\000\377\267\267\267\377"
  "\367\367\367\374\254\254\254\324J\020\020\377\347\002\002\377\377\004\004\377\356\000"
  "\000\377\301\017\017\377\306\224\224\377\276\267\267\377\221\221\221\322\301"
  "\301\301\217\000\000\000\371\000\000\000\340\000\000\000\250\000\000\000\377ddd\363\321\321\321\257"
  "\253\253\253\357\337\264\264\377\373\255\255\377\376\217\217\377\364rr\377"
  "\353\264\264\377\306\225\225\377qOO\377^^^\351\247\247\247\236\000\000\000\374"
  "\000\000\000\326\000\000\000\027\000\000\000\365>>>\374\256\256\256\253WWW\361\267OO\377\344"
  "cc\377\364\251\251\377\362\277\277\377\310pp\377\214MM\377vMM\377RRR\367"
  "\227\227\227\254\000\000\000\376\000\000\000\266\377\377\377\000\000\000\000\301)))\377\241\241"
  "\241\273```\347\214MM\377\266MM\377\314qq\377\277hh\377\226MM\377uMM\377"
  "\206MM\377bbb\370\256\256\256\311\000\000\000\376\000\000\000\261\377\377\377\000\000\000\000"
  "N\025\025\025\377\216\216\216\323\177\177\177\311aMM\377\204MM\377\236mm\377"
  "\226ll\377uMM\377\214MM\377zMM\377\177\177\177\360\220\220\220\273\000\000\000"
  "\377\000\000\000\333\377\377\377\000\377\377\377\000\004\004\004\373ttt\356\260\260\260\253"
  "ggg\357_MM\377\214ii\377\234qq\377\215MM\377{MM\377gbb\361\203\203\203\232"
  "\040\040\040\366\000\000\000\377\000\000\000Y\000\000\000\003\000\000\000\006\000\000\000\335yyy\372\253\253\253"
  "\342ccc\372YYY\374jjj\375vvv\376QQQ\376ddd\357vvv\237\025\025\025\366\000\000\000"
  "\367\000\000\000\365\000\000\000\022\000\000\000\003\000\000\000\264\000\000\000\374___\376\234\234\234\351"
  "\201\201\201\347www\360\206\206\206\362\244\244\244\361\236\236\236\326~"
  "~~\225\015\015\015\371\000\000\000\346\000\000\000\022\000\000\000\"\000\000\000\003\377\377\377\000\000\000"
  "\000\032\000\000\000\202\000\000\000\375\000\000\000\374\040\040\040\366ZZZ\347\305\305\305\306\377"
  "\377\377\274\220\220\220\205\005\005\005\367\000\000\000\326\377\377\377\000\377\377\377"
  "\000\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000"
  "\000\000\000\345\000\000\000\271\000\000\000\227\000\000\000\343\020\020\020\372###\367\002\002\002\376\000"
  "\000\000\345\000\000\000\006\377\377\377\000\377\377\377\000\377\377\377\000\377\377\377\000"
  "\377\377\377\000\377\377\377\000\377\377\377\000\000\000\000q\000\000\000f\377\377\377\000\377"
  "\377\377\000\000\000\000b\000\000\000\354\000\000\000\355\000\000\000\353\000\000\000m\377\377\377\000\377"
  "\377\377\000\377\377\377\000\377\377\377\000"
};

