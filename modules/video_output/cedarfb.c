/*****************************************************************************
 * cedarfb.c : cedar framebuffer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2009 the VideoLAN team
 *
 * Authors: Wills Wang <wills.wang@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <signal.h>                                      /* SIGUSR1, SIGUSR2 */
#include <fcntl.h>                                                 /* open() */
#include <unistd.h>                                               /* close() */

#include <termios.h>                                       /* struct termios */
#include <sys/ioctl.h>
#include <sys/mman.h>                                              /* mmap() */

#include <libcedarx/libcedarx.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>
#include <vlc_fs.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define PIXEL_ALIGN(x)  (((x) & 0x3F)? (((x) + 0x3F) & (~0x3F)) : (x))
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_shortname("Cedarfb")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_description(N_("Cedar video output"))
    set_capability("vout display", 30)
    set_callbacks(Open, Close)
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           Display(vout_display_t *, picture_t *, subpicture_t *);
static int            Control(vout_display_t *, int, va_list);

/* */
struct vout_display_sys_t {
    /* Video memory */
    picture_t **picture;
    picture_pool_t *pool;
    picture_t *last;
    picture_t *cur;
    int picture_count;
};


/**
 * This function allocates and initializes a FB vout method.
 */
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

    switch (vd->fmt.i_chroma) {
        case VLC_CODEC_MV12:
            break;
        default:
            vd->fmt.i_chroma = VLC_CODEC_MV12;
            break;
    }

    /* Allocate instance and initialize some members */
    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    /* */    
    sys->picture = NULL;
    sys->pool = NULL;
    sys->last = NULL;
    sys->cur = NULL;
    sys->picture_count = 0;
    
    if (libcedarx_display_open() < 0)
        goto free_out;

    if (libcedarx_display_request_layer(0) < 0)
        goto close_out;

    /* */
    vd->info.has_hide_mouse = true;
    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = NULL;

    msg_Dbg( vd, "Opened sun4i framebuffer with success" );
    return VLC_SUCCESS;

close_out:
    libcedarx_display_close();
free_out:
    free(sys);
    return VLC_EGENERIC;
}

/**
 * Terminate an output method created by Open
 */
static void Close(vlc_object_t *object)
{
    int i;
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;
    cedarx_picture_t pic;

    msg_Dbg( vd, "done cleaning up sun4i framebuffer" );
    if (sys) {
        libcedarx_display_release_layer();
        if (sys->cur)
            picture_Release(sys->last);
        if (sys->last)
            picture_Release(sys->last);
        
        if (sys->picture) {
            for (i = 0; i < sys->picture_count; i ++) {
                if (sys->picture[i]->p[0].p_pixels || sys->picture[i]->p[1].p_pixels) {
                    memset(&pic, 0, sizeof(cedarx_picture_t));
                    pic.y[0] = sys->picture[i]->p[0].p_pixels;
                    pic.u[0] = sys->picture[i]->p[1].p_pixels;
                    pic.size_y[0] = sys->picture[i]->p[0].i_pitch * \
                                    sys->picture[i]->p[0].i_lines;
                    pic.size_u[0] = sys->picture[i]->p[1].i_pitch * \
                                    sys->picture[i]->p[1].i_lines;
                    libcedarx_display_free_frame(&pic);
                }
            }
            free(sys->picture);
        }
            
        libcedarx_display_close();
        
        if (sys->pool) {
            picture_pool_Delete(sys->pool);
        }
        
        free(sys);
        vd->sys = NULL;
    }
}

/* */
static int Lock(picture_t *picture)
{
    cedarx_picture_t pic;
    
    if (picture && !picture->p[0].p_pixels && !picture->p[0].p_pixels) {
        memset(&pic, 0, sizeof(cedarx_picture_t));
        pic.size_y[0] = picture->p[0].i_pitch * picture->p[0].i_lines;
        pic.size_u[0] = picture->p[1].i_pitch * picture->p[1].i_lines;
        if (libcedarx_display_alloc_frame(&pic) < 0)
            return VLC_ENOMEM;
        picture->p[0].p_pixels = pic.y[0];
        picture->p[1].p_pixels = pic.u[0];
    }
    return VLC_SUCCESS;
}

/* */
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    unsigned i;
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool) {
        if (!sys->picture) {
            picture_resource_t rsc;

            sys->picture = malloc(count * sizeof(picture_t *));
            if (!sys->picture)
                return NULL;

            memset(&rsc, 0, sizeof(rsc));
            rsc.p[0].i_pitch = PIXEL_ALIGN(vd->fmt.i_width);
            rsc.p[0].i_lines = PIXEL_ALIGN(vd->fmt.i_height);
            rsc.p[1].i_pitch = PIXEL_ALIGN(vd->fmt.i_width);
            rsc.p[1].i_lines = PIXEL_ALIGN(vd->fmt.i_height / 2);
            for (i = 0; i < count; i ++) {

                sys->picture[i] = picture_NewFromResource(&vd->fmt, &rsc);
                if (!sys->picture[i]) {
                    sys->picture_count = i;
                    break;
                }
            }
            sys->picture_count = i;
        }

        /* */
        picture_pool_configuration_t pool;
        memset(&pool, 0, sizeof(pool));
        pool.picture_count = sys->picture_count;
        pool.picture       = sys->picture;
        pool.lock          = Lock;
        sys->pool = picture_pool_NewExtended(&pool);
        if (!sys->pool) {
            for (i = 0; i < sys->picture_count; i++) {
                picture_Release(sys->picture[i]);
            }
            free(sys->picture);
            sys->picture = NULL;
            sys->picture_count = 0;
        }
    }

    return sys->pool;
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    cedarx_picture_t pic;
    vout_display_sys_t *sys = vd->sys;

    VLC_UNUSED(subpicture);

    if (sys->last != picture) {
        memset(&pic, 0, sizeof(cedarx_picture_t));
        pic.width = picture->format.i_width;
        pic.height = picture->format.i_height;
        pic.top_offset = picture->format.i_x_offset;
        pic.left_offset = picture->format.i_y_offset;
        pic.display_width = picture->format.i_visible_width;
        pic.display_height = picture->format.i_visible_height;
        pic.display_height = picture->format.i_visible_height;
    	pic.y[0] = picture->p[0].p_pixels;                      
    	pic.u[0] = picture->p[1].p_pixels;                      
        pic.size_y[0] = picture->p[0].i_pitch * picture->p[0].i_lines;
        pic.size_u[1] = picture->p[1].i_pitch * picture->p[1].i_lines;
        pic.pts = picture->date;
        pic.frame_rate = picture->format.i_frame_rate * 1000 / picture->format.i_frame_rate_base;
        libcedarx_display_video_frame(&pic);
        if (sys->cur) {
            picture_Release(sys->cur);
        }
            
        sys->cur = sys->last;
        sys->last = picture;
    } else {
        picture_Release(picture);
    }
}

static int Control(vout_display_t *vd, int query, va_list arg)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query) {
    case VOUT_DISPLAY_CHANGE_FULLSCREEN:
	case VOUT_DISPLAY_CHANGE_SOURCE_CROP: 
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    case VOUT_DISPLAY_CHANGE_WINDOW_STATE: {
        return VLC_SUCCESS;
    }
    default:
        msg_Err(vd, "Unsupported query(%d) in vout display fb", query);
        return VLC_EGENERIC;
    }
    VLC_UNUSED(sys);
    VLC_UNUSED(arg);
}


