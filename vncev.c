/* This program is a simple server to show events coming from the client */
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "rfb.h"
#include "default8x16.h"

char f[640*480];
char* keys[0x400];

int hex2number(unsigned char c)
{
   if(c>'f') return(-1);
   else if(c>'F')
     return(10+c-'a');
   else if(c>'9')
     return(10+c-'A');
   else
     return(c-'0');
}

void read_keys()
{
   int i,j,k;
   char buffer[1024];
   FILE* keysyms=fopen("keysym.h","r");

   memset(keys,0,0x400*sizeof(char*));
   
   if(!keysyms)
     return;
   
   while(!feof(keysyms)) {
      fgets(buffer,1024,keysyms);
      if(!strncmp(buffer,"#define XK_",strlen("#define XK_"))) {
	 for(i=strlen("#define XK_");buffer[i] && buffer[i]!=' '
	     && buffer[i]!='\t';i++);
	 if(buffer[i]==0) /* don't support wrapped lines */
	   continue;
	 buffer[i]=0;
	 for(i++;buffer[i] && buffer[i]!='0';i++);
	 if(buffer[i]==0 || buffer[i+1]!='x') continue;
	 for(j=0,i+=2;(k=hex2number(buffer[i]))>=0;i++)
	   j=j*16+k;
	 if(keys[j&0x3ff]) {
	    char* x=malloc(1+strlen(keys[j&0x3ff])+1+strlen(buffer+strlen("#define ")));
	    strcpy(x,keys[j&0x3ff]);
	    strcat(x,",");
	    strcat(x,buffer+strlen("#define "));
	    free(keys[j&0x3ff]);
	    keys[j&0x3ff]=x;
	 } else
	   keys[j&0x3ff] = strdup(buffer+strlen("#define "));
      }
      
   }
   fclose(keysyms);
}

int lineHeight=16,lineY=480-16;
void output(rfbScreenInfoPtr s,char* line)
{
   rfbDoCopyRect(s,0,0,640,480-lineHeight,0,-lineHeight);
   rfbDrawString(s,&default8x16Font,10,lineY,line,0x01);
   fprintf(stderr,"%s\n",line);
}

void dokey(Bool down,KeySym k,rfbClientPtr cl)
{
   char buffer[1024];
   
   sprintf(buffer,"%s: %s (0x%x)",
	   down?"down":"up",keys[k&0x3ff]?keys[k&0x3ff]:"",k);
   output(cl->screen,buffer);
}

void doptr(int buttonMask,int x,int y,rfbClientPtr cl)
{
   char buffer[1024];
   if(buttonMask) {
      sprintf(buffer,"Ptr: mouse button mask 0x%x at %d,%d",buttonMask,x,y);
      output(cl->screen,buffer);
   }
   
}

void newclient(rfbClientPtr cl)
{
   char buffer[1024];
   struct sockaddr_in addr;
   int len=sizeof(addr),ip;
   
   getpeername(cl->sock,&addr,&len);
   ip=ntohl(addr.sin_addr.s_addr);
   sprintf(buffer,"Client connected from ip %d.%d.%d.%d",
	   (ip>>24)&0xff,(ip>>16)&0xff,(ip>>8)&0xff,ip&0xff);
   output(cl->screen,buffer);
}

int main(int argc,char** argv)
{
   rfbScreenInfoPtr s=rfbGetScreen(&argc,argv,640,480,8,1,1);
   s->colourMap.is16=FALSE;
   s->colourMap.count=2;
   s->colourMap.data.bytes="\xd0\xd0\xd0\x30\x01\xe0";
   s->rfbServerFormat.trueColour=FALSE;
   s->frameBuffer=f;
   s->kbdAddEvent=dokey;
   s->ptrAddEvent=doptr;
   s->newClientHook=newclient;
   
   memset(f,0,640*480);
   read_keys();
   rfbInitServer(s);
   
   while(1) {
      rfbProcessEvents(s,999999);
   }
}
