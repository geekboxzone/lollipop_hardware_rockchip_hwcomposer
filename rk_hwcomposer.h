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




#ifndef __gc_hwcomposer_h_
#define __gc_hwcomposer_h_

/* Set 0 to enable LOGV message. See cutils/log.h */
#include <cutils/log.h>
#include <hardware/hwcomposer.h>
//#include <ui/android_native_buffer.h>

#include <hardware/rga.h>
#include <utils/Thread.h>
#include <linux/fb.h>
#include <hardware/gralloc.h>
#include "../libgralloc_ump/gralloc_priv.h"
#include "../libon2/vpu_global.h"
#include "../libon2/vpu_mem.h"

//Control macro
#define hwcDEBUG                    0
#define hwcUseTime                  0
#define hwcBlitUseTime              0
#define hwcDumpSurface              0
#define DUMP_AFTER_RGA_COPY_IN_GPU_CASE 0
#define DEBUG_CHECK_WIN_CFG_DATA    0     //check rk_fb_win_cfg_data for lcdc
#define ENABLE_HWC_WORMHOLE         1
#define DUMP_SPLIT_AREA             0
#define SYNC_IN_VIDEO               0
#define USE_HWC_FENCE               1
#define USE_QUEUE_DDRFREQ           1
#define USE_VIDEO_BACK_BUFFERS      1
#define USE_HW_VSYNC                1
#define WRITE_VPU_FRAME_DATA        0
#define MOST_WIN_ZONES              4
#define ENBALE_WIN_ANY_ZONES        0
#define ENABLE_TRANSFORM_BY_RGA     0
#define OPTIMIZATION_FOR_TRANSFORM_UI   1

//Command macro
#define FB1_IOCTL_SET_YUV_ADDR	    0x5002
#define RK_FBIOSET_VSYNC_ENABLE     0x4629
#define RK_FBIOSET_DMABUF_FD	    0x5004
#define RK_FBIOGET_DSP_FD     	    0x4630
#define RK_FBIOGET_LIST_STAT   		0X4631
//#define USE_LCDC_COMPOSER
#define FBIOSET_OVERLAY_STATE     	0x5018

//Amount macro
#define MaxZones                    10
#define bakupbufsize                4
#define MaxVideoBackBuffers         (3)
#define MAX_VIDEO_SOURCE            (5)
#define GPUDRAWCNT                  (10)

//Other macro
#define GPU_BASE        handle->base
#define GPU_WIDTH       handle->width
#define GPU_HEIGHT      handle->height
#define GPU_FORMAT      handle->format
#define GPU_DST_FORMAT  DstHandle->format

#define GHWC_VERSION  "2.002"
//HWC version Tag
//Get commit info:  git log --format="Author: %an%nTime:%cd%nCommit:%h%n%n%s%n%n"
//Get version: busybox strings /system/lib/hw/hwcomposer.rk30board.so | busybox grep HWC_VERSION
//HWC_VERSION Author:zxl Time:Tue Aug 12 17:27:36 2014 +0800 Version:1.17 Branch&Previous-Commit:rk/rk312x/mid/4.4_r1/develop-9533348.
#define HWC_VERSION "HWC_VERSION  \
Author:zxl \
Previous-Time:Fri Aug 15 14:09:02 2014 +0800 \
Version:2.002 \
Branch&Previous-Commit:rk/rk32/mid/4.4_r1/develop-f00a2d3."

/* Set it to 1 to enable swap rectangle optimization;
 * Set it to 0 to disable. */
/* Set it to 1 to enable pmem cache flush.
 * For linux kernel 3.0 later, you may not be able to flush PMEM cache in a
 * different process (surfaceflinger). Please add PMEM cache flush in gralloc
 * 'unlock' function, which will be called in the same process SW buffers are
 * written/read by software (Skia) */

#ifdef __cplusplus
extern "C" {
#endif


#if PLATFORM_SDK_VERSION >= 17

#define  hwc_layer_list_t	 	hwc_display_contents_1_t
#endif
enum
{
    /* NOTE: These enums are unknown to Android.
     * Android only checks against HWC_FRAMEBUFFER.
     * This layer is to be drawn into the framebuffer by hwc blitter */
    //HWC_TOWIN0 = 0x10,
    //HWC_TOWIN1,
    HWC_BLITTER = 100,
    HWC_DIM,
    HWC_CLEAR_HOLE
    
};



typedef struct _mix_info
{
    int gpu_draw_fd[GPUDRAWCNT];
    int alpha[GPUDRAWCNT];

}
mix_info;

typedef enum _hwcSTATUS
{
	hwcSTATUS_OK					= 	 0,
	hwcSTATUS_INVALID_ARGUMENT      =   -1,
	hwcSTATUS_IO_ERR 			    = 	-2,
	hwcRGA_OPEN_ERR                 =   -3,
	hwcTHREAD_ERR                   =   -4,
	hwcMutex_ERR                    =   -5,

}
hwcSTATUS;

typedef struct _hwcRECT
{
    int                    left;
    int                    top;
    int                    right;
    int                    bottom;
}
hwcRECT;

typedef enum _hwc_lcdc_res
{
	win0				= 1,
	win1                = 2,
	win2_0              = 3,   
	win2_1              = 4,
	win2_2              = 5,
	win2_3              = 6,   
	win3_0              = 7,
	win3_1              = 8,
	win3_2              = 9,
	win3_3              = 10,
	win_ext             = 11,

}
hwc_lcdc_res;

typedef struct _ZoneInfo
{
    unsigned int        stride;
    unsigned int        width;
    unsigned int        height;
    hwc_rect_t  src_rect;
    hwc_rect_t  disp_rect;
    struct private_handle_t *handle;      
	int         layer_fd;
	int         direct_fd;
	unsigned int addr;
	int         zone_alpha;
	int         blend;
	bool        is_stretch;
	int         is_large;
	int         size;
	float       hfactor;
	int         format;
	int         zone_index;
	int         layer_index;
	int         transform;
	int         realtransform;
	int         layer_flag;
	int         dispatched;
	int         sort;
	char        LayerName[LayerNameLength + 1];   
#ifdef USE_HWC_FENCE
    int         acq_fence_fd;
#endif
}
ZoneInfo;

typedef struct _ZoneManager
{
    ZoneInfo    zone_info[MaxZones];
    int         bp_size;
    int         zone_cnt;   
    int         composter_mode;        
	        
}
ZoneManager;
typedef struct _vop_info
{
    int         state;   // 1:on ,0:off
	int         zone_num;  // nums    
	int         reserve;
	int         reserve2;	
}
vop_info;
typedef struct _BpVopInfo
{
    vop_info    vopinfo[4];
    int         bp_size; // toatl size
    int         bp_vop_size;    // vop size
}
BpVopInfo;

typedef struct _hwbkupinfo
{
    buffer_handle_t phd_bk;
    int membk_fd;
    int buf_fd;
    unsigned int pmem_bk;
    unsigned int buf_addr;
    void* pmem_bk_log;
    void* buf_addr_log;
    int xoffset;
    int yoffset;
    int w_vir;
    int h_vir;
    int w_act;
    int h_act;
    int format;
}
hwbkupinfo;
typedef struct _hwbkupmanage
{
    int count;
    buffer_handle_t phd_drt;    
    int          direct_fd;         
    unsigned int direct_addr;
    void* direct_addr_log;    
    int invalid;
    int needrev;
    int dstwinNo;
    int skipcnt;
    unsigned int ckpstcnt;    
    unsigned int inputspcnt;    
	char LayerName[LayerNameLength + 1];    
    unsigned int crrent_dis_fd;
    hwbkupinfo bkupinfo[bakupbufsize];
    struct private_handle_t *handle_bk;
}
hwbkupmanage;
#define IN
#define OUT

/* Area struct. */
struct hwcArea
{
    /* Area potisition. */
    hwcRECT                          rect;

    /* Bit field, layers who own this Area. */
    int                        owners;

    /* Point to next area. */
    struct hwcArea *                 next;
};


/* Area pool struct. */
struct hwcAreaPool
{
    /* Pre-allocated areas. */
    hwcArea *                        areas;

    /* Point to free area. */
    hwcArea *                        freeNodes;

    /* Next area pool. */
    hwcAreaPool *                    next;
};

struct DisplayAttributes {
    uint32_t vsync_period; //nanos
    uint32_t xres;
    uint32_t yres;
    uint32_t stride;
    float xdpi;
    float ydpi;
    int fd;
	int fd1;
	int fd2;
	int fd3;
    bool connected; //Applies only to pluggable disp.
    //Connected does not mean it ready to use.
    //It should be active also. (UNBLANKED)
    bool isActive;
    // In pause state, composition is bypassed
    // used for WFD displays only
    bool isPause;
};

struct tVPU_FRAME_v2
{
    uint32_t         videoAddr[2];    // 0: Y address; 1: UV address;
    uint32_t         width;         // 16 aligned frame width
    uint32_t         height;        // 16 aligned frame height
    uint32_t         format;        // 16 aligned frame height
};



typedef struct 
{
   tVPU_FRAME_v2 vpu_frame;
   void*      vpu_handle;
} vpu_frame_t;

typedef struct _videoCacheInfo
{
    struct private_handle_t* video_hd;
    struct private_handle_t* vui_hd;
    void * video_base;
    bool bMatch;
}videoCacheInfo;

typedef struct _hwcContext
{
    hwc_composer_device_1_t device;

    /* Reference count. Normally: 1. */
    unsigned int reference;

    /* GC state goes below here */

    /* Raster engine */
    int   engine_fd;
    /* Feature: 2D PE 2.0. */
    /* Base address. */
    unsigned int baseAddress;

    /* Framebuffer stuff. */
    int       fbFd;
    int       fbFd1;
    int       vsync_fd;
    int       ddrFd;
    videoCacheInfo video_info[MAX_VIDEO_SOURCE];
    int vui_fd;
    int vui_hide;

    int video_fmt;
    struct private_handle_t fbhandle ;    
    bool      fb1_cflag;
    char      cupcore_string[16];
    DisplayAttributes              dpyAttr[HWC_NUM_DISPLAY_TYPES];
    struct                         fb_var_screeninfo info;

    hwc_procs_t *procs;
    pthread_t hdmi_thread;
    pthread_mutex_t lock;
    nsecs_t         mNextFakeVSync;
    float           fb_fps;
    unsigned int fbPhysical;
    unsigned int fbStride;
	int          wfdOptimize;
    /* PMEM stuff. */
    unsigned int pmemPhysical;
    unsigned int pmemLength;
	vpu_frame_t  video_frame;
	unsigned int fbSize;
	unsigned int lcdSize;
    alloc_device_t  *mAllocDev;	
	ZoneManager  zone_manager;;

#if ENABLE_HWC_WORMHOLE
    /* Splited composition area queue. */
    hwcArea *                        compositionArea;

    /* Pre-allocated area pool. */
    hwcAreaPool                      areaPool;
#endif
    /* skip flag */
     int      mSkipFlag;
     int      flag;
     int      fb_blanked;

    /* video flag */
     bool      mVideoMode;
     bool      mIsMediaView;
     bool      mVideoRotate;
     int        mtrsformcnt;       

#if USE_VIDEO_BACK_BUFFERS
     /* The index of video buffer will be used */
     int      mCurVideoIndex;
     int      fd_video_bk[MaxVideoBackBuffers];
     buffer_handle_t pbvideo_bk[MaxVideoBackBuffers];
#endif
}
hwcContext;
#define gcmALIGN(n, align) \
( \
    ((n) + ((align) - 1)) & ~((align) - 1) \
)

#define hwcMIN(x, y)			(((x) <= (y)) ?  (x) :  (y))
#define hwcMAX(x, y)			(((x) >= (y)) ?  (x) :  (y))

#define hwcIS_ERROR(status)			(status < 0)


#define _hwcONERROR(prefix, func) \
    do \
    { \
        status = func; \
        if (hwcIS_ERROR(status)) \
        { \
            LOGD( "ONERROR: status=%d @ %s(%d) in ", \
                status, __FUNCTION__, __LINE__); \
            goto OnError; \
        } \
    } \
    while (false)
#define hwcONERROR(func)            _hwcONERROR(hwc, func)

#ifdef  ALOGD
#define LOGV        ALOGV
#define LOGE        ALOGE
#define LOGD        ALOGD
#define LOGI        ALOGI
#endif
/******************************************************************************\
 ********************************* Blitters ***********************************
\******************************************************************************/

/* 2D blit. */
hwcSTATUS
hwcBlit(
    IN hwcContext * Context,
    IN hwc_layer_1_t * Src,
    IN struct private_handle_t * DstHandle,
    IN hwc_rect_t * SrcRect,
    IN hwc_rect_t * DstRect,
    IN hwc_region_t * Region
    );


hwcSTATUS
hwcDim(
    IN hwcContext * Context,
    IN hwc_layer_1_t * Src,
    IN struct private_handle_t * DstHandle,
    IN hwc_rect_t * DstRect,
    IN hwc_region_t * Region
    );

hwcSTATUS
hwcLayerToWin(
    IN hwcContext * Context,
    IN hwc_layer_1_t * Src,
    IN struct private_handle_t * DstHandle,
    IN hwc_rect_t * SrcRect,
	IN hwc_rect_t * DstRect,
    IN hwc_region_t * Region,
    IN int Index,
    IN int Win
    );
hwcSTATUS
hwcClear(
    IN hwcContext * Context,
    IN unsigned int Color,
    IN struct private_handle_t * DstHandle,
    IN hwc_rect_t * DstRect,
    IN hwc_region_t * Region
    );


/******************************************************************************\
 ************************** Native buffer handling ****************************
\******************************************************************************/

hwcSTATUS
hwcGetFormat(
    IN  struct private_handle_t * Handle,
    OUT RgaSURF_FORMAT * Format
    );

int hwChangeRgaFormat(IN int fmt );
hwcSTATUS
hwcLockBuffer(
    IN  hwcContext *  Context,
    IN  struct private_handle_t * Handle,
    OUT void * *  Logical,
    OUT unsigned int* Physical,
    OUT unsigned int* Width,
    OUT unsigned int* Height,
    OUT unsigned int* Stride,
    OUT void * *  Info
    );


hwcSTATUS
hwcUnlockBuffer(
    IN hwcContext * Context,
    IN struct private_handle_t * Handle,
    IN void * Logical,
    IN void * Info,
    IN unsigned int  Physical
    );

int
_HasAlpha(RgaSURF_FORMAT Format);

int closeFb(int fd);
int  getHdmiMode();
void init_hdmi_mode();
/******************************************************************************\
 ****************************** Rectangle split *******************************
\******************************************************************************/
/* Split rectangles. */
bool
hwcSplit(
    IN  hwcRECT * Source,
    IN  hwcRECT * Dest,
    OUT hwcRECT * Intersection,
    OUT hwcRECT * Rects,
    OUT int  * Count
    );

hwcSTATUS
WormHole(
    IN hwcContext * Context,
    IN hwcRECT * Rect
    );

void
_FreeArea(
    IN hwcContext * Context,
    IN hwcArea* Head
    );

void
_SplitArea(
    IN hwcContext * Context,
    IN hwcArea * Area,
    IN hwcRECT * Rect,
    IN int Owner
    );

hwcArea *
_AllocateArea(
    IN hwcContext * Context,
    IN hwcArea * Slibing,
    IN hwcRECT * Rect,
    IN int Owner
    );



extern "C" int clock_nanosleep(clockid_t clock_id, int flags,
                           const struct timespec *request,
                           struct timespec *remain);

#ifdef __cplusplus
}
#endif

#endif /* __gc_hwcomposer_h_ */

