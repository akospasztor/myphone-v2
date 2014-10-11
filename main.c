#define ENABLE_BIT_DEFINITIONS

#include <ioavr.h>
#include <inavr.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "main.h"
#include "binary.h"

unsigned char about[] = "   myPhone v2.0 ~~~ written by Akos Pasztor   ";

/*** VARIABLES ****************************************************************/
unsigned char disp[LCD_MAX + 1];     //lcd buffer (ddram mirror)
unsigned int ptr_disp;               //lcd buffer pointer

unsigned long battery;               //battery value
unsigned char battery_str[5];        //battery value in string

int allapot;                         //device status     0:idle     1:keypad    2:contacts    3:call
int gsm_allapot;                     //gsm status
unsigned char service[17];           //service name

unsigned int maxrows;
unsigned char lastname[17];
unsigned char firstname[17];
unsigned char phone[17];
unsigned char statusbar[17];

unsigned char incoming_number[17];
unsigned char fullname[34];

unsigned char ftdi[USART_BUF + 1];   //FTDI receiver buffer
unsigned int ptr_ftdi;               //FTDI rec buffer pointer
unsigned char ftdi_char;             

unsigned char gsm[USART_BUF + 1];    //GSM receiver buffer
unsigned int ptr_gsm;                //GSM rec buffer pointer
unsigned char gsm_char;              
char * pch;                          //GSM rec buffer search
char gsm_csq[3];                     //GSM CSQ buffer
unsigned short gsm_csq_num;          //GSM CSQ in int
unsigned short gsm_error;            
unsigned short gsm_calling_state;    //0:ready   1:atd sent  2:dialing   3:call in progress

unsigned int spi_addr;              
unsigned int spi_row_counter;

unsigned char keys[KEYPAD_MAX + 1];  //KEYPAD buffer
int ptr_keys;                        //KEYPAD buffer pointer

extern unsigned int warning;
extern volatile slong timer[];

/*** FLAGS ********************************************************************/
unsigned short FLAG_dispClr;         
unsigned short FLAG_battery;

unsigned int FLAG_gsm_idle;
unsigned int FLAG_gsm;
unsigned int FLAG_gsm_ri;
unsigned int FLAG_gsm_clip_get;

unsigned int FLAG_ftdi;
short FLAG_ftdi_dwn;
short FLAG_spi_tx;

unsigned int FLAG_key;
unsigned int FLAG_key_counter;
unsigned short FLAG_key_state;

/*** TIMERS *******************************************************************/
unsigned int TMR_battery;
unsigned int TMR_spiready;
unsigned int TMR_gsm;
unsigned int TMR_gsm_csq;
unsigned int TMR_gsm_time;
unsigned int TMR_gsm_separator;
unsigned int TMR_gsm_ri;

/******************************************************************************/
void settimer(int timerID,slong value)
{
	uchar intsave;
	intsave=__save_interrupt();
	__disable_interrupt();
	timer[timerID]=value;
	__restore_interrupt(intsave);
}
/******************************************************************************/
slong gettimer(int timerID)
{
	slong value;
	uchar intsave;
	intsave=__save_interrupt();
	__disable_interrupt();
	value=timer[timerID];
	__restore_interrupt(intsave);
	return value;
}
/******************************************************************************/
void d_1us(void)   { __delay_cycles(CLK_KHZ/1000); } //1 usec
void d_50us(void)  { __delay_cycles(CLK_KHZ/20); }   //50 usec
void d_100us(void) { __delay_cycles(CLK_KHZ/10); }   //100 usec
void d_500us(void) { __delay_cycles(CLK_KHZ/2); }    //500 usec
void d_1ms(void)   { __delay_cycles(CLK_KHZ); }      //1 msec
void d_5ms(void)   { __delay_cycles(CLK/200); }      //5 msec
void d_10ms(void)  { __delay_cycles(CLK/100); }      //10 msec
void d_50ms(void)  { __delay_cycles(CLK/20); }       //50 msec
void d_100ms(void) { __delay_cycles(CLK/10); }       //100 msec
void d_200ms(void) { __delay_cycles(CLK/5); }        //200 msec
void d_1s(void)    { __delay_cycles(CLK); }          //1 sec

/******************************************************************************/
void system_init(void)
{       
    //clear flags
    FLAG_dispClr = 0;
    FLAG_battery = 0;
        
    FLAG_gsm = 0;
    FLAG_gsm_idle = 0;
    FLAG_gsm_ri = 0;
    FLAG_gsm_clip_get = 0;
    
    FLAG_ftdi = 0;
    FLAG_ftdi_dwn = 0;
    FLAG_spi_tx = 0;
    
    FLAG_key = 0;
    FLAG_key_state = 0;
    FLAG_key_counter = 0;
    
    //clear variables
    disp_clr(0);    
    ftdi_clr();
    gsm_clr();
    keypad_clr();
    
    battery = 0;
    memset(battery_str, '\0', sizeof(battery_str));
    
    contact_clr();
    in_num_clr();
    allapot = 0;
    gsm_allapot = 0;
    gsm_error = 0;
    gsm_calling_state = 0;
    memset(gsm_csq, '\0', sizeof(gsm_csq));
    
    spi_addr = 0;
    spi_row_counter = 0;
    
    //clear timers
    TMR_battery = 0;
    TMR_spiready = 0;  
    TMR_gsm = 0;
    TMR_gsm_csq = 0;
    TMR_gsm_time = 0;
    TMR_gsm_separator = 0;
    TMR_gsm_ri = 0;
    
    //port directions
    PORTC |= b00000110;         //GSM_FET: OFF, Vegfok OFF
	DDRC = b11101111;           //Ring Indicator: input
	DDRF = b11110110;           //USB ON, BAT: input
    DDRG = 0xFF;                //LED: output
    
    //set keypad
    C1 = 1; C2 = 1; C3 = 1;
    PORTE |= b11111000;         //pull-ups enable
    DDRD   = b11100000;         //columns: output
    DDRE   = b00000000;         //rows: input
        
    //timers
    TCNT2 = 0x40;               //Timer2 start (256-192=64)
	TIMSK = b01000000;          //Timer2 Interrupt enable
	TCCR2 = 0x03;               //Timer2 on, 64 prescaler    
}

void adc_init(void)
{
    ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);   //prescalar: div by 128   
    ADMUX |= (1 << REFS0);    //reference: AVCC
    ADCSRA |= (1 << ADFR);    //free-running
    ADCSRA |= (1 << ADEN);    //enable
    ADCSRA |= (1 << ADSC);    //start
}

void watchdog_init(void)
{
    WDTCR = (1<<WDCE)|(1<<WDE);
    WDTCR |= (1<<WDCE)|(1<<WDP0);
}

void contact_clr(void)
{
    memset(lastname, '\0', sizeof(lastname));
    memset(firstname, '\0', sizeof(firstname));
    memset(phone, '\0', sizeof(phone));
    memset(statusbar, '\0', sizeof(statusbar));
}

void in_num_clr(void)
{
    memset(incoming_number, '\0', sizeof(incoming_number));
}

void fullname_clr(void)
{
    memset(fullname, '\0', sizeof(fullname));
}

void createFullname(void)
{
    fullname_clr(); 
    
    int q=0, cntr=0;
    while(lastname[q] != '\0')
        fullname[cntr++] = lastname[q++];
    
    q=0;
    fullname[cntr++] = ' ';
    while(lastname[q] != '\0')
        fullname[cntr++] = firstname[q++];
    fullname[cntr] = '\0';
}

/******************************************************************************/
void lcd_init(void)
{
    warning = 1;    
    
    PORTA = 0x00; DDRA = 0xFF;
    PORTB = 0x00; DDRB = 0xFF;  
    d_50ms();
    LCD_RS=0; LCD_RW=0;
    
    PORTA = b00111100; lcd_e(); d_50us(); //8bit
    PORTA = b00001100; lcd_e(); d_50us(); //display ON, cursor OFF, blink OFF
    PORTA = b00000001; lcd_e(); d_5ms();  //clr
    PORTA = b00000110; lcd_e(); d_50us(); //entry mode

    warning = 0;
}

/******************************************************************************/
void lcd_e(void)
{
    LCD_E=1;
    d_1us();
    LCD_E=0;
}

/******************************************************************************/
void lcd_home(void)
{
    DDRA = 0xFF;
    LCD_RS=0; LCD_RW=0;
    
    PORTA = 0x02;
    lcd_e(); d_50us();
}

/******************************************************************************/
void lcd_clr(void)
{
    DDRA = 0xFF;
    LCD_RS=0; LCD_RW=0;
    
    PORTA = b00000001;
    lcd_e(); d_5ms();
}

/******************************************************************************/
void write_char(unsigned char c)
{
    DDRA = 0xFF;          
    LCD_RS=1; LCD_RW=0;
    
    PORTA = c;        
    lcd_e(); d_50us();
}

/******************************************************************************/
void write_str(unsigned char* str)
{    
    int i=0;
    for(i=0; str[i]!='\0'; ++i)
    {
        write_char(str[i]);
    }
}
/******************************************************************************/
void disp_clr(unsigned short init)
{
    memset(disp, ' ', sizeof(disp));
    disp[LCD_MAX] = '\0';
    ptr_disp = 0;
    
    if(init)
    {
        allapot = 0;
        write_row(1,"    myPhone     ");
        write_row(2,"      v2.0      ");
        write_row(3,"~~~~~~~~~~~~~~~~");
        write_row(4,"by  Akos Pasztor");
    }
}

/******************************************************************************/
void disp_wchar(unsigned char c)
{
    if(ptr_disp > 0x47) { ptr_disp = 0; }
    
    disp[ptr_disp] = c;
    
    switch(ptr_disp)
    {
        case 0x0F: //1st row ending
        ptr_disp = 0x28;
        break;
        
        case 0x37: //2nd row ending
        ptr_disp = 0x10;
        break;
        
        case 0x1F: //3rd row ending
        ptr_disp = 0x38;
        break;
        
        default:
        ptr_disp++;
        break;
    }
}

/******************************************************************************/
void disp_wstr(unsigned char* str)
{
    int i=0;
    for(i=0; str[i]!='\0'; ++i)
    {
        disp_wchar(str[i]);
    }
}

/******************************************************************************/
void write_row(unsigned int sor, unsigned char* str)
{
    int i, start = 0;
    switch(sor)
    {
        case 2: start = 0x28; break;
        case 3: start = 0x10; break;
        case 4: start = 0x38; break;
    }
    
    for(i=0; i<16; ++i)
    {
        if(str[i] == '\0') break;
        disp[i+start] = str[i];
    }
    while(i<16)
    {
        disp[i+start] = ' ';
        i++;
    }
}

/******************************************************************************/
void move_row(unsigned short r1, unsigned short r2)
{
    int i;
    if(r1 > 4 || r2 > 4) { return; }
    switch(r1)
    {
        case 1: r1=0x00; break;
        case 2: r1=0x28; break;
        case 3: r1=0x10; break;
        case 4: r1=0x38; break;
    }
    switch(r2)
    {
        case 1: r2=0x00; break;
        case 2: r2=0x28; break;
        case 3: r2=0x10; break;
        case 4: r2=0x38; break;
    }
    
    for(i=0; i<16; ++i)
    {
        disp[r2+i] = disp[r1+i];
    }
    
}

/******************************************************************************/
unsigned char* rightJustify(unsigned char* row)
{        
    int i;
    int length = strlen(row);
    for(i=length-1; i>=0; --i)
    {
        row[16-length+i] = row[i];
        row[i] = ' ';
    }
    return row;
}

/******************************************************************************/
void keypad_clr(void)
{
    memset(keys, ' ', sizeof(keys));
    keys[KEYPAD_MAX] = '\0';
    ptr_keys = KEYPAD_MAX-1;
}

/******************************************************************************/
void addkey(unsigned char c)
{
    if(ptr_keys >= 0)
    {
        int i;
        for(i=ptr_keys; i<KEYPAD_MAX-1; ++i)
        {
            keys[i] = keys[i+1];
        }
        keys[KEYPAD_MAX-1] = c;
        ptr_keys--;
    }
}

/******************************************************************************/
void usart_init(void)
{
    // USART initialization
    // Communication Parameters: 8 Data, 1 Stop, No Parity
    // USART Receiver: On
    // USART Transmitter: On
    // USART Mode: Asynchronous
    // FTDI Baud rate: 2400 - UBRR: 319 (0x13F) @ 12.288 MHz
    // GSM Baud rate: 2400 - UBRR: 319 (0x13F) @ 12.288 MHz
    
    //GSM    
    UCSR0A = 0x00;
    UCSR0B = 0x18; //enable
    UCSR0C = 0x06; //data, stop, parity
    
    UBRR0H = 0x01; //baud high
    UBRR0L = 0x3F; //baud low    

    //FTDI    
    UCSR1A = 0x00;
    UCSR1B = 0x18; //enable
    UCSR1C = 0x06; //data, stop, parity
    
    UBRR1H = 0x01; //baud high
    UBRR1L = 0x3F; //baud low    
}

/******************************************************************************/
void ftdi_transmit(unsigned char c)
{
	while(!(UCSR1A & (1<<UDRE1)));
	UDR1 = c;
}

/******************************************************************************/
unsigned char ftdi_receive(void)
{
	while(!(UCSR1A & (1<<RXC1)));
	return UDR1;
}

/******************************************************************************/
unsigned char ftdi_receive_ns(void)
{
	if((UCSR1A & (1<<RXC1)))
		return UDR1; 
	else
		return 0;
}
/******************************************************************************/
void ftdi_wb(unsigned char c)
{
    if(ptr_ftdi < USART_BUF) { ftdi[ptr_ftdi++] = c; }
}

/******************************************************************************/
unsigned char ftdi_pop(void)
{
    if(ptr_ftdi)
    {
        int i;
        char c = ftdi[0];
        for(i=1; i<ptr_ftdi; ++i)
        {
            ftdi[i-1] = ftdi[i];
        }
        ftdi[--ptr_ftdi] = '\0';
        return c;
    }
    return 0;
}

/******************************************************************************/
void ftdi_clr(void)
{
    memset(ftdi, '\0', sizeof(ftdi));
    ftdi[USART_BUF] = '\0';
    ptr_ftdi = 0;
}

/******************************************************************************/
void gsm_tx(unsigned char* str)
{
    int i;
    for(i=0; str[i] != '\0'; ++i)
    {
        gsm_txc(str[i]);
    }
}

/******************************************************************************/
void gsm_txc(unsigned char c)
{
    while(!(UCSR0A & (1<<UDRE0)));
	UDR0 = c;
}

/******************************************************************************/
unsigned char gsm_rx(void)
{
    if((UCSR0A & (1<<RXC0)))
		return UDR0; 
	else
		return 0;
}

/******************************************************************************/
void gsm_wb(unsigned char c)
{
    if(ptr_gsm < USART_BUF) { gsm[ptr_gsm++] = c; }
}

/******************************************************************************/
unsigned char gsm_pop(void)
{
    if(ptr_gsm)
    {
        int i;
        char c = gsm[0];
        for(i=1; i<ptr_gsm; ++i)
        {
            gsm[i-1] = gsm[i];
        }
        gsm[--ptr_gsm] = '\0';
        return c;
    }
    return 0;
}

/******************************************************************************/
void toDoEndCall(void)
{
    keypad_clr();
    
    INHIB = 1;      //vegfok ki
    
    allapot = 0;
    gsm_calling_state = 0;
    gsm_allapot = 10;
    
    FLAG_dispClr = 1;
    FLAG_gsm_idle = 1; 
    FLAG_gsm_ri = 0;
    FLAG_gsm_clip_get = 0;
    
    TMR_gsm = 2000;               
}

/******************************************************************************/
void gsm_clr(void)
{
    memset(gsm, '\0', sizeof(gsm));
    gsm[USART_BUF] = '\0';
    ptr_gsm = 0;
}

/******************************************************************************/
void service_clr(void)
{
    memset(service, '\0', sizeof(service));
}

/******************************************************************************/
void SPI_MasterInit(void)
{
    /* Set CS to 1 */ 
    CS = 1;
    
    /* Set MOSI and SCK output, SS output, MOSI input */
    DDRB |= (1<<DDB2)|(1<<DDB1)|(1<<DDB0);
    DDRB &= ~(1<<DDB3);
    
    /* Enable SPI, Master, set clock rate Fclk/16 */
    SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0);    
}

void SPI_WFTC(void)
{
    while(!(SPSR & (1<<SPIF)));
}

unsigned short SPI_isReady(void)
{
    return (SPSR & (1<<SPIF)) ? 1 : 0;
}

void SPI_Tchar(unsigned int addr, unsigned char c)
{
    CS = 0; SPDR = 0x06; SPI_WFTC(); CS = 1;
    
    CS = 0; SPDR = 0x02; SPI_WFTC();    
    SPDR = (addr >> 8);  SPI_WFTC();
    SPDR = addr;         SPI_WFTC();
    SPDR = c;            SPI_WFTC();
    CS = 1;
}

void SPI_Transmit(unsigned char* str)
{
    CS = 0; SPDR = 0x06; SPI_WFTC(); CS = 1;
    
    CS = 0;
    SPDR = 0x02; SPI_WFTC();
    SPDR = 0x00; SPI_WFTC();
    SPDR = 0x00; SPI_WFTC();
    
    int i;
    for(i=0; str[i] != '\0'; ++i)
    {
        if(i == 127) break;
        SPDR = str[i];
        SPI_WFTC();
    }
    
    CS = 1;
}

void SPI_Receive(unsigned int row)
{ 
    CS = 0;
    
    //read command
    SPDR = 0x03; SPI_WFTC();
    
    //address
    SPDR = ((row*64) >> 8); SPI_WFTC();
    SPDR = (row*64);        SPI_WFTC();
    
    int i;
    contact_clr();
    
    for(i=0; i<17; ++i)
    {
        SPDR = 0x00; SPI_WFTC();
        if(SPDR == ',') { break; }
        if(SPDR != (char)255) { lastname[i] = SPDR; }
    }
    for(i=0; i<17; ++i)
    {
        SPDR = 0x00; SPI_WFTC();
        if(SPDR == ',') { break; }
        if(SPDR != (char)255) { firstname[i] = SPDR; }
    }
    for(i=0; i<17; ++i)
    {
        SPDR = 0x00; SPI_WFTC();
        if(SPDR == ';' || SPDR == ',' || (char)0) { break; }
        if(SPDR != (char)255) { phone[i] = SPDR; }
    }
    
    CS = 1;
}

unsigned int SPI_count_rows(void)
{
    char c;
    int i = 0;
    int error = 0;
    
    CS = 0;    
    SPDR = 0x03; SPI_WFTC();
    
    SPDR = 0x00; SPI_WFTC();
    SPDR = 0x00; SPI_WFTC();
    
    error = 1;
    do
    {
        SPDR = 0x00; SPI_WFTC();
        c = SPDR;
        if(i < 64 && c == (char)0x00) { error = 0; }
        if(i >= 64 && error) { return 0; }
        ++i;
    } while(c != ';');
    
    CS = 1;
    return (i/64)+1;
}

void SPI_erase(void)
{
    CS = 0; SPDR = 0x06; SPI_WFTC(); CS = 1;
    CS = 0; SPDR = 0xC7; SPI_WFTC(); CS = 1;
}

void rec(void)
{
    int i;
    CS = 0;
    SPDR = 0x03; SPI_WFTC();
    SPDR = 0x00; SPI_WFTC();
    SPDR = 0x00; SPI_WFTC();
    for(i=0; SPDR != ';'; ++i)
    {
       SPDR = 0x00; SPI_WFTC();
       ftdi_transmit(SPDR); 
    }
    CS = 1;   
}

/******************************************************************************/
__C_task main(void)
{     
    system_init();
    adc_init();
    
    GSM_FET = 0;
    d_1s(); d_1s(); d_1s();
    
    lcd_init();
    usart_init();
    SPI_MasterInit();
    
    disp_clr(1);
    
    watchdog_init();
    
    __enable_interrupt();
       
    while(1)
    {   
        //BATTERY---------------------------------------------------------------
        if(FLAG_battery)
        {
            battery = (long)ADCL;
            battery += ((long)ADCH)*256;
            battery = 5*battery*100/1024;
            sprintf(battery_str, "%d", battery);
            
            ptr_disp = 0x38;
            disp_wstr("Bat: ");
            disp_wchar(battery_str[0]);
            disp_wchar('.');
            disp_wchar(battery_str[1]);
            disp_wchar('V');            
            
            TMR_battery = 2000;
            FLAG_battery = 0;
        }
        
        //FTDI------------------------------------------------------------------
        if(FLAG_ftdi)
        {
            if(FLAG_ftdi_dwn == 2)
            { 
                ptr_disp=0x00;
                disp_wchar( (char)0x7E );
                disp_wstr("MEM");
                
                spi_addr = 0;
                spi_row_counter = 0;
                FLAG_ftdi_dwn = 1;
            }
        
            //general: ECHO data back to sender & send to gsm
            if(!FLAG_ftdi_dwn)
            {
                ftdi_transmit( ftdi_char );
                gsm_txc(ftdi_char);
            }
            
            //set flags, vars
            ftdi_char = 0;            
            FLAG_ftdi = 0;
        }
        
        //SPI-------------------------------------------------------------------
        if(FLAG_spi_tx)
        {
            if(ptr_ftdi && !TMR_spiready)
            {
                char c = ftdi_pop();
                if(c == (char)13) 		//set new address when CR
                { 
                    SPI_Tchar( spi_addr, (char)0 );
                    spi_row_counter++;
                    spi_addr = 64*spi_row_counter;
                } 
                else
                {
                    SPI_Tchar( spi_addr, c );
                    spi_addr++;
                }
            }
            
            if(FLAG_ftdi_dwn < 0 && !ptr_ftdi)
            { 
                FLAG_ftdi_dwn = 0;
                FLAG_ftdi = 0;
                gsm_allapot = 10;
            }
            
            FLAG_spi_tx = 0;
        }
        
        //GSM IMCOMING CALL
        if(FLAG_gsm_ri==2 && !TMR_gsm && FLAG_gsm_idle)
        {
            FLAG_gsm_ri = 1;
            FLAG_gsm_idle = 0;
            allapot = 3;
            gsm_allapot = 22;
            INHIB = 0;
            
            disp_clr(0);
            write_row(3,"Incoming call:  ");
        }
        
        //GSM COMMAND-----------------------------------------------------------
        if(gsm_allapot == 3 && !TMR_gsm) //ATE0
        {
            gsm_tx("ATE0"); gsm_txc((char)13);
            gsm_tx("ATE0"); gsm_txc((char)13);
            TMR_gsm = 1000;
            gsm_allapot = 4;
        }
        if(gsm_allapot == 4 && !TMR_gsm) //ATV0
        {
            gsm_tx("ATV0"); gsm_txc((char)13);
            TMR_gsm = 500;
            gsm_allapot = 5;
        }
        if(gsm_allapot == 5 && !TMR_gsm) //check SIM
        {
            write_row(1,"                ");
            write_row(2,"                ");
            write_row(3,"Switching ON GSM");
            
            if((gsm[0] == 'A' && gsm[1] == 'T') || (gsm[0] == (char)48 && gsm[1] == (char)13))
            {
                write_row(4,"Searching ...   ");
                gsm_clr(); FLAG_gsm = 0;
                
                gsm_tx("AT%TSIM"); gsm_txc((char)13);
                gsm_allapot = 6;
            }
            else
            {   
                write_row(4,"GSM Error.      ");
                gsm_allapot = -1;
                gsm_error = 1;
            }
        }
        if(gsm_allapot == 7 && !TMR_gsm) //check CREG?
        {
            gsm_tx("AT+CREG?"); gsm_txc((char)13);
            gsm_allapot = 8;
        }
        if(gsm_allapot == 9 && !TMR_gsm) //set CLIP
        {
            gsm_tx("AT+CLIP=1"); gsm_txc((char)13);
            TMR_gsm = 2000;
            gsm_allapot = 10;
        }
        
        if(gsm_allapot == 10 && !TMR_gsm) //COPS?
        {
            if(FLAG_dispClr) { disp_clr(0); FLAG_dispClr = 0; }
            gsm_clr();
            gsm_tx("AT+COPS?"); gsm_txc((char)13);
            gsm_allapot = 11;
        }
        if(gsm_allapot == 12 && !TMR_gsm) //CSQ
        {
            gsm_tx("AT+CSQ"); gsm_txc((char)13);
            gsm_allapot = 13;
        }
        if(gsm_allapot == 14 && !TMR_gsm) //CCLK?
        {
            gsm_tx("AT+CCLK?"); gsm_txc((char)13);
            gsm_allapot = 15;
        }
        if(gsm_allapot == 22 && !TMR_gsm) //CPAS
        {
            gsm_tx("AT+CPAS"); gsm_txc((char)13);
            gsm_allapot = 23;
        }
        
        //GSM RESPONSE----------------------------------------------------------
        if(FLAG_gsm)
        {
            int i;
            
            //get CLIP
            if(FLAG_gsm == 9 && !FLAG_gsm_clip_get)
            {
                in_num_clr();
                pch = strchr(gsm, '"');
                if(pch != NULL)
                {                       
                    pch++;
                    i=0;
                    while( (*pch) != '"' )
                    {
                        incoming_number[i] = (*pch);
                        pch++; i++;
                    }
                    incoming_number[i] = '\0';
                    FLAG_gsm_clip_get = 1;
                    
                    //search in contacts
                    maxrows = SPI_count_rows();
                    for(i=1; i<maxrows; ++i)
                    {
                	    SPI_Receive(i);
                        int j=0, same = 0;
                        for(j=0; incoming_number[j] != '\0'; ++j)
                        {
                            same = 1;
                            if(incoming_number[j] != phone[j]) { same = 0; break; }
                        }
                        if(same)
                        {
                            move_row(2,1);
                            move_row(3,2);
                            write_row(3, "                ");
                            createFullname();
                            
                            ptr_disp = 0x10;
                            for(j=0; j<16; ++j)
                            {
                                if(fullname[j] == '\0') break;
                                if(j > 13) { disp_wstr(".."); break; }
                                else { disp_wchar(fullname[j]); }
                            }
                            break;
                        }
                    }
                    write_row(4, rightJustify(incoming_number));                    
                    
                    gsm_clr();
                    gsm_allapot = 22;
                    TMR_gsm = 500;
                    FLAG_gsm = 0;
                }   
            }
            
            //gsm_allapot            
            switch(gsm_allapot)
            {
                case 6:     //TSIM
                    if(strstr(gsm, "%TSIM 1") == NULL)
                    {
                        gsm_allapot = -2;
                        write_row(1,"NO SIM CARD");
                    }
                    else
                    {
                        TMR_gsm = 5000;
                        gsm_allapot = 7;
                    }
                    gsm_clr();
                break;
                
                case 8:     //CREG?
                    if(strstr(gsm, "+CREG: 0,1") == NULL)
                    {
                        write_row(2,"Switching ON GSM");
                        write_row(3,"Searching ...   ");
                        write_row(4,"No Carrier      ");
                        TMR_gsm = 5000;
                        gsm_allapot = 3;
                    }
                    else
                    { 
                        TMR_gsm = 500;
                        gsm_allapot = 9;
                    }
                    gsm_clr();
                break;
                
                case 11:    //COPS?
                    pch = strchr(gsm, '"');
                    if(pch != NULL)
                    {                       
                        pch++;
                        i=0;
                        while( (*pch) != '"' )
                        {
                            service[i] = (*pch);
                            pch++; i++;
                        }
                        service[i] = '\0';
                        
                        if(!FLAG_gsm_idle)
                        { 
                            write_row(2,"Switching ON GSM");
                            write_row(3,"Searching ...   ");
                            write_row(4,"Net: ");
                            ptr_disp=0x3D;
                            disp_wstr(service);
                            TMR_gsm = 1500;
                        }
                        else if(!FLAG_ftdi_dwn)
                        {
                            ptr_disp=0x00;
                            disp_wstr(rightJustify(service));
                            TMR_gsm = 200;
                        }
                        
                        gsm_allapot = 12;
                    }
                    else
                    {
                        if(!FLAG_gsm_idle) { write_row(4, "No Carrier      "); }
                        else
                        {
                            write_row(1, "      No Carrier");
                            write_row(2, "                ");
                        }
                        TMR_gsm = 3000;
                        gsm_allapot = 3;
                    }
                    gsm_clr();
                break;
                
                case 13:    //CSQ
                    if(!FLAG_gsm_idle)
                    {
                        disp_clr(0);
                        write_row(1,rightJustify(service));  
                    }
                    
                    pch = strchr(gsm, ',');
                    if(pch != NULL)
                    {
                        if(*(pch-2) == ' ')
                        { 
                            gsm_csq[0] = *(pch-1);
                            gsm_csq[1] = '\0';
                        } 
                        else
                        { 
                            gsm_csq[0] = *(pch-2);
                            gsm_csq[1] = *(pch-1);
                        }
                        
                        gsm_csq_num = atoi(gsm_csq);
                        
                        ptr_disp = 0x28;
                        disp_wstr("Signal: ");
                        
                        if(gsm_csq_num == 99) { disp_wstr(" Unknown"); } 
                        else
                        {
                            gsm_csq_num++;
                            gsm_csq_num /= 4;
                            for(i=0; i<(8-gsm_csq_num) && i<7; ++i)
                            { 
                                disp_wchar( (char)0xDB );
                            }
                            do {
                                disp_wchar( (char)0xFF );
                                ++i;
                            } while(i<8);
                        }
                    }
                    TMR_gsm = 100;
                    gsm_allapot = 14;
                    gsm_clr();
                break;
                
                case 15:    //CCLK?
                    pch = strchr(gsm, '"');
                    if(pch != NULL)
                    {
                        pch++;
                        //yy
                            ptr_disp=0x10; disp_wstr("20");
                            disp_wchar(*pch); pch++;
                            disp_wchar(*pch); pch++;
                            disp_wchar('.');
                        pch++;    
                        //mm
                            disp_wchar(*pch); pch++;
                            disp_wchar(*pch); pch++;
                            disp_wchar('.');
                        pch++;
                        //dd
                            disp_wchar(*pch); pch++;
                            disp_wchar(*pch); pch++;
                        pch++;
                        //hh
                            ptr_disp=0x1B;
                            disp_wchar(*pch); pch++;
                            disp_wchar(*pch); pch++;
                        pch++;
                        //mm
                            ptr_disp=0x1E;
                            disp_wchar(*pch); pch++;
                            disp_wchar(*pch); pch++;
                    }
                    else
                    {
                        write_row(4, "Date&Time Error.");
                    }
                    
                    gsm_allapot = 16;
                    gsm_clr();
                break;
                
                case 20:    //DIALING-------------------------------------------                    
                    switch(gsm_calling_state)
                    {
                        case 1:     //atd sent, waiting for 0 or 4(error)
                            if(FLAG_gsm == 4)
                            {
                                move_row(3,2);
                                move_row(4,3);
                                write_row(4, "     Call Failed");
                                toDoEndCall();
                            }
                            else
                            {
                                move_row(3,2);
                                move_row(4,3);
                                write_row(4, "         Dialing");
                                gsm_calling_state = 2;
                            }
                        break;
                        
                        case 2:     //dialing
                            switch(FLAG_gsm)
                            {                                
                                case 3:
                                    move_row(2,1);
                                    move_row(3,2);
                                    move_row(4,3);
                                    write_row(4, "       No Answer");
                                    toDoEndCall();
                                break;
                                
                                case 7:
                                    move_row(2,1);
                                    move_row(3,2);
                                    move_row(4,3);
                                    write_row(4, "       User Busy");
                                    toDoEndCall();
                                break;
                                
                                case 2:
                                    INHIB = 0;              //amplifier on
                                    move_row(2,1);
                                    move_row(3,2);
                                    move_row(4,3);
                                    write_row(4, "       Connected");
                                    gsm_calling_state = 3;
                                break;
                            }
                        break;
                        
                        case 3:     //connected
                            if(FLAG_gsm == 3)
                            {
                                move_row(2,1);
                                move_row(3,2);
                                move_row(4,3);
                                write_row(4, "      Call Ended");
                                toDoEndCall();
                            }
                        break;
                    } //switch: gsm_calling_state
                    
                    gsm_clr();
                break;
                
                case 23:    //CPAS----------------------------------------------                   
                    //get phone number                   
                    if(strstr(gsm, "+CPAS: 3") != NULL)
                    {
                        TMR_gsm = 1500;
                        gsm_allapot = 22;   
                    }
                    else
                    {
                        INHIB = 1;
                        toDoEndCall();
                    }
                    gsm_clr();
                break;
                
                case 24:    //ANSWER CALL---------------------------------------
                    if(FLAG_gsm == 4 && gsm_calling_state == 1)
                    {
                        move_row(2,1);
                        move_row(3,2);
                        move_row(4,3);
                        write_row(4,"      No response.");
                        toDoEndCall();
                    }
                    else if(gsm_calling_state != 2)
                    {
                        move_row(2,1);
                        move_row(3,2);
                        move_row(4,3);
                        write_row(4, "       Connected");
                        gsm_calling_state = 2;
                    }
                    
                    if(FLAG_gsm == 3 && gsm_calling_state == 2)
                    {
                        move_row(2,1);
                        move_row(3,2);
                        move_row(4,3);
                        write_row(4, "      Call Ended");
                        toDoEndCall();
                    }
                    gsm_clr();
                break;
                       
            } //switch-close: gsm_allapot
            
            FLAG_gsm = 0;
        } //if-close: FLAG_gsm
        
        //KEYPAD----------------------------------------------------------------        
        if(FLAG_key)
        {
            //ANSWER btn is active during incoming call
            if(allapot == 3 && FLAG_gsm_ri == 1 && FLAG_key == 13)
            {
                //PWM
                TCCR0 = 0x00;
                
                gsm_tx("ATA");
                gsm_txc((char)13);
                
                FLAG_gsm_ri = 0;
                gsm_allapot = 24;
                gsm_calling_state = 1;
                TMR_gsm = 100;
            }
            
            //END btn is active during call
            if(allapot == 3 && FLAG_key == 15)
            {
                //PWM
                TCCR0 = 0x00;
                
                move_row(2,1);
                move_row(3,2);
                move_row(4,3);
                write_row(4,"     Ending Call");
                
                gsm_tx("ATH");
                gsm_txc((char)13);
                
                toDoEndCall();
                
                FLAG_key = 0;
                FLAG_key_state = 0;
                FLAG_key_counter = 0;
            }
            //active processes which are blocking the keypad
            else if(FLAG_ftdi_dwn > 0 || !FLAG_gsm_idle || allapot == 3)
            {
                FLAG_key = 0;
                FLAG_key_state = 0;
                FLAG_key_counter = 0;
            }
            else
            {                
                //NUMBERS AND *+# KEYS
                if(FLAG_key >= 1 && FLAG_key <= 9)
                { 
                    addkey((char)(FLAG_key+48));
                    allapot = 1;
                }
                if(FLAG_key == 11)
                { 
                    if(FLAG_key_counter > 1000) { addkey('+'); }
                    else { addkey((char)48); }
                    allapot = 1;
                }
                if(FLAG_key == 10) { addkey('*'); allapot = 1; }
                if(FLAG_key == 12) { addkey('#'); allapot = 1; }
                
                //CTRL KEYS
                if(FLAG_key == 13)
                {
                    if( FLAG_gsm_idle && (allapot==2 || (allapot==1 && ptr_keys < KEYPAD_MAX-1)) )    //---DIAL-----------------------
                    {
                        move_row(3,4);
                        write_row(1,"                ");
                        write_row(2,"                ");
                        write_row(3,"        Calling:");
                        
                        gsm_tx("ATD");
                        if(allapot == 1 && ptr_keys < KEYPAD_MAX-1)
                        {
                            int i;
                            for(i=0; keys[i]!='\0'; ++i)
                            {
                                if(keys[i]!=' ') { gsm_txc(keys[i]); }
                            }
                        }
                        if(allapot == 2) { gsm_tx(phone); }
                        gsm_txc(';');
                        gsm_txc((char)13);
                        
                        FLAG_gsm_idle = 0;
                        gsm_calling_state = 1;
                        gsm_allapot = 20;
                        allapot = 3;
                    }
                }
                
                if(FLAG_key == 15)
                {
                    keypad_clr();
                    disp_clr(0);
                    allapot = 0;
                    gsm_allapot = 10;
                    TMR_gsm_separator = 2000;
                }
                
                if(FLAG_key == 14)
                { 
                    if(allapot == 1) { keypad_clr(); disp_clr(0); }
                    if(allapot != 2) { spi_row_counter = 1; disp_clr(0); }
                    
                    allapot = 2;
                    if(spi_row_counter == 1) { maxrows = SPI_count_rows(); }
                    if(spi_row_counter >= maxrows) { spi_row_counter = 1; }
                    
                    if(!maxrows) { write_row(1,"No Data."); }
                    else
                    {
                        SPI_Receive(spi_row_counter);
                        sprintf(statusbar, "%d/%d", spi_row_counter, maxrows-1);
                        
                        write_row(1, lastname);
                        write_row(2, firstname);
                        write_row(3, rightJustify(phone));
                        //write_row(3, phone);
                        write_row(4, statusbar);
                        spi_row_counter++;
                    }
                }
                
                //ftdi_transmit(FLAG_key);
                FLAG_key = 0;
                FLAG_key_state = 0;
                FLAG_key_counter = 0;
                
                //keys to disp
                if(allapot == 1 && ptr_keys < KEYPAD_MAX-1)
                {
                    if(ptr_keys == KEYPAD_MAX-2) { disp_clr(0); disp_wstr("KEYPAD:"); }
                    ptr_disp = 0x10;
                    disp_wstr(keys);
                }
            }
        }
        
        
    } //loop
    
}
