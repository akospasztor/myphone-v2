#ifndef __MAIN_H__
#define	__MAIN_H__

typedef unsigned char   uchar;
typedef unsigned int    uint;
typedef unsigned long   ulong;

typedef signed char     schar;
typedef signed int      sint;
typedef signed long     slong;

typedef unsigned char   uint8;
typedef unsigned int    uint16;
typedef unsigned long   uint32;

//CLOCK
#define CLK             12288000
#define CLK_KHZ         12288

//Timer
#define TIMERS
enum{TMR_CUSTOM,TMR_LAST};

//Timeout values
#define	TO_1sec         (337UL)

#define LED             PORTG_Bit1
#define LED_G           PORTC_Bit6
#define LED_R           PORTC_Bit7

#define USB_ON          PINF_Bit3

//KEYPAD
#define C1              PORTD_Bit5
#define C2              PORTD_Bit6
#define C3              PORTD_Bit7

#define R1              PINE_Bit3
#define R2              PINE_Bit4
#define R3              PINE_Bit5
#define R4              PINE_Bit6
#define R5              PINE_Bit7

#define SWON            PORTC_Bit0
#define GSM_FET         PORTC_Bit1
#define R_I             PINC_Bit4
#define RING            PORTB_Bit4
#define INHIB           PORTC_Bit2

#define CS              PORTB_Bit0

#define LCD_RS       	PORTB_Bit5
#define LCD_RW       	PORTB_Bit6
#define LCD_E        	PORTB_Bit7

#define LCD_MAX     	80
#define USART_BUF   	64
#define KEYPAD_MAX  	16

//init
void system_init(void);
void lcd_init(void);
void usart_init(void);
void watchdog_init(void);

void contact_clr(void);
void in_num_clr(void);
void fullname_clr(void);

void createFullname(void);

//battery
void adc_init(void);

//lcd
void lcd_e(void);
void lcd_home(void);
void lcd_clr(void);
void disp_clr(unsigned short init);
void write_char(unsigned char c);
void write_str(unsigned char* str);
void write_row(unsigned int sor, unsigned char* str);   //write row into display buffer
void move_row(unsigned short r1, unsigned short r2);    //move r1 row to r2 row
void disp_wchar(unsigned char c);                       //write char into display buffer
void disp_wstr(unsigned char* str);                     //write str into display buffer
unsigned char* rightJustify(unsigned char* row);        //make row rightjustified

//ftdi
void ftdi_transmit(unsigned char c);
unsigned char ftdi_receive(void);
unsigned char ftdi_receive_ns(void);
void ftdi_wb(unsigned char c);          //write char into ftdi rec buffer
unsigned char ftdi_pop(void);           //pop first char of ftdi buffer and shift left
void ftdi_clr(void);                    //clear ftdi rec buffer

//gsm
void gsm_tx(unsigned char* str);
void gsm_txc(unsigned char c);
unsigned char gsm_rx(void);
void gsm_wb(unsigned char c);
unsigned char gsm_pop(void);
void gsm_clr(void);
void service_clr(void);
void toDoEndCall(void);

//keypad
void keypad_clr(void);
void addkey(unsigned char c);

void SPI_MasterInit(void);
void SPI_Transmit(unsigned char* str);
void SPI_Tchar(unsigned int addr, unsigned char c);
void SPI_Receive(unsigned int row);
unsigned int SPI_count_rows(void);
void SPI_erase(void);
void SPI_WFTC(void);
unsigned short SPI_isReady(void);

void d_500us(void);
void d_1ms(void);
void d_5ms(void);
void d_10ms(void);
void d_50ms(void);
void d_200ms(void);
void d_1s(void);

#endif
