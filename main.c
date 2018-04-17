/*
 *
 * Created: 3/5/2018 8:38:31 AM
 * Author : Chester Lau
 */ 

#define F_CPU 8000000UL  
#include <math.h>
#include "io.c"
#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include "wifi.h"
volatile unsigned char blink = 0;
volatile unsigned char servo = 0;
volatile unsigned char TimerFlag = 0; 
volatile unsigned long count = 0;
volatile unsigned char breach = 0;
volatile unsigned char flag = 0;
volatile unsigned char flag2 = 0;
volatile unsigned char flag3 = 0;
#define DOUBLE_SPEED_MODE

#ifdef DOUBLE_SPEED_MODE
#define BAUD_PRESCALE (int)round(((((double)F_CPU / ((double)BAUDRATE * 8.0))) - 1.0))
#else
#define BAUD_PRESCALE (int)round(((((double)F_CPU / ((double)BAUDRATE * 16.0))) - 1.0))
#endif

void USART_Init(unsigned long);
char USART_RxChar();
void USART_TxChar(char);
void USART_SendString(char*);

typedef struct task{
	int current;
	unsigned long period;
	unsigned long elapsedTime;
	int (*TickFct)(int);
}task;
void USART_Init(unsigned long BAUDRATE)
{
	#ifdef DOUBLE_SPEED_MODE
	UCSR1A |=(1 << U2X0);
	#endif
	UCSR1B |= (1 << RXEN1) | (1 << TXEN1) | (1 << RXCIE1);
	
	UCSR1C |= (1 << UCSZ01) | (1 << UCSZ00);
	UBRR1L = BAUD_PRESCALE;
	UBRR1H = (BAUD_PRESCALE >> 8);
}

char USART_RxChar()
{
	while (!(UCSR1A & (1 << RXC1)));
	return(UDR1);
}

void USART_TxChar(char data)
{
	UDR1 = data;
	while (!(UCSR1A & (1<<UDRE1)));
}

void USART_SendString(char *str)
{
	int i=0;
	while (str[i]!=0)
	{
		USART_TxChar(str[i]);
		i++;
	}
}

char _buffer2[150];
char _buffer[150];
unsigned long _avr_timer_M = 1; 
unsigned long _avr_timer_cntcurr = 0; 
#define SREG    _SFR_IO8(0x3F)

#define DEFAULT_BUFFER_SIZE		160
#define DEFAULT_TIMEOUT			10000

#define DOMAIN				"maker.ifttt.com"
#define PORT				"80"

#define SSID				"chester's iphone"
#define PASSWORD			"chesteristhebest"

enum ESP8266_RESPONSE_STATUS{
	ESP8266_RESPONSE_WAITING,
	ESP8266_RESPONSE_FINISHED,
	ESP8266_RESPONSE_TIMEOUT,
	ESP8266_RESPONSE_BUFFER_FULL,
	ESP8266_RESPONSE_STARTING,
	ESP8266_RESPONSE_ERROR
};

enum ESP8266_CONNECT_STATUS {
	ESP8266_CONNECTED_TO_AP,
	ESP8266_CREATED_TRANSMISSION,
	ESP8266_TRANSMISSION_DISCONNECTED,
	ESP8266_NOT_CONNECTED_TO_AP,
	ESP8266_CONNECT_UNKNOWN_ERROR
};

enum ESP8266_JOINAP_STATUS {
	ESP8266_WIFI_CONNECTED,
	ESP8266_CONNECTION_TIMEOUT,
	ESP8266_WRONG_PASSWORD,
	ESP8266_NOT_FOUND_TARGET_AP,
	ESP8266_CONNECTION_FAILED,
	ESP8266_JOIN_UNKNOWN_ERROR
};

int8_t Response_Status;
volatile int16_t Counter = 0, pointer = 0;
uint32_t TimeOut = 0;
char RESPONSE_BUFFER[DEFAULT_BUFFER_SIZE];



void ESP8266_Clear()
{
	memset(RESPONSE_BUFFER,0,DEFAULT_BUFFER_SIZE);
	Counter = 0;	pointer = 0;
}

bool SendATandExpectResponse(char* ATCommand, char* ExpectedResponse)  //bool
{
	ESP8266_Clear();
	USART_SendString(ATCommand);
	USART_SendString("\r\n");
	return 1;
}

uint8_t ESP8266_Start(uint8_t _ConnectionNumber, char* Domain, char* Port)
{
	char _atCommand[60];
	memset(_atCommand, 0, 60);
	_atCommand[59] = 0;

	
	sprintf(_atCommand, "AT+CIPSTART=\"TCP\",\"%s\",%s", Domain, Port);
	
	SendATandExpectResponse(_atCommand, "CONNECT\r\n");
	
	return ESP8266_RESPONSE_FINISHED;
}

uint8_t ESP8266_Send(char* Data)
{
	char _atCommand[20];
	memset(_atCommand, 0, 20);
	sprintf(_atCommand, "AT+CIPSEND=%d", (strlen(Data))); //+2
	_atCommand[19] = 0;
	SendATandExpectResponse(_atCommand, "\r\nOK\r\n>");
	_delay_ms(500);
	SendATandExpectResponse(Data, "\r\nSEND OK\r\n");
	return ESP8266_RESPONSE_FINISHED;
}

ISR (USART1_RX_vect)
{
	uint8_t oldsrg = SREG;
	cli();
	RESPONSE_BUFFER[Counter] = UDR1;
	Counter++;
	if(Counter == DEFAULT_BUFFER_SIZE){
		Counter = 0; pointer = 0;
	}
	SREG = oldsrg;
}

void TimerOn() {
	
	TCCR1B = 0x0B;
	
	OCR1A = 125;	
	
	TIMSK1 = 0x02; 

	
	TCNT1=0;

	_avr_timer_cntcurr = _avr_timer_M;
	
	SREG |= 0x80; 
}

void TimerOff() {
	TCCR1B = 0x00; 
}

void TimerISR() {
	TimerFlag = 1;
}


ISR(TIMER1_COMPA_vect) {
	
	_avr_timer_cntcurr--; 
	if (_avr_timer_cntcurr == 0) { 
		TimerISR(); 
		_avr_timer_cntcurr = _avr_timer_M;
	}
}


void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

void set_PWM(double frequency) {
	static double current_frequency; // Keeps track of the currently set frequency
	// Will only update the registers when the frequency changes, otherwise allows
	// music to play uninterrupted.
	if (frequency != current_frequency) {
		if (!frequency) { TCCR3B &= 0x08; } //stops timer/counter
		else { TCCR3B |= 0x03; } // resumes/continues timer/counter
		
		// prevents OCR3A from overflowing, using prescaler 64
		// 0.954 is smallest frequency that will not result in overflow
		if (frequency < 0.954) { OCR3A = 0xFFFF; }
		
		// prevents OCR0A from underflowing, using prescaler 64					// 31250 is largest frequency that will not result in underflow
		else if (frequency > 31250) { OCR3A = 0x0000; }
		
		// set OCR3A based on desired frequency
		else { OCR3A = (short)(8000000 / (128 * frequency)) - 1; }

		TCNT3 = 0; // resets counter
		current_frequency = frequency; // Updates the current frequency
	}
}

void PWM_on() {
	TCCR3A = (1 << COM3A0);
	// COM3A0: Toggle PB3 on compare match between counter and OCR0A
	TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30);
	// WGM02: When counter (TCNT0) matches OCR0A, reset counter
	// CS01 & CS30: Set a prescaler of 64
	set_PWM(0);
}

void PWM_off() {
	TCCR3A = 0x00;
	TCCR3B = 0x00;
}

#define USART_BAUDRATE 9600   
#define BAUD_PRESCALEB (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

volatile unsigned char value;  

ISR(USART0_RX_vect){
 
   value = UDR0;            
    PORTA = ~value;     
}
void USART_Initialize(void){
  
   UBRR0L = BAUD_PRESCALEB;
  UCSR0B = ((1<<TXEN0)|(1<<RXEN0) | (1<<RXCIE0));
}


void USART_SendByte(uint8_t u8Data){
  while((UCSR0A &(1<<UDRE0)) == 0);

  UDR0 = u8Data;
}

uint8_t USART_ReceiveByte(){
  while((UCSR0A &(1<<RXC0)) == 0);
  return UDR0;
}
unsigned char tmpC = 0x00;
enum speaker{son, soff} spk;

void buzzer(){
	
	switch(spk){
		
		case son:
		
			spk = soff;
		break;
		
		case soff:
		
			spk = son;
		
	
		break;
		
	}
	switch(spk){
		
		case son:	
		tmpC = 0x01;
		break;
		
		case soff:
		tmpC = 0x00;
		break;
	}
}

enum state{init, code1, code2, code3,alarm} current;

void cases(){
	switch(current){
		case init: //alarm is unarmed
		if(value == 0x12 && (PINB & 0x01)== 0){
			current = code1;
		}
		else if((PINB & 0x01) == 1){
			current = alarm;
		}
		else{
			current = init;
		}
		break;
		case code1:
		if(value == 0x34 && (PINB & 0x01)== 0){
			count = 0;
			current = code2;
		}
		else if((PINB & 0x01) == 1){
			current = alarm;
		}
		else if(count > 3000){
			current = init;
		}
		else{
			current = code1;
		}
		break;
		case code2:
		if(value == 0x56 && (PINB & 0x01)== 0){
			count = 0;
			current = code3;
		}
		else if((PINB & 0x01) == 1){
			current = alarm;
		}	
		else if(count > 3000){
			current = init;
		}
		else{
			current = code2;
		}
		break;
		case code3:
		if(value == 0x00 && (PINB & 0x01)== 0){
			current = init;
		}
		else{
			current = code3;
		}
		break;
		case alarm:
		if(value == 0x88 && (PINB & 0x01)== 0){
			current = init;
		}
		else{
			current = alarm;
		}
	}
	switch(current){
		case init:
		breach = 0;
		count = 0;
		if (blink == 0){
		  LCD_ClearScreen();
		  LCD_DisplayString(1,"Alarm is Active");
		  blink = 1;
		  flag = 1;
		}
		 
		  if(servo == 0){
		  PWM_on();
		  set_PWM(1100);
		  PORTB = PORTB;
		  servo = 1;
		  }
		  else{
			  PWM_off();
			  PORTB = 0x0F;
		  }
		break;
		
		case code1:
		count++;
		break;
		
		case code2:
		count++;
		break;
		
		case code3:
		count = 0;
		if(blink ==1){
		LCD_ClearScreen();
		LCD_DisplayString(1,"Alarm is        Disabled");
		blink = 0;
		}
		
		if(servo == 1){
		 PWM_on();
		 set_PWM(300);
		 PORTB = PORTB;
		 servo = 0;
		}
		else{
			PWM_off();
			PORTB = 0x0F;
		}
		break;
		
		case alarm:
		value = 0;
		breach = 1;
		if(blink == 1){
		LCD_ClearScreen();
		LCD_DisplayString(1,"Alarm has been  breached");
		flag2 = 1;
		blink = 0;
		}
		break;
	}
}	
int main(void){
	DDRC = 0xFF; PORTC = 0x00; // LCD data lines
	DDRD = 0xFF; PORTD = 0x00; // LCD control lines
	DDRB = 0xF0; PORTB = 0x0F;
	DDRA = 0xFF; PORTA = 0x00;
	
	memset(_buffer, 0, 150);
	sprintf(_buffer, "POST /trigger/ESP/with/key/d17wk7MXszQitphwh-6Voj HTTP/1.1\r\nHost: maker.ifttt.com\r\nContent-Length: 0\r\nContent-Type: application/json\r\n\r\n");
	memset(_buffer2, 0, 150);
	sprintf(_buffer2, "POST /trigger/ARMED/with/key/d17wk7MXszQitphwh-6Voj HTTP/1.1\r\nHost: maker.ifttt.com\r\nContent-Length: 0\r\nContent-Type: application/json\r\n\r\n");
	USART_Init(115200);
	sei();
	_delay_ms(10000);
	static task task1,task2;
	task *tasks[] = {&task1};
	unsigned long buzz = 4;
	unsigned short timer = 100;
	unsigned short timer2 = 250;
	unsigned short i;
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);
	task1.current = init;
	task1.period = timer;
	task1.elapsedTime = task1.period;
	task1.TickFct = &cases; 
	
	task2.current = son;
	task2.period = buzz;
	task2.elapsedTime = task2.period;
	task2.TickFct = &buzzer;
   LCD_init(); 
   USART_Initialize(); 
         
   value = 'A';
   PORTA = ~value;
   TimerSet(1);
   TimerOn();
  
   while(1){   
	   PORTA = tmpC; 
      //value = 0;
	 /* for(i = 0; i < numTasks; i++){
		  tasks[i]->current = tasks[i]->TickFct(tasks[i]->current);
	  }*/
	 if(flag == 1){
		 flag = 0;
		 ESP8266_Start(0, DOMAIN, PORT);
		 _delay_ms(1000);
		 ESP8266_Send(_buffer);
	 }
	 if(flag2 == 1){
		 ESP8266_Start(0, DOMAIN, PORT);
		 _delay_ms(1000);
		 ESP8266_Send(_buffer2);
		 flag2 = 0;
	 }
	  if(buzz >= 4 && breach == 1){
		  buzzer();
		  buzz = 0;
	  } 
	 if(timer >= 100){  
	 cases();
	 timer = 0;
	  }
	 
	 if(timer2 >= 250){ 
     USART_SendByte(value);   
     //_delay_ms(250); 
	 }
   while(!TimerFlag);
	   TimerFlag = 0;
	   buzz++;
       timer++;
	   timer2++;
	}
}


