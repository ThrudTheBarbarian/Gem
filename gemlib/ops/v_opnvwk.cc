#include "clientmsg.h"
#include "vdi.h"

/*****************************************************************************\
|* C interface
\*****************************************************************************/
extern "C" void v_opnvwk(int16_t *workIn, int16_t *handle, int16_t *workOut)
	{
	VDI::sharedInstance().v_opnvwk(workIn, handle, workOut);
	}

/*****************************************************************************\
|* Class declaration
\*****************************************************************************/
void VDI::v_opnvwk(int16_t *workIn, int16_t *handle, int16_t *workOut)
	{
	if (_io == nullptr)
		{
		if (!_connectToGemDaemon())
			{
			if (handle != NULL)
				*handle = 0;
			if (workOut != NULL)
				memset(workOut, 0, 56*sizeof(int16_t));
			}
		}
   else
		{
		ClientMsg msg(ClientMsg::TYPE_V_OPNVWK);
		msg.append(workIn, 16);

		_sendBlockingMessage(msg);
		}
	}
