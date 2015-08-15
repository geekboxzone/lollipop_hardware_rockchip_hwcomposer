/*

* rockchip hwcomposer( 2D graphic acceleration unit) .

*

* Copyright (C) 2015 Rockchip Electronics Co., Ltd.

*/
#include <sys/prctl.h>
#include "rk_hwcomposer_hdmi.h"
#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <hardware_legacy/uevent.h>

int         g_hdmi_mode;
void rk_parse_uevent_buf(const char *buf,int* type,int* flag,int len)
{
	const char *str = buf;
	while(*str)
	{
		sscanf(str,"SCREEN=%d,ENABLE=%d",type,flag);
		//ALOGI("SCREEN=%d ENABLE=%d,",*type,*flag);
        str += strlen(str) + 1;
        if (str - buf >= len)
            break;
	    //ALOGI("line %d,buf[%s]",__LINE__,str);
    }
}

void rk_check_hdmi_state()
{
#ifdef RK3288_MID
    int fd = open("/sys/devices/virtual/switch/hdmi/state", O_RDONLY);
#else
    int fd = open("/sys/devices/virtual/display/HDMI/connect", O_RDONLY);
#endif
	if (fd > 0)
	{
		char statebuf[100];
		memset(statebuf, 0, sizeof(statebuf));
		int err = read(fd, statebuf, sizeof(statebuf));
		if (err < 0)
		{
		    ALOGE("error reading hdmi state: %s", strerror(errno));
		    return;
		}
		close(fd);
		g_hdmi_mode = atoi(statebuf);
		//if(g_hdmi_mode == 0)
		hdmi_noready = true;
		/* if (g_hdmi_mode==0)
		{
			property_set("sys.hdmi.mode", "0");
		}
		else
		{
			property_set("sys.hdmi.mode", "1");
		}*/    
	}else{
		ALOGD("err=%s",strerror(errno));
	}
}

//0,1,2
 void rk_check_hdmi_uevents(const char *buf,int len)
{
	//ALOGD("line %d,buf[%s]",__LINE__,buf);
#ifdef RK3288_MID
    if (!strcmp(buf, "change@/devices/virtual/switch/hdmi")){
        rk_check_hdmi_state();
        handle_hotplug_event(g_hdmi_mode,6);
		ALOGI("uevent receive!g_hdmi_mode=%d,line=%d",g_hdmi_mode,__LINE__);
	}else if(strstr(buf, "change@/devices/virtual/display/HDMI") != NULL){
        rk_check_hdmi_state();
		if(g_hdmi_mode == 1){
			handle_hotplug_event(1,6);
			ALOGI("uevent receive!g_hdmi_mode=%d,line=%d",g_hdmi_mode,__LINE__);
		}
    }
#else
    if(strstr(buf, "change@/devices/lcdc") != NULL){
        int type=0;
        int flag=0;
        rk_check_hdmi_state();
        rk_parse_uevent_buf(buf,&type,&flag,len);
        handle_hotplug_event(flag,type);
        ALOGI("uevent receive!hdmistate=%d,type=%d,flag=%d,line=%d",g_hdmi_mode,type,flag,__LINE__);
	}
#endif
}

void rk_handle_uevents(const char *buff,int len)
{
	// uint64_t timestamp = 0;
    rk_check_hdmi_uevents(buff,len);
}


void  *rk_hwc_hdmi_thread(void *arg)
{
    prctl(PR_SET_NAME,"hwc_uevent");
    static char uevent_desc[4096];
    struct pollfd fds[1];
    int timeout;
    int err;
    uevent_init();
    fds[0].fd = uevent_get_fd();
    fds[0].events = POLLIN;
    timeout = 200;//ms
    memset(uevent_desc, 0, sizeof(uevent_desc));
    do {
        err = poll(fds, 1, timeout);
        if (err == -1) {
            if (errno != EINTR)
                ALOGE("event error: %m");
            continue;
        }

        if (fds[0].revents & POLLIN) {
            int len = uevent_next_event(uevent_desc, sizeof(uevent_desc) - 2);
            rk_handle_uevents(uevent_desc,len);
        }
    } while (1);

    pthread_exit(NULL);

    return NULL;
}
