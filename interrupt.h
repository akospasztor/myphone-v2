#ifndef __INTERRUPT_H__
#define	__INTERRUPT_H__


#define HBIT_ON     50
#define HBIT_MAX    1000

#define WARN_ON     50
#define WARN_MAX    500

#define ERR_ON      50
#define ERR_MAX     500

#define KEY_FALL    3

//FLAGS
extern unsigned short FLAG_battery;

extern unsigned int FLAG_gsm;
extern unsigned int FLAG_gsm_idle;
extern unsigned int FLAG_gsm_ri;

extern unsigned int FLAG_ftdi;
extern short FLAG_ftdi_dwn;

extern short FLAG_spi_tx;

extern unsigned int FLAG_key;
extern unsigned int FLAG_key_counter;
extern unsigned short FLAG_key_state;

//VARIABLES
extern int allapot;
extern int gsm_allapot;

unsigned int led_c = 0;         	//led counter
unsigned int warning = 0;       	//0:off    1:yellow   2:red
unsigned int keyactive;

unsigned short i;

extern unsigned char disp[];
extern unsigned int ptr_disp;
unsigned int lcd_ptr = 0;       	//lcd cursor pointer

extern unsigned char ftdi_char;
extern unsigned char gsm_char;
unsigned char rx_ftdi;          	//ftdi receive
unsigned char rx_gsm;           	//gsm receive

extern unsigned int ptr_gsm;
extern unsigned char gsm[];
extern unsigned short gsm_error;

extern unsigned int spi_addr;
extern unsigned int spi_row_counter;

//TIMERS
extern unsigned int TMR_battery;
extern unsigned int TMR_spiready;
extern unsigned int TMR_gsm;
extern unsigned int TMR_gsm_csq;
extern unsigned int TMR_gsm_time;
extern unsigned int TMR_gsm_separator;
extern unsigned int TMR_gsm_ri;

#endif
