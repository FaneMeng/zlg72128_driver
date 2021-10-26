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
#define CONFIG_OF
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#endif

#include "zlg72128.h"

#define DEBUG() printk("----------------%d----------------\n",__LINE__)

/*---------------------------------zlg72128寄存器定义------------------------------*/
#define ZLG72128_REG_SYSTEMREG				0x00
#define ZLG72128_REG_KEY					0x01
#define ZLG72128_REG_REPEATCNT				0x02
#define ZLG72128_REG_FUNCTIONKEY			0x03
#define ZLG72128_REG_CMDBUF0				0x07
#define ZLG72128_REG_CMDBUF1				0x08
#define ZLG72128_REG_FLAGONOFF				0x0b
#define ZLG72128_REG_DISPCTRL0				0x0c
#define ZLG72128_REG_DISPCTRL1				0x0d
#define ZLG72128_REG_FLASH0					0x0e
#define ZLG72128_REG_FLASH1					0x0f
#define ZLG72128_REG_DISPBUF0				0x10
#define ZLG72128_REG_DISPBUF(n)				(0x10|(n))
/*---------------------------------------------------------------------------------*/
#define ZLG72128_LED_DISCOUNT				12

/*---------------------------------控制命令定义------------------------------------*/
#define ZLG72128_CMD_SEGONOFF				(0x01<<4) 
#define ZLG72128_CMD_DOWNLOAD				(0x02<<4)
#define ZLG72128_CMD_RESET					(0x03<<4)
#define ZLG72128_CMD_TEST					(0x04<<4)
#define ZLG72128_CMD_SHIFTLEFT				(0x05<<4)
#define ZLG72128_CMD_CYCLICSHIFTLEFT		(0x06<<4)
#define ZLG72128_CMD_SHIFTRIGHT				(0x07<<4)
#define ZLG72128_CMD_CYCLICSHIFTRIGHT		(0x08<<4)
#define ZLG72128_CMD_SCANNING				(0x09<<4)
/*---------------------------------------------------------------------------------*/

#define container_of(ptr, type, member) ({						\
		const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
		(type *)( (char *)__mptr - offsetof(type,member) );})

#define ZLG72128_DRIVER_NAME				"zlg72128"
/**
 * \brief ZLG72128从机地址（用户可检测A4的电平来确定当前ZLG72128模块的从机地址）
 *        若引脚A4为浮空或者高电平，则从机地址为0x30
 *        若引脚A4为低电平，则从机地址为0x20
 */
#define  ZLG72128_PIN_HIGH_SLV_ADDR  0x30
#define  ZLG72128_PIN_LOW_SLV_ADDR   0x20
/*zlg72128设备名*/
#define ZLG72128_LEDDEV_NAME				"i2cLED"
#define ZLG72128_KEYDEV_NAME				"i2cKEY"
/*普通按键延时释放*/
#define ZLG72128_KEY_RELEASE				100
#define ZLG72128_KEY_READ_RELEASE_COUNT		3
/*最大支持按键数*/
#define ZLG72128_MAX_KEY_COUNT				32
#define ZLG72128_MAX_COMMON_KEY_COUNT		24

#define ZLG72128_MAX_DEV_COUNT				8

/*i2c总线号,默认使用i2c2*/
static int i2cline = 3;
module_param(i2cline,int,S_IRUGO);
/*i2c设备地址*/
static int sla = 0x30;
module_param(sla,int,S_IRUGO);
/*中断所使用的gpio,默认为gpio1_31*/
static int irq = 62; 
module_param(irq,int,S_IRUGO);
/*复位所用的gpio,默认为gpio1_30*/
static int rst = 63; 
module_param(rst,int,S_IRUGO);

/*-----------------------------结构体定义-----------------------------*/
/*--------------------------------------------------------------------*/
/*
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

/*--------------------------------------------------------------------*/
/*-----------------------------相关全局变量---------------------------*/
/*--------------------------------------------------------------------*/
static struct zlg72128_dev_t zlg72128_devs[ZLG72128_MAX_DEV_COUNT];
static char *zlg72128_dev_name[ZLG72128_MAX_DEV_COUNT] = {
	"zlg72128-0","zlg72128-1", "zlg72128-2", "zlg72128-3", 
	"zlg72128-4","zlg72128-5", "zlg72128-6", "zlg72128-7",
};

/*
 * 自定义的数据输出函数，数据传输必需通过i2c总线进行传输，
 */
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

/*-------------------------------矩阵键盘-----------------------------*/

static int key_list_def[ZLG72128_MAX_KEY_COUNT] = {
	KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, 
	KEY_9, KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, 
	KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, 
	KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, 
};

/* 校验keyid是否在keys列表中
 * 返回值：
 *		0：按键无效
 *		1：按键有效
 * */
static int is_keyid_in_list(struct zlg72128_dev_t *zdev,int keyid)
{
	int i;
	for(i=0;i<zdev->keys_count;i++){
		if(keyid == zdev->keys[i].keyId)
			return i+1;
	}
	return 0;
}

static void common_key_timeout_handle(unsigned long data)
{
	//printk("-------------------------common key timeout-------------------\n");
	int i;
	struct zlg72128_dev_t *zdev = (struct zlg72128_dev_t *)data;
	for(i=0;i<zdev->keys_count;i++){
		if(!zdev->keys[i].keyFlag){	/*如果为普通按键*/
			if(zdev->keys[i].keyStatus){	/*如果按键被按下*/
				zdev->keys[i].keyStatus = 0;
				/*开始释放按键*/
				input_report_key(zdev->zlg72128_key,zdev->keys[i].keyCode,0);
				input_sync(zdev->zlg72128_key);
				//printk("common key %d is release\n",zdev->keys[i].keyId);
			}
		}
	}	
}

/*中断处理*/
static void keyboard_work_handle(struct work_struct *data)
{
	unsigned char regkey=0,regfunctionkey=0,regrepeatcnt=0;
	unsigned char ret=0,tmp=0;
	struct zlg72128_dev_t *zdev = (struct zlg72128_dev_t *)container_of(data,struct zlg72128_dev_t,keyboard_work);

	/*首先读取key寄存器的值，如果key为0则可能为功能按键，否则为普通按键*/
	ret = zlg72128_read(zdev,ZLG72128_REG_KEY,(char *)(&regkey),1);
	if(ret)
		goto error;

	ret = zlg72128_read(zdev,ZLG72128_REG_SYSTEMREG,(char *)(&tmp),1);
	if(ret)
		goto error;

	if(!regkey)
		goto read_function_key;

	/*判断按键是否在keys列表中*/
	ret = is_keyid_in_list(zdev,regkey);
	if(!ret){
		printk("the keyid %d is unuse\n",regkey);
		return;
	}
	//printk("press common key is 0x%02x\n",regkey);

	/*开始返回按下的按键值*/
	regkey -= 1;
	input_report_key(zdev->zlg72128_key,zdev->keys[regkey].keyCode,1);
	input_sync(zdev->zlg72128_key);

	/*开启定时器，定时结束后释放普通按键*/

	/*普通按键有连按功能*/
	ret = zlg72128_read(zdev,ZLG72128_REG_REPEATCNT,(char *)(&regrepeatcnt),1);
	if(ret)
		goto error;

	zdev->keys[regkey].keyRepeatCnt = regrepeatcnt;
	if(regrepeatcnt){
		zdev->keys[regkey].keyRepeatCnt = regrepeatcnt;
		//printk("repeat press %d times\n",regrepeatcnt);
	}

	/*标志按下*/
	zdev->keys[regkey].keyStatus = 1;
	/*开启定时器*/
	mod_timer(&zdev->common_key_timeout,jiffies + (zdev->keyRelease * HZ / 1000));

	return;

read_function_key:
	if(!zdev->functionKey)
		zdev->functionKey = 0xff;

	ret = zlg72128_read(zdev,ZLG72128_REG_FUNCTIONKEY,(char *)(&regfunctionkey),1);
	if(ret)
		goto error;

	tmp = regfunctionkey ^ zdev->functionKey;
	zdev->functionKey = regfunctionkey;
	regfunctionkey = (~tmp) & 0xff;

	/*有按键改变*/
	if(regfunctionkey != 0xff){
		/*判断按键是否在keys列表中*/
		ret = is_keyid_in_list(zdev,regfunctionkey);
		if(!ret){
			printk("the keyid %d is unuse\n",regfunctionkey);
			return;
		}

		ret -= 1;
		if(zdev->keys[ret].keyStatus){
			/*开始释放按下的按键值*/
			zdev->keys[ret].keyStatus = 0;
			input_report_key(zdev->zlg72128_key,zdev->keys[ret].keyCode,0);
			input_sync(zdev->zlg72128_key);
			//	printk("release function key is 0x%02x\n",keys[ret].keyId);
		}else{
			/*开始上报按下的按键值*/
			zdev->keys[ret].keyStatus = 1;
			input_report_key(zdev->zlg72128_key,zdev->keys[ret].keyCode,1);
			input_sync(zdev->zlg72128_key);
			//	printk("press function key is 0x%02x\n",keys[ret].keyId);
		}
	}

	return;

error:
	printk("--> Error : handle keywork <--\n");
}

/*键盘中断*/
static irqreturn_t zlg72128_key_event(int irq,void *dev_id)
{
	struct zlg72128_dev_t *zdev = dev_id;
	schedule_work(&zdev->keyboard_work);

	return IRQ_HANDLED;
}

/*键盘初始化*/
static int zlg72128_btns_creat(struct device *dev, struct zlg72128_dev_t *zdev, int *keys)
{
	int i=0,ret=0;

	/*初始化输入设备*/
	zdev->zlg72128_key = input_allocate_device();
	if(zdev->zlg72128_key == NULL){
		printk("input allocate device error!!\n");
		ret = -ENOMEM;
		goto input_alloc_err;
	}

	zdev->zlg72128_key->name = ZLG72128_KEYDEV_NAME;
	zdev->zlg72128_key->evbit[0] = BIT_MASK(EV_KEY);
	zdev->keys_count = 0;

	/*根据zlg72128_keys_list设置支持的按键*/
	for(i=0,zdev->keys_count=0;i<ZLG72128_MAX_KEY_COUNT;i++,zdev->keys_count++){
		/*按键值无效*/
		if(keys[i] < 0)
			continue;

		/*初始化可使用的按键列表*/
		if(i < ZLG72128_MAX_COMMON_KEY_COUNT){
			/*普通按键*/
			zdev->keys[zdev->keys_count].keyId = i + 1;
		}else{
			/*功能按键*/
			zdev->keys[zdev->keys_count].keyId = (~(1<<(i-ZLG72128_MAX_COMMON_KEY_COUNT))) & 0xff;
			zdev->keys[zdev->keys_count].keyFlag = 1;
		}
		zdev->keys[zdev->keys_count].keyCode = keys[i];

		/*把键码加入到输入设备*/
		set_bit(zdev->keys[zdev->keys_count].keyCode,zdev->zlg72128_key->keybit);
	}
 
	/*工作队列*/
	INIT_WORK(&zdev->keyboard_work,keyboard_work_handle);

	/*注册中断*/
	ret = request_irq(gpio_to_irq(zdev->irq),zlg72128_key_event,IRQF_TRIGGER_FALLING,"zlg72128-keys",(void *)zdev);
	if(ret){
		printk("request irq error!\n");
		goto request_irq_err;
	}

	/*定时器初始化*/
	init_timer(&zdev->common_key_timeout);
	zdev->common_key_timeout.function = common_key_timeout_handle;
	zdev->common_key_timeout.data = (unsigned long)zdev;

	/*注册输入设备*/
	ret = input_register_device(zdev->zlg72128_key);
	if(ret){
		input_free_device(zdev->zlg72128_key);
		printk("input register error!\n");
		goto input_register_err;
	}

	return 0;

input_register_err:
	free_irq(gpio_to_irq(zdev->irq),NULL);
request_irq_err:
	input_free_device(zdev->zlg72128_key);
input_alloc_err:
	return ret;
}

/*--------------------------------------------------------------------*/

/*------------------------------LED显示-------------------------------*/
/** \brief 定义ZLG72128操作句柄  */
typedef struct zlg72128_dev_t   *zlg72128_handle_t;

/**
 * \brief 设置数码管闪烁时间，即当设定某位闪烁时点亮持续的时间和熄灭持续的时间
 *
 *     出厂设置为：亮和灭的时间均为500ms。时间单位为ms，有效的时间为：
 *  150、200、250、……、800、850、900，即 150ms ~ 900ms，且以50ms为间距。
 *  若时间不是恰好为这些值，则会设置一个最接近的值。
 *
 * \param[in] handle : ZLG72128的操作句柄
 * \param[in] on_ms  : 点亮持续的时间，有效值为 150ms ~ 900ms，且以50ms为间距
 * \param[in] off_ms : 熄灭持续的时间，有效值为 150ms ~ 900ms，且以50ms为间距
 *
 * \retval -2 : 参数错误
 * \retval -1 : 执行失败
 * \retval  0 : 执行成功
 *
 * \note 仅设置闪烁时间并不能立即看到闪烁，还必须打开某位的闪烁开关后方能看到
 * 闪烁现象。 \sa zlg72128_digitron_flash_ctrl()
 */
static int zlg72128_digitron_flash_time_cfg (zlg72128_handle_t  handle,
                                      unsigned char      on_ms,
                                      unsigned char      off_ms)
{
	/*参数检测*/
	if((on_ms < 0) || (on_ms > 15) || (off_ms < 0) || (off_ms > 15)){
		return ZLG72128_RETURN_PARAMETER_ERROR;
	}

	return zlg72128_write(handle,ZLG72128_REG_FLAGONOFF, (on_ms << 4) | off_ms, 0, 1);
}

/**
 * \brief 控制（打开或关闭）数码管闪烁，默认均不闪烁
 *
 * \param[in] handle   : ZLG72128的操作句柄
 * \param[in] ctrl_val : 控制值，位0 ~ 11分别对应从左至右的数码管。值为1时闪烁，
 *                       值为0时不闪烁。
 *
 * \retval -2 : 参数错误
 * \retval -1 : 执行失败
 * \retval  0 : 执行成功
 */
static int zlg72128_digitron_flash_ctrl (zlg72128_handle_t  handle,
                                  int                ctrl_val)
{
	return zlg72128_write(handle,ZLG72128_REG_FLASH0,
			(ctrl_val & 0xff00) >> 8,
			ctrl_val & 0xff,
			2);
}

/**
 * \brief 控制数码管的显示属性（打开显示或关闭显示）
 *
 *    默认情况下，所有数码管均处于打开显示状态，则将按照12位数码管扫描，实际中，
 * 可能需要显示的位数并不足12位，此时，即可使用该函数关闭某些位的显示。
 *
 * \param[in] handle    : ZLG72128的操作句柄
 * \param[in] ctrl_val  : 控制值，位0 ~ 位11分别对应从左至右的数码管。
 *                        值为1关闭显示，值为0打开显示
 *
 * \retval -2 : 参数错误
 * \retval -1 : 执行失败
 * \retval  0 : 执行成功
 *
 * \note 这与设置段码为熄灭段码不同，设置段码为熄灭段码时，数码管同样被扫描，同
 * 样需要在每次扫描时填充相应的段码。使用该函数关闭某些位的显示后，是从根本上不
 * 再扫描该位。
 */
static int zlg72128_digitron_disp_ctrl (zlg72128_handle_t  handle,
                                 int                ctrl_val)
{
	return zlg72128_write(handle,ZLG72128_REG_DISPCTRL0,
			(ctrl_val & 0xff00) >> 8,
			ctrl_val & 0xff,
			2);
}

/**
 * \brief 设置数码管显示的字符
 *
 *      ZLG72128已经提供了常见的10种数字和21种字母的直接显示。若需要显示不支持的
 *  图形，可以直接使用 \sa zlg72128_digitron_dispbuf_set() 函数直接设置段码。
 *
 * \param[in] handle     : ZLG72128的操作句柄
 * \param[in] pos        : 本次显示的位置，有效值 0 ~ 11
 * \param[in] ch         : 显示的字符，支持字符 0123456789AbCdEFGHiJLopqrtUychT
 *                         以及空格（无显示）
 * \param[in] is_dp_disp : 是否显示小数点，TRUE:显示; FALSE:不显示
 * \param[in] is_flash   : 该位是否闪烁，TRUE:闪烁; FALSE:不闪烁
 *
 * \retval -2 : 参数错误
 * \retval -1 : 执行失败
 * \retval  0 : 执行成功
 */
static int zlg72128_digitron_disp_char (zlg72128_handle_t  handle,
                                 unsigned char      pos,
                                 char               ch,
                                 unsigned char      is_dp_disp,
                                 unsigned char      is_flash)
{
	char *dis_char_list="0123456789AbCdEFGHiJLopqrtUychT ";
	int dis_len = strlen(dis_char_list);
	int cmdbuf0=0,cmdbuf1=0;
	int i = 0;
	/*参数检测*/
	if(!((pos >=0 )&&(pos <= 11))){
		return ZLG72128_RETURN_PARAMETER_ERROR;
	}
	
	for(i=0;i<dis_len;i++){
		if(ch == dis_char_list[i]){
			break;
		}
	}

	if(i == dis_len){
		return ZLG72128_RETURN_PARAMETER_ERROR;
	}

	if((is_dp_disp != ZLG72128_TRUE) && (is_dp_disp != ZLG72128_FALSE)){
		return ZLG72128_RETURN_PARAMETER_ERROR;
	}

	if((is_flash != ZLG72128_TRUE) && (is_flash != ZLG72128_FALSE)){
		return ZLG72128_RETURN_PARAMETER_ERROR;
	}

	cmdbuf0	= ZLG72128_CMD_DOWNLOAD | pos;
	cmdbuf1 = (is_dp_disp<<7)|(is_flash<<6)|i;

	return zlg72128_write(handle,ZLG72128_REG_CMDBUF0,cmdbuf0,cmdbuf1,2);
}

/**
 * \brief 从指定位置开始显示一个字符串
 *
 * \param[in] handle     : ZLG72128的操作句柄
 * \param[in] start_pos  : 字符串起始位置
 * \param[in] p_str      : 显示的字符串
 *
 * \retval -2 : 参数错误
 * \retval -1 : 执行失败
 * \retval  0 : 执行成功
 *
 * \note 若遇到不支持的字符，该字符位将不显示
 */
static int zlg72128_digitron_disp_str (zlg72128_handle_t  handle,
                                unsigned char      start_pos,
                                const char        *p_str)
{
	char *dis_char_list="0123456789AbCdEFGHiJLopqrtUychT ";
	int dis_len = strlen(dis_char_list);
	int i=0,j=0,k=0;
	int str_len = strlen(p_str);

	for(i=0,k=start_pos;(i<str_len)&&(k<12);i++,k++){
		for(j=0;j<dis_len;j++){
			if(p_str[i] == dis_char_list[j]){
				break;
			}
		}

		if(j < dis_len){
			if( zlg72128_digitron_disp_char(handle,
						k,
						p_str[i],
						ZLG72128_FALSE,
						ZLG72128_FALSE) != ZLG72128_RETURN_OK){
				return ZLG72128_RETURN_ERROR;
			}
		}else{
			if( zlg72128_digitron_disp_char(handle,
						k,
						' ',
						ZLG72128_FALSE,
						ZLG72128_FALSE) != ZLG72128_RETURN_OK){
				return ZLG72128_RETURN_ERROR;
			}
		}
	}

	return ZLG72128_RETURN_OK;
}

/**
 * \brief 设置数码管显示的数字（0 ~ 9）
 *
 * \param[in] handle     : ZLG72128的操作句柄
 * \param[in] pos        : 本次显示的位置，有效值 0 ~ 11
 * \param[in] num        : 显示的数字，0 ~ 9
 * \param[in] is_dp_disp : 是否显示小数点，1:显示; 0:不显示
 * \param[in] is_flash   : 该位是否闪烁，1:闪烁; 0:不闪烁
 *
 * \retval -2 : 参数错误
 * \retval -1 : 执行失败
 * \retval  0 : 执行成功
 */
static int zlg72128_digitron_disp_num (zlg72128_handle_t  handle,
                                unsigned char      pos,
                                unsigned char      num,
                                unsigned char      is_dp_disp,
                                unsigned char      is_flash)
{
	if((num < 0) || (num > 9))
		return ZLG72128_RETURN_PARAMETER_ERROR;

	return zlg72128_digitron_disp_char(handle, pos, num+'0', is_dp_disp, is_flash);
}

/**
 * \brief 直接设置数码管显示的内容（段码）
 *
 *      当用户需要显示一些自定义的特殊图形时，可以使用该函数，直接设置段码。一
 *  般来讲，ZLG72128已经提供了常见的10种数字和21种字母的直接显示，用户不必使用
 *  此函数直接设置段码，可以直接使用 \sa 函数显示数字或字母
 *      为方便快速设置多个数码管位显示的段码，本函数可以一次连续写入多个数码管
 *  显示的段码。
 *
 * \param[in] handle    : ZLG72128的操作句柄
 * \param[in] start_pos : 本次设置段码起始位置，有效值 0 ~ 11
 * \param[in] p_buf     : 段码存放的缓冲区
 * \param[in] num       : 本次设置的数码管段码的个数
 *
 * \retval -2 : 参数错误
 * \retval -1 : 执行失败
 * \retval  0 : 执行成功
 *
 * \note 若 start_pos + num 的值超过 12 ，则多余的缓冲区内容被丢弃。
 */
static int zlg72128_digitron_dispbuf_set (zlg72128_handle_t  handle,
                                   unsigned char      start_pos,
                                   unsigned char     *p_buf,
                                   unsigned char      num)
{
	int i=0,ret=0;

	if ((start_pos + num) > 12) {
        num = (start_pos + num) - 12;
    }

	for(i=0;i<num;i++){
		ret = zlg72128_write(handle,ZLG72128_REG_DISPBUF(start_pos + i),p_buf[i],0,1);
		if(ret != ZLG72128_RETURN_OK){
			return ret;
		}
	}

	return ZLG72128_RETURN_OK;
}

/**
 * \brief 直接控制段的点亮或熄灭
 *
 *   为了更加灵活的控制数码管显示，提供了直接控制某一段的属性（亮或灭）。
 *
 * \param[in] handle : ZLG72128的操作句柄
 * \param[in] pos    : 控制段所处的位，有效值 0 ~ 11
 * \param[in] seg    : 段，有效值 0 ~ 7，分别对应：a,b,c,d,e,f,g,dp
 *                     建议使用下列宏：
 *                       - ZLG72128_DIGITRON_SEG_A
 *                       - ZLG72128_DIGITRON_SEG_B
 *                       - ZLG72128_DIGITRON_SEG_C
 *                       - ZLG72128_DIGITRON_SEG_D
 *                       - ZLG72128_DIGITRON_SEG_E
 *                       - ZLG72128_DIGITRON_SEG_F
 *                       - ZLG72128_DIGITRON_SEG_G
 *                       - ZLG72128_DIGITRON_SEG_DP
 *
 * \param[in] is_on  : 是否点亮该段，TRUE:点亮; FALSE:熄灭
 *
 * \retval -2 : 参数错误
 * \retval -1 : 执行失败
 * \retval  0 : 执行成功
 */
static int zlg72128_digitron_seg_ctrl (zlg72128_handle_t  handle,
                                unsigned char      pos,
                                char               seg,
                                unsigned char      is_on)
{
	int cmdbuf0=0,cmdbuf1=0;

	/*参数检测*/
	if(!((pos >= 0) && (pos <= 11))){
		return ZLG72128_RETURN_PARAMETER_ERROR;
	}

	if(!((seg >= 0) && (seg <= 7))){
		return ZLG72128_RETURN_PARAMETER_ERROR;
	}

	if((is_on != ZLG72128_TRUE) && (is_on != ZLG72128_FALSE)){
		return ZLG72128_RETURN_PARAMETER_ERROR;
	}

	cmdbuf0 = ZLG72128_CMD_SEGONOFF | is_on;
	cmdbuf1 = (pos<<4)|seg;

	return zlg72128_write(handle,ZLG72128_REG_CMDBUF0,cmdbuf0,cmdbuf1,2);
}

/**
 * \brief 显示移位命令
 *
 *      使当前所有数码管的显示左移或右移，并可指定是否是循环移动。如果不是循环
 *   移位，则移位后，右边空出的位（左移）或左边空出的位（右移）将不显示任何内容。
 *   若是循环移动，则空出的位将会显示被移除位的内容。
 *
 * \param[in] handle    : ZLG72128的操作句柄
 * \param[in] dir       : 移位方向
 *                       - ZLG72128_DIGITRON_SHIFT_LEFT   左移
 *                       - ZLG72128_DIGITRON_SHIFT_RIGHT  右移
 *
 * \param[in] is_cyclic : 是否是循环移位，TRUE，循环移位；FALSE，不循环移位
 * \param[in] num       : 本次移位的位数，有效值 0（不移动） ~ 11，其余值无效
 *
 * \retval -2 : 参数错误
 * \retval -1 : 执行失败
 * \retval  0 : 执行成功
 *
 * \note 关于移位的说明
 *
 *   实际中，可能会发现移位方向相反，这是由于可能存在两种硬件设计：
 *   1、 最右边为0号数码管，从左至右为：11、10、9、8、7、6、5、4、3、2、 1、 0
 *   2、 最左边为0号数码管，从左至右为： 0、 1、2、3、4、5、6、7、8、9、10、11
 *   这主要取决于硬件设计时 COM0 ~ COM11 引脚所对应数码管所处的物理位置。
 *
 *       此处左移和右移的概念是以《数据手册》中典型应用电路为参考的，即最右边
 *   为0号数码管。那么左移和右移的概念分别为：
 *   左移：数码管0显示切换到1，数码管1显示切换到2，……，数码管10显示切换到11
 *   右移：数码管11显示切换到10，数码管1显示切换到2，……，数码管10显示切换到11
 *
 *   可见，若硬件电路设计数码管位置是相反的，则移位效果可能也恰恰是相反的，此处
 * 只需要稍微注意即可。
 */
static int zlg72128_digitron_shift (zlg72128_handle_t  handle,
                             unsigned char      dir,
                             unsigned char      is_cyclic,
                             unsigned char      num)
{
	int cmdbuf0=0;
	/*参数检测*/
	if((dir != ZLG72128_DIGITRON_SHIFT_RIGHT) && (dir != ZLG72128_DIGITRON_SHIFT_LEFT)){
		return ZLG72128_RETURN_PARAMETER_ERROR;
	}

	if((is_cyclic != ZLG72128_TRUE) && (is_cyclic != ZLG72128_FALSE)){
		return ZLG72128_RETURN_PARAMETER_ERROR;
	}

	if(!((num >= 0) && (num <= 11))){
		return ZLG72128_RETURN_PARAMETER_ERROR;
	}

	if(is_cyclic == ZLG72128_TRUE){
		if(dir == ZLG72128_DIGITRON_SHIFT_RIGHT){
			cmdbuf0 = ZLG72128_CMD_CYCLICSHIFTRIGHT | num;
		}else if(dir == ZLG72128_DIGITRON_SHIFT_LEFT){
			cmdbuf0 = ZLG72128_CMD_CYCLICSHIFTLEFT | num;
		}
	}else if(is_cyclic == ZLG72128_FALSE){
		if(dir == ZLG72128_DIGITRON_SHIFT_RIGHT){
			cmdbuf0 = ZLG72128_CMD_SHIFTRIGHT | num;
		}else if(dir == ZLG72128_DIGITRON_SHIFT_LEFT){
			cmdbuf0 = ZLG72128_CMD_SHIFTLEFT | num;
		}
	}

	return zlg72128_write(handle,ZLG72128_REG_CMDBUF0,cmdbuf0,0,1);
}

/**
 * \brief 复位命令，将数码管的所有LED段熄灭
 *
 * \param[in] handle : ZLG72128的操作句柄
 *
 * \retval -2 : 参数错误
 * \retval -1 : 执行失败
 * \retval  0 : 执行成功
 */
static int zlg72128_digitron_disp_reset (zlg72128_handle_t handle)
{
	return zlg72128_write(handle,ZLG72128_REG_CMDBUF0,ZLG72128_CMD_RESET,0,1);
}

/*zlg72128硬件复位*/
static int zlg72128_digitron_reset(zlg72128_handle_t handle)
{
	if(handle->rst){
		gpio_set_value(handle->rst,0);
		msleep(50);
		gpio_set_value(handle->rst,1);

		return ZLG72128_RETURN_OK;
	}else{
		return ZLG72128_RETURN_ERROR;
	}
}

/**
 * \brief 测试命令，所有LED段以0.5S的速率闪烁，用于测试数码管显示是否正常
 *
 * \param[in] handle : ZLG72128的操作句柄
 *
 * \retval -2 : 参数错误
 * \retval -1 : 执行失败
 * \retval  0 : 执行成功
 */
static int zlg72128_digitron_disp_test (zlg72128_handle_t handle)
{
	return zlg72128_write(handle,ZLG72128_REG_CMDBUF0,ZLG72128_CMD_TEST,0,1);
}

/*-------------------------------ioctl控制----------------------------*/
static long zlg72128_ioctl(struct file *flip,unsigned int cmd,unsigned long arg)
{
	int ret = ZLG72128_RETURN_OK;
	struct zlg72128_dev_t *zdev = flip->private_data;

	switch(cmd){
		case ZLG72128_DIGITRON_DISP_CTRL    :{
				struct zlg72128_digitron_disp_ctrl_t *disp_ctrl_t = (struct zlg72128_digitron_disp_ctrl_t *)arg;
				ret = zlg72128_digitron_disp_ctrl(zdev,
						disp_ctrl_t->ctrl_val);
			}break; 
		case ZLG72128_DIGITRON_DISP_CHAR    :{
				struct zlg72128_digitron_disp_char_t *disp_char_t = (struct zlg72128_digitron_disp_char_t *)arg;
				ret = zlg72128_digitron_disp_char(zdev,
						disp_char_t->pos,
						disp_char_t->ch,
						disp_char_t->is_dp_disp,
						disp_char_t->is_flash);
			}break; 
		case ZLG72128_DIGITRON_DISP_STR     :{
				struct zlg72128_digitron_disp_str_t disp_str_t;
				memset(&disp_str_t,0,sizeof(struct zlg72128_digitron_disp_str_t));
				ret = copy_from_user(&disp_str_t, (struct zlg72128_digitron_disp_str_t *)arg,sizeof(struct zlg72128_digitron_disp_str_t));

				ret = zlg72128_digitron_disp_str(zdev,
						disp_str_t.start_pos,
						disp_str_t.p_str);
			}break; 
		case ZLG72128_DIGITRON_DISP_NUM     :{
				struct zlg72128_digitron_disp_num_t *disp_num_t = (struct zlg72128_digitron_disp_num_t *)arg;
				ret = zlg72128_digitron_disp_num(zdev,
						disp_num_t->pos,
						disp_num_t->num,
						disp_num_t->is_dp_disp,
						disp_num_t->is_flash);
			}break; 
		case ZLG72128_DIGITRON_DISPBUF_SET  :{
				struct zlg72128_digitron_dispbuf_set_t dispbuf_set_t;
				memset(&dispbuf_set_t,0,sizeof(struct zlg72128_digitron_dispbuf_set_t));
				ret = copy_from_user(&dispbuf_set_t, (struct zlg72128_digitron_dispbuf_set_t *)arg,sizeof(struct zlg72128_digitron_dispbuf_set_t));

				ret = zlg72128_digitron_dispbuf_set(zdev,
						dispbuf_set_t.start_pos,
						dispbuf_set_t.p_buf,
						dispbuf_set_t.num);
			}break; 
		case ZLG72128_DIGITRON_SEG_CTRL     :{
				struct zlg72128_digitron_seg_ctrl_t *seg_ctrl_t = (struct zlg72128_digitron_seg_ctrl_t *)arg;
				ret = zlg72128_digitron_seg_ctrl(zdev, 
						seg_ctrl_t->pos,
						seg_ctrl_t->seg,
						seg_ctrl_t->is_on);
			}break; 
		case ZLG72128_DIGITRON_FLASH_CTRL   :{
				struct zlg72128_digitron_flash_ctrl_t *flash_ctrl_t = (struct zlg72128_digitron_flash_ctrl_t *)arg;
				ret = zlg72128_digitron_flash_ctrl(zdev,
						flash_ctrl_t->ctrl_val);
			}break; 
		case ZLG72128_DIGITRON_FLASH_TIME_CFG:{
				struct zlg72128_digitron_flash_time_cfg_t *flash_time_cfg_t = (struct zlg72128_digitron_flash_time_cfg_t *)arg;
				ret = zlg72128_digitron_flash_time_cfg(zdev,
						flash_time_cfg_t->on_ms,
						flash_time_cfg_t->off_ms);
			}break;
		case ZLG72128_DIGITRON_SHIFT        :{
				struct zlg72128_digitron_shift_t *shift_t = (struct zlg72128_digitron_shift_t *)arg;
				ret = zlg72128_digitron_shift(zdev,
						shift_t->dir,
						shift_t->is_cyclic,
						shift_t->num);
			}break; 
		case ZLG72128_DIGITRON_DISP_RESET   :{
				ret = zlg72128_digitron_disp_reset(zdev);
			}break; 
		case ZLG72128_DIGITRON_DISP_TEST    :{
				ret = zlg72128_digitron_disp_test(zdev);
			}break; 
		case ZLG72128_DIGITRON_RESET        :{
				ret = zlg72128_digitron_reset(zdev);
			}break; 
		default: ret = ZLG72128_RETURN_PARAMETER_ERROR; break;
	}

	return ret;
}

static int zlg72128_open (struct inode *node, struct file *flip)
{
	int i=0;
	int z_minor=MINOR(node->i_rdev);
	
	for(i=0;i<ZLG72128_MAX_DEV_COUNT;i++){
		if(zlg72128_devs[i].status){
			if(zlg72128_devs[i].zlg72128_misc.minor == z_minor){
				flip->private_data = &zlg72128_devs[i];
				break;
			}
		}
	}

	return 0;
}

static struct file_operations zlg72128_fops = {  
	.owner  =   THIS_MODULE,
	.open = zlg72128_open,
	.unlocked_ioctl = zlg72128_ioctl,
};  

struct zlg72128_dev_data {
    int i2cline;    //i2c总线
    int sla;		//i2c设备地址
    int irq;        //中断引脚
    int rst;        //复位引脚
    int *key_list;  //键盘值映射，第0~23个元素分别代表K1~K23普通按键键值，第24~31个元素分别代表F1~F8特殊按键键值，0为无效键值
};

static int zlg72128_probe(struct platform_device *pdev)
{
	int zlg72128_dev_count=0;
	int ret=0,i=0;
	struct zlg72128_dev_data dev_data;
	int _i2cline,_sla,_rst,_irq;
	int key_list[ZLG72128_MAX_KEY_COUNT] = {0};

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

	{
#ifdef CONFIG_OF
	}
	if (pdev->dev.of_node) {
		/*设置i2c adapter*/
		if(!of_property_read_u32(pdev->dev.of_node,"i2cline",&_i2cline)){
			i2cline = _i2cline;
		}
		
		/*设置从机地址*/
		if(!of_property_read_u32(pdev->dev.of_node,"sla",&_sla)){
			sla = _sla;
		}

		if(!of_property_read_u32(pdev->dev.of_node,"irq",&_irq)){
			irq = _irq;
		}

		if(!of_property_read_u32(pdev->dev.of_node,"rst",&_rst)){
			rst = _rst;
		}

		if(of_property_read_u32_array(pdev->dev.of_node,"keys_list",key_list,ZLG72128_MAX_KEY_COUNT) < 0){
			dev_data.key_list = key_list_def;
		}else{
			dev_data.key_list = key_list;
		}
	}else{
#endif
		struct zlg72128_dev_data *pdata = pdev->dev.platform_data;
		/*设置从机地址*/
		if(pdata->sla){
			sla = pdata->sla;
		}
		/*设置i2c adapter*/
		if(pdata->i2cline){
			i2cline = pdata->i2cline;
		}
		
		if(pdata->irq){
			irq = pdata->irq;
		}

		if(pdata->rst){
			rst = pdata->rst;
		}

		if(pdata->key_list){
			dev_data.key_list = pdata->key_list;
		}else{
			dev_data.key_list = key_list_def;
		}
	}

	zlg72128_devs[zlg72128_dev_count].addr = sla;
	/*设置普通按键释放延时ms*/
	zlg72128_devs[zlg72128_dev_count].keyRelease = ZLG72128_KEY_RELEASE;
	zlg72128_devs[zlg72128_dev_count].zlg72128_adapter = i2c_get_adapter(i2cline);
	dev_data.irq = irq;
	dev_data.rst = rst;
	dev_data.sla = sla;
	dev_data.i2cline = i2cline;

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

	if(dev_data.rst){
		zlg72128_devs[zlg72128_dev_count].rst = dev_data.rst;
		ret = gpio_request_one(zlg72128_devs[zlg72128_dev_count].rst, GPIOF_OUT_INIT_LOW, "zlg72128_rst_gpio");
		if(ret){
			printk("zlg72128 reset gpio request error!!\n");
			goto rst_err;
		}
		/*复位*/
		gpio_direction_output(zlg72128_devs[zlg72128_dev_count].rst,0);
		msleep(50);
		gpio_direction_output(zlg72128_devs[zlg72128_dev_count].rst,1);
	}

	if(dev_data.irq){
		zlg72128_devs[zlg72128_dev_count].irq = dev_data.irq;
		ret = gpio_request_one(zlg72128_devs[zlg72128_dev_count].irq, GPIOF_IN, "zlg72128_irq_gpio");
		if(ret){
			printk("zlg72128 irq gpio request error!!\n");
			goto irq_err;
		}
	}else{
		printk("zlg72128-%d irq is not initial!!\n",zlg72128_dev_count);
		goto irq_err;
	}

	ret = zlg72128_btns_creat(&pdev->dev,&zlg72128_devs[zlg72128_dev_count],dev_data.key_list);
	if(ret < 0){
		printk("zlg72128-%d btn create error!!!\n",zlg72128_dev_count);
		goto btns_creat_err;
	}

	/*把zlg72128设备加入到platform_data，在ioctl中被调用*/
	pdev->dev.platform_data = &zlg72128_devs[zlg72128_dev_count];

	/*初始化并注册miscdev*/  
	zlg72128_devs[zlg72128_dev_count].zlg72128_misc.minor	=	MISC_DYNAMIC_MINOR;		//自动分配次设备号
	zlg72128_devs[zlg72128_dev_count].zlg72128_misc.name	=	zlg72128_dev_name[zlg72128_dev_count];	//dev目录下的设备文件名
	zlg72128_devs[zlg72128_dev_count].zlg72128_misc.fops	=	&zlg72128_fops;			//操作函数集

	ret = misc_register(&zlg72128_devs[zlg72128_dev_count].zlg72128_misc);
	if(ret){
		printk("zlg72128 misc register error!!\n");
		goto misc_register_err;
	}

	zlg72128_devs[zlg72128_dev_count].status = 1;

	/*出厂设置*/

	/*1. 设置闪烁的点亮和熄灭时间各为500ms*/
	ret = zlg72128_digitron_flash_time_cfg (&zlg72128_devs[zlg72128_dev_count],7,7);
	if(ret != ZLG72128_RETURN_OK){
		printk("zlg72128-%d init flash_time_cfg error!!!\n",zlg72128_dev_count);
		goto led_init_err;
	}

	/*2. 默认均不闪烁*/
	ret = zlg72128_digitron_flash_ctrl (&zlg72128_devs[zlg72128_dev_count], 0);
	if(ret != ZLG72128_RETURN_OK){
		printk("zlg72128-%d init flash_ctrl error!!!\n",zlg72128_dev_count);
		goto led_init_err;
	}
	
	/*3. 打开12路LED显示*/
	ret = zlg72128_digitron_disp_ctrl (&zlg72128_devs[zlg72128_dev_count], 0);
	if(ret != ZLG72128_RETURN_OK){
		printk("zlg72128-%d init disp_ctrl error!!!\n",zlg72128_dev_count);
		goto led_init_err;
	}

	/*4. 熄灭所有数码管*/
	ret = zlg72128_digitron_disp_reset (&zlg72128_devs[zlg72128_dev_count]);
	if(ret != ZLG72128_RETURN_OK){
		printk("zlg72128-%d init disp_rest error!!!\n",zlg72128_dev_count);
		goto led_init_err;
	}

	return 0;

led_init_err:
	zlg72128_devs[zlg72128_dev_count].status = 0;
	misc_deregister(&zlg72128_devs[zlg72128_dev_count].zlg72128_misc);
misc_register_err:
	input_unregister_device(zlg72128_devs[zlg72128_dev_count].zlg72128_key);
	free_irq(gpio_to_irq(zlg72128_devs[zlg72128_dev_count].irq),NULL);
	input_free_device(zlg72128_devs[zlg72128_dev_count].zlg72128_key);
btns_creat_err:
	gpio_free(zlg72128_devs[zlg72128_dev_count].irq);
irq_err:
	if(zlg72128_devs[zlg72128_dev_count].rst){
		gpio_free(zlg72128_devs[zlg72128_dev_count].rst);
	}
rst_err:
	return ret;
}

static int zlg72128_remove(struct platform_device *pdev)
{
	int i=0;
	for(;(i<ZLG72128_MAX_DEV_COUNT) && (zlg72128_devs[i].status);i++){
		zlg72128_devs[i].status = 0;
		misc_deregister(&zlg72128_devs[i].zlg72128_misc);
		input_unregister_device(zlg72128_devs[i].zlg72128_key);
		input_free_device(zlg72128_devs[i].zlg72128_key);

		if(zlg72128_devs[i].irq){
			free_irq(gpio_to_irq(zlg72128_devs[i].irq),NULL);
			gpio_free(zlg72128_devs[i].irq);
		}

		if(zlg72128_devs[i].rst){
			gpio_free(zlg72128_devs[i].rst);
		}
	}
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id __maybe_unused zlg72128_dt_ids[] = {
    { .compatible = "zlg72128",},
    { }  
};
MODULE_DEVICE_TABLE(of, zlg72128_dt_ids);
#endif

static struct platform_driver zlg72128_platform_driver = {
    .driver = {
        .name = ZLG72128_DRIVER_NAME,
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = of_match_ptr(zlg72128_dt_ids),
#endif
    },   
    .probe = zlg72128_probe,
    .remove = zlg72128_remove,
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
MODULE_AUTHOR("zhongweijie@zlg.cn");

