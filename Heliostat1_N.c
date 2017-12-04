/*********************************************************************************************/
/* Heliostat - control a mirror driven by 2 motors to reflect sunbeam to a fixed target      */
/* Project name: Heliostat1                  Programmer: Ying Sun                            */
/* Update history: 11/24/2017 initiated			                                             */
/*********************************************************************************************/

/**************************** Specify the chip that we are using *****************************/
#include <p18cxxx.h>
#include <math.h>
#include <stdlib.h>
#include <xc.h>

/************************* Configure the Microcontroller PIC18f4525 **************************/
#pragma config OSC = XT
#pragma config WDT = OFF
#pragma config PWRT = OFF
#pragma config FCMEN = OFF
#pragma config IESO = OFF
#pragma config BOREN = ON
#pragma config BORV = 2
#pragma config WDTPS = 128
#pragma config PBADEN = OFF
#pragma config DEBUG = OFF
#pragma config LVP = OFF
#pragma config STVREN = OFF
#define _XTAL_FREQ 4000000

/******************************** Define Prototype Functions *********************************/
void Delay_ms(unsigned int x);
void Transmit(unsigned char value);
void PrintNum(unsigned char value, unsigned char position);
void PrintNum1(unsigned char value, unsigned char position);
void PrintNum2(unsigned char value, unsigned char position);
void SetupSerial();
void ClearScreen();
void Backlight(unsigned char state);
void SetPosition(unsigned char position);
void PrintLine(const unsigned char *string, unsigned char numChars);
void PrintInt(int value, unsigned char position);
void PrintInt1(int value, unsigned char position);
void write_eeprom(unsigned char address, unsigned char data);
unsigned char read_eeprom (unsigned short address); 
void interrupt isr(void);

/************************************** Global variables *************************************/
unsigned char mode, update1, motor_on, motor_plus, home_on;
unsigned char debounce0, debounce1, debounce2, debounce3;
unsigned char LEDcount, output, output1, output2, counter, counter1, skipCount;
unsigned char temp, sec_cnt, update_day, update_hr, update_min, update_sec;
unsigned char hr, min, sec, day_h, day_l, up, pan100, tilt100, stop_pan, stop_tilt;
int day, pan_count, tilt_count;

void Delay_ms(unsigned int x){ 	/****** Generate a delay for x ms, assuming 4 MHz clock ******/
    unsigned char y;
    for(;x > 0; x--) for(y=0; y< 82;y++);
}

void Transmit(unsigned char value) {  /********** send an ASCII Character to USART ***********/
    while(!PIR1bits.TXIF) continue;		// Wait until USART is ready
    TXREG = value;						// Send the data
    while (!PIR1bits.TXIF) continue;	// Wait until USART is ready
    Delay_ms(4); // Give the LCD some time to listen to what we've got to say.
}

void PrintNum(unsigned char value1, unsigned char position1){ /** Print number at position ***/
    int units, tens, hundreds, thousands;
    SetPosition(position1);			// Set at the present position
    hundreds = value1 / 100;		// Get the hundreds digit, convert to ASCII and send
    if (hundreds != 0) Transmit(hundreds + 48);
    else Transmit(20);
    value1 = value1 - hundreds * 100;
    tens = value1 / 10;				// Get the tens digit, convert to ASCII and send
    Transmit(tens + 48);
    units = value1 - tens * 10;
    Transmit(units + 48);			// Convert to ASCII and send
}

void PrintNum1(unsigned char value1, unsigned char position1){ /** Print number at position **/
    int units, tens, hundreds, thousands;
    SetPosition(position1);			// Set at the present position
    hundreds = value1 / 100;		// Get the hundreds digit, convert to ASCII and send
//    if (hundreds != 0) Transmit(hundreds + 48);
//    else Transmit(20);
    value1 = value1 - hundreds * 100;
    tens = value1 / 10;				// Get the tens digit, convert to ASCII and send
//    Transmit(tens + 48);
    units = value1 - tens * 10;
    Transmit(units + 48);			// Convert to ASCII and send
}

void PrintNum2(unsigned char value1, unsigned char position1){ /** Print number at position **/
    int units, tens, hundreds, thousands;
    SetPosition(position1);			// Set at the present position
    hundreds = value1 / 100;		// Get the hundreds digit, convert to ASCII and send
//    if (hundreds != 0) Transmit(hundreds + 48);
//    else Transmit(20);
    value1 = value1 - hundreds * 100;
    tens = value1 / 10;				// Get the tens digit, convert to ASCII and send
    Transmit(tens + 48);
    units = value1 - tens * 10;
    Transmit(units + 48);			// Convert to ASCII and send
}

void SetupSerial(){  /*********** Set up the USART Asynchronous Transmit (pin 25) ************/
// For LCD - use SPBRG = 25;  9600 BAUD at 4MHz: 4,000,000/(16x9600) - 1 = 25.04
// For Bluetooth - use SPBRG = 1; 115200 BAUD at 4MHz: 4,000,000/(16x115200) - 1 = 1
    TRISC = 0x80;					// Transmit and receive, 0xC0 if transmit only
    SPBRG = 25;
    TXSTAbits.TXEN = 1;				// Transmit enable
    TXSTAbits.SYNC = 0;				// Asynchronous mode
    RCSTAbits.CREN = 1;				// Continuous receive (receiver enabled)
    RCSTAbits.SPEN = 1;				// Serial Port Enable
    TXSTAbits.BRGH = 1;				// High speed baud rate
}

void ClearScreen(){   /************************** Clear LCD Screen ***************************/
    Transmit(254);					// See datasheets for Serial LCD and HD44780
    Transmit(0x01);					// Available on our course webpage
}

void Backlight(unsigned char state){  /************* Turn LCD Backlight on/off ***************/
    Transmit(124);
    if (state) Transmit(0x9D);		// If state == 1, backlight on; ox96 for 73% on
    else Transmit(0x81);			// otherwise, backlight off
}

void SetPosition(unsigned char position){ 	/********** Set LCD Cursor Position  *************/
    Transmit(254);
    Transmit(128 + position);
}

void PrintLine(const unsigned char *string, unsigned char numChars){ /**** Print characters ****/
    unsigned char count;
    for (count=0; count<numChars; count++) Transmit(string[count]);
}

void PrintInt(int value, unsigned char position){ /******** Print number at position *********/
    int units, tens, hundreds, thousands;
    SetPosition(position);				// Set at the present position
    if (value > 9999) {
        PrintLine((const unsigned char*)"Over", 4);
        return;
    }
    if (value < -9999) {
        PrintLine((const unsigned char*)"Under", 5);
        return;
    }
    if (value < 0) {
        value = -value;
        Transmit(45);               // - sign
    }
//    else Transmit(43);            // + sign
    thousands = value / 1000;		// Get the thousands digit, convert to ASCII and send
    if (thousands != 0) Transmit(thousands + 48);
    value = value - thousands * 1000;
    hundreds = value / 100;			// Get the hundreds digit, convert to ASCII and send
    if (hundreds != 0) Transmit(hundreds + 48);
    value = value - hundreds * 100;
    tens = value / 10;				// Get the tens digit, convert to ASCII and send
    Transmit(tens + 48);
    units = value - tens * 10;
    Transmit(units + 48);			// Convert to ASCII and send
}

void PrintInt2(int value, unsigned char position){ /******** Print number at position ********/
    int units, tens, hundreds, thousands;
    SetPosition(position);				// Set at the present position
    if (value > 9999) {
        PrintLine((const unsigned char*)"Over", 4);
        return;
    }
    if (value < -9999) {
        PrintLine((const unsigned char*)"Under", 5);
        return;
    }
    if (value < 0) {
        value = -value;
        Transmit(45);
    }
//    else Transmit(43);
    thousands = value / 1000;		// Get the thousands digit, convert to ASCII and send
    if (thousands != 0) Transmit(thousands + 48);
    value = value - thousands * 1000;
    hundreds = value / 100;			// Get the hundreds digit, convert to ASCII and send
    Transmit(hundreds + 48);
    value = value - hundreds * 100;
    tens = value / 10;				// Get the tens digit, convert to ASCII and send
    Transmit(tens + 48);
    units = value - tens * 10;
    Transmit(units + 48);			// Convert to ASCII and send
}

void write_eeprom(unsigned char address, unsigned char data) /******* Write to EEPROM ********/
{
    while (EECON1bits.WR);      // make sure it's not busy with an earlier write.                                 ... so try doing it directly as the datasheet defines.
    EEADR = address;
    EEDATA = data;
    EECON1bits.EEPGD = 0;
    EECON1bits.CFGS  = 0;
    EECON1bits.WREN  = 1;
    INTCONbits.GIE   = 0;
    EECON2 = 0x55;              // required sequence start
    EECON2 = 0xAA;
    EECON1bits.WR    = 1;
    INTCONbits.GIE   = 1;       // required sequence end
}

unsigned char read_eeprom (unsigned short address) /************ Read from EEPROM ************/
{   while (EECON1bits.WR);      // make sure it's not busy with an earlier write.
    EEADR = address;
    EECON1bits.EEPGD = 0;
    EECON1bits.CFGS  = 0;
    EECON1bits.RD    = 1;
    return (EEDATA);
}

void interrupt isr(void) { /************ high priority interrupt service routine *************/
    if (INTCONbits.TMR0IF == 1) {	// When there is a timer0 overflow, this loop runs
        INTCONbits.TMR0IE = 0;		// Disable TMR0 interrupt
        INTCONbits.TMR0IF = 0;		// Reset timer 0 interrupt flag to 0
        TMR0H = 0xD9;       // Reload TMR0 for 10 ms count, Sampling rate = 240 Hz
        TMR0L = 0x20;       // 0xFFFF-0x2710 = 0xD85F = 10,000 adjusted to D920
        PORTCbits.RC0 = !PORTCbits.RC0; // Toggle pin 15;
        sec_cnt++;
        if (sec_cnt == 100) {
            sec_cnt = 0;  sec++;  update_sec = 1;
            if (sec == 60) {
                sec = 0;    min++;  update_min = 1;
                if (min == 60) {
                    min = 0;    hr++;   update_hr = 1;
                    if (hr == 24) {
                        hr = 0; day++;  update_day = 1;
                        if (day == 366) day = 0;
                    }
                }
            }
        }
        if (debounce0) debounce0--;
        if (debounce1) debounce1--;
        if (debounce2) debounce2--;
        if (debounce3) debounce3--;
        INTCONbits.TMR0IE = 1;		// Enable TMR0 interrupt
    }
    if (INTCONbits.INT0IF == 1) {	// INT0 (pin 33) negative edge - Pan Hall A
        INTCONbits.INT0IE = 0;		// Disable interrupt
        INTCONbits.INT0IF = 0;		// Reset interrupt flag
        pan100++;
        if (pan100 >= 100) {
            pan100 = 0;
            update1 = 1;
            if (motor_plus) pan_count++;
            else pan_count--;
        }
        INTCONbits.INT0IE = 1;		// Enable interrupt
    }
    if (INTCON3bits.INT1IF == 1) {	// INT1 (pin 34) negative edge - Tilt Hall A
        INTCON3bits.INT1IE = 0;		// Disable interrupt
        INTCON3bits.INT1IF = 0;		// Reset interrupt flag
        tilt100++;
        if (tilt100 >= 100) {
            tilt100 = 0;
            update1 = 1;
            if (motor_plus) tilt_count++;
            else tilt_count--;           
        }        
        INTCON3bits.INT1IE = 1;		// Enable interrupt
    }
    if (INTCON3bits.INT2IF == 1) {	// INT1 (pin 35) either edge - Advance mode 
        INTCON3bits.INT2IE = 0;		// Disable interrupt
        INTCON3bits.INT2IF = 0;		// Reset interrupt flag
        if (debounce0 == 0) {
            if (mode == 4) home_on = 0; // leaving home/reset
            mode++;
            if (mode == 8) mode = 0;
            update1 = 1;
            debounce0 = 10;			// Set switch debounce delay counter decremented by TMR0
        }
        INTCON3bits.INT2IE = 1;		// Enable interrupt
    }
}

void main(){   /****************************** Main program **********************************/
    TRISB = 0b00010111;			// RB0-2, 4 as inputs, others outputs, RB3 drives red LED
    TRISC = 0b00000000;			// RC0 as output, 100 Hz calibration
    TRISD = 0b00001111;			// Set top 4 bits of port D as outputs to drive the motors
    PORTD = 0;					// Set port D to 0's
    SetupSerial();				// Set up USART Asynchronous Transmit for LCD display
    Delay_ms(100);	
    Transmit(18);               // Ctl R to reset BAUD rate to 9600
    Delay_ms(2500);				// Wait until the LCD display is ready
    Backlight(1);               // turn LCD display backlight on
    ClearScreen();              // Clear screen and set cursor to first position
    PrintLine((const unsigned char*)"  Heliostat 1", 13);
    SetPosition(64);            // Go to beginning of Line 2;
    PrintLine((const unsigned char*)"Motor Controller",16);	// Put your trademark here
    Delay_ms(3000);
    ClearScreen();              // Clear screen and set cursor to first position
    T0CON = 0b10001000;			// Turn on TMR0 and use the prescaler 000 (1:2))
    INTCON = 0b10110000;		// GIE(7) = TMR0IE = INT0IE = 1
    INTCONbits.TMR0IF = PIR1bits.TMR1IF = 0;
    INTCONbits.TMR0IE = 1;		// Enable TMR0 interrupt
    INTCON2bits.INTEDG0 = 0;	// Set pin 33 (RB0/INT0) for negative edge trigger
    INTCON2bits.INTEDG1 = 0;	// Set pin 34 (RB1/INT1) for negative edge trigger
    INTCONbits.INT0IF = INTCON3bits.INT1IF = INTCON3bits.INT2IF = 0;// Reset interrupt flags
    INTCONbits.INT0IE = 1;		// Enable INT0 interrupt (mode down)
    INTCON3bits.INT1IE = 1;		// Enable INT1 interrupt (mode up)
    INTCON3bits.INT2IE = 1;		// Enable INT2 interrupt (mode)
    mode = 0;   motor_on = 0;   home_on = 0;    pan100 = 0;     tilt100 = 0;
    day_l = read_eeprom(0);   day_h = read_eeprom(1);   hr = read_eeprom(2);   min = read_eeprom(3);
    day = (unsigned int)day_h * 256 + day_l;
    pan_count = read_eeprom(4); tilt_count = read_eeprom(5);
    update1 = update_day = update_hr = update_min = update_sec = 1;	// update flags
    SetPosition(0);     PrintLine((const unsigned char*)"( )",3);
    while (1) {
        if (update1) {			// The update flag is set by INT0 or INT1
            update1 = 0;
            PrintNum1(mode, 1);
            switch (mode) {
                case 0: SetPosition(64);    // 0:auto
                PrintLine((const unsigned char*)"P     T         ",16);
                PrintInt(pan_count, 65);
                PrintInt(tilt_count, 71);
                break;
            case 1: SetPosition(64);        // manual - pan
                PrintLine((const unsigned char*)"P:              ",16);
                PrintInt(pan_count, 67);
                write_eeprom(4, pan_count);
                break;
            case 2: SetPosition(64);        // manual - tilt
                PrintLine((const unsigned char*)"T:              ",16);
                PrintInt(tilt_count, 67);
                write_eeprom(5, tilt_count);
                break;
            case 3: SetPosition(64);        // Learning mode
                PrintLine((const unsigned char*)"Learn this      ",16);
                break;
            case 4: SetPosition(64);        // Home/reset
                PrintLine((const unsigned char*)"Home/reset      ",16);
                break;                
            case 5: SetPosition(64);        // Set day of year
                PrintLine((const unsigned char*)"Change Day      ",16);
                break;
            case 6: SetPosition(64);        // Set hour
                PrintLine((const unsigned char*)"Change Hour     ",16);
                break;
            case 7: SetPosition(64);        // Set minute
                PrintLine((const unsigned char*)"Change Min      ",16);
                break;
            }
        }
        if (!debounce2) {
            stop_pan = PORTDbits.RD2;
            if (stop_pan) debounce2 = 250;
        }
        if (!debounce3) {
            stop_tilt = PORTDbits.RD3;
            if (stop_tilt) debounce3 = 250;
        }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               
        if (!debounce1 && (mode == 1)) {    // pan motor +
            motor_on = PORTDbits.RD1;
            if (motor_on) {
                motor_plus = 1;
                PORTD = 0b00010000;
                PORTBbits.RB3 = 1;
                debounce1 = 10;
            }
            else {
                PORTBbits.RB3 = 0;
                PORTD = 0b00000000;
            }
        }
        if (!debounce1 && (mode == 1) && !stop_pan) {    // pan motor -
            motor_on = PORTDbits.RD0;
            if (motor_on) {
                motor_plus = 0;
                PORTD = 0b00100000;
                PORTBbits.RB3 = 1;
                debounce1 = 10;
            }
            else {
                PORTBbits.RB3 = 0;
                PORTD = 0b00000000;
            }
        }
        if (!debounce1 && (mode == 2)) {    // tilt motor +
            motor_on = PORTDbits.RD1;
            if (motor_on) {
                motor_plus = 1;
                PORTD = 0b01000000;
                PORTBbits.RB3 = 1;
                debounce1 = 10;
            }
            else {
                PORTBbits.RB3 = 0;
                PORTD = 0b00000000;
            }
        }
        if (!debounce1 && (mode == 2) && !stop_tilt) {    // tilt motor -
            motor_on = PORTDbits.RD0;
            if (motor_on) {
                motor_plus = 0;
                PORTD = 0b10000000;
                PORTBbits.RB3 = 1;
                debounce1 = 10;
            }
            else {
                PORTBbits.RB3 = 0;
                PORTD = 0b00000000;
            }
        }
        if (mode == 4) {                            // home & reset
            if (home_on) {
                PORTBbits.RB3 = 1;
                PORTD = 0b00100000;
                while (!stop_pan) stop_pan = PORTDbits.RD2;
                PORTD = 0b10000000;
                pan_count = 0;
                write_eeprom(4, pan_count);
                PORTD = 0b10000000;
                while (!stop_tilt) stop_tilt = PORTDbits.RD3;
                PORTD = 0b00000000;
                tilt_count = 0;
                write_eeprom(5, tilt_count);
                PORTBbits.RB3 = 0;
                home_on = 0;
                SetPosition(75);
                PrintLine((const unsigned char*)"DONE",4);
                update1 = 0;
            }
            else {
                home_on = PORTDbits.RD1;
                if (home_on) {
                    SetPosition(75);
                    PrintLine((const unsigned char*)"WAIT",4);
                }
            }
        }
        if (!debounce2 && (mode == 5)) {            // day +
            up = PORTDbits.RD1;
            if (up) {
                day++;
                if (day > 366) day = 0;
                debounce2 = 20;
                update_day = 1;
            }
        }
        if (!debounce2 && (mode == 5)) {            // day -
            up = PORTDbits.RD0;
            if (up) {
                day--;
                if (day < 0) day = 366;
                debounce2 = 20;
                update_day = 1;
            }
        }
        if (!debounce2 && (mode == 6)) {            // hr +
            up = PORTDbits.RD1;
            if (up) {
                if (hr == 59) hr = 0;
                else hr++;
                debounce2 = 20;
                update_hr = 1;
            }
        }
        if (!debounce2 && (mode == 6)) {            // hr -
            up = PORTDbits.RD0;
            if (up) {
                if (hr == 0) hr = 59;
                else hr--;
                debounce2 = 20;
                update_hr = 1;
            }
        }
        if (!debounce2 && (mode == 7)) {            // min +
            up = PORTDbits.RD1;
            if (up) {
                if (min == 59) min = 0;
                else min++;
                debounce2 = 20;
                update_min = 1;
            }
        }
        if (!debounce2 && (mode == 7)) {            // min -
            up = PORTDbits.RD0;
            if (up) {
                if (min == 0) min = 59;
                else min--;
                debounce2 = 20;
                update_min = 1;
            }
        }
        if (update_day) {
            update_day = 0;
            PrintInt2(day, 4);
            day_h = day / 256;
            day_l = day - (unsigned int)day_h * 256;
            write_eeprom(0, day_l);
            write_eeprom(1, day_h);
        }
        if (update_hr) {
            update_hr = 0;
            PrintNum2(hr, 8);
            write_eeprom(2, hr);
        }        
        if (update_min) {
            update_min = 0;
            PrintNum2(min, 11);
            write_eeprom(3, min);
        }
        if (update_sec) {
            update_sec = 0;
            PrintNum2(sec, 14);
        }
    }
}
