/*
--------------------------------------------------
    James William Fletcher (github.com/mrbid)
        August 2023
--------------------------------------------------
    C & SDL / OpenGL ES2 / GLSL ES
    Colour Converter: https://www.easyrgb.com
*/
#include "inc/excess.h"
//#define BENCH_FPS
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
    
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
            case SDL_KEYDOWN:
            {
                if(event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_TAB)
                {
                    focus_mouse = 1 - focus_mouse;
                    SDL_ShowCursor(1 - focus_mouse);
                    if(focus_mouse == 1)
                    {
                        mx = winw2, my = winh2;
                        lx = winw2, ly = winh2;
                        SDL_WarpMouseInWindow(wnd, winw2, winh2);
                    }
                }
                else if(event.key.keysym.sym == SDLK_F2)
                {
                    showhud = 1 - showhud;
                    if(showhud == 0)
                    {
                        shadeVoxel(&projection_id, &view_id, &position_id, &voxel_id);
                        glUniformMatrix4fv(projection_id, 1, GL_FALSE, (float*)&projection.m[0][0]);
                    }
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
                        g.voxels[lray]--;
                        g.st = g.voxels[lray];
                        if(g.st < 8.f || g.colors[g.voxels[lray]] == 0)
                        {
                            if(g.colors[0] != 0)
                            {
                                uint i = 7;
                                for(NULL; i < 40 && g.colors[i] != 0; i++){}
                                g.st = (float)(i-1);
                                g.voxels[lray] = i-1;
                            }
                        }
                        updateSelectColor();
                    }
                    else
                    {
                        g.st -= 1.f;
                        if(g.st < 8.f || g.colors[(uint)g.st] == 0)
                        {
                            if(g.colors[0] != 0)
                            {
                                uint i = 7;
                                for(NULL; i < 40 && g.colors[i] != 0; i++){}
                                g.st = (float)(i-1);
                            }
                        }
                        updateSelectColor();
                    }
                }
                else if(event.key.keysym.sym == SDLK_QUOTE || event.key.keysym.sym == SDLK_c) // + change selected node
                {
                    traceViewPath(0);
                    if(lray > -1 && g.voxels[lray] > 7)
                    {
                        g.voxels[lray]++;
                        g.st = g.voxels[lray];
                        if(g.st > 39.f || g.colors[g.voxels[lray]] == 0)
                        {
                            g.st = 8.f;
                            g.voxels[lray] = g.st;
                        }
                        updateSelectColor();
                    }
                    else
                    {
                        g.st += 1.f;
                        if(g.st > 39.f || g.colors[(uint)g.st] == 0){g.st = 8.f;}
                        updateSelectColor();
                    }
                }
                else if(event.key.keysym.sym == SDLK_RSHIFT) // place a voxel
                {
                    ptt = t+rrsp;
                    traceViewPath(1);
                    if(lray > -1)
                    {
                        if(g.pb.w == 1){g.voxels[PTI(g.pb.x, g.pb.y, g.pb.z)] = g.st;}
                        if(mirror == 1)
                        {
                            const float x = g.pb.x > 64.f ? 64.f+(64.f-g.pb.x) : 64.f + (64.f-g.pb.x);
                            g.voxels[PTI(x, g.pb.y, g.pb.z)] = g.st;
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
                    }
                }
                else if(event.key.keysym.sym == SDLK_q || event.key.keysym.sym == SDLK_z) // clone pointed voxel color
                {
                    traceViewPath(0);
                    //printf("%u\n", lray);
                    if(lray > -1 && g.voxels[lray] > 7)
                    {
                        g.st = g.voxels[lray];
                        updateSelectColor();
                    }
                }
                else if(event.key.keysym.sym == SDLK_e) // replace pointed voxel
                {
                    rtt = t+rrsp;
                    traceViewPath(0);
                    if(lray > -1)
                    {
                        g.voxels[lray] = g.st;
                        if(mirror == 1)
                        {
                            const float x = ghp.x > 64.f ? 64.f+(64.f-ghp.x) : 64.f + (64.f-ghp.x);
                            g.voxels[PTI(x, ghp.y, ghp.z)] = g.st;
                        }
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
                    if(isInBounds(rp) == 1){g.voxels[PTI(rp.x, rp.y, rp.z)] = 8;}
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
                    g.st += 1.f;
                    if(g.st > 39.f || g.colors[(uint)g.st-1] == 0){g.st = 8.f;}
                    updateSelectColor();
                }
                else if(event.wheel.y > 0)
                {
                    g.st -= 1.f;
                    if(g.st < 8.f || g.colors[(uint)g.st] == 0)
                    {
                        if(g.colors[0] != 0)
                        {
                            uint i = 7;
                            for(NULL; i < 39 && g.colors[i] != 0; i++){}
                            g.st = (float)(i);
                        }
                    }
                    updateSelectColor();
                }
                //printf("%.2f %u\n", g.st, g.colors[(uint)g.st]);
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
                if(event.button.button == SDL_BUTTON_LEFT){ptt = 0.f;}
                else if(event.button.button == SDL_BUTTON_RIGHT){dtt = 0.f;}
                else if(event.button.button == SDL_BUTTON_X2){rtt = 0.f;}
                md = 0;
                idle = t;
            }
            break;

            case SDL_MOUSEBUTTONDOWN:
            {
                lx = event.button.x;
                ly = event.button.y;
                mx = event.button.x;
                my = event.button.y;

                if(focus_mouse == 0) // lock mouse focus on every mouse input to the window
                {
                    SDL_ShowCursor(0);
                    focus_mouse = 1;
                    break;
                }

                if(event.button.button == SDL_BUTTON_LEFT) // place a voxel
                {
                    ptt = t+rrsp;
                    traceViewPath(1);
                    //printf("L: %u %f %u\n", lray, g.pb.w, g.voxels[lray]);
                    if(lray > -1)
                    {
                        if(g.pb.w == 1 && isInBounds(g.pb) && g.voxels[PTI(g.pb.x, g.pb.y, g.pb.z)] == 0)
                            g.voxels[PTI(g.pb.x, g.pb.y, g.pb.z)] = g.st;
                    
                        if(mirror == 1)
                        {
                            const float x = g.pb.x > 64.f ? 64.f+(64.f-g.pb.x) : 64.f + (64.f-g.pb.x);
                            g.voxels[PTI(x, g.pb.y, g.pb.z)] = g.st;
                            //printf("%f %f %f\n", x, g.pb.y, g.pb.z);
                        }
                    }
                }
                else if(event.button.button == SDL_BUTTON_RIGHT) // remove pointed voxel
                {
                    dtt = t+rrsp;
                    traceViewPath(0);
                    //printf("R: %u %f %u\n", lray, g.pb.w, g.voxels[lray]);
                    if(lray > -1)
                    {
                        g.voxels[lray] = 0;
                        if(mirror == 1)
                        {
                            const float x = ghp.x > 64.f ? 64.f+(64.f-ghp.x) : 64.f + (64.f-ghp.x);
                            g.voxels[PTI(x, ghp.y, ghp.z)] = 0;
                            //printf("%f %f %f\n", x, g.pb.y, g.pb.z);
                        }
                    }
                }
                else if(event.button.button == SDL_BUTTON_MIDDLE || event.button.button == SDL_BUTTON_X1) // clone pointed voxel
                {
                    traceViewPath(0);
                    if(lray > -1 && g.voxels[lray] > 7)
                    {
                        g.st = g.voxels[lray];
                        updateSelectColor();
                    }
                }
                else if(event.button.button == SDL_BUTTON_X2) // replace pointed node
                {
                    rtt = t+rrsp;
                    traceViewPath(0);
                    if(lray > -1)
                    {
                        g.voxels[lray] = g.st;
                        if(mirror == 1)
                        {
                            const float x = ghp.x > 64.f ? 64.f+(64.f-ghp.x) : 64.f + (64.f-ghp.x);
                            g.voxels[PTI(x, ghp.y, ghp.z)] = g.st;
                        }
                    }
                }
                idle = t;
            }
            break;

            case SDL_WINDOWEVENT:
            {
                if(event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    winw = event.window.data1;
                    winh = event.window.data2;
                    winw2 = winw/2;
                    winh2 = winh/2;
                    doPerspective();
                }
            }
            break;

            case SDL_QUIT:
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
                if(g.pb.w == 1){g.voxels[PTI(g.pb.x, g.pb.y, g.pb.z)] = g.st;}
                if(mirror == 1)
                {
                    const float x = g.pb.x > 64.f ? 64.f+(64.f-g.pb.x) : 64.f + (64.f-g.pb.x);
                    g.voxels[PTI(x, g.pb.y, g.pb.z)] = g.st;
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
            }
            dtt = t+0.1f;
        }

        if(rtt != 0.f && t > rtt) // replace trigger
        {
            traceViewPath(0);
            if(lray > -1)
            {
                g.voxels[lray] = g.st;
                if(mirror == 1)
                {
                    const float x = ghp.x > 64.f ? 64.f+(64.f-ghp.x) : 64.f + (64.f-ghp.x);
                    g.voxels[PTI(x, ghp.y, ghp.z)] = g.st;
                }
            }
            rtt = t+0.1f;
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

    mIdent(&view);
    mRotate(&view, g.yrot, 1.f, 0.f, 0.f);
    mRotate(&view, g.xrot, 0.f, 0.f, 1.f);
    mTranslate(&view, g.pp.x, g.pp.y, g.pp.z);

    mGetViewZ(&look_dir, view); // refresh

//*************************************
// begin render
//*************************************
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//*************************************
// main render
//*************************************

    // voxels
    if(showhud == 1)
    {
        shadeVoxel(&projection_id, &view_id, &position_id, &voxel_id);
        glUniformMatrix4fv(projection_id, 1, GL_FALSE, (float*)&projection.m[0][0]);
    }
    glBindBuffer(GL_ARRAY_BUFFER, mdlVoxel.vid);
    glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(position_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mdlVoxel.iid);

    // render voxels
    ipp = g.pp; // inverse player position (setting global 'ipp' here is perfect)
    vInv(&ipp); // <--
    glUniformMatrix4fv(view_id, 1, GL_FALSE, (float*)&view.m[0][0]);

    // for(uint i = 0; i < max_voxels; i++)
    // {
    //     if(g.voxels[i] == 0){continue;}
    //     if(g.colors[g.voxels[i]-1] != 0)
    //     {
    //         const float fi = (float)i;
    //         glUniform2f(voxel_id, fi, g.colors[g.voxels[i]-1]);
    //         glDrawElements(GL_TRIANGLES, voxel_numind, GL_UNSIGNED_BYTE, 0);
    //     }
    // }

    for(uchar z = 0; z < 128; z++)
    {
        for(uchar y = 0; y < 128; y++)
        {
            for(uchar x = 0; x < 128; x++)
            {
                if(insideFrustum(x,y,z) == 0){continue;}
                if( x <=   0 || y <=   0 || z <=   0 ||
                    x >= 127 || y >= 127 || z >= 127 ||
                    g.voxels[PTIB(x-1, y, z)] == 0 || // check if generally occluded (not by lookdir)
                    g.voxels[PTIB(x+1, y, z)] == 0 ||
                    g.voxels[PTIB(x, y-1, z)] == 0 ||
                    g.voxels[PTIB(x, y+1, z)] == 0 ||
                    g.voxels[PTIB(x, y, z-1)] == 0 ||
                    g.voxels[PTIB(x, y, z+1)] == 0 )
                {
                    const uint i = PTI(x,y,z);
                    if(g.voxels[i] == 0){continue;}
                    uint rc = g.colors[g.voxels[i]-1];
                    if(rc == 0){rc=16745211;}
                    const float fi = (float)i;
                    glUniform2f(voxel_id, fi, rc);
                    glDrawElements(GL_TRIANGLES, voxel_numind, GL_UNSIGNED_BYTE, 0);
                }
            }
        }
    }

    // canvas frame
    if(showhud == 1)
    {
        glUniform2f(voxel_id, 0.f, 65535.f);
        esRebind(GL_ARRAY_BUFFER, &mdlVoxel.vid, &(GLfloat[]){
                                                                127.5f, 127.5f, 127.5f,
                                                                -0.5f, 127.5f, 127.5f,
                                                                -0.5f, 127.5f, -0.5f,
                                                                127.5f, 127.5f, -0.5f,
                                                                127.5f, -0.5f, 127.5f, //
                                                                -0.5f, -0.5f, 127.5f,
                                                                -0.5f, -0.5f, -0.5f,
                                                                127.5f, -0.5f, -0.5f,
                                                            }
                                                            , 8 * 3 * sizeof(float), GL_STATIC_DRAW);
        esRebind(GL_ELEMENT_ARRAY_BUFFER, &mdlVoxel.iid, &(GLbyte[]){0,1,2,3,0,4,5,6,7,4,5,1,2,6,7,3}, 16, GL_STATIC_DRAW);
        glDrawElements(GL_LINE_STRIP, voxel_numind, GL_UNSIGNED_BYTE, 0);
        esRebind(GL_ARRAY_BUFFER,         &mdlVoxel.vid, voxel_vertices, sizeof(voxel_vertices), GL_STATIC_DRAW);
        esRebind(GL_ELEMENT_ARRAY_BUFFER, &mdlVoxel.iid, voxel_indices,  sizeof(voxel_indices),  GL_STATIC_DRAW);

        // hud
        drawHud(focus_mouse);
        shadeHud(&position_id, &texcoord_id, &sampler_id);
        glUniformMatrix4fv(projection_id, 1, GL_FALSE, (float*)&projection.m[0][0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hudmap);
        glUniform1i(sampler_id, 0);
        glBindBuffer(GL_ARRAY_BUFFER, mdlPlane.vid);
        glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(position_id);
        glBindBuffer(GL_ARRAY_BUFFER, mdlPlane.tid);
        glVertexAttribPointer(texcoord_id, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(texcoord_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mdlPlane.iid);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
            glDrawElements(GL_TRIANGLES, hud_numind, GL_UNSIGNED_BYTE, 0);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

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
        // fps
        char tmp[16];
        sprintf(tmp, "%u", g_fps);
        SDL_FillRect(sHud, &(SDL_Rect){0, 0, lenText(tmp)+8, 19}, 0xCC000000);
        drawText(sHud, tmp, 4, 4, 2);
        // center hud
        const int left = winw2-177;
        int top = winh2-152;
        SDL_FillRect(sHud, &(SDL_Rect){winw2-193, top-3, 382, 281}, 0x33FFFFFF);
        SDL_FillRect(sHud, &(SDL_Rect){winw2-190, top, 376, 275}, 0xCC000000);
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
    
        for(uint i = 0; i < 16; i++)
        {
            const uint left2 = left+(i*22);
            {
                uint tu = g.colors[7+i];
                if(tu != 0)
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
                if(tu != 0)
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
    else
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
            }
        }

        const int hso = winw2-175;
        for(uint i = 0; i < 16; i++)
        {
            const uint left = hso+(i*22);
            {
                uint tu = g.colors[7+i];
                if(tu != 0)
                {
                    uchar r = (tu & 0x00FF0000) >> 16;
                    uchar gc = (tu & 0x0000FF00) >> 8;
                    uchar b = (tu & 0x000000FF);
                    if(g.st-1 == 7+i)
                        SDL_FillRect(sHud, &(SDL_Rect){left, 11, 20, 20}, 0xFFFFFFFF);
                    else
                        SDL_FillRect(sHud, &(SDL_Rect){left, 11, 20, 20}, 0xFF000000);
                    SDL_FillRect(sHud, &(SDL_Rect){left+1, 12, 18, 18}, SDL_MapRGB(sHud->format, r,gc,b));
                }
            }
            {
                uint tu = g.colors[23+i];
                if(tu != 0)
                {
                    uchar r = (tu & 0x00FF0000) >> 16;
                    uchar gc = (tu & 0x0000FF00) >> 8;
                    uchar b = (tu & 0x000000FF);
                    if(g.st-1 == 23+i)
                        SDL_FillRect(sHud, &(SDL_Rect){left, 33, 20, 20}, 0xFFFFFFFF);
                    else
                        SDL_FillRect(sHud, &(SDL_Rect){left, 33, 20, 20}, 0xFF000000);
                    SDL_FillRect(sHud, &(SDL_Rect){left+1, 34, 18, 18}, SDL_MapRGB(sHud->format, r,gc,b));
                }
            }
        }
    }

    // flip the new hud to gpu
    flipHud();
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
    printf("\nMouse locks when you click on the game window, press ESCAPE/TAB to unlock the mouse.\n\n");
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
    printf("F3 = Save. (auto saves on exit, backup made if idle for 3 mins.)\n");
    printf("F8 = Load. (will erase what you have done since the last save)\n");
    printf("\n* Arrow Keys can be used to move the view around.\n");
    printf("* Your state is automatically saved on exit.\n");
    printf("\nConsole Arguments:\n");
    printf("./wox <project_name> <mouse_sensitivity> <color_palette_file_path>\n");
    printf("e.g; ./wox Untitled 0.003 /tmp/colors.txt\n");
    printf("1st, \"Untitled\", Name of project to open or create.\n");
    printf("2nd, \"0.003\", Mouse sensitivity.\n");
    printf("3rd, \"/tmp/colors.txt\", path to a color palette file, the file must contain a hex\n");
    printf("color on each new line, 32 colors maximum. e.g; \"#00FFFF\".\n\n");
    printf("To load from file: ./wox loadgz <file_path>\n");
    printf("e.g; ./wox loadgz /home/user/file.wox.gz\n\n");
    printf("To export: ./wox export <project_name> <option: wox,txt,vv,ply> <export_path>\n");
    printf("e.g; ./wox export txt /home/user/file.txt\n");
    printf("When exporting as ply you will want to merge vertices by distance in Blender\nor `Cleaning and Repairing > Merge Close Vertices` in MeshLab.\n\n");
    printf("Find more color palettes at; https://lospec.com/palette-list\n");
    printf("You can use any palette upto 32 colors. But don't use #000000 (Black)\nin your color palette as it will terminate at that color.\n\n");
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
    uint export_type = 0;
    if(argc >= 2 && strlen(argv[1]) < 256)
    {
        sprintf(openTitle, "%s", argv[1]);
    }
    if(argc >= 3 && strcmp(argv[1], "loadgz") == 0 && strlen(argv[2]) < 256)
    {
        sprintf(openTitle, "%s", argv[2]);
        load_state = 1;
    }
    if(argc >= 5 && strcmp(argv[1], "export") == 0 && strlen(argv[2]) < 256 && strlen(argv[4]) < 1024)
    {
        sprintf(openTitle, "%s", argv[2]);

        if     (strcmp(argv[3], "txt") == 0){export_type=1;}
        else if(strcmp(argv[3], "vv") == 0){export_type=2;}
        else if(strcmp(argv[3], "ply") == 0){export_type=3;}

        sprintf(export_path, "%s", argv[4]);
    }

    // default state
    uint pls=0;
    if(loadState(openTitle, load_state) == 0)
    {
        defaultState(0);
        memset(&g.voxels, 0, max_voxels);
        //
        g.voxels[PTI(64,64,64)] = 1; // center
        g.voxels[PTI(64,64,0)] = 7;
        g.voxels[PTI(0,64,64)] = 3;
        g.voxels[PTI(64,0,64)] = 5;
        g.voxels[PTI(64,64,127)] = 6;
        g.voxels[PTI(127,64,64)] = 2;
        g.voxels[PTI(64,127,64)] = 4;
        //
        g.voxels[PTI(0,0,0)] = 1;
        g.voxels[PTI(127,127,127)] = 1;
        g.voxels[PTI(0,127,127)] = 1;
        g.voxels[PTI(127,127,0)] = 1;
        g.voxels[PTI(127,0,0)] = 1;
        g.voxels[PTI(0,0,127)] = 1;
        g.voxels[PTI(127,0,127)] = 1;
        g.voxels[PTI(0,127,0)] = 1;
        //
        // system palette
        g.colors[0] = 16448250;
        g.colors[1] = 16711680;
        g.colors[2] = 4194304;
        g.colors[3] = 65280;
        g.colors[4] = 16384;
        g.colors[5] = 255;
        g.colors[6] = 64;
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
                            if(r != 0 && gc != 0 && b != 0)
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
                            if(r != 0 && gc != 0 && b != 0)
                            {
                                if( x <=   0 || y <=   0 || z <=   0 ||
                                    x >= 127 || y >= 127 || z >= 127 ||
                                    g.voxels[PTIB(x-1, y, z)] == 0 ||
                                    g.voxels[PTIB(x+1, y, z)] == 0 ||
                                    g.voxels[PTIB(x, y-1, z)] == 0 ||
                                    g.voxels[PTIB(x, y+1, z)] == 0 ||
                                    g.voxels[PTIB(x, y, z-1)] == 0 ||
                                    g.voxels[PTIB(x, y, z+1)] == 0 )
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
                // I should be doing this in the single loop writing to memory so
                // that I can append the header later for writing to file.
                uint vc = 0;
                for(uchar z = 0; z < 128; z++){
                    for(uchar y = 0; y < 128; y++){
                        for(uchar x = 0; x < 128; x++){
                            const uint i = PTI(x,y,z);
                            if(g.voxels[i] < 8){continue;}
                            const uint tu = g.colors[g.voxels[i]-1];
                            uchar r = (tu & 0x00FF0000) >> 16;
                            uchar gc = (tu & 0x0000FF00) >> 8;
                            uchar b = (tu & 0x000000FF);
                            if(r != 0 && gc != 0 && b != 0)
                            {
                                if(g.voxels[PTIB(x-1, y, z)] == 0){vc+=6;}
                                if(g.voxels[PTIB(x+1, y, z)] == 0){vc+=6;}
                                if(g.voxels[PTIB(x, y-1, z)] == 0){vc+=6;}
                                if(g.voxels[PTIB(x, y+1, z)] == 0){vc+=6;}
                                if(g.voxels[PTIB(x, y, z-1)] == 0){vc+=6;}
                                if(g.voxels[PTIB(x, y, z+1)] == 0){vc+=6;}
                }}}}
                // but it's unlikely to ever be that expensive that anyone would notice this inefficiency
                fprintf(f, "ply\n");
                fprintf(f, "format ascii 1.0\n");
                fprintf(f, "comment Created by %s %s - woxels.github.io\n", appTitle, appVersion);
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
                const uint faces = vc/3;
                fprintf(f, "element face %u\n", faces);
                fprintf(f, "property list uchar uint vertex_indices\n");
                fprintf(f, "end_header\n");
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
                            if(r != 0 && gc != 0 && b != 0)
                            {
                                if(g.voxels[PTIB(x-1, y, z)] == 0){fw_mx(f, x, y, z, r, gc, b);}
                                if(g.voxels[PTIB(x+1, y, z)] == 0){fw_px(f, x, y, z, r, gc, b);}
                                if(g.voxels[PTIB(x, y-1, z)] == 0){fw_my(f, x, y, z, r, gc, b);}
                                if(g.voxels[PTIB(x, y+1, z)] == 0){fw_py(f, x, y, z, r, gc, b);}
                                if(g.voxels[PTIB(x, y, z-1)] == 0){fw_mz(f, x, y, z, r, gc, b);}
                                if(g.voxels[PTIB(x, y, z+1)] == 0){fw_pz(f, x, y, z, r, gc, b);}
                            }
                        }
                    }
                }
                // merge vertices by distance in Blender or `Cleaning and Repairing > Merge Close Vertices` in MeshLab
                for(int i = 0, t = 0; i < faces; i++)
                {
                    const int i1 = t++;
                    const int i2 = t++;
                    const int i3 = t++;
                    fprintf(f, "3 %i %i %i\n", i1, i2, i3);
                }
                fclose(f);
                char tmp[16];
                timestamp(tmp);
                printf("[%s] Exported PLY: %s\n", tmp, export_path);
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
    int msaa = 16;
    if(msaa > 0)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    wnd = SDL_CreateWindow(appTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winw, winh, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    while(wnd == NULL)
    {
        if(msaa == 0)
        {
            printf("ERROR: SDL_CreateWindow(): %s\n", SDL_GetError());
            return 1;
        }
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa/2);
        wnd = SDL_CreateWindow(appTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winw, winh, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    }
    SDL_GL_SetSwapInterval(1);
    glc = SDL_GL_CreateContext(wnd);
    if(glc == NULL)
    {
        printf("ERROR: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        return 1;
    }

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
        printf("semaJmailliWrehctelF\n");
        printf("buhtig.moc\\dibrm\n");
        //printf("James William Fletcher (github.com/mrbid)\n");
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

//*************************************
// projection & compile & link shader program
//*************************************
    doPerspective();
    makeVoxel();
    makeHud();

//*************************************
// configure render options
//*************************************
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(0.f);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    shadeVoxel(&projection_id, &view_id, &position_id, &voxel_id);
    glUniformMatrix4fv(projection_id, 1, GL_FALSE, (float*)&projection.m[0][0]);

//*************************************
// final init stuff
//*************************************
    sHud = SDL_RGBA32Surface(winw, winh);
    drawHud(0);
    hudmap = esLoadTextureA(winw, winh, sHud->pixels, 0);
    updateSelectColor();

//*************************************
// execute update / render loop
//*************************************
    t = fTime();
    while(1){main_loop();}
    return 0;
}
