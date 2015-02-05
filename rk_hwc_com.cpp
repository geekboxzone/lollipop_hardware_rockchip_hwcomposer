/*

* rockchip hwcomposer( 2D graphic acceleration unit) .

*

* Copyright (C) 2015 Rockchip Electronics Co., Ltd.

*/




#include "rk_hwcomposer.h"
#include <linux/fb.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
//#include <linux/android_pmem.h>
#include <ui/PixelFormat.h>
#include <fcntl.h>




/*******************************************************************************
**
**  YUV pixel formats of android hal.
**
**  Different android versions have different definitaions.
**  These are collected from hardware/libhardware/include/hardware/hardware.h
*/


hwcSTATUS
hwcGetFormat(
    IN  struct private_handle_t * Handle,
    OUT RgaSURF_FORMAT * Format
    
    )
{
    struct private_handle_t *handle = Handle;
    if (Format != NULL)
    {
    	
        switch (GPU_FORMAT)
        {
        case HAL_PIXEL_FORMAT_RGB_565:
            *Format = RK_FORMAT_RGB_565;
            break;
        case HAL_PIXEL_FORMAT_RGB_888:
            *Format = RK_FORMAT_RGB_888;
            break;
        case HAL_PIXEL_FORMAT_RGBA_8888:
            *Format = RK_FORMAT_RGBA_8888;
            break;

        case HAL_PIXEL_FORMAT_RGBX_8888:
            *Format = RK_FORMAT_RGBX_8888;
            break;


        case HAL_PIXEL_FORMAT_BGRA_8888:
            *Format = RK_FORMAT_BGRA_8888;
            break;

        case HAL_PIXEL_FORMAT_YCrCb_NV12:
            /* YUV 420 semi planner: NV12 */
            *Format = RK_FORMAT_YCbCr_420_SP;
            break;
		case HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO:
		   *Format = RK_FORMAT_YCbCr_420_SP;
			 break; 
        default:
            return hwcSTATUS_INVALID_ARGUMENT;
        }
    }


    return hwcSTATUS_OK;
}

int hwChangeRgaFormat(IN int fmt )
{
    switch (fmt)
    {
    case HAL_PIXEL_FORMAT_RGB_565:
        return RK_FORMAT_RGB_565;
    case HAL_PIXEL_FORMAT_RGB_888:
        return RK_FORMAT_RGB_888;
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return RK_FORMAT_RGBA_8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
        return RK_FORMAT_RGBX_8888;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return RK_FORMAT_BGRA_8888;
    case HAL_PIXEL_FORMAT_YCrCb_NV12:
        return RK_FORMAT_YCbCr_420_SP;
	case HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO:
	   return RK_FORMAT_YCbCr_420_SP;
    default:
        return hwcSTATUS_INVALID_ARGUMENT;
    }
}


