/*
 * tinykey.c
 *
 * Created: 4/13/2015 7:33:09 PM
 *  Author: mark
 */ 

#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <avr/interrupt.h>


#define LED 0b00100

#define COMP_CLK 0b


/*

  2 - pb3  : key clock in   (from key)
  3 - pb4  : key data in    (from key)


  7 - pb2 LED 
  6 - pb1 key clock out  (to computer)
  5 - pb0 key data out  (to computer)

*/

//			PORTB   543210
#define KEY_CLK    0b01000
#define KEY_DATA   0b10000
#define PCCLK      0b00010
#define PCDATA     0b00001
#define LED        0b00100


//keyboard input
#define KEYB_SIZE 10

volatile char keyb_readpos=0;  //written by reader
volatile char keyb_writepos=0; //written by driver
volatile char keyb[KEYB_SIZE];
volatile char keybits=0;
volatile char keybitcnt=0;

#define KEY_PIN PINB

volatile char hit=0;

ISR (PCINT0_vect)
{
	//hit=1;
	
	if (!(KEY_PIN & KEY_CLK))
	{
		
		char inbit = (KEY_PIN&KEY_DATA)<<3; //put this bit in pos for 128
		hit=2;
		//this was a falling edge
		
		if (keybitcnt==0)
		{
			if (inbit==0)
			keybitcnt++;
		}
		else if (keybitcnt<9)
		{
			keybits = keybits>>1;
			keybits |= inbit;
			keybitcnt++;
		}
		else if (keybitcnt==9)
		keybitcnt++;
		else { //cnt is 10 (11th bit of frame)
			if (inbit==128)
			{
				keyb[keyb_writepos] = keybits;  //write in buffer
				if (keyb_writepos==(KEYB_SIZE-1))
				keyb_writepos=0;
				else
				keyb_writepos++;
				hit=3;
			}
			
			keybitcnt=0;
		}
		
	}
	
}

unsigned char nextkey(){
	unsigned char x;
	
	//sprintf(str, "[%d:%d]", keyb_readpos, keyb_writepos);
	//debug(str);
	
	if (keyb_writepos == keyb_readpos)
	return 0;
	
	x = keyb[keyb_readpos++];
	if (keyb_readpos >= KEYB_SIZE)
	keyb_readpos = 0;
	
	return x;
}




//keyboard output (pretend to be a keyboard to the computer)

#define PS2LOW(V)    PORTB &= ~(V);  DDRB |= (V);          /* drop pullup, make output low (output is now low) */

#define PS2HIGH(V)   DDRB &= (~V);  PORTB |= (V);          /* make pins input, and pull up */


#define PS2DELAY  _delay_us(50);

void ps2sendbit(unsigned char b)
{
	if(b) {
		PS2HIGH(PCDATA);
	}
	else{
		PS2LOW(PCDATA);
	}
	
	PS2DELAY;
	
	PS2LOW (PCCLK);     //clock low
	
	PS2DELAY;
	PS2DELAY;
	
	PS2HIGH(PCCLK);
	
	PS2DELAY;
}

//send a char code
void ps2send(unsigned char c) {
	char i;
	char parity=1;
	
	PORTB |= LED;
	
	ps2sendbit(0);  //start bit;
	
	for (i=0;i<8;i++) {
		
		
		parity ^= ( c&1);  //flip parity for every '1'
		
		ps2sendbit(c&1);
		c= c >>1;	
	}
	ps2sendbit (parity);
	
	ps2sendbit(1); //stop bit
	//no need to pull data, clk high; the clock is left high, and the last data bit was high
	PORTB &= ~LED;
	
	//lets pause a bit
	//PS2DELAY;
	//PS2DELAY;
	//PS2DELAY;
	_delay_ms(10);
}

void ps2unsend(unsigned char key)
{
	ps2send(0xf0);
	ps2send(key);
}



unsigned char ktab[]=
{
	'a',0x1C,
	'b',0x32,
	'c',0x21,
	'd',0x23,
	'e',0x24,
	'f',0x2B,
	'g',0x34,
	'h',0x33,
	'i',0x43,
	'j',0x3B,
	'k',0x42,
	'l',0x4B,
	'm',0x3A,
	'n',0x31,
	'o',0x44,
	'p',0x4D,
	'q',0x15,
	'r',0x2D,
	's',0x1B,
	't',0x2C,
	'u',0x3C,
	'v',0x2A,
	'w',0x1D,
	'x',0x22,
	'y',0x35,
	'z',0x1A,
	'0',0x45,
	'1',0x16,
	'2',0x1E,
	'3',0x26,
	'4',0x25,
	'5',0x2E,
	'6',0x36,
	'7',0x3D,
	'8',0x3E,
	'9',0x46,
	'=',0x55,
	' ',0x29,
	'\t',0x0D,
	'\n',0x5A,
	'[',0x54,
	']',0x5B,
	';',0x4C,
	0,0
};

unsigned char rxlat(char c)
{
	unsigned char i;
	if ( (c >='A') && (c <='Z')) {
		//convert to lowercase	
		c +=  ('a' - 'A');
		
	}
	
	for (i=0;ktab[i];i+=2)
	{
		if (c == ktab[i])
			return ktab[i+1];
	}
	return 0x4C;  //return a semicolon
}



void keyprint(char* s)
{
	while(*s){
		
		ps2send(rxlat(*s));
		_delay_ms(30);	
		ps2unsend(rxlat(*s));
		_delay_ms(30);		
		s++;
	} 
}

void delay1s()
{
	char i;
	for (i=0;i<100;i++){
		_delay_ms(10);
	}
	
}


int main(void)
{
	char c;
	char str[20];
	DDRB = LED;
	PORTB = 0xff; //all pullup or on
	int i;
	
	GIMSK = (1 << PCIE);  //enable pin change interrupt
	PCMSK = (1<<PCINT3);	 //enable PCINT3
	sei(); //interrupts on!
	
	while (1){
		
		#ifdef TESTING
			ps2send(0x1c);
		//	ps2send(0x32);		
				
			_delay_ms(20);
		//
			ps2unsend(0x1c);
		//	ps2unsend(0x32);
			
			delay1s();
			
			/*=	
			ps2send(0x21);
			ps2send(0x23);
			_delay_ms(20);
		
			ps2unsend(0x21);
			ps2unsend(0x23);
		*/
			delay1s();
			
			keyprint("hello 123 \n");
			
			delay1s();
			//_delay_ms(5000);
			if (hit){
				sprintf(str, "hit %d\n", hit);
				keyprint(str);
				keyprint("z\n");
				hit=0;
			}
			c = nextkey();
			if(c){
				sprintf(str, "got %x\n", c);
				keyprint(str);
			}
		
		#else
			c = nextkey();
			if(c){
				ps2send(c);
			}
		#endif
	}
	
	
	/*
    while(1)
    {
	
		PORTB = 1;
		_delay_ms(1000);
		
		PORTB = 2;
		_delay_ms(1000);
		
		PORTB = 4;
		_delay_ms(1000);
		
		PORTB = 8;
		_delay_ms(1000);
		
		PORTB = 16;
		_delay_ms(1000);
		
    }
	*/
	
}