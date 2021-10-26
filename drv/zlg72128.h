#ifndef __ZLG72128_LEDKEYS_H
#define __ZLG72128_LEDKEYS_H


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









/*-------------------------------------------------------------------*/
/*----------------------------ioctl 控制命令-------------------------*/
/*-------------------------------------------------------------------*/
#define ZLG72128_MAGIC                  'F'

#define ZLG72128_DIGITRON_DISP_CTRL			_IOC(_IOC_WRITE,ZLG72128_MAGIC,0,0)
#define ZLG72128_DIGITRON_DISP_CHAR			_IOC(_IOC_WRITE,ZLG72128_MAGIC,1,0)
#define ZLG72128_DIGITRON_DISP_STR			_IOC(_IOC_WRITE,ZLG72128_MAGIC,2,0)
#define ZLG72128_DIGITRON_DISP_NUM			_IOC(_IOC_WRITE,ZLG72128_MAGIC,3,0)
#define ZLG72128_DIGITRON_DISPBUF_SET		_IOC(_IOC_WRITE,ZLG72128_MAGIC,4,0)
#define ZLG72128_DIGITRON_SEG_CTRL			_IOC(_IOC_WRITE,ZLG72128_MAGIC,5,0)
#define ZLG72128_DIGITRON_FLASH_CTRL		_IOC(_IOC_WRITE,ZLG72128_MAGIC,6,0)
#define ZLG72128_DIGITRON_FLASH_TIME_CFG	_IOC(_IOC_WRITE,ZLG72128_MAGIC,7,0)
#define ZLG72128_DIGITRON_SHIFT				_IOC(_IOC_WRITE,ZLG72128_MAGIC,8,0)
#define ZLG72128_DIGITRON_DISP_RESET		_IOC(_IOC_WRITE,ZLG72128_MAGIC,9,0)
#define ZLG72128_DIGITRON_DISP_TEST			_IOC(_IOC_WRITE,ZLG72128_MAGIC,10,0)
#define ZLG72128_DIGITRON_RESET				_IOC(_IOC_WRITE,ZLG72128_MAGIC,11,0)

/*-------------------------------------------------------------------*/
/*-----------------------------相关宏定义----------------------------*/
/*-------------------------------------------------------------------*/

/*
 * ioctl命令返回值 : -2参数错误, -1执行失败, 0执行成功
 * */
#define ZLG72128_RETURN_PARAMETER_ERROR		-2
#define	ZLG72128_RETURN_ERROR				-1
#define ZLG72128_RETURN_OK					0

/** \brief 部分接口调用ZLG72128_TRUE或ZLG72128_FLASE控制数码管属性 */
#define ZLG72128_FALSE					0
#define ZLG72128_TRUE					1

/**
 * \name 数码管段
 * 用于 zlg72128_digitron_seg_ctrl() 段控制函数的 seg 参数。
 */
#define AM_ZLG72128_DIGITRON_SEG_A		0
#define AM_ZLG72128_DIGITRON_SEG_B		1
#define AM_ZLG72128_DIGITRON_SEG_C		2
#define AM_ZLG72128_DIGITRON_SEG_D		3
#define AM_ZLG72128_DIGITRON_SEG_E		4
#define AM_ZLG72128_DIGITRON_SEG_F		5
#define AM_ZLG72128_DIGITRON_SEG_G		6
#define AM_ZLG72128_DIGITRON_SEG_DP		7

/**
 * \name 数码管显示移位的方向
 * 用于 \sa zlg72128_digitron_shift() 函数的 \a dir 参数。
 */
#define  ZLG72128_DIGITRON_SHIFT_RIGHT   0   /**< \brief 右移  */
#define  ZLG72128_DIGITRON_SHIFT_LEFT    1   /**< \brief 左移  */

/*-------------------------------------------------------------------*/
/*---------------------对应ioctl命令所使用的结构体-------------------*/
/*-------------------------------------------------------------------*/

/* 功能	:	设置显示属性（开或关）
 * 成员	:
 *		ctrl_val : 控制值
 * 说明 : 
 *		ctrl_val的bit0~bit11为有效位，分别对应数码管0~11，位值为0时打开显示，位值为1时关闭显示，上电时默认为0x0000，即所有位均正常显示
 * */
struct zlg72128_digitron_disp_ctrl_t{
	int ctrl_val;
};

/* 功能 : 在指定位置显示字符
 * 成员 :
 *		pos			:	字符显示位置(0~11)
 *		ch			:	显示的字符(空格为不显示)
 *		is_dp_disp	:	是否显示小数点(TRUE显示, FALSE不显示)
 *		is_flash	:	是否闪烁(TRUE闪烁, FALSE不闪烁)
 * 说明 : 
 *		显示的字符必须是ZLG72128已经支持的可以自动完成译码的字符，包括字符'0'~'9'与AbCdEFGHiJLopqrtUychT(区分大小写)。注意，若要显示数字1.则ch参数应为字符'1'，而不是数字1
 * */
struct zlg72128_digitron_disp_char_t{
	unsigned char pos;
	char ch;
	unsigned char is_dp_disp;
	unsigned char is_flash;
};

/* 功能 : 显示字符串
 * 成员 : 
 *		start_pos	:	字符串显示起始位置 
 *		p_str		:	字符串
 * 说明 :
 *		字符串显示遇到字符结束标志"\0"将自动结束，或当超过有效的字符显示区域时，也会自动结束。显示的字符应确保是ZLG72128能够自动完成译码的，包括字符'0'~'9'与AbCdEFGHiJLopqrtUychT（区分大小写）。如遇到有不支持的字符，对应位置将不显示任何内容
 * */
struct zlg72128_digitron_disp_str_t{
	unsigned char start_pos;
	unsigned char p_str[12];
};

/* 功能 : 显示0~9的数字
 * 成员 : 
 *		pos : 数字显示位置(0~11)
 *		num : 显示的数字(0~9)
 *		is_dp_disp	: 是否显示小数点(TRUE显示, FALSE不显示)
 *		is_flash	: 是否闪烁(TRUE闪烁, FALSE不闪烁)
 * 说明 : 
 *		该函数仅用于显示一个0~9的数字，若数字大于9，应自行根据需要分别显示各个位。注意，num参数为数字0~9，不是字符'0'~'9'
 * */
struct zlg72128_digitron_disp_num_t{
	unsigned char pos;
	unsigned char num;
	unsigned char is_dp_disp;
	unsigned char is_flash;
};

/* 功能 : 直接显示段码, 设置多个数码管的显示内容
 * 成员 : 
 *		start_pos	: 起始位置(0~11)
 *		p_buf		: 段码缓冲区
 *		num			: 本次设置段码的个数
 * 说明 : 
 *		段码为8位，bit0~bit7分别对应段a~dp。位值为1时，对应段点亮，位值为0时，对应段熄灭, 若 start_pos + num 的值超过 12 ，则多余的缓冲区内容被丢弃
 * */
struct zlg72128_digitron_dispbuf_set_t{
	unsigned char start_pos;
	unsigned char p_buf[12];
	unsigned char num;
};

/* 功能 : 直接控制段的点亮或熄灭
 * 成员 : 
 *		pos		: 数码管的位置(0~11)
 *		seg		: 控制的段(0~7)
 *		is_on	: 是否点亮该段(TRUE点亮, FALSE熄灭)
 * 说明 : 
 *		建议不要直接使用立即数0 ~ 7，而应使用与a ~ dp对应的宏：AM_ZLG72128_DIGITRON_SEG_A ~ AM_ZLG72128_DIGITRON_SEG_DP
 * */
struct zlg72128_digitron_seg_ctrl_t{
	unsigned char pos;
	char seg;
	unsigned char is_on;
};

/* 功能 : 闪烁控制
 * 成员 : 
 *		ctrl_val	: 闪烁控制值
 * 说明 : 
 *		ctrl_val为控制值，bit0 ~ bit11为有效位，分别对应数码管0 ~ 11，为0时不闪烁，为1时闪烁
 * */
struct zlg72128_digitron_flash_ctrl_t{
	int ctrl_val;
};

/* 功能 : 闪烁持续时间控制
 * 成员 : 
 *		on_ms	: 点亮持续时间(N*50ms + 150ms)
 *		off_ms	: 熄灭持续时间(N*50ms + 150ms)
 * 说明 : 
 *		上电时，数码管点亮和熄灭的持续时间默认值为500ms。on_ms和off_ms有效的时间值为150、200、250、……、800、850、900，即150ms ~ 900ms,且时间间隔为50ms。若时间间隔不是这些值，应该选择一个最接近的值
 * */
struct zlg72128_digitron_flash_time_cfg_t{
	unsigned char on_ms;
	unsigned char off_ms;
};

/* 功能 : 显示移位控制
 * 成员 : 
 *		dir			: 移位方向
 *		is_cyclic	: 是否循环移位(TRUE循环移位, FALSE不循环移位)
 *		num			: 移动的位数(0~11, 0不移位, 其他值无效)
 * 说明 : 
 *		如果不是循环移位，则移动后，右边空出的位（左移）或左边空出的位（右移）将不显示任何内容。若是循环移动，则空出的位将会显示被移除位的内容
 * */
struct zlg72128_digitron_shift_t{
	unsigned char dir;
	unsigned char is_cyclic;
	unsigned char num;
};

#endif
