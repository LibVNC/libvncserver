#include <rfb/rfb.h>

/* this is now the default */
#define USE_ATTRIBUTE_BUFFER

typedef struct vncConsole {
  /* width and height in cells (=characters) */
  int width, height;

  /* current position */
  int x,y;

  /* characters */
  char *screenBuffer;

#ifdef USE_ATTRIBUTE_BUFFER
  /* attributes: colours. If NULL, default to gray on black, else
     for each cell an unsigned char holds foreColour|(backColour<<4) */
  char *attributeBuffer;
#endif

  /* if this is set, the screen doesn't scroll. */
  rfbBool wrapBottomToTop;

  /* height and width of one character */
  int cWidth, cHeight;
  /* offset of characters */
  int xhot,yhot;

  /* colour */
  unsigned char foreColour,backColour;
  int8_t cx1,cy1,cx2,cy2;

  /* input buffer */
  char *inputBuffer;
  int inputCount;
  int inputSize;
  long selectTimeOut;
  rfbBool doEcho; /* if reading input, do output directly? */

  /* selection */
  char *selection;

  /* mouse */
  rfbBool wasRightButtonDown;
  rfbBool currentlyMarking;
  int markStart,markEnd;

  /* should text cursor be drawn? (an underscore at current position) */
  rfbBool cursorActive;
  rfbBool cursorIsDrawn;
  rfbBool dontDrawCursor; /* for example, while scrolling */

  rfbFontDataPtr font;
  rfbScreenInfoPtr screen;
} vncConsole, *vncConsolePtr;

#ifdef USE_ATTRIBUTE_BUFFER
vncConsolePtr vcGetConsole(int *argc,char **argv,
			   int width,int height,rfbFontDataPtr font,
			   rfbBool withAttributes);
#else
vncConsolePtr vcGetConsole(int argc,char **argv,
			   int width,int height,rfbFontDataPtr font);
#endif
void vcDrawCursor(vncConsolePtr c);
void vcHideCursor(vncConsolePtr c);
void vcCheckCoordinates(vncConsolePtr c);

void vcPutChar(vncConsolePtr c,unsigned char ch);
void vcPrint(vncConsolePtr c,unsigned char* str);
void vcPrintF(vncConsolePtr c,char* format,...);

void vcPutCharColour(vncConsolePtr c,unsigned char ch,
		     unsigned char foreColour,unsigned char backColour);
void vcPrintColour(vncConsolePtr c,unsigned char* str,
		   unsigned char foreColour,unsigned char backColour);
void vcPrintFColour(vncConsolePtr c,unsigned char foreColour,
		    unsigned char backColour,char* format,...);

char vcGetCh(vncConsolePtr c);
char vcGetChar(vncConsolePtr c); /* blocking */
char *vcGetString(vncConsolePtr c,char *buffer,int maxLen);

void vcKbdAddEventProc(rfbBool down,rfbKeySym keySym,rfbClientPtr cl);
void vcPtrAddEventProc(int buttonMask,int x,int y,rfbClientPtr cl);
void vcSetXCutTextProc(char* str,int len, struct _rfbClientRec* cl);

void vcToggleMarkCell(vncConsolePtr c,int pos);
void vcUnmark(vncConsolePtr c);

void vcProcessEvents(vncConsolePtr c);

/* before using this function, hide the cursor */
void vcScroll(vncConsolePtr c,int lineCount);
