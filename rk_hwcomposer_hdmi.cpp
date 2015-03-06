/*

* rockchip hwcomposer( 2D graphic acceleration unit) .

*

* Copyright (C) 2015 Rockchip Electronics Co., Ltd.

*/
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

//0,1,2
 void rk_check_hdmi_uevents(const char *buf)
{
	//ALOGD("line %d,buf[%s]",__LINE__,buf);
    if ( !strcmp(buf, "change@/devices/virtual/switch/hdmi"))
	{
		int fd = open("/sys/devices/virtual/switch/hdmi/state", O_RDONLY);
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
			/* if (g_hdmi_mode==0)
			{
				property_set("sys.hdmi.mode", "0");
			}
			else
			{
				property_set("sys.hdmi.mode", "1");
			}*/
			handle_hdmi_event(g_hdmi_mode,1);
			ALOGD("HDMI unevent happened!g_hdmi_mode=%d,line=%d",g_hdmi_mode,__LINE__);
			}  
		else
		{
			ALOGD("err=%s",strerror(errno));
		}
	}
    else if( strstr(buf, "change@/devices/virtual/display/HDMI") != NULL )
    {
		int fd = open("/sys/devices/virtual/switch/hdmi/state", O_RDONLY);
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
			if(g_hdmi_mode == 1)
			{
				handle_hdmi_event(1,3);
				ALOGD("HDMI unevent happened!g_hdmi_mode=%d,line=%d",g_hdmi_mode,__LINE__);
			}
		}  
        else
        {
            ALOGD("err=%s",strerror(errno));
        }
    }
}

 void rk_handle_uevents(const char *buff)
{
	// uint64_t timestamp = 0;
    rk_check_hdmi_uevents(buff);
}


void  *rk_hwc_hdmi_thread(void *arg)
{
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
            uevent_next_event(uevent_desc, sizeof(uevent_desc) - 2);
            rk_handle_uevents(uevent_desc);
        }
    } while (1);

    pthread_exit(NULL);

    return NULL;
}
