//need libm.a

#define F_CPU 2000000UL //CPU frequency
#define TIMER TCC0 //timer to use

#define DAC_CONINTVAL DAC_CONINTVAL_1CLK_gc //DAC settings
#define DAC_REFRESH DAC_REFRESH_16CLK_gc
#define ADC_PRESCALER ADC_PRESCALER_DIV8_gc //ADC sampling speed
#define ADC_OFFSET 0

#define EEPROM_SIZE 1024 //in bytes
#define PROFILES_LENGTH 11 //max number of profiles, limited by memory

#define SWV_BUFFER_SIZE 16 //must be <=16
#define SWV_MAX_DATAPOINTS 1000
#define CV_BUFFER_SIZE 16 //must be <=16
#define CV_MAX_DATAPOINTS 1500
#define LSV_BUFFER_SIZE 16 //must be <=16
#define LSV_MAX_DATAPOINTS 1500

#define CA_MAX_DATAPOINTS 1500


#define ACV_POINTSPERCYCLE 50

//#define DEBUG
//#define DUBUGSIZE
//#define DEBUG_INDEX_STOP 1000

//user interface
#define SWV 0
#define CV 1
#define ACV 2
#define LSV 3
#define CONSTVOLT 4
#define CA 5

#define PROFILE_SEL 0
#define PROFILE_OPT 1
#define PROFILE_TEST 2
#define PROFILE_EDIT 3
#define PROFILE_RESULTS 4
#define EDIT_NOSEL 0
#define EDIT_SEL 1
#define OPT_START 0
#define OPT_EDIT 1

#define RANGE_10UA 1
#define RANGE_50UA 2

#define UP 0
#define LEFT 1
#define RIGHT 2
#define DOWN 3
#define INVALID 4

#define bit_get(p,m) ((p) & (m))

#include "dac_driver.h"
#include "adc_driver.h"
#include "lcd.h"
#include "usart_driver.h"
#include <util/delay.h>
#include <stdio.h>
#include <math.h>
//#include <avr/io.h>
//#include <avr/pgmspace.h>
#include <string.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include "spi_driver.h"

//structure to store the profile - 29 bytes
typedef struct {
	char name[15];
	uint8_t type;
	int16_t op1;
	int16_t op2;
	int16_t op3;
	int16_t op4;
	int16_t op5;
	int16_t op6;
	uint8_t curr_range;
} profile;

//profiles are located in both EEPROM and RAM
profile EEMEM profilesEE[PROFILES_LENGTH];
profile profiles[PROFILES_LENGTH];

int buttonHandler(profile profiles[PROFILES_LENGTH], uint8_t* status, uint8_t* profile_index, uint8_t* profile_opt_index, uint8_t* profile_edit_index, uint8_t* profile_edit_sel,int16_t* length);
int16_t SWV_test (char* name, int16_t freq, int16_t start, int16_t stop, int16_t height, int16_t increment, uint8_t curr_range);
int16_t CV_test (char* name, int16_t slope, int16_t start, int16_t stop, int16_t scans, int16_t sample_rate, uint8_t curr_range);
int16_t ACV_test (char* name, int16_t freq, int16_t height, int16_t cycles, int16_t start, int16_t stop, int16_t incr, uint8_t curr_range);
int16_t LSV_test (char* name, int16_t wait, int16_t start, int16_t stop, int16_t slope, int16_t sample_rate, uint8_t curr_range);
int16_t CONSTVOLT_test (char* name, int16_t voltage, int16_t time);
int16_t CA_test (char* name, int16_t wait_time, int16_t step_voltage, int16_t step_width, int16_t quiet_time, int16_t sample_rate, uint8_t steps, uint8_t curr_range);
USART_data_t USART_data;

void send_string(char* string);
int main()
{
	uint8_t status; 
	uint8_t profile_index;
	uint8_t profile_opt_index;
	uint8_t profile_edit_index;
	uint8_t profile_edit_sel;

	uint8_t index;
	char temp_string[16];
	uint8_t i;
	int16_t length;



	//INITIALIZATION

		//SPI

//	PORTD.DIRCLR = PIN5_bm;
//	PORTD.DIRCLR = PIN6_bm;
//	PORTD.DIRCLR = PIN7_bm;
	/*
	PORTD.DIRSET = PIN4_bm;
	PORTD.PIN4CTRL = PORT_OPC_WIREDANDPULL_gc;
	PORTD.OUTSET = PIN4_bm;
	SPI_MasterInit(&spiMasterD,
	               &SPIC,
	               &PORTD,
	               false,
	               SPI_MODE_0_gc,
	               SPI_INTLVL_OFF_gc,
	               false,
	               SPI_PRESCALER_DIV4_gc);*/

	//4 DIR SWITCH 
	/////////////////////////////////
	//setup pins as input
	PORTA.DIRCLR = PIN4_bm;
	PORTA.PIN4CTRL = PORT_OPC_PULLDOWN_gc;
	PORTA.DIRCLR = PIN5_bm;
	PORTA.PIN5CTRL = PORT_OPC_PULLDOWN_gc;
	PORTA.DIRCLR = PIN6_bm;
	PORTA.PIN6CTRL = PORT_OPC_PULLDOWN_gc;
	PORTA.DIRCLR = PIN7_bm;
	PORTA.PIN7CTRL = PORT_OPC_PULLDOWN_gc;
	//PORTE.INT0MASK = 0;
	/////////////////////////////////


	//SPST SWITCHES
	/////////////////////////////////
    //setup pins as output
	PORTE.DIRSET = PIN1_bm; //switch0
	PORTE.DIRSET = PIN0_bm; //switch1
	PORTE.DIRSET = PIN2_bm; //switch2
	PORTE.DIRSET = PIN3_bm; //switch3

	//set initital switch positions
	PORTE.OUTCLR = PIN1_bm; //switch0
	PORTE.OUTSET = PIN0_bm; //switch1
	PORTE.OUTCLR = PIN2_bm; //switch2
	PORTE.OUTCLR = PIN3_bm; //switch3
	/////////////////////////////////


	//DAC
	/////////////////////////////////
	DAC_DualChannel_Enable( &DACB,DAC_REFSEL_AVCC_gc,false,DAC_CONINTVAL,DAC_REFRESH);
	//1.65V reference
	while (DAC_Channel_DataEmpty(&DACB, CH1) == false) {}
		DAC_Channel_Write(&DACB,2048,CH1);
	//intitial
	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,2048,CH0);
	/////////////////////////////////

	//USART
	/////////////////////////////////
	//TX as output
	PORTC.DIRSET = PIN3_bm;
	//RX as input
	PORTC.DIRCLR = PIN2_bm;
	USART_InterruptDriver_Initialize(&USART_data, &USARTC0, USART_DREINTLVL_LO_gc);
	// USARTD0, 8 Data bits, No Parity, 1 Stop bit.
	USART_Format_Set(&USARTC0, USART_CHSIZE_8BIT_gc, USART_PMODE_DISABLED_gc, false);
		//enable interrupts
	USART_RxdInterruptLevel_Set(&USARTC0, USART_RXCINTLVL_LO_gc);
	//BUAD RATE to 9600
	USART_Baudrate_Set(&USARTC0, 12 , 0);
	USART_Rx_Enable(&USARTC0);
	USART_Tx_Enable(&USARTC0);
	/////////////////////////////////

	//ADC
	/////////////////////////////////
	// Move stored calibration values to ADC A. 
	ADC_CalibrationValues_Load(&ADCA);
	// Set up ADC A to have signed conversion mode and 12 bit resolution. 
  	ADC_ConvMode_and_Resolution_Config(&ADCA, ADC_ConvMode_Signed, ADC_RESOLUTION_12BIT_gc);
	// Set sample rate.
	ADC_Prescaler_Config(&ADCA, ADC_PRESCALER);
	// Set reference voltage on ADC A to be VCC internal
	ADC_Reference_Config(&ADCA, ADC_REFSEL_VCC_gc);
	//configure input mode to differential
	ADC_Ch_InputMode_and_Gain_Config(&ADCA.CH0,ADC_CH_INPUTMODE_DIFF_gc,ADC_DRIVER_CH_GAIN_NONE);
	ADC_Ch_InputMode_and_Gain_Config(&ADCA.CH1,ADC_CH_INPUTMODE_DIFF_gc,ADC_DRIVER_CH_GAIN_NONE);
	//configure pins
	ADC_Ch_InputMux_Config(&ADCA.CH0, ADC_CH_MUXPOS_PIN0_gc, ADC_CH_MUXNEG_PIN1_gc);
	ADC_Ch_InputMux_Config(&ADCA.CH1, ADC_CH_MUXPOS_PIN2_gc, ADC_CH_MUXNEG_PIN1_gc);
	//enable adc
	ADC_Enable(&ADCA);
	/////////////////////////////////

	//TODO
	//DAC -> ADC calibration
	/////////////////////////////////

	/////////////////////////////////

	//LCD
	/////////////////////////////////
	
	//settings in lcdconf.h
	lcdInit();
	lcdInit();
	lcdClear();
	lcdHome();
	lcdPrintData("CheapStat",9);
	_delay_ms(1000);
	/////////////////////////////////
	
	//USER INTERFACE
	/////////////////////////////////
	status = PROFILE_SEL; 
	profile_index = 0;
	profile_opt_index = OPT_START;
	profile_edit_index = 0;
	profile_edit_sel = EDIT_NOSEL;
	/////////////////////////////////

	//PROFILES (EEPROM)
	/////////////////////////////////
	//copy profiles from EEPROM to SRAM
	for(i = 0; i < PROFILES_LENGTH; i++)
		eeprom_read_block((void*)&(profiles[i]), (const void*)&(profilesEE[i]), sizeof(profile));
	/////////////////////////////////

	//INTERRUPTS
	/////////////////////////////////
	PMIC.CTRL |= PMIC_LOLVLEX_bm;
	sei();
	/////////////////////////////////

	//END INITIALIZATION




	//CREATING PROFILES ON EEPROM
	/////////////////////////////////
/*
	strcpy(profiles[0].name,"SWV #1         ");
	profiles[0].type = SWV;
	profiles[0].op1 = 60;
	profiles[0].op2 = 0;
	profiles[0].op3 = -500;
	profiles[0].op4 = 50;
	profiles[0].op5 = 3;
	profiles[0].op6 = 0;
	profiles[0].curr_range = RANGE_10UA;

	strcpy(profiles[1].name,"CV #1          ");
	profiles[1].type = CV;
	profiles[1].op1 = 5000;
	profiles[1].op2 = -100;
	profiles[1].op3 = 100;
	profiles[1].op4 = 1;
	profiles[1].op5 = 1;
	profiles[1].op6 = 0;
	profiles[1].curr_range = RANGE_10UA;

	strcpy(profiles[2].name,"LSV #1         ");
	profiles[2].type = LSV;
	profiles[2].op1 = 30;
	profiles[2].op2 = -800;
	profiles[2].op3 = 500;
	profiles[2].op4 = 10;
	profiles[2].op5 = 10;
	profiles[2].op6 = 0;
	profiles[2].curr_range = RANGE_10UA;

	strcpy(profiles[3].name,"ACV #1         ");
	profiles[3].type = ACV;
	profiles[3].op1 = 100;
	profiles[3].op2 = 50;
	profiles[3].op3 = 50;
	profiles[3].op4 = 0;
	profiles[3].op5 = -550;
	profiles[3].op6 = 4;
	profiles[3].curr_range = RANGE_10UA;

	strcpy(profiles[4].name,"CA #1         ");
	profiles[4].type = CA;
	profiles[4].op1 = 1000;
	profiles[4].op2 = 500;
	profiles[4].op3 = 250;
	profiles[4].op4 = 1000;
	profiles[4].op5 = 500;
	profiles[4].op6 = 1;
	profiles[4].curr_range = RANGE_10UA;

	strcpy(profiles[5].name,"ConstantVoltage");
	profiles[5].type = CONSTVOLT;
	profiles[5].op1 = -550;
	profiles[5].op2 = 60;
	profiles[5].op3 = 0;
	profiles[5].op4 = 0;
	profiles[5].op5 = 0;
	profiles[5].op6 = 0;
	profiles[5].curr_range = RANGE_10UA;

	strcpy(profiles[6].name,"SWV #2         ");
	profiles[6].type = SWV;
	profiles[6].op1 = 60;
	profiles[6].op2 = 0;
	profiles[6].op3 = -500;
	profiles[6].op4 = 50;
	profiles[6].op5 = 3;
	profiles[6].op6 = 0;
	profiles[6].curr_range = RANGE_10UA;

	strcpy(profiles[7].name,"CV #2          ");
	profiles[7].type = CV;
	profiles[7].op1 = 10;
	profiles[7].op2 = 100;
	profiles[7].op3 = -100;
	profiles[7].op4 = 1;
	profiles[7].op5 = 1;
	profiles[7].op6 = 0;
	profiles[7].curr_range = RANGE_10UA;

	strcpy(profiles[8].name,"LSV #2         ");
	profiles[8].type = LSV;
	profiles[8].op1 = 60;
	profiles[8].op2 = 0;
	profiles[8].op3 = -500;
	profiles[8].op4 = 50;
	profiles[8].op5 = 3;
	profiles[8].op6 = 0;
	profiles[8].curr_range = RANGE_10UA;

	strcpy(profiles[9].name,"ACV #2         ");
	profiles[9].type = ACV;
	profiles[9].op1 = 100;
	profiles[9].op2 = 50;
	profiles[9].op3 = 100;
	profiles[9].op4 = 0;
	profiles[9].op5 = -500;
	profiles[9].op6 = 100;
	profiles[9].curr_range = RANGE_10UA;

	strcpy(profiles[10].name,"CA #2         ");
	profiles[10].type = CA;
	profiles[10].op1 = 1000;
	profiles[10].op2 = -500;
	profiles[10].op3 = 250;
	profiles[10].op4 = 1000;
	profiles[10].op5 = 500;
	profiles[10].op6 = 1;
	profiles[10].curr_range = RANGE_10UA;

*/


	for(i = 0; i < PROFILES_LENGTH; i++)
		eeprom_write_block((const void*)&(profiles[i]), (void*)&(profilesEE[i]), sizeof(profile));
	/////////////////////////////////


	//MAIN LOOP
	while(1)
	{
		//for selecting profiles
		if(status == PROFILE_SEL)
		{
			//clear display
			lcdClear();
			lcdHome();

			//determine where to start list
			if(profile_index > 1)
				index = profile_index-2;
			else
				index = 0;

			//first line
			if(profile_index == 0)
				lcdPrintData("~",1);
			else
				lcdPrintData(" ",1);
			lcdPrintData(profiles[index].name, 15);

			//second line
			lcdGotoXY(0,1);
			if(profile_index == 1)
				lcdPrintData("~",1);
			else
				lcdPrintData(" ",1);
			lcdPrintData(profiles[index+1].name, 15);

			//third line
			lcdGotoXY(0,2);
			if(profile_index > 1)
				lcdPrintData("~",1);
			else
				lcdPrintData(" ",1);
			lcdPrintData(profiles[index+2].name, 15);
		
			while(buttonHandler(profiles,&status,&profile_index,&profile_opt_index,&profile_edit_index,&profile_edit_sel,&length)!=1) {}
		}
		//give options for selected profile
		else if(status == PROFILE_OPT)
		{
			//clear display
			lcdClear();
			lcdHome();

			//display name
			lcdPrintData(profiles[profile_index].name, 15);
		
			//start option
			lcdGotoXY(0,1);
			if(profile_opt_index == OPT_START)
				lcdPrintData("~",1);
			else
				lcdPrintData(" ",1);
			lcdPrintData("start",5);

			//edit option
			lcdGotoXY(0,2);
			if(profile_opt_index == OPT_EDIT)
				lcdPrintData("~",1);
			else
				lcdPrintData(" ",1);
			lcdPrintData("edit",4);

			while(buttonHandler(profiles,&status,&profile_index,&profile_opt_index,&profile_edit_index,&profile_edit_sel,&length)!=1) {}
		}
		//display during test
		else if(status == PROFILE_TEST)
		{
			//clear display
			lcdClear();
			lcdHome();

			//display name
			lcdPrintData(profiles[profile_index].name, 15);

			lcdGotoXY(0,1);
			lcdPrintData(" testing...",10);
			

			if(profiles[profile_index].type == SWV)
				length = SWV_test(profiles[profile_index].name, profiles[profile_index].op1, profiles[profile_index].op2, profiles[profile_index].op3, profiles[profile_index].op4, profiles[profile_index].op5, profiles[profile_index].curr_range);
			else if(profiles[profile_index].type == CV)
				length = CV_test(profiles[profile_index].name, profiles[profile_index].op1, profiles[profile_index].op2, profiles[profile_index].op3, profiles[profile_index].op4, profiles[profile_index].op5, profiles[profile_index].curr_range);
			else if(profiles[profile_index].type == ACV)
				length = ACV_test(profiles[profile_index].name, profiles[profile_index].op1, profiles[profile_index].op2, profiles[profile_index].op3, profiles[profile_index].op4, profiles[profile_index].op5, profiles[profile_index].op6, profiles[profile_index].curr_range);
			else if(profiles[profile_index].type == LSV)
				length = LSV_test(profiles[profile_index].name, profiles[profile_index].op1, profiles[profile_index].op2, profiles[profile_index].op3, profiles[profile_index].op4, profiles[profile_index].op5, profiles[profile_index].curr_range);
			else if(profiles[profile_index].type == CONSTVOLT)
				length = CONSTVOLT_test(profiles[profile_index].name, profiles[profile_index].op1, profiles[profile_index].op2);
			else if(profiles[profile_index].type == CA)
				length = CA_test(profiles[profile_index].name, profiles[profile_index].op1, profiles[profile_index].op2, profiles[profile_index].op3, profiles[profile_index].op4, profiles[profile_index].op5, profiles[profile_index].op6, profiles[profile_index].curr_range);
			
			if(length == -1)
			{
				_delay_ms(1000);
				status = PROFILE_EDIT;
			}
			else
				status = PROFILE_RESULTS;
		}
		else if(status == PROFILE_EDIT)
		{
			//clear display
			lcdClear();
			lcdHome();

			//display name
			lcdPrintData(profiles[profile_index].name, 15);
		
			//list options
			lcdGotoXY(0,1);
			if(profile_edit_index == 0)
			{
				if(profile_edit_sel == EDIT_NOSEL)
					lcdPrintData("~",1);
				else
					lcdPrintData(">",1);

				lcdPrintData("Voltammetry=",12);
				if(profiles[profile_index].type == SWV)
					lcdPrintData("SWV",3);	
				else if(profiles[profile_index].type == CV)
					lcdPrintData("CV",2);
				else if(profiles[profile_index].type == ACV)
					lcdPrintData("ACV",3);
				else if(profiles[profile_index].type == LSV)
					lcdPrintData("LSV",3);
				else if(profiles[profile_index].type == CONSTVOLT)
					lcdPrintData("CONSTVOLT",3);
				else if(profiles[profile_index].type == CA)
					lcdPrintData("CA",3);

				lcdGotoXY(0,2);
				lcdPrintData(" ",1);
				if(profiles[profile_index].type == SWV)
				{
					lcdPrintData("Freq=",5);
					sprintf(temp_string,"%3d",profiles[profile_index].op1);
					lcdPrintData(temp_string,3);
					lcdPrintData("Hz",2);
				}
				else if(profiles[profile_index].type == CV)
				{
					lcdPrintData("Slope=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op1);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV/s",4);
				}
				else if(profiles[profile_index].type == ACV)
				{
					lcdPrintData("Freq=",5);
					sprintf(temp_string,"%3d",profiles[profile_index].op1);
					lcdPrintData(temp_string,3);
					lcdPrintData("Hz",2);
				}
				else if(profiles[profile_index].type == LSV)
				{
					lcdPrintData("Settle=",7);
					sprintf(temp_string,"%3d",profiles[profile_index].op1);
					lcdPrintData(temp_string,3);
					lcdPrintData("s",1);
				}
				else if(profiles[profile_index].type == CONSTVOLT)
				{
					lcdPrintData("Voltage=",8);
					sprintf(temp_string,"%4d",profiles[profile_index].op1);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CA)
				{
					lcdPrintData("Wait=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op1);
					lcdPrintData(temp_string,4);
					lcdPrintData("ms",2);
				}
			}
			else if(profile_edit_index == 1)
			{
				lcdPrintData(" ",1);
				
				lcdPrintData("Voltammetry=",12);
				if(profiles[profile_index].type == SWV)
					lcdPrintData("SWV",3);	
				else if(profiles[profile_index].type == CV)
					lcdPrintData("CV",2);
				else if(profiles[profile_index].type == ACV)
					lcdPrintData("ACV",3);
				else if(profiles[profile_index].type == LSV)
					lcdPrintData("LSV",3);
				else if(profiles[profile_index].type == CONSTVOLT)
					lcdPrintData("CONSTVOLT",3);
				else if(profiles[profile_index].type == CA)
					lcdPrintData("CA",3);

				lcdGotoXY(0,2);
				if(profile_edit_sel == EDIT_NOSEL)
					lcdPrintData("~",1);
				else
					lcdPrintData(">",1);
				
				if(profiles[profile_index].type == SWV)
				{
					lcdPrintData("Freq=",5);
					sprintf(temp_string,"%3d",profiles[profile_index].op1);
					lcdPrintData(temp_string,3);
					lcdPrintData("Hz",2);
				}
				else if(profiles[profile_index].type == CV)
				{
					lcdPrintData("Slope=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op1);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV/s",4);
				}
				else if(profiles[profile_index].type == ACV)
				{
					lcdPrintData("Freq=",5);
					sprintf(temp_string,"%3d",profiles[profile_index].op1);
					lcdPrintData(temp_string,3);
					lcdPrintData("Hz",2);
				}
				else if(profiles[profile_index].type == LSV)
				{
					lcdPrintData("Settle=",7);
					sprintf(temp_string,"%3d",profiles[profile_index].op1);
					lcdPrintData(temp_string,3);
					lcdPrintData("s",1);
				}
				else if(profiles[profile_index].type == CONSTVOLT)
				{
					lcdPrintData("Voltage=",8);
					sprintf(temp_string,"%4d",profiles[profile_index].op1);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CA)
				{
					lcdPrintData("Wait=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op1);
					lcdPrintData(temp_string,4);
					lcdPrintData("ms",2);
				}
			}
			else if(profile_edit_index == 2)
			{
				lcdPrintData(" ",1);
				if(profiles[profile_index].type == SWV)
				{
					lcdPrintData("Freq=",5);
					sprintf(temp_string,"%3d",profiles[profile_index].op1);
					lcdPrintData(temp_string,3);
					lcdPrintData("Hz",2);
				}
				else if(profiles[profile_index].type == CV)
				{
					lcdPrintData("Slope=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op1);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV/s",4);
				}
				else if(profiles[profile_index].type == ACV)
				{
					lcdPrintData("Freq=",5);
					sprintf(temp_string,"%3d",profiles[profile_index].op1);
					lcdPrintData(temp_string,3);
					lcdPrintData("Hz",2);
				}
				else if(profiles[profile_index].type == LSV)
				{
					lcdPrintData("Settle=",7);
					sprintf(temp_string,"%4d",profiles[profile_index].op1);
					lcdPrintData(temp_string,4);
					lcdPrintData("s",1);
				}
				else if(profiles[profile_index].type == CONSTVOLT)
				{
					lcdPrintData("Voltage=",8);
					sprintf(temp_string,"%4d",profiles[profile_index].op1);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",1);
				}
				else if(profiles[profile_index].type == CA)
				{
					lcdPrintData("Wait=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op1);
					lcdPrintData(temp_string,4);
					lcdPrintData("ms",2);
				}

				lcdGotoXY(0,2);
				if(profile_edit_sel == EDIT_NOSEL)
					lcdPrintData("~",1);
				else
					lcdPrintData(">",1);
				
				if(profiles[profile_index].type == SWV)
				{
					lcdPrintData("Start=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op2);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CV)
				{
					lcdPrintData("Start=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op2);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == ACV)
				{
					lcdPrintData("Height=",7);
					sprintf(temp_string,"%4d",profiles[profile_index].op2);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == LSV)
				{
					lcdPrintData("Start=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op2);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CONSTVOLT)
				{
					lcdPrintData("Duration=",9);
					sprintf(temp_string,"%4d",profiles[profile_index].op2);
					lcdPrintData(temp_string,4);
					lcdPrintData("s",1);
				}
				else if(profiles[profile_index].type == CA)
				{
					lcdPrintData("Step V=",7);
					sprintf(temp_string,"%4d",profiles[profile_index].op2);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
			}
			else if(profile_edit_index == 3)
			{
				lcdPrintData(" ",1);
				if(profiles[profile_index].type == SWV)
				{
					lcdPrintData("Start=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op2);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CV)
				{
					lcdPrintData("Start=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op2);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == ACV)
				{
					lcdPrintData("Height=",7);
					sprintf(temp_string,"%4d",profiles[profile_index].op2);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == LSV)
				{
					lcdPrintData("Start=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op2);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CONSTVOLT)
				{
					lcdPrintData("Duration=",9);
					sprintf(temp_string,"%4d",profiles[profile_index].op2);
					lcdPrintData(temp_string,4);
					lcdPrintData("s",1);
				}
				else if(profiles[profile_index].type == CA)
				{
					lcdPrintData("Step V=",7);
					sprintf(temp_string,"%4d",profiles[profile_index].op2);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}

				lcdGotoXY(0,2);
				if(profile_edit_sel == EDIT_NOSEL)
					lcdPrintData("~",1);
				else
					lcdPrintData(">",1);

				if(profiles[profile_index].type == SWV)
				{
					lcdPrintData("Stop=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op3);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CV)
				{
					lcdPrintData("Stop=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op3);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == ACV)
				{
					lcdPrintData("Cycles=",7);
					sprintf(temp_string,"%4d",profiles[profile_index].op3);
					lcdPrintData(temp_string,4);
				}
				else if(profiles[profile_index].type == LSV)
				{
					lcdPrintData("Stop=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op3);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CA)
				{
					lcdPrintData("Step W=",7);
					sprintf(temp_string,"%4d",profiles[profile_index].op3);
					lcdPrintData(temp_string,4);
					lcdPrintData("ms",2);
				}
			}
			else if(profile_edit_index == 4)
			{
				lcdPrintData(" ",1);
				if(profiles[profile_index].type == SWV)
				{
					lcdPrintData("Stop=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op3);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CV)
				{
					lcdPrintData("Stop=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op3);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == ACV)
				{
					lcdPrintData("Cycles=",7);
					sprintf(temp_string,"%4d",profiles[profile_index].op3);
					lcdPrintData(temp_string,4);
				}
				else if(profiles[profile_index].type == LSV)
				{
					lcdPrintData("Stop=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op3);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CA)
				{
					lcdPrintData("Step W=",7);
					sprintf(temp_string,"%4d",profiles[profile_index].op3);
					lcdPrintData(temp_string,4);
					lcdPrintData("ms",2);
				}

				lcdGotoXY(0,2);
				if(profile_edit_sel == EDIT_NOSEL)
					lcdPrintData("~",1);
				else
					lcdPrintData(">",1);
				
				if(profiles[profile_index].type == SWV)
				{
					lcdPrintData("Height=",7);
					sprintf(temp_string,"%4d",profiles[profile_index].op4);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CV)
				{
					lcdPrintData("# of Scans=",11);
					sprintf(temp_string,"%2d",profiles[profile_index].op4);
					lcdPrintData(temp_string,2);
				}
				else if(profiles[profile_index].type == ACV)
				{
					lcdPrintData("Start=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op4);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == LSV)
				{
					lcdPrintData("Slope=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op4);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV/s",4);
				}
				else if(profiles[profile_index].type == CA)
				{
					lcdPrintData("Quiet=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op4);
					lcdPrintData(temp_string,4);
					lcdPrintData("ms",2);
				}
			}
			else if(profile_edit_index == 5)
			{
				lcdPrintData(" ",1);
				if(profiles[profile_index].type == SWV)
				{
					lcdPrintData("Height=",7);
					sprintf(temp_string,"%4d",profiles[profile_index].op4);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CV)
				{
					lcdPrintData("# of Scans=",11);
					sprintf(temp_string,"%2d",profiles[profile_index].op4);
					lcdPrintData(temp_string,2);
				}
				else if(profiles[profile_index].type == ACV)
				{
					lcdPrintData("Start=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op4);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == LSV)
				{
					lcdPrintData("Slope=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op4);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV/s",4);
				}
				else if(profiles[profile_index].type == CA)
				{
					lcdPrintData("Quiet=",6);
					sprintf(temp_string,"%4d",profiles[profile_index].op4);
					lcdPrintData(temp_string,4);
					lcdPrintData("ms",2);
				}

				lcdGotoXY(0,2);
				if(profile_edit_sel == EDIT_NOSEL)
					lcdPrintData("~",1);
				else
					lcdPrintData(">",1);

				if(profiles[profile_index].type == SWV)
				{
					lcdPrintData("Incr=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op5);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CV)
				{
					lcdPrintData("SR=",3);
					sprintf(temp_string,"%3d",profiles[profile_index].op5);
					lcdPrintData(temp_string,3);
					lcdPrintData("mV/sample",9);
				}
				else if(profiles[profile_index].type == ACV)
				{
					lcdPrintData("Stop=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op5);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == LSV)
				{
					lcdPrintData("SR=",3);
					sprintf(temp_string,"%3d",profiles[profile_index].op5);
					lcdPrintData(temp_string,3);
					lcdPrintData("mV/sample",9);
				}
				else if(profiles[profile_index].type == CA)
				{
					lcdPrintData("SR=",3);
					sprintf(temp_string,"%4d",profiles[profile_index].op5);
					lcdPrintData(temp_string,4);
					lcdPrintData("sample/s",8);
				}
				
			}
			else if(profile_edit_index == 6)
			{

				lcdPrintData(" ",1);
				if(profiles[profile_index].type == SWV)
				{
					lcdPrintData("Incr=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op5);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CV)
				{
					lcdPrintData("SR=",3);
					sprintf(temp_string,"%3d",profiles[profile_index].op5);
					lcdPrintData(temp_string,3);
					lcdPrintData("mV/sample",9);
				}
				else if(profiles[profile_index].type == ACV)
				{
					lcdPrintData("Stop=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op5);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == LSV)
				{
					lcdPrintData("SR=",3);
					sprintf(temp_string,"%3d",profiles[profile_index].op5);
					lcdPrintData(temp_string,3);
					lcdPrintData("mV/sample",9);
				}
				else if(profiles[profile_index].type == CA)
				{
					lcdPrintData("SR=",3);
					sprintf(temp_string,"%4d",profiles[profile_index].op5);
					lcdPrintData(temp_string,4);
					lcdPrintData("sample/s",8);
				}

				lcdGotoXY(0,2);
				if(profile_edit_sel == EDIT_NOSEL)
					lcdPrintData("~",1);
				else
					lcdPrintData(">",1);

				if(profiles[profile_index].type == SWV)
				{
					lcdPrintData("Range=",6);
					if(profiles[profile_index].curr_range == RANGE_10UA)
						lcdPrintData("0-10uA",5);
					else
						lcdPrintData("0-50uA",6);
					}
				else if(profiles[profile_index].type == CV)
				{
					lcdPrintData("Range=",6);
					if(profiles[profile_index].curr_range == RANGE_10UA)
						lcdPrintData("0-10uA",5);
					else
						lcdPrintData("0-50uA",6);
				}
				else if(profiles[profile_index].type == ACV)
				{
					lcdPrintData("Incr=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op6);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == LSV)
				{
					lcdPrintData("Range=",6);
					if(profiles[profile_index].curr_range == RANGE_10UA)
						lcdPrintData("0-10uA",6);
					else
						lcdPrintData("0-50uA",6);
				}
				else if(profiles[profile_index].type == CA)
				{
					lcdPrintData("Steps=",6);
					sprintf(temp_string,"%2d",profiles[profile_index].op6);
					lcdPrintData(temp_string,2);
				}
				

			}
			else if(profile_edit_index == 7)
			{
				
				if(profiles[profile_index].type == ACV)
				{
					lcdPrintData(" ",1);
					lcdPrintData("Incr=",5);
					sprintf(temp_string,"%4d",profiles[profile_index].op6);
					lcdPrintData(temp_string,4);
					lcdPrintData("mV",2);
				}
				else if(profiles[profile_index].type == CA)
				{
					lcdPrintData(" ",1);
					lcdPrintData("Steps=",6);
					sprintf(temp_string,"%2d",profiles[profile_index].op6);
					lcdPrintData(temp_string,2);
				}

				lcdGotoXY(0,2);
				if(profile_edit_sel == EDIT_NOSEL)
					lcdPrintData("~",1);
				else
					lcdPrintData(">",1);

				lcdPrintData("Range=",6);
				if(profiles[profile_index].curr_range == RANGE_10UA)
					lcdPrintData("0-10uA",5);
				else
					lcdPrintData("0-50uA",6);
				
			}


			while(buttonHandler(profiles,&status,&profile_index,&profile_opt_index,&profile_edit_index,&profile_edit_sel,&length)!=1) {}
		}
		else if (status == PROFILE_RESULTS)
		{
			//clear display
			lcdClear();
			lcdHome();

			//display name
			lcdPrintData(profiles[profile_index].name, 15);
		

			lcdGotoXY(0,1);
			lcdPrintData("Test Complete",13);
			//lcdGotoXY(0,2);
			//lcdPrintData("~Store Results?",15);

			while(buttonHandler(profiles,&status,&profile_index,&profile_opt_index,&profile_edit_index,&profile_edit_sel,&length)!=1) {}
		}
		
	}

	return 0;


}

void send_string(char* string)
{
	uint8_t i = 0;
	while(string[i]!='\0')
	{
		do{}
		while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, string[i]);
		i++;
	}
}

int buttonHandler(profile profiles[PROFILES_LENGTH], uint8_t* status, uint8_t* profile_index, uint8_t* profile_opt_index, uint8_t* profile_edit_index, uint8_t* profile_edit_sel, int16_t* length)
{
	uint8_t dir;
	dir = INVALID;
	while(dir == INVALID)
	{
	//get action
	if(bit_is_set(PORTA.IN,4))
		dir = RIGHT;
	else if(bit_is_set(PORTA.IN,5))
		dir = DOWN;
	else if(bit_is_set(PORTA.IN,6))
		dir = LEFT;
	else if(bit_is_set(PORTA.IN,7))
		dir = UP;
	}
	//else
	//	return 0;

	//perform instruction
	if(*status == PROFILE_SEL)
	{
		if(dir == UP)
		{
			if((*profile_index) > 0)
				(*profile_index)--;
		}
		else if(dir == DOWN)
		{
			if((*profile_index) < (PROFILES_LENGTH-1))
				(*profile_index)++;
		}
		else if(dir == RIGHT)
		{
			*status = PROFILE_OPT;
			*profile_opt_index = 0;	
		}
	}
	else if(*status == PROFILE_OPT)
	{
		if(dir == UP)
		{
			if(*profile_opt_index == 1)
				*profile_opt_index = 0;
		}
		else if(dir == DOWN)
		{
			if(*profile_opt_index == 0)
				*profile_opt_index = 1;
		}
		else if(dir == RIGHT)
		{
			if(*profile_opt_index == 0)
				*status = PROFILE_TEST;
			else
				*status = PROFILE_EDIT;
		}
		else if(dir == LEFT)
		{
			*status = PROFILE_SEL;
		}	
	}
	else if(*status == PROFILE_TEST)
	{

	}
	else if(*status == PROFILE_EDIT)
	{
		//TODO editing of name and type
		if(dir == LEFT)
		{
			if(*profile_edit_sel == EDIT_NOSEL)
			{
				*status = PROFILE_OPT;
				*profile_edit_index = 0;
			}
			else
			{
				*profile_edit_sel = EDIT_NOSEL;
				eeprom_write_block((const void*)&(profiles[*profile_index]), (void*)&(profilesEE[*profile_index]), sizeof(profile));
			
			}
		}	
		else if(dir == RIGHT)
			*profile_edit_sel = EDIT_SEL;
		else if(dir == UP)
		{
			if(*profile_edit_sel == EDIT_NOSEL)
			{
				if(*profile_edit_index > 0)
					(*profile_edit_index)--;	
			}
			else if(*profile_edit_index == 0)
			{
				
			}
			else if(*profile_edit_index == 1)
			{
				if(profiles[*profile_index].type == SWV)
				{
					if(profiles[*profile_index].op1<200)
						profiles[*profile_index].op1++;
				}
				else if(profiles[*profile_index].type == CV)
				{
					if(profiles[*profile_index].op1<5000)
						profiles[*profile_index].op1 = profiles[*profile_index].op1 + 10;
				}
				else if(profiles[*profile_index].type == ACV)
				{
					if(profiles[*profile_index].op1<1000)
						profiles[*profile_index].op1++;
				}
				else if(profiles[*profile_index].type == LSV)
				{
					if(profiles[*profile_index].op1<9999)
						profiles[*profile_index].op1 = profiles[*profile_index].op1 + 10;
				}
				else if(profiles[*profile_index].type == CONSTVOLT)
				{
					if(profiles[*profile_index].op1<1600)
						profiles[*profile_index].op1 = profiles[*profile_index].op1 + 10;
				}
				else if(profiles[*profile_index].type == CA)
				{
					if(profiles[*profile_index].op1<9900)
						profiles[*profile_index].op1 = profiles[*profile_index].op1 + 100;
				}
				
			}
			else if(*profile_edit_index == 2)
			{
				
				if(profiles[*profile_index].type == SWV)
				{
					if(profiles[*profile_index].op2<1600)
						profiles[*profile_index].op2 = profiles[*profile_index].op2 + 10;
				}
				else if(profiles[*profile_index].type == CV)
				{
					if(profiles[*profile_index].op2<1600)
						profiles[*profile_index].op2 = profiles[*profile_index].op2 + 10;
				}
				else if(profiles[*profile_index].type == ACV)
				{
					if(profiles[*profile_index].op2<1600)
						profiles[*profile_index].op2 = profiles[*profile_index].op2 + 10;
				}
				else if(profiles[*profile_index].type == LSV)
				{
					if(profiles[*profile_index].op2<1600)
						profiles[*profile_index].op2 = profiles[*profile_index].op2 + 10;
				}
				else if(profiles[*profile_index].type == CONSTVOLT)
				{
					if(profiles[*profile_index].op2<9990)
						profiles[*profile_index].op2 = profiles[*profile_index].op2 + 1;
				}
				else if(profiles[*profile_index].type == CA)
				{
					if(profiles[*profile_index].op2<1600)
						profiles[*profile_index].op2 = profiles[*profile_index].op2 + 10;
				}

				
			}
			else if(*profile_edit_index == 3)
			{
				if(profiles[*profile_index].type == SWV)
				{
					if(profiles[*profile_index].op3<1600)
						profiles[*profile_index].op3 = profiles[*profile_index].op3 + 10;
				}
				else if(profiles[*profile_index].type == CV)
				{
					if(profiles[*profile_index].op3<1600)
						profiles[*profile_index].op3 = profiles[*profile_index].op3 + 10;
				}
				else if(profiles[*profile_index].type == ACV)
				{
					if(profiles[*profile_index].op3<9999)
						profiles[*profile_index].op3 = profiles[*profile_index].op3 + 1;
				}
				else if(profiles[*profile_index].type == LSV)
				{
					if(profiles[*profile_index].op3<1600)
						profiles[*profile_index].op3 = profiles[*profile_index].op3 + 10;
				}
				else if(profiles[*profile_index].type == CA)
				{
					if(profiles[*profile_index].op3<9990)
						profiles[*profile_index].op3 = profiles[*profile_index].op3 + 10;
				}


				
			}
			else if(*profile_edit_index == 4)
			{
				if(profiles[*profile_index].type == SWV)
				{
					if(profiles[*profile_index].op4<1600)
						profiles[*profile_index].op4++;
				}
				else if(profiles[*profile_index].type == CV)
				{
					if(profiles[*profile_index].op4<99)
						profiles[*profile_index].op4++;
				}
				else if(profiles[*profile_index].type == ACV)
				{
					if(profiles[*profile_index].op4<1600)
						profiles[*profile_index].op4 = profiles[*profile_index].op4 + 10;
				}
				else if(profiles[*profile_index].type == LSV)
				{
					if(profiles[*profile_index].op4<5000)
						profiles[*profile_index].op4 = profiles[*profile_index].op4 + 10;
				}
				else if(profiles[*profile_index].type == CA)
				{
					if(profiles[*profile_index].op4<9900)
						profiles[*profile_index].op4 = profiles[*profile_index].op4 + 100;
				}
			}
			else if(*profile_edit_index == 5)
			{
				if(profiles[*profile_index].type == SWV)
				{
					if(profiles[*profile_index].op5<1600)
						profiles[*profile_index].op5++;
				}
				else if(profiles[*profile_index].type == CV)
				{
					if(profiles[*profile_index].op5<1600)
						profiles[*profile_index].op5++;
				}
				else if(profiles[*profile_index].type == ACV)
				{
					if(profiles[*profile_index].op5<1600)
						profiles[*profile_index].op5 = profiles[*profile_index].op5 + 10;
				}
				else if(profiles[*profile_index].type == LSV)
				{
					if(profiles[*profile_index].op5<1600)
						profiles[*profile_index].op5++;
				}
				else if(profiles[*profile_index].type == CA)
				{
					if(profiles[*profile_index].op5<9990)
						profiles[*profile_index].op5 = profiles[*profile_index].op5 + 10;
				}
			}
			else if(*profile_edit_index == 6)
			{
				if(profiles[*profile_index].type == SWV)
				{
					if(profiles[*profile_index].curr_range == RANGE_50UA)
						profiles[*profile_index].curr_range = RANGE_10UA;
				}
				else if(profiles[*profile_index].type == CV)
				{
					if(profiles[*profile_index].curr_range == RANGE_50UA)
						profiles[*profile_index].curr_range = RANGE_10UA;
				
				}
				else if(profiles[*profile_index].type == ACV)
				{
					if(profiles[*profile_index].op6<1600)
						profiles[*profile_index].op6 = profiles[*profile_index].op6 + 1;
				}
				else if(profiles[*profile_index].type == LSV)
				{
					if(profiles[*profile_index].curr_range == RANGE_50UA)
						profiles[*profile_index].curr_range = RANGE_10UA;
				
				}
				else if(profiles[*profile_index].type == CA)
				{
					if(profiles[*profile_index].op6<99)
						profiles[*profile_index].op6 = profiles[*profile_index].op6 + 1;
				}
			}
			else if(*profile_edit_index == 7)
			{
				if(profiles[*profile_index].curr_range == RANGE_50UA)
					profiles[*profile_index].curr_range = RANGE_10UA;
			}
				
		}
		else if(dir == DOWN)
		{
			if(*profile_edit_sel == EDIT_NOSEL)
			{
				if((*profile_edit_index < 6 && profiles[*profile_index].type != CONSTVOLT) || 
				(*profile_edit_index == 6 && profiles[*profile_index].type == ACV) || 
				(*profile_edit_index == 6 && profiles[*profile_index].type == CA) || 
				(*profile_edit_index < 2 && profiles[*profile_index].type == CONSTVOLT) )
					(*profile_edit_index)++;	
			}
			else if(*profile_edit_index == 0)
			{

			}
			else if(*profile_edit_index == 1)
			{
				if(profiles[*profile_index].type == SWV)
				{
					if(profiles[*profile_index].op1>9)
						profiles[*profile_index].op1 = profiles[*profile_index].op1 - 10;
				}
				else if(profiles[*profile_index].type == CV)
				{
					if(profiles[*profile_index].op1>9)
						profiles[*profile_index].op1 = profiles[*profile_index].op1 - 10;
				}
				else if(profiles[*profile_index].type == ACV)
				{
					if(profiles[*profile_index].op1>10)
						profiles[*profile_index].op1--;
				}
				else if(profiles[*profile_index].type == LSV)
				{
					if(profiles[*profile_index].op1>1)
						profiles[*profile_index].op1--;
				}
				else if(profiles[*profile_index].type == CONSTVOLT)
				{
					if(profiles[*profile_index].op1>-1600)
						profiles[*profile_index].op1 = profiles[*profile_index].op1 - 10;
				}
				else if(profiles[*profile_index].type == CA)
				{
					if(profiles[*profile_index].op1>99)
						profiles[*profile_index].op1 = profiles[*profile_index].op1 - 100;
				}
				
			}
			else if(*profile_edit_index == 2)
			{
				
				if(profiles[*profile_index].type == SWV)
				{
					if(profiles[*profile_index].op2>-1600)
						profiles[*profile_index].op2 = profiles[*profile_index].op2 - 10;
				}
				else if(profiles[*profile_index].type == CV)
				{
					if(profiles[*profile_index].op2>-1600)
						profiles[*profile_index].op2 = profiles[*profile_index].op2 - 10;
				}
				else if(profiles[*profile_index].type == ACV)
				{
					if(profiles[*profile_index].op2>9)
						profiles[*profile_index].op2 = profiles[*profile_index].op2 - 10;
				}
				else if(profiles[*profile_index].type == LSV)
				{
					if(profiles[*profile_index].op2>-1600)
						profiles[*profile_index].op2 = profiles[*profile_index].op2 - 10;
				}
				else if(profiles[*profile_index].type == CONSTVOLT)
				{
					if(profiles[*profile_index].op2>9)
						profiles[*profile_index].op2 = profiles[*profile_index].op2 - 1;
				}
				else if(profiles[*profile_index].type == CA)
				{
					if(profiles[*profile_index].op2>-1600)
						profiles[*profile_index].op2 = profiles[*profile_index].op2 - 10;
				}
				
			}
			else if(*profile_edit_index == 3)
			{
				if(profiles[*profile_index].type == SWV)
				{
					if(profiles[*profile_index].op3>-1600)
						profiles[*profile_index].op3 = profiles[*profile_index].op3 - 10;
				}
				else if(profiles[*profile_index].type == CV)
				{
					if(profiles[*profile_index].op3>-1600)
						profiles[*profile_index].op3 = profiles[*profile_index].op3 - 10;
				}
				else if(profiles[*profile_index].type == ACV)
				{
					if(profiles[*profile_index].op3>0)
						profiles[*profile_index].op3 = profiles[*profile_index].op3 - 1;
				}
				else if(profiles[*profile_index].type == LSV)
				{
					if(profiles[*profile_index].op3>-1600)
						profiles[*profile_index].op3 = profiles[*profile_index].op3 - 10;
				}
				else if(profiles[*profile_index].type == CA)
				{
					if(profiles[*profile_index].op3>9)
						profiles[*profile_index].op3 = profiles[*profile_index].op3 - 10;
				}
				
			}
			else if(*profile_edit_index == 4)
			{
				if(profiles[*profile_index].type == SWV)
				{
					if(profiles[*profile_index].op4>0)
						profiles[*profile_index].op4--;
				}
				else if(profiles[*profile_index].type == CV)
				{
					if(profiles[*profile_index].op4>1)
						profiles[*profile_index].op4--;
				}
				else if(profiles[*profile_index].type == ACV)
				{
					if(profiles[*profile_index].op4>-1600)
						profiles[*profile_index].op4 = profiles[*profile_index].op4 - 10;
				}
				else if(profiles[*profile_index].type == LSV)
				{
					if(profiles[*profile_index].op4>10)
						profiles[*profile_index].op4 = profiles[*profile_index].op4 - 10;
				}
				else if(profiles[*profile_index].type == CA)
				{
					if(profiles[*profile_index].op4>99)
						profiles[*profile_index].op4 = profiles[*profile_index].op4 - 100;
				}
			}
			else if(*profile_edit_index == 5)
			{
				if(profiles[*profile_index].type == SWV)
				{
					if(profiles[*profile_index].op5>0)
						profiles[*profile_index].op5--;
				}
				else if(profiles[*profile_index].type == CV)
				{
					if(profiles[*profile_index].op5>1)
						profiles[*profile_index].op5--;
				}
				else if(profiles[*profile_index].type == ACV)
				{
					if(profiles[*profile_index].op5>-1600)
						profiles[*profile_index].op5 = profiles[*profile_index].op5 - 10;
				}
				else if(profiles[*profile_index].type == LSV)
				{
					if(profiles[*profile_index].op5>1)
						profiles[*profile_index].op5--;
				}
				else if(profiles[*profile_index].type == CA)
				{
					if(profiles[*profile_index].op5>10)
						profiles[*profile_index].op5 = profiles[*profile_index].op5 - 10;
				}
			}
			else if(*profile_edit_index == 6)
			{
				if(profiles[*profile_index].type == SWV)
				{
					if(profiles[*profile_index].curr_range == RANGE_10UA)
						profiles[*profile_index].curr_range = RANGE_50UA;
				}
				else if(profiles[*profile_index].type == CV)
				{
					if(profiles[*profile_index].curr_range == RANGE_10UA)
						profiles[*profile_index].curr_range = RANGE_50UA;
				
				}
				else if(profiles[*profile_index].type == ACV)
				{
					if(profiles[*profile_index].op6>0)
						profiles[*profile_index].op6 = profiles[*profile_index].op6 - 1;
				}
				else if(profiles[*profile_index].type == LSV)
				{
					if(profiles[*profile_index].curr_range == RANGE_10UA)
						profiles[*profile_index].curr_range = RANGE_50UA;
				
				}
				else if(profiles[*profile_index].type == CA)
				{
					if(profiles[*profile_index].op6>1)
						profiles[*profile_index].op6 = profiles[*profile_index].op6 - 1;
				}
			}
			else if(*profile_edit_index == 7)
			{
				if(profiles[*profile_index].curr_range == RANGE_10UA)
					profiles[*profile_index].curr_range = RANGE_50UA;
			}
		}
			
	}
	else if (*status == PROFILE_RESULTS)
	{
		if(dir == RIGHT)
		{//TODO write results to EEPROM
		}
		else
			*status = PROFILE_SEL;
	}
	else
		_delay_ms(1000);//return 0;

	_delay_ms(200);
	return 1;
}

int16_t SWV_test (char* name, int16_t freq, int16_t start, int16_t stop, int16_t height, int16_t increment, uint8_t curr_range)
{
	int16_t current;
	int32_t half_period_temp;
	uint16_t half_period;
	uint16_t samples;
	uint16_t i,j,k;
	bool up;

	#ifdef DEBUG
		uint16_t l;
		bool cont;
	#endif

	int16_t start_DAC, stop_DAC, height_DAC, increment_DAC;
	
	//storing ADC results
	int16_t forwardCurrent[SWV_MAX_DATAPOINTS/2];
	int16_t reverseCurrent[SWV_MAX_DATAPOINTS/2];
	int16_t result_buffer[SWV_BUFFER_SIZE];

	#ifdef DEBUG
		int16_t ADC_waveform[SWV_MAX_DATAPOINTS/2];
		int16_t DAC_waveform[SWV_MAX_DATAPOINTS/2];
	#endif

	//check limits
	if(start<-1600 || start>1600 || stop<-1600 || stop>1600 || freq>200 || freq<1 || height>1600 || increment>1600)
	{
		lcdClear();
		lcdHome();
		lcdPrintData("outside limits",14);
		return -1;
	}

	//convert to DAC index
	start_DAC = (int16_t) (round(start*(2048/1600))+2048);  
	stop_DAC = (int16_t) (round(stop*(2048/1600))+2048);   

	//determine direction
	if((stop-start)>0)
	{
		//up
		increment_DAC = (int16_t) (round(increment*(2048/1600)));
		height_DAC = (int16_t) (round(height*(2048/1600)));
		up=true;
	}
	else
	{
		//down
		increment_DAC = (int16_t) (round(increment*(-2048/1600)));
		height_DAC = (int16_t) (round(height*(-2048/1600)));
		up=false;
	}

	samples = (stop_DAC-start_DAC-height_DAC/2)/increment_DAC;
	if(samples > SWV_MAX_DATAPOINTS/2)
	{
		lcdClear();
		lcdHome();
		lcdPrintData("too many data points",20);
		return -1;
	}


	//calculate period in clock cycles
	half_period_temp = (int32_t) (round(F_CPU/(2*freq)));
	if(half_period_temp <= 50000) //above 20Hz
	{
		TIMER.CTRLA = TC_CLKSEL_DIV1_gc;
		half_period = half_period_temp;
	}
	else if(half_period_temp <= 100000) //10-20Hz
	{
		TIMER.CTRLA = TC_CLKSEL_DIV2_gc;
		half_period = half_period_temp/2;
	}
	else if(half_period_temp <= 200000) //5-10Hz
	{
		TIMER.CTRLA = TC_CLKSEL_DIV4_gc;
		half_period = half_period_temp/4;
	}
	else if(half_period_temp <= 400000) //2.5-5Hz
	{
		TIMER.CTRLA = TC_CLKSEL_DIV8_gc;
		half_period = half_period_temp/8;
	}
	else //if(half_period_temp <= 3200000) //0.3125-2.5Hz
	{
		TIMER.CTRLA = TC_CLKSEL_DIV64_gc;
		half_period = half_period_temp/64;
	}

	current = start_DAC-(height_DAC/2);
	i = 0;
	j = 0;
	k = 0;

	#ifdef DEBUG
		l = 0;
		cont = true;
	#endif

	//change switches
	PORTE.OUTSET = PIN1_bm; //switch0
	PORTE.OUTSET = PIN2_bm; //switch2
	if(curr_range == RANGE_10UA)
		PORTE.OUTCLR = PIN3_bm; //switch3
	else
		PORTE.OUTSET = PIN3_bm; //switch3
	//_delay_ms(50);
	PORTE.OUTCLR = PIN0_bm; //switch1
	_delay_ms(200);

while(1)
{
	//set DAC and trigger timer
	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,current,CH0);
	TIMER.CNT = 0;

	//calculate next DAC value
	current += height_DAC;
	//break if complete
	if((up&&(current>stop_DAC)) || (!up&&(current<stop_DAC)))
		break;

	#ifdef DEBUG
		if((up&&(current>DEBUG_INDEX_STOP))||(!up&&(current<DEBUG_INDEX_STOP)))
			cont = false;	
	#endif

	//ADC measurements
	while(TIMER.CNT<half_period) {
		ADC_Ch_Conversion_Start(&ADCA.CH1);
		while(!ADC_Ch_Conversion_Complete(&ADCA.CH1) && TIMER.CNT<half_period) {}
		
		//get end point
		result_buffer[j] = ADC_ResultCh_GetWord_Signed(&ADCA.CH1,ADC_OFFSET);
		j++;
		if(j==SWV_BUFFER_SIZE)
			j=0;

		#ifdef DEBUG
			ADC_Ch_Conversion_Start(&ADCA.CH0);
			while(!ADC_Ch_Conversion_Complete(&ADCA.CH0) && TIMER.CNT<half_period) {}
			if(cont) {
				if(l < samples) {
					ADC_waveform[l] = ADC_ResultCh_GetWord_Signed(&ADCA.CH1,ADC_OFFSET);
					DAC_waveform[l] = ADC_ResultCh_GetWord_Signed(&ADCA.CH0,ADC_OFFSET);
					l++;
				}
				else
					l = 0;
			}	
		#endif		
	}

	forwardCurrent[i] = 0;
	for(k = 0; k < SWV_BUFFER_SIZE; k++)
		forwardCurrent[i] += result_buffer[k];

	//set DAC and trigger timer
	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,current,CH0);
	TIMER.CNT = 0;

	//calculate next DAC value
	current -= (height_DAC-increment_DAC);

	//ADC measurements
	while(TIMER.CNT<half_period) {
		ADC_Ch_Conversion_Start(&ADCA.CH1);
		while(!ADC_Ch_Conversion_Complete(&ADCA.CH1) && TIMER.CNT<half_period) {}
		
		//get end point
		result_buffer[j] = ADC_ResultCh_GetWord_Signed(&ADCA.CH1,ADC_OFFSET);
		j++;
		if(j==SWV_BUFFER_SIZE)
			j=0;
		
		#ifdef DEBUG
			ADC_Ch_Conversion_Start(&ADCA.CH0);
			while(!ADC_Ch_Conversion_Complete(&ADCA.CH0) && TIMER.CNT<half_period) {}
			if(cont) {
				if(l < samples) {
					ADC_waveform[l] = ADC_ResultCh_GetWord_Signed(&ADCA.CH1,ADC_OFFSET);
					DAC_waveform[l] = ADC_ResultCh_GetWord_Signed(&ADCA.CH0,ADC_OFFSET);
					l++;
				}
				else
					l = 0;
			}	
		#endif
				
	}

	reverseCurrent[i] = 0;
	for(k = 0; k < 16; k++)
		reverseCurrent[i] += result_buffer[k];
	
	i++;
}

	PORTE.OUTSET = PIN0_bm; //switch1
	//PORTE.OUTCLR = PIN0_bm; //switch1
	PORTE.OUTCLR = PIN1_bm; //switch0
	PORTE.OUTCLR = PIN2_bm; //switch2
	PORTE.OUTCLR = PIN3_bm; //switch3
	current = 2048;
	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,current,CH0);

	//start output to USB
	#ifndef DEBUG

		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, SWV);
		for(j = 0; j < 15; j++)
		{
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, name[j]);
		}
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, freq>>8);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, freq);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, start>>8);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, start);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, stop>>8);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, stop);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, height>>8);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, height);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, increment>>8);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, increment);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, curr_range);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, i>>8);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, i);

		for(j = 0; j < i; j++)
		{
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, forwardCurrent[j]>>8);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, forwardCurrent[j]);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, reverseCurrent[j]>>8);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, reverseCurrent[j]);
		}

		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0,SWV);

	#endif

	#ifdef DEBUG
		for(j = 0; j < DEBUGSIZE; j++)
		{
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, DAC_waveform[j]>>8);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, DAC_waveform[j]);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, ADC_waveform[j]>>8);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, ADC_waveform[j]);
		}

	#endif

	return i;
}

int16_t CV_test (char* name, int16_t slope, int16_t start, int16_t stop, int16_t scans, int16_t sample_rate, uint8_t curr_range)
{
	uint16_t step_time;
	uint16_t steps_per_sample;
	uint16_t steps_taken;
	uint16_t ramps;
	uint16_t samples;
	uint16_t i,j,k;
	bool up;

	int16_t current_DAC, min_DAC, max_DAC;
	
	//storing ADC results
	int16_t current[CV_MAX_DATAPOINTS];
	int16_t result_buffer[CV_BUFFER_SIZE];

	//check limits
	if(start<-1600 || start>1600 || stop<-1600 || stop>1600 || slope>5000 || slope<10 || sample_rate<1 || sample_rate>1600)
	{
		lcdClear();
		lcdHome();
		lcdPrintData("outside limits",14);
		return -1;
	}

	//determine starting direction and calculate
	if((stop-start)>0)
	{
		up=true;
		min_DAC = (int16_t) (round(start*(4096.0/3300))+2048);  
		max_DAC = (int16_t) (round(stop*(4096.0/3300))+2048);   
	}
	else
	{
		up=false;
		max_DAC = (int16_t) (round(start*(4096.0/3300))+2048);  
		min_DAC = (int16_t) (round(stop*(4096.0/3300))+2048);   
	}

	ramps = 2*scans;

	steps_per_sample = (uint16_t) (round(sample_rate*(4096.0/3300)));
	
	samples = 2*scans*((max_DAC-min_DAC)/steps_per_sample);

	if(samples > CV_MAX_DATAPOINTS)
	{
		lcdClear();
		lcdHome();
		lcdPrintData("too many data points",20);
		return -1;
	}

	//2,000,000 [cycles/sec] * 1/slope [sec/mV] * 3300/4096 [mv/index]
	if(slope > 30)
	{
		step_time = (uint16_t) (round(2000000*(1.0/slope)*(3300.0/4096)));
		TIMER.CTRLA = TC_CLKSEL_DIV1_gc;
	}
	else
	{
		step_time = (uint16_t) (round(500000*(1.0/slope)*(3300.0/4096)));
		TIMER.CTRLA = TC_CLKSEL_DIV4_gc;
	}
	

	if(up)
		current_DAC = min_DAC;
	else
		current_DAC = max_DAC;

	i = 0;
	j = 0;
	steps_taken = 0;
	k = 0;

	for(k = 0; k < CV_BUFFER_SIZE; k++)
		result_buffer[k] = 0;

	//change switches
	PORTE.OUTSET = PIN1_bm; //switch0
	PORTE.OUTSET = PIN2_bm; //switch2
	if(curr_range == RANGE_10UA)
		PORTE.OUTCLR = PIN3_bm; //switch3
	else
		PORTE.OUTSET = PIN3_bm; //switch3
	//_delay_ms(50);
	PORTE.OUTCLR = PIN0_bm; //switch1

	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,current_DAC,CH0);
	_delay_ms(250);

while(1)
{
	//set DAC and trigger timer
	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,current_DAC,CH0);
	TIMER.CNT = 0;

	//calculate next DAC value
	if(up)
		current_DAC++;
	else
		current_DAC--;

	//decision making
	if(up && current_DAC >= max_DAC)
	{
		up = false; //switch to going down
		ramps--;
		if(ramps==0)
			break;
	}
	else if(!up && current_DAC <= min_DAC)
	{
		up = true; //switch to going up
		ramps--;
		if(ramps==0)
			break;
	}

	current[i] = 0;
	//ADC measurements
	while(TIMER.CNT<step_time) {
		ADC_Ch_Conversion_Start(&ADCA.CH1);
		while(!ADC_Ch_Conversion_Complete(&ADCA.CH1) && TIMER.CNT<step_time) {}
		
		if(current[i] == 0)
			current[i] = ADC_ResultCh_GetWord_Signed(&ADCA.CH1,ADC_OFFSET);
		else
			current[i] = (current[i] + ADC_ResultCh_GetWord_Signed(&ADCA.CH1,ADC_OFFSET)) >> 1;
	}

	steps_taken++;
	if(steps_taken >= steps_per_sample)
	{
		steps_taken = 0;
		i++;
	}
}

	PORTE.OUTSET = PIN0_bm; //switch1
	//PORTE.OUTCLR = PIN0_bm; //switch1
	PORTE.OUTCLR = PIN1_bm; //switch0
	PORTE.OUTCLR = PIN2_bm; //switch2
	PORTE.OUTCLR = PIN3_bm; //switch3
	current_DAC = 2048;
	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,current_DAC,CH0);

	//start output to USB
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, CV);
	for(j = 0; j < 15; j++)
	{
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, name[j]);
	}
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, slope>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, slope);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, start>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, start);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, stop>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, stop);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, scans>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, scans);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, sample_rate>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, sample_rate);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, curr_range);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, i>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, i);

	for(j = 0; j < i; j++)
	{
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, current[j]>>8);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, current[j]);
	}

	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0,CV);

	return i;
}

int16_t ACV_test (char* name, int16_t freq, int16_t height, int16_t cycles, int16_t start, int16_t stop, int16_t increment, uint8_t curr_range)
{

uint8_t i,j,k;
int16_t current_DAC,steps,point_period,point_period_temp;
int16_t V[ACV_POINTSPERCYCLE],I[ACV_POINTSPERCYCLE];
bool up;
int16_t start_DAC, stop_DAC, height_DAC, increment_DAC;
uint8_t min,max;
int32_t temp_mag, temp_phase;

	//convert to DAC index
	start_DAC = (int16_t) (round(start*(2048/1600))+2048);  
	stop_DAC = (int16_t) (round(stop*(2048/1600))+2048);   
	height_DAC = (int16_t) (round(height*(2048/1600)));

	//determine direction
	if((stop-start)>0)
	{
		//up
		increment_DAC = (int16_t) (round(increment*(2048/1600)));
		up=true;
	}
	else
	{
		//down
		increment_DAC = (int16_t) (round(increment*(-2048/1600)));
		up=false;
	}

	//calculate period in clock cycles
	point_period_temp = (int32_t) (round(F_CPU/(freq*ACV_POINTSPERCYCLE)));
	if(point_period_temp <= 50000)
	{
		TIMER.CTRLA = TC_CLKSEL_DIV1_gc;
		point_period = point_period_temp;
	}
	else if(point_period_temp <= 100000) 
	{
		TIMER.CTRLA = TC_CLKSEL_DIV2_gc;
		point_period = point_period_temp/2;
	}
	else if(point_period_temp <= 200000)
	{
		TIMER.CTRLA = TC_CLKSEL_DIV4_gc;
		point_period = point_period_temp/4;
	}
	else if(point_period_temp <= 400000) 
	{
		TIMER.CTRLA = TC_CLKSEL_DIV8_gc;
		point_period = point_period_temp/8;
	}
	else //if(period_temp <= 3200000) 
	{
		TIMER.CTRLA = TC_CLKSEL_DIV64_gc;
		point_period = point_period_temp/64;
	}

	//calculate number of DC offset steps there are
	steps = ((stop_DAC-start_DAC)/increment_DAC)+1;
	int16_t mag[steps],phase[steps];

	//TODO calculate sine wave resolution?
	//TODO memory check?

	//calculate points for sine wave
	for(i = 0; i < ACV_POINTSPERCYCLE; i++)
	{
		V[i] = (int16_t) (round(sin(((2*M_PI)/ACV_POINTSPERCYCLE)*i)*height_DAC)) + start_DAC;
	}
	current_DAC = start_DAC;

		//change switches
	PORTE.OUTSET = PIN1_bm; //switch0
	PORTE.OUTSET = PIN2_bm; //switch2
	if(curr_range == RANGE_10UA)
		PORTE.OUTCLR = PIN3_bm; //switch3
	else
		PORTE.OUTSET = PIN3_bm; //switch3
	//_delay_ms(50);
	PORTE.OUTCLR = PIN0_bm; //switch1

	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,current_DAC,CH0);
	_delay_ms(250);

	for(k = 0; k < steps; k++)
	{
		mag[k] = 0;
		phase[k] = 0;
		temp_mag = 0;
		temp_phase = 0;
		 
		//prform specified number of cycles at given DC offset
		for(j = 0; j < cycles; j++)
		{
			//apply sine wave and record current
			for(i = 0; i < ACV_POINTSPERCYCLE; i++)
			{
				I[i] = 0;
				
				//set DAC and trigger timer
				while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
					DAC_Channel_Write(&DACB,V[i],CH0);
				TIMER.CNT = 0;

				//ADC measurements
				while(TIMER.CNT<point_period) 
				{
					ADC_Ch_Conversion_Start(&ADCA.CH1);
					while(!ADC_Ch_Conversion_Complete(&ADCA.CH1) && TIMER.CNT<point_period) {}
					if(I[i] == 0)
						I[i] = ADC_ResultCh_GetWord_Signed(&ADCA.CH1,ADC_OFFSET);
					else
						I[i] = (I[i] + ADC_ResultCh_GetWord_Signed(&ADCA.CH1,ADC_OFFSET)) >> 1;
				}
			}

			min = 0;
			max = 0;
			for(i = 0; i < ACV_POINTSPERCYCLE; i++)
			{
				if(I[i] > I[max])
					max = i;
				if(I[i] < I[min])
					min = i;
			}

			//calculate magnitude and phase shift
			temp_mag += ((I[max]-I[min]) >> 1);
			temp_phase += ((max+min) >> 1); 

		}

		//average out results
		mag[k] = temp_mag/cycles; 
		phase[k] = temp_phase/cycles;

		//set up new points
		current_DAC+=increment_DAC;

		//calculate new sine wave points
		for(i = 0; i < ACV_POINTSPERCYCLE; i++)
		{
			V[i]+=increment_DAC;
		}
	}

	PORTE.OUTSET = PIN0_bm; //switch1
	//PORTE.OUTCLR = PIN0_bm; //switch1
	PORTE.OUTCLR = PIN1_bm; //switch0
	PORTE.OUTCLR = PIN2_bm; //switch2
	PORTE.OUTCLR = PIN3_bm; //switch3
	current_DAC = 2048;
	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,current_DAC,CH0);

	//start output to USB
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, ACV);
	for(j = 0; j < 15; j++)
	{
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, name[j]);
	}
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, freq>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, freq);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, height>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, height);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, cycles>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, cycles);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, start>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, start);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, stop>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, stop);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, increment>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, increment);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, curr_range);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, k>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, k);

	for(j = 0; j < k; j++)
	{
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, mag[j]>>8);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, mag[j]);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, phase[j]>>8);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, phase[j]);
	}

/*
do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, ACV_POINTSPERCYCLE>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, ACV_POINTSPERCYCLE);

	for(j = 0; j < ACV_POINTSPERCYCLE; j++)
	{
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, V[j]>>8);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, V[j]);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, I[j]>>8);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, I[j]);
	}

*/
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0,ACV);

	return k;

}

int16_t LSV_test (char* name, int16_t settle, int16_t start, int16_t stop, int16_t slope, int16_t sample_rate, uint8_t curr_range)
{

	uint16_t step_time;
	uint16_t steps_per_sample;
	uint16_t steps_taken;
	uint16_t samples;
	uint16_t i,j,k,l;
	bool up;

	int16_t current_DAC, start_DAC, stop_DAC;
	
	//storing ADC results
	int16_t current[LSV_MAX_DATAPOINTS];
	int16_t result_buffer[LSV_BUFFER_SIZE];

	//check limits
	if(start<-1600 || start>1600 || stop<-1600 || stop>1600 || slope>5000 || slope<10 || sample_rate<1 || sample_rate>1600)
	{
		lcdClear();
		lcdHome();
		lcdPrintData("outside limits",14);
		return -1;
	}

	start_DAC = (int16_t) (round(start*(4096.0/3300))+2048);  
	stop_DAC = (int16_t) (round(stop*(4096.0/3300))+2048);  
	steps_per_sample = (uint16_t) (round(sample_rate*(4096.0/3300)));

	//determine starting direction and calculate
	if((stop-start)>0)
	{
		up=true;
		samples = (stop_DAC-start_DAC)/steps_per_sample;
	}
	else
	{
		up=false;
		samples = (start_DAC-stop_DAC)/steps_per_sample;
	}

	if(samples > LSV_MAX_DATAPOINTS)
	{
		lcdClear();
		lcdHome();
		lcdPrintData("too many data points",20);
		return -1;
	}

	//2,000,000 [cycles/sec] * 1/slope [sec/mV] * 3300/4096 [mv/index]
	if(slope > 30)
	{
		step_time = (uint16_t) (round(2000000*(1.0/slope)*(3300.0/4096)));
		TIMER.CTRLA = TC_CLKSEL_DIV1_gc;
	}
	else
	{
		step_time = (uint16_t) (round(500000*(1.0/slope)*(3300.0/4096)));
		TIMER.CTRLA = TC_CLKSEL_DIV4_gc;
	}
		
	current_DAC = start_DAC;

	i = 0;
	j = 0;
	k = 0;
	steps_taken = 0;

	for(k = 0; k < CV_BUFFER_SIZE; k++)
		result_buffer[k] = 0;

	//change switches
	PORTE.OUTSET = PIN1_bm; //switch0
	PORTE.OUTSET = PIN2_bm; //switch2
	if(curr_range == RANGE_10UA)
		PORTE.OUTCLR = PIN3_bm; //switch3
	else
		PORTE.OUTSET = PIN3_bm; //switch3
	//_delay_ms(50);
	PORTE.OUTCLR = PIN0_bm; //switch1

	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,current_DAC,CH0);
	_delay_ms(250);

	//wait for settle time
	for(l = 0; l < settle; l++)
		_delay_ms(1000);

	while((up&&(current_DAC<stop_DAC))||(!up&&current_DAC>stop_DAC))
	{
		//set DAC and trigger timer
		while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
			DAC_Channel_Write(&DACB,current_DAC,CH0);
		TIMER.CNT = 0;

		//calculate next DAC value
		if(up)
			current_DAC++;
		else
			current_DAC--;

		current[i] = 0;

		//ADC measurements
		while(TIMER.CNT<step_time) {
			ADC_Ch_Conversion_Start(&ADCA.CH1);
			while(!ADC_Ch_Conversion_Complete(&ADCA.CH1) && TIMER.CNT<step_time) {}
		
			if(current[i] == 0)
				current[i] = ADC_ResultCh_GetWord_Signed(&ADCA.CH1,ADC_OFFSET);
			else
				current[i] = (current[i] + ADC_ResultCh_GetWord_Signed(&ADCA.CH1,ADC_OFFSET)) >> 1;
		}

		steps_taken++;
		if(steps_taken >= steps_per_sample)
		{
			steps_taken = 0;
			i++;
		}
	}

	PORTE.OUTSET = PIN0_bm; //switch1
	//PORTE.OUTCLR = PIN0_bm; //switch1
	PORTE.OUTCLR = PIN1_bm; //switch0
	PORTE.OUTCLR = PIN2_bm; //switch2
	PORTE.OUTCLR = PIN3_bm; //switch3

	current_DAC = 2048;
	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,current_DAC,CH0);

	//start output to USB
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, LSV);
	for(j = 0; j < 15; j++)
	{
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, name[j]);
	}
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, settle>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, settle);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, start>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, start);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, stop>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, stop);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, slope>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, slope);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, sample_rate>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, sample_rate);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, curr_range);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, i>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, i);

	for(j = 0; j < i; j++)
	{
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, current[j]>>8);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, current[j]);
	}

	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0,LSV);

	return i;

}

int16_t CONSTVOLT_test (char* name, int16_t voltage, int16_t time) {

	uint16_t elapsed;
	int16_t current_DAC;

	//check limits
	if(voltage<-1600 || voltage>1600 || time<0 || time>9999)
	{
		lcdClear();
		lcdHome();
		lcdPrintData("outside limits",14);
		return -1;
	}

	current_DAC = (int16_t) (round(voltage*(4096.0/3300))+2048);  

	//init timer
	TIMER.CTRLA = TC_CLKSEL_DIV1024_gc;

	//change switches
	PORTE.OUTSET = PIN1_bm; //switch0
	PORTE.OUTSET = PIN2_bm; //switch2
	PORTE.OUTSET = PIN3_bm; //switch3
	PORTE.OUTCLR = PIN0_bm; //switch1

	//apply voltage
	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,current_DAC,CH0);

	
	//start timer
	TIMER.CNT = 0;

	elapsed = 0;
	while(elapsed < time) {

		//wait for 1 sec
		while(TIMER.CNT<1953) {}

		//reset timer
		TIMER.CNT = 0;

		//increment elapsed
		elapsed++;

	}
	
	
	PORTE.OUTSET = PIN0_bm; //switch1
	PORTE.OUTCLR = PIN1_bm; //switch0
	PORTE.OUTCLR = PIN2_bm; //switch2
	PORTE.OUTCLR = PIN3_bm; //switch3

	current_DAC = 2048;
	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,current_DAC,CH0);

	return elapsed;
}

int16_t CA_test (char* name, int16_t wait_time, int16_t step_voltage, int16_t step_width, int16_t quiet_time, int16_t sample_rate, uint8_t steps, uint8_t curr_range) {

	uint16_t waitTicks, stepTicks, quietTicks;
	uint16_t timer,offset;
	int16_t step_DAC;
	uint8_t i;
	uint16_t j,k;
	uint16_t length;

	char temp_string[16];

	waitTicks = (uint16_t) round(wait_time * 0.001 * sample_rate);
	stepTicks = (uint16_t) round(step_width * 0.001 * sample_rate);
	quietTicks = (uint16_t) round(quiet_time * 0.001 * sample_rate);

	length = steps*(waitTicks + stepTicks + quietTicks);

	

	if(length > CA_MAX_DATAPOINTS)
	{
		
		
		lcdClear();
		lcdHome();
		lcdPrintData("too many data points",14);


		return -1;
	}

	uint16_t results[length];

	step_DAC = (int16_t) (round(step_voltage*(4096.0/3300))+2048);  

	//init timer
	timer = (uint16_t) round(500000/sample_rate);
	TIMER.CTRLA = TC_CLKSEL_DIV4_gc;

	//change switches
	PORTE.OUTSET = PIN1_bm; //switch0
	PORTE.OUTSET = PIN2_bm; //switch2
	if(curr_range == RANGE_10UA)
		PORTE.OUTCLR = PIN3_bm; //switch3
	else
		PORTE.OUTSET = PIN3_bm; //switch3
	PORTE.OUTCLR = PIN0_bm; //switch1

	//apply voltage
	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
		DAC_Channel_Write(&DACB,2048,CH0);

	k = 0;
	for(i = 0; i < steps; i++) {
		for(j = 0; j < waitTicks; j++) {
			TIMER.CNT = 0;
			while(TIMER.CNT<timer) {
				ADC_Ch_Conversion_Start(&ADCA.CH1);
				while(!ADC_Ch_Conversion_Complete(&ADCA.CH1) && TIMER.CNT<timer) {}
					results[k] = ADC_ResultCh_GetWord_Signed(&ADCA.CH1,ADC_OFFSET);
			}
			k++;
			
		}
		while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
			DAC_Channel_Write(&DACB,step_DAC,CH0);
		for(j = 0; j < stepTicks; j++) {
			TIMER.CNT = 0;
			while(TIMER.CNT<timer) {
				ADC_Ch_Conversion_Start(&ADCA.CH1);
				while(!ADC_Ch_Conversion_Complete(&ADCA.CH1) && TIMER.CNT<timer) {}
					results[k] = ADC_ResultCh_GetWord_Signed(&ADCA.CH1,ADC_OFFSET);
			}
			k++;
		}
		while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
			DAC_Channel_Write(&DACB,2048,CH0);
		for(j = 0; j < quietTicks; j++) {
			TIMER.CNT = 0;
			while(TIMER.CNT<timer) {
				ADC_Ch_Conversion_Start(&ADCA.CH1);
				while(!ADC_Ch_Conversion_Complete(&ADCA.CH1) && TIMER.CNT<timer) {}
					results[k] = ADC_ResultCh_GetWord_Signed(&ADCA.CH1,ADC_OFFSET);
			}
			k++;
		}
	}
	
	
	PORTE.OUTSET = PIN0_bm; //switch1
	PORTE.OUTCLR = PIN1_bm; //switch0
	PORTE.OUTCLR = PIN2_bm; //switch2
	PORTE.OUTCLR = PIN3_bm; //switch3

	while (DAC_Channel_DataEmpty(&DACB, CH0) == false) {}
			DAC_Channel_Write(&DACB,2048,CH0);

	//start output to USB
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, CA);
	for(j = 0; j < 15; j++)
	{
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, name[j]);
	}
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, wait_time>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, wait_time);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, step_voltage>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, step_voltage);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, step_width>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, step_width);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, quiet_time>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, quiet_time);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, sample_rate>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, sample_rate);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, steps);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, curr_range);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, length>>8);
	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
	USART_PutChar(&USARTC0, length);


	for(j = 0; j < length; j++)
	{
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, results[j]>>8);
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, results[j]);
	}

	do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0,CA);


	return i;

}


ISR(USARTC0_RXC_vect)
{
	uint8_t i,j,type;
	USART_RXComplete(&USART_data);
	if(USART_RXBufferData_Available(&USART_data))
		type = USART_RXBuffer_GetByte(&USART_data);
	else
		type = 0;
	//recieve profiles
	if(type == 'u')
	{
		for(i = 0; i < PROFILES_LENGTH; i++)
		{
			for(j = 0; j < 15; j++)
			{
				do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
				profiles[i].name[j] = USART_RXBuffer_GetByte(&USART_data);
			}
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].type = USART_RXBuffer_GetByte(&USART_data);
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].op1 = USART_RXBuffer_GetByte(&USART_data)<<8;
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].op1 |= USART_RXBuffer_GetByte(&USART_data);
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].op2 = USART_RXBuffer_GetByte(&USART_data)<<8;
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].op2 |= USART_RXBuffer_GetByte(&USART_data);
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].op3 = USART_RXBuffer_GetByte(&USART_data)<<8;
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].op3 |= USART_RXBuffer_GetByte(&USART_data);
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].op4 = USART_RXBuffer_GetByte(&USART_data)<<8;
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].op4 |= USART_RXBuffer_GetByte(&USART_data);
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].op5 = USART_RXBuffer_GetByte(&USART_data)<<8;
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].op5 |= USART_RXBuffer_GetByte(&USART_data);
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].op6 = USART_RXBuffer_GetByte(&USART_data)<<8;
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].op6 |= USART_RXBuffer_GetByte(&USART_data);
			do{USART_RXComplete(&USART_data);} while(!USART_RXBufferData_Available(&USART_data));
			profiles[i].curr_range |= USART_RXBuffer_GetByte(&USART_data);
			//write to EEPROM
			eeprom_write_block((const void*)&(profiles[i]), (void*)&(profilesEE[i]), sizeof(profile));
		}
	}
	//send profiles
	else if(type == 'd');
	{
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, 'z');
		for(i = 0; i < PROFILES_LENGTH; i++)
		{
			for(j = 0; j < 15; j++)
			{
				do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
				USART_PutChar(&USARTC0, profiles[i].name[j]);
			}
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].type);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].op1>>8);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].op1);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].op2>>8);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].op2);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].op3>>8);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].op3);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].op4>>8);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].op4);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].op5>>8);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].op5);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].op6>>8);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].op6);
			do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
			USART_PutChar(&USARTC0, profiles[i].curr_range);
		}
		do{} while(!USART_IsTXDataRegisterEmpty(&USARTC0));
		USART_PutChar(&USARTC0, 'z');
	}
}


