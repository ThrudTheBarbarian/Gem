//
//  vs_color.c
//  gemc
//
//  Created by ThrudTheBarbarian on 9/13/23.
//

#include <stdio.h>

#include "gemio.h"
#include "gemmsg.h"
#include "macros.h"

/*****************************************************************************\
|*   14  : Set the colour of a pen (0..255)
\*****************************************************************************/
void vs_color(int16_t handle, int16_t idx, int16_t* rgb)
	{
	/*************************************************************************\
	|* Check to see if we're connected
	\*************************************************************************/
	if (!_gemIoIsConnected())
		if (!_gemIoConnect())
			return;
	
	/*************************************************************************\
	|* Construct and send the message
	\*************************************************************************/
	GemMsg msg;
	_gemMsgInit(&msg, MSG_VS_COLOR);
	_gemMsgAppend(&msg, &idx, 1);
	_gemMsgAppend(&msg, rgb, 3);
	_gemIoWrite(&msg);
			
	/*************************************************************************\
	|* Clear the message allocations
	\*************************************************************************/
	_gemMsgDestroy(&msg);
	}
