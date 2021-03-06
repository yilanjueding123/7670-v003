
#include "ap_peripheral_handling.h"
#include "ap_state_config.h"
#include "ap_state_handling.h"

//#include "drv_l1_system.h"
#include "driver_l1.h"
#include "drv_l1_cdsp.h"


#define LED_STATUS_FLASH		1
#define LED_STATUS_BLINK		2

#define CRAZY_KEY_TEST			0		// Send key events faster than human finger can do
#define LED_ON					1
#define LED_OFF 				0

static INT8U	led_status; //0: nothing  1: flash	2: blink
static INT8U	led_cnt;
static INT8U	power_keyup = 1;

static INT32U	led_mode;
static INT16U	g_led_count;
static INT8U	g_led_r_state; //0 = OFF;	1=ON;	2=Flicker
static INT8U	g_led_g_state;
static INT8U	g_led_flicker_state; //0=肮奀匢佶	1=蝠杸匢佶
static INT8U	led_red_flag;
static INT8U	led_green_flag;
static INT8U	led_night_flag;
static INT16U	power_off_cnt = 0;

//static INT8U	   charge_full_flag=0;
#if TV_DET_ENABLE
INT8U			tv_plug_in_flag;
INT8U			tv_debounce_cnt = 0;

static INT8U	tv = !TV_DET_ACTIVE;
static INT8U	backlight_tmr = 0;

#endif

#if 1 //C_SCREEN_SAVER == CUSTOM_ON

INT8U			auto_off_force_disable = 0;
void ap_peripheral_auto_off_force_disable_set(INT8U);

#endif

static INT8U	led_flash_timerid;
static INT16U	config_cnt;

//----------------------------
typedef struct 
{
INT8U			byRealVal;
INT8U			byCalVal;
} AD_MAP_t;


//----------------------------
extern void avi_adc_gsensor_data_register(void * *msgq_id, INT32U * msg_id);
INT8U			gsensor_data[2][32] =
{
	0
};




#define C_BATTERY_STABLE_THRESHOLD 4  // Defines threshold number that AD value is deemed stable

#if C_BATTERY_DETECT			== CUSTOM_ON
static INT16U	low_voltage_cnt;
static INT32U	battery_value_sum = 0;
static INT8U	bat_ck_cnt = 0;
extern INT8S	video_record_sts;

#endif



#if USE_ADKEY_NO
static INT8U	ad_detect_timerid;
static INT16U	ad_value;
static KEYSTATUS ad_key_map[USE_ADKEY_NO + 1];

//static INT16U ad_key_cnt = 0;
#define C_RESISTOR_ACCURACY 	5//josephhsieh@140418 3			// 2% accuracy
#define C_KEY_PRESS_WATERSHED	600//josephhsieh@140418 175
#define C_KEY_STABLE_THRESHOLD	4//josephhsieh@140418 3			// Defines threshold number that AD value of key is deemed stable
#define C_KEY_FAST_JUDGE_THRESHOLD 40			// Defines threshold number that key is should be judge before it is release. 0=Disable
#define C_KEY_RELEASE_STABLE_THRESHOLD 4  // Defines threshold number that AD value is deemed stable

INT16U			adc_key_value;

//static INT8U	ad_value_cnt ;
INT32U			key_pressed_cnt;
INT8U			fast_key_exec_flag;
INT8U			normal_key_exec_flag;
INT8U			long_key_exec_flag;

#endif

static INT32U	key_active_cnt;
static INT8U	power_off_timerid;
static INT8U	usbd_detect_io_timerid;
static KEYSTATUS key_map[USE_IOKEY_NO];
static INT8U	key_detect_timerid;
static INT16U	adp_out_cnt;
static INT16U	usbd_cnt;
static INT8U	up_firmware_flag = 0;
static INT8U	flash_flag = 0;

static INT8U	r_up_firmware_flag = 0;
static INT8U	r_flash_flag = 0;

#if USB_PHY_SUSPEND 			== 1
static INT16U	phy_cnt = 0;

#endif

static INT16U	adp_cnt;
INT8U			adp_status;
static INT8U	battery_low_flag = 0;

INT8U			usbd_exit;
INT8U			s_usbd_pin;

//extern INT8U MODE_KEY_flag;
//	prototypes
void ap_peripheral_key_init(void);
void ap_peripheral_rec_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_function_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_next_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_prev_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_ok_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_sos_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_usbd_plug_out_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_pw_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_menu_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_video_prev_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_ir_change_exe(INT16U * tick_cnt_ptr);


void ap_peripheral_modection_exe(INT16U * tick_cnt_ptr);

#if KEY_FUNTION_TYPE			== SAMPLE2
void ap_peripheral_capture_key_exe(INT16U * tick_cnt_ptr);

#endif

void ap_peripheral_null_key_exe(INT16U * tick_cnt_ptr);

#if USE_ADKEY_NO
void ap_peripheral_ad_detect_init(INT8U adc_channel, void(*bat_detect_isr) (INT16U data));
void ap_peripheral_ad_check_isr(INT16U value);

#endif

extern volatile INT8U video_down_flag;
extern volatile INT8U pic_down_flag;

extern INT8U usb_state_get(void);
extern void usb_state_set(INT8U flag);


static void init_usbstate(void)
{

	static INT8U	usb_dete_cnt = 0;
	static INT8U	err_cnt = 0;

	while (++err_cnt < 100)
	{
		if (sys_pwr_key1_read())
			usb_dete_cnt++;
		else 
		{
			usb_dete_cnt		= 0;
			break;
		}

		if (usb_dete_cnt > 3)
			break;

		OSTimeDly(2);
	}

	if (usb_dete_cnt > 3)
		usb_state_set(3);

	err_cnt 			= 0;


}


void ap_peripheral_init(void)
{
#if TV_DET_ENABLE
	INT32U			i;

#endif

	power_off_timerid	= usbd_detect_io_timerid = led_flash_timerid = 0xFF;
	key_detect_timerid	= 0xFF;

	//LED IO init
	//gpio_init_io(LED, GPIO_OUTPUT);
	//gpio_set_port_attribute(LED, ATTRIBUTE_HIGH);
	//gpio_write_io(LED, DATA_LOW);
	//led_status = 0;
	//led_cnt = 0;
	init_usbstate();


	//DBG_PRINT("usb=%d\r\n",usb_state_get());
	gpio_init_io(CHARGE_PIN, GPIO_INPUT);
	gpio_set_port_attribute(CHARGE_PIN, ATTRIBUTE_LOW);
	gpio_write_io(CHARGE_PIN, 1);					//pull low

#if 1
	gpio_init_io(IR_CTRL, GPIO_OUTPUT);
	gpio_set_port_attribute(IR_CTRL, ATTRIBUTE_HIGH);
	gpio_write_io(IR_CTRL, DATA_LOW);
#endif

	power_off_cnt		= PERI_POWER_OFF;
	ap_peripheral_key_init();
	LED_pin_init();

#if USE_ADKEY_NO
	ad_detect_timerid	= 0xFF;
	ap_peripheral_ad_detect_init(AD_KEY_DETECT_PIN, ap_peripheral_ad_check_isr);

#else

	adc_init();
#endif

	config_cnt			= 0;

	//MODE_KEY_flag = 2;
}



void ap_peripheral_led_set(INT8U type)
{
#ifdef PWM_CTR_LED
	INT8U			byPole;
	INT16U			wPeriod = 0;
	INT16U			wPreload = 0;
	INT8U			byEnable;

	if (type)
	{ //high
		ap_peripheral_PWM_LED_high();
	}
	else 
	{ //low
		ap_peripheral_PWM_LED_low();
	}

#else

	gpio_write_io(LED, type);
	led_status			= 0;
	led_cnt 			= 0;
#endif
}


void ap_peripheral_led_flash_set(void)
{
#ifdef PWM_CTR_LED
	ap_peripheral_PWM_LED_high();
	led_status			= LED_STATUS_FLASH;
	led_cnt 			= 0;

#else

	gpio_write_io(LED, DATA_HIGH);
	led_status			= LED_STATUS_FLASH;
	led_cnt 			= 0;
#endif
}


void ap_peripheral_auto_off_force_disable_set(INT8U auto_off_disable)
{
	auto_off_force_disable = auto_off_disable;
}


void ap_peripheral_led_blink_set(void)
{

#ifdef PWM_CTR_LED
	INT8U			byPole = 1;
	INT16U			wPeriod = 0x6000;
	INT16U			wPreload = 0x2fff;
	INT8U			byEnable = TRUE;

	ext_rtc_pwm0_enable(byPole, wPeriod, wPreload, byEnable);

	//	  ext_rtc_pwm1_enable(byPole, wPeriod, wPreload, byEnable) ; 
	DBG_PRINT("PWM0/1 blink on	750ms,380ms \r\n");

#else

	gpio_write_io(LED, DATA_HIGH);
	led_status			= LED_STATUS_BLINK;
	led_cnt 			= 0;
#endif
}


void LED_pin_init(void)
{
	INT32U			type;

	//led init as ouput pull-low
	gpio_init_io(LED1, GPIO_OUTPUT);
	gpio_set_port_attribute(LED1, ATTRIBUTE_HIGH);
	gpio_write_io(LED1, DATA_LOW);

	gpio_init_io(LED2, GPIO_OUTPUT);
	gpio_set_port_attribute(LED2, ATTRIBUTE_HIGH);
	gpio_write_io(LED2, DATA_LOW);

	//OSTimeDly(50);
	led_night_flag		= 0;
	led_red_flag		= LED_OFF;
	led_green_flag		= LED_OFF;
	type				= LED_INIT;
	msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &type, sizeof(INT32U), MSG_PRI_NORMAL);

	//msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_ZD, &type, sizeof(INT8U), MSG_PRI_NORMAL);	
	sys_registe_timer_isr(LED_blanking_isr);		//timer base c to start adc convert
}


extern INT8U	card_space_less_flag;
INT8U			PREV_LED_TYPE = 0;


static void charge_flash_pro(void)
{
	if (gpio_read_io(CHARGE_PIN) == 0)
	{

		g_led_g_state		= 4;
	}
	else 
	{
		led_green_on();
	}
}


extern INT8U	record_led_flag;


void set_led_mode(LED_MODE_ENUM mode)
{
	INT8U			i, type = 0;
	static INT8U	prev_mode = 0xaa;

	led_mode			= mode;

	g_led_g_state		= 0;						//3oE??‾oiμAAA
	g_led_r_state		= 0;
	g_led_flicker_state = 0;



	PREV_LED_TYPE		= prev_mode;

	switch ((INT32U)
	mode)
	{
		case LED_INIT:
			led_red_off();
			led_green_on();
			DBG_PRINT("led_type = LED_INIT\r\n");
			break;

		case LED_UPDATE_PROGRAM:
			led_red_off();
			g_led_g_state = 1;
			DBG_PRINT("led_type = LED_UPDATE_PROGRAM\r\n");
			break;

		case LED_UPDATE_FINISH:
			led_red_off();
			led_green_on();
			DBG_PRINT("led_type = LED_UPDATE_FINISH\r\n");
			break;

		case LED_UPDATE_FAIL:
			sys_release_timer_isr(LED_blanking_isr);

			for (i = 0; i < 2; i++)
			{
				led_all_off();
				OSTimeDly(15);
				led_green_on();
				OSTimeDly(15);
				led_all_off();
			}

			DBG_PRINT("led_type = LED_UPDATE_FAIL\r\n");
			sys_registe_timer_isr(LED_blanking_isr);
			break;

		case LED_USB_CONNECT:
			//led_red_on();
			break;

		case LED_CHARGEING:
			if (prev_mode != mode)
			{
				g_led_count 		= 0;
			}

			g_led_g_state = 4;
			DBG_PRINT("led_type = LED_CHARGEING\r\n");
			break;

		case LED_RECORD:
			if (prev_mode != mode)
			{
				g_led_count 		= 0;
			}

			led_all_off();
			g_led_r_state = 0;
			g_led_g_state = 0;
			g_led_flicker_state = 0;
			DBG_PRINT("led_type = LED_RECORD\r\n");
			break;

		case LED_MONTION_WAIT:
			led_all_off();
			break;

		case LED_WAITING_RECORD:
			if (usb_state_get())
			{
				charge_flash_pro();
			}
			else 
			{

				led_red_off();
				led_green_on();

			}

			DBG_PRINT("led_type = LED_WAITING_RECORD\r\n");
			break;

		case LED_AUDIO_RECORD:
			DBG_PRINT("led_type = LED_AUDIO_RECORD\r\n");
			break;

		case LED_WAITING_AUDIO_RECORD:
			DBG_PRINT("led_type = LED_WAITING_AUDIO_RECORD\r\n");
			break;

		case LED_CAPTURE:
			// sys_release_timer_isr(LED_blanking_isr);
			{

				led_green_on();
				led_red_on();

				// OSTimeDly(20);
				// led_green_on();
			}
			//sys_registe_timer_isr(LED_blanking_isr);	
			DBG_PRINT("led_type = LED_CAPTURE\r\n");
			break;

		case LED_CARD_DETE_SUC:
			if (storage_sd_upgrade_file_flag_get() == 2)
				break;

			if ((prev_mode == LED_CHARGE_FULL) || (prev_mode == LED_CHARGEING))
			{
				if (usb_state_get())
				{
					charge_flash_pro();
				}

				break;
			}

			sys_release_timer_isr(LED_blanking_isr);
			led_green_off();
			OSTimeDly(7);
			led_green_on();
			sys_registe_timer_isr(LED_blanking_isr);
			break;

		case LED_CAPTURE_FAIL:
			for (i = 0; i < 2; i++)
			{
				led_all_off();
				OSTimeDly(50);
				led_red_on();
				OSTimeDly(50);
			}

		case LED_WAITING_CAPTURE:
			if (usb_state_get())
			{
				charge_flash_pro();
			}
			else 
			{

				led_red_off();
				led_green_on();

			}

			DBG_PRINT("led_type = LED_WAITING_CAPTURE\r\n");
			break;

		case LED_MOTION_DETECTION:
			// led_red_on();
			// led_green_on();		
			led_green_off();
			g_led_r_state = 1;
			DBG_PRINT("led_type = LED_MOTION_DETECTION\r\n");
			break;

		case LED_NO_SDC:
			if (usb_state_get())
			{
				charge_flash_pro();
			}
			else 
			{
				for (i = 0; i < 2; i++)
				{
					led_green_off();
					OSTimeDly(10);
					led_green_on();
					OSTimeDly(10);
				}

				OSTimeDly(50);

				for (i = 0; i < 2; i++)
				{
					led_green_off();
					OSTimeDly(10);
					led_green_on();
					OSTimeDly(10);
				}

				msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
			}

			DBG_PRINT("led_type = LED_NO_SDC\r\n");
			break;

		case LED_SDC_FULL:
		case LED_CARD_NO_SPACE:
			if (storage_sd_upgrade_file_flag_get() == 2)
				break;

			if (usb_state_get())
			{
				charge_flash_pro();
			}
			else 
			{
				for (i = 0; i < 2; i++)
				{
					led_green_off();
					OSTimeDly(15);
					led_green_on();
					OSTimeDly(15);
				}

				msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);

			}

			break;

		case LED_TELL_CARD:
			break;

		case LED_LOW_BATT:
			if (storage_sd_upgrade_file_flag_get() == 2)
				break;

#if 0
			sys_release_timer_isr(LED_blanking_isr);

			for (i = 0; i < 3; i++)
			{
				led_green_off();
				OSTimeDly(15);
				led_green_on();
				OSTimeDly(15);
			}

			sys_registe_timer_isr(LED_blanking_isr);
#endif

			msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
			break;

		case LED_LOW_BATT1:
			//g_led_r_state=1;
			break;

		case LED_CHARGE_FULL:
			led_green_on();
			DBG_PRINT("led_type = LED_CHARGE_FULL\r\n");
			break;
	}

	prev_mode			= mode;
}


static void night_led_ctrl(INT8U on)
{
#if 0

	if (on)
	{
		gpio_write_io(M_LED_0, DATA_LOW);
		gpio_write_io(M_LED_1, DATA_LOW);
		gpio_write_io(M_LED_2, DATA_LOW);
		gpio_write_io(M_LED_3, DATA_LOW);
		gpio_write_io(M_LED_4, DATA_LOW);

	}
	else 
	{
		gpio_write_io(M_LED_0, DATA_HIGH);
		gpio_write_io(M_LED_1, DATA_HIGH);
		gpio_write_io(M_LED_2, DATA_HIGH);
		gpio_write_io(M_LED_3, DATA_HIGH);
		gpio_write_io(M_LED_4, DATA_HIGH);
	}

#endif
}


void led_red_on(void)
{
	if (led_red_flag != LED_ON)
	{
		gpio_write_io(LED2, DATA_HIGH);
		led_red_flag		= LED_ON;
	}
}


void led_green_on(void)
{
	if (led_green_flag != LED_ON)
	{
		gpio_write_io(LED1, DATA_HIGH);
		led_green_flag		= LED_ON;
	}
}


void led_all_off(void)
{
	if (led_green_flag != LED_OFF)
	{
		gpio_write_io(LED1, DATA_LOW);
		led_green_flag		= LED_OFF;
	}

	if (led_red_flag != LED_OFF)
	{
		gpio_write_io(LED2, DATA_LOW);
		led_red_flag		= LED_OFF;
	}
}


void led_green_off(void)
{
	if (led_green_flag != LED_OFF)
	{
		gpio_write_io(LED1, DATA_LOW);
		led_green_flag		= LED_OFF;
	}
}


void led_red_off(void)
{
	//if(usb_state_get())
	//	return;
	if (led_red_flag != LED_OFF)
	{
		gpio_write_io(LED2, DATA_LOW);
		led_red_flag		= LED_OFF;
	}
}


extern INT8U	video_stop_flag;


void LED_blanking_isr(void)
{

	//if(card_space_less_flag)
	//	return;
	if (g_led_count++ == 127)
	{
		g_led_count 		= 0;
	}

	if (video_stop_flag)
		return;

	if (g_led_g_state == 1)
	{
		if (g_led_count % 10 == 0)
		{
			if (up_firmware_flag == 1)
			{
				led_green_off();
				up_firmware_flag	= 0;
			}
			else 
			{
				led_green_on();
				up_firmware_flag	= 1;
			}
		}
	}
	else if (g_led_g_state == 2)
	{
		if (g_led_count / 64 == g_led_flicker_state)
			led_green_on();
		else 
			led_green_off();
	}
	else if (g_led_g_state == 3)
	{

		if (g_led_count % 32 == 0)
		{
			if (flash_flag == 0)
			{
				led_green_on();
				flash_flag			= 1;
			}
			else 
			{
				led_green_off();
				flash_flag			= 0;
			}

		}
	}
	else if (g_led_g_state == 4)
	{
		if (g_led_count % 128 == 0)
		{
			if (flash_flag == 0)
			{
				led_green_on();
				flash_flag			= 1;
			}
			else if (flash_flag == 1)
			{
				led_green_off();
				flash_flag			= 2;
			}
			else 
			{
				led_green_off();
				flash_flag			= 0;
			}

		}


	}



	if (g_led_r_state == 1)
	{
		if (g_led_count % 64 == 0)
		{
			if (r_up_firmware_flag == 1)
			{
				led_red_off();
				r_up_firmware_flag	= 0;
			}
			else 
			{
				led_red_on();
				r_up_firmware_flag	= 1;
			}
		}
	}

	else if (g_led_r_state == 2)
	{
#if 0

		if (g_led_count < 384)
			led_red_on();
		else 
			led_red_off();

#endif

		if (g_led_count % 128 == 0)
		{
			if (r_flash_flag == 0)
			{
				led_red_off();
				r_flash_flag		= 1;
			}
			else 
			{
				led_red_on();
				r_flash_flag		= 0;
			}
		}
	}
	else if (g_led_r_state == 3)
	{
		if (g_led_count % 32 == 0)
		{
			if (r_flash_flag == 0)
			{
				led_red_on();
				r_flash_flag		= 1;
			}
			else 
			{
				led_red_off();
				r_flash_flag		= 0;
			}


		}
	}

	//	ap_peripheral_key_judge();
	// msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_SINGLE_JUGE, &type, sizeof(INT8U), MSG_PRI_NORMAL);
}



#if C_MOTION_DETECTION			== CUSTOM_ON


void ap_peripheral_motion_detect_judge(void)
{
	INT32U			result;

	//DBG_PRINT("-\r\n");//fan
	if (video_down_flag)
		return;

	result				= hwCdsp_MD_get_result();

	//DBG_PRINT("MD_result = 0x%x\r\n",result);
	if (result > 0x30)
	{
		msgQSend(ApQ, MSG_APQ_MOTION_DETECT_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
	}
}


void ap_peripheral_motion_detect_start(void)
{
	motion_detect_status_set(MOTION_DETECT_STATUS_START);
}


void ap_peripheral_motion_detect_stop(void)
{
	motion_detect_status_set(MOTION_DETECT_STATUS_STOP);
}


#endif

#if USE_ADKEY_NO


void ap_peripheral_ad_detect_init(INT8U adc_channel, void(*ad_detect_isr) (INT16U data))
{
#if C_BATTERY_DETECT				== CUSTOM_ON
	battery_value_sum	= 0;
	bat_ck_cnt			= 0;
#endif

	//	ad_value_cnt = 0;
	adc_init();
	adc_vref_enable_set(TRUE);
	adc_conv_time_sel(4);
	adc_manual_ch_set(adc_channel);
	adc_manual_callback_set(ad_detect_isr);

	if (ad_detect_timerid == 0xFF)
	{
		ad_detect_timerid	= AD_DETECT_TIMER_ID;
		sys_set_timer((void *) msgQSend, (void *) PeripheralTaskQ, MSG_PERIPHERAL_TASK_AD_DETECT_CHECK,
			 ad_detect_timerid, PERI_TIME_INTERVAL_AD_DETECT);
	}
}


void ap_peripheral_ad_check_isr(INT16U value)
{
	ad_value			= value;
}


INT16U adc_key_release_calibration(INT16U value)
{
	return value;
}


void ap_peripheral_clr_screen_saver_timer(void)
{
	key_active_cnt		= 0;
}


#if 0 //(KEY_TYPE == KEY_TYPE1)||(KEY_TYPE==KEY_TYPE2)||(KEY_TYPE==KEY_TYPE3)||(KEY_TYPE==KEY_TYPE4)||(KEY_TYPE==KEY_TYPE5)


#else

/*
	0.41v => 495
	0.39v =>
	0.38v => 460
	0.37v => 
	0.36v => 440
	0.35v =>
	0.34v =>
*/
enum 
{
BATTERY_CNT = 64, 
BATTERY_Lv3 = 2200 * BATTERY_CNT, 
BATTERY_Lv2 = 2175 * BATTERY_CNT, 
BATTERY_Lv1 = 2155 * BATTERY_CNT, 
BATTERY_Lv0 = 2135 * BATTERY_CNT
};


static INT32U	ad_time_stamp;


//#define SA_TIME	50	//seconds, for screen saver time. Temporary use "define" before set in "STATE_SETTING".
void ap_peripheral_ad_key_judge(void)
{

	INT32U			t;

	t					= OSTimeGet();

	if ((t - ad_time_stamp) < 2)
	{
		return;
	}

	ad_time_stamp		= t;

	adc_manual_ch_set(AD_KEY_DETECT_PIN);

	//DBG_PRINT("%d, ",(ad_value>>4));
#if C_BATTERY_DETECT				== CUSTOM_ON
	ap_peripheral_battery_check_calculate();
#endif

	adc_manual_sample_start();

}


#endif // AD-Key

#endif

#if C_BATTERY_DETECT			== CUSTOM_ON

INT32U			previous_direction = 0;
extern void ap_state_handling_led_off(void);
extern INT8U	display_str_battery_low;


#define BATTERY_GAP 			10*BATTERY_CNT


static INT8U ap_peripheral_smith_trigger_battery_level(INT32U direction)
{
	static INT8U	bat_lvl_cal_bak = (INT8U)

	BATTERY_Lv3;
	INT8U			bat_lvl_cal;

	// DBG_PRINT("(%d)\r\n", battery_value_sum);
	if (battery_value_sum >= BATTERY_Lv3)
	{
		bat_lvl_cal 		= 4;
	}
	else if ((battery_value_sum < BATTERY_Lv3) && (battery_value_sum >= BATTERY_Lv2))
	{
		bat_lvl_cal 		= 3;
	}
	else if ((battery_value_sum < BATTERY_Lv2) && (battery_value_sum >= BATTERY_Lv1))
	{
		bat_lvl_cal 		= 2;
	}
	else if ((battery_value_sum < BATTERY_Lv1) && (battery_value_sum >= BATTERY_Lv0))
	{
		bat_lvl_cal 		= 1;
	}
	else if (battery_value_sum < BATTERY_Lv0)
		bat_lvl_cal = 0;

	if ((direction == 0) && (bat_lvl_cal > bat_lvl_cal_bak))
	{
		if (battery_value_sum >= BATTERY_Lv3 + BATTERY_GAP)
		{
			bat_lvl_cal 		= 4;
		}
		else if ((battery_value_sum < BATTERY_Lv3 + BATTERY_GAP) && (battery_value_sum >= BATTERY_Lv2 + BATTERY_GAP))
		{
			bat_lvl_cal 		= 3;
		}
		else if ((battery_value_sum < BATTERY_Lv2 + BATTERY_GAP) && (battery_value_sum >= BATTERY_Lv1 + BATTERY_GAP))
		{
			bat_lvl_cal 		= 2;
		}
		else if ((battery_value_sum < BATTERY_Lv1 + BATTERY_GAP) && (battery_value_sum >= BATTERY_Lv0 + BATTERY_GAP))
		{
			bat_lvl_cal 		= 1;
		}
		else if (battery_value_sum < BATTERY_Lv0 + BATTERY_GAP)
			bat_lvl_cal = 0;
	}


	if ((direction == 1) && (bat_lvl_cal < bat_lvl_cal_bak))
	{
		if (battery_value_sum >= BATTERY_Lv3 - BATTERY_GAP)
		{
			bat_lvl_cal 		= 4;
		}
		else if ((battery_value_sum < BATTERY_Lv3 - BATTERY_GAP) && (battery_value_sum >= BATTERY_Lv2 - BATTERY_GAP))
		{
			bat_lvl_cal 		= 3;
		}
		else if ((battery_value_sum < BATTERY_Lv2 - BATTERY_GAP) && (battery_value_sum >= BATTERY_Lv1 - BATTERY_GAP))
		{
			bat_lvl_cal 		= 2;
		}
		else if ((battery_value_sum < BATTERY_Lv1 - BATTERY_GAP) && (battery_value_sum >= BATTERY_Lv0 - BATTERY_GAP))
		{
			bat_lvl_cal 		= 1;
		}
		else if (battery_value_sum < BATTERY_Lv0 - BATTERY_GAP)
			bat_lvl_cal = 0;
	}


	bat_lvl_cal_bak 	= bat_lvl_cal;
	return bat_lvl_cal;

}


void ap_peripheral_battery_check_calculate(void)
{
	INT8U			bat_lvl_cal;
	INT32U			direction = 0, led_type;

	if (adp_status == 0)
	{
		//unkown state
		return;
	}
	else if (adp_status == 1)
	{
		//adaptor in state
		direction			= 1;					//low voltage to high voltage

		if (previous_direction != direction)
		{
			//msgQSend(ApQ, MSG_APQ_BATTERY_CHARGED_SHOW, NULL, NULL, MSG_PRI_NORMAL);
		}

		previous_direction	= direction;
	}
	else 
	{
		//adaptor out state
		direction			= 0;					//high voltage to low voltage

		if (previous_direction != direction)
		{
			//msgQSend(ApQ, MSG_APQ_BATTERY_CHARGED_CLEAR, NULL, NULL, MSG_PRI_NORMAL);
		}

		previous_direction	= direction;
	}

	battery_value_sum	+= (ad_value >> 4);

	// DBG_PRINT("%d, ",(ad_value>>4));
	bat_ck_cnt++;

	if (bat_ck_cnt >= BATTERY_CNT)
	{
		DBG_PRINT("[%d]\r\n", (ad_value >> 4));
		bat_lvl_cal 		= ap_peripheral_smith_trigger_battery_level(direction);

		// DBG_PRINT("b:[%d],", bat_lvl_cal);
		if (!battery_low_flag)
		{
			msgQSend(ApQ, MSG_APQ_BATTERY_LVL_SHOW, &bat_lvl_cal, sizeof(INT8U), MSG_PRI_NORMAL);
		}

		if ((bat_lvl_cal == 0) && (direction == 0))
		{
			low_voltage_cnt++;

			if (low_voltage_cnt > 3)
			{
				low_voltage_cnt 	= 0;
				ap_state_handling_led_off();

#if C_BATTERY_LOW_POWER_OFF 					== CUSTOM_ON

				if (!battery_low_flag)
				{
					battery_low_flag	= 1;
					{
						INT8U			type;

						msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_STOP, NULL, NULL, MSG_PRI_NORMAL);
						type				= FALSE;
						msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK_SWITCH, &type, sizeof(INT8U),
							 MSG_PRI_NORMAL);
						type				= BETTERY_LOW_STATUS_KEY;
						msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_KEY_REGISTER, &type, sizeof(INT8U),
							 MSG_PRI_NORMAL);

						//msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
						//DBG_PRINT("fankun3\r\n");
						led_type			= LED_LOW_BATT;
						msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U),
							 MSG_PRI_NORMAL);

						//msgQSend(ApQ, MSG_APQ_BATTERY_LOW_SHOW, NULL, sizeof(INT8U), MSG_PRI_NORMAL);
					}
				}

#endif
				//OSTimeDly(100);
				//display_str_battery_low = 1;
				//msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
			}
		}
		else 
		{
			if (battery_low_flag)
			{
				INT8U			type;

				battery_low_flag	= 0;
				msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
				type				= GENERAL_KEY;
				msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_KEY_REGISTER, &type, sizeof(INT8U), MSG_PRI_NORMAL);
				led_type			= PREV_LED_TYPE;
				msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
			}

			low_voltage_cnt 	= 0;
		}

		bat_ck_cnt			= 0;
		battery_value_sum	= 0;
	}
}


#endif






void ap_peripheral_night_mode_set(INT8U type)
{
#if 0

	if (type)
	{
		gpio_write_io(IR_CTRL, 1);
	}
	else 
	{
		gpio_write_io(IR_CTRL, 0);
	}

#endif
}


void ap_peripheral_key_init(void)
{
	INT32U			i;

	gp_memset((INT8S *) &key_map, NULL, sizeof(KEYSTATUS));
	ap_peripheral_key_register(GENERAL_KEY);

#if 1

	for (i = 0; i < USE_IOKEY_NO; i++)
	{
		//	DBG_PRINT("key_map:%d\r\n",key_map[i].key_io);
		if (key_map[i].key_io != PW_KEY)
		{
			key_map[i].key_cnt	= 0;
			gpio_init_io(key_map[i].key_io, GPIO_INPUT);
			gpio_set_port_attribute(key_map[i].key_io, ATTRIBUTE_LOW);
			gpio_write_io(key_map[i].key_io, (key_map[i].key_active) ^ 1);

			// DBG_PRINT("INIT\r\n");
		}
	}

#endif


	gpio_init_io(KEY_DET, GPIO_INPUT);
	gpio_set_port_attribute(KEY_DET, ATTRIBUTE_LOW);
	gpio_write_io(KEY_DET, 0);

	//gpio_init_io(SWITCH_KEY, GPIO_INPUT);		
	//gpio_set_port_attribute(SWITCH_KEY, ATTRIBUTE_LOW);	
	//gpio_write_io(SWITCH_KEY, 1);
	//switch_key_state=gpio_read_io(SWITCH_KEY);
}


void ap_peripheral_time_set(void)
{
#if 0
	TIME_T			READ_TIME;

	DBG_PRINT("TIME_SET1\r\n");
	ap_state_config_timefile_get(&READ_TIME);
	Time_card_storage(&READ_TIME);

#if USING_EXT_RTC					== CUSTOM_ON
	ap_state_handling_calendar_init();

#else

	ap_state_handing_intime_init(READ_TIME);

#endif

#endif

}


void ap_peripheral_key_register(INT8U type)
{
	INT32U			i;

	if (type == GENERAL_KEY)
	{
		key_map[0].key_io	= PW_KEY;
		key_map[0].key_function = (KEYFUNC)ap_peripheral_pw_key_exe;
		key_map[0].key_active = 1;
		key_map[0].key_long = 0;



	}
	else if (type == USBD_DETECT)
	{
#if USE_IOKEY_NO

		for (i = 0; i < USE_IOKEY_NO; i++)
		{
			if ((key_map[i].key_io != PW_KEY) && (key_map[i].key_io != FUN_KEY))
				key_map[i].key_io = NULL;
		}

#endif

#if USE_ADKEY_NO

		for (i = 0; i < USE_ADKEY_NO; i++)
		{
			ad_key_map[i].key_function = ap_peripheral_null_key_exe;
		}

#endif
	}
	else if (type == DISABLE_KEY)
	{
#if USE_IOKEY_NO

		for (i = 0; i < USE_IOKEY_NO; i++)
		{
			key_map[i].key_io	= NULL;
		}

#endif

#if USE_ADKEY_NO

		for (i = 0; i < USE_ADKEY_NO; i++)
		{
			ad_key_map[i].key_function = ap_peripheral_null_key_exe;
		}

#endif
	}
	else if (type == BETTERY_LOW_STATUS_KEY)
	{
		key_map[0].key_io	= PW_KEY;
		key_map[0].key_function = (KEYFUNC)
		ap_peripheral_pw_key_exe;

#if USE_ADKEY_NO

		for (i = 0; i < USE_ADKEY_NO; i++)
		{
			ad_key_map[i].key_function = ap_peripheral_null_key_exe;
		}

#endif
	}

}


extern INT8U ap_state_config_auto_off_get(void);

INT8U			long_pw_key_pressed = 0;

#if CRAZY_KEY_TEST				== 1
INT8U			crazy_key_enable = 0;
INT32U			crazy_key_cnt = 0;

#endif

//INT8U led_red_prev_on_flag=0;
//static INT8U short_pw_flag=0;
//INT8U short_pw_cnt=0;
//extern volatile INT8U poweon_rec_flag;
extern INT8U	usb_det_flag;
INT8U			switch_det_cnt = 0;
static INT8U	current_switch_state = 0;
static INT8U	prev_switch_state = 0x55;
void ap_peripheral_switch_0(void);
void ap_peripheral_switch_1(void);
void ap_peripheral_switch_2(void);


void ap_peripheral_key_judge(void)
{
	INT32U			i, key_press = 0;
	INT16U			key_down = 0, key1_down = 0;
	INT32U			led_type;

	if (usb_det_flag)
	{
		if (++switch_det_cnt > 32)
		{
			switch_det_cnt		= 0;

			if (gpio_read_io(KEY_DET))
			{
				current_switch_state = 2;
			}
			else 
			{
				if (sys_pwr_key0_read())
					current_switch_state = 1;
				else 
					current_switch_state = 0;
			}


			if (current_switch_state != prev_switch_state)
			{
				if (current_switch_state == 0)
				{
					DBG_PRINT("jh_0\r\n");
					ap_peripheral_switch_0();
				}
				else if (current_switch_state == 1)
				{
					DBG_PRINT("jh_1\r\n");
					ap_peripheral_switch_1();
				}
				else 
				{
					DBG_PRINT("jh_2\r\n");
					ap_peripheral_switch_2();
				}

				prev_switch_state	= current_switch_state;
			}
		}
	}

	if (video_record_sts & 0x04)
		gpio_write_io(IR_CTRL, 1);
	else 
		gpio_write_io(IR_CTRL, 0);

#if 0

	for (i = 0; i < USE_IOKEY_NO; i++)
	{
		if (key_map[i].key_io) //
		{

			if (key_map[i].key_io == PW_KEY) //
			{
				key_down			= sys_pwr_key0_read();

			}
			else 
			{
				if (key_map[i].key_active)
					key_down = gpio_read_io(key_map[i].key_io);
				else 
					key_down = !gpio_read_io(key_map[i].key_io);
			}

			if (key_down)
			{
				power_off_cnt		= PERI_POWER_OFF;

				if (!key_map[i].key_long)
				{

					key_map[i].key_cnt	+= 1;

#if SUPPORT_LONGKEY 								== CUSTOM_ON

					if (key_map[i].key_cnt >= Long_Single_width)
					{
						key_map[i].key_long = 1;

						if (power_keyup == 0)
							key_map[i].key_function(& (key_map[i].key_cnt));



					}

#endif
				}
				else 
				{
					key_map[i].key_cnt	= 0;
				}

				if (key_map[i].key_cnt == 65535)
				{
					key_map[i].key_cnt	= 16;
				}
			}
			else 
			{
				if (key_map[i].key_long)
				{
					key_map[i].key_long = 0;
					key_map[i].key_cnt	= 0;

				}

				if (key_map[i].key_cnt >= Short_Single_width) //Short_Single_width
				{

					if (power_keyup == 0)
					{

						key_map[i].key_function(& (key_map[i].key_cnt));

					}

					key_press			= 1;
				}

				key_map[i].key_cnt	= 0;

				if (key_map[i].key_io == PW_KEY)
					power_keyup = 0;


			}
		}
	}

#endif

#if 0

	if (short_pw_flag)
	{
		short_pw_cnt++;

		if (short_pw_cnt >= 64) //1s
		{
			if (short_pw_flag == 2)
			{
				key_map[0].key_cnt	= 24;			//24;
				key_map[0].key_function(& (key_map[0].key_cnt));
				key_map[0].key_cnt	= 0;
			}
			else if (short_pw_flag == 1)
			{
				key_map[0].key_cnt	= 12;			//48;
				key_map[0].key_function(& (key_map[0].key_cnt));
				key_map[0].key_cnt	= 0;
			}

			short_pw_flag		= 0;
			short_pw_cnt		= 0;
		}
	}

#endif

#if 1

	if (!auto_off_force_disable && (!s_usbd_pin) && (! (usb_state_get())))
	{
		if (power_off_cnt)
		{
			power_off_cnt--;

			if (power_off_cnt <= 0)
			{
				DBG_PRINT("fankun2\r\n");
				msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
			}
		}
	}
	else 
		power_off_cnt = PERI_POWER_OFF;

#endif



}


void ap_peripheral_charge_det(void)
{
	INT16U			pin_state = 0;
	static INT16U	prev_pin_state = 0;
	INT16U			led_type;
	static INT8U	loop_cnt = 0;
	static INT16U	prev_ledtype = 0;

	pin_state			= gpio_read_io(CHARGE_PIN);
	DBG_PRINT("pin_state=%d", pin_state);

	if (pin_state == prev_pin_state)
	{
		loop_cnt++;
	}
	else 
		loop_cnt = 0;

	if (loop_cnt >= 3)
	{


		loop_cnt			= 3;

		if (pin_state)
			led_type = LED_CHARGE_FULL;
		else 
			led_type = LED_CHARGEING;

		if (prev_ledtype != led_type)
		{
			prev_ledtype		= led_type;

			if (((video_record_sts & 0x06) == 0))
			{
				if (storage_sd_upgrade_file_flag_get() != 2)
					msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
			}

			//msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
		}
	}

	prev_pin_state		= pin_state;
}


static int ap_peripheral_power_key_read(int pin)
{
	int 			status;


#if (								KEY_TYPE == KEY_TYPE1)||(KEY_TYPE == KEY_TYPE2)||(KEY_TYPE == KEY_TYPE3)||(KEY_TYPE == KEY_TYPE4)||(KEY_TYPE == KEY_TYPE5)
	status				= gpio_read_io(pin);

#else

	switch (pin)
	{
		case PWR_KEY0:
			status = sys_pwr_key0_read();
			break;

		case PWR_KEY1:
			status = sys_pwr_key1_read();
			break;
	}

#endif

	if (status != 0)
		return 1;

	else 
		return 0;
}


void ap_peripheral_adaptor_out_judge(void)
{
	//DBG_PRINT("NA");
	adp_out_cnt++;

	switch (adp_status)
	{
		case 0: //unkown state
			if (ap_peripheral_power_key_read(ADP_OUT_PIN))
			{
				//DBG_PRINT("xx:%d",adp_cnt);
				adp_cnt++;

				if (adp_cnt > 16)
				{
					adp_out_cnt 		= 0;
					adp_cnt 			= 0;
					adp_status			= 1;
					OSQPost(USBTaskQ, (void *) MSG_USBD_INITIAL);

#if C_BATTERY_DETECT								== CUSTOM_ON && USE_ADKEY_NO

					//battery_lvl = 1;
#endif
				}
			}
			else 
			{
				adp_cnt 			= 0;
			}

			//	DBG_PRINT("yy:%d\r\n",adp_out_cnt);
			if (adp_out_cnt > 24)
			{
				adp_out_cnt 		= 0;
				adp_status			= 3;

#if C_BATTERY_DETECT							== CUSTOM_ON && USE_ADKEY_NO

				//battery_lvl = 2;
				low_voltage_cnt 	= 0;
#endif
			}

			break;

		case 1: //adaptor in state
			if (!ap_peripheral_power_key_read(ADP_OUT_PIN))
			{
				if (adp_out_cnt > 8)
				{
					adp_status			= 2;

#if C_BATTERY_DETECT								== CUSTOM_ON
					low_voltage_cnt 	= 0;
#endif

					// 若螢幕保護開時，要點亮背光
					if (screen_saver_enable)
					{
						screen_saver_enable = 0;

#if C_SCREEN_SAVER										== CUSTOM_ON
						ap_state_handling_lcd_backlight_switch(1);
#endif
					}
				}
			}
			else 
			{
				adp_out_cnt 		= 0;
			}

			break;

		case 2: //adaptor out state
			if (!ap_peripheral_power_key_read(ADP_OUT_PIN))
			{
				if ((adp_out_cnt > PERI_ADP_OUT_PWR_OFF_TIME))
				{
					//DBG_PRINT("1111111111111111111\r\n");
					//adp_out_cnt=320;
#if 1

					//ap_peripheral_pw_key_exe(&adp_out_cnt);
#endif

					adp_out_cnt 		= 0;
					usb_state_set(0);
					msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
				}

				adp_cnt 			= 0;
			}
			else 
			{
				adp_cnt++;

				if (adp_cnt > 3)
				{
					adp_out_cnt 		= 0;
					adp_status			= 1;
					usbd_exit			= 0;
					OSQPost(USBTaskQ, (void *) MSG_USBD_INITIAL);
				}
			}

			break;

		case 3: //adaptor initial out state
			if (ap_peripheral_power_key_read(ADP_OUT_PIN))
			{
				if (adp_out_cnt > 3)
				{
					adp_out_cnt 		= 0;
					adp_status			= 1;
					OSQPost(USBTaskQ, (void *) MSG_USBD_INITIAL);
				}
			}
			else 
			{
				if (usb_state_get())
					usb_state_set(0);

				adp_out_cnt 		= 0;
			}

			break;

		default:
			break;
	}

	///DBG_PRINT("USB_PIN=%d\r\n",s_usbd_pin);
	if (s_usbd_pin == 1)
	{
		usbd_cnt++;

		if (!ap_peripheral_power_key_read(C_USBDEVICE_PIN))
		{
			if (usbd_cnt > 3)
			{
				ap_peripheral_usbd_plug_out_exe(&usbd_cnt);
			}
		}
		else 
		{
			usbd_cnt			= 0;
		}
	}

#if USB_PHY_SUSPEND 				== 1

	if (s_usbd_pin == 0)
	{
		if (ap_peripheral_power_key_read(C_USBDEVICE_PIN))
		{
			if (phy_cnt == PERI_USB_PHY_SUSPEND_TIME)
			{
				// disable USB PHY CLK for saving power
				DBG_PRINT("Turn Off USB PHY clk (TODO)\r\n");
				phy_cnt++;							// 目的是 Turn Off 只做一次
			}
			else if (phy_cnt < PERI_USB_PHY_SUSPEND_TIME)
			{
				phy_cnt++;
			}
		}
		else 
		{
			phy_cnt 			= 0;
		}
	}

#endif

}


void ap_peripheral_switch_0(void)
{
	INT32U led_type;


	if (!usb_state_get())
	{
		msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
	}
	else if (!s_usbd_pin)
	{
		if (video_record_sts & 0x02)
		{
			msgQSend(ApQ, MSG_APQ_VIDEO_RECORD_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
		}
		else if (video_record_sts & 0x04)
		{
			ap_video_record_md_disable();
			led_type			= LED_WAITING_RECORD;
			msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);

		}
	}

}


void ap_peripheral_switch_1(void)
{
	INT8U i;

	if (!s_usbd_pin)
	{
		if ((ap_state_handling_storage_id_get() != NO_STORAGE) && (!card_space_less_flag) && (!video_down_flag))
		{
			if ((video_record_sts & 0x02) == 0)
			{
				for (i = 0; i < 3; i++)
				{
					led_green_off();
					OSTimeDly(15);
					led_green_on();
					OSTimeDly(15);
				}

				if (video_record_sts & 0x04)
				{
					ap_video_record_md_disable();
					ap_state_config_md_set(0);
				}

				msgQSend(ApQ, MSG_APQ_VIDEO_RECORD_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
			}
			else 
			{
				if (video_record_sts & 0x04)
				{
					msgQSend(ApQ, MSG_APQ_VIDEO_RECORD_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);

					for (i = 0; i < 3; i++)
					{
						led_green_off();
						OSTimeDly(15);
						led_green_on();
						OSTimeDly(15);
					}

					msgQSend(ApQ, MSG_APQ_VIDEO_RECORD_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
				}
			}
		}
	}

}


void ap_peripheral_switch_2(void)
{

	INT8U i;

	if (!s_usbd_pin)
	{
		if ((ap_state_handling_storage_id_get() != NO_STORAGE) && (!card_space_less_flag) && (!video_down_flag))
		{
			if ((video_record_sts & 0x04) == 0)
			{
				if (video_record_sts & 0x02)
					msgQSend(ApQ, MSG_APQ_VIDEO_RECORD_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);

				for (i = 0; i < 5; i++)
				{
					led_green_off();
					OSTimeDly(15);
					led_green_on();
					OSTimeDly(15);
				}

				ap_state_config_md_set(1);
				msgQSend(ApQ, MSG_APQ_VIDEO_RECORD_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
			}
			else 
			{
			}
		}
	}

}



void ap_peripheral_function_key_exe(INT16U * tick_cnt_ptr)
{

	*tick_cnt_ptr		= 0;
}


void ap_peripheral_next_key_exe(INT16U * tick_cnt_ptr)
{

	*tick_cnt_ptr		= 0;
}


void ap_peripheral_prev_key_exe(INT16U * tick_cnt_ptr)
{

	*tick_cnt_ptr		= 0;
}


void ap_peripheral_ok_key_exe(INT16U * tick_cnt_ptr)
{

	*tick_cnt_ptr		= 0;
}


#if KEY_FUNTION_TYPE			== SAMPLE2

void ap_peripheral_capture_key_exe(INT16U * tick_cnt_ptr)
{

	*tick_cnt_ptr		= 0;
}


#endif

void ap_peripheral_sos_key_exe(INT16U * tick_cnt_ptr)
{

	*tick_cnt_ptr		= 0;
}


void ap_peripheral_usbd_plug_out_exe(INT16U * tick_cnt_ptr)
{
	msgQSend(ApQ, MSG_APQ_DISCONNECT_TO_PC, NULL, NULL, MSG_PRI_NORMAL);
	*tick_cnt_ptr		= 0;
}


void ap_peripheral_pw_key_exe(INT16U * tick_cnt_ptr)
{


	*tick_cnt_ptr		= 0;
}


void ap_peripheral_video_prev_exe(INT16U * tick_cnt_ptr)
{

	*tick_cnt_ptr		= 0;

}


void ap_peripheral_modection_exe(INT16U * tick_cnt_ptr)
{

	*tick_cnt_ptr		= 0;

}


void ap_peripheral_ir_change_exe(INT16U * tick_cnt_ptr)
{


}


void ap_peripheral_menu_key_exe(INT16U * tick_cnt_ptr)
{

	*tick_cnt_ptr		= 0;
}


void ap_peripheral_null_key_exe(INT16U * tick_cnt_ptr)
{

}


#if GPDV_BOARD_VERSION			!= GPCV1237A_Aerial_Photo
void ap_TFT_backlight_tmr_check(void)
{
	if (backlight_tmr)
	{
		backlight_tmr--;

		if ((backlight_tmr == 0) && (tv == !TV_DET_ACTIVE))
		{
			//gpio_write_io(TFT_BL, DATA_HIGH);	//turn on LCD backlight
#if _DRV_L1_TFT 							== 1
			tft_backlight_en_set(1);
#endif
		}
	}
}


#endif

//+++ TV_OUT_D1
#if TV_DET_ENABLE
INT8U tv_plug_status_get(void)
{
	return tv_plug_in_flag;
}


#endif

//---
#ifdef SDC_DETECT_PIN
void ap_peripheral_SDC_detect_init(void)
{

	gpio_init_io(SDC_DETECT_PIN, GPIO_INPUT);
	gpio_set_port_attribute(SDC_DETECT_PIN, ATTRIBUTE_LOW);
	gpio_write_io(SDC_DETECT_PIN, 1);				//pull high
}


INT32S ap_peripheral_SDC_at_plug_OUT_detect()
{
	INT32S ret;
	BOOLEAN cur_status;
	ap_peripheral_SDC_detect_init();
	cur_status			= gpio_read_io(SDC_DETECT_PIN);
	DBG_PRINT("SDC_DETECT_PIN_=%d\r\n", cur_status);

	if (cur_status)
	{ //plug_out
		ret 				= -1;
	}
	else 
	{ //plug_in	
		ret 				= 0;
	}

	return ret;
}



INT32S ap_peripheral_SDC_at_plug_IN_detect()
{
	INT32S ret;
	BOOLEAN cur_status;
	ap_peripheral_SDC_detect_init();
	cur_status			= gpio_read_io(SDC_DETECT_PIN);
	DBG_PRINT("SDC_DETECT_PIN=%d\r\n", cur_status);

	if (cur_status)
	{ //plug_out
		ret 				= -1;
	}
	else 
	{ //plug_in
		ret 				= 0;
	}

	return ret;
}


#endif

