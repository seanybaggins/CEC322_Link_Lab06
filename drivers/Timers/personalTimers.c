/*
 * personalTimers.c
 *
 *  Created on: Mar 3, 2018
 *      Author: Sean
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "driverlib/timer.h"
#include "personalTimers.h"

void setupTimers(void) {
    // Turning on the timers
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);

    // Waiting for the peripherals to be fully turned on
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0) ||
            !SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER1));

    // Disabling the timer functionalities to allow for configuration
    TimerDisable(TIMER0_BASE, TIMER_A);
    TimerDisable(TIMER1_BASE, TIMER_A);

    // Configuring the timer for periodic usage
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC);

    //************************************************************************
    // Specifying how often the periodic timer will trigger
    //************************************************************************
    // First timer is to trigger every second
    TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet());

    // Second Timer is to trigger at rate that will allow the stepper motor
    // to rotate at 60 rev/min
    TimerLoadSet(TIMER1_BASE, TIMER_A, SysCtlClockGet()/96);

    //************************************************************************
    // Interrupts
    //************************************************************************

    // Creating existence of interrupts
    IntEnable(INT_TIMER0A);
    IntEnable(INT_TIMER1A);

    // Specifying when the timer interrupts trigger
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);

    // Registering the interrupt in the interrupt verctor table (IVT)
    TimerIntRegister(TIMER0_BASE, TIMER_A, oneSecondTimer);
    TimerIntRegister(TIMER1_BASE, TIMER_A, stepperMotorTimer);

    //************************************************************************

    //Enabling the Timer functionality now that configuration is finished.
    TimerEnable(TIMER0_BASE, TIMER_A);
    TimerEnable(TIMER1_BASE, TIMER_A);
}
