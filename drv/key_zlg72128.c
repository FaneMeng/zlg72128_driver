#include <linux/module.h>  
#include <linux/kernel.h>  
#include <linux/types.h>  
#include <linux/fs.h>  
#include <linux/init.h>  
#include <linux/delay.h>  
#include <asm/uaccess.h>  
#include <asm/irq.h>  
#include <asm/io.h>  
#include <linux/gpio.h>
#include <linux/cdev.h>  
#include <linux/device.h>  
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>

#define ZLG72128_DRIVER_NAME				"zlg72128"
#define container_of(ptr, type, member) ({						\
		const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
		(type *)( (char *)__mptr - offsetof(type,member) );})

struct platform_driver {
	int (*probe)(struct platform_device *);
	int (*remove)(struct platform_device *);
	void (*shutdown)(struct platform_device *);
	int (*suspend)(struct platform_device *, pm_message_t state);
	int (*resume)(struct platform_device *);
	struct device_driver driver;
	const struct platform_device_id *id_table;
	bool prevent_deferred_probe;
};

static struct platform_driver zlg72128_platform_driver = {
    .driver = {
        .name = ZLG72128_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(zlg72128_dt_ids),
    },   
    .probe = zlg72128_probe,
    .remove = zlg72128_remove,
};

struct of_device_id {
	char	name[32];
	char	type[32];
	char	compatible[128];
	const void *data;
};

static int __init zlg72128_init(void)
{
    return platform_driver_register(&zlg72128_platform_driver);
}

static void __exit zlg72128_exit(void)
{
    platform_driver_unregister(&zlg72128_platform_driver);
}

late_initcall(zlg72128_init);
module_exit(zlg72128_exit);

MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("mengfy1995@hotmail.com");