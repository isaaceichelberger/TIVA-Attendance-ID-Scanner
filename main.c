#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "inc/tm4c123gh6pm.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"

#define PORTB GPIO_PORTB_BASE
#define PORTA GPIO_PORTA_BASE
#define PORTE GPIO_PORTE_BASE
#define PORTC GPIO_PORTC_BASE
#define WIEGAND_WAIT_TIME 3000

volatile unsigned int data[100];
unsigned int counter = 0;
unsigned int studentCounter = 0;
unsigned char flag;
unsigned int wiegand_counter;
unsigned long cardCode;
volatile unsigned int studentCardNumbers[100];
volatile int cardnumberdataset[4] = {63448, 63323, 76236, 75219};
volatile char firstnames[4][10] = {"Isaac", "Eric", "Elizabeth", "Nathan"};
volatile char lastnames[4][15] = {"Eichelberger", "Iniguez", "Woods", "Hutchins"};
volatile int presentints[4] = {0, 0, 0, 0};

FILE *filePointer; // For Card Codes
FILE *filePresent;
FILE *fileAbsent;
FILE *fileRoster;

void interruptPC6(void);
void interruptPE0(void);
void printCardCodeFile(void);
void lcdprintmain(void);
void lcdaccessdenied(void);
void lcdprintwelcome(char name[]);
void InitializeScreen(void);
void compareNumber(int cardCode);
void printStudentFiles(int stopNumber);
void makeAbsentFile(void);

int main(void)
{

    SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_XTAL_16MHZ|SYSCTL_OSC_MAIN);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    // Pin Enabling for the LCD
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    GPIOPinConfigure(GPIO_PB1_U1TX);
    GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_1);

    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_1);

    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0);

    UARTConfigSetExpClk(UART1_BASE, SysCtlClockGet(), 9600,
        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 9600,
        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    GPIOPinTypeGPIOInput(PORTC, GPIO_PIN_6); // Data Input Pin - 1
    GPIOPinTypeGPIOInput(PORTE, GPIO_PIN_0); // Data Input Pin - 0
    // Pullups for data pins
    GPIOPadConfigSet(PORTC, GPIO_PIN_6, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOPadConfigSet(PORTE, GPIO_PIN_3, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    // Interrupts for Port E
    GPIOIntDisable(PORTE, GPIO_PIN_0);
    GPIOIntClear(PORTE, GPIO_PIN_0);
    GPIOIntTypeSet(PORTE, GPIO_PIN_0, GPIO_FALLING_EDGE);
    GPIOIntEnable(PORTE, GPIO_PIN_0);
    IntEnable(INT_GPIOE);
    // Interrupt Setup PC6
    GPIOIntDisable(PORTC, GPIO_PIN_6);
    GPIOIntClear(PORTC, GPIO_PIN_6);
    GPIOIntTypeSet(PORTC, GPIO_PIN_6, GPIO_FALLING_EDGE);
    GPIOIntEnable(PORTC, GPIO_PIN_6);
    IntEnable(INT_GPIOC);
    IntMasterEnable();
    wiegand_counter = WIEGAND_WAIT_TIME;
    filePointer = fopen("C:\\Embedded Project\\data\\cardnumbers.txt", "w+"); // Overwrite File
    fclose(filePointer);
    filePresent = fopen("C:\\Embedded Project\\data\\presentstudents.txt", "w+"); // Overwrite File
    fclose(filePresent);
    fileAbsent = fopen("C:\\Embedded Project\\data\\absentstudents.txt", "w+"); // Overwrite File
    fclose(fileAbsent);
    makeAbsentFile();
    InitializeScreen();
    lcdprintmain();

    while(1)
    {
        if (!flag)
        {
            if (--wiegand_counter == 0)
            {
                flag = 1;
            }
        }

        if (counter >= 35 && flag)
        {
            int i;
            if (counter == 35)
            {
                  // card code = bits 15 to 34
                  for (i=14; i<34; i++)
                  {
                     cardCode <<= 1;
                     cardCode |= data[i];
                  }
            }

            else
            {
                fprintf(filePointer, "%s\n", "Unable to decode.");
            }
            studentCardNumbers[studentCounter] = cardCode;
            studentCounter++;
            printCardCodeFile();
            compareNumber(cardCode);
            counter = 0;
            cardCode = 0;
            for (i = 0; i < 100; i++)
            {
               data[i] = 0;
            }
            lcdprintmain();
        }
    }
}


void interruptPC6(void)
{
    GPIOIntClear(PORTC, GPIO_PIN_6);
    GPIOIntDisable(PORTC, GPIO_PIN_6);
    data[counter] = 1;
    counter++;
    flag = 0;
    wiegand_counter = WIEGAND_WAIT_TIME;
    GPIOIntEnable(PORTC, GPIO_PIN_6);
}

void interruptPE0(void)
{
    GPIOIntClear(PORTE, GPIO_PIN_0);
    GPIOIntDisable(PORTE, GPIO_PIN_0);
    counter++;
    flag = 0;
    wiegand_counter = WIEGAND_WAIT_TIME;
    GPIOIntEnable(PORTE, GPIO_PIN_0);
}

void printCardCodeFile(void)
{
    int i;
    filePointer = fopen("C:\\Embedded Project\\data\\cardnumbers.txt", "a+");
    fflush(filePointer);
    for (i = studentCounter - 1; i < studentCounter; i++)
    {
        fprintf(filePointer, "%d\n", studentCardNumbers[i]);
    }
    fclose(filePointer);
}

void lcdprintmain(void)
{
    InitializeScreen();
    char message1[32] = "Scan ID                         ";
    int i = 0;
    for (i = 0; i < 32; i++)
    {
        UARTCharPut(UART1_BASE, message1[i]);
    }
}

void lcdaccessdenied(void)
{
    InitializeScreen();
    char message2[32] = "Access Denied                   ";
    int i = 0;
    for (i = 0; i < 32; i++)
    {
        UARTCharPut(UART1_BASE, message2[i]);
    }
    SysCtlDelay(10000000);
}

void lcdprintwelcome(char name[])
{
    InitializeScreen();
    char message3[40] = "Welcome to class  ";
    strcat(message3, name);
    strcat(message3, "!         ");
    int i = 0;
    for (i = 0; i < 32; i++)
    {
        UARTCharPut(UART1_BASE, message3[i]);
    }
    SysCtlDelay(10000000);

}

void InitializeScreen(void)
{
    int i;
    UARTCharPut(UART1_BASE, 254);
    UARTCharPut(UART1_BASE, 128);
    for(i = 0; i < 32; i++)
    {
        UARTCharPut(UART1_BASE, ' ');
    }
}

void compareNumber(int cardCode)
{
    int i = 0;
    int flag = 0;
    int stopNumber = -1;
    for (i = 0; i < 4; i++)
    {
        if (cardCode == cardnumberdataset[i])
        {
            flag = 1;
            stopNumber = i;
        }
    }

    if (flag && stopNumber != -1)
    {
        char name[10];
        snprintf(name, 10, "%s", firstnames[stopNumber]);
        lcdprintwelcome(name);
        printStudentFiles(stopNumber);
        makeAbsentFile();
    }
    else
    {
        lcdaccessdenied();
    }
}

void makeAbsentFile()
{
    fileAbsent = fopen("C:\\Embedded Project\\data\\absentstudents.txt", "w+");
    int i = 0;
    char firstname[10];
    char lastname[15];
    for (i = 0; i < 4; i++)
    {
        if (presentints[i] == 0)
        {
            snprintf(firstname, 10, "%s", firstnames[i]);
            snprintf(lastname, 15, "%s", lastnames[i]);
            fprintf(fileAbsent, "%s %s\n", firstname, lastname);
        }
    }
    fclose(fileAbsent);
}

void printStudentFiles(int stopNumber)
{
    char firstname[10];
    char lastname[15];
    filePresent = fopen("C:\\Embedded Project\\data\\presentstudents.txt", "a+");
    if (presentints[stopNumber] == 0)
    {
        snprintf(firstname, 10, "%s", firstnames[stopNumber]);
        snprintf(lastname, 15, "%s", lastnames[stopNumber]);
        fprintf(filePresent, "%s %s\n", firstname, lastname);
        presentints[stopNumber] = 1;
    }
    fclose(filePresent);
}

