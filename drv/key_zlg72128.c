#include <linux/module.h>  
#include <linux/kernel.h>  
#include <linux/types.h>  
#include <linux/fs.h>  
#include <linux/init.h>  
#include <linux/delay.h>  
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
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






static int zlg72128_probe(struct i2c_client *i2c_client,const struct i2c_devied_id *id)
{
	printk("This is zlg72128 probe function!\n");
	return 0;
}

static int zlg72128_remove(struct i2c_client *i2c_client)
{
	printk("This is zlg72128 remove function!\n");
	return 0;
}

const struct  i2c_device_id zlg72128_id_table = 
{
	.name = ZLG72128_DRIVER_NAME
};

const struct of_device_id  zlg72128_dt_ids[] = {
    { .compatible = "zlg72128"},
    { }  
};

static struct i2c_driver zlg72128_driver = {
	.probe = zlg72128_probe,
    .remove = zlg72128_remove,
    .driver = {
        .owner = THIS_MODULE,
		.name = ZLG72128_DRIVER_NAME,
        .of_match_table = zlg72128_dt_ids
    },
	.id_table = &zlg72128_id_table 
};

static int __init zlg72128_init(void)
{
	int ret;
    ret = i2c_add_driver(&zlg72128_driver);
	if(ret < 0)
	{
		printk("zlg72128_driver error!\n");
		return ret;
	}
	printk("zlg72128_driver added!\n");
	return 0;
}

static void __exit zlg72128_exit(void)
{
    i2c_del_driver(&zlg72128_driver);
	printk("zlg72128 driver deleted!\n");
}

module_init(zlg72128_init);
module_exit(zlg72128_exit);

MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("mengfy1995@hotmail.com");