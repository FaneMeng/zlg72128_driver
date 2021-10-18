#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/input/matrix_keypad.h>
#include <linux/i2c.h>

struct zlg72128_dev_data {
	int i2cline;	//i2c总线
	int sla;		//i2c设备地址
    int irq;		//中断引脚
    int rst;		//复位引脚
    int *key_list;	//键盘值映射，第0~23个元素分别代表K1~K23普通按键键值，第24~31个元素分别代表F1~F8特殊按键键值，0为无效键值
};

static int key_list[] = {
	KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8,
	KEY_9, KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G,
	KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O,
	KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8,
};

static struct zlg72128_dev_data zlg72128_data = {
	.i2cline  = 3,
	.sla	  = 0x30,
	.irq	  = 96,
	.rst	  = 100,
	.key_list = key_list,
};

static struct platform_device pdev ={
    .name   =   "zlg72128",
    .id     =   0, 
    .dev    =   {   
        .platform_data = &zlg72128_data,
    },  
};

static int __init zlg72128_init(void)
{
    return platform_device_register(&pdev);
}

static void __exit zlg72128_exit(void)
{
    platform_device_unregister(&pdev);
}

module_init(zlg72128_init);
module_exit(zlg72128_exit);

MODULE_LICENSE("GPL");
