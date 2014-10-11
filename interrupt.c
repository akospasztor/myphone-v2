#include <ioavr.h>
#include <inavr.h>
#include <string.h>

#include "interrupt.h"
#include "main.h"

/******************************************************************************/
#pragma vector=TIMER2_OVF_vect
__interrupt void TIMER2_OVF_handler(void)
{
    //init interrupt
    __disable_interrupt();
    TCNT2=0x40;
    
    __watchdog_reset();
    
    //LED-----------------------------------------------------------------------
    switch(warning)
    {        
        case 1:
            if(led_c < WARN_ON) { LED_G=1; LED_R=1; } else { LED_G=0; LED_R=0;}       
            led_c++;
            if(led_c > WARN_MAX) { led_c = 0; }
        break;
        
        case 2:
            if(led_c < ERR_ON) { LED_G=0; LED_R=1; } else { LED_G=0; LED_R=0;}       
            led_c++;
            if(led_c > ERR_MAX) { led_c = 0; }
        break;
        
        default:
        case 0:
            if(led_c < HBIT_ON) { LED_G=1; LED_R=0; } else { LED_G=0; LED_R=0; }       
            led_c++;
            if(led_c > HBIT_MAX) { led_c = 0; }
        break;
    }
    
    //busy led
    LED = (FLAG_gsm_idle) ? 0 : 1;
    
    //LCD-----------------------------------------------------------------------
    if(lcd_ptr >= LCD_MAX) { lcd_ptr = 0; lcd_home(); } 
    else { write_char(disp[lcd_ptr++]); }
    
    //FTDI----------------------------------------------------------------------
    rx_ftdi = ftdi_receive_ns();
    if(rx_ftdi && FLAG_gsm_idle)
    { 
        ftdi_char = rx_ftdi;
        rx_ftdi = 0;
        
        switch(ftdi_char)
        {
            case '!':       FLAG_ftdi_dwn = 2;
                            ftdi_clr();
                            SPI_erase();
                            break;
            
            case ';':       if(FLAG_ftdi_dwn > 0) { FLAG_ftdi_dwn = -1; }
                            ftdi_wb(ftdi_char);
                            break;
            
            case (char)10:  if(!FLAG_ftdi_dwn) { ftdi_wb(ftdi_char); }
                            break;
            
            default:        ftdi_wb(ftdi_char); break;
        }
        
        FLAG_ftdi = 1;
    }
    
    //SPI-----------------------------------------------------------------------
    if(FLAG_ftdi_dwn && FLAG_ftdi_dwn <= 1) { FLAG_spi_tx = 1; }
    
    
    //GSM-----------------------------------------------------------------------
    rx_gsm = gsm_rx();
    if(rx_gsm)
    { 
        gsm_char = rx_gsm;
        rx_gsm = 0;
        gsm_wb(gsm_char);
        ftdi_transmit(gsm_char);
        
        //general response
        if( (gsm[ptr_gsm-2] == (char)48) && (gsm[ptr_gsm-1] == (char)13) ) { FLAG_gsm = 1; }
        
        //CLIP response
        if( (gsm_allapot == 22 || gsm_allapot == 23) && (gsm[ptr_gsm-2] == (char)48) && (gsm[ptr_gsm-1] == (char)13) )
        { 
            if(strstr(gsm, "+CLIP:") != NULL) { FLAG_gsm = 9; }
        }
        
        //ATD responses
        if( (gsm_allapot == 20) && (gsm[ptr_gsm-2] == '4') && (gsm[ptr_gsm-1] == (char)13) )    { FLAG_gsm = 4; }   //4: error
        if( (gsm_allapot == 20) && (gsm[ptr_gsm-2] == '3') && (gsm[ptr_gsm-1] == (char)13) )    { FLAG_gsm = 3; }   //3: no carrier
        if( (gsm_allapot == 20) && (gsm[ptr_gsm-2] == '7') && (gsm[ptr_gsm-1] == (char)13) )    { FLAG_gsm = 7; }   //7: busy
        if( (gsm_allapot == 20) && (gsm[ptr_gsm-2] == 'O') && (gsm[ptr_gsm-1] == 'K') )         { FLAG_gsm = 2; }   //2: answer
        
        //ATA responses
        if( (gsm_allapot == 24) && (gsm[ptr_gsm-2] == '4') && (gsm[ptr_gsm-1] == (char)13) )    { FLAG_gsm = 4; }   //4: error
        if( (gsm_allapot == 24) && (gsm[ptr_gsm-2] == '3') && (gsm[ptr_gsm-1] == (char)13) )    { FLAG_gsm = 3; }   //3: no carrier
    }
    
    if(!TMR_gsm)
    {
        switch(gsm_allapot)
        {          
            case 0:
                TMR_gsm = 3000;
                gsm_allapot = 1;
            break;
            
            case 1:
                disp_clr(0);
                gsm_error = 0;
                write_row(4,"Switching ON GSM");
                SWON = 1;
                TMR_gsm = 50;
                gsm_allapot = 2;
            break;
            
            case 2:
                SWON = 0;
                FLAG_gsm_idle = 0;
                TMR_gsm = 8500;
                gsm_allapot = 3;
            break;
                        
            case 16: 
                TMR_gsm_time = 20000;
                FLAG_gsm_idle = 1;
                gsm_allapot = 17;
            break;
        }
    }
    
    //Ring Indicator
    if(!R_I && FLAG_gsm_idle && !FLAG_gsm_ri)
    {
        FLAG_gsm_ri = 2;
    }
    //Ringing
    if(FLAG_gsm_ri && !TMR_gsm_ri)
    {
        if(TCCR0) { TCCR0 = 0x00; }
        else { TCCR0 = 0x73; OCR0 = 0x40; }
        TMR_gsm_ri = 1000;
    }
    
    //separator
    if(!TMR_gsm_separator && FLAG_gsm_idle && !allapot)
    {
        ptr_disp = 0x1D;
        if(disp[0x1D] == ':')       { disp_wchar(' '); }
        else if(disp[0x1D] == ' ')  { disp_wchar(':'); }
        TMR_gsm_separator = 800;
    }
    
    //network, datetime
    if(!TMR_gsm_time && gsm_allapot == 17 && FLAG_gsm_idle && !allapot && !FLAG_ftdi)  { gsm_allapot = 10; }
    
    //battery
    if(!TMR_battery && FLAG_gsm_idle && !allapot && !FLAG_battery && gsm_allapot == 17) { FLAG_battery = 1; }
    
    //usb on
    if(FLAG_gsm_idle && !allapot && gsm_allapot == 17)
    {
        ptr_disp=0x43;
        if(USB_ON) { disp_wstr("CHARG"); }
        else       { disp_wstr("     "); }
    }
    
    //KEYPAD--------------------------------------------------------------------
    C1 = 0;
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R1) { keyactive = 1;  FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R2) { keyactive = 4;  FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R3) { keyactive = 7;  FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R4) { keyactive = 10; FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R5) { keyactive = 13; FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    C1 = 1;
    C2 = 0;
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R1) { keyactive = 2;  FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R2) { keyactive = 5;  FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R3) { keyactive = 8;  FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R4) { keyactive = 11; FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R5) { keyactive = 14; FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    C2 = 1;
    C3 = 0;
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R1) { keyactive = 3;  FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R2) { keyactive = 6;  FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R3) { keyactive = 9;  FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R4) { keyactive = 12; FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    if(FLAG_key_state == KEY_FALL-1 || FLAG_key_state == 0) { if(!R5) { keyactive = 15; FLAG_key_state = KEY_FALL; FLAG_key_counter++; } }
    C3 = 1;
    
    if(FLAG_key_counter >= 50)
    {
        if(FLAG_key_state == 1 && FLAG_key == 0)
        {
            FLAG_key = keyactive;
        }
    }
    
    if(FLAG_key_state > 0) { FLAG_key_state--; }
    else { keyactive = 0; FLAG_key_counter = 0; }
    
    //TIMERS--------------------------------------------------------------------
    if(TMR_battery)         { TMR_battery--; }
    if(TMR_spiready)        { TMR_spiready--; }
    if(TMR_gsm)             { TMR_gsm--; }
    if(TMR_gsm_csq)         { TMR_gsm_csq--; }
    if(TMR_gsm_time)        { TMR_gsm_time--; }
    if(TMR_gsm_separator)   { TMR_gsm_separator--; }
    if(TMR_gsm_ri)          { TMR_gsm_ri--; }
    
    //END INTERRUPT------------------------------------------------------------- 
    __enable_interrupt();
}
/******************************************************************************/
