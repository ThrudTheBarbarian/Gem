#include "debug.h"
#include "screen.h"
#include "vdi.h"

/*****************************************************************************\
|* Opcode 5.6: Move the cursor right if possible.
|*
|* Original signature is: v_curright(int16_t handle);
|*
\*****************************************************************************/
void VDI::v_curright(int16_t handle)
	{
	if (handle == 0)
		{
		int lastCol = _screen->width() / _charWidth;
		if (_cursorX < lastCol - 1)
			{
			bool erased = _eraseCursor();
			_cursorX ++;
			if (erased)
				_drawCursor();
			}
		}
	else
		{
		WARN("Non-screen devices currently unsupported");
		}
	}

/*****************************************************************************\
|* And from the socket interface...
\*****************************************************************************/
void VDI::v_curright(Workstation *)
	{
	v_curright((int16_t)0);
	}
