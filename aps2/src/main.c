/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include <asf.h>
#include <string.h>
#include "ili9341.h"
#include "lvgl.h"
#include "touch/touch.h"

LV_FONT_DECLARE(dseg70);
LV_FONT_DECLARE(dseg40);
LV_FONT_DECLARE(dseg25);
LV_FONT_DECLARE(dseg15);
LV_FONT_DECLARE(dseg10);
LV_FONT_DECLARE(monts15);
LV_FONT_DECLARE(monts10);
LV_IMG_DECLARE(clock);
LV_IMG_DECLARE(fumaca);
/************************************************************************/
/* LCD / LVGL                                                           */
/************************************************************************/

#define LV_HOR_RES_MAX          (240)
#define LV_VER_RES_MAX          (320)

/************************************************************************/
/* MAG-NET*/

#define MAGNET_PIO		   PIOA
#define MAGNET_PIO_ID	   ID_PIOA
#define MAGNET_PIO_IDX	   11 // alterar para 19 quando usar o MAG-NET ou para 11 quando usar o botao
#define MAGNET_PIO_IDX_MASK (1 << MAGNET_PIO_IDX)

static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf_1[LV_HOR_RES_MAX * LV_VER_RES_MAX];
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

static  lv_obj_t * labelBtn1;
static  lv_obj_t * labelBtn2;
static  lv_obj_t * labelBtn3;
static  lv_obj_t * labelBtn4;
static  lv_obj_t * labelBtn5;
static  lv_obj_t * labelBtn6;
static  lv_obj_t * labelBtn7;
static  lv_obj_t * labelBtn8;
lv_obj_t * labelFloor;
lv_obj_t * labelFloorDigit;
lv_obj_t * labelSetValue;
lv_obj_t * labelClock;
lv_obj_t * labelVelocidade;
lv_obj_t * labelVelEscrita;
lv_obj_t * labelAcelEscrita;
lv_obj_t * labelVelMedEscrita;
lv_obj_t * labelTempoEscrita;
lv_obj_t * labelDistanciaEscrita;
lv_obj_t * labelChave;
lv_obj_t * labelHome;
lv_obj_t * labelCelsius;
lv_obj_t * labelCelsius2;
lv_obj_t * labelConfig;
lv_obj_t * labelSet;
lv_obj_t * labelClock2;
lv_obj_t * labelFumaca;
lv_obj_t * labelDia;
lv_obj_t * labelFT;
lv_obj_t * labelFT2;
lv_obj_t * scr1;
lv_obj_t * scr2;
lv_obj_t * labelAceleracao;
lv_obj_t * labelVelUnity;
lv_obj_t * labelAcelUnity;
lv_obj_t * labelTempo;
lv_obj_t * labelDistancia;
lv_obj_t * labelTempoUnity;
lv_obj_t * labelDistanciaUnity;
lv_obj_t * labelVelMedia;
lv_obj_t * labelVelMediaUnity;

volatile char setPower;

volatile float diametro_roda = 0.508;

volatile float velocidadeMedia = 0;

volatile float distanciaPercorrida = 0;

volatile uint32_t tempoPercorrido = 0;

volatile uint32_t flagTrajeto = 0;

#include "arm_math.h"

SemaphoreHandle_t xMutexLVGL;

typedef struct  {
  uint32_t year;
  uint32_t month;
  uint32_t day;
  uint32_t week;
  uint32_t hour;
  uint32_t minute;
  uint32_t second;
} calendar;

void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type);

/************************************************************************/
/* RTOS                                                                 */
/************************************************************************/

#define TASK_LCD_STACK_SIZE                (1024*4/sizeof(portSTACK_TYPE))
#define TASK_LCD_STACK_PRIORITY            (tskIDLE_PRIORITY)

#define TASK_MAGNET_STACK_SIZE             (1024*4/sizeof(portSTACK_TYPE))
#define TASK_MAGNET_STACK_PRIORITY         (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,  signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);
extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	for (;;) {	}
}
extern void vApplicationIdleHook(void) { }
extern void vApplicationTickHook(void) { }
extern void vApplicationMallocFailedHook(void) {
	configASSERT( ( volatile void * ) NULL );
}

static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource) {
	uint16_t pllPreScale = (int) (((float) 32768) / freqPrescale);
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);
	if (rttIRQSource & RTT_MR_ALMIEN) {
		uint32_t ul_previous_time;
		ul_previous_time = rtt_read_timer_value(RTT);
		while (ul_previous_time == rtt_read_timer_value(RTT));
		rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);
	}
	/* config NVIC */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);
	/* Enable RTT interrupt */
	if (rttIRQSource & (RTT_MR_RTTINCIEN | RTT_MR_ALMIEN))
	rtt_enable_interrupt(RTT, rttIRQSource);
	else
	rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN | RTT_MR_ALMIEN);
}

/************************************************************************/
/* lvgl                                                                 */
/************************************************************************/

static void event_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	if(code == LV_EVENT_CLICKED) {
		if (setPower == 0){
			setPower = 1;
			lv_scr_load(scr2);
		} else {
			setPower = 0;
			lv_scr_load(scr1);
		}
	}
}

static void but2_callback(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	if(code == LV_EVENT_CLICKED) { LV_LOG_USER("Clicked"); }
	else if(code == LV_EVENT_VALUE_CHANGED) { LV_LOG_USER("Toggled"); }
}

static void but3_callback(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	if(code == LV_EVENT_CLICKED) { LV_LOG_USER("Clicked"); }
	else if(code == LV_EVENT_VALUE_CHANGED) { LV_LOG_USER("Toggled"); }
}

static void down_handler(lv_event_t * e) {
//     lv_event_code_t code = lv_event_get_code(e);
//     char *c;
//     int temp;
//     if(code == LV_EVENT_CLICKED) {
//         c = lv_label_get_text(labelSetValue);
//         temp = atoi(c);
//         lv_label_set_text_fmt(labelSetValue, "%02d", temp - 1);
//     }
}

static void up_handler(lv_event_t * e) {
//     lv_event_code_t code = lv_event_get_code(e);
//     char *c;
//     int temp;
//     if(code == LV_EVENT_CLICKED) {
//         c = lv_label_get_text(labelSetValue);
//         temp = atoi(c);
//         lv_label_set_text_fmt(labelSetValue, "%02d", temp + 1);
//     }
}

static void iniciar_handler(lv_event_t * e) {
	// handler para o botao de iniciar que inicia um trajeto ou pausa um trajeto
	lv_event_code_t code = lv_event_get_code(e);
	if(code == LV_EVENT_CLICKED) {
		if (flagTrajeto == 0){
			flagTrajeto = 1;
			// alterar o texto do botao para pausar
			lv_label_set_text_fmt(labelBtn7, LV_SYMBOL_PAUSE);
		}
		else{
			flagTrajeto = 0;
			// alterar o texto do botao para iniciar
			lv_label_set_text_fmt(labelBtn7, LV_SYMBOL_PLAY);
		}
	}
}

static void reiniciar_handler(lv_event_t * e) {
	// handler para o botao de reiniciar que reinicia um trajeto
	lv_event_code_t code = lv_event_get_code(e);
	if(code == LV_EVENT_CLICKED) {
		// reinicia as variaveis de trajeto
		velocidadeMedia = 0;
		distanciaPercorrida = 0;
		tempoPercorrido = 0;
		// atualiza os labels
		// mutex
		lv_label_set_text_fmt(labelVelMedia, "%02d", velocidadeMedia);
		lv_label_set_text_fmt(labelDistancia, "%02d", distanciaPercorrida);
		lv_label_set_text_fmt(labelTempo, "%02d:%02d:%02d", tempoPercorrido/3600, (tempoPercorrido%3600)/60, tempoPercorrido%60);

	}
}

void lv_termostato(void) {
	static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, lv_color_black());
    lv_style_set_border_color(&style, lv_color_black());
    lv_style_set_border_width(&style, 5);
	
	scr1 = lv_obj_create(NULL);
	scr2 = lv_obj_create(NULL);
	
	lv_scr_load(scr1);
	
    lv_obj_t * btn1 = lv_btn_create(scr1);
    lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn1, LV_ALIGN_TOP_LEFT, 3, -3);
    labelBtn1 = lv_label_create(btn1);
	lv_label_set_text(labelBtn1, LV_SYMBOL_POWER);
    lv_obj_center(labelBtn1);
	lv_obj_add_style(btn1, &style, 0);
	
	lv_obj_t * btn6 = lv_btn_create(scr2);
	lv_obj_add_event_cb(btn6, event_handler, LV_EVENT_ALL, NULL);
	lv_obj_align(btn6, LV_ALIGN_TOP_LEFT, 10, -10);
	labelBtn6 = lv_label_create(btn6);
	lv_label_set_text(labelBtn6, LV_SYMBOL_POWER);
	lv_obj_center(labelBtn6);
	lv_obj_add_style(btn6, &style, 0);

	lv_obj_t * btn7 = lv_btn_create(scr1);
	lv_obj_add_event_cb(btn7, iniciar_handler, LV_EVENT_ALL, NULL);
	// alinha no canto inferior esquerdo
	lv_obj_align(btn7, LV_ALIGN_BOTTOM_LEFT, 10, -10);
	labelBtn7 = lv_label_create(btn7);
	lv_label_set_text(labelBtn7, LV_SYMBOL_PLAY);
	lv_obj_center(labelBtn7);
	lv_obj_add_style(btn7, &style, 0);

	lv_obj_t * btn8 = lv_btn_create(scr1);
	lv_obj_add_event_cb(btn8, reiniciar_handler, LV_EVENT_ALL, NULL);
	// alinha no canto inferior direito
	lv_obj_align(btn8, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
	labelBtn8 = lv_label_create(btn8);
	lv_label_set_text(labelBtn8, LV_SYMBOL_REFRESH);
	lv_obj_center(labelBtn8);
	lv_obj_add_style(btn8, &style, 0);


// 	lv_obj_t * btn2 = lv_btn_create(scr1);
//     lv_obj_add_event_cb(btn2, but2_callback, LV_EVENT_ALL, NULL);
// 	lv_obj_align_to(btn2, btn1, LV_ALIGN_OUT_RIGHT_TOP, -5, 0);
//     labelBtn2 = lv_label_create(btn2);
// 	lv_label_set_text(labelBtn2, "| M |" );
//     lv_obj_center(labelBtn2);
// 	lv_obj_add_style(btn2, &style, 0);
// 
// 	lv_obj_t * btn3 = lv_imgbtn_create(scr1);
// 	lv_imgbtn_set_src(btn3, LV_IMGBTN_STATE_RELEASED, 0, &clock, 0);
// 	lv_obj_add_flag(btn3, LV_OBJ_FLAG_CHECKABLE);
// 	lv_obj_add_event_cb(btn3, but3_callback, LV_EVENT_ALL, NULL);
// 	lv_obj_set_height(btn3, 25);
// 	lv_obj_set_width(btn3, 25);
// 	lv_obj_align_to(btn3, btn2, LV_ALIGN_OUT_RIGHT_TOP, 3, 9);
// 
// 	labelChave = lv_label_create(scr1);
// 	lv_obj_align_to(labelChave, btn3, LV_ALIGN_OUT_RIGHT_MID, 0, -3);
// 	lv_obj_set_style_text_color(labelChave, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelChave, "  ]");
// 
// 	labelHome = lv_label_create(scr1);
// 	lv_obj_align_to(labelHome, labelChave, LV_ALIGN_OUT_RIGHT_MID, 0, -30);
// 	lv_obj_set_style_text_color(labelHome, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelHome, LV_SYMBOL_HOME"");
// 
// 	lv_obj_t * btn4 = lv_btn_create(scr1);
//     lv_obj_add_event_cb(btn4, up_handler, LV_EVENT_ALL, NULL);
// 	lv_obj_align_to(btn4, labelChave, LV_ALIGN_OUT_RIGHT_TOP, 20, -7);
//     labelBtn4 = lv_label_create(btn4);
// 	lv_label_set_text(labelBtn4, "[ "LV_SYMBOL_UP"  |");
//     lv_obj_center(labelBtn4);
// 	lv_obj_add_style(btn4, &style, 0);
// 
// 	lv_obj_t * btn5 = lv_btn_create(scr1);
//     lv_obj_add_event_cb(btn5, down_handler, LV_EVENT_ALL, NULL);
// 	lv_obj_align_to(btn5, btn4, LV_ALIGN_OUT_RIGHT_TOP, -5, 0);
//     labelBtn5 = lv_label_create(btn5);
// 	lv_label_set_text(labelBtn5, LV_SYMBOL_DOWN"  ]");
//     lv_obj_center(labelBtn5);
// 	lv_obj_add_style(btn5, &style, 0);
// 
// 	labelFloor = lv_label_create(scr1);
//     lv_obj_align(labelFloor, LV_ALIGN_LEFT_MID, 35 , -25);
//     lv_obj_set_style_text_font(labelFloor, &dseg70, LV_STATE_DEFAULT);
//     lv_obj_set_style_text_color(labelFloor, lv_color_white(), LV_STATE_DEFAULT);
//     lv_label_set_text_fmt(labelFloor, "%02d", 23);
// 
// 	labelFloorDigit = lv_label_create(scr1);
//     lv_obj_align_to(labelFloorDigit, labelFloor, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, -15);
//     lv_obj_set_style_text_font(labelFloorDigit, &dseg40, LV_STATE_DEFAULT);
//     lv_obj_set_style_text_color(labelFloorDigit, lv_color_white(), LV_STATE_DEFAULT);
//     lv_label_set_text_fmt(labelFloorDigit, ".%d", 4);

	labelClock = lv_label_create(scr1);
	lv_obj_align(labelClock, LV_ALIGN_TOP_RIGHT, -3 , 3);
    lv_obj_set_style_text_font(labelClock, &dseg25, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(labelClock, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text_fmt(labelClock, "17:05");

	labelVelEscrita = lv_label_create(scr1);
	lv_obj_align_to(labelVelEscrita, labelBtn1, LV_ALIGN_OUT_BOTTOM_MID, 5, 20);
	lv_obj_set_style_text_color(labelVelEscrita, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelVelEscrita, "vel =           km/h");

	labelVelocidade = lv_label_create(scr1);
	lv_obj_align_to(labelVelocidade, labelVelEscrita, LV_ALIGN_RIGHT_MID, -65, 5);
	lv_obj_set_style_text_font(labelVelocidade, &dseg15, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelVelocidade, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelVelocidade, "%02d", 0);

// 	// adiciona a label de unidade de velocidade ao lado direito da label de velocidade
// 	labelVelUnity = lv_label_create(scr1);
// 	lv_obj_align_to(labelVelUnity, labelVelocidade, LV_ALIGN_OUT_RIGHT_TOP, 2, 0);
// 	lv_obj_set_style_text_font(labelVelUnity, &dseg10, LV_STATE_DEFAULT);
// 	lv_obj_set_style_text_color(labelVelUnity, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelVelUnity, "velocidade (km/h)");

	labelAcelEscrita = lv_label_create(scr1);
	lv_obj_align_to(labelAcelEscrita, labelVelEscrita, LV_ALIGN_OUT_BOTTOM_LEFT, -4, 15);
	lv_obj_set_style_text_color(labelAcelEscrita, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelAcelEscrita, "acel =           km/h2");

	labelAceleracao = lv_label_create(scr1);
	lv_obj_align_to(labelAceleracao, labelAcelEscrita, LV_ALIGN_RIGHT_MID, -86, 5);
	lv_obj_set_style_text_font(labelAceleracao, &dseg15, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelAceleracao, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelAceleracao, "%02d", 0);

// 	// adiciona a label de unidade de aceleracao ao lado direito da label de aceleracao
// 	labelAcelUnity = lv_label_create(scr1);
// 	lv_obj_align_to(labelAcelUnity, labelAceleracao, LV_ALIGN_OUT_RIGHT_TOP, 2, 0);
// 	lv_obj_set_style_text_font(labelAcelUnity, &dseg10, LV_STATE_DEFAULT);
// 	lv_obj_set_style_text_color(labelAcelUnity, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelAcelUnity, "aceleracao (km/h/s)");

	labelVelMedEscrita = lv_label_create(scr1);
	lv_obj_align_to(labelVelMedEscrita, labelAcelEscrita, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 15);
	lv_obj_set_style_text_color(labelVelMedEscrita, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelVelMedEscrita, "vel med =         km/h");

	labelVelMedia = lv_label_create(scr1);
	lv_obj_align_to(labelVelMedia, labelVelMedEscrita, LV_ALIGN_RIGHT_MID, -63, 5);
	lv_obj_set_style_text_font(labelVelMedia, &dseg15, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelVelMedia, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelVelMedia, "0");

// 	// adiciona a label de unidade de velocidade media ao lado direito da label de velocidade media
// 	labelVelMediaUnity = lv_label_create(scr1);
// 	lv_obj_align_to(labelVelMediaUnity, labelVelMedia, LV_ALIGN_OUT_RIGHT_TOP, 2, 0);
// 	lv_obj_set_style_text_font(labelVelMediaUnity, &dseg10, LV_STATE_DEFAULT);
// 	lv_obj_set_style_text_color(labelVelMediaUnity, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelVelMediaUnity, "velocidade media (km/h)");

	labelTempoEscrita = lv_label_create(scr1);
	lv_obj_align_to(labelTempoEscrita, labelVelMedEscrita, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 15);
	lv_obj_set_style_text_color(labelTempoEscrita, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelTempoEscrita, "tempo = ");

	labelTempo = lv_label_create(scr1);
	lv_obj_align_to(labelTempo, labelTempoEscrita, LV_ALIGN_RIGHT_MID, 60, 5);
	lv_obj_set_style_text_font(labelTempo, &dseg15, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelTempo, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelTempo, "0");

// 	// adiciona a label de unidade de tempo ao lado direito da label de tempo
// 	labelTempoUnity = lv_label_create(scr1);
// 	lv_obj_align_to(labelTempoUnity, labelTempo, LV_ALIGN_OUT_RIGHT_TOP, 2, 0);
// 	lv_obj_set_style_text_font(labelTempoUnity, &dseg10, LV_STATE_DEFAULT);
// 	lv_obj_set_style_text_color(labelTempoUnity, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelTempoUnity, "tempo (s)");

	labelDistanciaEscrita = lv_label_create(scr1);
	lv_obj_align_to(labelDistanciaEscrita, labelTempoEscrita, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 15);
	lv_obj_set_style_text_color(labelDistanciaEscrita, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelDistanciaEscrita, "dist =              km");
	
	labelDistancia = lv_label_create(scr1);
	lv_obj_align_to(labelDistancia, labelDistanciaEscrita, LV_ALIGN_RIGHT_MID, -45, 5);
	lv_obj_set_style_text_font(labelDistancia, &dseg15, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelDistancia, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelDistancia, "0");

// 	// adiciona a label de unidade de distancia ao lado direito da label de distancia
// 	labelDistanciaUnity = lv_label_create(scr1);
// 	lv_obj_align_to(labelDistanciaUnity, labelDistancia, LV_ALIGN_OUT_RIGHT_TOP, 2, 0);
// 	lv_obj_set_style_text_font(labelDistanciaUnity, &dseg10, LV_STATE_DEFAULT);
// 	lv_obj_set_style_text_color(labelDistanciaUnity, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelDistanciaUnity, "distancia (m)");




// 	labelSetValue = lv_label_create(scr1);
//     lv_obj_align_to(labelSetValue, labelClock, LV_ALIGN_OUT_BOTTOM_LEFT, 3, 30);    
//     lv_obj_set_style_text_font(labelSetValue, &dseg40, LV_STATE_DEFAULT);
//     lv_obj_set_style_text_color(labelSetValue, lv_color_white(), LV_STATE_DEFAULT);
//     lv_label_set_text_fmt(labelSetValue, "%02d", 22);
// 
// 	labelCelsius = lv_label_create(scr1);
// 	lv_obj_align_to(labelCelsius, labelFloor, LV_ALIGN_OUT_RIGHT_TOP, 0, 0);
// 	lv_obj_set_style_text_font(labelCelsius, &monts15, LV_STATE_DEFAULT);
// 	lv_obj_set_style_text_color(labelCelsius, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelCelsius, "°C");
// 
// 	labelConfig = lv_label_create(scr1);
// 	lv_obj_align_to(labelConfig, labelSetValue, LV_ALIGN_OUT_LEFT_TOP, 17, 0);
// 	lv_obj_set_style_text_color(labelConfig, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelConfig, LV_SYMBOL_SETTINGS);
// 
// 	labelSet = lv_label_create(scr1);
// 	lv_obj_align_to(labelSet, labelConfig, LV_ALIGN_OUT_BOTTOM_LEFT, -10, 10);
// 	lv_obj_set_style_text_font(labelSet, &monts15, LV_STATE_DEFAULT);
// 	lv_obj_set_style_text_color(labelSet, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelSet, "MAIO");
// 
// 	labelClock2 = lv_img_create(scr1);
// 	lv_img_set_src(labelClock2, &clock);
// 	lv_obj_set_height(labelClock2, 25);
// 	lv_obj_set_width(labelClock2, 25);
// 	lv_obj_align_to(labelClock2, labelSetValue, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 15);
// 
// 	labelFumaca = lv_img_create(scr1);
// 	lv_img_set_src(labelFumaca, &fumaca);
// 	lv_obj_set_height(labelFumaca, 25);
// 	lv_obj_set_width(labelFumaca, 30);
// 	lv_obj_align_to(labelFumaca, labelClock2, LV_ALIGN_OUT_RIGHT_MID, 8, -2);
// 
// 	labelDia = lv_label_create(scr1);
// 	lv_obj_align(labelDia, LV_ALIGN_TOP_LEFT, 10, 10);
// 	lv_obj_set_style_text_font(labelDia, &monts15, LV_STATE_DEFAULT);
// 	lv_obj_set_style_text_color(labelDia, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelDia, "SEG");
// 
// 	labelCelsius2 = lv_label_create(scr1);
// 	lv_obj_align_to(labelCelsius2, labelSetValue, LV_ALIGN_OUT_RIGHT_TOP, 2, 0);
// 	lv_obj_set_style_text_font(labelCelsius2, &monts15, LV_STATE_DEFAULT);
// 	lv_obj_set_style_text_color(labelCelsius2, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelCelsius2, "°C");
// 
// 	labelFT = lv_label_create(scr1);
// 	lv_obj_align(labelFT, LV_ALIGN_LEFT_MID, 2 , -55);
// 	lv_obj_set_style_text_font(labelFT, &monts10, LV_STATE_DEFAULT);
// 	lv_obj_set_style_text_color(labelFT, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelFT, "FLOOR");
// 
// 	labelFT2 = lv_label_create(scr1);
// 	lv_obj_align_to(labelFT2, labelFT, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
// 	lv_obj_set_style_text_font(labelFT2, &monts10, LV_STATE_DEFAULT);
// 	lv_obj_set_style_text_color(labelFT2, lv_color_white(), LV_STATE_DEFAULT);
// 	lv_label_set_text_fmt(labelFT2, "TEMP");
}


/************************************************************************/
/* callbacks                                                               */
/************************************************************************/
// Queues
QueueHandle_t xQueueMagnet;

volatile uint32_t start_mag = 0;

void magnet_callback(void){
	//printf("Teste");
	if (start_mag == 0){
		start_mag = 1;
		RTT_init(10000, 2353, 0);
		}
	else{
		printf("Magnet\n");
		uint32_t tempo = rtt_read_timer_value(RTT);
		// tempo em segundos
		RTT_init(10000, 2353, 0);
		xQueueSendFromISR(xQueueMagnet, &tempo, NULL);
		}

}

void MAGNET_INIT(int mode){
	if (mode == 0){
		pmc_enable_periph_clk(MAGNET_PIO_ID);
		pio_set_input(MAGNET_PIO,MAGNET_PIO_IDX_MASK,PIO_PULLUP);
		pio_handler_set(MAGNET_PIO, MAGNET_PIO_ID, MAGNET_PIO_IDX_MASK, PIO_IT_RISE_EDGE, magnet_callback);
		pio_enable_interrupt(MAGNET_PIO, MAGNET_PIO_IDX_MASK);
		NVIC_EnableIRQ(MAGNET_PIO_ID);
		NVIC_SetPriority(MAGNET_PIO_ID, 4);
	}
	else{
		pmc_enable_periph_clk(MAGNET_PIO_ID);
		pio_set_input(MAGNET_PIO,MAGNET_PIO_IDX_MASK,PIO_PULLUP | PIO_DEBOUNCE);
		pio_set_debounce_filter(MAGNET_PIO, MAGNET_PIO_IDX_MASK, 10);
		pio_handler_set(MAGNET_PIO, MAGNET_PIO_ID, MAGNET_PIO_IDX_MASK, PIO_IT_RISE_EDGE, magnet_callback);
		pio_enable_interrupt(MAGNET_PIO, MAGNET_PIO_IDX_MASK);
		NVIC_EnableIRQ(MAGNET_PIO_ID);
		NVIC_SetPriority(MAGNET_PIO_ID, 4);
	}
}


/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_lcd(void *pvParameters) {
	int px, py;
	lv_termostato();
	for (;;)  {
		xSemaphoreTake(xMutexLVGL, portMAX_DELAY);
		lv_tick_inc(50);
		lv_task_handler();
		xSemaphoreGive(xMutexLVGL);
		vTaskDelay(50);
	}
}

static void task_clock(void *pvParameters) {
	calendar rtc_initial = {2023, 5, 3, 1, 17, 13, 1};                                            
    RTC_init(RTC, ID_RTC, rtc_initial, RTC_IER_ALREN);  
	uint32_t current_hour, current_min, current_sec;
	uint32_t current_year, current_month, current_day, current_week;
	for (;;)  {
		rtc_get_time(RTC, &current_hour, &current_min, &current_sec);
		rtc_get_date(RTC, &current_year, &current_month, &current_day, &current_week);
		lv_label_set_text_fmt(labelClock, "%02d:%02d", current_hour, current_min);
		vTaskDelay(1000);
		lv_label_set_text_fmt(labelClock, "%02d %02d", current_hour, current_min);
		if (flagTrajeto == 1){
		tempoPercorrido++;
		lv_label_set_text_fmt(labelTempo, "%02d:%02d:%02d", tempoPercorrido/3600, (tempoPercorrido%3600)/60, tempoPercorrido%60);
		}
		vTaskDelay(1000);
		if (flagTrajeto == 1){
		tempoPercorrido++;
		lv_label_set_text_fmt(labelTempo, "%02d:%02d:%02d", tempoPercorrido/3600, (tempoPercorrido%3600)/60, tempoPercorrido%60);
		}
	}
}

void task_magnet(void *pvParameters){
	uint32_t tempo;
	float tempo_em_segundos;
	float velocidade;
	float ultima_velocidade = 0;
	float aceleracao;
	printf("Task Magnet created!\n");
	while(1){
		// read the magnet queue
		if (xQueueReceive(xQueueMagnet, &tempo, portMAX_DELAY) == pdTRUE){
				tempo_em_segundos = (float)tempo/10000;
				vTaskDelay(100);
				printf("Tempo em segundos: %f\n", tempo_em_segundos);
				float raio = diametro_roda/2;
				velocidade = (2*PI*raio)*3.6/tempo_em_segundos;
				printf("Velocidade: %f Km/h\n", velocidade);
				aceleracao = (velocidade - ultima_velocidade);
				printf("Aceleracao: %f\n", aceleracao);
				ultima_velocidade = velocidade;
				// alterar a velocidade no display
				xSemaphoreTake(xMutexLVGL, portMAX_DELAY);
				// formata o texto
				char velocidade_str[10];
				sprintf(velocidade_str, "%.2f", velocidade);
				lv_label_set_text_fmt(labelVelocidade, "%s", velocidade_str);
				// formata o texto
				char aceleracao_str[10];
				sprintf(aceleracao_str, "%.2f", aceleracao);
				lv_label_set_text_fmt(labelAceleracao, "%s", aceleracao_str);
				xSemaphoreGive(xMutexLVGL);
				if (flagTrajeto == 1){
					// calcula a distancia percorrida em km
					distanciaPercorrida += (2*PI*raio)/1000;
					// alterar a distancia no display
					xSemaphoreTake(xMutexLVGL, portMAX_DELAY);
					// formata o texto
					char distancia_str[10];
					sprintf(distancia_str, "%.2f", distanciaPercorrida);
					lv_label_set_text_fmt(labelDistancia, "%s", distancia_str);
					xSemaphoreGive(xMutexLVGL);

					// calcula a velocidade media
					velocidadeMedia = velocidadeMedia + (velocidade - velocidadeMedia)/2;
					// alterar a velocidade media no display
					xSemaphoreTake(xMutexLVGL, portMAX_DELAY);
					// formata o texto
					char velocidadeMedia_str[10];
					sprintf(velocidadeMedia_str, "%.2f", velocidadeMedia);
					lv_label_set_text_fmt(labelVelMedia, "%s", velocidadeMedia_str);
					xSemaphoreGive(xMutexLVGL);

					printf("Distancia percorrida: %f\n", distanciaPercorrida);
					printf("Velocidade media: %f\n", velocidadeMedia);
					printf("Tempo percorrido: %d\n", tempoPercorrido);
				}
			}
		}

		// sleep the task ulntil the next magnet event
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
}


/********************************************************************/
/* Simulador de velocidade                                          */

#define TASK_SIMULATOR_STACK_SIZE (4096 / sizeof(portSTACK_TYPE))
#define TASK_SIMULATOR_STACK_PRIORITY (tskIDLE_PRIORITY)

#define RAIO 0.508/2
#define VEL_MAX_KMH  5.0f
#define VEL_MIN_KMH  0.5f
//#define RAMP 

/**
* raio 20" => 50,8 cm (diametro) => 0.508/2 = 0.254m (raio)
* w = 2 pi f (m/s)
* v [km/h] = (w*r) / 3.6 = (2 pi f r) / 3.6
* f = v / (2 pi r 3.6)
* Exemplo : 5 km / h = 1.38 m/s
*           f = 0.87Hz
*           t = 1/f => 1/0.87 = 1,149s
*/
float kmh_to_hz(float vel, float raio) {
    float f = vel / (2*PI*raio*3.6);
    return(f);
}

static void task_simulador(void *pvParameters) {

    pmc_enable_periph_clk(ID_PIOC);
    pio_set_output(PIOC, PIO_PC31, 1, 0, 0);

    float vel = VEL_MAX_KMH;
    float f;
    int ramp_up = 1;

    while(1){
        pio_clear(PIOC, PIO_PC31);
        //delay_ms(1);
		vTaskDelay(1);
        pio_set(PIOC, PIO_PC31);
#ifdef RAMP
        if (ramp_up) {
            printf("[SIMU] ACELERANDO: %d \n", (int) (10*vel));
            vel += 0.5;
        } else {
            printf("[SIMU] DESACELERANDO: %d \n",  (int) (10*vel));
            vel -= 0.5;
        }

        if (vel >= VEL_MAX_KMH)
        ramp_up = 0;
    else if (vel <= VEL_MIN_KMH)
    ramp_up = 1;
	#endif
#ifndef RAMP
        vel = 5;
        //printf("[SIMU] CONSTANTE: %d \n", (int) (10*vel));
#endif
        f = kmh_to_hz(vel, RAIO);
        int t = 965*(1.0/f); //UTILIZADO 965 como multiplicador ao invés de 1000
                             //para compensar o atraso gerado pelo Escalonador do freeRTOS
		//printf("Tempo: %d\n", t);						 
        //delay_ms(t);
		vTaskDelay(t);
    }
}



/************************************************************************/
/* configs                                                              */
/************************************************************************/

static void configure_lcd(void) {
	pio_configure_pin(LCD_SPI_MISO_PIO, LCD_SPI_MISO_FLAGS);  //
	pio_configure_pin(LCD_SPI_MOSI_PIO, LCD_SPI_MOSI_FLAGS);
	pio_configure_pin(LCD_SPI_SPCK_PIO, LCD_SPI_SPCK_FLAGS);
	pio_configure_pin(LCD_SPI_NPCS_PIO, LCD_SPI_NPCS_FLAGS);
	pio_configure_pin(LCD_SPI_RESET_PIO, LCD_SPI_RESET_FLAGS);
	pio_configure_pin(LCD_SPI_CDS_PIO, LCD_SPI_CDS_FLAGS);
	ili9341_init();
	ili9341_backlight_on();
}

static void configure_console(void) {
	const usart_serial_options_t uart_serial_options = {
		.baudrate = USART_SERIAL_EXAMPLE_BAUDRATE,
		.charlength = USART_SERIAL_CHAR_LENGTH,
		.paritytype = USART_SERIAL_PARITY,
		.stopbits = USART_SERIAL_STOP_BIT,
	};
	stdio_serial_init(CONSOLE_UART, &uart_serial_options);
	setbuf(stdout, NULL);
}

void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type) {
	pmc_enable_periph_clk(ID_RTC);
	rtc_set_hour_mode(rtc, 0);
	rtc_set_date(rtc, t.year, t.month, t.day, t.week);
	rtc_set_time(rtc, t.hour, t.minute, t.second);
	NVIC_DisableIRQ(id_rtc);
	NVIC_ClearPendingIRQ(id_rtc);
	NVIC_SetPriority(id_rtc, 4);
	NVIC_EnableIRQ(id_rtc);
	rtc_enable_interrupt(rtc,  irq_type);
}


/************************************************************************/
/* port lvgl                                                            */
/************************************************************************/

void my_flush_cb(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
	ili9341_set_top_left_limit(area->x1, area->y1);   ili9341_set_bottom_right_limit(area->x2, area->y2);
	ili9341_copy_pixels_to_screen(color_p,  (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));
	lv_disp_flush_ready(disp_drv);
}

void my_input_read(lv_indev_drv_t * drv, lv_indev_data_t*data) {
	int px, py, pressed;
	if (readPoint(&px, &py))
		data->state = LV_INDEV_STATE_PRESSED;
	else
		data->state = LV_INDEV_STATE_RELEASED; 
	data->point.x = py;
	data->point.y = 320 - px;
}

void configure_lvgl(void) {
	lv_init();
	lv_disp_draw_buf_init(&disp_buf, buf_1, NULL, LV_HOR_RES_MAX * LV_VER_RES_MAX);
	
	lv_disp_drv_init(&disp_drv);            /*Basic initialization*/
	disp_drv.draw_buf = &disp_buf;          /*Set an initialized buffer*/
	disp_drv.flush_cb = my_flush_cb;        /*Set a flush callback to draw to the display*/
	disp_drv.hor_res = LV_HOR_RES_MAX;      /*Set the horizontal resolution in pixels*/
	disp_drv.ver_res = LV_VER_RES_MAX;      /*Set the vertical resolution in pixels*/

	lv_disp_t * disp;
	disp = lv_disp_drv_register(&disp_drv); /*Register the driver and save the created display objects*/
	
	/* Init input on LVGL */
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = my_input_read;
	lv_indev_t * my_indev = lv_indev_drv_register(&indev_drv);
}

/************************************************************************/
/* main                                                                 */
/************************************************************************/
int main(void) {
	/* board and sys init */
	board_init();
	sysclk_init();
	configure_console();
	MAGNET_INIT(1);
	/* Disable the watchdog */                                                                      
    WDT->WDT_MR = WDT_MR_WDDIS;  

	/* LCd, touch and lvgl init*/
	configure_lcd();
	ili9341_set_orientation(ILI9341_FLIP_Y | ILI9341_SWITCH_XY);
	configure_touch();
	configure_lvgl();
	
	xMutexLVGL = xSemaphoreCreateMutex();

	xQueueMagnet = xQueueCreate(10, sizeof(uint32_t));

	/* Create task to control oled */
	if (xTaskCreate(task_lcd, "LCD", TASK_LCD_STACK_SIZE, NULL, TASK_LCD_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create lcd task\r\n");
	}

	if (xTaskCreate(task_clock, "Clock", TASK_LCD_STACK_SIZE, NULL, TASK_LCD_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create clock task\r\n");
	}
	if (xTaskCreate(task_magnet, "Magnet", TASK_MAGNET_STACK_SIZE, NULL, TASK_MAGNET_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create magnet task\r\n");
	}
	
	if (xTaskCreate(task_simulador, "Simulador", TASK_SIMULATOR_STACK_SIZE, NULL, TASK_SIMULATOR_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create simulador task\r\n");
	}
	

	/* Start the scheduler. */
	vTaskStartScheduler();

	while(1){ }
}
