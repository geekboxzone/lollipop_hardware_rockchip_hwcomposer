/****************************************************************************
*
*    Copyright (c) 2005 - 2011 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************
*
*    Auto-generated file on 12/13/2011. Do not edit!!!
*
*****************************************************************************/




#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "rk_hwcomposer.h"

#include <hardware/hardware.h>


#include <stdlib.h>
#include <errno.h>
#include <cutils/properties.h>
#include <fcntl.h>
#include <sync/sync.h>
#include "../libgralloc_ump/gralloc_priv.h"
#include <time.h>
#include <poll.h>
#include "rk_hwcomposer_hdmi.h"
#include <ui/PixelFormat.h>
#include <sys/stat.h>
//#include "hwc_ipp.h" 
#include "hwc_rga.h"
#include <hardware/rk_fh.h>
#include <linux/ion.h>
#include <ion/ion.h>

#define MAX_DO_SPECIAL_COUNT        8
#define RK_FBIOSET_ROTATE            0x5003 
#define FPS_NAME                    "com.aatt.fpsm"
#define BOTTOM_LAYER_NAME           "NavigationBar"
#define TOP_LAYER_NAME              "StatusBar"
#define WALLPAPER                   "ImageWallpaper"

//#define ENABLE_HDMI_APP_LANDSCAP_TO_PORTRAIT
static int SkipFrameCount = 0;

static hwcContext * _contextAnchor = NULL;
#ifdef ENABLE_HDMI_APP_LANDSCAP_TO_PORTRAIT            
static int bootanimFinish = 0;
#endif
static hwbkupmanage bkupmanage;

int gwin_tab[MaxZones] = {win0,win1,win2_0,win2_1,win2_2,win2_3,win3_0,win3_1,win3_2,win3_3};
#undef LOGV
#define LOGV(...)

static int
hwc_blank(
            struct hwc_composer_device_1 *dev,
            int dpy,
            int blank);
static int
hwc_query(
        struct hwc_composer_device_1* dev,
        int what,
        int* value);

static int hwc_event_control(
                struct hwc_composer_device_1* dev,
                int dpy,
                int event,
                int enabled);

static int
hwc_prepare(
    hwc_composer_device_1_t * dev,
    size_t numDisplays,
    hwc_display_contents_1_t** displays
    );


static int
hwc_set(
    hwc_composer_device_1_t * dev,
    size_t numDisplays,
    hwc_display_contents_1_t  ** displays
    );

static int
hwc_device_close(
    struct hw_device_t * dev
    );

static int
hwc_device_open(
    const struct hw_module_t * module,
    const char * name,
    struct hw_device_t ** device
    );

static struct hw_module_methods_t hwc_module_methods =
{
    open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM =
{
    common:
    {
        tag:           HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 2,
        id:            HWC_HARDWARE_MODULE_ID,
        name:          "Hardware Composer Module",
        author:        "Vivante Corporation",
        methods:       &hwc_module_methods,
        dso:           NULL,
        reserved:      {0, }
    }
};

static int LayerZoneCheck( hwc_layer_1_t * Layer)
{
    hwc_region_t * Region = &(Layer->visibleRegionScreen);
    hwc_rect_t const * rects = Region->rects;
    int i;
    for (i = 0; i < (int) Region->numRects ;i++)
    {
        LOGV("checkzone=%s,[%d,%d,%d,%d]", \
            Layer->LayerName,rects[i].left,rects[i].top,rects[i].right,rects[i].bottom );
        if(rects[i].left < 0 || rects[i].top < 0
           || rects[i].right > _contextAnchor->fbhandle.width  
           || rects[i].bottom > _contextAnchor->fbhandle.height)
        {
            return -1;
        }
    }

    return 0;
}

void hwc_sync(hwc_display_contents_1_t  *list)
{
  if (list == NULL)
  {
    return ;
  }
  
  for (int i=0; i<(int)list->numHwLayers; i++)
  {
     if (list->hwLayers[i].acquireFenceFd>0)
     {
       sync_wait(list->hwLayers[i].acquireFenceFd,-1);
       ALOGV("fenceFd=%d,name=%s",list->hwLayers[i].acquireFenceFd,list->hwLayers[i].LayerName);
     }

  }
}

void hwc_sync_release(hwc_display_contents_1_t  *list)
{
  for (int i=0; i<(int)list->numHwLayers; i++)
  {
    hwc_layer_1_t* layer = &list->hwLayers[i];
    if (layer == NULL)
    {
      return ;
    }
    if (layer->acquireFenceFd>0)
    {
      ALOGV(">>>close acquireFenceFd:%d,layername=%s",layer->acquireFenceFd,layer->LayerName);
      close(layer->acquireFenceFd);
      list->hwLayers[i].acquireFenceFd = -1;
    }
  }

  if (list->outbufAcquireFenceFd>0)
  {
    ALOGV(">>>close outbufAcquireFenceFd:%d",list->outbufAcquireFenceFd);
    close(list->outbufAcquireFenceFd);
    list->outbufAcquireFenceFd = -1;
  }
   
}
int rga_video_copybit(struct private_handle_t *handle,int tranform,int w_valid,int h_valid)
{
    struct rga_req  Rga_Request;
    RECT clip;
    unsigned char RotateMode = 0;
    int Rotation = 0;
    int SrcVirW,SrcVirH,SrcActW,SrcActH;
    int DstVirW,DstVirH,DstActW,DstActH;
    int xoffset = 0;
    int yoffset = 0;
    int Dstfmt = RK_FORMAT_RGB_565;
    int   rga_fd = _contextAnchor->engine_fd;
    if (!rga_fd)
    {
       return -1; 
    }
    if (!handle )
    {
      return -1;
    }
    memset(&Rga_Request, 0x0, sizeof(Rga_Request));
    clip.xmin = 0;
    clip.xmax = handle->height - 1;
    clip.ymin = 0;
    clip.ymax = handle->width - 1;
    switch (tranform)
    {

        case HWC_TRANSFORM_ROT_90:
            RotateMode      = 1;
            Rotation    = 90;          
            SrcVirW = handle->video_width;
            SrcVirH = handle->video_height;
            SrcActW = w_valid;
            SrcActH = h_valid;
            DstVirW = h_valid;
            DstVirH = w_valid;
            DstActW = w_valid;
            DstActH = h_valid;
            xoffset = h_valid -1;
            yoffset = 0;
            
            break;

        case HWC_TRANSFORM_ROT_180:
            RotateMode      = 1;
            Rotation    = 180;    
            SrcVirW = handle->video_width;
            SrcVirH = handle->video_height;
            SrcActW = w_valid;
            SrcActH = h_valid;
            DstVirW = w_valid;
            DstVirH = h_valid;
            DstActW = w_valid;
            DstActH = h_valid;
            clip.xmin = 0;
            clip.xmax =  handle->width - 1;
            clip.ymin = 0;
            clip.ymax = handle->height - 1;
            xoffset = w_valid -1;
            yoffset = h_valid -1;
            
            break;

        case HWC_TRANSFORM_ROT_270:
            RotateMode      = 1;
            Rotation        = 270;   
            SrcVirW = handle->video_width;
            SrcVirH = handle->video_height;
            SrcActW = w_valid;
            SrcActH = h_valid;
            DstVirW = h_valid;
            DstVirH = w_valid;
            DstActW = w_valid;
            DstActH = h_valid;
            xoffset = 0;
            yoffset = w_valid -1;
            break;
        case 0:                      
        default:
        {
            char property[PROPERTY_VALUE_MAX];
            int fmtflag = 0;
			if (property_get("sys.yuv.rgb.format", property, NULL) > 0) {
				fmtflag = atoi(property);
			}
			if(fmtflag == 1)
                Dstfmt = RK_FORMAT_RGB_565;
			else 
                Dstfmt = RK_FORMAT_RGBA_8888;        
            SrcVirW = handle->video_width;
            SrcVirH = handle->video_height;
            SrcActW = handle->video_width;
            SrcActH = handle->video_height;
            DstVirW = handle->width;
            DstVirH = handle->height;
            DstActW = handle->width;
            DstActH = handle->height;
            clip.xmin = 0;
            clip.xmax =  handle->width - 1;
            clip.ymin = 0;
            clip.ymax = handle->height - 1;                
            break;
    }        
    }       
    ALOGV("src addr=[%x],w-h[%d,%d],act[%d,%d][f=%d]",
        handle->video_addr, SrcVirW, SrcVirH,SrcActW,SrcActH,RK_FORMAT_YCbCr_420_SP);
    ALOGV("dst fd=[%x],w-h[%d,%d],act[%d,%d][f=%d],rot=%d,rot_mod=%d",
        handle->share_fd, DstVirW, DstVirH,DstActW,DstActH,Dstfmt,Rotation,RotateMode);
    RGA_set_src_vir_info(&Rga_Request, 0, handle->video_addr, 0,SrcVirW, SrcVirH, RK_FORMAT_YCbCr_420_SP, 0);
    RGA_set_dst_vir_info(&Rga_Request, handle->share_fd, 0, 0,DstVirW,DstVirH,&clip, Dstfmt, 0);    
    RGA_set_bitblt_mode(&Rga_Request, 0, RotateMode,Rotation,0,0,0);    
    RGA_set_src_act_info(&Rga_Request,SrcActW,SrcActH, 0,0);
    RGA_set_dst_act_info(&Rga_Request,DstActW,DstActH, xoffset,yoffset);

    if(ioctl(rga_fd, RGA_BLIT_SYNC, &Rga_Request) != 0) {
        LOGE(" %s(%d) RGA_BLIT fail",__FUNCTION__, __LINE__);
        ALOGE("err src addr=[%x],w-h[%d,%d],act[%d,%d][f=%d],x_y_offset[%d,%d]",
            handle->video_addr, SrcVirW, SrcVirH,SrcActW,SrcActH,RK_FORMAT_YCbCr_420_SP,xoffset,yoffset);
        ALOGE("err dst fd=[%x],w-h[%d,%d],act[%d,%d][f=%d],rot=%d,rot_mod=%d",
            handle->share_fd, DstVirW, DstVirH,DstActW,DstActH,RK_FORMAT_RGB_565,Rotation,RotateMode);
    }
        
#if 0
    FILE * pfile = NULL;
    int srcStride = android::bytesPerPixel(src_handle->format);
    char layername[100];
    memset(layername,0,sizeof(layername));
    system("mkdir /data/dumplayer/ && chmod /data/dumplayer/ 777 ");
    sprintf(layername,"/data/dumplayer/dmlayer%d_%d_%d_%d.bin",\
           layer_seq,src_handle->stride,src_handle->height,srcStride);

    pfile = fopen(layername,"wb");
    if(pfile)
    {
    fwrite((const void *)src_handle->base,(size_t)(2 * src_handle->stride*src_handle->height),1,pfile);
    fclose(pfile);
    }
    layer_seq++;
#endif
   
    return 0;
}


int is_x_intersect(hwc_rect_t * rec,hwc_rect_t * rec2)
{
    if(rec2->top == rec->top)
        return 1;
    else if(rec2->top < rec->top)
    {
        if(rec2->bottom > rec->top)
            return 1;
        else
            return 0;
    }
    else
    {
        if(rec->bottom > rec2->top  )
            return 1;
        else
            return 0;        
    }
    return 0;
}
int is_zone_combine(ZoneInfo * zf,ZoneInfo * zf2)
{
    if(zf->format != zf2->format)
    {
        ALOGV("line=%d",__LINE__);
        return 0;
    }    
    if(zf->is_stretch || zf2->is_stretch )    
    {
        ALOGV("line=%d",__LINE__);    
        return 0;
    }    
    if(is_x_intersect(&(zf->disp_rect),&(zf2->disp_rect)))  
    {
        ALOGV("line=%d",__LINE__);
        return 0;
    }    
    else
        return 1;
}

int collect_all_zones( hwcContext * Context,hwc_display_contents_1_t * list)
{
    size_t i,j;
    for (i = 0,j=0; i < (list->numHwLayers - 1) ; i++)
    {
        hwc_layer_1_t * layer = &list->hwLayers[i];
        hwc_region_t * Region = &layer->visibleRegionScreen;
        hwc_rect_t * SrcRect = &layer->sourceCrop;
        hwc_rect_t * DstRect = &layer->displayFrame;
        struct private_handle_t* SrcHnd = (struct private_handle_t *) layer->handle;
        float hfactor;
        float vfactor;
        hwcRECT dstRects[16];
        unsigned int m;
        bool is_stretch = 0;
        hwc_rect_t const * rects = Region->rects;

        if((layer->transform == HWC_TRANSFORM_ROT_90)||(layer->transform == HWC_TRANSFORM_ROT_270))
        {
            hfactor = (float) (SrcRect->bottom - SrcRect->top)
                    / (DstRect->right - DstRect->left);
            vfactor = (float) (SrcRect->right - SrcRect->left)
                    / (DstRect->bottom - DstRect->top);
        }
        else
        {
            hfactor = (float) (SrcRect->right - SrcRect->left)
                    / (DstRect->right - DstRect->left);

            vfactor = (float) (SrcRect->bottom - SrcRect->top)
                    / (DstRect->bottom - DstRect->top);
        }

        is_stretch = (hfactor != 1.0) || (vfactor != 1.0);
        for (m = 0; m < (unsigned int) Region->numRects && m < 16 ; m++)
        {
           
            dstRects[m].left   = hwcMAX(DstRect->left,   rects[m].left);
            dstRects[m].top    = hwcMAX(DstRect->top,    rects[m].top);
            dstRects[m].right  = hwcMIN(DstRect->right,  rects[m].right);
            dstRects[m].bottom = hwcMIN(DstRect->bottom, rects[m].bottom);


            /* Check dest area. */
            if ((dstRects[m].right <= dstRects[m].left)
            ||  (dstRects[m].bottom <= dstRects[m].top)
            )
            {
                /* Skip this empty rectangle. */
                LOGI("%s(%d):  skip empty rectangle [%d,%d,%d,%d]",
                     __FUNCTION__,
                     __LINE__,
                     dstRects[m].left,
                     dstRects[m].top,
                     dstRects[m].right,
                     dstRects[m].bottom);
                continue;
            }
            if((dstRects[m].right - dstRects[m].left) < 16
                || (dstRects[m].bottom - dstRects[m].top) < 16)
            {
                ALOGD("lcdc dont support too small area");
                return -1;
            }
            LOGV("%s(%d): Region rect[%d]:  [%d,%d,%d,%d]",
                 __FUNCTION__,
                 __LINE__,
                 m,
                 rects[m].left,
                 rects[m].top,
                 rects[m].right,
                 rects[m].bottom);
        }
        if((m+j) > MaxZones)
        {
            return -1;
        }
        for (unsigned int k = 0; k < m; k++)
        {

            Context->zone_manager.zone_info[j].zone_alpha = (layer->blending) >> 16;
            Context->zone_manager.zone_info[j].is_stretch = is_stretch;
            Context->zone_manager.zone_info[j].zone_index = j;
            Context->zone_manager.zone_info[j].layer_index = i;
            Context->zone_manager.zone_info[j].dispatched = 0;
            Context->zone_manager.zone_info[j].direct_fd = 0;
            Context->zone_manager.zone_info[j].sort = 0;
            Context->zone_manager.zone_info[j].addr = 0;
            Context->zone_manager.zone_info[j].handle = (struct private_handle_t *)layer->handle;
            Context->zone_manager.zone_info[j].transform = layer->transform;
            Context->zone_manager.zone_info[j].realtransform = layer->realtransform;
            strcpy(Context->zone_manager.zone_info[j].LayerName,layer->LayerName);
            Context->zone_manager.zone_info[j].disp_rect.left = dstRects[k].left;
            Context->zone_manager.zone_info[j].disp_rect.top = dstRects[k].top;
            Context->zone_manager.zone_info[j].disp_rect.right = dstRects[k].right;
            Context->zone_manager.zone_info[j].disp_rect.bottom = dstRects[k].bottom;  
#ifdef USE_HWC_FENCE
	    Context->zone_manager.zone_info[j].acq_fence_fd =layer->acquireFenceFd;
 #endif
            if(SrcHnd->format == HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO)
            {
                int w_valid = 0 ,h_valid = 0;
    		    hwc_rect_t * psrc_rect = &(Context->zone_manager.zone_info[j].src_rect);            
                Context->zone_manager.zone_info[j].format = HAL_PIXEL_FORMAT_YCrCb_NV12;
                ALOGV("HAL_PIXEL_FORMAT_YCrCb_NV12 transform=%d, addr[%x][%dx%d],ori_fd[%d][%dx%d]",
                        layer->transform,SrcHnd->video_addr,SrcHnd->video_width,SrcHnd->video_height,
                        SrcHnd->share_fd,SrcHnd->width,SrcHnd->height);
                switch (layer->transform)                
                {
                    case 0:
                    
                        psrc_rect->left   = SrcRect->left
                            - (int) ((DstRect->left   - dstRects[k].left)   * hfactor);
                        psrc_rect->top    = SrcRect->top
                            - (int) ((DstRect->top    - dstRects[k].top)    * vfactor);
                        psrc_rect->right  = SrcRect->right
                            - (int) ((DstRect->right  - dstRects[i].right)  * hfactor);
                        psrc_rect->bottom = SrcRect->bottom
                            - (int) ((DstRect->bottom - dstRects[k].bottom) * vfactor);
                        Context->zone_manager.zone_info[j].layer_fd = 0;    
                        Context->zone_manager.zone_info[j].addr = SrcHnd->video_addr; 
                        Context->zone_manager.zone_info[j].width = SrcHnd->video_width;
                        Context->zone_manager.zone_info[j].height = SrcHnd->video_height;
                        Context->zone_manager.zone_info[j].stride = SrcHnd->video_width; 
                        //Context->zone_manager.zone_info[j].format = SrcHnd->format;                        
                        break;
            		 case HWC_TRANSFORM_ROT_270:
                        psrc_rect->left   = SrcRect->top +  (SrcRect->right - SrcRect->left)
                            - ((dstRects[k].bottom - DstRect->top)    * vfactor);

                        psrc_rect->top    =  SrcRect->left
                            - (int) ((DstRect->left   - dstRects[k].left)   * hfactor);

                        psrc_rect->right  = psrc_rect->left
                            + (int) ((dstRects[k].bottom - dstRects[k].top) * vfactor);

                        psrc_rect->bottom = psrc_rect->top
                            + (int) ((dstRects[k].right  - dstRects[k].left) * hfactor);
                        h_valid = psrc_rect->bottom - psrc_rect->top;
                        w_valid = psrc_rect->right - psrc_rect->left;
                        Context->zone_manager.zone_info[j].layer_fd = SrcHnd->share_fd;  
                        Context->zone_manager.zone_info[j].width = w_valid;
                        Context->zone_manager.zone_info[j].height = h_valid;
                        Context->zone_manager.zone_info[j].stride = w_valid;                                                    
                        Context->zone_manager.zone_info[j].format = HAL_PIXEL_FORMAT_RGB_565;
                        
                        break;

                    case HWC_TRANSFORM_ROT_90:
                        psrc_rect->left   = SrcRect->top
                            - (int) ((DstRect->top    - dstRects[k].top)    * vfactor);

                        psrc_rect->top    =  SrcRect->left
                            - (int) ((DstRect->left   - dstRects[k].left)   * hfactor);

                        psrc_rect->right  = psrc_rect->left
                            + (int) ((dstRects[k].bottom - dstRects[k].top) * vfactor);

                        psrc_rect->bottom = psrc_rect->top
                            + (int) ((dstRects[k].right  - dstRects[k].left) * hfactor);
                        h_valid = psrc_rect->bottom - psrc_rect->top;
                        w_valid = psrc_rect->right - psrc_rect->left;
                        Context->zone_manager.zone_info[j].layer_fd = SrcHnd->share_fd;  
                        Context->zone_manager.zone_info[j].width = w_valid;
                        Context->zone_manager.zone_info[j].height = h_valid ;
                        Context->zone_manager.zone_info[j].stride = w_valid;   
                        Context->zone_manager.zone_info[j].format = HAL_PIXEL_FORMAT_RGB_565;
                        break;

            		case HWC_TRANSFORM_ROT_180:
                        psrc_rect->left   = SrcRect->left +  (SrcRect->right - SrcRect->left)
                            - ((dstRects[k].right - DstRect->left)   * hfactor);

                        psrc_rect->top    = SrcRect->top
                            - (int) ((DstRect->top    - dstRects[k].top)    * vfactor);

                        psrc_rect->right  = psrc_rect->left
                            + (int) ((dstRects[k].right  - dstRects[k].left) * hfactor);

                        psrc_rect->bottom = psrc_rect->top
                            + (int) ((dstRects[k].bottom - dstRects[k].top) * vfactor);
                        w_valid = psrc_rect->right - psrc_rect->left;
                        h_valid = psrc_rect->bottom - psrc_rect->top;   
                        Context->zone_manager.zone_info[j].layer_fd = SrcHnd->share_fd; 
                        Context->zone_manager.zone_info[j].width = w_valid;
                        Context->zone_manager.zone_info[j].height = h_valid;
                        Context->zone_manager.zone_info[j].stride = w_valid;                                                    
                        Context->zone_manager.zone_info[j].format = HAL_PIXEL_FORMAT_RGB_565;                          
                        break;
                    default:
                        break;
                }   
                //ALOGD("layer->transform=%d",layer->transform);
                if(layer->transform)
                    rga_video_copybit(SrcHnd,layer->transform,w_valid,h_valid);
    			psrc_rect->left = psrc_rect->left - psrc_rect->left%2;
    			psrc_rect->top = psrc_rect->top - psrc_rect->top%2;
    			psrc_rect->right = psrc_rect->right - psrc_rect->right%2;
    			psrc_rect->bottom = psrc_rect->bottom - psrc_rect->bottom%2;
                
            }
            else
            {
                Context->zone_manager.zone_info[j].src_rect.left   = hwcMAX ((SrcRect->left \
                    - (int) ((DstRect->left   - dstRects[k].left)   * hfactor)),0);
                Context->zone_manager.zone_info[j].src_rect.top    = hwcMAX ((SrcRect->top \
                    - (int) ((DstRect->top    - dstRects[k].top)    * vfactor)),0);
                Context->zone_manager.zone_info[j].src_rect.right  = SrcRect->right \
                    - (int) ((DstRect->right  - dstRects[k].right)  * hfactor);
                Context->zone_manager.zone_info[j].src_rect.bottom = SrcRect->bottom \
                    - (int) ((DstRect->bottom - dstRects[k].bottom) * vfactor);            
                Context->zone_manager.zone_info[j].format = SrcHnd->format;
                Context->zone_manager.zone_info[j].width = SrcHnd->width;
                Context->zone_manager.zone_info[j].height = SrcHnd->height;
                Context->zone_manager.zone_info[j].stride = SrcHnd->stride;
                Context->zone_manager.zone_info[j].layer_fd = SrcHnd->share_fd;                
            }    
            
    		j++;
            
        }        

    }
    Context->zone_manager.zone_cnt = j;
    for(i=0;i<j;i++)
    {
           
        ALOGV("Zone[%d]->layer[%d],"
            "[%d,%d,%d,%d] =>[%d,%d,%d,%d],"
            "w_h_s_f[%d,%d,%d,%d],tr_rtr_bled[%d,%d,%d],acq_fence_fd=%d,"
            "layname=%s",                        
            Context->zone_manager.zone_info[i].zone_index,
            Context->zone_manager.zone_info[i].layer_index,
            Context->zone_manager.zone_info[i].src_rect.left,
            Context->zone_manager.zone_info[i].src_rect.top,
            Context->zone_manager.zone_info[i].src_rect.right,
            Context->zone_manager.zone_info[i].src_rect.bottom,
            Context->zone_manager.zone_info[i].disp_rect.left,
            Context->zone_manager.zone_info[i].disp_rect.top,
            Context->zone_manager.zone_info[i].disp_rect.right,
            Context->zone_manager.zone_info[i].disp_rect.bottom,
            Context->zone_manager.zone_info[i].width,
            Context->zone_manager.zone_info[i].height,
            Context->zone_manager.zone_info[i].stride,
            Context->zone_manager.zone_info[i].format,
            Context->zone_manager.zone_info[i].transform,
            Context->zone_manager.zone_info[i].realtransform,
            Context->zone_manager.zone_info[i].blend,
            Context->zone_manager.zone_info[i].acq_fence_fd,
            Context->zone_manager.zone_info[i].LayerName);
    }
    return 0;
}

// return 0: suess
// return -1: fail
int try_wins_dispatch_hor(hwcContext * Context)
{
    int win_disphed_flag[4] = {0,}; // win0, win1, win2, win3 flag which is dispatched
    int win_disphed[4] = {win0,win1,win2_0,win3_0};
    int i,j;
    int sort = 1;
    int cnt = 0;
    int srot_tal[4][2] = {0,};
    int sort_stretch[4] = {0}; 
    
    ZoneManager* pzone_mag = &(Context->zone_manager);
    // try dispatch stretch wins
    char const* compositionTypeName[] = {
            "win0",
            "win1",
            "win2_0",
            "win2_1",
            "win2_2",
            "win2_3",
            "win3_0",
            "win3_1",
            "win3_2",
            "win3_3",
            };
    pzone_mag->zone_info[0].sort = sort;
    for(i=0;i<(pzone_mag->zone_cnt-1);)
    {
        pzone_mag->zone_info[i].sort = sort;
        cnt = 0;

        //means 4: win2 or win3 most has 4 zones 
        for(j=1;j<4 && (i+j) < pzone_mag->zone_cnt;j++)
        {
            ZoneInfo * next_zf = &(pzone_mag->zone_info[i+j]);
            bool is_combine = false;
            int k;
            for(k=0;k<=cnt;k++)  // compare all sorted_zone info
            {
                ZoneInfo * sorted_zf = &(pzone_mag->zone_info[i+j-1-k]);
                if(is_zone_combine(sorted_zf,next_zf))
                {
                    is_combine = true;
                }
                else
                {
                    is_combine = false;
                    break;
                }
            }
            if(is_combine)
            {
                pzone_mag->zone_info[i+j].sort = sort;
                cnt++;
            }
            else
            {
                sort++;
                pzone_mag->zone_info[i+j].sort = sort;
                cnt++;
                break;
            }
        }
        i += cnt;      
    }
    if(sort >4)  // lcdc dont support 5 wins
    {
        ALOGD("lcdc dont support 5 wins");
        return -1;
    }    

   //pzone_mag->zone_info[i].sort: win type
   // srot_tal[i][0] : tatal same wins
   // srot_tal[0][i] : dispatched lcdc win
    for(i=0;i<pzone_mag->zone_cnt;i++)
    {
        ALOGV("sort[%d].type=%d",i,pzone_mag->zone_info[i].sort);
        if( pzone_mag->zone_info[i].sort == 1){
            srot_tal[0][0]++;
            if(pzone_mag->zone_info[i].is_stretch)
                sort_stretch[0] = 1;
        }    
        else if(pzone_mag->zone_info[i].sort == 2){
            srot_tal[1][0]++;
            if(pzone_mag->zone_info[i].is_stretch)
                sort_stretch[1] = 1;
        }    
        else if(pzone_mag->zone_info[i].sort == 3){
            srot_tal[2][0]++;
            if(pzone_mag->zone_info[i].is_stretch)
                sort_stretch[2] = 1;
            
        }    
        else if(pzone_mag->zone_info[i].sort == 4){
            srot_tal[3][0]++;   
            if(pzone_mag->zone_info[i].is_stretch)
                sort_stretch[3] = 1;            
        }    
    }

    // first dispatch more zones win
    j = 0;
    for(i=0;i<4;i++)    
    {        
        if( srot_tal[i][0] >=2)  // > twice zones
        {
            srot_tal[i][1] = win_disphed[j+2]; 
            win_disphed_flag[j+2] = 1; // win2 ,win3 is dispatch flag
            ALOGV("more twice zones srot_tal[%d][1]=%d",i,srot_tal[i][1]);
            j++;
            if(j > 2)  // lcdc only has win2 and win3 supprot more zones
            {
                ALOGD("lcdc only has win2 and win3 supprot more zones");
                return -1;  
            }
        }
    }
    // second dispatch stretch win
    j = 0;
    for(i=0;i<4;i++)    
    {        
        if( sort_stretch[i] == 1)  // strech
        {
            srot_tal[i][1] = win_disphed[j];  // win 0 and win 1 suporot stretch
            win_disphed_flag[j] = 1; // win2 ,win3 is dispatch flag
            ALOGV("stretch zones srot_tal[%d][1]=%d",i,srot_tal[i][1]);
            j++;
            if(j > 2)  // lcdc only has win2 and win3 supprot more zones
            {
                ALOGD("lcdc only has win0 and win1 supprot stretch");
                return -1;  
            }
        }
    }
    
    // third dispatch common zones win
    for(i=0;i<4;i++)    
    {        
        if( srot_tal[i][1] == 0)  // had not dispatched
        {
            for(j=0;j<4;j++)
            {
                if(win_disphed_flag[j] == 0) // find the win had not dispatched
                    break;
            }  
            if(j>=4)
            {
                ALOGE("4 wins had beed dispatched ");
                return -1;
            }    
            srot_tal[i][1] = win_disphed[j];
            win_disphed_flag[j] = 1;
            ALOGV("srot_tal[%d][1].dispatched=%d",i,srot_tal[i][1]);
        }
    }

    for(i=0;i<pzone_mag->zone_cnt;i++)
    {        
         switch(pzone_mag->zone_info[i].sort) {
            case 1:
                pzone_mag->zone_info[i].dispatched = srot_tal[0][1]++;
                break;
            case 2:
                pzone_mag->zone_info[i].dispatched = srot_tal[1][1]++;            
                break;
            case 3:
                pzone_mag->zone_info[i].dispatched = srot_tal[2][1]++;            
                break;
            case 4:
                pzone_mag->zone_info[i].dispatched = srot_tal[3][1]++;            
                break;                
            default:
                ALOGE("try_wins_dispatch_hor sort err!");
                return -1;
        }
        ALOGV("zone[%d].dispatched[%d]=%s,sort=%d", \
        i,pzone_mag->zone_info[i].dispatched,
        compositionTypeName[pzone_mag->zone_info[i].dispatched -1],
        pzone_mag->zone_info[i].sort);

    }
    Context->zone_manager.composter_mode = HWC_LCDC;

    return 0;
}

// return 0: suess
// return -1: fail
int try_wins_dispatch_ver(hwcContext * Context)
{
    int win_disphed_flag[4] = {0,}; // win0, win1, win2, win3 flag which is dispatched
    int win_disphed[4] = {win0,win1,win2_0,win3_0};
    int i,j;
    ZoneManager* pzone_mag = &(Context->zone_manager);
    // try dispatch stretch wins
    char const* compositionTypeName[] = {
            "win0",
            "win1",
            "win2_0",
            "win2_1",
            "win2_2",
            "win2_3",
            "win3_0",
            "win3_1",
            "win3_2",
            "win3_3",
            "win_ext"
            };
            
    // first dispatch stretch win         
    if(pzone_mag->zone_cnt <=4)
    {
        for(i=0,j=0;i<pzone_mag->zone_cnt;i++)
        {
            if(pzone_mag->zone_info[i].is_stretch == true
                && pzone_mag->zone_info[i].dispatched == 0) 
            {
                pzone_mag->zone_info[i].dispatched = win_disphed[j];  // win 0 and win 1 suporot stretch
                win_disphed_flag[j] = 1; // win2 ,win3 is dispatch flag
                ALOGV("stretch zones [%d]=%d",i,pzone_mag->zone_info[i].dispatched);
                j++;
                if(j > 2)  // lcdc only has win2 and win3 supprot more zones
                {
                    ALOGD("lcdc only has win0 and win1 supprot stretch");
                    return -1;  
                }     
            }
        }
        // second dispatch common zones win  
        for(i=0,j=0;i<pzone_mag->zone_cnt;i++)    
        {        
            if( pzone_mag->zone_info[i].dispatched == 0)  // had not dispatched
            {
                for(j=0;j<4;j++)
                {
                    if(win_disphed_flag[j] == 0) // find the win had not dispatched
                        break;
                }  
                if(j>=4)
                {
                    ALOGE("4 wins had beed dispatched ");
                    return -1;
                }    
                pzone_mag->zone_info[i].dispatched  = win_disphed[j];
                win_disphed_flag[j] = 1;
                ALOGV("zone[%d][1].dispatched=%d",i,pzone_mag->zone_info[i].dispatched);
            }
        }
    
    }
    else
    {
        // first dispatch Bottom and  Top      

        for(i=0,j=3;i<pzone_mag->zone_cnt;i++)
        {
            bool IsBottom = !strcmp(BOTTOM_LAYER_NAME, pzone_mag->zone_info[i].LayerName);
            bool IsTop = !strcmp(TOP_LAYER_NAME,pzone_mag->zone_info[i].LayerName);
            if(IsTop || IsBottom)
            {
              pzone_mag->zone_info[i].dispatched  =  win_disphed[j];
              win_disphed_flag[j] = 1;
              j--;
            }  
        }
        for(i=0,j=0;i<pzone_mag->zone_cnt;i++)    
        {        
            if( pzone_mag->zone_info[i].dispatched == 0)  // had not dispatched
            {
                for(j=0;j<4;j++)
                {
                    if(win_disphed_flag[j] == 0) // find the win had not dispatched
                        break;
                }  
                if(j>=4)
                {
                    bool isLandScape = ( (0==pzone_mag->zone_info[i].realtransform) \
                                || (HWC_TRANSFORM_ROT_180==pzone_mag->zone_info[i].realtransform) );
                    bool isSmallRect = (isLandScape && (pzone_mag->zone_info[i].height< Context->fbhandle.height/4))  \
                                ||(!isLandScape && (pzone_mag->zone_info[i].width < Context->fbhandle.width/4)) ;
                    if(isSmallRect)
                        pzone_mag->zone_info[i].dispatched  = win_ext;
                    else
                        return -1;  // too large
                }   
                else
                {
                    pzone_mag->zone_info[i].dispatched  = win_disphed[j];
                    win_disphed_flag[j] = 1;
                    ALOGV("zone[%d].dispatched=%d",i,pzone_mag->zone_info[i].dispatched);
                }    
            }
        }
    }
       
    Context->zone_manager.composter_mode = HWC_LCDC;
    return 0;
}
static int skip_count = 0;
static uint32_t
check_layer(
    hwcContext * Context,
    uint32_t Count,
    uint32_t Index,
    hwc_layer_1_t * Layer
    )
{
    struct private_handle_t * handle =
        (struct private_handle_t *) Layer->handle;

  
    (void) Context;
    (void) Count;
    (void) Index;
#if 0    
    float hfactor = 1;
    float vfactor = 1;

    hfactor = (float) (Layer->sourceCrop.right - Layer->sourceCrop.left)
            / (Layer->displayFrame.right - Layer->displayFrame.left);

    vfactor = (float) (Layer->sourceCrop.bottom - Layer->sourceCrop.top)
            / (Layer->displayFrame.bottom - Layer->displayFrame.top);

    /* Check whether this layer is forced skipped. */

    if(hfactor<1.0 ||vfactor<1.0 )
    {
        ALOGE("[%d,%d,%d,%d],[%d,%d,%d,%d]",Layer->sourceCrop.right , Layer->sourceCrop.left,Layer->displayFrame.right , Layer->displayFrame.left,
            Layer->sourceCrop.bottom ,Layer->sourceCrop.top,Layer->displayFrame.bottom ,Layer->displayFrame.top);
    }
    ALOGD("[%f,%f],name[%d]=%s",hfactor,vfactor,Index,Layer->LayerName);
#endif    
    if ((Layer->flags & HWC_SKIP_LAYER)
       // ||(hfactor != 1.0f)  // because rga scale down too slowly,so return to opengl  ,huangds modify
       // ||(vfactor != 1.0f)  // because rga scale down too slowly,so return to opengl ,huangds modify
        || handle == NULL
        ||((Layer->transform != 0)&&(handle->format != HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO))
        || skip_count<5
        )
    {
        /* We are forbidden to handle this layer. */
        LOGV("%s(%d):Will not handle layer %s: SKIP_LAYER,Layer->transform=%d,Layer->flags=%d",
             __FUNCTION__, __LINE__, Layer->LayerName,Layer->transform,Layer->flags);
        Layer->compositionType = HWC_FRAMEBUFFER;
        if (skip_count<5)
        {
         skip_count++;
        }
        return HWC_FRAMEBUFFER;
    }

    /* Check whether this layer can be handled by Vivante 2D. */
    do
    {
        RgaSURF_FORMAT format = RK_FORMAT_UNKNOWN;
        /* Check for dim layer. */
       

            /* TODO: I BELIEVE YOU CAN HANDLE SUCH LAYER!. */


        /* At least surfaceflinger can handle this layer. */
        Layer->compositionType = HWC_FRAMEBUFFER;

        /* Get format. */
        if( hwcGetFormat(handle, &format) != hwcSTATUS_OK
            || (LayerZoneCheck(Layer) != 0))
        {
             return HWC_FRAMEBUFFER;
        }

        LOGV("name[%d]=%s",Index,Layer->LayerName);

        if(            
              (getHdmiMode()==0)            
            )  // layer <=3,do special processing

        {           
            Layer->compositionType = HWC_LCDC;
            Layer->flags           = 0;
            //ALOGD("win 0");
            break;                 
        }
        else
        {
           
            Layer->compositionType = HWC_FRAMEBUFFER;
            return HWC_FRAMEBUFFER;
            /*    ----end  ----*/
        }           

    }
    while (0);

    /* Return last composition type. */
    return Layer->compositionType;
}

/*****************************************************************************/

#if hwcDEBUG
static void
_Dump(
    hwc_display_contents_1_t* list
    )
{
    size_t i, j;

    for (i = 0; list && (i < list->(numHwLayers - 1)); i++)
    {
        hwc_layer_1_t const * l = &list->hwLayers[i];

        if(l->flags & HWC_SKIP_LAYER)
        {
            LOGD("layer %p skipped", l);
        }
        else
        {
            LOGD("layer=%p, "
                 "type=%d, "
                 "hints=%08x, "
                 "flags=%08x, "
                 "handle=%p, "
                 "tr=%02x, "
                 "blend=%04x, "
                 "{%d,%d,%d,%d}, "
                 "{%d,%d,%d,%d}",
                 l,
                 l->compositionType,
                 l->hints,
                 l->flags,
                 l->handle,
                 l->transform,
                 l->blending,
                 l->sourceCrop.left,
                 l->sourceCrop.top,
                 l->sourceCrop.right,
                 l->sourceCrop.bottom,
                 l->displayFrame.left,
                 l->displayFrame.top,
                 l->displayFrame.right,
                 l->displayFrame.bottom);

            for (j = 0; j < l->visibleRegionScreen.numRects; j++)
            {
                LOGD("\trect%d: {%d,%d,%d,%d}", j,
                     l->visibleRegionScreen.rects[j].left,
                     l->visibleRegionScreen.rects[j].top,
                     l->visibleRegionScreen.rects[j].right,
                     l->visibleRegionScreen.rects[j].bottom);
            }
        }
    }
}
#endif

#if hwcDumpSurface
static void
_DumpSurface(
    hwc_display_contents_1_t* list
    )
{
    size_t i;
    static int DumpSurfaceCount = 0;

    char pro_value[PROPERTY_VALUE_MAX];
    property_get("sys.dump",pro_value,0);
    //LOGI(" sys.dump value :%s",pro_value);
    if(!strcmp(pro_value,"true"))
    {
        for (i = 0; list && (i < (list->numHwLayers - 1)); i++)
        {
            hwc_layer_1_t const * l = &list->hwLayers[i];

            if(l->flags & HWC_SKIP_LAYER)
            {
                LOGI("layer %p skipped", l);
            }
            else
            {
                struct private_handle_t * handle_pre = (struct private_handle_t *) l->handle;
                int32_t SrcStride ;
                FILE * pfile = NULL;
                char layername[100] ;


                if( handle_pre == NULL)
                    continue;

                SrcStride = android::bytesPerPixel(handle_pre->format);
                memset(layername,0,sizeof(layername));
                system("mkdir /data/dump/ && chmod /data/dump/ 777 ");
                //mkdir( "/data/dump/",777);
                sprintf(layername,"/data/dump/dmlayer%d_%d_%d_%d.bin",DumpSurfaceCount,handle_pre->stride,handle_pre->height,SrcStride);
                DumpSurfaceCount ++;
                pfile = fopen(layername,"wb");
                if(pfile)
                {
                    fwrite((const void *)handle_pre->base,(size_t)(SrcStride * handle_pre->stride*handle_pre->height),1,pfile);
                    fclose(pfile);
                    LOGI(" dump surface layername %s,w:%d,h:%d,formatsize :%d",layername,handle_pre->width,handle_pre->height,SrcStride);
                }
            }
        }

    }
    property_set("sys.dump","false");


}
#endif

void
hwcDumpArea(
    IN hwcArea * Area
    )
{
    hwcArea * area = Area;

    while (area != NULL)
    {
        char buf[128];
        char digit[8];
        bool first = true;

        sprintf(buf,
                "Area[%d,%d,%d,%d] owners=%08x:",
                area->rect.left,
                area->rect.top,
                area->rect.right,
                area->rect.bottom,
                area->owners);

        for (int i = 0; i < 32; i++)
        {
            /* Build decimal layer indices. */
            if (area->owners & (1U << i))
            {
                if (first)
                {
                    sprintf(digit, " %d", i);
                    strcat(buf, digit);
                    first = false;
                }
                else
                {
                    sprintf(digit, ",%d", i);
                    strcat(buf, digit);
                }
            }

            if (area->owners < (1U << i))
            {
                break;
            }
        }

        LOGD("%s", buf);

        /* Advance to next area. */
        area = area->next;
    }
}
    

static int backupbuffer(hwbkupinfo *pbkupinfo)
{
    struct rga_req  Rga_Request;
    RECT clip;
    int bpp;

    ALOGV("backupbuffer addr=[%x,%x],bkmem=[%x,%x],w-h[%d,%d][%d,%d,%d,%d][f=%d]",
        pbkupinfo->buf_fd, pbkupinfo->buf_addr_log, pbkupinfo->membk_fd, pbkupinfo->pmem_bk_log,pbkupinfo->w_vir,
        pbkupinfo->h_vir,pbkupinfo->xoffset,pbkupinfo->yoffset,pbkupinfo->w_act,pbkupinfo->h_act,pbkupinfo->format);

    bpp = pbkupinfo->format == RK_FORMAT_RGB_565 ? 2:4;

    clip.xmin = 0;
    clip.xmax = pbkupinfo->w_act - 1;
    clip.ymin = 0;
    clip.ymax = pbkupinfo->h_act - 1;

    memset(&Rga_Request, 0x0, sizeof(Rga_Request));

    
    RGA_set_src_vir_info(&Rga_Request, pbkupinfo->buf_fd, 0, 0,pbkupinfo->w_vir, pbkupinfo->h_vir, pbkupinfo->format, 0);
    RGA_set_dst_vir_info(&Rga_Request, pbkupinfo->membk_fd, 0, 0,pbkupinfo->w_act, pbkupinfo->h_act, &clip, pbkupinfo->format, 0);
    //RGA_set_src_vir_info(&Rga_Request, (int)pbkupinfo->buf_addr_log, 0, 0,pbkupinfo->w_vir, pbkupinfo->h_vir, pbkupinfo->format, 0);
    //RGA_set_dst_vir_info(&Rga_Request, (int)pbkupinfo->pmem_bk_log, 0, 0,pbkupinfo->w_act, pbkupinfo->h_act, &clip, pbkupinfo->format, 0);
    //RGA_set_mmu_info(&Rga_Request, 1, 0, 0, 0, 0, 2);

    RGA_set_bitblt_mode(&Rga_Request, 0, 0,0,0,0,0);
    RGA_set_src_act_info(&Rga_Request,pbkupinfo->w_act,  pbkupinfo->h_act,  pbkupinfo->xoffset, pbkupinfo->yoffset);
    RGA_set_dst_act_info(&Rga_Request, pbkupinfo->w_act,  pbkupinfo->h_act, 0, 0);

   // uint32_t RgaFlag = (i==(RgaCnt-1)) ? RGA_BLIT_SYNC : RGA_BLIT_ASYNC;
    if(ioctl(_contextAnchor->engine_fd, RGA_BLIT_ASYNC, &Rga_Request) != 0) {
        LOGE(" %s(%d) RGA_BLIT fail",__FUNCTION__, __LINE__);
    }
// #endif       
    return 0; 
}
static int restorebuffer(hwbkupinfo *pbkupinfo, int direct_fd)
{
    struct rga_req  Rga_Request;
    RECT clip;
    memset(&Rga_Request, 0x0, sizeof(Rga_Request));

    clip.xmin = 0;
    clip.xmax = pbkupinfo->w_vir - 1;
    clip.ymin = 0;
    clip.ymax = pbkupinfo->h_vir - 1;


    ALOGV("restorebuffer addr=[%x,%x],bkmem=[%x,%x],direct_fd=%x,w-h[%d,%d][%d,%d,%d,%d][f=%d]",
        pbkupinfo->buf_fd, pbkupinfo->buf_addr_log, pbkupinfo->membk_fd, pbkupinfo->pmem_bk_log,direct_fd,pbkupinfo->w_vir,
        pbkupinfo->h_vir,pbkupinfo->xoffset,pbkupinfo->yoffset,pbkupinfo->w_act,pbkupinfo->h_act,pbkupinfo->format);

    //RGA_set_src_vir_info(&Rga_Request, (int)pbkupinfo->pmem_bk_log, 0, 0,pbkupinfo->w_act, pbkupinfo->h_act, pbkupinfo->format, 0);
   // RGA_set_dst_vir_info(&Rga_Request, (int)pbkupinfo->buf_addr_log, 0, 0,pbkupinfo->w_vir, pbkupinfo->h_vir, &clip, pbkupinfo->format, 0);
   // RGA_set_mmu_info(&Rga_Request, 1, 0, 0, 0, 0, 2);
      
    RGA_set_src_vir_info(&Rga_Request,  pbkupinfo->membk_fd, 0, 0,pbkupinfo->w_act, pbkupinfo->h_act, pbkupinfo->format, 0);
    if(direct_fd)
        RGA_set_dst_vir_info(&Rga_Request, direct_fd, 0, 0,pbkupinfo->w_vir, pbkupinfo->h_vir, &clip, pbkupinfo->format, 0);    
    else
    RGA_set_dst_vir_info(&Rga_Request, pbkupinfo->buf_fd, 0, 0,pbkupinfo->w_vir, pbkupinfo->h_vir, &clip, pbkupinfo->format, 0);
    RGA_set_bitblt_mode(&Rga_Request, 0, 0,0,0,0,0);
    RGA_set_src_act_info(&Rga_Request,pbkupinfo->w_act,  pbkupinfo->h_act, 0, 0);
    RGA_set_dst_act_info(&Rga_Request,pbkupinfo->w_act,  pbkupinfo->h_act,  pbkupinfo->xoffset, pbkupinfo->yoffset);
    if(ioctl(_contextAnchor->engine_fd, RGA_BLIT_ASYNC, &Rga_Request) != 0) {
        LOGE(" %s(%d) RGA_BLIT fail",__FUNCTION__, __LINE__);
    }
    return 0; 
}
static int  CopyBuffByRGA (hwbkupinfo *pcpyinfo)
{
    struct rga_req  Rga_Request;
    RECT clip;
    memset(&Rga_Request, 0x0, sizeof(Rga_Request));
    clip.xmin = 0;
    clip.xmax = pcpyinfo->w_vir - 1;
    clip.ymin = 0;
    clip.ymax = pcpyinfo->h_vir - 1;
    ALOGV("CopyBuffByRGA addr=[%x,%x],bkmem=[%x,%x],w-h[%d,%d][%d,%d,%d,%d][f=%d]",
        pcpyinfo->buf_fd, pcpyinfo->buf_addr_log, pcpyinfo->membk_fd, pcpyinfo->pmem_bk_log,pcpyinfo->w_vir,
        pcpyinfo->h_vir,pcpyinfo->xoffset,pcpyinfo->yoffset,pcpyinfo->w_act,pcpyinfo->h_act,pcpyinfo->format);
    RGA_set_src_vir_info(&Rga_Request,  pcpyinfo->membk_fd, 0, 0,pcpyinfo->w_vir, pcpyinfo->h_vir, pcpyinfo->format, 0);
    RGA_set_dst_vir_info(&Rga_Request, pcpyinfo->buf_fd, 0, 0,pcpyinfo->w_vir, pcpyinfo->h_vir, &clip, pcpyinfo->format, 0);    
    RGA_set_bitblt_mode(&Rga_Request, 0, 0,0,0,0,0);
    RGA_set_src_act_info(&Rga_Request,pcpyinfo->w_act,  pcpyinfo->h_act, pcpyinfo->xoffset,pcpyinfo->yoffset);
    RGA_set_dst_act_info(&Rga_Request,pcpyinfo->w_act,  pcpyinfo->h_act,  pcpyinfo->xoffset, pcpyinfo->yoffset);

   // uint32_t RgaFlag = (i==(RgaCnt-1)) ? RGA_BLIT_SYNC : RGA_BLIT_ASYNC;
    if(ioctl(_contextAnchor->engine_fd, RGA_BLIT_ASYNC, &Rga_Request) != 0) {
        LOGE(" %s(%d) RGA_BLIT fail",__FUNCTION__, __LINE__);
    }
        
    return 0; 
}
static int Is_lcdc_using( int fd)
{
    int i;
    int dsp_fd[4];
    hwcContext * context = _contextAnchor;
    // ioctl 
    // ioctl 
	int sync = 0;
    int count = 0;
    while(!sync)
    {
        count++;
        usleep(1000);
        ioctl(context->fbFd, RK_FBIOGET_LIST_STAT, &sync);
    }
    ioctl(context->fbFd, RK_FBIOGET_DSP_FD, dsp_fd);     
    for(i= 0;i<4;i++)
    {
        if(fd == dsp_fd[i])
            return 1;
    }
    return 0;
}

/**
 * @brief Sort by ypos (positive-order)
 *
 * @param win_id 		Win index
 * @param p_fb_info 	Win config data
 * @return 				Errno no
 */

static int  sort_area_by_ypos(int win_id,struct rk_fb_win_cfg_data* p_fb_info)
{
    int i,j,k;
    bool bSwitch;
	if((win_id !=2 && win_id !=3) || p_fb_info==NULL)
	{
		ALOGW("%s(%d):invalid param",__FUNCTION__,__LINE__);
		return -1;
	}

	struct rk_fb_area_par tmp_fb_area;
	for(i=0;i<4;i++)
	{
		if(p_fb_info->win_par[i].win_id == win_id)
		{
		    for(j=0;j<3;j++)
		    {
		        bSwitch=false;
                for(k=RK_WIN_MAX_REGION-1;k>j;k--)
                {
                    if((p_fb_info->win_par[i].area_par[k].ion_fd || p_fb_info->win_par[i].area_par[k].phy_addr)  &&
                        (p_fb_info->win_par[i].area_par[k-1].ion_fd || p_fb_info->win_par[i].area_par[k-1].phy_addr) )
                        {
                            if(p_fb_info->win_par[i].area_par[k].ypos < p_fb_info->win_par[i].area_par[k-1].ypos )
                            {
                                //switch
                                memcpy(&tmp_fb_area,&(p_fb_info->win_par[i].area_par[k-1]),sizeof(struct rk_fb_area_par));
                                memcpy(&(p_fb_info->win_par[i].area_par[k-1]),&(p_fb_info->win_par[i].area_par[k]),sizeof(struct rk_fb_area_par));
                                memcpy(&(p_fb_info->win_par[i].area_par[k]),&tmp_fb_area,sizeof(struct rk_fb_area_par));
                                bSwitch=true;
                            }
                        }
                }
                if(!bSwitch)    //end advance
                    return 0;
            }
            break;
        }
    }
	return 0;
}


static int
hwc_buff_recover(        
    int gpuflag
    )
{
    int LcdCont;
    hwbkupinfo cpyinfo;
    int i;
    hwcContext * context = _contextAnchor;
    ZoneManager* pzone_mag = &(context->zone_manager);    
    //bool IsDispDirect = bkupmanage.crrent_dis_fd == bkupmanage.direct_fd;//Is_lcdc_using(bkupmanage.direct_fd);//
    bool IsDispDirect = Is_lcdc_using(bkupmanage.direct_fd);//Is_lcdc_using(bkupmanage.direct_fd);//
    
    int needrev = 0;
    ALOGV("bkupmanage.dstwinNo=%d",bkupmanage.dstwinNo);
    if (context == NULL )
    {
        LOGE("%s(%d):Invalid device!", __FUNCTION__, __LINE__);
        return HWC_EGL_ERROR;
    }
    if(!gpuflag)
    {
        if( pzone_mag->zone_cnt <= 2 )
        {
            return 0;
        }
        LcdCont = 0;
        for(  i= 0; i < pzone_mag->zone_cnt ; i++)
        {
            if(pzone_mag->zone_info[i].dispatched == win0 || \
                pzone_mag->zone_info[i].dispatched == win1
               )
            {
                LcdCont ++;
            }
        }
        if( LcdCont != 2)
        {
            return 0;   // dont need recover
        }
    }
    for (i = 0; i < pzone_mag->zone_cnt && i< 2; i++)
    {
        struct private_handle_t * handle = pzone_mag->zone_info[i].handle;
        if( handle == NULL)
            continue;
        if( handle == bkupmanage.handle_bk && \
            handle->share_fd == bkupmanage.bkupinfo[0].buf_fd )
        {
            ALOGV(" handle->phy_addr==bkupmanage ,name=%s",pzone_mag->zone_info[i].LayerName);
            needrev = 1;
            break;
        }
    }
    if(!needrev)  
        return 0;
    if(!IsDispDirect)
    {
        struct rk_fb_win_cfg_data fb_info ;
        memset(&fb_info,0,sizeof(fb_info));
        cpyinfo.membk_fd = bkupmanage.bkupinfo[bkupmanage.count -1].buf_fd;
        cpyinfo.buf_fd = bkupmanage.direct_fd;
        cpyinfo.xoffset = 0;
        cpyinfo.yoffset = 0;
        cpyinfo.w_vir = bkupmanage.bkupinfo[0].w_vir;        
        cpyinfo.h_vir = bkupmanage.bkupinfo[0].h_vir;
        cpyinfo.w_act = bkupmanage.bkupinfo[0].w_vir;        
        cpyinfo.h_act = bkupmanage.bkupinfo[0].h_vir;
        cpyinfo.format = bkupmanage.bkupinfo[0].format;
        CopyBuffByRGA(&cpyinfo);
        if(ioctl(context->engine_fd, RGA_FLUSH, NULL) != 0)
        {
            LOGE("%s(%d):RGA_FLUSH Failed!", __FUNCTION__, __LINE__);
        }        
        fb_info.win_par[0].data_format = bkupmanage.bkupinfo[0].format;
        fb_info.win_par[0].win_id = 0;
        fb_info.win_par[0].z_order = 0;
        fb_info.win_par[0].area_par[0].ion_fd = bkupmanage.direct_fd;
        fb_info.win_par[0].area_par[0].acq_fence_fd = -1;
        fb_info.win_par[0].area_par[0].x_offset = 0;
        fb_info.win_par[0].area_par[0].y_offset = 0;
        fb_info.win_par[0].area_par[0].xpos = 0;
        fb_info.win_par[0].area_par[0].ypos = 0;
        fb_info.win_par[0].area_par[0].xsize = bkupmanage.bkupinfo[0].w_vir;
        fb_info.win_par[0].area_par[0].ysize = bkupmanage.bkupinfo[0].h_vir;
        fb_info.win_par[0].area_par[0].xact = bkupmanage.bkupinfo[0].w_vir;
        fb_info.win_par[0].area_par[0].yact = bkupmanage.bkupinfo[0].h_vir;
        fb_info.win_par[0].area_par[0].xvir = bkupmanage.bkupinfo[0].w_vir;
        fb_info.win_par[0].area_par[0].yvir = bkupmanage.bkupinfo[0].h_vir;
#ifdef USE_HWC_FENCE
        fb_info.wait_fs = 1;
#endif
        ioctl(context->fbFd, RK_FBIOSET_CONFIG_DONE, &fb_info);

#ifdef USE_HWC_FENCE
	for(int k=0;k<RK_MAX_BUF_NUM;k++)
	{
	    if(fb_info.rel_fence_fd[k]!= -1)
            close(fb_info.rel_fence_fd[k]);
	}
//	list->retireFenceFd = fb_info.ret_fence_fd;
#endif

        bkupmanage.crrent_dis_fd =  bkupmanage.direct_fd;            
    }
    for(i = 0; i < bkupmanage.count;i++)
    {    
        restorebuffer(&bkupmanage.bkupinfo[i],0);
    }
    if(ioctl(context->engine_fd, RGA_FLUSH, NULL) != 0)
    {
        LOGE("%s(%d):RGA_FLUSH Failed!", __FUNCTION__, __LINE__);
    }
    return 0;
}
int
hwc_layer_recover(
    hwc_composer_device_1_t * dev,
    size_t numDisplays,
    hwc_display_contents_1_t** displays
    )
{
    hwc_buff_recover(0);  
    return 0;
}

static int
hwc_LcdcToGpu(  
    hwc_display_contents_1_t* list
    )
{
    if(!bkupmanage.needrev)
        return 0;    
    hwc_buff_recover(1); 
    bkupmanage.needrev = 0;
    return 0; 
}
int hwc_do_special_composer( hwcContext * Context)
{
    int                 srcFd;
    unsigned int        srcWidth;
    unsigned int        srcHeight;
    RgaSURF_FORMAT      srcFormat;
    unsigned int        srcStride;

    int                 dstFd ;
    unsigned int        dstStride;
    unsigned int        dstWidth;
    unsigned int        dstHeight ;
    RgaSURF_FORMAT      dstFormat;
    int                 x_off;
    int                 y_off;
    unsigned int        act_dstwidth;
    unsigned int        act_dstheight;

    RECT clip;
    int DstBuferIndex = -1,ComposerIndex = 0;
    int LcdCont;
    unsigned char       planeAlpha;
    int                 perpixelAlpha;
    int                 curFd = 0;

    struct rga_req  Rga_Request[MAX_DO_SPECIAL_COUNT];
    int             RgaCnt = 0;
    int     dst_indexfid = 0;
    struct private_handle_t *handle_cur = NULL;  
    static int backcout = 0;
    bool IsDiff = 0;
    int dst_bk_fd = 0;
    ZoneManager* pzone_mag = &(Context->zone_manager);
    int i;
    int ext_cnt = 0;
    int ext_st = 0;

    for(i=0;i<pzone_mag->zone_cnt;i++)
    {
        if(pzone_mag->zone_info[i].dispatched  == win_ext)
           ext_cnt ++; 
    }
    if( ext_cnt == 0)
    {
        return 0;
    }
    for(i=0;i<pzone_mag->zone_cnt;i++)
    {
        if(pzone_mag->zone_info[i].dispatched  == win_ext)
        {
            ext_st = i;
            break; 
        }   
    }
    if(ext_st == 0)
    {
       goto BackToGPU;  
    }
    LcdCont = 0;
  
    memset(&Rga_Request, 0x0, sizeof(Rga_Request));
    ALOGD("ext_st=%d,ext_cnt=%d,zone_cnt=%d",ext_st,ext_cnt,pzone_mag->zone_cnt);
    for(ComposerIndex = ext_st ;ComposerIndex < (ext_st + ext_cnt); ComposerIndex++)
    {
        bool NeedBlit = true;
        ZoneInfo *psrcZeInfo = &pzone_mag->zone_info[ComposerIndex];
        srcWidth = psrcZeInfo->width;
        srcHeight = psrcZeInfo->height;
        srcStride = psrcZeInfo->stride;
        srcFd = psrcZeInfo->layer_fd;
        hwcGetFormat( psrcZeInfo->handle,
                 &srcFormat
                 );        
        srcFd = psrcZeInfo->layer_fd;
        for(DstBuferIndex = ext_st -1; DstBuferIndex >=0; DstBuferIndex--)
        {
            ZoneInfo *pdstZeInfo = &pzone_mag->zone_info[DstBuferIndex];

            bool IsWp = strstr(pdstZeInfo->LayerName,WALLPAPER);            
            if( IsWp) {               
                DstBuferIndex = -1;
                break;
            }
                      
            dstWidth = pdstZeInfo->width;
            dstHeight = pdstZeInfo->height;
            dstStride = pdstZeInfo->stride;
            dstFd = pdstZeInfo->layer_fd;            
            hwcGetFormat( pdstZeInfo->handle,
                 &dstFormat
                 );        
                 
            if(dstHeight > 4096) {
                LOGV("  %d->%d: dstHeight=%d > 2048", ComposerIndex, DstBuferIndex, dstHeight);
                continue;   // RGA donot support destination vir_h > 2048
            }
            
            hwc_rect_t const * srcVR = &(psrcZeInfo->disp_rect);
            hwc_rect_t const * dstVR = &(pdstZeInfo->disp_rect);

            LOGD("  %d->%d:  src= rot[%d] fmt[%d] wh[%d(%d),%d] src[%d,%d,%d,%d] vis[%d,%d,%d,%d]",
                ComposerIndex, DstBuferIndex,
                psrcZeInfo->realtransform, srcFormat, srcWidth, srcStride, srcHeight,
                psrcZeInfo->src_rect.left,psrcZeInfo->src_rect.top,
                psrcZeInfo->src_rect.right,psrcZeInfo->src_rect.bottom,
                srcVR->left, srcVR->top, srcVR->right, srcVR->bottom
                );
            LOGD("           dst= rot[%d] fmt[%d] wh[%d(%d),%d] src[%d,%d,%d,%d] vis[%d,%d,%d,%d] ",
                pdstZeInfo->realtransform, dstFormat, dstWidth, dstStride, dstHeight,
                pdstZeInfo->src_rect.left,pdstZeInfo->src_rect.top,
                pdstZeInfo->src_rect.right,pdstZeInfo->src_rect.bottom,
                dstVR->left, dstVR->top, dstVR->right, dstVR->bottom
                );

            // lcdc need address aligned to 128 byte when win1 area is too large.
            // (win0 area consider is large)
            // display width must smaller than dst stride.
            if( dstStride < (unsigned int)(dstVR->right - dstVR->left )) {
                LOGE("  dstStride[%d] < [%d]", dstStride, dstVR->right - dstVR->left);
                DstBuferIndex = -1;
                break;
            }

            // incoming param error, need to debug!
            if(dstVR->right > 4096) {
                LOGE("  dstLayer's VR right (%d) is too big!!!", dstVR->right);
                DstBuferIndex = -1;
                break;
            }

            act_dstwidth = srcWidth;
            act_dstheight = srcHeight;
            x_off = psrcZeInfo->disp_rect.left;
            y_off = psrcZeInfo->disp_rect.top;


          // if the srcLayer inside the dstLayer, then get DstBuferIndex and break.
            if( (srcVR->left >= dstVR->left )
             && (srcVR->top >= dstVR->top )
             && (srcVR->right <= dstVR->right )
             && (srcVR->bottom <= dstVR->bottom )
            )
            {
                handle_cur = pdstZeInfo->handle;
                break;
            }
        }

      //  if(ComposerIndex == 2) // first find ,store
          //  dst_indexfid = DstBuferIndex;
       // else if( DstBuferIndex != dst_indexfid )
         //   DstBuferIndex = -1;
        // there isn't suitable dstLayer to copy, use gpu compose.
        if (DstBuferIndex < 0)      goto BackToGPU;

        // Remove the duplicate copies of bottom bar.
        if(!(bkupmanage.dstwinNo == 0xff || bkupmanage.dstwinNo == DstBuferIndex) )
        {
            ALOGW(" last and current frame is not the win,[%d - %d]",bkupmanage.dstwinNo,DstBuferIndex);
            goto BackToGPU;
        }    
        if(srcFormat == RK_FORMAT_YCbCr_420_SP)
            goto BackToGPU;

        if (NeedBlit)
        {
            curFd = dstFd ;
            clip.xmin = 0;
            clip.xmax = dstStride - 1;
            clip.ymin = 0;
            clip.ymax = dstHeight - 1;
            //x_off  = x_off < 0 ? 0:x_off;


            LOGD("    src[%d]=%s,  dst[%d]=%s",ComposerIndex,pzone_mag->zone_info[ComposerIndex].LayerName,DstBuferIndex,pzone_mag->zone_info[DstBuferIndex].LayerName);
            LOGD("    src info f[%d] w_h[%d(%d),%d]",srcFormat,srcWidth,srcStride,srcHeight);
            LOGD("    dst info f[%d] w_h[%d(%d),%d] rect[%d,%d,%d,%d]",dstFormat,dstWidth,dstStride,dstHeight,x_off,y_off,act_dstwidth,act_dstheight);
            RGA_set_src_vir_info(&Rga_Request[RgaCnt], srcFd, (int)0, 0,srcStride, srcHeight, srcFormat, 0);
            RGA_set_dst_vir_info(&Rga_Request[RgaCnt], dstFd, (int)0, 0,dstStride, dstHeight, &clip, dstFormat, 0);
            /* Get plane alpha. */
            planeAlpha = pzone_mag->zone_info[ComposerIndex].zone_alpha;
            /* Setup blending. */
            {
               
                perpixelAlpha = _HasAlpha(srcFormat);
                LOGV("perpixelAlpha=%d,planeAlpha=%d,line=%d ",perpixelAlpha,planeAlpha,__LINE__);
                /* Setup alpha blending. */
                if (perpixelAlpha && planeAlpha < 255 && planeAlpha != 0)
                {

                   RGA_set_alpha_en_info(&Rga_Request[RgaCnt],1,2, planeAlpha ,1, 9,0);
                }
                else if (perpixelAlpha)
                {
                    /* Perpixel alpha only. */
                   RGA_set_alpha_en_info(&Rga_Request[RgaCnt],1,1, 0, 1, 3,0);

                }
                else /* if (planeAlpha < 255) */
                {
                    /* Plane alpha only. */
                   RGA_set_alpha_en_info(&Rga_Request[RgaCnt],1, 0, planeAlpha ,0,0,0);

                }
            }

            RGA_set_bitblt_mode(&Rga_Request[RgaCnt], 0, 0,0,0,0,0);
            RGA_set_src_act_info(&Rga_Request[RgaCnt],srcWidth, srcHeight,  0, 0);
            RGA_set_dst_act_info(&Rga_Request[RgaCnt], act_dstwidth, act_dstheight, x_off, y_off);

            RgaCnt ++;
        }
    }

#if 0
    // Check Aligned
    if(_contextAnchor->IsRk3188)
    {
        int TotalSize = 0;
        int32_t bpp ;
        bool  IsLarge = false;
        int DstLayerIndex;
        for(int i = 0; i < 2; i++)
        {
            hwc_layer_1_t *dstLayer = &(list->hwLayers[i]);
            hwc_region_t * Region = &(dstLayer->visibleRegionScreen);
            hwc_rect_t const * rects = Region->rects;
            struct private_handle_t * handle_pre = (struct private_handle_t *) dstLayer->handle;
            bpp = android::bytesPerPixel(handle_pre->format);

            TotalSize += (rects[0].right - rects[0].left) \
                            *  (rects[0].bottom - rects[0].top) * 4;
        }
        // fb regard as RGBX , datasize is width * height * 4, so 0.75 multiple is width * height * 4 * 3/4
        if ( TotalSize >= (_contextAnchor->fbWidth * _contextAnchor->fbHeight * 3))
        {
            IsLarge = true;
        }
        for(DstLayerIndex = 1; DstLayerIndex >=0; DstLayerIndex--)
        {
            hwc_layer_1_t *dstLayer = &(list->hwLayers[DstLayerIndex]);

            hwc_rect_t * DstRect = &(dstLayer->displayFrame);
            hwc_rect_t * SrcRect = &(dstLayer->sourceCrop);
            hwc_region_t * Region = &(dstLayer->visibleRegionScreen);
            hwc_rect_t const * rects = Region->rects;
            struct private_handle_t * handle_pre = (struct private_handle_t *) dstLayer->handle;
            hwcRECT dstRects;
            hwcRECT srcRects;
            int xoffset;
            bpp = android::bytesPerPixel(handle_pre->format);

            hwcLockBuffer(_contextAnchor,
                  (struct private_handle_t *) dstLayer->handle,
                  &dstLogical,
                  &dstPhysical,
                  &dstWidth,
                  &dstHeight,
                  &dstStride,
                  &dstInfo);


            dstRects.left   = hwcMAX(DstRect->left,   rects[0].left);

            srcRects.left   = SrcRect->left
                - (int) (DstRect->left   - dstRects.left);

            xoffset = hwcMAX(srcRects.left - dstLayer->exLeft, 0);


            LOGV("[%d]=%s,IsLarge=%d,dstStride=%d,xoffset=%d,exAddrOffset=%d,bpp=%d,dstPhysical=%x",
                DstLayerIndex,list->hwLayers[DstLayerIndex].LayerName,
                IsLarge, dstStride,xoffset,dstLayer->exAddrOffset,bpp,dstPhysical);
            if( IsLarge &&
                ((dstStride * bpp) % 128 || (xoffset * bpp + dstLayer->exAddrOffset) % 128)
            )
            {
                LOGV("  Not 128 aligned && win is too large!") ;
                break;
            }


        }
        if (DstLayerIndex >= 0)     goto BackToGPU;        
    }
    // there isn't suitable dstLayer to copy, use gpu compose.
#endif

	/*
    if(!strcmp("Keyguard",list->hwLayers[DstBuferIndex].LayerName))
    {       
         bkupmanage.skipcnt = 10;
    }
    else if( bkupmanage.skipcnt > 0)
    {
        bkupmanage.skipcnt --;
        if(bkupmanage.skipcnt > 0)
          goto BackToGPU; 
    }
	*/
#if 0   
    if(strcmp(bkupmanage.LayerName,list->hwLayers[DstBuferIndex].LayerName))
    {
        ALOGD("[%s],[%s]",bkupmanage.LayerName,list->hwLayers[DstBuferIndex].LayerName);
        strcpy( bkupmanage.LayerName,list->hwLayers[DstBuferIndex].LayerName);        
        goto BackToGPU; 
    }
#endif    
    // Realy Blit
   // ALOGD("RgaCnt=%d",RgaCnt);
    IsDiff = handle_cur != bkupmanage.handle_bk \
                || (curFd != bkupmanage.bkupinfo[0].buf_fd &&
                    curFd != bkupmanage.bkupinfo[bkupmanage.count -1].buf_fd ); 

    if(!IsDiff )  // restore from current display buffer         
    {
        //if(bkupmanage.crrent_dis_fd != bkupmanage.direct_fd)
        if(!Is_lcdc_using(bkupmanage.direct_fd))
        {
            hwbkupinfo cpyinfo;
            ALOGV("bkupmanage.invalid=%d",bkupmanage.invalid);
            if(bkupmanage.invalid)
            {
                cpyinfo.membk_fd = bkupmanage.bkupinfo[bkupmanage.count -1].buf_fd;
                cpyinfo.buf_fd = bkupmanage.direct_fd;
                cpyinfo.xoffset = 0;
                cpyinfo.yoffset = 0;
                cpyinfo.w_vir = bkupmanage.bkupinfo[0].w_vir;        
                cpyinfo.h_vir = bkupmanage.bkupinfo[0].h_vir;
                cpyinfo.w_act = bkupmanage.bkupinfo[0].w_vir;        
                cpyinfo.h_act = bkupmanage.bkupinfo[0].h_vir;
                cpyinfo.format = bkupmanage.bkupinfo[0].format;
                CopyBuffByRGA(&cpyinfo);
                bkupmanage.invalid = 0;
            }    
            pzone_mag->zone_info[DstBuferIndex].direct_fd = bkupmanage.direct_fd;
            dst_bk_fd = bkupmanage.crrent_dis_fd = bkupmanage.direct_fd;
            for(int i=0; i<RgaCnt; i++)
            {
                Rga_Request[i].dst.yrgb_addr = bkupmanage.direct_fd;
            }
            
        }
        for(int i=0; i<bkupmanage.count; i++) 
        {
            restorebuffer(&bkupmanage.bkupinfo[i],dst_bk_fd);
        }   
        if(!dst_bk_fd  )
            bkupmanage.crrent_dis_fd =  bkupmanage.bkupinfo[bkupmanage.count -1].buf_fd;
    }
    for(int i=0; i<RgaCnt; i++) {

        if(IsDiff) // backup the dstbuff        
        {
            bkupmanage.bkupinfo[i].format = Rga_Request[i].dst.format;
            bkupmanage.bkupinfo[i].buf_fd = Rga_Request[i].dst.yrgb_addr;
            bkupmanage.bkupinfo[i].buf_addr_log = (void*)Rga_Request[i].dst.uv_addr;            
            bkupmanage.bkupinfo[i].xoffset = Rga_Request[i].dst.x_offset;
            bkupmanage.bkupinfo[i].yoffset = Rga_Request[i].dst.y_offset;
            bkupmanage.bkupinfo[i].w_vir = Rga_Request[i].dst.vir_w;
            bkupmanage.bkupinfo[i].h_vir = Rga_Request[i].dst.vir_h;            
            bkupmanage.bkupinfo[i].w_act = Rga_Request[i].dst.act_w;
            bkupmanage.bkupinfo[i].h_act = Rga_Request[i].dst.act_h;  
            if(!i)
            {
                backcout = 0;
                bkupmanage.invalid = 1;
            }    
            bkupmanage.crrent_dis_fd =  bkupmanage.bkupinfo[i].buf_fd;
            if(Rga_Request[i].src.format == RK_FORMAT_RGBA_8888)
                bkupmanage.needrev = 1;
            else
                bkupmanage.needrev = 0;
            backcout ++;
            backupbuffer(&bkupmanage.bkupinfo[i]);

        }
        uint32_t RgaFlag = (i==(RgaCnt-1)) ? RGA_BLIT_SYNC : RGA_BLIT_ASYNC;
        if(ioctl(_contextAnchor->engine_fd, RgaFlag, &Rga_Request[i]) != 0) {
            LOGE(" %s(%d) RGA_BLIT fail",__FUNCTION__, __LINE__);
        }
        bkupmanage.dstwinNo = DstBuferIndex;

    }
    bkupmanage.handle_bk = handle_cur;
    bkupmanage.count = backcout;
    return 0;

BackToGPU:
   // ALOGD(" go brack to GPU");
    return -1;
}
 
//extern "C" void *blend(uint8_t *dst, uint8_t *src, int dst_w, int src_w, int src_h);


int hwc_prepare_virtual(hwc_composer_device_1_t * dev, hwc_display_contents_1_t  *contents)
{
	if (contents==NULL)
	{
		return -1;
	}
	hwcContext * context = _contextAnchor;
	for (size_t j = 0; j <(contents->numHwLayers - 1); j++)
	{
		struct private_handle_t * handle = (struct private_handle_t *)contents->hwLayers[j].handle;

		if (handle && GPU_FORMAT==HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO)
		{
			ALOGD("WFD rga_video_copybit,%x,w=%d,h=%d",\
				GPU_BASE,GPU_WIDTH,GPU_HEIGHT);
			if (context->wfdOptimize==0)
			{
				rga_video_copybit(handle,0,0,0);
			}
		}
	}
	return 0;
}

static int hwc_prepare_external(hwc_composer_device_1 *dev,
                               hwc_display_contents_1_t *list) {
    //XXX: Fix when framework support is added
    return 0;
}
static int hwc_prepare_primary(hwc_composer_device_1 *dev, hwc_display_contents_1_t *list) 
{
    size_t i;
    size_t j;
    
    hwcContext * context = _contextAnchor;
    int ret;
    bool vertical = false;
#if hwcDumpSurface
    _DumpSurface(list);
#endif
    char value[PROPERTY_VALUE_MAX];
    int new_value = 0;

    /* Check layer list. */
    if ((list == NULL)
    ||  (list->numHwLayers == 0)
    //||  !(list->flags & HWC_GEOMETRY_CHANGED)
    )
    {
        return 0;
    }

    LOGV("%s(%d):>>> hwc_prepare_primary %d layers <<<",
         __FUNCTION__,
         __LINE__,
         list->numHwLayers -1);
#if hwcDEBUG
    LOGD("%s(%d):Layers to prepare:", __FUNCTION__, __LINE__);
    _Dump(list);
#endif

    for (i = 0; i < (list->numHwLayers - 1); i++)
    {
        struct private_handle_t * handle = (struct private_handle_t *) list->hwLayers[i].handle;

        if( ( list->hwLayers[i].flags & HWC_SKIP_LAYER)
            ||(handle == NULL)
           )
            continue;

        if( GPU_FORMAT == HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO)
        {
            tVPU_FRAME vpu_hd;
            bool IsDiff = false;
            if(context->vdieo_hd != handle && context->video_base != (void*)handle->base)
               IsDiff = true;
            if(IsDiff)   
            {
                memcpy(&vpu_hd,(void*)handle->base,sizeof(tVPU_FRAME));
                handle->video_addr = vpu_hd.videoAddr[0];
                handle->video_width = vpu_hd.width;
                handle->video_height = vpu_hd.height;  
                ALOGV("prepare [%x,%dx%d]",handle->video_addr,handle->video_width,handle->video_height);
                context->vdieo_hd = handle ;
                context->video_base = (void*)handle->base;
            }    
        }
        if(list->hwLayers[i].realtransform == HAL_TRANSFORM_ROT_90
            || list->hwLayers[i].realtransform == HAL_TRANSFORM_ROT_270 )
        {
            vertical = true;
        }

    }
    /* Check all layers: tag with different compositionType. */
    for (i = 0; i < (list->numHwLayers - 1); i++)
    {
        hwc_layer_1_t * layer = &list->hwLayers[i];

        uint32_t compositionType =
             check_layer(context, list->numHwLayers - 1, i, layer);

        if (compositionType == HWC_FRAMEBUFFER)
        {          
            break;
        }
    }

    property_get("sys.hwc.diable", value, "0");
    new_value = atoi(value); 
    if(new_value == 1)
        goto GpuComP;
    /* Roll back to FRAMEBUFFER if any layer can not be handled. */
    if (i != (list->numHwLayers - 1))
    {
        goto GpuComP;
    }
    ret = collect_all_zones(context,list);
    if(ret !=0 )
    {
        goto GpuComP;
    }
   
    if(vertical == true)
    {
        #if 0
        ret = try_wins_dispatch_ver(context);
        if(ret)
        {
            goto GpuComP;
        }
        ret = hwc_do_special_composer(context);
        if(ret)
        {
            goto GpuComP;
        }
        #else
        ret = try_wins_dispatch_hor(context); 
        if(ret)
        {
            goto GpuComP; 
        }        
        #endif
        
        
    }   
    else
    {
        ret = try_wins_dispatch_hor(context); 
        if(ret)
        {
            goto GpuComP; 
        }
    
    }            
    return 0;
GpuComP   :

    hwc_LcdcToGpu(list);         //Dont remove
    bkupmanage.dstwinNo = 0xff;  // GPU handle
    bkupmanage.invalid = 1;
    for (j = 0; j <(list->numHwLayers - 1); j++)
    {
        struct private_handle_t * handle = (struct private_handle_t *)list->hwLayers[j].handle;
        list->hwLayers[j].compositionType = HWC_FRAMEBUFFER;        
        if (handle && GPU_FORMAT==HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO)
        {
            ALOGV("rga_video_copybit,%x,w=%d,h=%d",\
                            GPU_BASE,GPU_WIDTH,GPU_HEIGHT);
            rga_video_copybit(handle,0,0,0);
        }
    }
    context->zone_manager.composter_mode = HWC_FRAMEBUFFER;
    
    return 0;


}


int
hwc_prepare(
    hwc_composer_device_1_t * dev,
    size_t numDisplays,
    hwc_display_contents_1_t** displays
    )
{
    char value[PROPERTY_VALUE_MAX];
    int new_value = 0;
    hwcContext * context = _contextAnchor;
    int ret = 0;
    size_t i;
    hwc_display_contents_1_t* list = displays[0];  // ignore displays beyond the first

    /* Check device handle. */
    if (context == NULL
    || &context->device.common != (hw_device_t *) dev
    )
    {
        LOGE("%s(%d):Invalid device!", __FUNCTION__, __LINE__);
        return HWC_EGL_ERROR;
    }



    context->zone_manager.composter_mode = HWC_FRAMEBUFFER;

    property_get("sys.hwc.compose_policy", value, "0");
    new_value = atoi(value); 
   // new_value = 0;
    /* Roll back to FRAMEBUFFER if any layer can not be handled. */
    if(new_value <= 0 )
    {
        for (i = 0; i < (list->numHwLayers - 1); i++)
        {
            list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
        }
        return 0;
    }
#if hwcDEBUG
    LOGD("%s(%d):Layers to prepare:", __FUNCTION__, __LINE__);
    _Dump(list);
#endif

    for (size_t i = 0; i < numDisplays; i++) {
        hwc_display_contents_1_t *list = displays[i];
        switch(i) {
            case HWC_DISPLAY_PRIMARY:
                ret = hwc_prepare_primary(dev, list);
                break;
            case HWC_DISPLAY_EXTERNAL:
                ret = hwc_prepare_external(dev, list);
                break;
            case HWC_DISPLAY_VIRTUAL:
                if(list)
                {
                    ret = hwc_prepare_virtual(dev, displays[0]);
                }    
                break;
            default:
                ret = -EINVAL;
        }
    }
    return ret;
}

int hwc_blank(struct hwc_composer_device_1 *dev, int dpy, int blank)
{
    // We're using an older method of screen blanking based on
    // early_suspend in the kernel.  No need to do anything here.
    hwcContext * context = _contextAnchor;

    ALOGD("hwc_blank dpy[%d],blank[%d]",dpy,blank);
 //    return 0;
    switch (dpy) {
    case HWC_DISPLAY_PRIMARY: {
        int fb_blank = blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK;
        int err = ioctl(context->fbFd, FBIOBLANK, fb_blank);
        if (err < 0) {
            if (errno == EBUSY)
                ALOGD("%sblank ioctl failed (display already %sblanked)",
                        blank ? "" : "un", blank ? "" : "un");
            else
                ALOGE("%sblank ioctl failed: %s", blank ? "" : "un",
                        strerror(errno));
            return -errno;
        }
        else
        {
            context->fb_blanked = blank;
        }
        break;
    }

    case HWC_DISPLAY_EXTERNAL:
        /*
        if (pdev->hdmi_hpd) {
            if (blank && !pdev->hdmi_blanked)
                hdmi_disable(pdev);
            pdev->hdmi_blanked = !!blank;
        }
        */
        break;

    default:
        return -EINVAL;

    }

    return 0;
}

int hwc_query(struct hwc_composer_device_1* dev,int what, int* value)
{

    hwcContext * context = _contextAnchor;

    switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
        // we support the background layer
        value[0] = 1;
        break;
    case HWC_VSYNC_PERIOD:
        // vsync period in nanosecond
        value[0] = 1e9 / context->fb_fps;
        break;
    default:
        // unsupported query
        return -EINVAL;
    }
    return 0;
}

static int display_commit( int dpy)
{
 
    return 0;
}

static int hwc_fbPost(hwc_composer_device_1_t * dev, size_t numDisplays, hwc_display_contents_1_t** list)
{
    return 0;
}
static int hwc_primary_Post( hwcContext * context,hwc_display_contents_1_t* list)
{

    if (list == NULL)
    {
       return -1;
    }    
    if (context->fbFd>0)
    {      
        struct fb_var_screeninfo info;
        struct rk_fb_win_cfg_data fb_info;
        memset(&fb_info,0,sizeof(fb_info));
        int numLayers = list->numHwLayers;
        hwc_layer_1_t *fbLayer = &list->hwLayers[numLayers - 1];
        if (!fbLayer)
        {
            ALOGE("fbLayer=NULL");
            return -1;
        }
        info = context->info;
        struct private_handle_t*  handle = (struct private_handle_t*)fbLayer->handle;
        if (!handle)
        {
            ALOGE("hanndle=NULL");
            return -1;
        }


        ALOGV("hwc_primary_Post num=%d,ion=%d",numLayers,handle->share_fd);
        #if 0
        unsigned int videodata[2];

        videodata[0] = videodata[1] = context->fbPhysical;
	    if(ioctl(context->fbFd, RK_FBIOSET_DMABUF_FD, &(handle->share_fd))== -1)
	    {
	        ALOGE("RK_FBIOSET_DMABUF_FD err");
	        return -1;
	    }
        if (ioctl(context->fbFd, FB1_IOCTL_SET_YUV_ADDR, videodata) == -1)
        {
            ALOGE("FB1_IOCTL_SET_YUV_ADDR err");
            return -1;
        }

        unsigned int offset = handle->offset;
        info.yoffset = offset/context->fbStride;
        if (ioctl(context->fbFd, FBIOPUT_VSCREENINFO, &info) == -1)
        {
            ALOGE("FBIOPUT_VSCREENINFO err!");
        }
        else
        {
            int sync = 0;
            ioctl(context->fbFd, RK_FBIOSET_CONFIG_DONE, &sync);
        }
        #else

        unsigned int offset = handle->offset;        
        fb_info.win_par[0].data_format = context->fbhandle.format;
        fb_info.win_par[0].win_id = 0;
        fb_info.win_par[0].z_order = 0;
        fb_info.win_par[0].area_par[0].ion_fd = handle->share_fd;
#ifdef USE_HWC_FENCE
        fb_info.win_par[0].area_par[0].acq_fence_fd = -1;//fbLayer->acquireFenceFd;
#else
        fb_info.win_par[0].area_par[0].acq_fence_fd = -1;
#endif
        fb_info.win_par[0].area_par[0].x_offset = 0;
        fb_info.win_par[0].area_par[0].y_offset = offset/context->fbStride;
        fb_info.win_par[0].area_par[0].xpos = 0;
        fb_info.win_par[0].area_par[0].ypos = 0;
        fb_info.win_par[0].area_par[0].xsize = handle->width;
        fb_info.win_par[0].area_par[0].ysize = handle->height;
        fb_info.win_par[0].area_par[0].xact = handle->width;
        fb_info.win_par[0].area_par[0].yact = handle->height;
        fb_info.win_par[0].area_par[0].xvir = handle->width;
        fb_info.win_par[0].area_par[0].yvir = handle->height;
#ifdef USE_HWC_FENCE
	    fb_info.wait_fs=0;
#endif

        ioctl(context->fbFd, RK_FBIOSET_CONFIG_DONE, &fb_info);

#ifdef USE_HWC_FENCE

        for(int k=0;k<RK_MAX_BUF_NUM;k++)
        {
            if(fb_info.rel_fence_fd[k]!= -1)
                close(fb_info.rel_fence_fd[k]);
        }

        list->retireFenceFd = fb_info.ret_fence_fd;

#endif

        for(int i = 0;i<4;i++)
        {
            for(int j=0;j<4;j++)
            {
                if(fb_info.win_par[i].area_par[j].ion_fd || fb_info.win_par[i].area_par[j].phy_addr)
                    ALOGV("win[%d],area[%d],z_win[%d,%d],[%d,%d,%d,%d]=>[%d,%d,%d,%d],w_h_f[%d,%d,%d],fd=%d,addr=%x",
                        i,j,
                        fb_info.win_par[i].z_order,
                        fb_info.win_par[i].win_id,
                        fb_info.win_par[i].area_par[j].x_offset,
                        fb_info.win_par[i].area_par[j].y_offset,
                        fb_info.win_par[i].area_par[j].xact,
                        fb_info.win_par[i].area_par[j].yact,
                        fb_info.win_par[i].area_par[j].xpos,
                        fb_info.win_par[i].area_par[j].ypos,
                        fb_info.win_par[i].area_par[j].xsize,
                        fb_info.win_par[i].area_par[j].ysize,
                        fb_info.win_par[i].area_par[j].xvir,
                        fb_info.win_par[i].area_par[j].yvir,
                        fb_info.win_par[i].data_format,
                        fb_info.win_par[i].area_par[j].ion_fd,
                        fb_info.win_par[i].area_par[j].phy_addr);
            }
            
        }    
        
        #endif        
    }
    return 0;
}

static int hwc_set_blit(hwcContext * context, hwc_display_contents_1_t *list) 
{
    hwcSTATUS status = hwcSTATUS_OK;
    unsigned int i;

    struct private_handle_t * handle     = NULL;

    int needSwap = false;
    int blitflag = false;
    EGLBoolean success = EGL_FALSE;
    struct private_handle_t * fbhandle = NULL;

        /* Prepare. */
    for (i = 0; i < (list->numHwLayers-1); i++)
    {
        /* Check whether this composition can be handled by hwcomposer. */
        if (list->hwLayers[i].compositionType >= HWC_BLITTER)
        {
            #if ENABLE_HWC_WORMHOLE
            hwcRECT FbRect;
            hwcArea * area;
            hwc_region_t holeregion;
            #endif            

            /* Get gc buffer handle. */
            fbhandle =  &(context->fbhandle);
            
#if ENABLE_HWC_WORMHOLE
            /* Reset allocated areas. */
            if (context->compositionArea != NULL)
            {
                _FreeArea(context, context->compositionArea);

                context->compositionArea = NULL;
            }

            FbRect.left = 0;
            FbRect.top = 0;
            FbRect.right = GPU_WIDTH;
            FbRect.bottom = GPU_HEIGHT;

            /* Generate new areas. */
            /* Put a no-owner area with screen size, this is for worm hole,
             * and is needed for clipping. */
            context->compositionArea = _AllocateArea(context,
                                                     NULL,
                                                     &FbRect,
                                                     0U);

            /* Split areas: go through all regions. */
            for (size_t i = 0; i < list->numHwLayers-1; i++)
            {
                int owner = 1U << i;
                hwc_layer_1_t *  hwLayer = &list->hwLayers[i];
                hwc_region_t * region  = &hwLayer->visibleRegionScreen;

                /* Now go through all rectangles to split areas. */
                for (size_t j = 0; j < region->numRects; j++)
                {
                    /* Assume the region will never go out of dest surface. */
                    _SplitArea(context,
                               context->compositionArea,
                               (hwcRECT *) &region->rects[j],
                               owner);

                }

            }
#if DUMP_SPLIT_AREA
                LOGD("SPLITED AREA:");
                hwcDumpArea(context->compositionArea);
#endif

            area = context->compositionArea;

            while (area != NULL)
            {
                /* Check worm hole first. */
                if (area->owners == 0U)
                {

                    holeregion.numRects = 1;
                    holeregion.rects = (hwc_rect_t const*)&area->rect;
                    /* Setup worm hole source. */
                    LOGV(" WormHole [%d,%d,%d,%d]",
                            area->rect.left,
                            area->rect.top,
                            area->rect.right,
                            area->rect.bottom
                        );

                    hwcClear(context,
                                 0xFF000000,
                                 fbhandle,
                                 (hwc_rect_t *)&area->rect,
                                 &holeregion
                                 );

                    /* Advance to next area. */


                }
                 area = area->next;
            }

#endif

            /* Done. */
            needSwap = true;
            blitflag = true;
            break;
        }
        else if (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER)
        {
            /* Previous swap rectangle is gone. */
            needSwap = true;
            break;

        }
    }

        /* Go through the layer list one-by-one blitting each onto the FB */
    for (i = 0; i < list->numHwLayers; i++)
    {
        switch (list->hwLayers[i].compositionType)
        {
        case HWC_BLITTER:
            LOGV("%s(%d):Layer %d is BLIITER", __FUNCTION__, __LINE__, i);
            /* Do the blit. */
#if hwcBlitUseTime
            gettimeofday(&tpendblit1,NULL);
#endif

            hwcONERROR(
                hwcBlit(context,
                        &list->hwLayers[i],
                        fbhandle,
                        &list->hwLayers[i].sourceCrop,
                        &list->hwLayers[i].displayFrame,
                        &list->hwLayers[i].visibleRegionScreen));
#if hwcBlitUseTime
            gettimeofday(&tpendblit2,NULL);
            usec2 = 1000*(tpendblit2.tv_sec - tpendblit1.tv_sec) + (tpendblit2.tv_usec- tpendblit1.tv_usec)/1000;
            LOGD("hwcBlit compositer %d layers=%s use time=%ld ms",i,list->hwLayers[i].LayerName,usec2);
#endif

            break;

        case HWC_CLEAR_HOLE:
            LOGV("%s(%d):Layer %d is CLEAR_HOLE", __FUNCTION__, __LINE__, i);
            /* Do the clear, color = (0, 0, 0, 1). */
            /* TODO: Only clear holes on screen.
             * See Layer::onDraw() of surfaceflinger. */
            if (i != 0) break;

            hwcONERROR(
                hwcClear(context,
                         0xFF000000,
                         fbhandle,
                         &list->hwLayers[i].displayFrame,
                         &list->hwLayers[i].visibleRegionScreen));
            break;

        case HWC_DIM:
            LOGV("%s(%d):Layer %d is DIM", __FUNCTION__, __LINE__, i);
            if (i == 0)
            {
                /* Use clear instead of dim for the first layer. */
                hwcONERROR(
                    hwcClear(context,
                             ((list->hwLayers[0].blending & 0xFF0000) << 8),
                             fbhandle,
                             &list->hwLayers[i].displayFrame,
                             &list->hwLayers[i].visibleRegionScreen));
            }
            else
            {
                /* Do the dim. */
                hwcONERROR(
                    hwcDim(context,
                           &list->hwLayers[i],
                           fbhandle,
                           &list->hwLayers[i].displayFrame,
                           &list->hwLayers[i].visibleRegionScreen));
            }
            break;

        case HWC_OVERLAY:
            /* TODO: HANDLE OVERLAY LAYERS HERE. */
            LOGV("%s(%d):Layer %d is OVERLAY", __FUNCTION__, __LINE__, i);
            break;            

        default:
            LOGV("%s(%d):Layer %d is FRAMEBUFFER", __FUNCTION__, __LINE__, i);
            break;
        }
    }

    if (blitflag)
    {
        if(ioctl(context->engine_fd, RGA_FLUSH, NULL) != 0)
        {
            LOGE("%s(%d):RGA_FLUSH Failed!", __FUNCTION__, __LINE__);
        }
        else
        {
         success =  EGL_TRUE ;
        }
        display_commit(0); 
    }
    else
    {
        //hwc_fbPost(dev,numDisplays,displays);
    }

OnError:

    LOGE("%s(%d):Failed!", __FUNCTION__, __LINE__);
    return HWC_EGL_ERROR;
    
}

static int hwc_set_lcdc(hwcContext * context, hwc_display_contents_1_t *list) 
{
    ZoneManager* pzone_mag = &(context->zone_manager);
    int i,j;
    int z_order = 0;
    int win_no = 0;
    
    struct rk_fb_win_cfg_data fb_info ;

    memset(&fb_info,0,sizeof(fb_info));

    for(i=0;i<pzone_mag->zone_cnt;i++)
    {
        hwc_rect_t * psrc_rect = &(pzone_mag->zone_info[i].src_rect);            
        hwc_rect_t * pdisp_rect = &(pzone_mag->zone_info[i].disp_rect);            
        int area_no = 0;
        int win_id = 0;
        ALOGV("Zone[%d]->layer[%d],dispatched=%d,"
        "[%d,%d,%d,%d] =>[%d,%d,%d,%d],"
        "w_h_s_f[%d,%d,%d,%d],tr_rtr_bled[%d,%d,%d],"
        "layer_fd[%d],addr=%x,acq_fence_fd=%d"
        "layname=%s",    
        pzone_mag->zone_info[i].zone_index,
        pzone_mag->zone_info[i].layer_index,
        pzone_mag->zone_info[i].dispatched,
        pzone_mag->zone_info[i].src_rect.left,
        pzone_mag->zone_info[i].src_rect.top,
        pzone_mag->zone_info[i].src_rect.right,
        pzone_mag->zone_info[i].src_rect.bottom,
        pzone_mag->zone_info[i].disp_rect.left,
        pzone_mag->zone_info[i].disp_rect.top,
        pzone_mag->zone_info[i].disp_rect.right,
        pzone_mag->zone_info[i].disp_rect.bottom,
        pzone_mag->zone_info[i].width,
        pzone_mag->zone_info[i].height,
        pzone_mag->zone_info[i].stride,
        pzone_mag->zone_info[i].format,
        pzone_mag->zone_info[i].transform,
        pzone_mag->zone_info[i].realtransform,
        pzone_mag->zone_info[i].blend,
        pzone_mag->zone_info[i].layer_fd,
        pzone_mag->zone_info[i].addr,
        pzone_mag->zone_info[i].acq_fence_fd,
        pzone_mag->zone_info[i].LayerName);


        switch(pzone_mag->zone_info[i].dispatched) {
            case win0:
                {
                    #if 0
                    struct fb_fix_screeninfo finfo;
                    struct fb_var_screeninfo info;
        		    hwc_rect_t * psrc_rect = &(pzone_mag->zone_info[i].src_rect);            
        		    hwc_rect_t * pdisp_rect = &(pzone_mag->zone_info[i].disp_rect);            
                    int sync = 1;
                    if (ioctl(context->fbFd, FBIOGET_FSCREENINFO, &finfo) == -1)
                    {
                        LOGE("%s(%d):  fd[%d] Failed", __FUNCTION__, __LINE__,context->fbFd);
                        return hwcSTATUS_IO_ERR;
                    }

                    if (ioctl(context->fbFd, FBIOGET_VSCREENINFO, &info) == -1)
                    {
                        LOGE("%s(%d):  fd[%d] Failed", __FUNCTION__, __LINE__,context->fbFd);
                        return hwcSTATUS_IO_ERR;
                    }

                    info.activate = FB_ACTIVATE_NOW;
                    info.grayscale &= 0xff;
                                    
                    info.xoffset = hwcMAX(psrc_rect->left, 0);
                    info.yoffset = hwcMAX(psrc_rect->top, 0);                  
                    info.xres = (psrc_rect->right- psrc_rect->left) ;
                    info.yres = (psrc_rect->bottom - psrc_rect->top);
                    info.xres_virtual = pzone_mag->zone_info[i].stride;
                    info.yres_virtual = pzone_mag->zone_info[i].height;
                  
                    info.nonstd |=  hwcMAX(pdisp_rect->left, 0) << 8;                  
                    info.nonstd |= hwcMAX(pdisp_rect->top , 0) << 20;
                    info.grayscale |= (pdisp_rect->right - pdisp_rect->left) << 8;
                    info.grayscale |= (pdisp_rect->bottom - pdisp_rect->top) << 20;


                    info.activate |= FB_ACTIVATE_FORCE;
                    info.nonstd &= 0x00;                                     
                    if(pzone_mag->zone_info[i].format == HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO)
                    {
                		int videodata[2];
                		videodata[0] = pzone_mag->zone_info[i].addr;     
                		videodata[1] = videodata[0] + info.xres_virtual * info.yres_virtual; 
                        info.nonstd |= HAL_PIXEL_FORMAT_YCrCb_NV12;    
                        ALOGD("FB1_IOCTL_SET_YUV_ADDR=%x",pzone_mag->zone_info[i].addr);
                        if (ioctl(context->fbFd, FB1_IOCTL_SET_YUV_ADDR, videodata) == -1)
                        {
                            LOGE("%s(%d):  fd[%d] Failed,DataAddr=%x", __FUNCTION__, __LINE__,context->fbFd,videodata[0]);
                            return hwcSTATUS_IO_ERR;
                        }                        
                    }
                    else
                    {
                        info.nonstd |= pzone_mag->zone_info[i].format;//HAL_PIXEL_FORMAT_YCrCb_NV12;//HAL_PIXEL_FORMAT_RGB_565
                    
                    }
                     _DumpInfo(&info,0);    
                    
                    if (ioctl(context->fbFd, FBIOPUT_VSCREENINFO, &info) == -1)
                    {
                        LOGE("%s(%d):  fd[%d] Failed", __FUNCTION__, __LINE__,context->fbFd);
                        return -1;
                    }
                    #if 0
                    int v_fd=dup(pzone_mag->zone_info[i].layer_fd);
                    if ( ioctl(context->fbFd, RK_FBIOSET_DMABUF_FD, &(v_fd)) == -1)
                    {
                        LOGE("%s(%d):  fd[%d] SET_DMABUF_FD Failed,share_fd=%x", __FUNCTION__, __LINE__,pzone_mag->zone_info[i].layer_fd);
                        return -1;
                    }
                    #endif        

                    ioctl(context->fbFd, RK_FBIOSET_CONFIG_DONE, &sync);
                    return 0;
                    #else
                    win_no ++;
                    win_id = 0;
                    area_no = 0;
                    
                    #endif
                    ALOGV("[%d]dispatched=%d,z_order=%d",i,pzone_mag->zone_info[i].dispatched,z_order);
                    z_order++;
                }
                break;
            case win1:
                ALOGV("[%d]dispatched=%d,z_order=%d",i,pzone_mag->zone_info[i].dispatched,z_order);                
                win_no ++;
                win_id = 1;
                area_no = 0;                
                z_order++;                
                break;
            case win2_0:
                ALOGV("[%d]dispatched=%d,z_order=%d",i,pzone_mag->zone_info[i].dispatched,z_order);
                win_no ++;
                win_id = 2;
                area_no = 0;                
                
                z_order++;                
                break;
            case win2_1:
                win_id = 2;
                area_no = 1;                
                ALOGV("[%d]dispatched=%d,z_order=%d",i,pzone_mag->zone_info[i].dispatched,z_order);
                break;  
            case win2_2:
                win_id = 2;                
                area_no = 2;   
                ALOGV("[%d]dispatched=%d,z_order=%d",i,pzone_mag->zone_info[i].dispatched,z_order);
                break;
            case win2_3:
                win_id = 2;
                area_no = 3;   
                ALOGV("[%d]dispatched=%d,z_order=%d",i,pzone_mag->zone_info[i].dispatched,z_order);
                break;
            case win3_0:
                win_no ++;
                win_id = 3;
                area_no = 0;                    
                ALOGV("[%d]dispatched=%d,z_order=%d",i,pzone_mag->zone_info[i].dispatched,z_order);
                z_order++;                
                break;
            case win3_1:
                win_id = 3;
                area_no = 1;      
                
                ALOGV("[%d]dispatched=%d,z_order=%d",i,pzone_mag->zone_info[i].dispatched,z_order);
                break;   
            case win3_2:
                win_id = 3;
                area_no = 2;                                         
                ALOGV("[%d]dispatched=%d,z_order=%d",i,pzone_mag->zone_info[i].dispatched,z_order);
                break;
            case win3_3:
                win_id = 3;    
                area_no = 3;                       
                ALOGV("[%d]dispatched=%d,z_order=%d",i,pzone_mag->zone_info[i].dispatched,z_order);
                break;                 
             case win_ext:
                break;
            default:
                ALOGE("hwc_set_lcdc  err!");
                return -1;
         }    
        if(win_no ==1)         
        {
            if(pzone_mag->zone_info[i].format ==  HAL_PIXEL_FORMAT_RGBA_8888) //
            {
                fb_info.win_par[win_no-1].data_format = HAL_PIXEL_FORMAT_RGBX_8888;
            }
            else
            {
                fb_info.win_par[win_no-1].data_format =  pzone_mag->zone_info[i].format;
            }
                
        }
        else
        {
            fb_info.win_par[win_no-1].data_format =  pzone_mag->zone_info[i].format;
        }    
        fb_info.win_par[win_no-1].win_id = win_id;
        fb_info.win_par[win_no-1].z_order = z_order-1;
        fb_info.win_par[win_no-1].area_par[area_no].ion_fd = \
                        pzone_mag->zone_info[i].direct_fd ? \
                        pzone_mag->zone_info[i].direct_fd: pzone_mag->zone_info[i].layer_fd;     
        fb_info.win_par[win_no-1].area_par[area_no].phy_addr = pzone_mag->zone_info[i].addr;
#ifdef USE_HWC_FENCE
        fb_info.win_par[win_no-1].area_par[area_no].acq_fence_fd = -1;//pzone_mag->zone_info[i].acq_fence_fd;
#else
        fb_info.win_par[win_no-1].area_par[area_no].acq_fence_fd = -1;
#endif
        fb_info.win_par[win_no-1].area_par[area_no].x_offset = hwcMAX(psrc_rect->left, 0);
        fb_info.win_par[win_no-1].area_par[area_no].y_offset = hwcMAX(psrc_rect->top, 0);
        fb_info.win_par[win_no-1].area_par[area_no].xpos =  hwcMAX(pdisp_rect->left, 0);
        fb_info.win_par[win_no-1].area_par[area_no].ypos = hwcMAX(pdisp_rect->top , 0);
        fb_info.win_par[win_no-1].area_par[area_no].xsize = pdisp_rect->right - pdisp_rect->left;
        fb_info.win_par[win_no-1].area_par[area_no].ysize = pdisp_rect->bottom - pdisp_rect->top;
        if(pzone_mag->zone_info[i].transform == HWC_TRANSFORM_ROT_90
            || pzone_mag->zone_info[i].transform == HWC_TRANSFORM_ROT_270)
        {
            fb_info.win_par[win_no-1].area_par[area_no].xact = psrc_rect->bottom - psrc_rect->top;
            fb_info.win_par[win_no-1].area_par[area_no].yact = psrc_rect->right- psrc_rect->left;
            fb_info.win_par[win_no-1].area_par[area_no].xvir = pzone_mag->zone_info[i].height ;
            fb_info.win_par[win_no-1].area_par[area_no].yvir = pzone_mag->zone_info[i].stride;  
        }
        else
        {
        fb_info.win_par[win_no-1].area_par[area_no].xact = psrc_rect->right- psrc_rect->left;
        fb_info.win_par[win_no-1].area_par[area_no].yact = psrc_rect->bottom - psrc_rect->top;
        fb_info.win_par[win_no-1].area_par[area_no].xvir = pzone_mag->zone_info[i].stride;
        fb_info.win_par[win_no-1].area_par[area_no].yvir = pzone_mag->zone_info[i].height;
        }    
    }    

    //win2 & win3 need sort by ypos (positive-order)
    sort_area_by_ypos(2,&fb_info);
    sort_area_by_ypos(3,&fb_info);
   
    for(i = 0;i<4;i++)
    {
        for(j=0;j<4;j++)
        {
            if(fb_info.win_par[i].area_par[j].ion_fd || fb_info.win_par[i].area_par[j].phy_addr)
                ALOGV("win[%d],area[%d],z_win[%d,%d],[%d,%d,%d,%d]=>[%d,%d,%d,%d],w_h_f[%d,%d,%d],acq_fence_fd=%d,fd=%d,addr=%x",
                    i,j,
                    fb_info.win_par[i].z_order,
                    fb_info.win_par[i].win_id,
                    fb_info.win_par[i].area_par[j].x_offset,
                    fb_info.win_par[i].area_par[j].y_offset,
                    fb_info.win_par[i].area_par[j].xact,
                    fb_info.win_par[i].area_par[j].yact,
                    fb_info.win_par[i].area_par[j].xpos,
                    fb_info.win_par[i].area_par[j].ypos,
                    fb_info.win_par[i].area_par[j].xsize,
                    fb_info.win_par[i].area_par[j].ysize,
                    fb_info.win_par[i].area_par[j].xvir,
                    fb_info.win_par[i].area_par[j].yvir,
                    fb_info.win_par[i].data_format,
                    fb_info.win_par[i].area_par[j].acq_fence_fd,
                    fb_info.win_par[i].area_par[j].ion_fd,
                    fb_info.win_par[i].area_par[j].phy_addr);
        }
        
    }    

#ifdef USE_HWC_FENCE
	fb_info.wait_fs=0; //not wait acquire fence temp(wait in hwc)
#endif

    if(!context->fb_blanked)
    {
        if(ioctl(context->fbFd, RK_FBIOSET_CONFIG_DONE, &fb_info) == -1)
        {
            ALOGE("RK_FBIOSET_CONFIG_DONE err line=%d !",__LINE__);
        }           

#ifdef USE_HWC_FENCE
    	for(i=0;i<RK_MAX_BUF_NUM;i++)
    	{
            // ALOGD("rel_fence_fd[%d] = %d", i, fb_info.rel_fence_fd[i]);
            if(fb_info.rel_fence_fd[i] != -1)
                list->hwLayers[i].releaseFenceFd = fb_info.rel_fence_fd[i];
    	}

        list->retireFenceFd = fb_info.ret_fence_fd;
#else
    	for(i=0;i<RK_MAX_BUF_NUM;i++)
    	{
    	    if(fb_info.rel_fence_fd[i]!= -1)
                close(fb_info.rel_fence_fd[i]);
    	}

#endif

    }
    else
    {
        for(i=0;i< (list->numHwLayers -1);i++)
    	{
            list->hwLayers[i].releaseFenceFd = -1;    	   
    	}
        list->retireFenceFd = -1;
    }
    return 0;
}

static int hwc_set_primary(hwc_composer_device_1 *dev, hwc_display_contents_1_t *list) 
{
    hwcContext * context = _contextAnchor;
#if hwcUseTime
    struct timeval tpend1, tpend2;
    long usec1 = 0;
#endif
#if hwcBlitUseTime
    struct timeval tpendblit1, tpendblit2;
    long usec2 = 0;
#endif

    hwc_display_t dpy = NULL;
    hwc_surface_t surf = NULL;

    hwc_sync(list);
    if (list != NULL) {
        dpy = list->dpy;
        surf = list->sur;        
    }
    /* Check device handle. */
    if (context == NULL
    || &context->device.common != (hw_device_t *) dev
    )
    {
        LOGE("%s(%d): Invalid device!", __FUNCTION__, __LINE__);
        return HWC_EGL_ERROR;
    }

    /* Check layer list. */
    if (list == NULL 
        || list->numHwLayers == 0)
    {
        /* Reset swap rectangles. */
        return 0;
    }

    if(list->skipflag)
    {
        hwc_sync_release(list);
        return 0;
    }

    LOGV("%s(%d):>>> Set start %d layers <<<",
         __FUNCTION__,
         __LINE__,
         list->numHwLayers);



#if hwcDEBUG
    LOGD("%s(%d):Layers to set:", __FUNCTION__, __LINE__);
    _Dump(list);
#endif
    #if hwcUseTime
    gettimeofday(&tpend1,NULL);
    #endif


    if(context->zone_manager.composter_mode == HWC_BLITTER)
    {
        hwc_set_blit(context,list);
    }
    else if(context->zone_manager.composter_mode == HWC_LCDC) 
    {
        hwc_set_lcdc(context,list);
    }
    else if(context->zone_manager.composter_mode == HWC_FRAMEBUFFER)
    {
        hwc_primary_Post(context,list);
    }
    
#if hwcUseTime
    gettimeofday(&tpend2,NULL);
    usec1 = 1000*(tpend2.tv_sec - tpend1.tv_sec) + (tpend2.tv_usec- tpend1.tv_usec)/1000;
    LOGD("hwcBlit compositer %d layers use time=%ld ms",list->numHwLayers,usec1);
#endif
        //close(Context->fbFd1);
#ifdef ENABLE_HDMI_APP_LANDSCAP_TO_PORTRAIT            
    if (list != NULL && getHdmiMode()>0) 
    {
      if (bootanimFinish==0)
      {
	       char pro_value[16];
	       property_get("service.bootanim.exit",pro_value,0);
		   bootanimFinish = atoi(pro_value);
		   if (bootanimFinish > 0)
		   {
			 usleep(1000000);
		   }
      }
	  else
	  {
	      if (strstr(list->hwLayers[list->numHwLayers-1].LayerName,"FreezeSurface")<=0)
	      {

			  if (list->hwLayers[0].transform==HAL_TRANSFORM_ROT_90 || list->hwLayers[0].transform==HAL_TRANSFORM_ROT_270)
			  {
			    int rotation = list->hwLayers[0].transform;
				if (ioctl(_contextAnchor->fbFd, RK_FBIOSET_ROTATE, &rotation)!=0)
				{
				  LOGE("%s(%d):RK_FBIOSET_ROTATE error!", __FUNCTION__, __LINE__);
				}
			  }
			  else
			  {
				int rotation = 0;
				if (ioctl(_contextAnchor->fbFd, RK_FBIOSET_ROTATE, &rotation)!=0)
				{
				  LOGE("%s(%d):RK_FBIOSET_ROTATE error!", __FUNCTION__, __LINE__);
				}
			  }
	      }
	  }
    }
#endif

    hwc_sync_release(list);
    return 0; //? 0 : HWC_EGL_ERROR;
}        

int hwc_set_virtual(hwc_composer_device_1_t * dev, hwc_display_contents_1_t  **contents, unsigned int rga_fb_addr)
{
	hwc_display_contents_1_t* list_pri = contents[0];
	hwc_display_contents_1_t* list_wfd = contents[2];
	hwc_layer_1_t *  fbLayer = &list_pri->hwLayers[list_pri->numHwLayers - 1];
	hwc_layer_1_t *  wfdLayer = &list_wfd->hwLayers[list_wfd->numHwLayers - 1];
	hwcContext * context = _contextAnchor;
	struct timeval tpend1, tpend2;
	long usec1 = 0;
	gettimeofday(&tpend1,NULL);
	if (list_wfd)
	{
		hwc_sync(list_wfd);
	}
	if (fbLayer==NULL || wfdLayer==NULL)
	{
		return -1;
	}

	if ((context->wfdOptimize>0) && wfdLayer->handle)
	{
		hwc_cfg_t cfg;
		memset(&cfg, 0, sizeof(hwc_cfg_t));
		cfg.src_handle = (struct private_handle_t *)fbLayer->handle;
		cfg.transform = fbLayer->realtransform;
		ALOGD("++++transform=%d",cfg.transform);
		cfg.dst_handle = (struct private_handle_t *)wfdLayer->handle;
		cfg.src_rect.left = (int)fbLayer->displayFrame.left;
		cfg.src_rect.top = (int)fbLayer->displayFrame.top;
		cfg.src_rect.right = (int)fbLayer->displayFrame.right;
		cfg.src_rect.bottom = (int)fbLayer->displayFrame.bottom;
		//cfg.src_format = cfg.src_handle->format;

		cfg.rga_fbAddr = rga_fb_addr;
		cfg.dst_rect.left = (int)wfdLayer->displayFrame.left;
		cfg.dst_rect.top = (int)wfdLayer->displayFrame.top;
		cfg.dst_rect.right = (int)wfdLayer->displayFrame.right;
		cfg.dst_rect.bottom = (int)wfdLayer->displayFrame.bottom;
		//cfg.dst_format = cfg.dst_handle->format;
		set_rga_cfg(&cfg);
		do_rga_transform_and_scale();
	}
	
	if (list_wfd)
	{
		hwc_sync_release(list_wfd);
	}

	gettimeofday(&tpend2,NULL);
	usec1 = 1000*(tpend2.tv_sec - tpend1.tv_sec) + (tpend2.tv_usec- tpend1.tv_usec)/1000;
	ALOGD("hwc use time=%ld ms",usec1);
	return 0;
}
static int hwc_set_external(hwc_composer_device_1_t *dev, hwc_display_contents_1_t* list, int dpy)
{
    return 0;
}

int
hwc_set(
    hwc_composer_device_1_t * dev,
    size_t numDisplays,
    hwc_display_contents_1_t  ** displays
    )
{

    int ret = 0;

    for (uint32_t i = 0; i < numDisplays; i++) {
        hwc_display_contents_1_t* list = displays[i];
        switch(i) {
            case HWC_DISPLAY_PRIMARY:
                ret = hwc_set_primary(dev, list);
                break;
            case HWC_DISPLAY_EXTERNAL:
                ret = hwc_set_external(dev, list, i);
                break;
            case HWC_DISPLAY_VIRTUAL:           
                if (list)
                {
                  
                    unsigned int fb_addr = 0;
                   // fb_addr = context->hwc_ion.pion->phys + context->hwc_ion.last_offset;
                    hwc_set_virtual(dev, displays,fb_addr);
                }                
                break;
            default:
                ret = -EINVAL;
        }
    }
    // This is only indicative of how many times SurfaceFlinger posts
    // frames to the display.


    return ret;
}

static void hwc_registerProcs(struct hwc_composer_device_1* dev,
                                    hwc_procs_t const* procs)
{
    hwcContext * context = _contextAnchor;

    context->procs =  (hwc_procs_t *)procs;
}


static int hwc_event_control(struct hwc_composer_device_1* dev,
        int dpy, int event, int enabled)
{

    hwcContext * context = _contextAnchor;

    switch (event) {
    case HWC_EVENT_VSYNC:
    {
        int val = !!enabled;
        int err;

        err = ioctl(context->fbFd, RK_FBIOSET_VSYNC_ENABLE, &val);
        if (err < 0)
        {
            LOGE(" RK_FBIOSET_VSYNC_ENABLE err=%d",err);
            return -1;
        }
        return 0;
    }
    default:
        return -1;
    }
}

static void handle_vsync_event(hwcContext * context )
{

    if (!context->procs)
        return;

    int err = lseek(context->vsync_fd, 0, SEEK_SET);
    if (err < 0) {
        ALOGE("error seeking to vsync timestamp: %s", strerror(errno));
        return;
    }

    char buf[4096];
    err = read(context->vsync_fd, buf, sizeof(buf));
    if (err < 0) {
        ALOGE("error reading vsync timestamp: %s", strerror(errno));
        return;
    }
    buf[sizeof(buf) - 1] = '\0';

  //  errno = 0;
    uint64_t timestamp = strtoull(buf, NULL, 0) ;/*+ (uint64_t)(1e9 / context->fb_fps)  ;*/

    uint64_t mNextFakeVSync = timestamp + (uint64_t)(1e9 / context->fb_fps);


    struct timespec spec;
    spec.tv_sec  = mNextFakeVSync / 1000000000;
    spec.tv_nsec = mNextFakeVSync % 1000000000;

    do {
        err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &spec, NULL);
    } while (err<0 && errno == EINTR);


    if (err==0)
    {
        context->procs->vsync(context->procs, 0, mNextFakeVSync );
        //ALOGD(" timestamp=%lld ms,preid=%lld us",mNextFakeVSync/1000000,(uint64_t)(1e6 / context->fb_fps) );
    }
    else
    {
        ALOGE(" clock_nanosleep ERR!!!");
    }
}

static void *hwc_thread(void *data)
{
    hwcContext * context = _contextAnchor;



#if 0
    uint64_t timestamp = 0;
    nsecs_t now = 0;
    nsecs_t next_vsync = 0;
    nsecs_t sleep;
    const nsecs_t period = nsecs_t(1e9 / 50.0);
    struct timespec spec;
   // int err;
    do
    {

        now = systemTime(CLOCK_MONOTONIC);
        next_vsync = context->mNextFakeVSync;

        sleep = next_vsync - now;
        if (sleep < 0) {
            // we missed, find where the next vsync should be
            sleep = (period - ((now - next_vsync) % period));
            next_vsync = now + sleep;
        }
        context->mNextFakeVSync = next_vsync + period;

        spec.tv_sec  = next_vsync / 1000000000;
        spec.tv_nsec = next_vsync % 1000000000;

        do
        {
            err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &spec, NULL);
        } while (err<0 && errno == EINTR);

        if (err == 0)
        {
            if (context->procs && context->procs->vsync)
            {
                context->procs->vsync(context->procs, 0, next_vsync);

                ALOGD(" hwc_thread next_vsync=%lld ",next_vsync);
            }

        }

    } while (1);
#endif

  //    char uevent_desc[4096];
   // memset(uevent_desc, 0, sizeof(uevent_desc));



    char temp[4096];

    int err = read(context->vsync_fd, temp, sizeof(temp));
    if (err < 0) {
        ALOGE("error reading vsync timestamp: %s", strerror(errno));
        return NULL;
    }

    struct pollfd fds[1];
    fds[0].fd = context->vsync_fd;
    fds[0].events = POLLPRI;
    //fds[1].fd = uevent_get_fd();
    //fds[1].events = POLLIN;

    while (true) {
        int err = poll(fds, 1, -1);

        if (err > 0) {
            if (fds[0].revents & POLLPRI) {
                handle_vsync_event(context);
            }

        }
        else if (err == -1) {
            if (errno == EINTR)
                break;
            ALOGE("error in vsync thread: %s", strerror(errno));
        }
    }

    return NULL;
}



int
hwc_device_close(
    struct hw_device_t *dev
    )
{
    int i;
    int err;
    hwcContext * context = _contextAnchor;

    LOGD("%s(%d):Close hwc device in thread=%d",
         __FUNCTION__, __LINE__, gettid());
    ALOGD("hwc_device_close ----------------------");
    /* Check device. */
    if (context == NULL
    || &context->device.common != (hw_device_t *) dev
    )
    {
        LOGE("%s(%d):Invalid device!", __FUNCTION__, __LINE__);

        return -EINVAL;
    }

    if (--context->reference > 0)
    {
        /* Dereferenced only. */
        return 0;
    }

    if(context->engine_fd)
        close(context->engine_fd);
    /* Clean context. */
    if(context->vsync_fd > 0)
        close(context->vsync_fd);
    if(context->fbFd > 0)
    {
        close(context->fbFd );

    }
    if(context->fbFd1 > 0)
    {
        close(context->fbFd1 );
    }

    for(i=0;i<bakupbufsize;i++)
    {
        if(bkupmanage.bkupinfo[i].phd_bk)
        {
            err = context->mAllocDev->free(context->mAllocDev, bkupmanage.bkupinfo[i].phd_bk);
            ALOGW_IF(err, "free(...) failed %d (%s)", err, strerror(-err));            
        }      
    
    }
    if(bkupmanage.phd_drt)
    {
        err = context->mAllocDev->free(context->mAllocDev, bkupmanage.phd_drt);
        ALOGW_IF(err, "free(...) failed %d (%s)", err, strerror(-err));            
    }      
    

    pthread_mutex_destroy(&context->lock);
    free(context);

    _contextAnchor = NULL;

    return 0;
}

static int hwc_getDisplayConfigs(struct hwc_composer_device_1* dev, int disp,
            			uint32_t* configs, size_t* numConfigs)
{
   int ret = 0;
   hwcContext * pdev = ( hwcContext  *)dev; 
    //in 1.1 there is no way to choose a config, report as config id # 0
    //This config is passed to getDisplayAttributes. Ignore for now.
    switch(disp) {
        
        case HWC_DISPLAY_PRIMARY:
            if(*numConfigs > 0) {
                configs[0] = 0;
                *numConfigs = 1;
            }
            ret = 0; //NO_ERROR
            break;
        case HWC_DISPLAY_EXTERNAL:
            ret = -1; //Not connected
            if(pdev->dpyAttr[HWC_DISPLAY_EXTERNAL].connected) {
                ret = 0; //NO_ERROR
                if(*numConfigs > 0) {
                    configs[0] = 0;
                    *numConfigs = 1;
                }
            }
            break;
    }
   return 0;
}

static int hwc_getDisplayAttributes(struct hwc_composer_device_1* dev, int disp,
            			 uint32_t config, const uint32_t* attributes, int32_t* values)
{

    hwcContext  *pdev = (hwcContext  *)dev; 
    //If hotpluggable displays are inactive return error
    if(disp == HWC_DISPLAY_EXTERNAL && !pdev->dpyAttr[disp].connected) {
        return -1;
    }
    static  uint32_t DISPLAY_ATTRIBUTES[] = {
        HWC_DISPLAY_VSYNC_PERIOD,
        HWC_DISPLAY_WIDTH,
        HWC_DISPLAY_HEIGHT,
        HWC_DISPLAY_DPI_X,
        HWC_DISPLAY_DPI_Y,
        HWC_DISPLAY_NO_ATTRIBUTE,
     };
    //From HWComposer

    const int NUM_DISPLAY_ATTRIBUTES = (sizeof(DISPLAY_ATTRIBUTES)/sizeof(DISPLAY_ATTRIBUTES)[0]);

    for (size_t i = 0; i < NUM_DISPLAY_ATTRIBUTES - 1; i++) {
        switch (attributes[i]) {
        case HWC_DISPLAY_VSYNC_PERIOD:
            values[i] = pdev->dpyAttr[disp].vsync_period;
            break;
        case HWC_DISPLAY_WIDTH:
            values[i] = pdev->dpyAttr[disp].xres;
            ALOGD("%s disp = %d, width = %d",__FUNCTION__, disp,
                    pdev->dpyAttr[disp].xres);
            break;
        case HWC_DISPLAY_HEIGHT:
            values[i] = pdev->dpyAttr[disp].yres;
            ALOGD("%s disp = %d, height = %d",__FUNCTION__, disp,
                    pdev->dpyAttr[disp].yres);
            break;
        case HWC_DISPLAY_DPI_X:
            values[i] = (int32_t) (pdev->dpyAttr[disp].xdpi*1000.0);
            break;
        case HWC_DISPLAY_DPI_Y:
            values[i] = (int32_t) (pdev->dpyAttr[disp].ydpi*1000.0);
            break;
        default:
            ALOGE("Unknown display attribute %d",
                    attributes[i]);
            return -EINVAL;
        }
    }

   return 0;
}

int is_surport_wfd_optimize()
{
   char value[PROPERTY_VALUE_MAX];
   memset(value,0,PROPERTY_VALUE_MAX);
   property_get("drm.service.enabled", value, "false");
   if (!strcmp(value,"false"))
   {
     return false;
   }
   else
   {
     return true;
   }
}

int hwc_copybit(struct hwc_composer_device_1 *dev,
             hwc_layer_1_t *src_layer,
			 hwc_layer_1_t *dst_layer,
			 int flag
			 )
{
 

    return 0;
}

static void hwc_dump(struct hwc_composer_device_1* dev, char *buff, int buff_len)
{
  // return 0;
}

int
hwc_device_open(
    const struct hw_module_t * module,
    const char * name,
    struct hw_device_t ** device
    )
{
    int  status    = 0;
    int rel;
    hwcContext * context = NULL;
    struct fb_fix_screeninfo fixInfo;
    struct fb_var_screeninfo info;
    int refreshRate = 0;
    float xdpi;
    float ydpi;
    uint32_t vsync_period; 
    hw_module_t const* module_gr;
    int err;
    int stride_gr;
    int i;

    
    char pro_value[PROPERTY_VALUE_MAX];
    LOGD("%s(%d):Open hwc device in thread=%d",
         __FUNCTION__, __LINE__, gettid());

    *device = NULL;

    if (strcmp(name, HWC_HARDWARE_COMPOSER) != 0)
    {
        LOGE("%s(%d):Invalid device name!", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    /* Get context. */
    context = _contextAnchor;

    /* Return if already initialized. */
    if (context != NULL)
    {
        /* Increament reference count. */
        context->reference++;

        *device = &context->device.common;
        return 0;
    }


    /* Allocate memory. */
    context = (hwcContext *) malloc(sizeof (hwcContext));
    
    if(context == NULL)
    {
        LOGE("%s(%d):malloc Failed!", __FUNCTION__, __LINE__);
        return -EINVAL;
    }
    memset(context, 0, sizeof (hwcContext));

    context->fbFd = open("/dev/graphics/fb0", O_RDWR, 0);
    if(context->fbFd < 0)
    {
         hwcONERROR(hwcSTATUS_IO_ERR);
    }

    rel = ioctl(context->fbFd, FBIOGET_FSCREENINFO, &fixInfo);
    if (rel != 0)
    {
         hwcONERROR(hwcSTATUS_IO_ERR);
    }



    if (ioctl(context->fbFd, FBIOGET_VSCREENINFO, &info) == -1)
    {
         hwcONERROR(hwcSTATUS_IO_ERR);
    }
    
    xdpi = 1000 * (info.xres * 25.4f) / info.width;
    ydpi = 1000 * (info.yres * 25.4f) / info.height;

    refreshRate = 1000000000000LLU /
    (
       uint64_t( info.upper_margin + info.lower_margin + info.yres )
       * ( info.left_margin  + info.right_margin + info.xres )
       * info.pixclock
     );
    
    if (refreshRate == 0) {
        ALOGW("invalid refresh rate, assuming 60 Hz");
        refreshRate = 60*1000;
    }

    
    vsync_period  = 1000000000 / refreshRate;

    context->dpyAttr[HWC_DISPLAY_PRIMARY].fd = context->fbFd;
    //xres, yres may not be 32 aligned
    context->dpyAttr[HWC_DISPLAY_PRIMARY].stride = fixInfo.line_length /(info.xres/8);
    context->dpyAttr[HWC_DISPLAY_PRIMARY].xres = info.xres;
    context->dpyAttr[HWC_DISPLAY_PRIMARY].yres = info.yres;
    context->dpyAttr[HWC_DISPLAY_PRIMARY].xdpi = xdpi;
    context->dpyAttr[HWC_DISPLAY_PRIMARY].ydpi = ydpi;
    context->dpyAttr[HWC_DISPLAY_PRIMARY].vsync_period = vsync_period;
    context->dpyAttr[HWC_DISPLAY_PRIMARY].connected = true;
    context->info = info;

    /* Initialize variables. */
   
    context->device.common.tag = HARDWARE_DEVICE_TAG;
    context->device.common.version = HWC_DEVICE_API_VERSION_1_3;
    
    context->device.common.module  = (hw_module_t *) module;

    /* initialize the procs */
    context->device.common.close   = hwc_device_close;
    context->device.prepare        = hwc_prepare;
    context->device.set            = hwc_set;
   // context->device.common.version = HWC_DEVICE_API_VERSION_1_0;
    context->device.blank          = hwc_blank;
    context->device.query          = hwc_query;
    context->device.eventControl   = hwc_event_control;

    context->device.registerProcs  = hwc_registerProcs;

    context->device.getDisplayConfigs = hwc_getDisplayConfigs;
    context->device.getDisplayAttributes = hwc_getDisplayAttributes;
    context->device.rkCopybit = hwc_copybit;

    context->device.fbPost2 = hwc_fbPost;
    context->device.dump = hwc_dump;
    context->device.layer_recover   = hwc_layer_recover;

    /* Get gco2D object pointer. */
    
    context->engine_fd = open("/dev/rga",O_RDWR,0);
    if( context->engine_fd < 0)
    {
        hwcONERROR(hwcRGA_OPEN_ERR);
        ALOGE("rga open err!");

    }
    

#if ENABLE_WFD_OPTIMIZE
	 property_set("sys.enable.wfd.optimize","1");
#endif
    {
    	char value[PROPERTY_VALUE_MAX];
    	memset(value,0,PROPERTY_VALUE_MAX);
    	property_get("sys.enable.wfd.optimize", value, "0");
    	int type = atoi(value);
        context->wfdOptimize = type;
        init_rga_cfg(context->engine_fd);
        if (type>0 && !is_surport_wfd_optimize())
        {
           property_set("sys.enable.wfd.optimize","0");
        }
    }

    /* Initialize pmem and frameubffer stuff. */
   // context->fbFd         = 0;
   // context->fbPhysical   = ~0U;
   // context->fbStride     = 0;



    if ( info.pixclock > 0 )
    {
        refreshRate = 1000000000000000LLU /
        (
            uint64_t( info.vsync_len + info.upper_margin + info.lower_margin + info.yres )
            * ( info.hsync_len + info.left_margin  + info.right_margin + info.xres )
            * info.pixclock
        );
    }
    else
    {
        ALOGW("fbdev pixclock is zero");
    }

    if (refreshRate == 0)
    {
        refreshRate = 60*1000;  // 60 Hz
    }

    context->fb_fps = refreshRate / 1000.0f;

    context->fbPhysical = fixInfo.smem_start;
    context->fbStride   = fixInfo.line_length;
	context->fbhandle.width = info.xres;
	context->fbhandle.height = info.yres;
    context->fbhandle.format = info.nonstd & 0xff;
    context->fbhandle.stride = (info.xres+ 31) & (~31);
    context->pmemPhysical = ~0U;
    context->pmemLength   = 0;
	property_get("ro.rk.soc",pro_value,"0");
    context->fbSize = info.xres*info.yres*4*3;
    context->lcdSize = info.xres*info.yres*4; 



    memset(&bkupmanage,0,sizeof(hwbkupmanage));
    bkupmanage.dstwinNo = 0xff;
    err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module_gr);
    ALOGE_IF(err, "FATAL: can't find the %s module", GRALLOC_HARDWARE_MODULE_ID);
    if (err == 0) {
        gralloc_open(module_gr, &context->mAllocDev);
        for(i=0;i<bakupbufsize;i++)
        {
            err = context->mAllocDev->alloc(context->mAllocDev, context->fbhandle.width,\
                        context->fbhandle.height/4, context->fbhandle.format,\
                        GRALLOC_USAGE_HW_COMPOSER|GRALLOC_USAGE_HW_RENDER,\
                        (buffer_handle_t*)(&bkupmanage.bkupinfo[i].phd_bk),&stride_gr);
            if(!err){
                struct private_handle_t*phandle_gr = (struct private_handle_t*)bkupmanage.bkupinfo[i].phd_bk;
                bkupmanage.bkupinfo[i].membk_fd = phandle_gr->share_fd;
                ALOGD("@hwc alloc[%d] [%dx%d,f=%d],fd=%d ",i,phandle_gr->width,phandle_gr->height,phandle_gr->format,phandle_gr->share_fd);                                
    
            }
            else{
                ALOGE("hwc alloc[%d] faild",i);
                goto OnError;
            }
        
        }
        err = context->mAllocDev->alloc(context->mAllocDev, context->fbhandle.width, \
                                        context->fbhandle.height, context->fbhandle.format, \
                                        GRALLOC_USAGE_HW_COMPOSER|GRALLOC_USAGE_HW_RENDER, \
                                        (buffer_handle_t*)(&bkupmanage.phd_drt),&stride_gr);  
        if(!err){
            struct private_handle_t*phandle_gr = (struct private_handle_t*)bkupmanage.phd_drt;
            bkupmanage.direct_fd = phandle_gr->share_fd;
            ALOGD("@hwc alloc drt [%dx%d,f=%d],fd=%d ",phandle_gr->width,phandle_gr->height,phandle_gr->format,phandle_gr->share_fd);                                

        }
        else {
            ALOGE("hwc alloc[%d] faild",i);
            goto OnError;
        }
                                        
        
    }   
	else
	{
	    ALOGE(" GRALLOC_HARDWARE_MODULE_ID failed");
	}

    /* Increment reference count. */
    context->reference++;
    _contextAnchor = context;
    if (context->fbhandle.width > context->fbhandle.height)
    {
        property_set("sys.display.oritation","0");
    }
    else
    {
        property_set("sys.display.oritation","2");
    }

#if USE_HW_VSYNC

    context->vsync_fd = open("/sys/class/graphics/fb0/vsync", O_RDONLY, 0);
    //context->vsync_fd = open("/sys/devices/platform/rk30-lcdc.0/vsync", O_RDONLY);
    if (context->vsync_fd < 0) {
         hwcONERROR(hwcSTATUS_IO_ERR);
    }


    if (pthread_mutex_init(&context->lock, NULL))
    {
         hwcONERROR(hwcMutex_ERR);
    }

    if (pthread_create(&context->hdmi_thread, NULL, hwc_thread, context))
    {
         hwcONERROR(hwcTHREAD_ERR);
    }
#endif

    /* Return device handle. */
    *device = &context->device.common;

    LOGD("RGA HWComposer verison%s\n"
         "Device:               %p\n"
         "fb_fps=%f",
         "1.0.0",
         context,
         context->fb_fps);

    property_set("sys.ghwc.version","1.000_32"); 

    char Version[32];

    memset(Version,0,sizeof(Version));
    if(ioctl(context->engine_fd, RGA_GET_VERSION, Version) == 0)
    {
        property_set("sys.grga.version",Version);
        LOGD(" rga version =%s",Version);

    }
    /*
	 context->ippDev = new ipp_device_t();
	 rel = ipp_open(context->ippDev);
     if (rel < 0)
     {
        delete context->ippDev;
        context->ippDev = NULL;
	    ALOGE("Open ipp device fail.");
     }
     */
    init_hdmi_mode();
    pthread_t t;
    if (pthread_create(&t, NULL, rk_hwc_hdmi_thread, NULL))
    {
        LOGD("Create readHdmiMode thread error .");
    }


    return 0;

OnError:

    if (context->vsync_fd > 0)
    {
        close(context->vsync_fd);
    }
    if(context->fbFd > 0)
    {
        close(context->fbFd );

    }
    if(context->fbFd1 > 0)
    {
        close(context->fbFd1 );
    }

    for(i=0;i<bakupbufsize;i++)
    {
        if(bkupmanage.bkupinfo[i].phd_bk)
        {
            err = context->mAllocDev->free(context->mAllocDev, bkupmanage.bkupinfo[i].phd_bk);
            ALOGW_IF(err, "free(...) failed %d (%s)", err, strerror(-err));            
        }      
    
    }
    if(bkupmanage.phd_drt)
    {
        err = context->mAllocDev->free(context->mAllocDev, bkupmanage.phd_drt);
        ALOGW_IF(err, "free(...) failed %d (%s)", err, strerror(-err));            
    }      

    pthread_mutex_destroy(&context->lock);

    /* Error roll back. */ 
    if (context != NULL)
    {
        if (context->engine_fd != 0)
        {
            close(context->engine_fd);
        }
        free(context);

    }

    *device = NULL;

    LOGE("%s(%d):Failed!", __FUNCTION__, __LINE__);

    return -EINVAL;
}

int  getHdmiMode()
{
   #if 0
    char pro_value[16];
    property_get("sys.hdmi.mode",pro_value,0);
        int mode = atoi(pro_value);
    return mode;
   #else
      // LOGD("g_hdmi_mode=%d",g_hdmi_mode);
   #endif
   // LOGD("g_hdmi_mode=%d",g_hdmi_mode);
   
    return g_hdmi_mode;
}

void init_hdmi_mode()
{

        int fd = open("/sys/devices/virtual/switch/hdmi/state", O_RDONLY);
		
        if (fd > 0)
        {
                 char statebuf[100];
                 memset(statebuf, 0, sizeof(statebuf));
                 int err = read(fd, statebuf, sizeof(statebuf));

                if (err < 0)
                 {
                        ALOGE("error reading vsync timestamp: %s", strerror(errno));
                        return;
                 }
                close(fd);
                g_hdmi_mode = atoi(statebuf);
                g_hdmi_mode = 0;   // debug disable for temp
               /* if (g_hdmi_mode==0)
                {
                    property_set("sys.hdmi.mode", "0");
                }
                else
                {
                    property_set("sys.hdmi.mode", "1");
                }*/

        }
        else
        {
           LOGE("Open hdmi mode error.");
        }

}
int closeFb(int fd)
{
    if (fd > 0)
    {
        int disable = 0;

        if (ioctl(fd, 0x5019, &disable) == -1)
        {
            LOGE("close fb[%d] fail.",fd);
            return -1;
        }
        ALOGD("fb1 realy close!");
        return (close(fd));
    }
    return -1;
}
