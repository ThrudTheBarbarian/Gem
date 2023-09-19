#include <QColor>
#include <QPainter>
#include <QPainterPath>

#include "clientmsg.h"
#include "connectionmgr.h"
#include "fillfactory.h"
#include "gem.h"
#include "screen.h"
#include "vdi.h"
#include "workstation.h"

/*****************************************************************************\
|* Map RGB to something useful
\*****************************************************************************/
typedef struct Colour
	{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;

	bool operator ==(const Colour& other)
		{
		if ((other.r == r) && (other.g == g) && (other.b == b))
			return true;
		return false;
		}
	} Colour;

/*****************************************************************************\
|* Forward declarations
\*****************************************************************************/
static void _tile(QImage &src, QImage *dst);
static void _fill(int x, int y, int w, int h,
				  Colour c, Colour ** src, Colour ** dst);

/*****************************************************************************\
|* Defines and constants
\*****************************************************************************/
#define STACK_SIZE 16777216


#define PUSH(a,b,c,d)						\
	do {									\
		stack[SP++] = a;					\
		stack[SP++]	= b;					\
		stack[SP++] = c;					\
		stack[SP++] = d;					\
	} while (0)

#define POP(a,b,c,d)						\
		do {								\
		a = stack[j++];						\
		b = stack[j++];						\
		c = stack[j++];						\
		d = stack[j++];						\
	} while (0)

#define COPY_RGB(x, y)						\
	do {									\
		dst[y][x] = src[y][x];				\
		visited[y][x] = 1;					\
	} while (0)
//fprintf(stderr, "[%d,%d] ", x, y);	\

/*****************************************************************************\
|* Opcode 103: Flood fill an area
|*
|* Original signature is: v_contourfill(int16_t handle,
|*										int16_t x,
|*										int16_t y,
|*										int16_t colour);
|*
\*****************************************************************************/
void VDI::v_contourfill(qintptr handle, int16_t x, int16_t y, int16_t colour)
	{
	ConnectionMgr *cm = _screen->connectionManager();
	Workstation *ws   = cm->findWorkstationForHandle(handle);

	QPainter painter(_img);

	if (ws != nullptr)
		{
		/*********************************************************************\
		|* Get the dimensions of the destination image (the screen image)
		\*********************************************************************/
		int W = _img->width();
		int H = _img->height();
		int h;

		/*********************************************************************\
		|* Create an RGB32 image that is the same width as the destination
		|* image, but has the height of the source image (or solid colour)
		|* and tile it with the source image (or solid colour)
		\*********************************************************************/
		QImage *pattern = nullptr;

		int type = ws->fillType();
		switch (type)
			{
			case FIS_SOLID:
				{
				h = 2;
				pattern = new QImage(_screen->width(), h, QImage::Format_RGB32);
				QPainter subPainter(pattern);
				QColor bg = ws->colour(colour);
				subPainter.fillRect(QRect(0,0,_screen->width(), h), bg);
				break;
				}

			case FIS_USER:
				{
				QImage& from = FillFactory::sharedInstance().patternFor(type, 0);
				h = from.height();
				pattern = new QImage(_screen->width(), h, QImage::Format_RGB32);
				_tile(from, pattern);
				break;
				}
			case FIS_HATCH:
			case FIS_PATTERN:
				{
				int style = ws->fillStyle();
				QImage& from = FillFactory::sharedInstance().patternFor(type, style);
				h = from.height();
				pattern = new QImage(_screen->width(), h, QImage::Format_RGB32);
				_tile(from, pattern);
				break;
				}
			default:
				return;
			}
		fprintf(stderr, "Image fmt: %d\n", _img->format());

		/*********************************************************************\
		|* Create pointers to the scanline data that map 1:1 to the y co-ords
		|* of the image to fill (the screen), even if the destination is not
		|* of the same size
		\*********************************************************************/
		Colour ** src = new Colour * [H];
		Colour ** dst = new Colour * [H];

		for (int i=0; i<H; i++)
			{
			src[i] = (Colour *) pattern->scanLine(i%h);
			dst[i] = (Colour *) _img->scanLine(i);
			}

		/*********************************************************************\
		|* Now flood-fill everywhere with the colour at x,y
		\*********************************************************************/
		QColor toFill = ws->colour(colour);
		Colour c;
		c.r = toFill.red();
		c.g = toFill.green();
		c.b = toFill.blue();

		_fill(x, y, W, H, c, src, dst);

		/*********************************************************************\
		|* Tidy up
		\*********************************************************************/
		DELETE_ARRAY(src);
		DELETE_ARRAY(dst);
		DELETE(pattern);
		}
	}

/*****************************************************************************\
|* And from the socket interface
|*
|* Opcode 9:	Fill a polygon.			[type=0] [pxy=...]
\*****************************************************************************/
void VDI::v_contourfill(Workstation *ws, ClientMsg *cm)
	{
	const Payload &p	= cm->payload();
	int16_t x			= ntohs(p[0]);
	int16_t y			= ntohs(p[1]);
	int16_t colour		= ntohs(p[2]);

	v_contourfill(ws->handle(), x, y, colour);
	}

#pragma mark - Helper functions


/*****************************************************************************\
|* Flood-fill the image
\*****************************************************************************/
static void _fill(int x, int y, int w, int h,
				  Colour fillColour, Colour ** src, Colour ** dst)
	{
	int h2 = h-1;

	/*********************************************************************\
	|* Check we have work to do
	\*********************************************************************/
	Colour oldColour	= dst[y][x];		// Colour to look for
	if (fillColour == oldColour)
		return;

	/*********************************************************************\
	|* Allocate a working stack
	\*********************************************************************/
	int SP = 0;							// Initialise the stack pointer
	int *stack = new int[STACK_SIZE];
	if (stack == nullptr)
		{
		WARN("Could not allocate floodfill stack!");
		return;
		}

	/*********************************************************************\
	|* Allocate a visited array
	\*********************************************************************/
	uint8_t **visited = new uint8_t*[h];
	if (visited == nullptr)
		{
		WARN("Could not allocate floodfill visited top array!");
		DELETE_ARRAY(stack);
		return;
		}
	for (int i=0; i<h; i++)
		{
		visited[i] = new uint8_t[w];
		if (visited[i] == nullptr)
			{
			WARN("Could not allocated floodfill index array!");
			DELETE_ARRAY(stack);
			for (int j=0; j<i-1; j++)
				DELETE_ARRAY(visited[j]);
			DELETE_ARRAY(visited);
			return;
			}
		memset(visited[i], 0, w);
		}


	/*********************************************************************\
	|* Find the src colour, and trace left & right to see the extent
	\*********************************************************************/
	int x1 = x;
	while ((x < w) && (dst[y][x] == oldColour))
		x ++;

	int x2 = x;
	while ((x1 >= 0) && (dst[y][x1] == oldColour))
		x1 --;
	x1++;

	/*********************************************************************\
	|* Fill the dst with the src
	\*********************************************************************/
	for (x=x1; x<x2; x++)
		COPY_RGB(x,y);


	/*********************************************************************\
	|* Seed the stack
	\*********************************************************************/
	if (y > 0)
		PUSH(x1, x2, y-1, 2);

	if (y < h2)
		PUSH(x1, x2, y+1, 3);

	int t1, t2, q1, q2;
	for (int j=0; j<SP;)
		{
		POP(t1, t2, q1, q2);

		if (q2 == 2)
			{
			if (t1 < 0)		t1 = 0;
			if (t2 > w)		t2 = w;

			int lastMinY = q1+1;
			for (x = t1; x<t2; x++)
				{
				y = q1;
				if ((dst[y][x] == oldColour) && (!visited[y][x]))
					{
					for (int xx = x1; xx<x2; xx++)
						COPY_RGB(xx,y);

					for (--y; y >= 0; y--)
						{
						if ((!(dst[y][x] == oldColour)) || (visited[y][x]))
							break;
						COPY_RGB(x,y);
						}
					if ((y < lastMinY - 1) && (x > 0))
						PUSH(y+1, lastMinY,x-1,-2);

					else if ((y > lastMinY+1) && (x<w))
						PUSH(lastMinY+1, y, x, -3);
					}
				else if ((y > lastMinY+1) && (x<w))
					PUSH(lastMinY+1, y, x, -3);

				lastMinY = y;
				}

			if ((lastMinY < q1) && (x < w))
				PUSH(lastMinY+1, q1+1, x, -3);
			}

		else if (q2 == 3)
			{
			if (t1 < 0)		t1 = 0;
			if (t2 > w)		t2 = w;

			int lastMaxY = q1-1;

			for (x = t1; x < t2; x++)
				{
				y = q1;
				if ((dst[y][x] == oldColour) && (!visited[y][x]))
					{
					COPY_RGB(x,y);

					for (++y; y < h; y++)
						{
						if ((!(dst[y][x] == oldColour)) || (visited[y][x]))
							break;
						COPY_RGB(x,y);
						}

					if ((y < lastMaxY - 1) && (x < w))
						PUSH(y-1, lastMaxY, x, -3);

					else if ((y > lastMaxY + 1) && (x > 0))
						PUSH(lastMaxY+1, y, x-1, -2);
					}

				else if ((y < lastMaxY) && (x < w))
					PUSH(y-1, lastMaxY, x, -3);

				lastMaxY = y;
				}
			if ((q1 < lastMaxY) && (x < w))
				PUSH(q1, lastMaxY, x, -3);
			}

		else if (q2 == -2)
			{
			if (t1 < 0)		t1 = 0;
			if (t2 > h)		t2 = h;

			int lastMinX = q1 + 1;
			for (y = t1; y < t2; y++)
				{
				x = q1;
				if ((dst[y][x] == oldColour) && (!visited[y][x]))
					{
					COPY_RGB(x,y);

					for (--x; x > -1; x--)
						{
						if ((!(dst[y][x] == oldColour)) || (visited[y][x]))
							break;
						COPY_RGB(x,y);
						}

					if ((x < lastMinX - 1) && (y > 0))
						PUSH(x+1, lastMinX, y-1, 2);

					else if ((x > lastMinX + 1) && (y < h))
						PUSH(lastMinX+1, x, y, 3);
					}

				else if ((x > lastMinX + 1) && (y < h))
					PUSH(lastMinX+1, x, y, 3);

				lastMinX = x;
				}

			if ((lastMinX < q1) && (y < h))
				PUSH(lastMinX+1, q1+1, y, 3);
			}
		else
			{
			if (t1 < 0)		t1 = 0;
			if (t2 > h)		t2 = h;

			int lastMaxX = q1 - 1;
			for (y = t1; y < t2; y++)
				{
				x = q1;
				if ((dst[y][x] == oldColour) && (!visited[y][x]))
					{
					COPY_RGB(x,y);

					for (++x; x < w; x++)
						{
						if ((!(dst[y][x] == oldColour)) || (visited[y][x]))
							break;
						COPY_RGB(x,y);
						}

					if ((x < lastMaxX - 1) && (y < h))
						PUSH(x-1, lastMaxX, y, 3);

					else if ((x > lastMaxX + 1) && (y > 0))
						PUSH(lastMaxX+1, x, y-1, 2);
					}

				else if ((x < lastMaxX - 1) && (y < h))
					PUSH(x-1, lastMaxX, y, 3);

				lastMaxX = x;
				}

			if ((q1 < lastMaxX) && (y < h))
				PUSH(q1, lastMaxX, y, 3);
			}
		}


	DELETE_ARRAY(stack);
	for (int i=0; i<h; i++)
		DELETE_ARRAY(visited[i]);
	DELETE_ARRAY(visited);
	}

/*****************************************************************************\
|* Tile a source image into a destination image
\*****************************************************************************/
static void _tile(QImage& src, QImage *dst)
	{
	int dstW = dst->width();
	int dstH = dst->height();
	int srcW = src.width();
	int srcH = src.height();

	int rows = dstH / srcH;
	if (dstH != srcH)
		rows ++;

	int cols = dstW / srcW;
	if (dstW != srcW)
		cols ++;

	int yc = 0;
	QPainter p(dst);
	for (int y=0; y<rows; y++)
		{
		int xc = 0;
		for (int x=0; x<cols; x++)
			{
			p.drawImage(xc, yc, src);
			xc += srcW;
			}
		yc += srcH;
		}
	}
