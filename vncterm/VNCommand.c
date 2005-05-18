#ifdef __STRICT_ANSI__
#define _BSD_SOURCE
#define _POSIX_SOURCE
#endif
#include "VNConsole.h"
#include "vga.h"
#ifdef LIBVNCSERVER_HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef LIBVNCSERVER_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef LIBVNCSERVER_HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef LIBVNCSERVER_HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <errno.h>


int main(int argc, char **argv)
{
  rfbBool interactive=FALSE,sendOnlyWholeLines=TRUE;
  int serverArgc,programArg0;
  for(serverArgc=1;serverArgc<argc
	&& argv[serverArgc][0]=='-' && argv[serverArgc][1]!='-';serverArgc++)
    if(!strcmp(argv[serverArgc],"-interactive")) {
      interactive=TRUE;
      sendOnlyWholeLines=FALSE;

      rfbPurgeArguments(&argc,&serverArgc,1,argv);
    }
  programArg0=serverArgc;
  if(programArg0<argc && argv[programArg0][0]=='-' && argv[programArg0][1]=='-')
    programArg0++;
  argv[argc]=0;

  if(programArg0<argc) {
    int in[2],out[2],err[2],pid;
    if(pipe(in)<0 || pipe(out)<0 || pipe(err)<0) {
      rfbErr("Couldn't make pipes!");
      return(1);
    }

    pid=fork();
    if(!pid) {
      dup2(in[0],0);
      dup2(out[1],1);
      dup2(err[1],2);
      /*setbuf(stdin,NULL);*/
      execvp(argv[programArg0],argv+programArg0);
    }

    {
      char buffer[1024];
      fd_set fs,fs1/*,ifs,ifs1*/;
      struct timeval tv,tv1;
      int i,c=1,num_fds,max_fd=out[0],status;
      FILE *input_pipe;
      vncConsolePtr console=vcGetConsole(&serverArgc,argv,80,25,&vgaFont,FALSE);
      if(interactive)
	console->doEcho = FALSE;

      if(max_fd<err[0])
	max_fd=err[0];
      FD_ZERO(&fs);
      FD_SET(out[0],&fs);
      FD_SET(err[0],&fs);
      /*FD_SET(0,&fs);*/
      tv.tv_sec=0; tv.tv_usec=5000;

      input_pipe=fdopen(in[1],"w");
      setbuf(input_pipe,NULL);
      while(c || waitpid(pid,&status,WNOHANG)==0) {
	/* event loop */
	vcProcessEvents(console);

	/* get input */
	if(console->inputCount) {
	  if(sendOnlyWholeLines) {
	    for(i=0;i<console->inputCount;i++)
	      if(console->inputBuffer[i]=='\n') {
		i++;
		fwrite(console->inputBuffer,i,1,input_pipe);
		fflush(input_pipe);
		/* fwrite(console->inputBuffer,i,1,stderr); */
		if(console->inputCount>i)
		  memmove(console->inputBuffer,console->inputBuffer+i,console->inputCount-i);
		console->inputCount-=i;
		i=0;
	      }
	  } else {
	    fwrite(console->inputBuffer,console->inputCount,1,input_pipe);
	    fflush(input_pipe);
	    /* fwrite(console->inputBuffer,console->inputCount,1,stderr); */
	    console->inputCount=0;
	  }
	}
	/* process output */
	fs1=fs; tv1=tv;
	num_fds=select(max_fd+1,&fs1,NULL,NULL,&tv1);
	if(num_fds>0) {
	  /*
	  if(FD_ISSET(0,&fs1)) {
	    ch=getchar();
	    fputc(ch,f);
	  }
	  */
	  if(FD_ISSET(out[0],&fs1)) {
	    c=read(out[0],buffer,1024);
	    for(i=0;i<c;i++)
	      vcPutChar(console,buffer[i]);
	  }
	  if(FD_ISSET(err[0],&fs1)) {
	    c=read(err[0],buffer,1024);
	    for(i=0;i<c;i++)
	      vcPutChar(console,buffer[i]);
	  }		
	} else
	  c=0;
      }
    }
  }
  rfbLog("exit\n");
  return(0);
}
