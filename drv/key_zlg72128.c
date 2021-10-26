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

#include "zlg72128.h"

/*默认按键键值*/
static int key_list_def[ZLG72128_MAX_KEY_COUNT] = {
	KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, 
	KEY_9, KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, 
	KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, 
	KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, 
};

/* 按键信息结构体
 * keyId        :   按键号，1～24为普通按键，0xf0~0xf7为功能按键，0为无效按键
 * keyFlag      :   按键的类型，0为普通按键，1为功能按键
 * keyCode      :   键值
 * keyRepeatCnt :   如果为普通按键，则代表连按次数
 * keyStatus    :   按键状态，1为按下，0为弹起
 * */
struct ZLG72128_KEY{
    int keyId;
    int keyFlag;
    int keyCode;
    int keyRepeatCnt;
    int keyStatus;
};

/*zlg72128设备信息结构体*/
struct zlg72128_dev_t {
	int addr;								/*从设备地址*/
	int status;								/*用于表示当前设备状态 0:未被使用 1:已使用*/
	struct miscdevice zlg72128_misc;		/*杂项设备*/
	struct i2c_adapter *zlg72128_adapter;	/*i2c adapter*/
	int irq;							/*中断所使用的gpio*/
	int rst;							/*复位所用的gpio*/
	struct work_struct keyboard_work;		/*用于键盘读取中断*/
	struct input_dev *zlg72128_key;			/*键盘输入设备*/
	struct timer_list common_key_timeout;   /*定时器, 用于普通按键释放延时*/
	int keyRelease;							/*普通按键实际延时*/
	int keys_count;							/*有效注册的按键*/
	int functionKey;						/*记录特殊按键的值，用于判断释放顺序*/
	struct ZLG72128_KEY keys[ZLG72128_MAX_KEY_COUNT]; /*自定义按键*/
};

/*设备树信息获取结构体*/
struct zlg72128_dev_data {
    int i2cline;    //i2c总线
    int sla;		//i2c设备地址
    int irq;        //中断引脚
    int rst;        //复位引脚
    int *key_list;  //键盘值映射，第0~23个元素分别代表K1~K23普通按键键值，第24~31个元素分别代表F1~F8特殊按键键值，0为无效键值
};

/*-----------------------------相关全局变量---------------------------*/
static struct zlg72128_dev_t zlg72128_devs[ZLG72128_MAX_DEV_COUNT];
static char *zlg72128_dev_name[ZLG72128_MAX_DEV_COUNT] = {
	"zlg72128-0","zlg72128-1", "zlg72128-2", "zlg72128-3", 
	"zlg72128-4","zlg72128-5", "zlg72128-6", "zlg72128-7",
};


/*--------------------------------i2c 数据传输----------------------------------*/
/*双字节写入*/
static int zlg72128_write(struct zlg72128_dev_t *zdev, int offset, char value0, char value1, int count)
{
	unsigned char data[3];
	struct i2c_msg msg;

	data[0]     =   offset;
	data[1]     =   value0;
	data[2]		=	value1;

	msg.addr    =   zdev->addr;
	msg.flags   =   0;
	msg.buf     =   data;
	msg.len     =   count + 1;

	if(i2c_transfer(zdev->zlg72128_adapter,&msg,1)!=1)
		return ZLG72128_RETURN_ERROR;

	return ZLG72128_RETURN_OK;
}

/*读多字节数据*/
static int zlg72128_read(struct zlg72128_dev_t *zdev,int offset,char *data,int count)
{
	unsigned char wr_data;
	struct i2c_msg msg[2];

	wr_data = offset;
	msg[0].addr     =   zdev->addr;
	msg[0].flags    =   0;
	msg[0].buf      =   &wr_data;
	msg[0].len      =   1;

	msg[1].addr     =   zdev->addr;
	msg[1].flags    =   I2C_M_RD;
	msg[1].buf      =   data;
	msg[1].len      =   count;

	if(i2c_transfer(zdev->zlg72128_adapter,msg,2)!=2)
		return ZLG72128_RETURN_ERROR;

	return ZLG72128_RETURN_OK;
}
/*--------------------------------------------------------------------*/


int zlg72128_probe(struct i2c_client *i2c_client,const struct i2c_devied_id *id)
{
	unsigned char regkey = 0;
	int zlg72128_dev_count=0;
	int ret=0,i=0;
	struct zlg72128_dev_data dev_data;
	int _i2cline,_sla,_rst,_irq;
	int key_list[ZLG72128_MAX_KEY_COUNT] = {0};
	printk("This is zlg72128 probe function!\n");
	for(zlg72128_dev_count=0;zlg72128_dev_count<ZLG72128_MAX_DEV_COUNT;zlg72128_dev_count++){
		if(!zlg72128_devs[zlg72128_dev_count].status){
			break;
		}
	}

	if(zlg72128_dev_count >= ZLG72128_MAX_DEV_COUNT){
		printk("zlg72128 dev count : %d --> too much device for zlg72128!!\n",zlg72128_dev_count);
		return -EINVAL;
	}

	memset(&dev_data,0,sizeof(dev_data));
	
	/*获取设备树信息*/
	if(!of_property_read_u32(i2c_client->dev.of_node,"i2cline",&_i2cline)){
		dev_data.i2cline = _i2cline;
	}
	
	if(!of_property_read_u32(i2c_client->dev.of_node,"sla",&_sla)){
		dev_data.sla = _sla;
	}
	if(!of_property_read_u32(i2c_client->dev.of_node,"irq",&_irq)){
		dev_data.irq = _irq;
	}
	if(!of_property_read_u32(i2c_client->dev.of_node,"rst",&_rst)){
		dev_data.rst = _rst;
	}
	if(of_property_read_u32_array(i2c_client->dev.of_node,"keys_list",key_list,ZLG72128_MAX_KEY_COUNT) < 0){
		dev_data.key_list = key_list_def;
	}else{
		dev_data.key_list = key_list;
	}

	/*填充设备信息结构体*/
	zlg72128_devs[zlg72128_dev_count].addr = dev_data.sla;
	zlg72128_devs[zlg72128_dev_count].keyRelease = ZLG72128_KEY_RELEASE;
	zlg72128_devs[zlg72128_dev_count].zlg72128_adapter = i2c_client->adapter;

	printk("zlg72128-%d i2cline : %d\n",zlg72128_dev_count,dev_data.i2cline);
	printk("zlg72128-%d sla     : 0x%02x\n",zlg72128_dev_count,dev_data.sla);
	printk("zlg72128-%d irq pin : %d\n",zlg72128_dev_count,dev_data.irq);
	printk("zlg72128-%d rst pin : %d\n",zlg72128_dev_count,dev_data.rst);
	printk("zlg72128-%d keys : \n",zlg72128_dev_count);
	for(ret=0;ret<ZLG72128_MAX_KEY_COUNT;){
		printk("\t\t");
		for(i=0;i<8;i++,ret++){
			printk("%2d ",dev_data.key_list[ret]);
		}
		printk("\n");
	}
	
	while(!regkey)
	{
		ret = zlg72128_read(&zlg72128_devs[zlg72128_dev_count],ZLG72128_REG_KEY,(char *)(&regkey),1);
	} 
	printk("zlg72128-%d regkey : %d\n",zlg72128_dev_count,regkey);
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