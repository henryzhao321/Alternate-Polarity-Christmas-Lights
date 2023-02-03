/**
  Generated Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This is the main file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  Description:
    This header file provides implementations for driver APIs for all modules selected in the GUI.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.81.8
        Device            :  PIC12F1612
        Driver Version    :  2.00
*/

/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
*/

#include "mcc_generated_files/mcc.h"

// Flash Memory Address - last row 
#define MEM_ADDR 0x7F0

// Cycles needed to debounce switch
#define SW_DEBOUNCE 30u

// Cycles needed to check solar dark signal
#define DARK_DEBOUNCE 100u

// Number of patterns 
#define MAX_PATTERN 5u

//50% of max PWM, so that there is space for 50% on the other PWM
//100% not possible as that would leave 0% on the other PWM
#define MAX_PWM 500u

//Stop it from going off altogether
#define MIN_PWM 50u

// When to turn off
#define OFF_PWM 0u

// Red and Green States
#define RED_MASK                0x0Fu
#define RED_OFF_RISING_TO_MAX   0x01u
#define RED_MIN_RISING_TO_MAX   0x02u
#define RED_MAX_FALLING_TO_MIN  0x03u
#define RED_MAX_FALLING_TO_OFF  0x04u
#define RED_MAX                 0x05u
#define RED_MIN                 0x06u
#define RED_OFF                 0x07u

#define GREEN_MASK              0xF0u
#define GREEN_OFF_RISING_TO_MAX 0x10u
#define GREEN_MIN_RISING_TO_MAX 0x20u
#define GREEN_MAX_FALLING_TO_MIN 0x30u
#define GREEN_MAX_FALLING_TO_OFF 0x40u
#define GREEN_MAX               0x50u
#define GREEN_MIN               0x60u
#define GREEN_OFF               0x70u

// In 13.1072 second cycles (Timer 6 limitation))
// Total 24 Hours = 6592
#define TIME_ON                 1098u // 14400 Seconds / 4 Hours
#define TIME_OFF                5494u // 72000 Seconds / 20 Hours

// Control time of day via 10 second timer1
volatile unsigned int timeofday = 0u;
void T6ISR(void);

void main(void)
{
    unsigned char switch_debounce;
    unsigned char dark_debounce;
    unsigned short pattern;    
    unsigned int step;
    unsigned int stepsize;
    unsigned int index;
    unsigned short rambuf[WRITE_FLASH_BLOCKSIZE];
    unsigned char state[16] =  {RED_OFF_RISING_TO_MAX   |   GREEN_OFF_RISING_TO_MAX,    // 0
                                RED_MAX_FALLING_TO_MIN  |   GREEN_MAX_FALLING_TO_MIN,
                                RED_MIN_RISING_TO_MAX   |   GREEN_MIN_RISING_TO_MAX,
                                RED_MAX_FALLING_TO_MIN  |   GREEN_MAX_FALLING_TO_MIN,
                                RED_MIN_RISING_TO_MAX   |   GREEN_MIN_RISING_TO_MAX,
                                // transition
                                RED_MAX                 |   GREEN_MAX_FALLING_TO_MIN,
                                RED_MAX_FALLING_TO_MIN  |   GREEN_MIN_RISING_TO_MAX,
                                RED_MIN_RISING_TO_MAX   |   GREEN_MAX_FALLING_TO_MIN,
                                RED_MAX_FALLING_TO_MIN  |   GREEN_MIN_RISING_TO_MAX,
                                // switch a roo
                                RED_MIN_RISING_TO_MAX   |   GREEN_MAX_FALLING_TO_OFF,
                                RED_MAX                 |   GREEN_OFF,
                                RED_MAX_FALLING_TO_OFF  |   GREEN_OFF_RISING_TO_MAX,
                                RED_OFF                 |   GREEN_MAX,
                                RED_OFF_RISING_TO_MAX   |   GREEN_MAX};             //13                        
                                
    
    // initialize the device
    SYSTEM_Initialize();
    INTERRUPT_GlobalInterruptEnable();
    INTERRUPT_PeripheralInterruptEnable();
    
    // Start up time of day timer
    TMR6_Stop();
    TMR6_Counter8BitSet(0u);
    TMR6_SetInterruptHandler(T6ISR);
    TMR6_Start();
    
    // Start Timer 4 half a period later than Timer 2
    // Timers are 8 bit so 255/2 = 127
    TMR2_Stop();
    TMR4_Stop();
    TMR2_Counter8BitSet(0u);
    TMR4_Counter8BitSet(127u);
    TMR2_Start();
    TMR4_Start();
    
    // Start off with RED and Green as off
    PWM1_LoadDutyValue(OFF_PWM);
    PWM2_LoadDutyValue(OFF_PWM);
    
    // Get the Pattern from Flash memory
    pattern = FLASH_ReadWord(MEM_ADDR);
    // check its a valid value
    if (pattern >= MAX_PATTERN)
    {
        pattern = 0u;
    }
    
    // prep some variables
    step = 0u;
    index = 0u;
    switch_debounce = 0u; 
    dark_debounce = 0u;
    
    // Forever loop
    while (1)
    {
        // Cycle time delay
        __delay_ms(25u);   
        
        // Check if Push Button held down for enough time to avoid glitches
        if (IO_RA4_GetValue() == 0u)
        {
            if (switch_debounce <= SW_DEBOUNCE)
            {
                switch_debounce++;
            }
        }
        else
        {
            switch_debounce = 0u;
        }
        
        // Next pattern - happens one time if button held down
        if (switch_debounce == SW_DEBOUNCE)
        {
            // Flash LEDs to provide feedback button was pressed
            PWM1_LoadDutyValue(OFF_PWM);
            PWM2_LoadDutyValue(OFF_PWM);
            __delay_ms(100u);
            PWM1_LoadDutyValue(MAX_PWM);
            PWM2_LoadDutyValue(MAX_PWM);
            __delay_ms(100u);
            PWM1_LoadDutyValue(OFF_PWM);
            PWM2_LoadDutyValue(OFF_PWM);
            __delay_ms(100u);
            
            // Skip to next pattern
            pattern++;
            if (pattern == MAX_PATTERN)
            {                
                pattern = 0u;
            } 
            
            // Save this pattern choice
            FLASH_WriteWord(MEM_ADDR, rambuf, pattern);
            
            // Reset time of day timer
            timeofday = 0u;
            
            // Reset these
            step = 0u;
            index = 0u;
        }
        
        // Check if it is dark for enough time to avoid glitches
        if (IO_RA5_GetValue() == 1u)
        {
            if (dark_debounce <= DARK_DEBOUNCE)
            {
                dark_debounce++;
            }
        }
        else
        {
            dark_debounce = 0u;
        }
        
        // Syncronisze as it's got dark - happens one time
        if (dark_debounce == DARK_DEBOUNCE)
        {            
            // Reset time of day timer
            timeofday = 0u;
            
            // Reset these
            step = 0u;
            index = 0u;
        }
        
        // Check which part of day this is
        if (timeofday < TIME_ON)
        {        
            // Do pattern
            if (pattern == 1u)
            {            
                PWM1_LoadDutyValue(MIN_PWM);
                PWM2_LoadDutyValue(MIN_PWM);
            }
            else if (pattern == 2u)
            {            
                PWM1_LoadDutyValue(MAX_PWM/2u);
                PWM2_LoadDutyValue(MAX_PWM/2u);
            }
            else if (pattern == 3u)
            {
                PWM1_LoadDutyValue(MAX_PWM);
                PWM2_LoadDutyValue(MAX_PWM);
            }        
            else if (pattern == 4u)
            {        
                // When getting close to bright or dark slow down by decreasing step size
                if ((step > 400u) || (step < 100u))
                {
                    stepsize = 1u;
                }
                else if ((step > 300u) || (step < 200u))
                {
                    stepsize = 2u;
                }
                else
                {
                    stepsize = 3u;
                }

                // Create a sawtooth up ramp that resets back to zero at the end 
                step = step + stepsize;
                if (step >= MAX_PWM)
                {
                    step = 0u;
                    // Move through the state machine
                    index++;
                    if (index >= 14u)
                    {
                        // Don't reset all the way back to start
                        index = 1u;
                    }
                }

                //TMR2 Based PWM on CCP1
                //Max is 1024 / 10 Bits
                //Low 2 Bits made from FOSC
                if ((state[index] & RED_MASK) == RED_OFF_RISING_TO_MAX)
                {
                    PWM1_LoadDutyValue(step);
                }
                else if ((state[index] & RED_MASK) == RED_MIN_RISING_TO_MAX)
                {
                    if (step < MIN_PWM)
                    {
                        PWM1_LoadDutyValue(MIN_PWM);
                    }
                    else
                    {
                        PWM1_LoadDutyValue(step);
                    }
                }
                else if ((state[index] & RED_MASK) == RED_MAX_FALLING_TO_MIN)
                {
                    if (step > (MAX_PWM - MIN_PWM))
                    {
                        PWM1_LoadDutyValue(MIN_PWM);
                    }
                    else
                    {
                        PWM1_LoadDutyValue(MAX_PWM - step);
                    }            
                }
                else if ((state[index] & RED_MASK) == RED_MAX_FALLING_TO_OFF)
                {
                    PWM1_LoadDutyValue(MAX_PWM - step);
                }
                else if ((state[index] & RED_MASK) == RED_MAX)
                {
                    PWM1_LoadDutyValue(MAX_PWM);
                }
                else if ((state[index] & RED_MASK) == RED_MIN)
                {
                    PWM1_LoadDutyValue(MIN_PWM);
                }
                else if ((state[index] & RED_MASK) == RED_OFF)
                {
                    PWM1_LoadDutyValue(OFF_PWM);
                }

                //TMR4 Based PWM on CCP2
                //Max is 1024 / 10 Bits
                //Low 2 Bits made from FOSC
                if ((state[index] & GREEN_MASK) == GREEN_OFF_RISING_TO_MAX)
                {
                    PWM2_LoadDutyValue(step);
                }
                else if ((state[index] & GREEN_MASK) == GREEN_MIN_RISING_TO_MAX)
                {
                    if (step < MIN_PWM)
                    {
                        PWM2_LoadDutyValue(MIN_PWM);
                    }
                    else
                    {
                        PWM2_LoadDutyValue(step);
                    }
                }
                else if ((state[index] & GREEN_MASK) == GREEN_MAX_FALLING_TO_MIN)
                {
                    if (step > (MAX_PWM - MIN_PWM))
                    {
                        PWM2_LoadDutyValue(MIN_PWM);
                    }
                    else
                    {
                        PWM2_LoadDutyValue(MAX_PWM - step);
                    }            
                }
                else if ((state[index] & GREEN_MASK) == GREEN_MAX_FALLING_TO_OFF)
                {
                    PWM2_LoadDutyValue(MAX_PWM - step);
                }
                else if ((state[index] & GREEN_MASK) == GREEN_MAX)
                {
                    PWM2_LoadDutyValue(MAX_PWM);
                }
                else if ((state[index] & GREEN_MASK) == GREEN_MIN)
                {
                    PWM2_LoadDutyValue(MIN_PWM);
                }
                else if ((state[index] & GREEN_MASK) == GREEN_OFF)
                {
                    PWM2_LoadDutyValue(OFF_PWM);
                }  
            }
            else
            {
                PWM1_LoadDutyValue(OFF_PWM);
                PWM2_LoadDutyValue(OFF_PWM);
            }
        }
        else
        {
            PWM1_LoadDutyValue(OFF_PWM);
            PWM2_LoadDutyValue(OFF_PWM);
        }
    }
}

void T6ISR(void)
{
    timeofday++;
    if (timeofday > ((TIME_ON-1u) + (TIME_OFF-1u)))
    {
        timeofday = 0u;
    }
}


/**
 End of File
*/