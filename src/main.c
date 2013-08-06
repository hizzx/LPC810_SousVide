/****************************************************************************
 *   $Id:: lpc8xx_wkt.c 3635 2012-10-31 00:31:46Z usb00423                     $
 *   Project: NXP LPC8xx Wakeup timer example
 *
 *   Description:
 *     This file contains wakeup timer test modules.
 *
 ****************************************************************************
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * NXP Semiconductors assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. NXP Semiconductors
 * reserves the right to make changes in the software without
 * notification. NXP Semiconductors also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
 
 * Permission to use, copy, modify, and distribute this software and its 
 * documentation is hereby granted, under NXP Semiconductors' 
 * relevant copyright in the software, without fee, provided that it 
 * is used in conjunction with NXP Semiconductors microcontrollers. This 
 * copyright, permission, and disclaimer notice must appear in all copies of 
 * this code.
****************************************************************************/
#include "LPC8xx.h"			/* LPC8xx Peripheral Registers */
#include "lpc8xx_gpio.h"

#include "myuart.h"
#include "delay.h"
#include "onewire.h"
#include "ds18b20.h"

// Trace buffer
#include <cr_mtb_buffer.h>
__CR_MTB_BUFFER(32);


void delay(uint32_t d);
void delayMilliseconds (uint32_t d);
void blink(uint32_t n, uint32_t on_t, uint32_t off_t);
int32_t readTemperature (void);
void readOutTemperature (void);
void heatingElementOn(void);
void heatingElementOff(void);

#define SYSTICK_DELAY		(SystemCoreClock/100)


/* This define should be enabled if you want to      */
/* maintain an SWD/debug connection to the LPC810,   */
/* but it will prevent you from having access to the */
/* LED on the LPC810 Mini Board, which is on the     */
/* SWDIO pin (PIO0_2).                               */
//#define USE_SWD

#define USE_UART

// Interrupt channel to use with pin interrupt
#define CHANNEL (1)


#define HEATING_ELEMENT_EN
#define HEATING_ELEMENT_PORT (0)
#define HEATING_ELEMENT_PIN (0)

#define ULED1_EN
#define ULED1_PORT (0)
#define ULED1_PIN (2)

#define SW1_EN
#define SW1_PORT (0)
#define SW1_PIN (1)

#define OW_PORT (0)
#define OW_PIN (3)

#define BASE_TEMPERATURE (54000)

/** Heater PWM cycle period in SysTicks (10ms) */
#define HEATER_PWM_PERIOD (3000)

#define SEP ' '

volatile uint32_t timeTick = 0;
volatile uint32_t interruptFlags = 0;
volatile uint32_t swDownTime=0;

volatile uint32_t heaterDutyCycle = 0;

/** Set to 1 if element is on, 0 if off */
volatile uint32_t heaterStatus = 0;

void configurePins()
{
  /* Enable switch-matrix (SWM) clock */
  LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 7);


#if 0
  /* Pin Assign 8 bit Configuration */
  /* U0_TXD */
  /* U0_RXD */
  LPC_SWM->PINASSIGN0 = 0xffff0004UL;

  /* Pin Assign 1 bit Configuration */
  #if !defined(USE_SWD)
    /* Pin setup generated via Switch Matrix Tool
       ------------------------------------------------
       PIO0_5 = RESET
       PIO0_4 = U0_TXD
       PIO0_3 = GPIO            - Disables SWDCLK
       PIO0_2 = GPIO (User LED) - Disables SWDIO
       PIO0_1 = GPIO
       PIO0_0 = U0_RXD
       ------------------------------------------------
       NOTE: SWD is disabled to free GPIO pins!
       ------------------------------------------------ */
    LPC_SWM->PINENABLE0 = 0xffffffbfUL;
  #else
    /* Pin setup generated via Switch Matrix Tool
       ------------------------------------------------
       PIO0_5 = RESET
       PIO0_4 = U0_TXD
       PIO0_3 = SWDCLK
       PIO0_2 = SWDIO
       PIO0_1 = GPIO
       PIO0_0 = U0_RXD
       ------------------------------------------------
       NOTE: LED on PIO0_2 unavailable due to SWDIO!
       ------------------------------------------------ */
    LPC_SWM->PINENABLE0 = 0xffffffb3UL;
  #endif
#endif


    /*
    Aiming for:
    PIO0_5 = RESET
    PIO0_4 = U0_TXD
    PIO0_3 = GPIO            - Disables SWDCLK
    PIO0_2 = GPIO (User LED) - Disables SWDIO
    PIO0_1 = GPIO
    PIO0_0 = GPIO
	*/

    /* Pin Assign 8 bit Configuration */
    /* U0_TXD */
    LPC_SWM->PINASSIGN0 = 0xffffff04UL;

    /* Pin Assign 1 bit Configuration */
    /* RESET */
    LPC_SWM->PINENABLE0 = 0xffffffbfUL;


#ifdef ULED1_EN
    // Set LED driver pin as output and set to logic high
    GPIOSetDir(ULED1_PORT,ULED1_PIN,1);
    GPIOSetBitValue(ULED1_PORT,ULED1_PIN,1);

    // Set SW1 pin as input
    GPIOSetDir(SW1_PORT,SW1_PIN,0);
#endif


	GPIOSetDir(HEATING_ELEMENT_PORT,HEATING_ELEMENT_PIN, 1);



}

/*****************************************************************************
**   Main Function  main()
******************************************************************************/
int main (void)
{

	SystemCoreClockUpdate();

	SysTick_Config( SYSTICK_DELAY );



	//
	// Enable and configure GPIO
	//

	GPIOInit();

#if 0
	/* Enable AHB clock to the GPIO domain. */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<6);

	/* Peripheral reset control to GPIO and GPIO INT, a "1" bring it out of reset. */
	LPC_SYSCON->PRESETCTRL &= ~(0x1<<10);
	LPC_SYSCON->PRESETCTRL |= (0x1<<10);
#endif



	configurePins();


	//
	// Initialize UART and display hello message
	//
#ifdef USE_UART
	MyUARTInit(LPC_USART0, 115200);
	LPC_USART0->INTENSET = 0x01;	/* Enable UART interrupt */
	MyUARTSendStringZ (LPC_USART0, (uint8_t*)"LPC810_SousVide_0.1.1\r\n");
#endif




	//
	// Configure SW1 pin to trigger interrupt when pressed.
	//
	GPIOSetPinInterrupt( CHANNEL , /* channel */
		    				SW1_PORT,
		    				SW1_PIN,
		    				0, /* sense 0=edge*/
		    				0 /* polarity, 0=active low */
		    				);
	GPIOPinIntEnable( CHANNEL /* channel */,
		    		0 /* falling edge */ );

	// Configure one wire pin pull-up resistor
	// UM10601 §6.5.6, Table 55, p64
	// set MODE=0x02 (pull-up resistor enabled)
	// Pull-up too weak. Need external 4k7 R.
	//LPC_IOCON->PIO0_3 = 0x02 << 3;

	ow_init (OW_PORT,OW_PIN);


	// Initialize delay library (to calibrate short delay loop)
	delay_init();


	/*
	uint64_t rom_addr;
	for (i = 0; i < 100; i++) {
		rom_addr = ds18b20_rom_read();
		MyUARTPrintHex(LPC_USART0, rom_addr >> 32 );
		MyUARTPrintHex(LPC_USART0, (uint32_t)(rom_addr & 0x00000000ffffffff ));
		MyUARTSendStringZ (LPC_USART0, (uint8_t*)"<\r\n");


		MyUARTPrintDecimal(LPC_USART0, ds18b20_temperature_read() );
		MyUARTSendStringZ (LPC_USART0, (uint8_t*)"<\r\n");
		blink (1,500,500);
	}
	*/

	// Test heating element pin
	int i;
	/*
	for (i = 0; i < 100; i++) {
		heatingElementOn();
		delayMilliseconds(100);
		heatingElementOff();
		delayMilliseconds(100);
	}
	*/


	/*
	 * Get temperature set-point. This is set by pressing SW1 once for each degree C
	 * above 54°C. Ie one press (the minimum) is 55°C. A pause of more than 5 seconds
	 * signals that that user has finished setting the temperature. To confirm selection
	 * the LED will blink for each button press before entering control mode.
	 */
	int nButtonPress = 0;
	while ( (nButtonPress==0) || ((timeTick - swDownTime) < 500) ) {

		// Wait for interrupt. Two probable sources:
		// SysTick every 10ms and pin interrupt from SW1
		__WFI();

		// Was interrupt due to SW1?
		if (interruptFlags & 0x01) {
			blink(1,250,0);
			nButtonPress++;
			interruptFlags = 0;
		}

	}

	// Echo back the number of button press to the user
	blink (nButtonPress,500,500);

	// Delay for 5s before entering control mode to avoid confusion
	// with flashing LEDs.
	delayMilliseconds (5000);

	/*
	 * Enter control mode. If under temperature LED will blink slowly. If at set-point
	 * +/- 1°C LED will be solid on. If over temperature LED will blink fast. This
	 * mode can only be exited by reset/power cycle. At any time the user can press
	 * the UI button and the temperature will be readout by blinking the LED.
	 */
	int32_t setPointTemperature =  BASE_TEMPERATURE
			+ 1000*nButtonPress;
	int32_t currentTemperature;
	int32_t error, prevError=0;
	int32_t integral=0,derivative=0;
	int32_t dt;
	uint32_t prevTime = timeTick;
	uint32_t now;
	int32_t Kp=50, Ki=1, Kd=0;
	int32_t output;
	int32_t heaterDutyCycle = 0;

	while (1) {

		// Read temperature in 0.001°C units. Eg 45200 = 45.2°C.
		currentTemperature = readTemperature();


		// PID calculations
		error = setPointTemperature - currentTemperature;
		now = timeTick;
		dt = now - prevTime;
		integral += (error*dt)/1000;
		derivative = ((error - prevError)*10000)/dt;
		output = Kp*error + Ki*integral + Kd*derivative;
		prevError = error;
		prevTime = now;

		// So say for 10C error, output = 10000*Kp.

		heaterDutyCycle = output/1000;
		setHeaterDutyCycle(heaterDutyCycle);

		//heater_status  = ((int32_t)(timeTick % 10000) > output) ? 0 : 1;


		// Log temperature to serial port
		MyUARTPrintDecimal(LPC_USART0, timeTick );
		MyUARTSendByte (LPC_USART0, SEP);
		MyUARTPrintDecimal(LPC_USART0, currentTemperature );
		MyUARTSendByte (LPC_USART0, SEP);
		MyUARTPrintDecimal(LPC_USART0, setPointTemperature );

		MyUARTSendByte (LPC_USART0, SEP);
		MyUARTSendByte (LPC_USART0, SEP);
		MyUARTSendByte (LPC_USART0, SEP);

		MyUARTPrintDecimal(LPC_USART0, Kp*error );

		MyUARTSendByte (LPC_USART0, SEP);
		MyUARTPrintDecimal(LPC_USART0, Ki*integral );

		MyUARTSendByte (LPC_USART0, SEP);
		MyUARTPrintDecimal(LPC_USART0, Kd*derivative );

		MyUARTSendByte (LPC_USART0, SEP);
		MyUARTPrintDecimal(LPC_USART0, output );

		MyUARTSendByte (LPC_USART0, SEP);
		MyUARTPrintDecimal(LPC_USART0, heaterDutyCycle );

		MyUARTSendByte (LPC_USART0, '\r');
		MyUARTSendByte (LPC_USART0, '\n');

#ifdef BANG_BANG
		// Slow blink if under temperature
		if (currentTemperature < (setPointTemperature-500) ) {
			heatingElementOn();
			blink (1, 2000, 2000);
		}
		// Fast blink if over temperature
		else if (currentTemperature > (setPointTemperature+500) ) {
			heatingElementOff();
			blink (1, 250, 250);
		}
		// Solid on if at setpoint (blink with 0 off time)
		else {
			heatingElementOff();
			blink (1, 500, 0);
		}
#endif

		/*
		if ( heater_status ) {
			heatingElementOn();
		} else {
			heatingElementOff();
		}
		*/
		blink (1,500,500);

		delayMilliseconds(10000);

		// Did we receive a button press?
		if (interruptFlags & 0x01) {
			// Read out temperature
			delayMilliseconds(5000);
			readOutTemperature();
			delayMilliseconds(5000);
			interruptFlags = 0;
		}


	}

}

/**
 * Read out current temperature in °C by blinking the LED. LED blinked in two
 * sequences: first sequence for the temperature (°C) tens digit followed by
 * a delay of 2s. Then the second sequence for the least significant digit (the units).
 * Zero is represented by a one short blink of 100ms where as all other digits use 500ms
 * blinks. Temperatures of < 10°C or > 99°C not supported.
 * Example: 42°C: 4 x normal blinks, delay of 2s, 2 normal blinks.
 * Example: 60°C: 6 x normal blinks, delay of 2s, 1 short blink.
 */
void readOutTemperature (void) {
	int32_t currentTemperatureDeg = readTemperature()/1000;

	// Tens
	blink(currentTemperatureDeg/10,500,500);

	delayMilliseconds(2000);

	// Units
	uint32_t d = currentTemperatureDeg%10;
	if (d==0) {
		blink(1, 100, 500);
	} else {
		blink (d,500,500);
	}
}


/**
 * Blink LED 'n' times. Specify on and off time in milliseconds with on_t, off_t.
 */
void blink (uint32_t n, uint32_t on_t, uint32_t off_t) {
	while (n--) {
		GPIOSetBitValue(ULED1_PORT,ULED1_PIN, 0);
		delayMilliseconds (on_t);
		GPIOSetBitValue(ULED1_PORT,ULED1_PIN, 1);
		delayMilliseconds (off_t);
	}
}

/**
 * Read temperature from DS18B20
 * @Return in °C*1000 eg 27400 = 27.4°C
 */
int32_t readTemperature () {
	return ds18b20_temperature_read();
}

void heatingElementOn() {
	GPIOSetBitValue(HEATING_ELEMENT_PORT,HEATING_ELEMENT_PIN, 1);
}
void heatingElementOff() {
	GPIOSetBitValue(HEATING_ELEMENT_PORT,HEATING_ELEMENT_PIN, 0);
}


/**
 * Set the heating element duty cycle. Allowed values 0 - 1023.
 */
void setHeaterDutyCycle (int dutyCycle) {
	if (dutyCycle < 0) {
		heaterDutyCycle=0;
		return;
	}
	heaterDutyCycle = (dutyCycle * HEATER_PWM_PERIOD) / 1024;
}

/**
 * SysTick interrupt happens every 10 ms
 **/
void SysTick_Handler(void) {
	timeTick++;
	if ( (timeTick%HEATER_PWM_PERIOD) >= heaterDutyCycle) {
		heatingElementOff();
		heaterStatus = 0;
	} else {
		heatingElementOn();
		heaterStatus = 1;
	}
}

/**
 * Pin interrupt handler
 */
void PININT1_IRQHandler(void) {

	// Eliminate switch bounce
	if ( (timeTick-swDownTime) > 20) {
		//MyUARTSendStringZ (LPC_USART0, (uint8_t*)"^");
		interruptFlags |= 0x01;
		swDownTime = timeTick;
	} else {
		//MyUARTSendStringZ (LPC_USART0, (uint8_t*)"x");
	}

	// Clear the interrupt
	LPC_PIN_INT->IST = (1<<CHANNEL);
	return;
}
