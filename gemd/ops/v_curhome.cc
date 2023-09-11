
#include "debug.h"
#include "vdi.h"

/*****************************************************************************\
|* Opcode 5.8: Move the cursor home.
|*
|* Original signature is: v_curhome(int16_t handle);
|*
\*****************************************************************************/
void VDI::v_curhome(int16_t handle)
	{
	if (handle == 0)
		{
		bool erased = _eraseCursor();
		_cursorX = _cursorY = 0;
		if (erased)
			_drawCursor();
		}
	else
		{
		WARN("Non-screen devices currently unsupported");
		}
	}