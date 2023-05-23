/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include <asf.h>
#include <string.h>
#include "ili9341.h"
#include "lvgl.h"
#include "touch/touch.h"

/************************************************************************/
/* LCD / LVGL                                                           */
/************************************************************************/

#define LV_HOR_RES_MAX          (320)
#define LV_VER_RES_MAX          (240)

LV_FONT_DECLARE(dseg70);
LV_FONT_DECLARE(dseg45);
LV_FONT_DECLARE(dseg35);
LV_FONT_DECLARE(dseg30);
LV_FONT_DECLARE(dseg60);

/*A static or global variable to store the buffers*/
static lv_disp_draw_buf_t disp_buf;

/*Static or global buffer(s). The second buffer is optional*/
static lv_color_t buf_1[LV_HOR_RES_MAX * LV_VER_RES_MAX];
static lv_disp_drv_t disp_drv;          /*A variable to hold the drivers. Must be static or global.*/
static lv_indev_drv_t indev_drv;
static  lv_obj_t * label;
lv_obj_t * labelFloor;
lv_obj_t * labelSetValue;
lv_obj_t * labelClock;
lv_obj_t * SecondDigt;

volatile int tipo = 0;
volatile uint32_t current_hour, current_min, current_sec;
volatile uint32_t current_year, current_month, current_day, current_week;

volatile char flag_rtc_alarm = 0;
// alarm semaphore
SemaphoreHandle_t xSemaphoreAlarm;

// Mutex
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
void lv_termostato(void);

/************************************************************************/
/* RTOS                                                                 */
/************************************************************************/

#define TASK_LCD_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_LCD_STACK_PRIORITY            (tskIDLE_PRIORITY)
#define TASK_CLOCK_STACK_SIZE              (1024*6/sizeof(portSTACK_TYPE))
#define TASK_CLOCK_STACK_PRIORITY          (tskIDLE_PRIORITY)

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


// RTC
void RTC_Handler(void) {
    uint32_t ul_status = rtc_get_status(RTC);
	
    /* seccond tick */
    if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {	
	// o código para irq de segundo vem aqui
    }
	    /* Time or date alarm */
    if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
    	// o código para irq de alame vem aqui
        flag_rtc_alarm = 1;
		xSemaphoreGiveFromISR(xSemaphoreAlarm, NULL);
		//printf("Alarme\n");
    }

    rtc_clear_status(RTC, RTC_SCCR_SECCLR);
    rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
    rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
    rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
    rtc_clear_status(RTC, RTC_SCCR_CALCLR);
    rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
}

void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type) {
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(rtc, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(rtc, t.year, t.month, t.day, t.week);
	rtc_set_time(rtc, t.hour, t.minute, t.second);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(id_rtc);
	NVIC_ClearPendingIRQ(id_rtc);
	NVIC_SetPriority(id_rtc, 4);
	NVIC_EnableIRQ(id_rtc);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(rtc,  irq_type);
}

/************************************************************************/
/* lvgl                                                                 */
/************************************************************************/

static void event_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);

	if(code == LV_EVENT_CLICKED) {
		LV_LOG_USER("Clicked");
	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
		LV_LOG_USER("Toggled");
	}
}

static void up_handler(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    char *c;
    int temp;
    if(code == LV_EVENT_CLICKED) {
        c = lv_label_get_text(labelSetValue);
        temp = atoi(c);
        lv_label_set_text_fmt(labelSetValue, "%02d", temp + 1);
    }
}

static void down_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	if(code == LV_EVENT_CLICKED) {
		c = lv_label_get_text(labelSetValue);
		temp = atoi(c);
		lv_label_set_text_fmt(labelSetValue, "%02d", temp - 1);
	}
}

void PowerOff(lv_event_t * e){
	// desliga o display quando o botão é pressionado
	//lv_disp_get_default();
	// limpa a tela
	lv_event_code_t code = lv_event_get_code(e);
	if(code == LV_EVENT_CLICKED) {
		lv_obj_clean(lv_scr_act());
		lv_obj_t * btn1 = lv_btn_create(lv_scr_act());
		lv_obj_add_event_cb(btn1, PowerOff, LV_EVENT_ALL, NULL);

		label = lv_label_create(btn1);
		lv_obj_center(label);

		// alinha o botão 1 no canto inferior esquerdo
		lv_obj_align(btn1, LV_ALIGN_BOTTOM_LEFT, 10, -10);
		lv_label_set_text(label, "[  " LV_SYMBOL_POWER);

		static lv_style_t style;
		lv_style_init(&style);
		lv_style_set_bg_color(&style, lv_color_black());
		lv_style_set_border_width(&style, 0);

		lv_obj_add_style(btn1, &style, 0);

		if (tipo == 1){
			lv_termostato();
			tipo = 0;
		}else{
			tipo = 1;
		}
	}



}

void lv_ex_btn_1(void) {


	lv_obj_t * btn2 = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btn2, event_handler, LV_EVENT_ALL, NULL);
	lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
	lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_set_height(btn2, LV_SIZE_CONTENT);

	label = lv_label_create(btn2);
	lv_label_set_text(label, "Toggle");
	lv_obj_center(label);
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/


void lv_termostato(void){


	lv_obj_t * btn1 = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btn1, PowerOff, LV_EVENT_ALL, NULL);

	label = lv_label_create(btn1);
	lv_label_set_text(label, "Corsi");
	lv_obj_center(label);

	// alinha o botão 1 no canto inferior esquerdo
	lv_obj_align(btn1, LV_ALIGN_BOTTOM_LEFT, 10, -10);
	lv_label_set_text(label, "[  " LV_SYMBOL_POWER);

	static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, lv_color_black());
    lv_style_set_border_width(&style, 0);

	lv_obj_add_style(btn1, &style, 0);

	// botão 2
	lv_obj_t * btn2 = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btn2, event_handler, LV_EVENT_ALL, NULL);
	
	label = lv_label_create(btn2);
	lv_obj_center(label);

	// alinha o botão 2 ao lado do botão 1
	lv_obj_align_to(btn2, btn1, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
	lv_label_set_text(label, "|  M");

	lv_obj_add_style(btn2, &style, 0);

	// botão 3
	lv_obj_t * btn3 = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btn3, event_handler, LV_EVENT_ALL, NULL);

	label = lv_label_create(btn3);
	lv_obj_center(label);

	// alinha o botão 3 ao lado do botão 2
	lv_obj_align_to(btn3, btn2, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

	lv_label_set_text(label, "|  " LV_SYMBOL_SETTINGS);

	lv_obj_add_style(btn3, &style, 0);

	// botão 4
	lv_obj_t * btn4 = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btn4, event_handler, LV_EVENT_ALL, NULL);

	label = lv_label_create(btn4);
	lv_obj_center(label);

	// alinha o botão 4 ao lado do botão 3
	lv_obj_align_to(btn4, btn3, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

	lv_label_set_text(label, "]");

	lv_obj_add_style(btn4, &style, 0);

	// botão 5
	lv_obj_t * btn5 = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btn5, up_handler, LV_EVENT_ALL, NULL);

	label = lv_label_create(btn5);
	lv_obj_center(label);

	// alinha o botão 5 ao lado do botão 4
	lv_obj_align_to(btn5, btn4, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

	lv_label_set_text(label, "[ " LV_SYMBOL_UP);

	lv_obj_add_style(btn5, &style, 0);

	// botão 6
	lv_obj_t * btn6 = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btn6, down_handler, LV_EVENT_ALL, NULL);

	label = lv_label_create(btn6);
	lv_obj_center(label);

	// alinha o botão 6 ao lado do botão 5
	lv_obj_align_to(btn6, btn5, LV_ALIGN_OUT_RIGHT_MID, 0, 0);


	lv_label_set_text(label, "| " LV_SYMBOL_DOWN);

	lv_obj_add_style(btn6, &style, 0);

	// botão 7
	lv_obj_t * btn7 = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btn7, event_handler, LV_EVENT_ALL, NULL);

	label = lv_label_create(btn7);
	lv_obj_center(label);

	// alinha o botão 7 ao lado do botão 6
	lv_obj_align_to(btn7, btn6, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

	lv_label_set_text(label, "]");
	lv_obj_add_style(btn7, &style, 0);


	labelFloor = lv_label_create(lv_scr_act());
    lv_obj_align(labelFloor, LV_ALIGN_LEFT_MID, 35 , -45);
    lv_obj_set_style_text_font(labelFloor, &dseg70, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(labelFloor, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text_fmt(labelFloor, "%02d", 23);

	// cria um relógio no canto superior direito utilizando o labelClock
	labelClock = lv_label_create(lv_scr_act());
	lv_obj_align(labelClock, LV_ALIGN_RIGHT_MID, 0 , -90);
	lv_obj_set_style_text_font(labelClock, &dseg30, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelClock, lv_color_white(), LV_STATE_DEFAULT);
	//lv_label_set_text_fmt(labelClock, "%02d:%02d", 23, 59);

	// cria um label para o valor da temperatura logo abaixo do relógio
	labelSetValue = lv_label_create(lv_scr_act());
	lv_obj_align(labelSetValue, LV_ALIGN_RIGHT_MID, -10 , -30);
	lv_obj_set_style_text_font(labelSetValue, &dseg45, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelSetValue, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelSetValue, "%02d", 22);

	// adiciona um digito a temperatura
	SecondDigt = lv_label_create(lv_scr_act());
	lv_obj_align_to(SecondDigt, labelFloor, LV_ALIGN_OUT_RIGHT_MID, 0, 20);
	lv_obj_set_style_text_font(SecondDigt, &dseg30, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(SecondDigt, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text(SecondDigt, ".4");





}



static void task_lcd(void *pvParameters) {
	int px, py;

	lv_ex_btn_1();
	lv_termostato();

	for (;;)  {
		// checa o mutex para ver se o display está sendo utilizado 
		xSemaphoreTake( xMutexLVGL, portMAX_DELAY );
		lv_tick_inc(50);
		fflush( stdout );
		lv_task_handler();
		xSemaphoreGive( xMutexLVGL );
		vTaskDelay(50);
	}
}

void task_clock(void *pvParameters) {
	// atualiza o relógio do display a cada 1 segundo utilizando o labelClock e RTC
	rtc_set_date_alarm(RTC, 1, current_month, 1, current_day);                              
    rtc_set_time_alarm(RTC, 1, current_hour, 1, current_min, 1, current_sec + 1);
	for (;;) {
		// se o alarme o semafaro (xSemaphoreAlarm) for ativado, atualiza o relógio
		if (xSemaphoreTake(xSemaphoreAlarm, 0) == pdTRUE){
			printf("Alarme ativado\n");
			rtc_get_time(RTC, &current_hour, &current_min, &current_sec);
			// prita o segundo no display
			printf("Segundo: %d \r ", current_sec);
			// printa o minuto no display
			printf("Minuto: %d \r ", current_min);
			// printa a hora no display
			printf("Hora: %d \r ", current_hour);
			if (current_sec == 59) {
				current_sec = 0;
				current_min++;
				if (current_min == 60) {
					current_min = 0;
					current_hour++;
					if (current_hour == 24) {
						current_hour = 0;
					}
				}
			}
			lv_label_set_text_fmt(labelClock, "%02d:%02d", current_hour, current_min);
			rtc_set_date_alarm(RTC, 1, current_month, 1, current_day);                              
    		rtc_set_time_alarm(RTC, 1, current_hour, 1, current_min, 1, current_sec + 1);
		}
	}
}

/************************************************************************/
/* configs                                                              */
/************************************************************************/

static void configure_lcd(void) {
	/**LCD pin configure on SPI*/
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

	/* Configure console UART. */
	stdio_serial_init(CONSOLE_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	setbuf(stdout, NULL);
}

/************************************************************************/
/* port lvgl                                                            */
/************************************************************************/

void my_flush_cb(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
	ili9341_set_top_left_limit(area->x1, area->y1);   ili9341_set_bottom_right_limit(area->x2, area->y2);
	ili9341_copy_pixels_to_screen(color_p,  (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));
	
	/* IMPORTANT!!!
	* Inform the graphics library that you are ready with the flushing*/
	lv_disp_flush_ready(disp_drv);
}

void my_input_read(lv_indev_drv_t * drv, lv_indev_data_t*data) {
	int px, py, pressed;
	
	if (readPoint(&px, &py))
		data->state = LV_INDEV_STATE_PRESSED;
	else
		data->state = LV_INDEV_STATE_RELEASED; 
	
	data->point.x = px;
	data->point.y = py;
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

	xMutexLVGL = xSemaphoreCreateMutex();

	/* LCd, touch and lvgl init*/
	configure_lcd();
	configure_touch();
	configure_lvgl();

	// cria o semáforo do alarme (inicialmente bloqueado)
	xSemaphoreAlarm = xSemaphoreCreateBinary();

	/** Configura RTC */                                                                            
    calendar rtc_initial = {2018, 3, 19, 12, 15, 45 ,1};                                            
    RTC_init(RTC, ID_RTC, rtc_initial, RTC_IER_ALREN);                                              
                                                                                                    
    /* Leitura do valor atual do RTC */           
    rtc_get_time(RTC, &current_hour, &current_min, &current_sec);
    rtc_get_date(RTC, &current_year, &current_month, &current_day, &current_week);

	/* Create task to control oled */
	if (xTaskCreate(task_lcd, "LCD", TASK_LCD_STACK_SIZE, NULL, TASK_LCD_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create lcd task\r\n");
	}

	// cria a tarefa do relógio
	if (xTaskCreate(task_clock, "Clock", TASK_CLOCK_STACK_SIZE, NULL, TASK_CLOCK_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create clock task\r\n");
	}
	
	/* Start the scheduler. */
	vTaskStartScheduler();

	while(1){ }
}
