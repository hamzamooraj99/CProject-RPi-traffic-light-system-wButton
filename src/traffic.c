// Traffic Light System for RPi
// Compile: gcc  -o  traffic traffic.c
// Run:     sudo ./traffic

//=======PACKAGES=======
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
//======================

// ======================TUNABLES======================
// The LEDs are connected to BCM_GPIO pins 10, 11 & 13
#define RLED 10 //Red Light LED
#define YLED 11 //Yellow Light LED
#define GLED 13 //Green Light LED

#define BUTTON 26

#define LDELAY 1000 //Delay for Lights being on
#define DDELAY 3000 //Delay between Go and Stop
#define BDELAY 200  //Delay for yellow light blinking
// ====================================================

#ifndef	TRUE
#define	TRUE	(1==1)
#define	FALSE	(1==2)
#endif

#define	PAGE_SIZE		(4*1024)
#define	BLOCK_SIZE		(4*1024)

#define	INPUT			 0
#define	OUTPUT			 1

#define	LOW			 0
#define	HIGH			 1


static volatile unsigned int gpiobase ;
static volatile uint32_t *gpio ;

//-----------------------------------------------------------------------------------------------------------------------------------------------------------
int failure (int fatal, const char *message, ...)
{
  va_list argp ;
  char buffer [1024] ;

  if (!fatal) //  && wiringPiReturnCodes)
    return -1 ;

  va_start (argp, message) ;
  vsnprintf (buffer, 1023, message, argp) ;
  va_end (argp) ;

  fprintf (stderr, "%s", buffer) ;
  exit (EXIT_FAILURE) ;

  return 0 ;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------DELAY FUNCTION---------------------------------------------------------------------
int delay (int howLong)
{
  struct timespec sleeper, dummy ;
  sleeper.tv_sec  = (time_t)(howLong / 1000) ;
  sleeper.tv_nsec = (long)(howLong % 1000) * 1000000 ;
  nanosleep (&sleeper, &dummy) ;
  return 0;
}



//----------------------------------------------------------------------RED ON/OFF FUNCTION------------------------------------------------------------------
int redOnOff (int theValue)
{
  if ((0b1010 & 0xFFFFFFC0 /* PI_GPIO_MASK */) == 0)	// bottom 64 pins belong to the Pi	
  {
    if (theValue == LOW)
    {
      *(gpio + 10) = 0b0000010000000000 ; // 1 shifted left by (10 & 31) in register 10 - Turns red light off
    }
    else
    {
      *(gpio + 7) = 0b0000010000000000 ; // 1 shifted left by (10 & 31) in register 7 - Turns red light on
    }
  }
  else
  {
    fprintf(stderr, "only supporting on-board pins\n");
    exit(1);
  }
  return 0;
}



//--------------------------------------------------------------------YELLOW ON/OFF FUNCTION-----------------------------------------------------------------
int yellowOnOff (int theValue)
{
  if ((0b1011 & 0xFFFFFFC0 /* PI_GPIO_MASK */) == 0)	// bottom 64 pins belong to the Pi	
  {
    if (theValue == LOW)
    {
      *(gpio + 10) = 0b0000100000000000 ; // 1 shifted left by (11 & 31) in register 10 - Turns yellow light off
    }
    else
    {
      *(gpio + 7) = 0b0000100000000000 ; // 1 shifted left by (11 & 31) in register 7 - Turns yellow light on
    }
  }
  else
  {
    fprintf(stderr, "only supporting on-board pins\n");
    exit(1);
  }
  return 0;
}



//---------------------------------------------------------------------GREEN ON/OFF FUNCTION----------------------------------------------------------------
int greenOnOff (int theValue)
{
  if ((0b1101 & 0xFFFFFFC0 /* PI_GPIO_MASK */) == 0)	// bottom 64 pins belong to the Pi	
  {
    if (theValue == LOW)
    {
      *(gpio + 10) = 0b00000010000000000000; // 1 shifted left by (13 & 31) in register 10 - Turns green light off
    }
    else
    {
      *(gpio + 7) = 0b00000010000000000000 ; // 1 shifted left by (13 & 31) in register 7 - Turns green light on
    }
  }
  else
  {
    fprintf(stderr, "only supporting on-board pins\n");
    exit(1);
  }
  return 0;
}



/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* MAIN */
/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int main (void)
{
  int redLED = RLED; int yellowLED = YLED; int greenLED = GLED;
  int button = BUTTON;
  int fSel, shift, clrOff, setOff;
  int   fd ;
  int   j, b;
  unsigned int lightDelay = LDELAY;
  unsigned int driveDelay = DDELAY;
  unsigned int blinkDelay = BDELAY;
  uint32_t res; /* testing only */

  printf ("Raspberry Pi Traffic Light System") ;

  if (geteuid () != 0)
    fprintf (stderr, "setup: Must be root. (Did you forget sudo?)\n") ;

  // --------------------------------------------------------------------------------------------------------------------
  // constants for RPi2
  gpiobase = 0x3F200000 ;

  // --------------------------------------------------------------------------------------------------------------------
  // memory mapping 
  // Open the master /dev/memory device

  if ((fd = open ("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC) ) < 0)
    return failure (FALSE, "setup: Unable to open /dev/mem: %s\n", strerror (errno)) ;

  // GPIO:
  gpio = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, gpiobase) ;
  if ((int32_t)gpio == -1)
    return failure (FALSE, "setup: mmap (GPIO) failed: %s\n", strerror (errno)) ;
  else
    fprintf(stderr, "NB: gpio = %x for gpiobase %x\n", gpio, gpiobase);


// ======================================================== Setting Mode using Literals ========================================================
  // setting the mode for redLED
  fprintf(stderr, "setting pin %d to %d for Red Light...\n", redLED, OUTPUT);
  // GPIO 10 lives in register 1 (GPFSEL)
  //GPIO 10 sits in slot 0 of register 1, thus shift by 0*3 (3 bits per pin)
  *(gpio + 1) = (*(gpio + 1) & ~(0b0111)) | (0b0001) ;  // Sets bits to one = output
  
  // setting the mode for yellowLED
  fprintf(stderr, "setting pin %d to %d for Yellow Light...\n", yellowLED, OUTPUT);
  //GPIO 11 lives in register 1 (GPFSEL)
  //GPIO 11 sits in slot 3 of register 1, thus shift by 1*3 (3 bits per pin)
  *(gpio + 1) = (*(gpio + 1) & ~(0b00111000)) | (0b00001000) ;  // Sets bits to one = output
  
  // setting the mode for greenLED
  fprintf(stderr, "setting pin %d to %d for Green Light...\n", greenLED, OUTPUT);
  //GPIO 13 lives in register 1 (GPFSEL)
  //GPIO 13 sits in slot 9 of register 1, thus shift by 3*3 (3 bits per pin)
  *(gpio + 1) = (*(gpio + 1) & ~(0b0000111000000000)) | (0b0000001000000000) ;  // Sets bits to one = output
  
  // setting the mode for the button
  //GPIO 26 lives in register 2 (GPFSEL)
  //GPIO 26 sits in slot 18 of register 2, thus shift by 6*3 (3 bits per pin)
  *(gpio + 2) = (*(gpio + 2) & ~(0b000111000000000000000000)) ;
// =============================================================================================================================================



// ======================================================== Traffic Light Loop ========================================================
// now, start a loop, listening to pinButton and if set pressed, set pinLED
 fprintf(stderr, "starting loop ...\n");
 for (j=0; j<1000; j++)
  {

    redOnOff(HIGH); 
    
    delay(driveDelay); delay(driveDelay);
    
    yellowOnOff(HIGH); delay(lightDelay);
    
    greenOnOff(HIGH); redOnOff(LOW); yellowOnOff(LOW);
    
    //delay(driveDelay);
    while ( (*(gpio + 13) & (0b00000100000000000000000000000000)) == 0)
      greenOnOff(HIGH);

    delay(lightDelay);
    greenOnOff(LOW);
    
    for (b=0; b<5; b++)
    {
      yellowOnOff(HIGH);
      delay(blinkDelay);
      yellowOnOff(LOW);
      delay(blinkDelay);
    }
    
    redOnOff(HIGH);
    
    delay(lightDelay);
    
  }
// ====================================================================================================================================



 // Clean up: write LOW
 clrOff = 10; 
 *(gpio + clrOff) = 1 << (redLED & 31) ;
 *(gpio + clrOff) = 1 << (yellowLED & 31) ;
 *(gpio + clrOff) = 1 << (greenLED & 31) ;

 fprintf(stderr, "end main.\n");
}
