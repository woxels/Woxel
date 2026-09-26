/*
--------------------------------------------------
    James William Fletcher (github.com/mrbid)
         & Test_User       (notabug.org/test_user)
            August 2023
--------------------------------------------------
    C & SDL / OpenGL ES2 / GLSL ES
    Colour Converter: https://www.easyrgb.com
*/
#include "inc/excess.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
//#define BENCH_FPS
void WOX_QUIT()
{
    SDL_HideWindow(wnd);
    saveState(openTitle, "", load_state);
    drawText(NULL, "*K", 0, 0, 0);
    SDL_FreeSurface(s_icon);
    SDL_FreeSurface(sHud);
    SDL_GL_DeleteContext(glc);
    SDL_DestroyWindow(wnd);
    SDL_Quit();
    exit(0);
}
void WOX_POP(const int w, const int h)
{
    winw = w;
    winh = h;
    winw2 = winw/2;
    winh2 = winh/2;
    if (winw < winh) {
        xscale = (float)winw/(float)winh;
        yscale = 1.f;
    } else {
        xscale = 1.f;
        yscale = (float)winh/(float)winw;
    }
    glUniform2f(scale_id, xscale, yscale);
    doPerspective();
}
static SDL_HitTestResult SDLCALL hitTest(SDL_Window *window, const SDL_Point *pt, void *data)
{
    if( SDL_PointInRect(pt, &(SDL_Rect){40, 0, winw2-85, 22}) == SDL_TRUE ||
        SDL_PointInRect(pt, &(SDL_Rect){winw2+30, 0, winw2-72, 22}) == SDL_TRUE)
        return SDL_HITTEST_DRAGGABLE;
    return SDL_HITTEST_NORMAL;
}
void drawHud(uint type);

//*************************************
// Base64 import / export (CLI: loadb64 / export b64)
// Web version gzip/zlib-compresses game_state then Base64-encodes it
// so the scene can be copied, shared, and pasted back.
//*************************************
static const char b64_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned char b64_dtable[256] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255, 62,255, 62,255, 63,
     52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
    255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
     15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
    255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
     41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};

static char* b64_encode(const unsigned char* src, size_t len, size_t* out_len)
{
    const size_t olen = 4 * ((len + 2) / 3);
    char* out = (char*)malloc(olen + 1);
    if(!out) return NULL;

    size_t i = 0, j = 0;
    const size_t n3 = len - (len % 3);
    while(i < n3)
    {
        const unsigned int n = ((unsigned int)src[i] << 16) |
                               ((unsigned int)src[i+1] << 8) |
                               (unsigned int)src[i+2];
        out[j++] = b64_alphabet[(n >> 18) & 63];
        out[j++] = b64_alphabet[(n >> 12) & 63];
        out[j++] = b64_alphabet[(n >> 6) & 63];
        out[j++] = b64_alphabet[n & 63];
        i += 3;
    }
    if(i < len)
    {
        unsigned int n = ((unsigned int)src[i] << 16);
        out[j++] = b64_alphabet[(n >> 18) & 63];
        if(i + 1 < len)
        {
            n |= ((unsigned int)src[i+1] << 8);
            out[j++] = b64_alphabet[(n >> 12) & 63];
            out[j++] = b64_alphabet[(n >> 6) & 63];
            out[j++] = '=';
        }
        else
        {
            out[j++] = b64_alphabet[(n >> 12) & 63];
            out[j++] = '=';
            out[j++] = '=';
        }
    }
    out[j] = 0;
    if(out_len) *out_len = j;
    return out;
}

static unsigned char* b64_decode(const char* src, size_t len, size_t* out_len)
{
    while(len && (src[len-1]=='\n' || src[len-1]=='\r' ||
                  src[len-1]==' '  || src[len-1]=='\t'))
        len--;
    if(len == 0 || (len & 3)) return NULL;

    size_t pads = 0;
    if(src[len-1] == '=') pads++;
    if(src[len-2] == '=') pads++;

    const size_t olen = (len / 4) * 3 - pads;
    unsigned char* out = (unsigned char*)malloc(olen + 1);
    if(!out) return NULL;

    const unsigned char* s = (const unsigned char*)src;
    unsigned char* o = out;
    const unsigned char* end = s + len - (pads ? 4 : 0);
    while(s < end)
    {
        const unsigned int a = b64_dtable[s[0]];
        const unsigned int b = b64_dtable[s[1]];
        const unsigned int c = b64_dtable[s[2]];
        const unsigned int d = b64_dtable[s[3]];
        if((a | b | c | d) == 255){ free(out); return NULL; }
        const unsigned int t = (a << 18) | (b << 12) | (c << 6) | d;
        o[0] = (unsigned char)(t >> 16);
        o[1] = (unsigned char)(t >> 8);
        o[2] = (unsigned char)t;
        s += 4;
        o += 3;
    }
    if(pads)
    {
        const unsigned int a = b64_dtable[s[0]];
        const unsigned int b = b64_dtable[s[1]];
        const unsigned int c = (s[2] == '=') ? 0 : b64_dtable[s[2]];
        const unsigned int d = (s[3] == '=') ? 0 : b64_dtable[s[3]];
        if((a | b | c | d) == 255){ free(out); return NULL; }
        const unsigned int t = (a << 18) | (b << 12) | (c << 6) | d;
        *o++ = (unsigned char)(t >> 16);
        if(pads < 2) *o++ = (unsigned char)(t >> 8);
    }
    if(out_len) *out_len = olen;
    return out;
}

static int b64_is_filepath(const char* s)
{
    if(s == NULL || s[0] == 0){return 0;}
    if(s[0] == '/' || s[0] == '.' || s[0] == '~'){return 1;}
    if(strchr(s, '/') != NULL || strchr(s, '\\') != NULL){return 1;}
#ifdef _WIN32
    if(s[0] != 0 && s[1] == ':'){return 1;}
#endif
    return 0;
}

static void b64_mkdirs(const char* path)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", path);
    for(char* p = buf + 1; *p; p++)
    {
        if(*p == '/' || *p == '\\')
        {
            *p = 0;
            mkdir(buf, 0755);
            *p = '/';
        }
    }
}

static void b64_resolve_path(char* out, size_t outsz, const char* path)
{
    if(path != NULL && path[0] != 0)
    {
        snprintf(out, outsz, "%s", path);
        return;
    }
    if(b64_is_filepath(openTitle))
    {
        snprintf(out, outsz, "%s", openTitle);
        return;
    }
    snprintf(out, outsz, "%s%s", appdir ? appdir : "", openTitle);
}

uint saveBase64(const char* path)
{
    char file[1024];
    b64_resolve_path(file, sizeof(file), path);

    uLongf zlen = compressBound(sizeof(game_state));
    unsigned char* zbuf = (unsigned char*)malloc(zlen);
    if(zbuf == NULL)
    {
        printf("ERROR: saveBase64() out of memory.\n");
        return 0;
    }
    const int zr = compress2(zbuf, &zlen, (const Bytef*)&g, sizeof(game_state), 9);
    if(zr != Z_OK)
    {
        free(zbuf);
        printf("ERROR: saveBase64() compression failed (%d).\n", zr);
        return 0;
    }

    size_t blen = 0;
    char* b64 = b64_encode(zbuf, zlen, &blen);
    free(zbuf);
    if(b64 == NULL)
    {
        printf("ERROR: saveBase64() encode failed.\n");
        return 0;
    }

    b64_mkdirs(file);
    FILE* f = fopen(file, "w");
    if(f == NULL)
    {
        free(b64);
        printf("ERROR: saveBase64() could not write: %s\n", file);
        return 0;
    }
    fwrite(b64, 1, blen, f);
    fputc('\n', f);
    fclose(f);
    free(b64);

    char tmp[16];
    timestamp(tmp);
    printf("[%s] Exported Base64: %s (%u voxels)\n", tmp, file, placedVoxels());
    snprintf(warnm, sizeof(warnm), "Exported Base64");
    wti = t + 2.f;
    return 1;
}

uint loadBase64(const char* path)
{
    char file[1024];
    b64_resolve_path(file, sizeof(file), path);

    FILE* f = fopen(file, "rb");
    if(f == NULL)
    {
        const size_t n = strlen(file);
        if(n < 4 || (strcmp(file + n - 4, ".b64") != 0 && strcmp(file + n - 4, ".B64") != 0))
        {
            char alt[1024];
            snprintf(alt, sizeof(alt), "%s.b64", file);
            f = fopen(alt, "rb");
            if(f != NULL){snprintf(file, sizeof(file), "%s", alt);}
        }
    }
    if(f == NULL)
    {
        printf("ERROR: loadBase64() could not open: %s\n", file);
        snprintf(warnm, sizeof(warnm), "Invalid Base64 data");
        wti = t + 2.f;
        return 0;
    }
    fseek(f, 0, SEEK_END);
    const long flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(flen <= 0)
    {
        fclose(f);
        printf("ERROR: loadBase64() empty file: %s\n", file);
        snprintf(warnm, sizeof(warnm), "Invalid Base64 data");
        wti = t + 2.f;
        return 0;
    }
    char* raw = (char*)malloc((size_t)flen + 1);
    if(raw == NULL){fclose(f); return 0;}
    const size_t nread = fread(raw, 1, (size_t)flen, f);
    fclose(f);
    raw[nread] = 0;

    size_t slen = 0;
    char* stripped = (char*)malloc(nread + 1);
    if(stripped == NULL){free(raw); return 0;}
    for(size_t i = 0; i < nread; i++)
    {
        const unsigned char c = (unsigned char)raw[i];
        if(c == ' ' || c == '\n' || c == '\r' || c == '\t'){continue;}
        stripped[slen++] = (char)c;
    }
    stripped[slen] = 0;
    free(raw);

    size_t zlen = 0;
    unsigned char* zbuf = b64_decode(stripped, slen, &zlen);
    free(stripped);
    if(zbuf == NULL || zlen == 0)
    {
        printf("ERROR: loadBase64() invalid Base64 data.\n");
        snprintf(warnm, sizeof(warnm), "Invalid Base64 data");
        wti = t + 2.f;
        if(zbuf){free(zbuf);}
        return 0;
    }

    game_state ng;
    memset(&ng, 0, sizeof(ng));
    uLongf dlen = sizeof(game_state);
    int zr = uncompress((Bytef*)&ng, &dlen, zbuf, zlen);
    if(zr != Z_OK)
    {
        z_stream strm;
        memset(&strm, 0, sizeof(strm));
        strm.next_in = zbuf;
        strm.avail_in = (uInt)zlen;
        strm.next_out = (Bytef*)&ng;
        strm.avail_out = (uInt)sizeof(game_state);
        if(inflateInit2(&strm, 32 + MAX_WBITS) == Z_OK)
        {
            const int ir = inflate(&strm, Z_FINISH);
            dlen = strm.total_out;
            inflateEnd(&strm);
            zr = (ir == Z_STREAM_END) ? Z_OK : ir;
        }
    }
    if(zr != Z_OK && zlen == sizeof(game_state))
    {
        memcpy(&ng, zbuf, sizeof(game_state));
        dlen = sizeof(game_state);
        zr = Z_OK;
    }
    free(zbuf);
    if(zr != Z_OK || dlen != sizeof(game_state))
    {
        printf("ERROR: loadBase64() decompression failed (%d).\n", zr);
        snprintf(warnm, sizeof(warnm), "Decompression failed - corrupted data");
        wti = t + 2.f;
        return 0;
    }

    memcpy(&g, &ng, sizeof(game_state));
    pal_clamp_st();
    fks = (g.ms == g.cms);
    has_changed = 1;
    if(sHud != NULL){updateSelectColor();}

    char tmp[16];
    timestamp(tmp);
    printf("[%s] Imported Base64: %s (%u voxels)\n", tmp, file, placedVoxels());
    snprintf(warnm, sizeof(warnm), "Imported Base64");
    wti = t + 2.f;
    return 1;
}

void main_loop()
{
    // time delta
    static float lt = 0;
    t = fTime();
    const float dt = t-lt;
    lt = t;

    // fps counter
    if(focus_mouse == 0)
    {
        static uint fc = 0;
        static float ft = 0.f;
        if(t > ft)
        {
            g_fps = fc/3;
#ifdef BENCH_FPS
            static uint count = 0;
            count++;
            if(count > 2){exit(0);}
            printf("%u\n", g_fps);
#endif
            fc = 0;
            ft = t+3.f;
        }
        fc++;
    }
#ifdef BENCH_FPS
    return;
#endif

    // input handling
    static float idle = 0.f;

    // if user is idle for 3 minutes, save.
    if(idle != 0.f && t-idle > 180.f)
    {
        saveState(openTitle, ".idle", load_state);
        idle = 0.f; // so we only save once
        // on input a new idle is set, and a
        // count-down for a new save begins.
    }

    // window decor stuff
    if(size == 1)
    {
        static float lt = 0;
        if(t > lt)
        {
            int w,h;
            SDL_GetWindowSize(wnd, &w, &h);
            winw = w+(mx-dsx);
            winh = h+(my-dsy);
            dsx = mx;
            dsy = my;
            if(winw > 400 && winh > 380)
            {
                SDL_SetWindowSize(wnd, winw, winh);
                winw2 = winw/2;
                winh2 = winh/2;
                doPerspective();
            }
            lt = t+0.03f;
        }
    }

    static uint last_focus_mouse = 0;
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
            case SDL_WINDOWEVENT:
            {
                switch(event.window.event)
                {
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                    {
                        focus_mouse = last_focus_mouse;
                        //if(focus_mouse == 1){drawHud(1);}else{drawHud(0);}
                        SDL_ShowCursor(focus_mouse ? SDL_DISABLE : SDL_ENABLE);
                        if(wayland == 1 && focus_mouse == 1)
                        {
                            SDL_GetRelativeMouseState(&xd, &yd);
                            SDL_SetRelativeMouseMode(SDL_TRUE);
                        }
                    }
                    break;

                    case SDL_WINDOWEVENT_FOCUS_LOST:
                    {
                        last_focus_mouse = focus_mouse;
                        focus_mouse = 0;
                        //drawHud(0);
                        SDL_ShowCursor(SDL_ENABLE);
                        if(wayland == 1)
                        {
                            SDL_GetRelativeMouseState(&xd, &yd);
                            SDL_SetRelativeMouseMode(SDL_FALSE);
                        }
                    }
                    break;

                    case SDL_WINDOWEVENT_RESIZED:
                    {
                        WOX_POP(event.window.data1, event.window.data2);
                    }
                    break;
                }
            }
            break;

            case SDL_KEYDOWN:
            {
                if(event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_TAB)
                {
                    focus_mouse = 1 - focus_mouse;
                    //if(focus_mouse == 1){drawHud(1);}else{drawHud(0);}
                    SDL_ShowCursor(1 - focus_mouse);
                    if(wayland == 1)
                    {
                        if(focus_mouse == 1)
                        {
                            SDL_GetRelativeMouseState(&xd, &yd);
                            SDL_SetRelativeMouseMode(SDL_TRUE);
                        }
                        else
                        {
                            SDL_GetRelativeMouseState(&xd, &yd);
                            SDL_SetRelativeMouseMode(SDL_FALSE);
                        }
                    }
                    else
                    {
                        mx = winw2, my = winh2;
                        lx = winw2, ly = winh2;
                        SDL_WarpMouseInWindow(wnd, winw2, winh2);
                    }
                }
                else if(event.key.keysym.sym == SDLK_F2)
                {
                    showhud = 1 - showhud;
                }
                if(focus_mouse == 0){break;}
                if(event.key.keysym.sym == SDLK_w){ks[0] = 1;}
                else if(event.key.keysym.sym == SDLK_a){ks[1] = 1;}
                else if(event.key.keysym.sym == SDLK_s){ks[2] = 1;}
                else if(event.key.keysym.sym == SDLK_d){ks[3] = 1;}
                else if(event.key.keysym.sym == SDLK_LSHIFT || event.key.keysym.sym == SDLK_LCTRL){ks[4] = 1;} // move down Z
                else if(event.key.keysym.sym == SDLK_LEFT){ks[5] = 1;}
                else if(event.key.keysym.sym == SDLK_RIGHT){ks[6] = 1;}
                else if(event.key.keysym.sym == SDLK_UP){ks[7] = 1;}
                else if(event.key.keysym.sym == SDLK_DOWN){ks[8] = 1;}
                else if(event.key.keysym.sym == SDLK_SPACE){ks[9] = 1;} // move up Z
                else if(event.key.keysym.sym == SDLK_SLASH || event.key.keysym.sym == SDLK_x) // - change selected node
                {
                    traceViewPath(0);
                    if(lray > -1 && g.voxels[lray] > 7)
                    {
                        g.voxels[lray] = (uchar)pal_prev((float)g.voxels[lray]);
                        g.st = (float)g.voxels[lray];
                        has_changed = 1;
                        updateSelectColor();
                    }
                    else
                    {
                        g.st = pal_prev(g.st);
                        updateSelectColor();
                    }
                }
                else if(event.key.keysym.sym == SDLK_QUOTE || event.key.keysym.sym == SDLK_c) // + change selected node
                {
                    traceViewPath(0);
                    if(lray > -1 && g.voxels[lray] > 7)
                    {
                        g.voxels[lray] = (uchar)pal_next((float)g.voxels[lray]);
                        g.st = (float)g.voxels[lray];
                        has_changed = 1;
                        updateSelectColor();
                    }
                    else
                    {
                        g.st = pal_next(g.st);
                        updateSelectColor();
                    }
                }
                else if(event.key.keysym.sym == SDLK_RSHIFT) // place a voxel
                {
                    ptt = t+rrsp;
                    traceViewPath(1);
                    if(lray > -1)
                    {
                        if(g.pb.w == 1 && isInBounds(g.pb) && g.voxels[PTI(g.pb.x, g.pb.y, g.pb.z)] == 0)
                        {
                            g.voxels[PTI(g.pb.x, g.pb.y, g.pb.z)] = pal_voxel();
                            if(mirror == 1)
                            {
                                const float x = g.pb.x > 64.f ? 64.f+(64.f-g.pb.x) : 64.f + (64.f-g.pb.x);
                                g.voxels[PTI(x, g.pb.y, g.pb.z)] = pal_voxel();
                            }
                            has_changed = 1;
                        }
                    }
                }
                else if(event.key.keysym.sym == SDLK_RCTRL) // remove pointed voxel
                {
                    dtt = t+rrsp;
                    traceViewPath(0);
                    if(lray > -1)
                    {
                        g.voxels[lray] = 0;
                        if(mirror == 1)
                        {
                            const float x = ghp.x > 64.f ? 64.f+(64.f-ghp.x) : 64.f + (64.f-ghp.x);
                            g.voxels[PTI(x, ghp.y, ghp.z)] = 0;
                        }
                        has_changed = 1;
                    }
                }
                else if(event.key.keysym.sym == SDLK_q || event.key.keysym.sym == SDLK_z) // clone pointed voxel color
                {
                    traceViewPath(0);
                    if(lray > -1)
                    {
                        if(g.voxels[lray] > 7)
                        {
                            g.st = (float)g.voxels[lray];
                            updateSelectColor();
                        }
                        else{sprintf(warnm, "This is a system color you cannot clone this."); wti = t+1.f;}
                    }
                }
                else if(event.key.keysym.sym == SDLK_e) // replace pointed voxel
                {
                    rtt = t+rrsp;
                    traceViewPath(0);
                    if(lray > -1)
                    {
                        g.voxels[lray] = pal_voxel();
                        if(mirror == 1)
                        {
                            const float x = ghp.x > 64.f ? 64.f+(64.f-ghp.x) : 64.f + (64.f-ghp.x);
                            g.voxels[PTI(x, ghp.y, ghp.z)] = pal_voxel();
                        }
                        has_changed = 1;
                    }
                }
                else if(event.key.keysym.sym == SDLK_r) // toggle mirror brush
                {
                    mirror = 1 - mirror;
                }
                else if(event.key.keysym.sym == SDLK_v) // place voxel at current position
                {
                    vec p = g.pp;
                    vInv(&p);
                    vec pi = look_dir;
                    vMulS(&pi, pi, 6.f);
                    vAdd(&p, p, pi);
                    const vec rp = (vec){roundf(p.x), roundf(p.y), roundf(p.z)};
                    if(isInBounds(rp) == 1)
                    {
                        g.voxels[PTI(rp.x, rp.y, rp.z)] = 8;
                        has_changed = 1;
                    }
                }
                else if(event.key.keysym.sym == SDLK_f) // toggle movement speeds
                {
                    fks = 1 - fks;
                    if(fks){g.ms = g.cms;}
                       else{g.ms = g.lms;}
                }
                else if(event.key.keysym.sym == SDLK_1)
                {
                    g.ms = 9.3f;
                    if(fks){g.cms=g.ms;}else{g.lms=g.ms;}
                }
                else if(event.key.keysym.sym == SDLK_2)
                {
                    g.ms = 18.6f;
                    if(fks){g.cms=g.ms;}else{g.lms=g.ms;}
                }
                else if(event.key.keysym.sym == SDLK_3)
                {
                    g.ms = 37.2f;
                    if(fks){g.cms=g.ms;}else{g.lms=g.ms;}
                }
                else if(event.key.keysym.sym == SDLK_4)
                {
                    g.ms = 74.4f;
                    if(fks){g.cms=g.ms;}else{g.lms=g.ms;}
                }
                else if(event.key.keysym.sym == SDLK_5)
                {
                    g.ms = 148.8f;
                    if(fks){g.cms=g.ms;}else{g.lms=g.ms;}
                }
                else if(event.key.keysym.sym == SDLK_6)
                {
                    g.ms = 297.6f;
                    if(fks){g.cms=g.ms;}else{g.lms=g.ms;}
                }
                else if(event.key.keysym.sym == SDLK_7)
                {
                    g.ms = 595.2f;
                    if(fks){g.cms=g.ms;}else{g.lms=g.ms;}
                }
                else if(event.key.keysym.sym == SDLK_F1)
                {
                    SDL_SetWindowSize(wnd, 1024, 768);
                    WOX_POP(1024, 768);
                    defaultState(0);
                    fks = 0;
                }
                else if(event.key.keysym.sym == SDLK_F3)
                {
                    saveState(openTitle, "", load_state);
                }
                else if(event.key.keysym.sym == SDLK_F8)
                {
                    loadState(openTitle, 0);
					has_changed = 1;
                }
                else if(event.key.keysym.sym == SDLK_p)
                {
                    g.plock = 1 - g.plock;
                }
                idle = t;
            }
            break;

            case SDL_KEYUP:
            {
                if(focus_mouse == 0){break;}
                if(event.key.keysym.sym == SDLK_w){ks[0] = 0;}
                else if(event.key.keysym.sym == SDLK_a){ks[1] = 0;}
                else if(event.key.keysym.sym == SDLK_s){ks[2] = 0;}
                else if(event.key.keysym.sym == SDLK_d){ks[3] = 0;}
                else if(event.key.keysym.sym == SDLK_LSHIFT || event.key.keysym.sym == SDLK_LCTRL){ks[4] = 0;}
                else if(event.key.keysym.sym == SDLK_LEFT){ks[5] = 0;}
                else if(event.key.keysym.sym == SDLK_RIGHT){ks[6] = 0;}
                else if(event.key.keysym.sym == SDLK_UP){ks[7] = 0;}
                else if(event.key.keysym.sym == SDLK_DOWN){ks[8] = 0;}
                else if(event.key.keysym.sym == SDLK_SPACE){ks[9] = 0;}
                else if(event.key.keysym.sym == SDLK_RSHIFT){ptt = 0.f;}
                else if(event.key.keysym.sym == SDLK_RCTRL){dtt = 0.f;}
                else if(event.key.keysym.sym == SDLK_e){rtt = 0.f;}
                idle = t;
            }
            break;

            case SDL_MOUSEWHEEL: // change selected node
            {
                if(focus_mouse == 0){break;}

                bigc = t+0.5f;

                if(event.wheel.y < 0)
                {
                    g.st = pal_next(g.st);
                    updateSelectColor();
                }
                else if(event.wheel.y > 0)
                {
                    g.st = pal_prev(g.st);
                    updateSelectColor();
                }
            }
            break;

            case SDL_MOUSEMOTION:
            {
                mx = event.motion.x;
                my = event.motion.y;

                if(focus_mouse == 0){break;}
                idle = t;
            }
            break;

            case SDL_MOUSEBUTTONUP:
            {
                if(event.button.button == SDL_BUTTON_LEFT)
                {
                    if(size == 1)
                    {
                        size=0;
                        SDL_GetWindowSize(wnd, &winw, &winh);
                        WOX_POP(winw, winh);
                        SDL_CaptureMouse(SDL_FALSE);
                    }
                    ptt = 0.f;
                }
                else if(event.button.button == SDL_BUTTON_RIGHT){dtt = 0.f;}
                else if(event.button.button == SDL_BUTTON_X2){rtt = 0.f;}
                idle = t;
            }
            break;

            case SDL_MOUSEBUTTONDOWN:
            {
                if(wayland == 0)
                {
                    lx = event.button.x;
                    ly = event.button.y;
                }
                mx = event.button.x;
                my = event.button.y;

                static float llct = 0.f;
                static uint maxed = 0;

                if(event.button.button == SDL_BUTTON_LEFT) // check window decor stuff
                {
                    if(wayland == 1 && focus_mouse == 0)
                    {
                        if(llct != 0.f && t-llct < 0.3f)
                        {
                            if(maxed == 0)
                            {
                                SDL_MaximizeWindow(wnd);
                                maxed = 1;
                                size = 0;
                                llct = t;
                                break;
                            }
                            else
                            {
                                SDL_RestoreWindow(wnd);
                                maxed = 0;
                                size = 0;
                                llct = t;
                                break;
                            }
                        }
                        llct = t;
                        if(my < 22)
                        {
                            if(mx < 14)
                            {
                                WOX_QUIT();
                                break;
                            }
                            else if(mx < 24)
                            {
                                SDL_MinimizeWindow(wnd);
                                break;
                            }
                            else if(mx < 40)
                            {
                                maxed = 1 - maxed;
                                if(maxed == 1){SDL_MaximizeWindow(wnd);}
                                else{SDL_RestoreWindow(wnd);}
                                break;
                            }
                            else if(mx > winw-14)
                            {
                                WOX_QUIT();
                                break;
                            }
                            else if(mx > winw-24)
                            {
                                SDL_MinimizeWindow(wnd);
                                break;
                            }
                            else if(mx > winw-40)
                            {
                                maxed = 1 - maxed;
                                if(maxed == 1){SDL_MaximizeWindow(wnd);}
                                else{SDL_RestoreWindow(wnd);}
                                break;
                            }

                            dsx = mx, dsy = my;
                            break;
                        }
                        else if(mx > winw-15 && my > winh-15)
                        {
                            size = 1;
                            dsx = mx, dsy = my;
                            SDL_CaptureMouse(SDL_TRUE);
                            break;
                        }
                    }
                }

                if(focus_mouse == 0) // lock mouse focus on every mouse input to the window
                {
                    SDL_ShowCursor(0);
                    focus_mouse = 1;
                    if(wayland == 1)
                    {
                        SDL_GetRelativeMouseState(&xd, &yd);
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                    }
                    break;
                }

                if(event.button.button == SDL_BUTTON_LEFT) // place a voxel
                {
                    ptt = t+rrsp;
                    traceViewPath(1);
                    if(lray > -1)
                    {
                        if(g.pb.w == 1 && isInBounds(g.pb) && g.voxels[PTI(g.pb.x, g.pb.y, g.pb.z)] == 0)
                        {
                            g.voxels[PTI(g.pb.x, g.pb.y, g.pb.z)] = pal_voxel();
                            if(mirror == 1)
                            {
                                const float x = g.pb.x > 64.f ? 64.f+(64.f-g.pb.x) : 64.f + (64.f-g.pb.x);
                                g.voxels[PTI(x, g.pb.y, g.pb.z)] = pal_voxel();
                            }
                            has_changed = 1;
                        }
                    }
                }
                else if(event.button.button == SDL_BUTTON_RIGHT) // remove pointed voxel
                {
                    dtt = t+rrsp;
                    traceViewPath(0);
                    if(lray > -1)
                    {
                        g.voxels[lray] = 0;
                        if(mirror == 1)
                        {
                            const float x = ghp.x > 64.f ? 64.f+(64.f-ghp.x) : 64.f + (64.f-ghp.x);
                            g.voxels[PTI(x, ghp.y, ghp.z)] = 0;
                        }
                        has_changed = 1;
                    }
                }
                else if(event.button.button == SDL_BUTTON_MIDDLE || event.button.button == SDL_BUTTON_X1) // clone pointed voxel
                {
                    traceViewPath(0);
                    if(lray > -1)
                    {
                        if(g.voxels[lray] > 7)
                        {
                            g.st = (float)g.voxels[lray];
                            updateSelectColor();
                        }
                        else{sprintf(warnm, "This is a system color you cannot clone this."); wti = t+1.f;}
                    }
                }
                else if(event.button.button == SDL_BUTTON_X2) // replace pointed node
                {
                    rtt = t+rrsp;
                    traceViewPath(0);
                    if(lray > -1)
                    {
                        g.voxels[lray] = pal_voxel();
                        if(mirror == 1)
                        {
                            const float x = ghp.x > 64.f ? 64.f+(64.f-ghp.x) : 64.f + (64.f-ghp.x);
                            g.voxels[PTI(x, ghp.y, ghp.z)] = pal_voxel();
                        }
                        has_changed = 1;
                    }
                }
                idle = t;
            }
            break;

            case SDL_QUIT:
            {
                WOX_QUIT();
            }
            break;
        }
    }

    // on window focus
    if(focus_mouse == 1)
    {
        mGetViewZ(&look_dir, view);

        if(g.plock == 1)
        {
            look_dir.z = -0.001f;
            vNorm(&look_dir);
        }

        if(ptt != 0.f && t > ptt) // place trigger
        {
            traceViewPath(1);
            if(lray > -1)
            {
                if(g.pb.w == 1 && isInBounds(g.pb) && g.voxels[PTI(g.pb.x, g.pb.y, g.pb.z)] == 0)
                {
                    g.voxels[PTI(g.pb.x, g.pb.y, g.pb.z)] = pal_voxel();
                    if(mirror == 1)
                    {
                        const float x = g.pb.x > 64.f ? 64.f+(64.f-g.pb.x) : 64.f + (64.f-g.pb.x);
                        g.voxels[PTI(x, g.pb.y, g.pb.z)] = pal_voxel();
                    }
                    has_changed = 1;
                }
            }
            ptt = t+0.1;
        }

        if(dtt != 0.f && t > dtt) // delete trigger
        {
            traceViewPath(0);
            if(lray > -1)
            {
                g.voxels[lray] = 0;
                if(mirror == 1)
                {
                    const float x = ghp.x > 64.f ? 64.f+(64.f-ghp.x) : 64.f + (64.f-ghp.x);
                    g.voxels[PTI(x, ghp.y, ghp.z)] = 0;
                }
                has_changed = 1;
            }
            dtt = t+0.1f;
        }

        if(rtt != 0.f) // replace trigger
        {
            traceViewPath(0);
            if(lray > -1)
            {
                g.voxels[lray] = pal_voxel();
                if(mirror == 1)
                {
                    const float x = ghp.x > 64.f ? 64.f+(64.f-ghp.x) : 64.f + (64.f-ghp.x);
                    g.voxels[PTI(x, ghp.y, ghp.z)] = pal_voxel();
                }
                has_changed = 1;
            }
        }

        if(ks[0] == 1) // W
        {
            vec m;
            vMulS(&m, look_dir, g.ms * dt);
            vSub(&g.pp, g.pp, m);
        }
        else if(ks[2] == 1) // S
        {
            vec m;
            vMulS(&m, look_dir, g.ms * dt);
            vAdd(&g.pp, g.pp, m);
        }

        if(ks[1] == 1) // A
        {
            vec vdc;
            mGetViewX(&vdc, view);
            vec m;
            vMulS(&m, vdc, g.ms * dt);
            vSub(&g.pp, g.pp, m);
        }
        else if(ks[3] == 1) // D
        {
            vec vdc;
            mGetViewX(&vdc, view);
            vec m;
            vMulS(&m, vdc, g.ms * dt);
            vAdd(&g.pp, g.pp, m);
        }

        if(ks[4] == 1) // LSHIFT (down)
        {
            vec vdc;
            if(g.plock == 1)
                vdc = (vec){0.f,0.f,-1.f};
            else
                mGetViewY(&vdc, view);
            vec m;
            vMulS(&m, vdc, g.ms * dt);
            vSub(&g.pp, g.pp, m);
        }
        else if(ks[9] == 1) // SPACE (up)
        {
            vec vdc;
            if(g.plock == 1)
                vdc = (vec){0.f,0.f,-1.f};
            else
                mGetViewY(&vdc, view);
            vec m;
            vMulS(&m, vdc, g.ms * dt);
            vAdd(&g.pp, g.pp, m);
        }

        if(ks[5] == 1) // LEFT
            g.xrot += 0.7f*dt;
        else if(ks[6] == 1) // RIGHT
            g.xrot -= 0.7f*dt;

        if(ks[7] == 1) // UP
            g.yrot += 0.7f*dt;
        else if(ks[8] == 1) // DOWN
            g.yrot -= 0.7f*dt;

        if(wayland == 1)
        {
            // camera/mouse control
            SDL_GetRelativeMouseState(&xd, &yd);
            if(xd != 0 || yd != 0)
            {
                g.xrot -= xd*g.sens;
                g.yrot -= yd*g.sens;

                if(g.plock == 1)
                {
                    if(g.yrot > 3.11f)
                        g.yrot = 3.11f;
                    if(g.yrot < 0.03f)
                        g.yrot = 0.03f;
                }
                else
                {
                    if(g.yrot > 3.14f)
                        g.yrot = 3.14f;
                    if(g.yrot < 0.1f)
                        g.yrot = 0.1f;
                }
            }
        }
        else
        {
            // camera/mouse control
            const float xd = lx-mx;
            const float yd = ly-my;
            if(xd != 0 || yd != 0)
            {
                g.xrot += xd*g.sens;
                g.yrot += yd*g.sens;

                if(g.plock == 1)
                {
                    if(g.yrot > 3.11f)
                        g.yrot = 3.11f;
                    if(g.yrot < 0.03f)
                        g.yrot = 0.03f;
                }
                else
                {
                    if(g.yrot > 3.14f)
                        g.yrot = 3.14f;
                    if(g.yrot < 0.1f)
                        g.yrot = 0.1f;
                }
                
                lx = winw2, ly = winh2;
                SDL_WarpMouseInWindow(wnd, lx, ly);
            }
        }
    }

    mIdent(&view);
    mRotate(&view, g.yrot, 1.f, 0.f, 0.f);
    mRotate(&view, g.xrot, 0.f, 0.f, 1.f);

    mGetViewZ(&look_dir, view); // refresh

//*************************************
// begin render
//*************************************
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//*************************************
// main render
//*************************************

    ipp = g.pp; // inverse player position (setting global 'ipp' here is perfect)
    vInv(&ipp); // <--

    // hud
    {
        static float nt = 0.f;
        if(t > nt)
        {
            drawHud(focus_mouse);
            flipHud();
            nt = t+0.1f; // limit hud to 10fps
        }
    }

    // has changed?
    if(has_changed == 1)
    {
        // update voxels
        for (int x = 0; x < 1024; x++)
        for (int y = 0; y < 2048; y++) {
            int index = (x * 2048) + y;
            if (g.voxels[index] < 1) {
            setpixel(sVoxel, x, y, 0x00000000);
            } else {
                uint32_t color = g.colors[g.voxels[index]-1];
                color = (color >> 16) | (((color >> 8) & 0xFF) << 8) | ((color & 0xFF) << 16) | (0xFF << 24);
                setpixel(sVoxel, x, y, color);
            }
        }
        voxelmap = esReLoadTextureA(1024, 2048, sVoxel->pixels, 0);

        // bind the new texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, voxelmap);
        glUniform1i(voxel_id, 0);

        // bind the hudmap
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, hudmap);
        glUniform1i(hud_id, 1);

		// reset
		has_changed = 0;
    }

    // pass the current look pos (player position)
    glUniform3f(look_pos_id, -g.pp.x, -g.pp.y, -g.pp.z);

    // pass the view unit vectors
    vec v;
    mGetViewX(&v, view);
    v.x *= -1.f;
    v.y *= -1.f;
    v.z *= -1.f;
    glUniform3fv(view_id + 0, 1, (GLfloat*)&v);
    //
    mGetViewY(&v, view);
    v.x *= -1.f;
    v.y *= -1.f;
    v.z *= -1.f;
    glUniform3fv(view_id + 1, 1, (GLfloat*)&v);
    //
    mGetViewZ(&v, view);
    glUniform3fv(view_id + 2, 1, (GLfloat*)&v);

    // ok let's draw
    glDrawElements(GL_TRIANGLES, hud_numind, GL_UNSIGNED_BYTE, 0);

//*************************************
// swap buffers / display render
//*************************************
    SDL_GL_SwapWindow(wnd);
}
void drawHud(const uint type)
{    
    // clear cpu hud before rendering to it
    SDL_FillRect(sHud, &sHud->clip_rect, 0x00000000);
    if(type == 0)
    {
        if(wayland == 1)
        {
            // updated maxed state >:( wayland
            const uint flags = SDL_GetWindowFlags(wnd);
            if(flags & SDL_WINDOW_MAXIMIZED){maxed = 1;}else{maxed = 0;}

            // window title
            SDL_FillRect(sHud, &(SDL_Rect){0, 0, winw, 19}, 0xDDFFFF00);
            SDL_FillRect(sHud, &(SDL_Rect){1, 1, winw-2, 17}, 0xBB777700);
            const uint len = lenText("Woxel");
            drawText(sHud, "Woxel", winw2-24, 3, 3);
            drawText(sHud, "Woxel", winw2-26, 3, 3);
            drawText(sHud, "Woxel", winw2-25, 5, 3);
            drawText(sHud, "Woxel", winw2-25, 4, 0);
            drawText(sHud, "X -", 5, 4, 3);
            drawText(sHud, "X -", 4, 5, 0);
            if(maxed == 0)
            {
                SDL_FillRect(sHud, &(SDL_Rect){25, 3, 11, 11}, 0xFF00BFFF);
                SDL_FillRect(sHud, &(SDL_Rect){24, 4, 11, 11}, 0xFF000000);
                SDL_FillRect(sHud, &(SDL_Rect){25, 5, 9, 9}, 0xBB777700);
            }
            else
            {
                SDL_FillRect(sHud, &(SDL_Rect){24, 4, 11, 11}, 0xFF00BFFF);
                SDL_FillRect(sHud, &(SDL_Rect){23, 3, 11, 11}, 0xFF000000);
                SDL_FillRect(sHud, &(SDL_Rect){24, 4, 9, 9}, 0xBB777700);
            }
            drawText(sHud, "- X", winw-23, 4, 3);
            drawText(sHud, "- X", winw-22, 5, 0);
            if(maxed == 0)
            {
                SDL_FillRect(sHud, &(SDL_Rect){winw-38, 3, 11, 11}, 0xFF00BFFF);
                SDL_FillRect(sHud, &(SDL_Rect){winw-37, 4, 11, 11}, 0xFF000000);
                SDL_FillRect(sHud, &(SDL_Rect){winw-36, 5, 9, 9}, 0xBB777700);
            }
            else
            {
                SDL_FillRect(sHud, &(SDL_Rect){winw-37, 4, 11, 11}, 0xFF00BFFF);
                SDL_FillRect(sHud, &(SDL_Rect){winw-36, 3, 11, 11}, 0xFF000000);
                SDL_FillRect(sHud, &(SDL_Rect){winw-35, 4, 9, 9}, 0xBB777700);
            }
            SDL_FillRect(sHud, &(SDL_Rect){winw-15, winh-15, 15, 15}, 0xDDFFFF00);
            SDL_FillRect(sHud, &(SDL_Rect){winw-14, winh-14, 13, 13}, 0xBB777700);
            drawText(sHud, "r", winw-9, winh-13, 3);
            drawText(sHud, "r", winw-10, winh-14, 0);

            SDL_FillRect(sHud, &(SDL_Rect){40, 3, winw2-85, 13}, 0xDDa0b010);
            SDL_FillRect(sHud, &(SDL_Rect){winw2+30, 3, winw2-72, 13}, 0xDDa0b010);

            // fps
            char tmp[16];
            sprintf(tmp, "%u", g_fps);
            SDL_FillRect(sHud, &(SDL_Rect){0, 19, lenText(tmp)+8, 19}, 0xCC000000);
            drawText(sHud, tmp, 4, 23, 2);
        }
        else
        {
            // fps
            char tmp[16];
            sprintf(tmp, "%u", g_fps);
            SDL_FillRect(sHud, &(SDL_Rect){0, 0, lenText(tmp)+8, 19}, 0xCC000000);
            drawText(sHud, tmp, 4, 4, 2);
        }

        // center hud
        const int left = winw2-177;
        int top = winh2-152;
        SDL_FillRect(sHud, &(SDL_Rect){winw2-193, top-3, 382, 303}, 0x33FFFFFF);
        SDL_FillRect(sHud, &(SDL_Rect){winw2-190, top, 376, 297}, 0xCC000000);
        int a = drawText(sHud, "Woxel", winw2-15, top+11, 3);
        a = drawText(sHud, appVersion, left+330, top+11, 4);
        a = drawText(sHud, "woxels.github.io", left, top+11, 4);

        top += 33;
        a = drawText(sHud, "WASD ", left, top, 2);
        drawText(sHud, "Move around based on relative orientation to X and Y.", a, top, 1);

        top += 11;
        a = drawText(sHud, "SPACE", left, top, 2);
        a = drawText(sHud, " + ", a, top, 4);
        a = drawText(sHud, "L-SHIFT ", a, top, 2);
        drawText(sHud, "Move up and down relative Z.", a, top, 1);

        top += 11;
        a = drawText(sHud, "F ", left, top, 2);
        drawText(sHud, "Toggle player fast speed on and off.", a, top, 1);

        top += 11;
        a = drawText(sHud, "1", left, top, 2);
        a = drawText(sHud, "-", a, top, 4);
        a = drawText(sHud, "7 ", a, top, 2);
        drawText(sHud, "Change move speed for selected fast state.", a, top, 1);

        top += 11;
        a = drawText(sHud, "P ", left, top, 2);
        drawText(sHud, "Toggle pitch lock.", a, top, 1);

        top += 22;
        a = drawText(sHud, "Left Click ", left, top, 2);
        a = drawText(sHud, "Place node.", a, top, 1);
        a = drawText(sHud, " Right Click ", a, top, 2);
        drawText(sHud, "Delete node.", a, top, 1);

        top += 11;
        a = drawText(sHud, "Q ", left, top, 2);
        a = drawText(sHud, "Clone color of pointed node.", a, top, 1);
        a = drawText(sHud, " E ", a, top, 2);
        drawText(sHud, "Replace color of pointed node.", a, top, 1);

        top += 11;
        a = drawText(sHud, "R ", left, top, 2);
        a = drawText(sHud, "Toggle mirror brush.", a, top, 1);
        a = drawText(sHud, " V ", a, top, 2);
        drawText(sHud, "Place node at current location.", a, top, 1);

        top += 11;
        a = drawText(sHud, "Middle Scroll ", left, top, 2);
        drawText(sHud, "Change selected color.", a, top, 1);

        top += 11;
        a = drawText(sHud, "X", left, top, 2);
        a = drawText(sHud, " + ", a, top, 4);
        a = drawText(sHud, "C ", a, top, 2);
        drawText(sHud, "Scroll color of pointed node.", a, top, 1);

        top += 22;
        a = drawText(sHud, "F1 ", left, top, 2);
        drawText(sHud, "Resets environment state back to default.", a, top, 1);

        top += 11;
        a = drawText(sHud, "F2 ", left, top, 2);
        drawText(sHud, "Toggles visibility of the HUD.", a, top, 1);

        top += 11;
        a = drawText(sHud, "F3 ", left, top, 2);
        drawText(sHud, "Save. Will auto save on exit. Backup made if idle for 3 mins.", a, top, 1);

        top += 11;
        a = drawText(sHud, "F8 ", left, top, 2);
        drawText(sHud, "Load. Will erase what you have done since the last save.", a, top, 1);

        top += 21;
        drawText(sHud, "Check the console output for more information.", left, top, 3);

        for(uint i = 0; i < 16; i++)
        {
            const uint left2 = left+(i*22);
            {
                uint tu = g.colors[7+i];
                if(i < g.pal_n)
                {
                    uchar r = (tu & 0x00FF0000) >> 16;
                    uchar gc = (tu & 0x0000FF00) >> 8;
                    uchar b = (tu & 0x000000FF);
                    SDL_FillRect(sHud, &(SDL_Rect){left2, top+21, 20, 20}, 0xFFFFFFFF);
                    const Uint32 tclr = SDL_MapRGB(sHud->format, r,gc,b);
                    SDL_FillRect(sHud, &(SDL_Rect){left2+1, top+22, 18, 18}, tclr);
                    if(mx > left2 && mx < left2+20 && my > top+21 && my < top+42)
                    {
                        char tmp[16];
                        sprintf(tmp, "%02X%02X%02X", r, gc, b);
                        const int len = lenText(tmp);
                        const int len2 = len/2;
                        SDL_FillRect(sHud, &(SDL_Rect){mx-len2-4, top-4, len+9, 19}, tclr);
                        SDL_FillRect(sHud, &(SDL_Rect){mx-len2-1, top-1, len+3, 13}, 0xFF00BFFF);
                        SDL_FillRect(sHud, &(SDL_Rect){mx-len2, top, len+1, 11}, 0xFF000000);
                        drawText(sHud, tmp, mx-len2+1, top, 3);
                    }
                }
            }
            {
                uint tu = g.colors[23+i];
                if((16+i) < g.pal_n)
                {
                    uchar r = (tu & 0x00FF0000) >> 16;
                    uchar gc = (tu & 0x0000FF00) >> 8;
                    uchar b = (tu & 0x000000FF);
                    SDL_FillRect(sHud, &(SDL_Rect){left2, top+43, 20, 20}, 0xFFFFFFFF);
                    const Uint32 tclr = SDL_MapRGB(sHud->format, r,gc,b);
                    SDL_FillRect(sHud, &(SDL_Rect){left2+1, top+44, 18, 18}, tclr);
                    if(mx > left2 && mx < left2+20 && my > top+43 && my < top+63)
                    {
                        char tmp[16];
                        sprintf(tmp, "%02X%02X%02X", r, gc, b);
                        const int len = lenText(tmp);
                        const int len2 = len/2;
                        SDL_FillRect(sHud, &(SDL_Rect){mx-len2-4, top-4, len+9, 19}, tclr);
                        SDL_FillRect(sHud, &(SDL_Rect){mx-len2-1, top-1, len+3, 13}, 0xFF00BFFF);
                        SDL_FillRect(sHud, &(SDL_Rect){mx-len2, top, len+1, 11}, 0xFF000000);
                        drawText(sHud, tmp, mx-len2+1, top, 3);
                    }
                }
            }
        }
    }
    else if(showhud == 1)
    {
        if(t < bigc)
        {
            SDL_FillRect(sHud, &(SDL_Rect){winw2-3, winh2-3, 6, 6}, sclr);
        }
        else
        {
            setpixel(sHud, winw2, winh2, sclr);
            //
            setpixel(sHud, winw2+1, winh2, sclr);
            setpixel(sHud, winw2-1, winh2, sclr);
            setpixel(sHud, winw2, winh2+1, sclr);
            setpixel(sHud, winw2, winh2-1, sclr);
            //
            setpixel(sHud, winw2+2, winh2, sclr);
            setpixel(sHud, winw2-2, winh2, sclr);
            setpixel(sHud, winw2, winh2+2, sclr);
            setpixel(sHud, winw2, winh2-2, sclr);
            //
            setpixel(sHud, winw2+3, winh2, sclr);
            setpixel(sHud, winw2-3, winh2, sclr);
            setpixel(sHud, winw2, winh2+3, sclr);
            setpixel(sHud, winw2, winh2-3, sclr);
            // now the part to prevent invisible crosshair
            setpixel(sHud, winw2+1, winh2+1, 0xCC000000);
            setpixel(sHud, winw2-1, winh2-1, 0xCC000000);
            setpixel(sHud, winw2-1, winh2+1, 0xCC000000);
            setpixel(sHud, winw2+1, winh2-1, 0xCC000000);
            //
            setpixel(sHud, winw2+2, winh2+1, 0xCC000000);
            setpixel(sHud, winw2-2, winh2-1, 0xCC000000);
            setpixel(sHud, winw2-1, winh2+2, 0xCC000000);
            setpixel(sHud, winw2+1, winh2-2, 0xCC000000);
            //
            setpixel(sHud, winw2+3, winh2+1, 0xCC000000);
            setpixel(sHud, winw2-3, winh2-1, 0xCC000000);
            setpixel(sHud, winw2-1, winh2+3, 0xCC000000);
            setpixel(sHud, winw2+1, winh2-3, 0xCC000000);

            if(g.plock == 1)
            {
                setpixel(sHud, winw2+4, winh2, sclr);
                setpixel(sHud, winw2-4, winh2, sclr);
                setpixel(sHud, winw2+5, winh2, sclr);
                setpixel(sHud, winw2-5, winh2, sclr);
            }

            if(mirror == 1)
            {
                setpixel(sHud, winw2+2, winh2+2, sclr);
                setpixel(sHud, winw2-2, winh2-2, sclr);
                //
                setpixel(sHud, winw2+3, winh2+2, sclr);
                setpixel(sHud, winw2-3, winh2-2, sclr);
                setpixel(sHud, winw2+2, winh2+3, sclr);
                setpixel(sHud, winw2-2, winh2-3, sclr);
                //
                setpixel(sHud, winw2+4, winh2+2, sclr);
                setpixel(sHud, winw2-4, winh2-2, sclr);
                setpixel(sHud, winw2+2, winh2+4, sclr);
                setpixel(sHud, winw2-2, winh2-4, sclr);
                //
                setpixel(sHud, winw2+5, winh2+2, sclr);
                setpixel(sHud, winw2-5, winh2-2, sclr);
                setpixel(sHud, winw2+2, winh2+5, sclr);
                setpixel(sHud, winw2-2, winh2-5, sclr);
                // now the part to prevent invisible crosshair
                setpixel(sHud, winw2+3, winh2+3, 0xCC000000);
                setpixel(sHud, winw2+3, winh2+4, 0xCC000000);
                setpixel(sHud, winw2+3, winh2+5, 0xCC000000);
                setpixel(sHud, winw2+4, winh2+3, 0xCC000000);
                setpixel(sHud, winw2+5, winh2+3, 0xCC000000);
                setpixel(sHud, winw2-3, winh2-3, 0xCC000000);
                setpixel(sHud, winw2-3, winh2-4, 0xCC000000);
                setpixel(sHud, winw2-3, winh2-5, 0xCC000000);
                setpixel(sHud, winw2-4, winh2-3, 0xCC000000);
                setpixel(sHud, winw2-5, winh2-3, 0xCC000000);
            }
        }

        const int hso = winw2-175;
        for(uint i = 0; i < 16; i++)
        {
            const uint left = hso+(i*22);
            {
                uint tu = g.colors[7+i];
                if(i < g.pal_n)
                {
                    uchar r = (tu & 0x00FF0000) >> 16;
                    uchar gc = (tu & 0x0000FF00) >> 8;
                    uchar b = (tu & 0x000000FF);
                    if(pal_color_index() == 7+i)
                        SDL_FillRect(sHud, &(SDL_Rect){left, 11, 20, 20}, 0xFFFFFFFF);
                    else
                        SDL_FillRect(sHud, &(SDL_Rect){left, 11, 20, 20}, 0xFF000000);
                    SDL_FillRect(sHud, &(SDL_Rect){left+1, 12, 18, 18}, SDL_MapRGB(sHud->format, r,gc,b));
                }
            }
            {
                uint tu = g.colors[23+i];
                if((16+i) < g.pal_n)
                {
                    uchar r = (tu & 0x00FF0000) >> 16;
                    uchar gc = (tu & 0x0000FF00) >> 8;
                    uchar b = (tu & 0x000000FF);
                    if(pal_color_index() == 23+i)
                        SDL_FillRect(sHud, &(SDL_Rect){left, 33, 20, 20}, 0xFFFFFFFF);
                    else
                        SDL_FillRect(sHud, &(SDL_Rect){left, 33, 20, 20}, 0xFF000000);
                    SDL_FillRect(sHud, &(SDL_Rect){left+1, 34, 18, 18}, SDL_MapRGB(sHud->format, r,gc,b));
                }
            }
        }
    }

    // tooltips
    if(wti > t)
    {
        const int hlen = lenText(warnm)/2;
        drawText(sHud, warnm, winw2-hlen, winh2-22, 3);
    }

    // flip the new hud to gpu
    flipHud();
}

//*************************************
// CLI source helpers (project name vs file path)
//*************************************
static int wox_has_ext(const char* path, const char* ext)
{
    const size_t n = strlen(path);
    const size_t e = strlen(ext);
    if(n < e){return 0;}
    const char* a = path + (n - e);
    for(size_t i = 0; i < e; i++)
    {
        const unsigned char ca = (unsigned char)a[i];
        const unsigned char cb = (unsigned char)ext[i];
        const char la = (ca >= 'A' && ca <= 'Z') ? (char)(ca + 32) : (char)ca;
        const char lb = (cb >= 'A' && cb <= 'Z') ? (char)(cb + 32) : (char)cb;
        if(la != lb){return 0;}
    }
    return 1;
}

static int wox_ieq(const char* a, const char* b)
{
    if(a == NULL || b == NULL){return 0;}
    while(*a && *b)
    {
        unsigned char ca = (unsigned char)*a++, cb = (unsigned char)*b++;
        if(ca >= 'A' && ca <= 'Z'){ca = (unsigned char)(ca + 32);}
        if(cb >= 'A' && cb <= 'Z'){cb = (unsigned char)(cb + 32);}
        if(ca != cb){return 0;}
    }
    return *a == 0 && *b == 0;
}

static void wox_expand_path(char* out, size_t outsz, const char* path)
{
    if(path == NULL){out[0] = 0; return;}
    if(path[0] == '~' && (path[1] == '/' || path[1] == '\\' || path[1] == 0))
    {
        const char* home = getenv("HOME");
        if(home != NULL && home[0] != 0)
        {
            snprintf(out, outsz, "%s%s", home, path + 1);
            return;
        }
    }
    snprintf(out, outsz, "%s", path);
}

static int wox_file_exists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if(f == NULL){return 0;}
    fclose(f);
    return 1;
}

static int wox_file_is_gzip(const char* path)
{
    FILE* f = fopen(path, "rb");
    if(f == NULL){return 0;}
    unsigned char m[2] = {0, 0};
    const size_t n = fread(m, 1, 2, f);
    fclose(f);
    return n == 2 && m[0] == 0x1f && m[1] == 0x8b;
}

static int wox_parse_format(const char* s)
{
    if(s == NULL || s[0] == 0){return -1;}
    if(s[0] == '.'){s++;}
    if(wox_ieq(s, "wox") || wox_ieq(s, "gz") || wox_ieq(s, "wox.gz")){return 0;}
    if(wox_ieq(s, "txt")){return 1;}
    if(wox_ieq(s, "vv")){return 2;}
    if(wox_ieq(s, "ply") || wox_ieq(s, "greedy") || wox_ieq(s, "quads") ||
       wox_ieq(s, "quad") || wox_ieq(s, "tris") || wox_ieq(s, "tri") ||
       wox_ieq(s, "triangles")){return 3;}
    if(wox_ieq(s, "b64") || wox_ieq(s, "base64")){return 4;}
    if(wox_has_ext(s, ".wox") || wox_has_ext(s, ".gz")){return 0;}
    if(wox_has_ext(s, ".txt")){return 1;}
    if(wox_has_ext(s, ".vv")){return 2;}
    if(wox_has_ext(s, ".ply")){return 3;}
    if(wox_has_ext(s, ".b64")){return 4;}
    return -1;
}

// PLY mesh mode: 0 = greedy quads, 1 = per-face quads, 2 = per-face tris
static int wox_parse_ply_mode(const char* s)
{
    if(s == NULL || s[0] == 0){return -1;}
    if(s[0] == '.'){s++;}
    if(wox_ieq(s, "greedy") || wox_ieq(s, "ply-greedy") || wox_ieq(s, "ply:greedy")){return 0;}
    if(wox_ieq(s, "quads") || wox_ieq(s, "quad") || wox_ieq(s, "ply-quads") || wox_ieq(s, "ply:quads")){return 1;}
    if(wox_ieq(s, "tris") || wox_ieq(s, "tri") || wox_ieq(s, "triangles") ||
       wox_ieq(s, "ply-tris") || wox_ieq(s, "ply:tris")){return 2;}
    return -1;
}

static void wox_title_from_path(char* out, size_t outsz, const char* path)
{
    const char* base = path;
    for(const char* p = path; *p; p++)
        if(*p == '/' || *p == '\\'){base = p + 1;}
    snprintf(out, outsz, "%s", (base[0] != 0) ? base : "Untitled");
    char* dot = strrchr(out, '.');
    if(dot != NULL && wox_has_ext(dot, ".gz"))
    {
        *dot = 0;
        char* dot2 = strrchr(out, '.');
        if(dot2 != NULL && wox_has_ext(dot2, ".wox")){*dot2 = 0;}
    }
    else if(dot != NULL && (wox_has_ext(dot, ".b64") || wox_has_ext(dot, ".wox")))
        *dot = 0;
    if(out[0] == 0){snprintf(out, outsz, "Untitled");}
}

// Try an existing file as gzip project first, then Base64.
static uint wox_load_file(const char* path)
{
    if(wox_file_is_gzip(path))
    {
        if(loadState(path, 1)){return 1;}
    }
    if(loadBase64(path)){return 2;}
    if(loadState(path, 1)){return 1;}
    return 0;
}

// Resolve project name or file path into resolved_src. Returns 0 on failure.
static uint wox_load_any(const char* src, char* resolved, size_t resolved_sz)
{
    char path[1024], alt[1024];
    wox_expand_path(path, sizeof(path), src);

    if(wox_file_exists(path))
    {
        snprintf(resolved, resolved_sz, "%s", path);
        return wox_load_file(path);
    }

    snprintf(alt, sizeof(alt), "%s.b64", path);
    if(wox_file_exists(alt))
    {
        snprintf(resolved, resolved_sz, "%s", alt);
        return wox_load_file(alt);
    }

    snprintf(alt, sizeof(alt), "%s.wox.gz", path);
    if(wox_file_exists(alt))
    {
        snprintf(resolved, resolved_sz, "%s", alt);
        return wox_load_file(alt);
    }

    if(appdir != NULL)
    {
        snprintf(alt, sizeof(alt), "%s%s.wox.gz", appdir, path);
        if(wox_file_exists(alt))
        {
            snprintf(resolved, resolved_sz, "%s", alt);
            if(loadState(alt, 1)){return 1;}
        }
        snprintf(alt, sizeof(alt), "%s%s.b64", appdir, path);
        if(wox_file_exists(alt))
        {
            snprintf(resolved, resolved_sz, "%s", alt);
            return wox_load_file(alt);
        }
        snprintf(alt, sizeof(alt), "%s%s", appdir, path);
        if(wox_file_exists(alt))
        {
            snprintf(resolved, resolved_sz, "%s", alt);
            return wox_load_file(alt);
        }
    }

    snprintf(resolved, resolved_sz, "%s", path);
    if(loadState(path, 0)){return 1;}
    return 0;
}

//*************************************
// Process Entry Point
//*************************************
int main(int argc, char** argv)
{
//*************************************
// init stuff
//*************************************
    printf("██╗    ██╗ ██████╗ ██╗  ██╗███████╗██╗     \n");
    printf("██║    ██║██╔═══██╗╚██╗██╔╝██╔════╝██║     \n");
    printf("██║ █╗ ██║██║   ██║ ╚███╔╝ █████╗  ██║     \n");
    printf("██║███╗██║██║   ██║ ██╔██╗ ██╔══╝  ██║     \n");
    printf("╚███╔███╔╝╚██████╔╝██╔╝ ██╗███████╗███████╗\n");
    printf(" ╚══╝╚══╝  ╚═════╝ ╚═╝  ╚═╝╚══════╝╚══════╝\n");
    printf("\nMouse locks when you click on the window, press ESCAPE/TAB to unlock the mouse.\n\n");
    printf("Input Mapping:\n");
    printf("W,A,S,D = Move around based on relative orientation to X and Y.\n");
    printf("SPACE + L-SHIFT = Move up and down relative Z.\n");
    printf("Left Click / R-SHIFT = Place node.\n");
    printf("Right Click / R-CTRL = Delete node.\n");
    printf("V = Places voxel at current position.\n");
    printf("Q / Z / Middle Click / Mouse4 = Clone color of pointed node.\n");
    printf("E / Mouse5 = Replace color of pointed node.\n");
    printf("F = Toggle player fast speed on and off.\n");
    printf("1-7 = Change move speed for selected fast state.\n");
    printf("X + C / Slash + Quote = Scroll color of pointed node.\n");
    printf("R = Toggle mirror brush.\n");
    printf("P = Toggle pitch lock.\n");
    printf("F1 = Resets environment state back to default.\n");
    printf("F2 = Toggle HUD visibility.\n");
    printf("F3 = Save. (auto saves on exit, backup made if idle for 3 mins)\n");
    printf("F8 = Load. (will erase what you have done since the last save)\n");
    printf("\n* Arrow Keys can be used to move the view around.\n");
    printf("* Your state is automatically saved on exit.\n");
    printf("\nConsole Arguments:\n");
    printf("./wox <project_name> <[OPTIONAL]mouse_sensitivity> <[OPTIONAL]color_palette_file_path>\n");
    printf("e.g; ./wox Untitled 0.003 /tmp/colors.txt\n");
    printf("1st, \"Untitled\", Name of project to open or create.\n");
    printf("2nd, \"0.003\", Mouse sensitivity.\n");
    printf("3rd, \"/tmp/colors.txt\", path to a color palette file, the file must contain a hex\n");
    printf("color on each new line, 32 colors maximum. e.g; \"#00FFFF\".\n\n");
    printf("To load from file: ./wox loadgz <file_path>\n");
    printf("e.g; ./wox loadgz /home/user/file.wox.gz\n\n");
    printf("To load Base64: ./wox loadb64 <file_path>\n");
    printf("e.g; ./wox loadb64 /home/user/file.b64\n");
    printf("Loaded files are adopted as a project (basename) so F3 / exit can save them.\n\n");
    printf("To export: ./wox export <project_or_file> <[OPTIONAL]format> <[OPTIONAL]ply_mode> <export_path>\n");
    printf("Formats: wox, txt, vv, ply, b64\n");
    printf("PLY modes: greedy (merged quads, default), quads (one quad per face), tris (two triangles per face)\n");
    printf("e.g; ./wox export Untitled ply ./file.ply\n");
    printf("e.g; ./wox export Untitled ply greedy ./file.ply\n");
    printf("e.g; ./wox export Untitled ply quads ./file.ply\n");
    printf("e.g; ./wox export Untitled ply tris ./file.ply\n");
    printf("e.g; ./wox export ./file.b64 greedy ./file.ply\n");
    printf("e.g; ./wox export ~/file.wox.gz txt ./file.txt\n");
    printf("Format is optional if the output path ends in .ply/.txt/.vv/.b64/.wox.gz\n\n");
    printf("Find more color palettes at; https://lospec.com/palette-list\n");
    printf("You can use any palette up to 32 colors. #000000 (Black) is a valid color.\n\n");
    printf("Default 32 Color Palette: https://lospec.com/palette-list/resurrect-32\n");
    printf("\n----\n");

    // seed random
    srand(time(0));
    srandf(time(0));

    // get paths
    basedir = SDL_GetBasePath();
    appdir = SDL_GetPrefPath("voxdsp", "woxel");

    // argv
    char export_path[1024] = {0};
    char source_path[1024] = {0};
    char resolved_src[1024] = {0};
    uint export_type = 0;
    uint ply_mode = 0; // 0 greedy quads, 1 per-face quads, 2 per-face tris
    uint adopt_title = 0;
    uint loaded_ok = 0;
    if(argc >= 2 && strlen(argv[1]) < 256)
    {
        sprintf(openTitle, "%s", argv[1]);
    }
    if(argc >= 3 && strcmp(argv[1], "loadgz") == 0 && strlen(argv[2]) < 1024)
    {
        wox_expand_path(source_path, sizeof(source_path), argv[2]);
        adopt_title = 1;
    }
    if(argc >= 3 && strcmp(argv[1], "loadb64") == 0 && strlen(argv[2]) < 1024)
    {
        wox_expand_path(source_path, sizeof(source_path), argv[2]);
        adopt_title = 1;
    }
    if(argc >= 2 && strcmp(argv[1], "export") == 0)
    {
        if(argc < 4)
        {
            printf("ERROR: usage: ./wox export <project_or_file> [wox|txt|vv|ply|b64] [greedy|quads|tris] <export_path>\n");
            printf("       ./wox export ./scene.b64 ./scene.ply\n");
            printf("       ./wox export ./scene.b64 ply quads ./scene.ply\n");
            return 1;
        }
        wox_expand_path(source_path, sizeof(source_path), argv[2]);
        if(argc >= 6)
        {
            int fmt = wox_parse_format(argv[3]);
            const int mode = wox_parse_ply_mode(argv[4]);
            wox_expand_path(export_path, sizeof(export_path), argv[5]);
            if(fmt < 0){fmt = wox_parse_format(argv[5]);}
            if(fmt < 0)
            {
                printf("ERROR: unknown export format '%s' (use wox, txt, vv, ply, or b64)\n", argv[3]);
                return 1;
            }
            export_type = (uint)fmt;
            if(export_type == 3 && mode >= 0){ply_mode = (uint)mode;}
            else if(export_type == 3)
            {
                const int m2 = wox_parse_ply_mode(argv[3]);
                if(m2 >= 0){ply_mode = (uint)m2;}
            }
        }
        else if(argc >= 5)
        {
            int fmt = wox_parse_format(argv[3]);
            const int mode = wox_parse_ply_mode(argv[3]);
            wox_expand_path(export_path, sizeof(export_path), argv[4]);
            if(fmt < 0){fmt = wox_parse_format(argv[4]);}
            if(fmt < 0)
            {
                printf("ERROR: unknown export format '%s' (use wox, txt, vv, ply, greedy, quads, tris, or b64)\n", argv[3]);
                return 1;
            }
            export_type = (uint)fmt;
            if(export_type == 3 && mode >= 0){ply_mode = (uint)mode;}
        }
        else
        {
            wox_expand_path(export_path, sizeof(export_path), argv[3]);
            const int fmt = wox_parse_format(argv[3]);
            if(fmt < 0)
            {
                printf("ERROR: cannot infer format from '%s' — pass ply/txt/vv/b64/wox\n", argv[3]);
                return 1;
            }
            export_type = (uint)fmt;
        }
    }

    // load source (file, sniffed type, or saved project name)
    if(source_path[0] != 0)
    {
        loaded_ok = wox_load_any(source_path, resolved_src, sizeof(resolved_src));
        if(loaded_ok && adopt_title)
        {
            wox_title_from_path(openTitle, sizeof(openTitle), resolved_src[0] ? resolved_src : source_path);
            load_state = 0;
            char tmpad[16];
            timestamp(tmpad);
            printf("[%s] Project name: %s (save path %s%s.wox.gz)\n", tmpad, openTitle, appdir ? appdir : "", openTitle);
        }
    }
    else
    {
        loaded_ok = loadState(openTitle, load_state);
        snprintf(resolved_src, sizeof(resolved_src), "%s", openTitle);
    }
    if(loaded_ok == 0 && export_path[0] != 0)
    {
        printf("ERROR: could not load '%s' for export.\n", source_path[0] ? source_path : openTitle);
        printf("Tried the path as a file (.b64 / .wox.gz) and as a project name.\n");
        return 1;
    }
    if(loaded_ok == 0)
    {
        defaultState(0);
        memset(&g.voxels, 0, max_voxels);
        //
        g.voxels[PTI(64,64,64)] = 1; // center
        g.voxels[PTI(64,64,1)] = 7;
        g.voxels[PTI(1,64,64)] = 3;
        g.voxels[PTI(64,1,64)] = 5;
        g.voxels[PTI(64,64,126)] = 6;
        g.voxels[PTI(126,64,64)] = 2;
        g.voxels[PTI(64,126,64)] = 4;
        //
        g.voxels[PTI(1,1,1)] = 1;
        g.voxels[PTI(126,126,126)] = 1;
        g.voxels[PTI(1,126,126)] = 1;
        g.voxels[PTI(126,126,1)] = 1;
        g.voxels[PTI(126,1,1)] = 1;
        g.voxels[PTI(1,1,126)] = 1;
        g.voxels[PTI(126,1,126)] = 1;
        g.voxels[PTI(1,126,1)] = 1;
        //
        // system palette
        g.colors[0] = 16777215;
        g.colors[1] = 16711680;
        g.colors[2] = 8388608;
        g.colors[3] = 65280;
        g.colors[4] = 32768;
        g.colors[5] = 255;
        g.colors[6] = 128;
        g.pal_n = 32;
        g.st = 8.f;
        // user palette
        g.colors[7] = 16777215;
        g.colors[8] = 16476957;
        g.colors[9] = 15219515;
        g.colors[10] = 8592477;
        g.colors[11] = 12788820;
        g.colors[12] = 15748984;
        g.colors[13] = 16155009;
        g.colors[14] = 16557968;
        g.colors[15] = 14928022;
        g.colors[16] = 11244666;
        g.colors[17] = 9858156;
        g.colors[18] = 6444389;
        g.colors[19] = 4076870;
        g.colors[20] = 745061;
        g.colors[21] = 756367;
        g.colors[22] = 2014323;
        g.colors[23] = 9558889;
        g.colors[24] = 16514950;
        g.colors[25] = 16496980;
        g.colors[26] = 13461565;
        g.colors[27] = 10372409;
        g.colors[28] = 8007749;
        g.colors[29] = 7028341;
        g.colors[30] = 9461417;
        g.colors[31] = 11044083;
        g.colors[32] = 15379949;
        g.colors[33] = 9425919;
        g.colors[34] = 5086182;
        g.colors[35] = 5072308;
        g.colors[36] = 4737655;
        g.colors[37] = 3203513;
        g.colors[38] = 9435362;
        //
        char tmp[16];
        timestamp(tmp);
        printf("[%s] New volumetric canvas created.\n", tmp);
        if(load_state == 0)
            printf("[%s] Created: %s%s.wox.gz\n", tmp, appdir, openTitle);
        else
            printf("[%s] Created: %s\n", tmp, openTitle);
    }
    else
    {
        char tmp[16];
        timestamp(tmp);
        if(load_state == 0)
            printf("[%s] Opened: %s%s.wox.gz\n", tmp, appdir, openTitle);
        else
            printf("[%s] Opened: %s\n", tmp, openTitle);
    }

    //memset(&g.voxels, 8, max_voxels);

    // if this is just an export job then export and quit.
    if(export_path[0] != 0x00)
    {
        if(export_type == 0){saveState(export_path, "", 1);}
        if(export_type == 4){saveBase64(export_path); return 0;}
        if(export_type == 1)
        {
            FILE* f = fopen(export_path, "w");
            if(f != NULL)
            {
                fprintf(f, "# %s %s\n", appTitle, appVersion);
                fprintf(f, "# X Y Z RRGGBB\n");
                for(uchar z = 0; z < 128; z++)
                {
                    for(uchar y = 0; y < 128; y++)
                    {
                        for(uchar x = 0; x < 128; x++)
                        {
                            const uint i = PTI(x,y,z);
                            if(g.voxels[i] < 8){continue;}
                            const uint tu = g.colors[g.voxels[i]-1];
                            uchar r = (tu & 0x00FF0000) >> 16;
                            uchar gc = (tu & 0x0000FF00) >> 8;
                            uchar b = (tu & 0x000000FF);
                            if(r != 0 || gc != 0 || b != 0)
                                fprintf(f, "%i %i %i %02X%02X%02X\n", ((int)x)-64, ((int)y)-64, z, r, gc, b);
                        }
                    }
                }
                fclose(f);
                char tmp[16];
                timestamp(tmp);
                printf("[%s] Exported TXT: %s\n", tmp, export_path);
            }
        }
        if(export_type == 2)
        {
            FILE* f = fopen(export_path, "w");
            if(f != NULL)
            {
                fprintf(f, "# %s %s - Visible Voxels only\n", appTitle, appVersion);
                fprintf(f, "# X Y Z RRGGBB\n");
                for(uchar z = 0; z < 128; z++)
                {
                    for(uchar y = 0; y < 128; y++)
                    {
                        for(uchar x = 0; x < 128; x++)
                        {
                            const uint i = PTI(x,y,z);
                            if(g.voxels[i] < 8){continue;}
                            const uint tu = g.colors[g.voxels[i]-1];
                            uchar r = (tu & 0x00FF0000) >> 16;
                            uchar gc = (tu & 0x0000FF00) >> 8;
                            uchar b = (tu & 0x000000FF);
                            if(r != 0 || gc != 0 || b != 0)
                            {
                                const int nx0 = PTIB((int)x-1, (int)y, (int)z);
                                const int nx1 = PTIB((int)x+1, (int)y, (int)z);
                                const int ny0 = PTIB((int)x, (int)y-1, (int)z);
                                const int ny1 = PTIB((int)x, (int)y+1, (int)z);
                                const int nz0 = PTIB((int)x, (int)y, (int)z-1);
                                const int nz1 = PTIB((int)x, (int)y, (int)z+1);
                                if( nx0 < 0 || ny0 < 0 || nz0 < 0 ||
                                    nx1 < 0 || ny1 < 0 || nz1 < 0 ||
                                    g.voxels[nx0] == 0 ||
                                    g.voxels[nx1] == 0 ||
                                    g.voxels[ny0] == 0 ||
                                    g.voxels[ny1] == 0 ||
                                    g.voxels[nz0] == 0 ||
                                    g.voxels[nz1] == 0 )
                                {
                                    fprintf(f, "%i %i %i %02X%02X%02X\n", ((int)x)-64, ((int)y)-64, z, r, gc, b);
                                }
                            }
                        }
                    }
                }
                fclose(f);
                char tmp[16];
                timestamp(tmp);
                printf("[%s] Exported VV: %s\n", tmp, export_path);
            }
        }
        if(export_type == 3)
        {
            FILE* f = fopen(export_path, "w");
            if(f != NULL)
            {
                const int greedy = (ply_mode == 0);
                const int tris = (ply_mode == 2);
                ply_mem_reset();
                const uint nquad = greedy ? ply_greedy_mesh(NULL, 1) : ply_cube_mesh(NULL, 1);
                const uint vc = nquad * 4;
                const uint nface = tris ? (nquad * 2) : nquad;
                const char* mode_name = greedy ? "greedy" : (tris ? "tris" : "quads");
                fprintf(f, "ply\n");
                fprintf(f, "format ascii 1.0\n");
                fprintf(f, "comment Created by %s %s - woxels.github.io\n", appTitle, appVersion);
                if(greedy)
                    fprintf(f, "comment greedy-meshed quads, same-color faces merged\n");
                else if(tris)
                    fprintf(f, "comment per-voxel-face triangles (two tris per cube face)\n");
                else
                    fprintf(f, "comment per-voxel-face quads\n");
                fprintf(f, "element vertex %u\n", vc);
                fprintf(f, "property float x\n");
                fprintf(f, "property float y\n");
                fprintf(f, "property float z\n");
                fprintf(f, "property float nx\n");
                fprintf(f, "property float ny\n");
                fprintf(f, "property float nz\n");
                fprintf(f, "property uchar red\n");
                fprintf(f, "property uchar green\n");
                fprintf(f, "property uchar blue\n");
                fprintf(f, "element face %u\n", nface);
                fprintf(f, "property list uchar uint vertex_indices\n");
                fprintf(f, "end_header\n");
                ply_mem_flush(f);
                ply_mem_free();
                for(uint i = 0, t = 0; i < nquad; i++)
                {
                    const uint i0 = t++;
                    const uint i1 = t++;
                    const uint i2 = t++;
                    const uint i3 = t++;
                    if(tris)
                    {
                        fprintf(f, "3 %u %u %u\n", i0, i2, i3);
                        fprintf(f, "3 %u %u %u\n", i0, i1, i2);
                    }
                    else
                        fprintf(f, "4 %u %u %u %u\n", i0, i1, i2, i3);
                }
                fclose(f);
                char tmp[16];
                timestamp(tmp);
                if(tris)
                    printf("[%s] Exported PLY: %s (%s, %u tris)\n", tmp, export_path, mode_name, nface);
                else
                    printf("[%s] Exported PLY: %s (%s, %u quads)\n", tmp, export_path, mode_name, nquad);
            }
        }
        return 0;
    }

    // custom mouse sensitivity
    if(argc >= 3)
    {
        g.sens = atof(argv[2]);
        if(g.sens == 0.f){g.sens = 0.003f;}
        char tmp[16];
        timestamp(tmp);
        printf("[%s] Custom mouse sensitivity applied to project \"%s\".\n", tmp, openTitle);
    }

    // load custom palette
    if(argc >= 4){loadColors(argv[3]);}

//*************************************
// window creation
//*************************************
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) < 0)
    {
        printf("ERROR: SDL_Init(): %s\n", SDL_GetError());
        return 1;
    }
    int msaa = 0;
    if(msaa > 0)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    if(isWayland() == 1)
    {
        wayland = 1;
        wnd = SDL_CreateWindow(appTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winw, winh, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
        while(wnd == NULL)
        {
            msaa--;
            if(msaa == 0)
            {
                printf("ERROR: SDL_CreateWindow(): %s\n", SDL_GetError());
                return 1;
            }
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa);
            wnd = SDL_CreateWindow(appTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winw, winh, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
        }
    }
    else
    {
        wayland = 0;
        wnd = SDL_CreateWindow(appTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winw, winh, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
        while(wnd == NULL)
        {
            msaa--;
            if(msaa == 0)
            {
                printf("ERROR: SDL_CreateWindow(): %s\n", SDL_GetError());
                return 1;
            }
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa);
            wnd = SDL_CreateWindow(appTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winw, winh, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
        }
    }
    SDL_GL_SetSwapInterval(1);
    glc = SDL_GL_CreateContext(wnd);
    if(glc == NULL)
    {
        printf("ERROR: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        return 1;
    }

    // callback for custom decor
    if(wayland == 1){SDL_SetWindowHitTest(wnd, hitTest, NULL);}

    // set icon
    s_icon = surfaceFromData((Uint32*)&icon, 16, 16);
    SDL_SetWindowIcon(wnd, s_icon);

    // window title
    char nt[512];
    sprintf(nt, "%s - %s", appTitle, openTitle);
    SDL_SetWindowTitle(wnd, nt);

    if(argc >= 2 && strcmp(argv[1], "debug") == 0)
    {
        printf("----\nDEBUG\n----\n");
        printAttrib(SDL_GL_DOUBLEBUFFER, "GL_DOUBLEBUFFER");
        printAttrib(SDL_GL_DEPTH_SIZE, "GL_DEPTH_SIZE");
        printAttrib(SDL_GL_RED_SIZE, "GL_RED_SIZE");
        printAttrib(SDL_GL_GREEN_SIZE, "GL_GREEN_SIZE");
        printAttrib(SDL_GL_BLUE_SIZE, "GL_BLUE_SIZE");
        printAttrib(SDL_GL_ALPHA_SIZE, "GL_ALPHA_SIZE");
        printAttrib(SDL_GL_BUFFER_SIZE, "GL_BUFFER_SIZE");
        printAttrib(SDL_GL_STENCIL_SIZE, "GL_STENCIL_SIZE");
        printAttrib(SDL_GL_ACCUM_RED_SIZE, "GL_ACCUM_RED_SIZE");
        printAttrib(SDL_GL_ACCUM_GREEN_SIZE, "GL_ACCUM_GREEN_SIZE");
        printAttrib(SDL_GL_ACCUM_BLUE_SIZE, "GL_ACCUM_BLUE_SIZE");
        printAttrib(SDL_GL_ACCUM_ALPHA_SIZE, "GL_ACCUM_ALPHA_SIZE");
        printAttrib(SDL_GL_STEREO, "GL_STEREO");
        printAttrib(SDL_GL_MULTISAMPLEBUFFERS, "GL_MULTISAMPLEBUFFERS");
        printAttrib(SDL_GL_MULTISAMPLESAMPLES, "GL_MULTISAMPLESAMPLES");
        printAttrib(SDL_GL_ACCELERATED_VISUAL, "GL_ACCELERATED_VISUAL");
        printAttrib(SDL_GL_RETAINED_BACKING, "GL_RETAINED_BACKING");
        printAttrib(SDL_GL_CONTEXT_MAJOR_VERSION, "GL_CONTEXT_MAJOR_VERSION");
        printAttrib(SDL_GL_CONTEXT_MINOR_VERSION, "GL_CONTEXT_MINOR_VERSION");
        printAttrib(SDL_GL_CONTEXT_FLAGS, "GL_CONTEXT_FLAGS");
        printAttrib(SDL_GL_CONTEXT_PROFILE_MASK, "GL_CONTEXT_PROFILE_MASK");
        printAttrib(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, "GL_SHARE_WITH_CURRENT_CONTEXT");
        printAttrib(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, "GL_FRAMEBUFFER_SRGB_CAPABLE");
        printAttrib(SDL_GL_CONTEXT_RELEASE_BEHAVIOR, "GL_CONTEXT_RELEASE_BEHAVIOR");
        printAttrib(SDL_GL_CONTEXT_EGL, "GL_CONTEXT_EGL");
        printf("----\n");
        printf("tseT_resU aka (xaH)\n");
        printf("gubaton.gro\\resu_tset\n");
        printf("semaJmailliWrehctelF\n");
        printf("buhtig.moc\\dibrm\n");
        printf("----\n");
        SDL_version compiled;
        SDL_version linked;
        SDL_VERSION(&compiled);
        SDL_GetVersion(&linked);
        printf("Compiled against SDL version %u.%u.%u.\n", compiled.major, compiled.minor, compiled.patch);
        printf("Linked against SDL version %u.%u.%u.\n", linked.major, linked.minor, linked.patch);
        printf("----\n");
        printf("currentPath: %s\n", basedir);
        printf("dataPath:    %s\n", appdir);
        printf("----\n");
    }

    // is this an rtx card? (lol don't need this anymore, but an amusing relic non-the-less)
    // if(strstr(glGetString(GL_RENDERER), "RTX") != NULL)
    //     SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Uh-oh! RTX DETECTED!", "We see that you are using an RTX graphics card.\n\nWe won't stop you using this software but please be aware this program has\nvery poor to basically an unusable experience on RTX graphics cards.\n\n ... ironically.\n\nConsider using Woxel.xyz the Web version or version 1.3 at github.com/woxels/Woxel", wnd);

//*************************************
// projection & compile & link shader program
//*************************************
    makeHud();
    shadeHud(&position_id, &hud_id, &look_pos_id, &scale_id, &view_id, &voxel_id);
    glUniform2f(scale_id, xscale, yscale);
    glBindBuffer(GL_ARRAY_BUFFER, mdlPlane.vid);
    glVertexAttribPointer(position_id, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(position_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mdlPlane.iid);
    WOX_POP(winw, winh);

//*************************************
// configure render options
//*************************************
    glDisable(GL_DEPTH_TEST);
    glLineWidth(0.f);
    glClearColor(0.f, 0.f, 0.f, 0.f);

//*************************************
// final init stuff
//*************************************
    sVoxel = SDL_RGBA32Surface(1024, 2048);
    for (int x = 0; x < 1024; x++)
    for (int y = 0; y < 2048; y++) {
        int index = (x * 2048) + y;
        if (g.voxels[index] < 1) {
	    setpixel(sVoxel, x, y, 0x00000000);
        } else {
            uint32_t color = g.colors[g.voxels[index]-1];
            color = (color >> 16) | (((color >> 8) & 0xFF) << 8) | ((color & 0xFF) << 16) | (0xFF << 24);
            setpixel(sVoxel, x, y, color);
        }
    }
    voxelmap = esLoadTextureA(1024, 2048, sVoxel->pixels, 0);
    flipHud();
    updateSelectColor();

//*************************************
// execute update / render loop
//*************************************
    t = fTime();
    while(1){main_loop();}
    return 0;
}
