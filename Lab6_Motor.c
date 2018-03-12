/*
 * Lab6_Motor.c
 *
 * Class: CEC322
 * University: ERAU - Prescott
 * Authors: Sean Link
 * Date: 2/20/2018
 *
 * This lab demonstrates the use of the timers, UART, LED, OLED, ADC and GPIO.
 * Ultimately these peripherals where used to drive a stepper motor at
 * variable desired frequencies. The motor was able to rotate both clockwise
 * and counter clockwise, as well as follow the user's movements when the
 * potentiometer was turned.
 */
// Base includes with the timers Examples
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "grlib/grlib.h"
#include "drivers/cfal96x64x16.h"

// Necessary for blinking LED
#include "drivers/LED/LED.h"

// Necessary for lower case string conversion
#include <ctype.h>

// Necessary for blinking LED
#include "driverlib/gpio.h"

// Necessary for printing to the OLED
#include "drivers/OLED/displays.h"

// Necessary for the functionality of the UART
#include "drivers/UART/personalUART.h"

// Necessary for the stepper motor
#include "drivers/stepperMotor/stepperMotor.h"

// Necessary for timers
#include "drivers/Timers/personalTimers.h"
#include "driverlib/timer.h"

// Necessary for ADC
#include "drivers/ADC/personalADC.h"

//****************************************************************************
// Globals
//****************************************************************************
tContext sContext;
uint8_t menuSelection = '\0';
uint32_t revolutionsPerMin = 60;
const int coilStates[9] = {0xC, 0x4, 0x6, 0x2, 0x3, 0x1, 0x9, 0x8};
MotorState motorState = OFF;

//****************************************************************************
// Interrupts
//****************************************************************************
uint32_t timerCount = 0;
void oneSecondTimer(void) {
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    clearBlack();

    timerCount++;
    displayInfoOnBoard("%d", revolutionsPerMin, 25, DISPLAY_NUMBER);
    displayInfoOnBoard("%d", motorState, 50, DISPLAY_NUMBER);
}

void stepperMotorTimer(void) {
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    static uint32_t stepperMotorIndex = 0;

    switch(motorState) {
        case OFF :
            GPIOPinWrite(GPIO_PORTL_BASE, STEPPER_MOTOR_PINS, 0x0);
            break;
        case CLOCKWISE :
            stepperMotorIndex++;
            GPIOPinWrite(GPIO_PORTL_BASE, STEPPER_MOTOR_PINS, coilStates[stepperMotorIndex % TOTAL_COIL_STATES]);
            break;
        case COUNTER_CLOCKWISE :
            stepperMotorIndex--;
            GPIOPinWrite(GPIO_PORTL_BASE, STEPPER_MOTOR_PINS, coilStates[stepperMotorIndex % TOTAL_COIL_STATES]);
            break;
        case FOLLOW :
            static bool firstCycleInFollow = true;
            static int32_t stepsMoved;
            static uint32_t ADCHome[1];

            if (firstCycleInFollow ==  true) {
                stepsMoved = 0;

                // Reading the first ADC value into ADCHome and retriggering
                // the ADC
                readADCData(ADCHome);

                // Changing flag so the if statement does not execute again
                firstCycleInFollow = false;
            }
            else {
                // Reading current ADC value
                uint32_t currentADCValue[1];
                readADCData(currentADCValue);

                // Calculating Difference
                int32_t differenceInADCValues = currentADCValue[0] - ADCHome[0];

                // Calculating desired number of steps
                int32_t stepsDesired = STEPS_PER_REVOLUTION * differenceInADCValues / MAX_ADC_VALUE;

                if (stepsDesired > stepsMoved) {
                    stepperMotorIndex++;
                    GPIOPinWrite(GPIO_PORTL_BASE, STEPPER_MOTOR_PINS, coilStates[stepperMotorIndex % TOTAL_COIL_STATES]);
                    stepsMoved++;
                }
                else if (stepsDesired < stepsMoved) {
                    stepperMotorIndex--;
                    GPIOPinWrite(GPIO_PORTL_BASE, STEPPER_MOTOR_PINS, coilStates[stepperMotorIndex % TOTAL_COIL_STATES]);
                    stepsMoved--;
                }
            }
            break;
        // The default is redundant. However, working with a higher voltage supply, I thought it was best to error
        // on the side of caution that the device be turned off in the case of something unexpected.
        default :
            GPIOPinWrite(GPIO_PORTL_BASE, STEPPER_MOTOR_PINS, 0x0);
            break;
    }
}
void IntUART0(void) {
    uint32_t ui32Status;

    // Get the interrupt status.
    ui32Status = UARTIntStatus(UART0_BASE, true);

    // Clear the asserted interrupts.
    UARTIntClear(UART0_BASE, ui32Status);

    // Get the character from the UART buffer
    while(UARTCharsAvail(UART0_BASE)) {
        menuSelection = tolower((uint8_t)UARTCharGetNonBlocking(UART0_BASE));
    }
}

//****************************************************************************
// Main function
//****************************************************************************
int
main(void)
{

    // Enable lazy stacking for interrupt handlers.  This allows floating-point
    // instructions to be used within interrupt handlers, but at the expense of
    // extra stack usage.
    FPULazyStackingEnable();

    // Set the clocking to run directly from the crystal.
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                       SYSCTL_XTAL_16MHZ);

    // Initialize the display driver.
    CFAL96x64x16Init();

    // Disabling all Interrupts
    IntMasterDisable();

    // Initialize the graphics context and find the middle X coordinate.
    GrContextInit(&sContext, &g_sCFAL96x64x16);
    GrContextFontSet(&sContext, g_psFontFixed6x8);

    //*************************************************************************
    // Configuration
    //*************************************************************************
    setupUART();

    setupLED();

    setupStepperMotor();

    setupTimers();

    setupADC();

    //************************************************************************
    // Initializing Variables
    //************************************************************************
    uint32_t blinkingLightCounter = 0;
    bool exitProgram = false;
    //************************************************************************
    // starting functional calls and main while loop
    //************************************************************************

    // Displaying Splash Screen
    diplaySplashOnOLED();

    // Displaying UART Menu
    printMainMenu();

    //************************************************************************
    // Enable processor interrupts.
    //************************************************************************
    IntMasterEnable();
    while(exitProgram == false)
    {
        // Blinking the LED
        if (blinkingLightCounter %
                (FIVE_PERCENT_CYCLE_ON + NIENTYFIVE_PERCENT_CYCLE_OFF) <=
                FIVE_PERCENT_CYCLE_ON) {
            GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, GPIO_PIN_2);
        }
        else {
            GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, 0);
        }

        //********************************************************************
        // Functional calls dependent on menu selection
        //********************************************************************

        switch(menuSelection) {
            case 'm' :
                printMainMenu();
                break;
            case 's' :
                motorState = (motorState + 1) % TOTAL_STATES;
                break;
            case '+' :
                revolutionsPerMin += 10;
                TimerLoadSet(TIMER1_BASE, TIMER_A, SysCtlClockGet() / (revolutionsPerMin * STEPS_PER_REVOLUTION / 60 /*seconds*/));
                break;
            case '-' :
                revolutionsPerMin -= 10;
                TimerLoadSet(TIMER1_BASE, TIMER_A, SysCtlClockGet() / (revolutionsPerMin * STEPS_PER_REVOLUTION / 60 /*seconds*/));
                break;
            case 'q' :
                exitProgram = true;
                break;
        }
        menuSelection = '\0';
        blinkingLightCounter++;
    }
    clearBlack();
}


//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif
