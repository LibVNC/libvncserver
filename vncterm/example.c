#include "VNConsole.h"
#include "vga.h"

int main(int argc,char **argv)
{
  vncConsolePtr c=vcGetConsole(&argc,argv,80,25,&vgaFont,FALSE);
  char buffer[1024];
  int i,j,l;
  for(j=32;j<256;j+=16) {
    vcPrintF(c,"%02x: ",j);
    for(i=j;i<j+16;i++)
      vcPutChar(c,i);
    vcPutChar(c,'\n');
  }
  i=0;
  while(1) {
    vcPrintF(c,"%d :> ",i);
    vcGetString(c,buffer,1024);
    l=strlen(buffer)-1;
    while(l>=0 && buffer[l]=='\n')
      buffer[l]=0;
    if(!strcmp(buffer,"quit"))
      return(0);
    if(!strcmp(buffer,"s"))
      vcScroll(c,2);
    if(!strcmp(buffer,"S"))
      vcScroll(c,-2);
    i++;
  }
  return(0);
}
