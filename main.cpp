#define F_CPU 14745600
#define LINE_THRESHOLD 0x28
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "lcd.c"

int oW = 0; //Organic Waste count
int rW = 0; //Recyclable Waste count
int hW = 0; //Hazardous Waste count
int bC = 0; //sqaure block count
int nn=1;
unsigned char ADC_Conversion(unsigned char);
unsigned char ADC_Value;
unsigned char Left_lineSensor = 0;
unsigned char Center_lineSensor = 0;
unsigned char Right_lineSensor = 0;
unsigned char Left_IRSensor = 0;
unsigned char Right_IRSensor = 0;

void motion_pin_config (void)
{
	DDRA = DDRA | 0x0F; //set direction of the PORTA 3 to PORTA 0 pins as output
	PORTA = PORTA & 0xF0; // set initial value of the PORTA 3 to PORTA 0 pins to logic 0
	DDRL = DDRL | 0x18;   //Setting PL3 and PL4 pins as output for PWM generation
	PORTL = PORTL | 0x18; //PL3 and PL4 pins are for velocity control using PWM
}

void motion_set (unsigned char Direction)
{
	unsigned char PortARestore = 0;
	Direction &= 0x0F; 			// removing upper nibbel as it is not needed
	PortARestore = PORTA; 			// reading the PORTA's original status
	PortARestore &= 0xF0; 			// setting lower direction nibbel to 0
	PortARestore |= Direction; 	// adding lower nibbel for direction command and restoring the PORTA status
	PORTA = PortARestore; 			// setting the command to the port
}

void forward (void) //both wheels forward
{
	motion_set(0x06);
}

void left (void) //Left wheel backward, Right wheel forward
{
	motion_set(0x05);
}

void right (void) //Left wheel forward, Right wheel backward
{
	motion_set(0x0A);
}

void stop (void) //hard stop
{
	motion_set(0x00);
}

void buzzer_pin_config (void)
{
	DDRC = DDRC | 0x08;		//Setting PORTC 3 as output
	PORTC = PORTC & 0xF7;	//Setting PORTC 3 logic low to turnoff buzzer
}

//Function to configure LCD port
void lcd_port_config (void)
{
 DDRC = DDRC | 0xF7; //all the LCD pin's direction set as output
 PORTC = PORTC & 0x80; // all the LCD pins are set to logic 0 except PORTC 7
}

//ADC pin configuration
void adc_pin_config (void)
{
 DDRF = 0x00; //set PORTF direction as input
 PORTF = 0x00; //set PORTF pins floating
 DDRK = 0x00; //set PORTK direction as input
 PORTK = 0x00; //set PORTK pins floating
}

//Function to Initialize PORTS
void port_init()
{
	lcd_port_config();
	adc_pin_config();
	buzzer_pin_config();
	 motion_pin_config();	
}

//Function to Initialize ADC
void adc_init()
{
	ADCSRA = 0x00;
	ADCSRB = 0x00;		//MUX5 = 0
	ADMUX = 0x20;		//Vref=5V external --- ADLAR=1 --- MUX4:0 = 0000
	ACSR = 0x80;
	ADCSRA = 0x86;		//ADEN=1 --- ADIE=1 --- ADPS2:0 = 1 1 0
}

//This Function accepts the Channel Number and returns the corresponding Analog Value 
unsigned char ADC_Conversion(unsigned char Ch)
{
	unsigned char a;
	if(Ch>7)
	{
		ADCSRB = 0x08;
	}
	Ch = Ch & 0x07;  			
	ADMUX= 0x20| Ch;	   		
	ADCSRA = ADCSRA | 0x40;		//Set start conversion bit
	while((ADCSRA&0x10)==0);	//Wait for ADC conversion to complete
	a=ADCH;
	ADCSRA = ADCSRA|0x10; //clear ADIF (ADC Interrupt Flag) by writing 1 to it
	ADCSRB = 0x00;
	return a;
}

// This Function prints the Analog Value Of Corresponding Channel No. at required Row and Coloumn Location. 
void print_sensor(char row, char coloumn,unsigned char channel)
{
	ADC_Value = ADC_Conversion(channel);
	lcd_print(row, coloumn, ADC_Value, 3);
}

void timer5_init()
{
	TCCR5B = 0x00;	//Stop
	TCNT5H = 0xFF;	//Counter higher 8-bit value to which OCR5xH value is compared with
	TCNT5L = 0x01;	//Counter lower 8-bit value to which OCR5xH value is compared with
	OCR5AH = 0x00;	//Output compare register high value for Left Motor
	OCR5AL = 0xFF;	//Output compare register low value for Left Motor
	OCR5BH = 0x00;	//Output compare register high value for Right Motor
	OCR5BL = 0xFF;	//Output compare register low value for Right Motor
	OCR5CH = 0x00;	//Output compare register high value for Motor C1
	OCR5CL = 0xFF;	//Output compare register low value for Motor C1
	TCCR5A = 0xA9;	/*{COM5A1=1, COM5A0=0; COM5B1=1, COM5B0=0; COM5C1=1 COM5C0=0}
 					  For Overriding normal port functionality to OCRnA outputs.
				  	  {WGM51=0, WGM50=1} Along With WGM52 in TCCR5B for Selecting FAST PWM 8-bit Mode*/
	
	TCCR5B = 0x0B;	//WGM12=1; CS12=0, CS11=1, CS10=1 (Prescaler=64)
}

void velocity (unsigned char left_motor, unsigned char right_motor)
{
	OCR5AL = (unsigned char)left_motor;
	OCR5BL = (unsigned char)right_motor;
}

void init_devices (void)
{
 cli(); //Clears the global interrupts
 port_init();
 adc_init();
 timer5_init();
 sei(); //Enables the global interrupts
}

void buzzer_on (void)
{
	unsigned char port_restore = 0;
	port_restore = PINC;
	port_restore = port_restore | 0x08;
	PORTC = port_restore;
}

void buzzer_off (void)
{
	unsigned char port_restore = 0;
	port_restore = PINC;
	port_restore = port_restore & 0xF7;
	PORTC = port_restore;
}
 //function for buzzer beep
void beep(void)
{
		buzzer_on();
		_delay_ms(1000); //delay
		buzzer_off();
		_delay_ms(200);	 //delay
}
// Function for aligning IR Sensors with Waste block at node
void nodeJump(void)
{
	forward();
	velocity(130,130);
	_delay_ms(200);
	Left_IRSensor = ADC_Conversion(4);	//Getting data of Left IR Sensor
	Right_IRSensor = ADC_Conversion(8);	//Getting data of Right IR Sensor
	_delay_ms(200);
}
// Function for advancing firebird v at sharp 90 deg turn 
void nodeJump2(void)
{
	forward();
	velocity(110,110);
	_delay_ms(880);
	}

void sensorValues(void){
	
	Left_lineSensor = ADC_Conversion(3);	//Getting data of Left WL Sensor
	Center_lineSensor = ADC_Conversion(2);	//Getting data of Center WL Sensor
	Right_lineSensor = ADC_Conversion(1);	//Getting data of Right WL Sensor
	Left_IRSensor = ADC_Conversion(4);	//Getting data of Left IR Sensor
	Right_IRSensor = ADC_Conversion(8);	//Getting data of Right IR Sensor

	print_sensor(1,1,3);	//Prints value of White Line Sensor1
	print_sensor(1,5,2);	//Prints Value of White Line Sensor2
	print_sensor(1,9,1);	//Prints Value of White Line Sensor3
	print_sensor(2,1,4);	//Prints value of Left IR Sensor
	print_sensor(2,5,8);	//Prints Value of Right IR Sensor
	
}


//Main Function
int main(void)
{
	init_devices();
	lcd_set_4bit();
	lcd_init();
	
	while(nn)
	{
		sensorValues(); 
		// Case 1: Only the center sensor detects the black line (Move Forward)
		if (Center_lineSensor > LINE_THRESHOLD && Left_lineSensor < LINE_THRESHOLD && Right_lineSensor < LINE_THRESHOLD) 
		{
			forward();               
			velocity(150, 150);      
		}
		// Case 2: Black line is detected under left sensor (Turn left)
		else if (Left_lineSensor > LINE_THRESHOLD && Center_lineSensor < LINE_THRESHOLD && Right_lineSensor < LINE_THRESHOLD) 
		{
			forward();                  
			velocity(100, 130);       
		}
		// Case 3: Black line is detected under right sensor (Turn right)
		else if (Right_lineSensor > LINE_THRESHOLD && Center_lineSensor < LINE_THRESHOLD && Left_lineSensor < LINE_THRESHOLD) 
		{
			forward();                 
			velocity(130, 100);       
		}
		else if(Right_lineSensor < LINE_THRESHOLD && Center_lineSensor < LINE_THRESHOLD && Left_lineSensor < LINE_THRESHOLD)
		{
			forward();
			velocity(130,130);
		}
		// // Case 4: Black line detected by both left and center sensors (Sharp left turn)
		// else if (Left_lineSensor > LINE_THRESHOLD && Center_lineSensor > LINE_THRESHOLD && Right_lineSensor < LINE_THRESHOLD) 
		// {
		// 	left();                  
		// 	velocity(130, 100);       
		// }
		// // Case 5: Black line detected by both right and center sensors (Sharp right turn)
		// else if (Right_lineSensor > LINE_THRESHOLD && Center_lineSensor > LINE_THRESHOLD && Left_lineSensor < LINE_THRESHOLD) 
		// {
		// 	right();                 
		// 	velocity(100, 130);       
		// }
    	else if ((Center_lineSensor > LINE_THRESHOLD && Left_lineSensor > LINE_THRESHOLD) || 
                (Right_lineSensor > LINE_THRESHOLD && Center_lineSensor > LINE_THRESHOLD))
        {
            bC++; 
			 lcd_print(2,9,bC,2);   
			 
			//  forward();
			//  velocity(100,100);
            // _delay_ms(50);  
			//if(bC==5)
			//{
			//	left();
			//	velocity(0, 150); 
			//	_delay_ms(300);
			//}
			//if(bC==10)
			//{
			//	right();
			//	velocity(150, 0);
			//	_delay_ms(300);
			//}
            if(bC == 4 || bC == 5 || bC == 13 || bC == 14) 
            {
					left(); 
					velocity(0, 150);
					_delay_ms(2410);
					nodeJump2();
					// int flag=0;
					// while(flag!=1)
                	// {
					// 	sensorValues();
					// 	left();                  
					// 	velocity(0, 150); 
					// 	if((Center_lineSensor > LINE_THRESHOLD && Left_lineSensor < LINE_THRESHOLD && Right_lineSensor < LINE_THRESHOLD))
					// 	{
					// 		flag=1;
					// 		stop();
					// 		_delay_ms(300);
					// 	}
					// }   
            }  
            else if(bC == 9 || bC == 10) 
            {
					right();
					velocity(150,0);
					_delay_ms(2300);
					nodeJump2();
					// int flag=0;
					// while(flag!=1)
					// {
						
					// 	sensorValues();
					// 	right();
					// 	velocity(150,0);
					// 	if((Center_lineSensor > LINE_THRESHOLD && Left_lineSensor < LINE_THRESHOLD && Right_lineSensor < LINE_THRESHOLD))
					// 	{
					// 		flag=1;
					// 		stop();
					// 		_delay_ms(300);
					// 	}
					// }
                
            }
            else if(bC == 18) 
            {
					nn=0;
                	stop();
					velocity(0,0);
            }
			
			if(bC!=5 || bC!=13 || bC!=4 || bC!=14 || bC!=9 || bC!=10)
			{
				forward();
				velocity(100,100);
				_delay_ms(300);
				if(bC==2 || bC==3 || bC==7)
				{
					// 					sensorValues();
					// 					if(Left_IRSensor<100)
					// 					{
					// 						beep();
					// 					}
					if(bC==7)
					{
						rW+=2;
					}
					else
					{
						rW++;
					}
					lcd_print(2,10,rW,2);
					beep();
				}
				else if(bC==6 || bC==11)
				{
					if(bC==6)
					{
						hW+=2;
					}
					else
					{
						hW++;
					}
					lcd_print(2,12,rW,2);
					beep();
				}
				else if(bC==8||bC==11)
				{
					if(bC==8)
					{
						oW+=2;
					}
					else
					{
						oW++;
					}
					lcd_print(2,15,rW,2);
					beep();
				}
				stop();
				_delay_ms(300);
				nodeJump2();
			}
        }
		// lcd_print(2,9,oW,2);  
		// lcd_print(2,12,rW,2); 

		// lcd_print(2,15,hW,2); 
       
	} 				
}

// if ((Center_lineSensor > LINE_THRESHOLD && Left_lineSensor > LINE_THRESHOLD) || 
//             (Right_lineSensor > LINE_THRESHOLD && Center_lineSensor > LINE_THRESHOLD) || 
//             (Left_lineSensor > LINE_THRESHOLD && Center_lineSensor > LINE_THRESHOLD && Right_lineSensor > LINE_THRESHOLD)) {
            
//             bC++;  // Increment node count
//             _delay_ms(500);  // Delay to avoid multiple counts for the same node
//         }

//         // Turn left at the fourth node
//         if (bC == 4) {
//             stop();  // Stop before turning
//             _delay_ms(200);  // Small delay before turning
//             left();  // Execute the left turn
//             velocity(100, 100);  // Adjust speed for smooth turning
//             _delay_ms(800);  // Adjust delay for the 90-degree turn
//             stop();  // Stop after turning
//             _delay_ms(500);  // Small delay after turning
//         }

//         // Stop at 18th node
//         if (bC == 18) {
//             forward();
//             velocity(100, 100);
//             _delay_ms(1000);  // Move forward a bit before stopping
//             stop();  // Stop after reaching the final node
//             break;
//         }
//     }
