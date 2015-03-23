/*

* rockchip hwcomposer( 2D graphic acceleration unit) .

*

* Copyright (C) 2015 Rockchip Electronics Co., Ltd.

*/



#ifndef __rk_hwcomposer_h_
#define __rk_hwcomposer_h_

/* Set 0 to enable LOGV message. See cutils/log.h */
#include <cutils/log.h>
#include <hardware/hwcomposer.h>
//#include <ui/android_native_buffer.h>

#include <hardware/rga.h>
#include <utils/Thread.h>
#include <linux/fb.h>
#include <hardware/gralloc.h>
#ifdef GPU_G6110
#include  <hardware/img_gralloc_public.h>
#else
#include <gralloc_priv.h>
#endif
#include <vpu_global.h>
#include <vpu_mem.h>

//Control macro
#define hwcDEBUG                    0
#define hwcUseTime                  0
#define hwcBlitUseTime              0
#define hwcDumpSurface              0
#define DUMP_AFTER_RGA_COPY_IN_GPU_CASE 0
#define DEBUG_CHECK_WIN_CFG_DATA    0     //check rk_fb_win_cfg_data for lcdc
#define DUMP_SPLIT_AREA             0
#define SYNC_IN_VIDEO               0
#define USE_HWC_FENCE               1
//#define USE_QUEUE_DDRFREQ           0
#define USE_VIDEO_BACK_BUFFERS      1
#define USE_SPECIAL_COMPOSER        0
#define ENABLE_LCDC_IN_NV12_TRANSFORM   1   //1: It will need reserve a phyical memory for transform.
#define USE_HW_VSYNC                1
#define WRITE_VPU_FRAME_DATA        0
#define MOST_WIN_ZONES              4
#define ENBALE_WIN_ANY_ZONES        0
#define ENABLE_TRANSFORM_BY_RGA     1               //1: It will need reserve a phyical memory for transform.
#define OPTIMIZATION_FOR_TRANSFORM_UI   1
#define OPTIMIZATION_FOR_DIMLAYER       1           //1: optimise for dim layer
#define HWC_EXTERNAL                    1           //1:hwc control two lcdc for display
#define USE_QUEUE_DDRFREQ               0

#ifdef GPU_G6110
#define G6110_SUPPORT_FBDC              0
#endif

//Command macro
#define FB1_IOCTL_SET_YUV_ADDR	    0x5002
#define RK_FBIOSET_VSYNC_ENABLE     0x4629
#define RK_FBIOSET_DMABUF_FD	    0x5004
#define RK_FBIOGET_DSP_FD     	    0x4630
#define RK_FBIOGET_LIST_STAT   		0X4631
//#define USE_LCDC_COMPOSER
#define FBIOSET_OVERLAY_STATE     	0x5018
#define RK_FBIOGET_IOMMU_STA        0x4632

//Amount macro
#define MaxZones                    10
#define bakupbufsize                4
#define MaxVideoBackBuffers         (3)
#define MAX_VIDEO_SOURCE            (5)
#define GPUDRAWCNT                  (10)

//Other macro
#define GPU_WIDTH       handle->width
#define GPU_HEIGHT      handle->height
#define GPU_FORMAT      handle->format
#define GPU_DST_FORMAT  DstHandle->format


#define GHWC_VERSION  "2.022"
//HWC version Tag
//Get commit info:  git log --format="Author: %an%nTime:%cd%nCommit:%h%n%n%s%n%n"
//Get version: busybox strings /system/lib/hw/hwcomposer.rk30board.so | busybox grep HWC_VERSION
//HWC_VERSION Author:zxl Time:Tue Aug 12 17:27:36 2014 +0800 Version:1.17 Branch&Previous-Commit:rk/rk312x/mid/4.4_r1/develop-9533348.
#define HWC_VERSION "HWC_VERSION  \
Author: wzq \
Version:2.022 \
"

#ifdef GPU_G6110
#if G6110_SUPPORT_FBDC
#define FBDC_BGRA_8888 0x125 //HALPixelFormatSetCompression(HAL_PIXEL_FORMAT_BGRA_8888,HAL_FB_COMPRESSION_DIRECT_16x4)
#define FBDC_RGBA_8888 0x121 //HALPixelFormatSetCompression(HAL_PIXEL_FORMAT_RGBA_8888,HAL_FB_COMPRESSION_DIRECT_16x4)


//lcdc support fbdc format
enum data_format {
FBDC_RGB_565 = 0x26,
FBDC_ARGB_888,
FBDC_RGBX_888,
FBDC_ABGR_888
};

//G6110 support fbdc compression methods
#define HAL_FB_COMPRESSION_NONE                0
#define HAL_FB_COMPRESSION_DIRECT_8x8          1
#define HAL_FB_COMPRESSION_DIRECT_16x4         2
#define HAL_FB_COMPRESSION_DIRECT_32x2         3
#define HAL_FB_COMPRESSION_INDIRECT_8x8        4
#define HAL_FB_COMPRESSION_INDIRECT_16x4       5
#define HAL_FB_COMPRESSION_INDIRECT_4TILE_8x8  6
#define HAL_FB_COMPRESSION_INDIRECT_4TILE_16x4 7
#endif //end of G6110_SUPPORT_FBDC

#define HAL_PIXEL_FORMAT_BAD		 0xff

/* Use bits [0-3] of "vendor format" bits as real format. Customers should
 * use *only* the unassigned bits below for custom pixel formats, YUV or RGB.
 *
 * If there are no bits set in this part of the field, or other bits are set
 * in the format outside of the "vendor format" mask, the non-extension format
 * is used instead. Reserve 0 for this purpose.
 */

#define HAL_PIXEL_FORMAT_VENDOR_EXT(fmt) (0x100 | (fmt & 0xF))

/*      Reserved ** DO NOT USE **    HAL_PIXEL_FORMAT_VENDOR_EXT(0) */
#define HAL_PIXEL_FORMAT_BGRX_8888   HAL_PIXEL_FORMAT_VENDOR_EXT(1)
#define HAL_PIXEL_FORMAT_sBGR_A_8888 HAL_PIXEL_FORMAT_VENDOR_EXT(2)
#define HAL_PIXEL_FORMAT_sBGR_X_8888 HAL_PIXEL_FORMAT_VENDOR_EXT(3)
/*      HAL_PIXEL_FORMAT_RGB_565     HAL_PIXEL_FORMAT_VENDOR_EXT(4) */
/*      HAL_PIXEL_FORMAT_BGRA_8888   HAL_PIXEL_FORMAT_VENDOR_EXT(5) */
#define HAL_PIXEL_FORMAT_NV12        HAL_PIXEL_FORMAT_VENDOR_EXT(6)
/*      Free for customer use        HAL_PIXEL_FORMAT_VENDOR_EXT(7) */
/*      Free for customer use        HAL_PIXEL_FORMAT_VENDOR_EXT(8) */
/*      Free for customer use        HAL_PIXEL_FORMAT_VENDOR_EXT(9) */
/*      Free for customer use        HAL_PIXEL_FORMAT_VENDOR_EXT(10) */
/*      Free for customer use        HAL_PIXEL_FORMAT_VENDOR_EXT(11) */
/*      Free for customer use        HAL_PIXEL_FORMAT_VENDOR_EXT(12) */
/*      Free for customer use        HAL_PIXEL_FORMAT_VENDOR_EXT(13) */
/*      Free for customer use        HAL_PIXEL_FORMAT_VENDOR_EXT(14) */
/*      Free for customer use        HAL_PIXEL_FORMAT_VENDOR_EXT(15) */

#endif  //end of GPU_G6110

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

#ifdef GPU_G6110
#define private_handle_t    tagIMG_native_handle_t
#define GPU_BASE            handle->pvBase
#else
#define GPU_BASE            handle->base
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
    int          direct_base;
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
	int           iommuEn;
    alloc_device_t  *mAllocDev;	
	ZoneManager  zone_manager;;

    /* skip flag */
     int      mSkipFlag;
     int      flag;
     int      fb_blanked;

    /* video flag */
     bool      mVideoMode;
     bool      mNV12_VIDEO_VideoMode;
     bool      mIsMediaView;
     bool      mVideoRotate;
     bool      mGtsStatus;
     bool      mTrsfrmbyrga;
     int        mtrsformcnt;

     /*dual display */
     int hdmi_anm;
     int flag_blank;
     int flag_external;
     int flag_hwcup_external;
     int last_fenceFd_flag;
     int last_rel_fenceFd[11];
     int last_ret_fenceFd;
     int last_frame_flag;
     bool mix_vh;
     bool NeedReDst;

     /* The index of video buffer will be used */
     int      mCurVideoIndex;
     int      fd_video_bk[MaxVideoBackBuffers];
#if defined(__arm64__) || defined(__aarch64__)
     long     base_video_bk[MaxVideoBackBuffers];
#else
     int      base_video_bk[MaxVideoBackBuffers];
#endif
     buffer_handle_t pbvideo_bk[MaxVideoBackBuffers];

#if OPTIMIZATION_FOR_DIMLAYER
     bool     bHasDimLayer;
     int      mDimFd;
#if defined(__arm64__) || defined(__aarch64__)
     long      mDimBase;
#else
     int       mDimBase;
#endif
     buffer_handle_t      mDimHandle;
#endif
}
hwcContext;
#define rkmALIGN(n, align) \
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


/******************************************************************************\
 ************************** Native buffer handling ****************************
\******************************************************************************/

hwcSTATUS
hwcGetFormat(
    IN  struct private_handle_t * Handle,
    OUT RgaSURF_FORMAT * Format
    );

int hwChangeRgaFormat(IN int fmt );

#if defined(__arm64__) || defined(__aarch64__)


hwcSTATUS
hwcUnlockBuffer(
    IN hwcContext * Context,
    IN struct private_handle_t * Handle,
    IN void * Logical,
    IN void * Info,
    IN unsigned long  Physical
    );

#else



hwcSTATUS
hwcUnlockBuffer(
    IN hwcContext * Context,
    IN struct private_handle_t * Handle,
    IN void * Logical,
    IN void * Info,
    IN unsigned int  Physical
    );

#endif

int
_HasAlpha(RgaSURF_FORMAT Format);

int closeFb(int fd);
int  getHdmiMode();
void init_hdmi_mode();







extern "C" int clock_nanosleep(clockid_t clock_id, int flags,
                           const struct timespec *request,
                           struct timespec *remain);

#ifdef __cplusplus
}
#endif

#endif 

