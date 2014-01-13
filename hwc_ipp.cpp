//create by qiuen 2013/7/5
#include "hwc_ipp.h"


#define ION_SIZE 1024*1024*6

#define ALIGN(n, align) \
( \
    ((n) + ((align) - 1)) & ~((align) - 1) \
)

struct tVPU_FRAME
{
    uint32_t         videoAddr[2];    // 0: Y address; 1: UV address;
    uint32_t         width;         // 16 aligned frame width
    uint32_t         height;        // 16 aligned frame height
};

typedef struct ipp_buffer_t
{
  int ipp_fd;
  ion_buffer_t *pion;
  ion_device_t *ion_device;
  uint32_t  offset;
  uint32_t  Yrgb;
  uint32_t  CbrMst;
} ipp_buffer_t;

static ipp_buffer_t ipp_buffer;

static IPP_FORMAT  ipp_get_format(int format)
{
   ALOGV("Enter %s",__FUNCTION__);
   IPP_FORMAT ipp_format = IPP_IMGTYPE_LIMIT;
   switch (format)
   {

#if 0	//only surport video,because can get phs addr for video
	case HAL_PIXEL_FORMAT_YCbCr_422_SP:
	case HAL_PIXEL_FORMAT_YCbCr_422_I: 
		ipp_format = IPP_Y_CBCR_H2V1;
		break;
	case HAL_PIXEL_FORMAT_RGBA_8888:   
	case HAL_PIXEL_FORMAT_RGBX_8888:
	case HAL_PIXEL_FORMAT_BGRA_8888: 
		ipp_format = IPP_XRGB_8888;
		break;
	
	case HAL_PIXEL_FORMAT_RGB_565:
		ipp_format = IPP_RGB_565;
        break;
	case HAL_PIXEL_FORMAT_YV12:
	case HAL_PIXEL_FORMAT_YCrCb_420_SP:      
	case HAL_PIXEL_FORMAT_YCbCr_NV12:    
#endif
	case HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO: 
		ipp_format = IPP_Y_CBCR_H2V2;
		break;
	
	default:
		ipp_format = IPP_IMGTYPE_LIMIT;
   }

   return ipp_format;
}

static ROT_DEG ipp_get_rot(int rot)
{
    ALOGV("Enter %s",__FUNCTION__);
    ROT_DEG ipp_rot = IPP_ROT_LIMIT;
	switch (rot)
	{
	    case HAL_TRANSFORM_FLIP_H: 
			ipp_rot = IPP_ROT_Y_FLIP;
			break;
			
	    case HAL_TRANSFORM_FLIP_V:  
			ipp_rot = IPP_ROT_X_FLIP;
			break;
			
	    case HAL_TRANSFORM_ROT_90: 
			ipp_rot = IPP_ROT_90;
			break;
			
	    case HAL_TRANSFORM_ROT_180: 
			ipp_rot = IPP_ROT_180;
			break;
			
	    case HAL_TRANSFORM_ROT_270:
			ipp_rot = IPP_ROT_270;
			break;
		default:
			ipp_rot = IPP_ROT_LIMIT;
	}
	return ipp_rot;
}

static int ipp_x_aligned(int w)
{
    return ALIGN(w, 8);
}

static void dump(void* vaddr, int width, int height)
{
   static int DumpSurfaceCount1 = 0;

   FILE * pfile = NULL;
   char layername[100] ;
   memset(layername,0,sizeof(layername));  
   system("mkdir /data/dump/ && chmod /data/dump/ 777 ");
   //mkdir( "/data/dump/",777);    
   sprintf(layername,"/data/dump/src_fb_dmlayer_%d_%d_%d.bin",width,height,DumpSurfaceCount1);         
   pfile = fopen(layername,"wb");
   if(pfile)
   {                                       
       fwrite((const void *)vaddr,(size_t)(width*width*3)/2,1,pfile);
       fclose(pfile);
   }
   DumpSurfaceCount1++;

}

static int  ipp_format_is_supported(int format)
{
    ALOGV("Enter %s",__FUNCTION__);
    IPP_FORMAT ipp_format = ipp_get_format(format);
    return (int)ipp_format;
}

static int ipp_set_req(struct private_handle_t *handle, int tranform, struct rk29_ipp_req *req)
{
#ifdef TARGET_BOARD_PLATFORM_RK30XXB
    tVPU_FRAME *pVideo = (tVPU_FRAME *)handle->iBase;
#else
    tVPU_FRAME *pVideo = (tVPU_FRAME *)handle->base;
#endif
	int width = pVideo->width;
	int height = pVideo->height;
	int format = GPU_FORMAT; 
	int rot = tranform;
	int ipp_format = ipp_get_format(format);
	int ipp_rot = ipp_get_rot(rot);
	uint32_t videoLen = (width*height)*3/2;
	uint32_t ipp_phys_addr_base = ipp_buffer.pion->phys+ipp_buffer.offset;
	if (ipp_phys_addr_base+videoLen >= (ipp_buffer.pion->phys+ION_SIZE))
	{
          ipp_buffer.offset = 0;
	  ipp_phys_addr_base = ipp_buffer.pion->phys+ipp_buffer.offset;
	}
	req->src0.YrgbMst = pVideo->videoAddr[0]; //
	req->src0.CbrMst = pVideo->videoAddr[1];//cbcr phys	
	req->src0.w = width;	
	req->src0.h = height;	
	req->src0.fmt = ipp_format;//420	
    ALOGV(">>IPP:addr(%x,%x),w=%d,h=%d,format=%d,ipp_rot=%d,tranform=%d,src(%d,%d)",req->src0.YrgbMst,req->src0.CbrMst,req->src0.w,req->src0.h,req->src0.fmt,ipp_rot,tranform,pVideo->width,pVideo->height);
	
	req->dst0.YrgbMst = ipp_phys_addr_base;
	req->dst0.CbrMst = req->dst0.YrgbMst+width*height;	
	if (ipp_rot==IPP_ROT_90 || ipp_rot==IPP_ROT_270)
        {
		req->dst0.w = height;//ipp_x_aligned(pVideo->width);//2¶ÔÆë	
		req->dst0.h = width;//swith	
	}
        else
        {
           req->dst0.w = width;
           req->dst0.h = height;
        }
	req->store_clip_mode = 1;//¶ÔÆë	
	req->src_vir_w = pVideo->width; 	
	req->dst_vir_w = ipp_x_aligned(req->dst0.w);	
	req->timeout = 50;//ms	
	req->flag = ipp_rot;//
	ipp_buffer.Yrgb = req->dst0.YrgbMst;
	ipp_buffer.CbrMst = req->dst0.CbrMst;
	
	ipp_buffer.offset += videoLen;
		
	return 0;
}

static int  get_ipp_is_enable()
{
    return ipp_buffer.ipp_fd;
}

static int ipp_reset()
{
    ipp_buffer.offset = 0;
	return 0;
}

static int  ipp_rotate_and_scale(struct private_handle_t *handle,\
							int tranform,unsigned int* srcPhysical, int *videoWidth, int *videoHeight)
{
   if (handle == NULL)
   {
     ALOGE("handle is NULL.");
	 return -1;
   }
   struct rk29_ipp_req ipp_req;
   int err = ipp_set_req(handle, tranform, &ipp_req);
   if (err == 0)
   {
          int ret = ioctl(ipp_buffer.ipp_fd, IPP_BLIT_SYNC, &ipp_req);
	  if (ret != 0)
	  {
	     ALOGE("ipp IPP_BLIT_SYNC error,ret=%d.",ret);
             return -1;
	  }
	  srcPhysical[0] = ipp_buffer.Yrgb;
	  srcPhysical[1] = ipp_buffer.CbrMst;
          *videoWidth = ipp_req.dst_vir_w;
          *videoHeight = ipp_req.dst0.h;
	  return 0;
   }
   else
   {
      ALOGE("hwc layer to ipp-req fail.");
	  return -1;
   }
}

 
int  ipp_open(ipp_device_t *ippDev)
{
	int ipp_fd = open("/dev/rk29-ipp", O_RDWR, 0);
	if (ipp_fd > 0)
	{
		ipp_buffer.ipp_fd = ipp_fd;
		ipp_buffer.offset = 0;
		ion_open(ION_SIZE, ION_MODULE_UI, &ipp_buffer.ion_device);
		int err = ipp_buffer.ion_device->alloc(ipp_buffer.ion_device, ION_SIZE ,_ION_HEAP_RESERVE, &ipp_buffer.pion);
		if (!err)
		{
		 ippDev->ipp_format_is_surport = &ipp_format_is_supported;
		 ippDev->ipp_is_enable = &get_ipp_is_enable;
		 ippDev->ipp_rotate_and_scale = &ipp_rotate_and_scale;
		 ippDev->ipp_reset = &ipp_reset;
		 ALOGD("init ion succ,ion size=%d,err=%d",ipp_buffer.pion->size,err);
		}
		else
		{
		 ALOGE("init ion fail,ion size=%d,err=%d",ION_SIZE,err);
		}
	}
	else
	{
		ALOGE("open ipp fail.(ipp_fd=%d)",ipp_fd);
	}
    return ipp_fd;
}

int ipp_close(ipp_device_t *ippDev)
{
   ALOGD("Enter ipp_close.");
   close(ipp_buffer.ipp_fd);
   ipp_buffer.ion_device->free(ipp_buffer.ion_device, ipp_buffer.pion);
   ion_close(ipp_buffer.ion_device);
   ipp_buffer.ion_device=NULL;
   if (ippDev)
   {
	 delete ippDev;
   }
   return 0;
}






