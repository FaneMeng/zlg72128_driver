#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>

#include "zlg72128.h"

#define LED_DEV	 "/dev/zlg72128-0"

int main(int argc,char *argv[])
{
	zlg72128_handle_t handle;
	int ret;

	handle = zlg72128_init(LED_DEV);
	if(handle == ZLG72128_RETURN_ERROR){
		printf("zlg72128_init error\n");
		return handle;
	}

	// 控制数码管的显示属性（打开显示）
	ret = zlg72128_digitron_disp_ctrl (handle, 0);
	if(ret != ZLG72128_RETURN_OK){
		printf("Error --------------%d----------------\n",__LINE__);
		goto error;
	}

	// 从0位置开始显示一个字符串"012356789"
	ret = zlg72128_digitron_disp_str (handle, 0, "0123456789");
	if(ret != ZLG72128_RETURN_OK){
		printf("Error --------------%d----------------\n",__LINE__);
		goto error;
	}

	// 设置第10个数码管显示的字符'1'
	ret = zlg72128_digitron_disp_char (handle, 10, '1', ZLG72128_TRUE, ZLG72128_TRUE);
	if(ret != ZLG72128_RETURN_OK){
		printf("Error --------------%d----------------\n",__LINE__);
		goto error;
	}

	// 设置第11个数码管显示的数字8
	ret = zlg72128_digitron_disp_num (handle, 11, 8, ZLG72128_TRUE, ZLG72128_TRUE);
	if(ret != ZLG72128_RETURN_OK){
		printf("Error --------------%d----------------\n",__LINE__);
		goto error;
	}

	// 设置数码管闪烁时间，闪烁时点亮持续的时间500ms和熄灭持续的时间500ms
	ret = zlg72128_digitron_flash_time_cfg (handle, 500, 500);
	if(ret != ZLG72128_RETURN_OK){
		printf("Error --------------%d----------------\n",__LINE__);
		goto error;
	}

	// brief 控制（打开或关闭）数码管闪烁，中间四个LED闪烁其他不闪烁
	ret = zlg72128_digitron_flash_ctrl (handle,0x0f0);
	if(ret != ZLG72128_RETURN_OK){
		printf("Error --------------%d----------------\n",__LINE__);
		goto error;
	}

	// 循环左移两位
	ret = zlg72128_digitron_shift (handle, ZLG72128_DIGITRON_SHIFT_LEFT, ZLG72128_TRUE, 2);
	if(ret != ZLG72128_RETURN_OK){
		printf("Error --------------%d----------------\n",__LINE__);
		goto error;
	}

	sleep(5);

	// 直接设置数码管显示的内容（段码）
	unsigned char buf[12] = {0x09,0x09,0x09,0x09,0x40,0x40,0x40,0x40,0x09,0x09,0x09,0x09};
	ret = zlg72128_digitron_dispbuf_set (handle, 0, buf, 12);
	if(ret != ZLG72128_RETURN_OK){
		printf("Error --------------%d----------------\n",__LINE__);
		goto error;
	}

	sleep(5);

	// 控制第5个数码管的G段熄灭
	ret = zlg72128_digitron_seg_ctrl (handle, 5, AM_ZLG72128_DIGITRON_SEG_G, ZLG72128_FALSE);
	if(ret != ZLG72128_RETURN_OK){
		printf("Error --------------%d----------------\n",__LINE__);
		goto error;
	}

	// 控制第6个数码管的G段熄灭
	ret = zlg72128_digitron_seg_ctrl (handle, 6, AM_ZLG72128_DIGITRON_SEG_G, ZLG72128_FALSE);
	if(ret != ZLG72128_RETURN_OK){
		printf("Error --------------%d----------------\n",__LINE__);
		goto error;
	}

	sleep(5);

	// 复位命令，将数码管的所有LED段熄灭
	ret = zlg72128_digitron_disp_reset (handle);
	if(ret != ZLG72128_RETURN_OK){
		printf("Error --------------%d----------------\n",__LINE__);
		goto error;
	}

	sleep(5);

	// 测试命令，所有LED段以0.5S的速率闪烁，用于测试数码管显示是否正常
	ret = zlg72128_digitron_disp_test (handle);
	if(ret != ZLG72128_RETURN_OK){
		printf("Error --------------%d----------------\n",__LINE__);
		goto error;
	}

error:
	return ret;
}
