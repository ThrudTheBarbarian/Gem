//
//  io.c
//  gemc
//
//  Created by ThrudTheBarbarian on 9/7/23.
//

#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


#include "gemio.h"

#define SOCKET_NAME "/tmp/gemd"

static int 			_gemfd	= -1;
static pthread_t	_io		= 0;

/*****************************************************************************\
|* Forward declarations
\*****************************************************************************/
static void * _socketIO(void *arg);

/*****************************************************************************\
|* Check to see if the connection has been made to the server
\*****************************************************************************/
int _gemIoIsConnected(void)
	{
	return (_gemfd > 0) ? 1 : 0;
	}

/*****************************************************************************\
|* Connect to the server
\*****************************************************************************/
int _gemIoConnect(void)
	{
	int ok = 1;
	
	if (_gemfd < 0)
		{
		if (pthread_create(&_io, NULL, _socketIO, NULL) != 0)
			{
			perror("Thread");
			ok = 0;
			}
		}
		
	return ok;
	}


/*****************************************************************************\
|* Thread function to handle socket communication
\*****************************************************************************/
static void * _socketIO(void *arg)
	{
	(void)arg;
	
	int ok = 0;
	_gemfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (_gemfd > 0)
		{
		/*********************************************************************\
		|* Zero everything for portability
		\*********************************************************************/
		struct sockaddr_un addr;
		memset(&addr, 0, sizeof(struct sockaddr_un));

		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, SOCKET_NAME, sizeof(addr.sun_path) - 1);

		/*********************************************************************\
		|* Connect to the gemd process
		\*********************************************************************/
		int ret = connect(_gemfd,
						  (const struct sockaddr *) &addr,
						  sizeof(struct sockaddr_un));
		if (ret == 0)
			ok = 1;
		else
			perror("Connect");
		}
	else
		perror("Socket create");
	
	/*************************************************************************\
	|* Handle reading from the socket, if everything went ok
	\*************************************************************************/
	if (ok)
		for(;;)
			{
			/*****************************************************************\
			|* Read the next message
			\*****************************************************************/
			
			/*****************************************************************\
			|* If we're in blocking mode, and the message matches the type,
			|* then despatch it synchronously and reset the blocking state
			\*****************************************************************/
			
			/*****************************************************************\
			|* Else despatch the message to the event-loop queue
			\*****************************************************************/
			
			}
		

	return NULL;
	}

/*****************************************************************************\
|* Write to the socket
\*****************************************************************************/
int _gemIoWrite(GemMsg *msg)
	{
	int size = msg->vec.length;

	/*************************************************************************\
	|* Create a new buffer with the correct checksum, embedded type and length
	\*************************************************************************/
	vec_data_t io;
	vec_init(&io);
	
	/*************************************************************************\
	|* Length is 2 (for length) + 2 (for type) + num bytes + 2 (for checksum)
	\*************************************************************************/
	uint16_t len = 2 + size + 2;
	vec_push(&io, len & 0xFF);
	vec_push(&io, len >> 8);

	/*************************************************************************\
	|* Do the type
	\*************************************************************************/
	uint16_t type = (uint16_t)msg->type;
	vec_push(&io, type & 0xFF);
	vec_push(&io, type >> 8);

	/*************************************************************************\
	|* Serialise the data
	\*************************************************************************/
	int16_t checksum = 0;
	for (int i=0; i<size; i+=2)
		{
		uint8_t data	= msg->vec.data[i];
		vec_push(&io, data);
		int16_t lo		= data;
		
		data			= msg->vec.data[i+1];
		vec_push(&io, data);
		int16_t hi	 	= data;
		
		checksum	   += hi << 8 | lo;
		}

	/*************************************************************************\
	|* Append the checksum
	\*************************************************************************/
	uint16_t cksum		= (uint16_t)checksum;
	vec_push(&io, cksum & 0xFF);
	vec_push(&io, cksum >> 8);
	
	/*************************************************************************\
	|* Write to the socket
	\*************************************************************************/
	int numSent = 0;
	while (numSent < io.length)
		numSent += write(_gemfd, io.data + numSent, io.length - numSent);
	
	return (numSent == io.length) ? 1 : 0;
	}

/*****************************************************************************\
|* Read from the socket
\*****************************************************************************/
int _gemIoRead(uint8_t *data, int numBytes)
	{
	return 0;
	}

