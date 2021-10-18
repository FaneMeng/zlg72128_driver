#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>

#include "zlg72128.h"

/*-------------------------------------------------------------------*/
/*---------------------对应ioctl命令所使用的结构体-------------------*/
/*-------------------------------------------------------------------*/

/* 功能 :   设置显示属性（开或关）
 * 成员 :
 *      ctrl_val : 控制值
 * 说明 :
 *      ctrl_val的bit0~bit11为有效位，分别对应数码管0~11，位值为0时打开显示，位值为1时关闭显示，上电时默>认为0x0000，即所有位均正常显示
 * */
struct zlg72128_digitron_disp_ctrl_t{
    int ctrl_val;
};

/* 功能 : 在指定位置显示字符
 * 成员 :
 *      pos         :   字符显示位置(0~11)
 *      ch          :   显示的字符(空格为不显示)
 *      is_dp_disp  :   是否显示小数点(TRUE显示, FALSE不显示)
 *      is_flash    :   是否闪烁(TRUE闪烁, FALSE不闪烁)
 * 说明 :
 *      显示的字符必须是ZLG72128已经支持的可以自动完成译码的字符，包括字符'0'~'9'与AbCdEFGHiJLopqrtUychT(区分大小写)。注意，若要显示数字1.则ch参数应为字符'1'，而不是数字1
 * */
struct zlg72128_digitron_disp_char_t{
    unsigned char pos;
    char ch;
    unsigned char is_dp_disp;
    unsigned char is_flash;
};

/* 功能 : 显示字符串
 * 成员 :
 *      start_pos   :   字符串显示起始位置
 *      p_str       :   字符串
 * 说明 :
 *      字符串显示遇到字符结束标志"\0"将自动结束，或当超过有效的字符显示区域时，也会自动结束。显示的字符>应确保是ZLG72128能够自动完成译码的，包括字符'0'~'9'与AbCdEFGHiJLopqrtUychT（区分大小写）。如遇到有不支持>的字符，对应位置将不显示任何内容
 * */
struct zlg72128_digitron_disp_str_t{
    unsigned char start_pos;
    char p_str[12];
};

/* 功能 : 显示0~9的数字
 * 成员 :
 *      pos : 数字显示位置(0~11)
 *      num : 显示的数字(0~9)
 *      is_dp_disp  : 是否显示小数点(TRUE显示, FALSE不显示)
 *      is_flash    : 是否闪烁(TRUE闪烁, FALSE不闪烁)
 * 说明 :
 *      该函数仅用于显示一个0~9的数字，若数字大于9，应自行根据需要分别显示各个位。注意，num参数为数字0~9>，不是字符'0'~'9'
 * */
struct zlg72128_digitron_disp_num_t{
    unsigned char pos;
    unsigned char num;
    unsigned char is_dp_disp;
    unsigned char is_flash;
};

/* 功能 : 直接显示段码, 设置多个数码管的显示内容
 * 成员 :
 *      start_pos   : 起始位置(0~11)
 *      p_buf       : 段码缓冲区
 *      num         : 本次设置段码的个数
 * 说明 :
 *      段码为8位，bit0~bit7分别对应段a~dp。位值为1时，对应段点亮，位值为0时，对应段熄灭, 若 start_pos + num 的值超过 12 ，则多余的缓冲区内容被丢弃
 * */
struct zlg72128_digitron_dispbuf_set_t{
    unsigned char start_pos;
    unsigned char p_buf[12];
    unsigned char num;
};

/* 功能 : 直接控制段的点亮或熄灭
 * 成员 :
 *      pos     : 数码管的位置(0~11)
 *      seg     : 控制的段(0~7)
 *      is_on   : 是否点亮该段(TRUE点亮, FALSE熄灭)
 * 说明 :
 *      建议不要直接使用立即数0 ~ 7，而应使用与a ~ dp对应的宏：AM_ZLG72128_DIGITRON_SEG_A ~ AM_ZLG72128_DIGITRON_SEG_DP
 * */
struct zlg72128_digitron_seg_ctrl_t{
    unsigned char pos;
    char seg;
    unsigned char is_on;
};

/* 功能 : 闪烁控制
 * 成员 :
 *      ctrl_val    : 闪烁控制值
 * 说明 :
 *      ctrl_val为控制值，bit0 ~ bit11为有效位，分别对应数码管0 ~ 11，为0时不闪烁，为1时闪烁
 * */
struct zlg72128_digitron_flash_ctrl_t{
    int ctrl_val;
};

/* 功能 : 闪烁持续时间控制
 * 成员 :
 *      on_ms   : 点亮持续时间
 *      off_ms  : 熄灭持续时间
 * 说明 :
 *      上电时，数码管点亮和熄灭的持续时间默认值为500ms。on_ms和off_ms有效的时间值为150、200、250、……、800、850、900，即150ms ~ 900ms,且时间间隔为50ms。若时间间隔不是这些值，应该选择一个最接近的值
 * */
struct zlg72128_digitron_flash_time_cfg_t{
    unsigned char on_ms;
    unsigned char off_ms;
};

/* 功能 : 显示移位控制
 * 成员 :
 *      dir         : 移位方向
 *      is_cyclic   : 是否循环移位(TRUE循环移位, FALSE不循环移位)
 *      num         : 移动的位数(0~11, 0不移位, 其他值无效)
 * 说明 :
 *      如果不是循环移位，则移动后，右边空出的位（左移）或左边空出的位（右移）将不显示任何内容。若是循环>移动，则空出的位将会显示被移除位的内容
 * */
struct zlg72128_digitron_shift_t{
    unsigned char dir;
    unsigned char is_cyclic;
    unsigned char num;
};


/*-------------------------------------------------------------------*/
/*--------------------------LED操作函数定义--------------------------*/
/*-------------------------------------------------------------------*/


/**
 * \brief ZLG72128初始化函数
 *   调用ZLG72128其他接口前，应该首先调用该初始化函数，以获取操作ZLG72128的handle。
 * 获取到的句柄在调用大部分接口时，需传入到接口的第一个参数handle中。
 *
 * \param[in] dev_path   : 对应的zlg72128设备，一般为/dev/zlg72128-n，其中n为0~7
 *
 * \return 操作ZLG72128的handle，若初始化失败，则返回值为ZLG72128_RETURN_ERROR。
 *
 */
zlg72128_handle_t zlg72128_init (char *dev_path)
{
	zlg72128_handle_t fd;
	fd = open(dev_path,O_RDONLY);
	if(fd < 0){
		return ZLG72128_RETURN_ERROR;
	}

	return fd;
}

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
int zlg72128_digitron_flash_time_cfg (zlg72128_handle_t  handle,
                                      unsigned int      on_ms,
                                      unsigned int      off_ms)
{
	struct zlg72128_digitron_flash_time_cfg_t flash_time_cfg_t;

	memset(&flash_time_cfg_t,0,sizeof(struct zlg72128_digitron_flash_time_cfg_t));

	flash_time_cfg_t.on_ms = (on_ms - 150) / 50;
	flash_time_cfg_t.off_ms = (off_ms - 150) / 50;

	return ioctl(handle,ZLG72128_DIGITRON_FLASH_TIME_CFG,&flash_time_cfg_t);
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
int zlg72128_digitron_flash_ctrl (zlg72128_handle_t  handle,
                                  int                ctrl_val)
{
	struct zlg72128_digitron_flash_ctrl_t flash_ctrl_t;

	memset(&flash_ctrl_t,0,sizeof(struct zlg72128_digitron_flash_ctrl_t));

	flash_ctrl_t.ctrl_val = ctrl_val;

	return ioctl(handle,ZLG72128_DIGITRON_FLASH_CTRL,&flash_ctrl_t);
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
int zlg72128_digitron_disp_ctrl (zlg72128_handle_t  handle,
                                 int                ctrl_val)
{
	struct zlg72128_digitron_disp_ctrl_t disp_ctrl_t;

	memset(&disp_ctrl_t,0,sizeof(struct zlg72128_digitron_disp_ctrl_t));

	disp_ctrl_t.ctrl_val = ctrl_val;

	return ioctl(handle,ZLG72128_DIGITRON_DISP_CTRL,&disp_ctrl_t);
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
int zlg72128_digitron_disp_char (zlg72128_handle_t  handle,
                                 unsigned char      pos,
                                 char               ch,
                                 unsigned char      is_dp_disp,
                                 unsigned char      is_flash)
{
	struct zlg72128_digitron_disp_char_t disp_char_t;
	
	memset(&disp_char_t,0,sizeof(struct zlg72128_digitron_disp_char_t));

	disp_char_t.pos = pos;
	disp_char_t.ch = ch;
	disp_char_t.is_dp_disp = is_dp_disp;
	disp_char_t.is_flash = is_flash;

	return ioctl(handle,ZLG72128_DIGITRON_DISP_CHAR,&disp_char_t);
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
int zlg72128_digitron_disp_str (zlg72128_handle_t  handle,
                                unsigned char      start_pos,
                                char        *p_str)
{
	struct zlg72128_digitron_disp_str_t disp_str_t;

	memset(&disp_str_t, 0, sizeof(struct zlg72128_digitron_disp_str_t));

	disp_str_t.start_pos = start_pos;
	memcpy(disp_str_t.p_str,p_str,strlen(p_str));

	return ioctl(handle,ZLG72128_DIGITRON_DISP_STR,&disp_str_t);
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
int zlg72128_digitron_disp_num (zlg72128_handle_t  handle,
                                unsigned char      pos,
                                unsigned char      num,
                                unsigned char      is_dp_disp,
                                unsigned char      is_flash)
{
	struct zlg72128_digitron_disp_num_t disp_num_t;
	
	memset(&disp_num_t,0,sizeof(struct zlg72128_digitron_disp_num_t));

	disp_num_t.pos = pos;
	disp_num_t.num = num;
	disp_num_t.is_dp_disp = is_dp_disp;
	disp_num_t.is_flash = is_flash;

	return ioctl(handle,ZLG72128_DIGITRON_DISP_NUM,&disp_num_t);
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
int zlg72128_digitron_dispbuf_set (zlg72128_handle_t  handle,
                                   unsigned char      start_pos,
                                   unsigned char     *p_buf,
                                   unsigned char      num)
{
	struct zlg72128_digitron_dispbuf_set_t dispbuf_set_t;

	memset(&dispbuf_set_t,0,sizeof(struct zlg72128_digitron_dispbuf_set_t));

	dispbuf_set_t.start_pos = start_pos;
	memcpy(dispbuf_set_t.p_buf,p_buf,num);
	dispbuf_set_t.num = num;

	return ioctl(handle,ZLG72128_DIGITRON_DISPBUF_SET,&dispbuf_set_t);
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
 * \param[in] is_on  : 是否点亮该段，1:点亮; 0:熄灭
 *
 * \retval -2 : 参数错误
 * \retval -1 : 执行失败
 * \retval  0 : 执行成功
 */
int zlg72128_digitron_seg_ctrl (zlg72128_handle_t  handle,
                                unsigned char      pos,
                                char               seg,
                                unsigned char      is_on)
{
	struct zlg72128_digitron_seg_ctrl_t seg_ctrl_t;

	memset(&seg_ctrl_t,0,sizeof(struct zlg72128_digitron_seg_ctrl_t));

	seg_ctrl_t.pos = pos;
	seg_ctrl_t.seg = seg;
	seg_ctrl_t.is_on = is_on;

	return ioctl(handle,ZLG72128_DIGITRON_SEG_CTRL,&seg_ctrl_t);
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
int zlg72128_digitron_shift (zlg72128_handle_t  handle,
                             unsigned char      dir,
                             unsigned char      is_cyclic,
                             unsigned char      num)
{
	struct zlg72128_digitron_shift_t shift_t;

	memset(&shift_t,0,sizeof(struct zlg72128_digitron_shift_t));

	shift_t.dir = dir;
	shift_t.is_cyclic = is_cyclic;
	shift_t.num = num;

	return ioctl(handle,ZLG72128_DIGITRON_SHIFT,&shift_t);
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
int zlg72128_digitron_disp_reset (zlg72128_handle_t handle)
{
	return ioctl(handle, ZLG72128_DIGITRON_DISP_RESET,0);
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
int zlg72128_digitron_disp_test (zlg72128_handle_t handle)
{
	return ioctl(handle, ZLG72128_DIGITRON_DISP_TEST,0);
}

/**
 * \brief 硬件复位命令
 *
 * \param[in] handle : ZLG72128的操作句柄
 *
 * \retval -2 : 参数错误
 * \retval -1 : 执行失败
 * \retval  0 : 执行成功
 */
int zlg72128_digitron_reset (zlg72128_handle_t handle)
{
	return ioctl(handle, ZLG72128_DIGITRON_RESET,0);
}
