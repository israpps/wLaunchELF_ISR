/*      
  _____     ___ ____ 
   ____|   |    ____|      PS2 Open Source Project
  |     ___|   |____       
  
---------------------------------------------------------------------------

    Copyright (C) 2008 - Neme & jimmikaelkael (www.psx-scene.com) 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the Free McBoot License.
    
	This program and any related documentation is provided "as is"
	WITHOUT ANY WARRANTIES, either express or implied, including, but not
 	limited to, implied warranties of fitness for a particular purpose. The
 	entire risk arising out of use or performance of the software remains
 	with you.
   	In no event shall the author be liable for any damages whatsoever
 	(including, without limitation, damages to your hardware or equipment,
 	environmental damage, loss of health, or any kind of pecuniary loss)
 	arising out of the use of or inability to use this software or
 	documentation, even if the author has been advised of the possibility of
 	such damages.

    You should have received a copy of the Free McBoot License along with
    this program; if not, please report at psx-scene :
    http://psx-scene.com/forums/freevast/

---------------------------------------------------------------------------

 The following Code is from uLaunchELF
 
---------------------------------------------------------------------------

*/

#include <tamtypes.h>
#include <kernel.h>

void TimerInit(void);
u64 Timer(void);
void TimerEnd(void);

//________________ From uLaunchELF ______________________
// Timer Define
#define T0_COUNT ((volatile unsigned long*)0x10000000) 
#define T0_MODE  ((volatile unsigned long*)0x10000010) 

static int TimerInterruptID = -1;
static u64 TimerInterruptCount = 0;

//--------------------------------------------------------------
// Timer Interrupt
int TimerInterrupt(int a)
{
	TimerInterruptCount++;
	*T0_MODE |= (1 << 11);
	return -1;
}
//--------------------------------------------------------------
// Timer Init
void TimerInit(void)
{
	*T0_MODE = 0x0000;
	TimerInterruptID = AddIntcHandler(9, TimerInterrupt, 0);
	EnableIntc(9);
	*T0_COUNT = 0;
	*T0_MODE = ( 2 << 0 )+( 1 << 7 )+( 1 << 9 );
	TimerInterruptCount = 0;
}
//--------------------------------------------------------------
// Timer Count
u64 Timer(void)
{
	u64 ticks = (*T0_COUNT + (TimerInterruptCount << 16)) * (1000.0F / ( 147456000.0F / 256.0F ));
	return ticks;
}
//--------------------------------------------------------------
// Timer End
void TimerEnd(void)
{
	*T0_MODE = 0x0000;
	if (TimerInterruptID >= 0){
		DisableIntc(9);
		RemoveIntcHandler(9, TimerInterruptID);
		TimerInterruptID = -1;
	}
	TimerInterruptCount = 0;
}
//--------------------------------------------------------------
