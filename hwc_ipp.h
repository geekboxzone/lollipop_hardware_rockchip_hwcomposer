/****************************************************************
* FILE: hwc_ipp.h                       
* FUNCTION:  
*  In order to meet the needs of power optimization when playing
*  the video
*  
* AUTHOR:  qiuen     2013/8/20
 ***************************************************************/

#ifndef __rk_hwc_ipp
#define __rk_hwc_ipp

#include <hardware/hwcomposer.h>

#include <hardware/hardware.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include "rk29-ipp.h"
#include <cutils/log.h>
#ifdef TARGET_BOARD_PLATFORM_RK30XXB
 #include  <hardware/hal_public.h>
#else
 #include "../libgralloc_ump/gralloc_priv.h"
#endif

#ifdef TARGET_BOARD_PLATFORM_RK30XXB
 struct private_handle_t;
 #define private_handle_t IMG_native_handle_t
 #define format iFormat
 #define width iWidth
 #define height iHeight
#endif

extern "C" {
  #include <ion/ionalloc.h>
}



typedef struct ipp_device_t 
{
    int (*ipp_is_enable)(); //ipp if available
    int (*ipp_reset)();//ipp reset
    int (*ipp_format_is_surport)(int format); //ipp is surport
	int (*ipp_rotate_and_scale)(struct private_handle_t *handle,\
								int tranform,\
								unsigned int* srcPhysical);//ipp rotate or scale
	void *reserved;//
} ipp_device_t;

int    ipp_open(ipp_device_t *ippDev);//open ipp
int    ipp_close(ipp_device_t *ippDev);//close ipp

#endif

