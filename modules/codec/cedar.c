/*****************************************************************************
 * cedar.c: Cedar decoder
 *****************************************************************************
 * Copyright © 2010-2012 VideoLAN
 *
 * Authors: Wills Wang <wills.wang@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* VLC includes */
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_threads.h>

/* Cedar */
#include <libcedarx/libcedarx.h>
#include <assert.h>

#define DECODE_USE_ASYNC_THREAD           1

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int        OpenDecoder  (vlc_object_t *);
static void       CloseDecoder (vlc_object_t *);

vlc_module_begin ()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_description(N_("Cedar hardware video decoder"))
    set_capability("decoder", 0)
    set_callbacks(OpenDecoder, CloseDecoder)
    add_shortcut("cedar")
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_t *DecodeBlock   (decoder_t *p_dec, block_t **pp_block);

/*****************************************************************************
 * decoder_sys_t : Cedar decoder structure
 *****************************************************************************/
struct decoder_sys_t
{
	int dummy;
#ifdef DECODE_USE_ASYNC_THREAD
    vlc_thread_t thread;
#endif
};

#ifdef DECODE_USE_ASYNC_THREAD
static void *DecoderThread(void *data)
{
    decoder_t *p_dec = data;

    for(;;)
    {
        if (libcedarx_decoder_decode_stream() < 0)
            msg_Err(p_dec, "Failed to decode stream!");
    }

    return NULL;
}
#endif

/*****************************************************************************
* OpenDecoder: probe the decoder and return score
*****************************************************************************/
static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    cedarx_info_t info;

    memset(&info, 0, sizeof(cedarx_info_t));
    /* Codec specifics */
    switch (p_dec->fmt_in.i_codec)
    {
        case VLC_CODEC_H264:
            switch(p_dec->fmt_in.i_original_fourcc)
            {
                case VLC_FOURCC('a','v','c','1'):
                case VLC_FOURCC('A','V','C','1'):
                    info.stream = CEDARX_STREAM_FORMAT_AVC1;
                    break;
                default:
                    info.stream = CEDARX_STREAM_FORMAT_H264;
                    break;
            }
            break;
        case VLC_CODEC_VC1:
        case VLC_CODEC_WMV3:
            info.stream = CEDARX_STREAM_FORMAT_VC1;
            break;
        case VLC_CODEC_H263:
            info.stream = CEDARX_STREAM_FORMAT_H263;
            break;
        case VLC_CODEC_FLV1:
            info.stream = CEDARX_STREAM_FORMAT_SORENSSON_H263;
            break;
        case VLC_CODEC_MJPG:
            info.stream = CEDARX_STREAM_FORMAT_MJPEG;
            break;
        case VLC_CODEC_VP6:
            info.stream = CEDARX_STREAM_FORMAT_VP6;
            break;
        case VLC_CODEC_VP8:
            info.stream = CEDARX_STREAM_FORMAT_VP8;
            break;
        case VLC_CODEC_WMV1:
            info.stream = CEDARX_STREAM_FORMAT_WMV1;
            break;
        case VLC_CODEC_WMV2:
            info.stream = CEDARX_STREAM_FORMAT_WMV2;
            break;
        case VLC_CODEC_RV10:
        case VLC_CODEC_RV20:
        case VLC_CODEC_RV30:
        case VLC_CODEC_RV40:
            info.stream = CEDARX_STREAM_FORMAT_REALVIDEO;
            break;
        case VLC_CODEC_CAVS:
            info.stream = CEDARX_STREAM_FORMAT_AVS;
            break;
        case VLC_CODEC_MP4V:
            switch(p_dec->fmt_in.i_original_fourcc)
            {
                case VLC_FOURCC('d','i','v','x'):
                case VLC_FOURCC('D','I','V','X'):
                    info.stream = CEDARX_STREAM_FORMAT_DIVX4;
                    break;
                case VLC_FOURCC('D','X','5','0'):
                case VLC_FOURCC('d','x','5','0'):
                    info.stream = CEDARX_STREAM_FORMAT_DIVX5;
                    break;
                default:
                    info.stream = CEDARX_STREAM_FORMAT_XVID;
                    break;
            }
            break;
        case VLC_CODEC_DIV1:
            info.stream = CEDARX_STREAM_FORMAT_DIVX1;
            break;
        case VLC_CODEC_DIV2:
            info.stream = CEDARX_STREAM_FORMAT_DIVX2;
            break;
        case VLC_CODEC_DIV3:
            switch(p_dec->fmt_in.i_original_fourcc)
            {
                case VLC_FOURCC('d','i','v','5'):
                case VLC_FOURCC('D','I','V','5'):
                    info.stream = CEDARX_STREAM_FORMAT_DIVX5;
                    break;
                case VLC_FOURCC('d','i','v','4'):
                case VLC_FOURCC('D','I','V','4'):
                case VLC_FOURCC('d','i','v','f'):
                case VLC_FOURCC('D','I','V','F'):
                  info.stream = CEDARX_STREAM_FORMAT_DIVX4;
                  break;
                default:
                  info.stream = CEDARX_STREAM_FORMAT_DIVX3;
                  break;
            }
            break;
        case VLC_CODEC_MPGV:
            switch(p_dec->fmt_in.i_original_fourcc)
            {
                case VLC_FOURCC('m','p','g','1'):
                case VLC_FOURCC('m','p','1','v'):
                  info.stream = CEDARX_STREAM_FORMAT_MPEG1;
                  break;
                default:
                  info.stream = CEDARX_STREAM_FORMAT_MPEG2;
                  break;
            }
            break;
        default:
            return VLC_EGENERIC;
    }

    info.container = CEDARX_CONTAINER_FORMAT_UNKNOW;
    info.width = p_dec->fmt_in.video.i_width;
    info.height = p_dec->fmt_in.video.i_height;
    info.data = p_dec->fmt_in.p_extra;
    info.data_size = p_dec->fmt_in.i_extra;
    if(p_dec->fmt_in.video.i_frame_rate > 0 && p_dec->fmt_in.video.i_frame_rate_base > 0) {
        info.frame_rate = INT64_C(1000) * 
                p_dec->fmt_in.video.i_frame_rate / 
                p_dec->fmt_in.video.i_frame_rate_base;
        info.frame_duration = INT64_C(1000000) * 
                p_dec->fmt_in.video.i_frame_rate_base / 
                p_dec->fmt_in.video.i_frame_rate;
    }

    /* Open the device */
    if(libcedarx_decoder_open(&info) < 0)
    {
        msg_Err(p_dec, "Couldn't find and open the Cedar device");  
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    p_sys = malloc(sizeof(decoder_sys_t));
    if(!p_sys)
        return VLC_ENOMEM;

    /* Fill decoder_sys_t */
    p_dec->p_sys = p_sys;

#ifdef DECODE_USE_ASYNC_THREAD
    if(vlc_clone(&p_sys->thread, DecoderThread, p_dec, VLC_THREAD_PRIORITY_VIDEO)) {
        free(p_sys);
        libcedarx_decoder_close();
        return VLC_EGENERIC;
    }
#endif
        
    /* Set output properties */
    p_dec->fmt_out.i_cat          = VIDEO_ES;
    p_dec->fmt_out.i_codec        = VLC_CODEC_MV12;
    p_dec->fmt_out.video.i_width  = p_dec->fmt_in.video.i_width;
    p_dec->fmt_out.video.i_height = p_dec->fmt_in.video.i_height;
    p_dec->fmt_out.video.i_sar_num  = p_dec->fmt_in.video.i_sar_num;
    p_dec->fmt_out.video.i_sar_den  = p_dec->fmt_in.video.i_sar_den;
    p_dec->b_need_packetized      = false;//true;

    /* Set callbacks */
    p_dec->pf_decode_video = DecodeBlock;
    msg_Dbg(p_dec, "Opened Cedar device with success");

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    msg_Dbg(p_dec, "done cleaning up Cedar");
    if (p_sys) {
#ifdef DECODE_USE_ASYNC_THREAD
        vlc_cancel(p_sys->thread);
        vlc_join(p_sys->thread, NULL);
#endif
        libcedarx_decoder_close();
        free(p_sys);
        p_dec->p_sys = NULL;
    }
}

static void Release(picture_t *picture)
{
    int id;
	void (*pf_release)(picture_t *);
    if (picture) {
		id = (int)picture->p[0].p_pixels;
		pf_release = (void *)picture->p[1].p_pixels;
        if (pf_release)
            pf_release(picture);
        
        if (!picture->i_refcount) {
            libcedarx_display_return_frame(id);
            picture->pf_release = pf_release;
        }
    }
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************/
static picture_t *DecodeBlock(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    picture_t *p_pic;
    mtime_t i_pts;
    u64 pts;
    u32 id, frame_rate, width, height;

    if(!p_sys || !pp_block)
        return NULL;
        
    p_block = *pp_block;
    if(p_block && !(p_block->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED)))
    {
        if (p_block->i_pts > VLC_TS_INVALID) {
            i_pts = p_block->i_pts;
        } else {
            i_pts = -1;
        }
    
        if (libcedarx_decoder_add_stream(p_block->p_buffer, p_block->i_buffer, i_pts, 0) < 0)
            msg_Warn(p_dec, "Failed to add stream!");
            
        /* Make sure we don't reuse the same timestamps twice */
        p_block->i_pts = p_block->i_dts = VLC_TS_INVALID;
        block_Release(p_block);
        *pp_block = NULL;

#ifndef DECODE_USE_ASYNC_THREAD
        if (libcedarx_decoder_decode_stream() < 0)
            msg_Warn(p_dec, "Failed to decode stream!");
#endif
    }

    if (!libcedarx_display_request_frame(&id, &pts, &frame_rate, &width, &height)) {
        if (!p_dec->fmt_out.video.i_width || !p_dec->fmt_out.video.i_height) {
            p_dec->fmt_out.video.i_width = width;
            p_dec->fmt_out.video.i_height = height;
        }
        
        p_pic = decoder_NewPicture(p_dec);
        if (p_pic) {
            p_pic->p[0].p_pixels     = id;
            p_pic->p[1].p_pixels     = p_pic->pf_release;
            p_pic->pf_release        = Release;
            p_pic->date              = pts;
            return p_pic;
        }
    }

    return NULL;
}
