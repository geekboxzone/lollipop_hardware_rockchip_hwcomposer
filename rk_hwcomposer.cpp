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
#include "hwc_ipp.h"
#include "hwc_rga.h"
#define MAX_DO_SPECIAL_COUNT        8
#define RK_FBIOSET_ROTATE            0x5003 
#define FPS_NAME                    "com.aatt.fpsm"
#define BOTTOM_LAYER_NAME           "NavigationBar"
#define TOP_LAYER_NAME              "StatusBar"
#define WALLPAPER                   "ImageWallpaper"
//#define ENABLE_HDMI_APP_LANDSCAP_TO_PORTRAIT
static int SkipFrameCount = 0;

static hwcContext * _contextAnchor = NULL;
static int bootanimFinish = 0;
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
    for (i = 0; i < (unsigned int) Region->numRects ;i++)
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
  
  for (int i=0; i<list->numHwLayers; i++)
  {
     if (list->hwLayers[i].acquireFenceFd>0)
     {
       sync_wait(list->hwLayers[i].acquireFenceFd,-1);
       ALOGV("fenceFd=%d,name=%s",list->hwLayers[i].acquireFenceFd,list->hwLayers[i].layerName);
     }

  }
}

void hwc_sync_release(hwc_display_contents_1_t  *list)
{
  for (int i=0; i<list->numHwLayers; i++)
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
static int layer_seq = 0;
int rga_video_copybit(struct private_handle_t *handle,int tranform,int w_valid,int h_valid)
{
    struct rga_req  Rga_Request;
    RECT clip;
    unsigned char RotateMode = 0;
    int Rotation = 0;
    int SrcVirW,SrcVirH,SrcActW,SrcActH;
    int DstVirW,DstVirH,DstActW,DstActH;
    
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
    ALOGV("src fd=[%x],w-h[%d,%d][f=%d]",
        handle->video_share_fd, handle->video_width, handle->video_height,RK_FORMAT_YCbCr_420_SP);
    ALOGV("dst fd=[%x],w-h[%d,%d][f=%d]",
        handle->share_fd, handle->width, handle->height,RK_FORMAT_YCbCr_420_SP);
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
            DstActW = h_valid;
            DstActH = w_valid;
            
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
            DstActW = h_valid;
            DstActH = w_valid;
            
            break;
        case 0:                      
        default:
            SrcVirW = handle->video_width;
            SrcVirH = handle->video_height;
            SrcActW = handle->video_width;
            SrcActH = handle->video_height;
            DstVirW = handle->width;
            DstVirH = handle->height;
            DstActW = handle->width;
            DstActH = handle->height;
            break;
    }        
    RGA_set_src_vir_info(&Rga_Request, handle->video_share_fd, 0, 0,SrcVirW, SrcVirH, RK_FORMAT_YCbCr_420_SP, 0);
    RGA_set_dst_vir_info(&Rga_Request, handle->share_fd, 0, 0,DstVirW,DstActH,&clip, RK_FORMAT_YCbCr_420_SP, 0);    
    RGA_set_bitblt_mode(&Rga_Request, 0, RotateMode,Rotation,0,0,0);    
    RGA_set_src_act_info(&Rga_Request,SrcActW,SrcActH, 0,0);
    RGA_set_dst_act_info(&Rga_Request,DstActW,DstActH, 0,0);

    if(ioctl(rga_fd, RGA_BLIT_SYNC, &Rga_Request) != 0) {
        LOGE(" %s(%d) RGA_BLIT fail",__FUNCTION__, __LINE__);
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
    {
        return 1;
    }
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
        return 0;
    if(zf->is_stretch || zf2->is_stretch )    
        return 0;
    if(is_x_intersect(&(zf->disp_rect),&(zf2->disp_rect)))     
        return 0;
    else
        return 1;
}

int collect_all_zones( hwcContext * Context,hwc_display_contents_1_t * list)
{
    int i,j;
    for (i = 0,j=0; i < (list->numHwLayers - 1) ; i++)
    {
        hwc_layer_1_t * layer = &list->hwLayers[i];
        hwc_region_t * Region = &layer->visibleRegionScreen;
        hwc_rect_t * SrcRect = &layer->sourceCrop;
        hwc_rect_t * DstRect = &layer->displayFrame;
        struct private_handle_t* SrcHnd = (struct private_handle_t *) layer->handle;
        unsigned int        srcWidth;
        unsigned int        srcHeight;
        float hfactor;
        float vfactor;
        hwcRECT srcRects[16];
        hwcRECT dstRects[16];
        unsigned int m;
        bool is_stretch = 0;
        hwc_rect_t const * rects = Region->rects;

        hfactor = (float) (SrcRect->right - SrcRect->left)
                / (DstRect->right - DstRect->left);

        vfactor = (float) (SrcRect->bottom - SrcRect->top)
                / (DstRect->bottom - DstRect->top);

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
            Context->zone_manager.zone_info[j].sort = 0;
            Context->zone_manager.zone_info[j].transform = layer->transform;
            Context->zone_manager.zone_info[j].realtransform = layer->realtransform;
            strcpy(Context->zone_manager.zone_info[j].LayerName,layer->LayerName);
            Context->zone_manager.zone_info[j].disp_rect.left = dstRects[k].left;
            Context->zone_manager.zone_info[j].disp_rect.top = dstRects[k].top;
            Context->zone_manager.zone_info[j].disp_rect.right = dstRects[k].right;
            Context->zone_manager.zone_info[j].disp_rect.bottom = dstRects[k].bottom;  
            if(SrcHnd->format == HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO)
            {
                int w_valid ,h_valid;
    		    hwc_rect_t * psrc_rect = &(Context->zone_manager.zone_info[j].src_rect);            
                Context->zone_manager.zone_info[j].format = HAL_PIXEL_FORMAT_YCrCb_NV12;
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
                        Context->zone_manager.zone_info[j].layer_fd = SrcHnd->video_share_fd; 
                        Context->zone_manager.zone_info[j].width = SrcHnd->video_width;
                        Context->zone_manager.zone_info[j].height = SrcHnd->video_height;
                        Context->zone_manager.zone_info[j].stride = SrcHnd->video_width;                                                    
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
                        w_valid = psrc_rect->bottom - psrc_rect->top;
                        h_valid = psrc_rect->right - psrc_rect->left;
                        Context->zone_manager.zone_info[j].layer_fd = SrcHnd->share_fd;  
                        Context->zone_manager.zone_info[j].width = h_valid;
                        Context->zone_manager.zone_info[j].height = w_valid;
                        Context->zone_manager.zone_info[j].stride = h_valid;                                                    
                        
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
                        Context->zone_manager.zone_info[j].width = h_valid;
                        Context->zone_manager.zone_info[j].height = w_valid ;
                        Context->zone_manager.zone_info[j].stride = h_valid;                                                                                  
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
                        h_valid = psrc_rect->right - psrc_rect->left;
                        w_valid = psrc_rect->bottom - psrc_rect->top;   
                        Context->zone_manager.zone_info[j].layer_fd = SrcHnd->share_fd; 
                        Context->zone_manager.zone_info[j].width = w_valid;
                        Context->zone_manager.zone_info[j].height = h_valid;
                        Context->zone_manager.zone_info[j].stride = w_valid;                                                    
                          
                        break;
                    default:
                        break;
                }    
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
            "w_h_s_f[%d,%d,%d,%d],tr_rtr_bled[%d,%d,%d],"
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
            Context->zone_manager.zone_info[i].LayerName);
    }
    return 0;
}

// return 0: suess
// return -1: fail
int try_wins_dispatch(hwcContext * Context)
{
    int win_disphed_flag[4] = {0,}; // win0, win1, win2, win3 flag which is dispatched
    int win_disphed[4] = {win0,win1,win2_0,win3_0};
    int i,j;
    int sort = 1;
    int cnt = 0;
    int srot_tal[4][2] = {0};
    int win_dispd = win2_0; 
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
    
    for(i=0;i<(pzone_mag->zone_cnt-1);)
    {
        pzone_mag->zone_info[i].sort = sort;
        cnt = 0;

        //means 4: win2 or win3 most has 4 zones 
        for(j=1;j<4 && (i+j) < pzone_mag->zone_cnt;j++)
        {
            ZoneInfo * cur_zf = &(pzone_mag->zone_info[i+j-1]);
            ZoneInfo * next_zf = &(pzone_mag->zone_info[i+j]);            
            if(is_zone_combine(cur_zf,next_zf))
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
        if( pzone_mag->zone_info[i].sort == 1)
            srot_tal[0][0]++;
        else if(pzone_mag->zone_info[i].sort == 2) 
            srot_tal[1][0]++;
        else if(pzone_mag->zone_info[i].sort == 3) 
            srot_tal[2][0]++;
        else if(pzone_mag->zone_info[i].sort == 4) 
            srot_tal[3][0]++;                        
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
    // second dispatch one zones win
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
                ALOGE("try_wins_dispatch sort err!");
                return -1;
        }
        ALOGD("zone[%d].dispatched[%d]=%s,sort=%d", \
        i,pzone_mag->zone_info[i].dispatched,
        compositionTypeName[pzone_mag->zone_info[i].dispatched -1],
        pzone_mag->zone_info[i].sort);

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
    hwc_layer_1_t * Layer,
    hwc_display_contents_1_t * list,
    int videoflag
    )
{
    struct private_handle_t * handle =
        (struct private_handle_t *) Layer->handle;

  
    char pro_value[PROPERTY_VALUE_MAX];
    (void) Context;
    (void) Count;
    (void) Index;

    /* Check whether this layer is forced skipped. */

    if ((Layer->flags & HWC_SKIP_LAYER)
        ||((Layer->transform != 0)&&(!videoflag))
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
       

        if (Layer->handle == NULL)
        {
            LOGE("%s(%d):No layer surface at %d.",
                 __FUNCTION__,
                 __LINE__,
                 Index);
            /* TODO: I BELIEVE YOU CAN HANDLE SUCH LAYER!. */
            if( SkipFrameCount == 0)
            {
                Layer->compositionType = HWC_FRAMEBUFFER;
                SkipFrameCount = 1;
            }
            else
            {
                Layer->compositionType = HWC_CLEAR_HOLE;
                Layer->flags           = 0;
            }

            break;
        }

        /* At least surfaceflinger can handle this layer. */
        Layer->compositionType = HWC_FRAMEBUFFER;

        /* Get format. */
        if( hwcGetFormat(handle, &format) != hwcSTATUS_OK
            || (LayerZoneCheck(Layer) != 0))
        {
             return HWC_FRAMEBUFFER;
        }

        LOGV("name[%d]=%s,phy_addr=%x",Index,list->hwLayers[Index].LayerName,handle->phy_addr);

        if(            
              (getHdmiMode()==0)            
            )  // layer <=3,do special processing

        {           
            Layer->compositionType = HWC_TOWIN0;
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
#include <ui/PixelFormat.h>

 
//extern "C" void *blend(uint8_t *dst, uint8_t *src, int dst_w, int src_w, int src_h);


int hwc_prepare_virtual(hwc_composer_device_1_t * dev, hwc_display_contents_1_t  *contents)
{
	if (contents==NULL)
	{
		return -1;
	}
	hwcContext * context = _contextAnchor;
	for (int j = 0; j <(contents->numHwLayers - 1); j++)
	{
		struct private_handle_t * handle = (struct private_handle_t *)contents->hwLayers[j].handle;

		if (handle && GPU_FORMAT==HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO)
		{
			ALOGD("WFD rga_video_copybit,%x,w=%d,h=%d",\
				GPU_BASE,GPU_WIDTH,GPU_HEIGHT);
			if (context->wfdOptimize==0)
			{
				if (!_contextAnchor->video_frame.vpu_handle)
				{
					rga_video_copybit(handle,0,0,0);
				}
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
    int videoflag = 0;
    hwcContext * context = _contextAnchor;
    int ret;
#if hwcDumpSurface
    _DumpSurface(list);
#endif

    /* Check layer list. */
    if ((list == NULL)
    ||  (list->numHwLayers == 0)
    //||  !(list->flags & HWC_GEOMETRY_CHANGED)
    )
    {
        return 0;
    }

    LOGD("%s(%d):>>> hwc_prepare_primary %d layers <<<",
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
            break;

        if( GPU_FORMAT == HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO)
        {
            videoflag = 1;
            break;
        }

    }
    /* Check all layers: tag with different compositionType. */
    for (i = 0; i < (list->numHwLayers - 1); i++)
    {
        hwc_layer_1_t * layer = &list->hwLayers[i];

        uint32_t compositionType =
             check_layer(context, list->numHwLayers - 1, i, layer,list,videoflag);

        if (compositionType == HWC_FRAMEBUFFER)
        {          
            break;
        }
    }

    /* Roll back to FRAMEBUFFER if any layer can not be handled. */
    if (i != (list->numHwLayers - 1))
    {
        size_t j;
        //if(new_value == 1)  // print log
        //LOGD("%s(%d):Fail back to 3D composition path,i=%x,list->numHwLayers=%d", __FUNCTION__, __LINE__,i,list->numHwLayers);

        for (j = 0; j <(list->numHwLayers - 1); j++)
        {
            list->hwLayers[j].compositionType = HWC_FRAMEBUFFER;
        }
        for (j = 0; j <(list->numHwLayers - 1); j++)
        {
            struct private_handle_t * handle = (struct private_handle_t *)list->hwLayers[j].handle;
            if (handle && GPU_FORMAT==HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO)
            {
                ALOGV("rga_video_copybit,%x,w=%d,h=%d",\
                                GPU_BASE,GPU_WIDTH,GPU_HEIGHT);
                rga_video_copybit(handle,0,0,0);
            }
        }
        return 0;

    }
    ret = collect_all_zones(context,list);
    if(ret !=0 )
    {
        context->zone_manager.composter_mode = HWC_FRAMEBUFFER;
    }
    else
    {
        ret = try_wins_dispatch(context); 
    }
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
    int videoflag = 0;
    hwcContext * context = _contextAnchor;
    int ret = 0;
    int i;
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
    new_value = 0; // debug force 0
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

    for (int i = 0; i < numDisplays; i++) {
        hwc_display_contents_1_t *list = displays[i];
        switch(i) {
            case HWC_DISPLAY_PRIMARY:
                ret = hwc_prepare_primary(dev, list);
                break;
            case HWC_DISPLAY_EXTERNAL:
                ret = hwc_prepare_external(dev, list);
                break;
            case HWC_DISPLAY_VIRTUAL:
                ret = hwc_prepare_virtual(dev, displays[0]);
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

    return 0;
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
    unsigned int videodata[2];
    int ret = 0;
    hwcContext * context = _contextAnchor;
 
    struct fb_var_screeninfo info;                    
    info = context->info;
    info.activate |= FB_ACTIVATE_FORCE;
    videodata[0] = videodata[1] = context->hwc_ion.pion->phys + context->hwc_ion.offset;
    ALOGV("hwc fbPost:%x,offset=%x,fd=%d,videodata=%x",\
           videodata[0],context->hwc_ion.offset,context->dpyAttr[0].fd,videodata[0]);
    if (ioctl(context->dpyAttr[0].fd, FB1_IOCTL_SET_YUV_ADDR, videodata) == -1)
    {
        ALOGE("%s(%d):  fd[%d] Failed,DataAddr=%x", __FUNCTION__, __LINE__,context->dpyAttr[dpy].fd,videodata[0]);
        return -1;
    } 
    int sync = 0;
    ioctl(context->dpyAttr[0].fd, FBIOPUT_VSCREENINFO, &info);
    ioctl(context->dpyAttr[0].fd, RK_FBIOSET_CONFIG_DONE, &sync);
    context->hwc_ion.last_offset = context->hwc_ion.offset;
    if ((context->hwc_ion.offset+context->lcdSize) >= context->fbSize)
    {
        context->hwc_ion.offset = 0; 
    }
    else
    {
        context->hwc_ion.offset = context->hwc_ion.offset+context->lcdSize;
    }  
    return 0;
}

static int hwc_fbPost(hwc_composer_device_1_t * dev, size_t numDisplays, hwc_display_contents_1_t** list)
{
    return 0;
}
static int hwc_primary_Post( hwcContext * context,hwc_display_contents_1_t* list)
{
    unsigned int videodata[2];
    int ret = 0;

    if (list == NULL)
    {
       return -1;
    }    
    if (context->fbFd>0)
    {
      

        struct fb_var_screeninfo info;
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
    int sync = 1;

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
            for (int i = 0; i < list->numHwLayers-1; i++)
            {
                int owner = 1U << i;
                hwc_layer_1_t *  hwLayer = &list->hwLayers[i];
                hwc_region_t * region  = &hwLayer->visibleRegionScreen;

                /* Now go through all rectangles to split areas. */
                for (int j = 0; j < region->numRects; j++)
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

        case HWC_TOWIN0:
            {
                struct private_handle_t * handle = NULL;
                LOGV("%s(%d):Layer %d is HWC_TOWIN0", __FUNCTION__, __LINE__, i);

              
                fbhandle =  &(context->fbhandle);
                /* Get gc buffer handle. */
                handle = (struct private_handle_t *)list->hwLayers[i].handle;
                hwcONERROR(
                    hwcLayerToWin(context,
                            &list->hwLayers[i],
                            fbhandle,
                            &list->hwLayers[i].sourceCrop,
                            &list->hwLayers[i].displayFrame,
                            &list->hwLayers[i].visibleRegionScreen,
                            i,
                            0
                            ));

                if((list->numHwLayers - 1) == 1 || i == 1 
                #ifndef USE_LCDC_COMPOSER
                    || GPU_FORMAT == HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO
                #endif    
                  )
                {
                    ioctl(context->fbFd, RK_FBIOSET_CONFIG_DONE, &sync);
                    if( context->fbFd1 > 0 )
                    {
                        LOGD(" close fb1 in video");
                        //close(Context->fbFd1);
                        if(closeFb(context->fbFd1) == 0)
                        {
                            context->fbFd1 = 0;
                            context->fb1_cflag = false;
                        }
                    }
                    hwc_sync_release(list);
                    return hwcSTATUS_OK;
                }

            }

            break;

        case HWC_TOWIN1:
            {


                LOGV("%s(%d):Layer %d is HWC_TOWIN1", __FUNCTION__, __LINE__, i);
                fbhandle =  &(context->fbhandle);

                if( context->fbFd1 == 0)
                {
                    context->fbFd1 = open("/dev/graphics/fb1", O_RDWR, 0);
                    if(context->fbFd1 <= 0)
                    {
                        LOGE(" fb1 open fail,return to opengl composer");
                        hwcONERROR(hwcSTATUS_INVALID_ARGUMENT);
                    }
                    else
                    {
                        ALOGD("fb1 open!");
                    }

                }

                //gettimeofday(&tpendblit1,NULL);

                  hwcONERROR(
                    hwcLayerToWin(context,
                            &list->hwLayers[i],
                            fbhandle,
                            &list->hwLayers[i].sourceCrop,
                            &list->hwLayers[i].displayFrame,
                            &list->hwLayers[i].visibleRegionScreen,
                            i,
                            1
                            ));
                //gettimeofday(&tpendblit2,NULL);
                //usec2 = 1000*(tpendblit2.tv_sec - tpendblit1.tv_sec) + (tpendblit2.tv_usec- tpendblit1.tv_usec)/1000;
                //LOGD("hwcBlit hwcVideo2LCDC %d layers=%s use time=%ld ms",i,list->hwLayers[i].LayerName,usec2);
            }
            if((list->numHwLayers == 1) || i == 1)
            {
                sync = 1; 
                ioctl(context->fbFd, RK_FBIOSET_CONFIG_DONE, &sync);

                /*
                for (int layerid = 0; layerid < list->numHwLayers; layerid++) {
                    if (!strcmp(FPS_NAME,list->hwLayers[layerid].LayerName)) {
                        eglSwapBuffers((EGLDisplay) dpy, (EGLSurface) surf);
                    }
                }*/
 		        hwc_sync_release(list);
				
                return hwcSTATUS_OK;
            }

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
    int i;
    int priority = 0;
    for(i=0;i<pzone_mag->zone_cnt;i++)
    {
        ALOGD("Zone[%d]->layer[%d],dispatched=%d,"
        "[%d,%d,%d,%d] =>[%d,%d,%d,%d],"
        "w_h_s_f[%d,%d,%d,%d],tr_rtr_bled[%d,%d,%d],"
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
        pzone_mag->zone_info[i].LayerName);

        switch(pzone_mag->zone_info[i].dispatched) {
            case win0:
                ALOGD("[%d]dispatched=%d,priority=%d",i,pzone_mag->zone_info[i].dispatched,priority);
                priority++;
                break;
            case win1:
                ALOGD("[%d]dispatched=%d,priority=%d",i,pzone_mag->zone_info[i].dispatched,priority);
                priority++;                
                break;
            case win2_0:
                ALOGD("[%d]dispatched=%d,priority=%d",i,pzone_mag->zone_info[i].dispatched,priority);
                priority++;                
                break;
            case win2_1:
                ALOGD("[%d]dispatched=%d,priority=%d",i,pzone_mag->zone_info[i].dispatched,priority);
                break;  
            case win2_2:
                ALOGD("[%d]dispatched=%d,priority=%d",i,pzone_mag->zone_info[i].dispatched,priority);
                break;
            case win2_3:
                ALOGD("[%d]dispatched=%d,priority=%d",i,pzone_mag->zone_info[i].dispatched,priority);
                break;
            case win3_0:
                ALOGD("[%d]dispatched=%d,priority=%d",i,pzone_mag->zone_info[i].dispatched,priority);
                priority++;                
                break;
            case win3_1:
                ALOGD("[%d]dispatched=%d,priority=%d",i,pzone_mag->zone_info[i].dispatched,priority);
                break;   
            case win3_2:
                ALOGD("[%d]dispatched=%d,priority=%d",i,pzone_mag->zone_info[i].dispatched,priority);
                break;
            case win3_3:
                ALOGD("[%d]dispatched=%d,priority=%d",i,pzone_mag->zone_info[i].dispatched,priority);
                break;                 
                
            default:
                ALOGE("try_wins_dispatch  err!");
                return -1;
         }    
    }    
    return 0;
}

static int hwc_set_primary(hwc_composer_device_1 *dev, hwc_display_contents_1_t *list) 
{
    hwcContext * context = _contextAnchor;
    hwcSTATUS status = hwcSTATUS_OK;
    unsigned int i;

    struct private_handle_t * handle     = NULL;

    int needSwap = false;
    int blitflag = false;
    EGLBoolean success = EGL_FALSE;
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
OnError:

    LOGE("%s(%d):Failed!", __FUNCTION__, __LINE__);
    return HWC_EGL_ERROR;
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
    hwcSTATUS status = hwcSTATUS_OK;
	hwcContext * context = _contextAnchor;


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
                    fb_addr = context->hwc_ion.pion->phys + context->hwc_ion.last_offset;
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
    hwcContext * context = _contextAnchor;

    LOGV("%s(%d):Close hwc device in thread=%d",
         __FUNCTION__, __LINE__, gettid());

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

	if (context->ippDev)
    {
      ipp_close(context->ippDev);
	}
    if (context->hwc_ion.ion_device)
    {
      context->hwc_ion.ion_device->free(context->hwc_ion.ion_device, context->hwc_ion.pion);
      ion_close(context->hwc_ion.ion_device);
      context->hwc_ion.ion_device = NULL;
    }
#ifdef USE_LCDC_COMPOSER
    if(context->rk_ion_device)
    {
        context->rk_ion_device->free(context->rk_ion_device, context->pion);
        ion_close(context->rk_ion_device);
    }
#endif
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
    int cpu_fd = 0;
    float xdpi;
    float ydpi;
    uint32_t vsync_period; 
    unsigned int fbPhy_end;
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

    
    /* Get gco2D object pointer. */
    /*
    context->engine_fd = open("/dev/rga",O_RDWR,0);
    if( context->engine_fd < 0)
    {
        hwcONERROR(hwcRGA_OPEN_ERR);

    }
    */

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

#ifdef USE_LCDC_COMPOSER
    if(context->rk_ion_device)
    {
        context->rk_ion_device->free(context->rk_ion_device, context->pion);
        ion_close(context->rk_ion_device);
    }
#endif
    pthread_mutex_destroy(&context->lock);

    /* Error roll back. */
    if (context != NULL)
    {
        if (context->engine_fd != NULL)
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
