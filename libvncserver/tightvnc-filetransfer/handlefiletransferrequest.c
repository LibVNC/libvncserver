/*
 * Copyright (c) 2005 Novell, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail,
 * you may find current contact information at www.novell.com 
 *
 * Author		: Rohit Kumar
 * Email ID	: rokumar@novell.com
 * Date		: 14th July 2005
 */
 
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

#include <rfb/rfb.h>
#include "rfbtightproto.h"
#include "filetransfermsg.h"
#include "handlefiletransferrequest.h"


pthread_mutex_t fileDownloadMutex = PTHREAD_MUTEX_INITIALIZER;

rfbBool fileTransferEnabled = TRUE;
rfbBool fileTransferInitted = FALSE;
char ftproot[PATH_MAX];


/******************************************************************************
 * File Transfer Init methods. These methods are called for initializating 
 * File Transfer and setting ftproot.
 ******************************************************************************/
 
void InitFileTransfer();
int SetFtpRoot(char* path);
char* GetHomeDir(uid_t uid);
void FreeHomeDir(char *homedir);

/*
 * InitFileTransfer method is called before parsing the command-line options 
 * for Xvnc. This sets the ftproot to the Home dir of the user running the Xvnc
 * server. In case of error ftproot is set to '\0' char.
 */
 
void
InitFileTransfer()
{
	char* userHome = NULL;
	uid_t uid = geteuid();

	if(fileTransferInitted)
		return;

	rfbLog("tightvnc-filetransfer/InitFileTransfer\n");
	
	memset(ftproot, 0, sizeof(ftproot));
	
	userHome = GetHomeDir(uid);

	if((userHome != NULL) && (strlen(userHome) != 0)) {
		SetFtpRoot(userHome);
		FreeHomeDir(userHome);
	}
	
	fileTransferEnabled = TRUE;
	fileTransferInitted = TRUE;
}

#ifndef __GNUC__
#define __FUNCTION__ "unknown"
#endif

/*
 *  This method is called from InitFileTransfer method and
 *  if the command line option for ftproot is provided.
 */
int
SetFtpRoot(char* path)
{
	struct stat stat_buf;
	DIR* dir = NULL;

	rfbLog("tightvnc-filetransfer/SetFtpRoot\n");
	
	if((path == NULL) || (strlen(path) == 0) || (strlen(path) > (PATH_MAX - 1))) {
		rfbLog("File [%s]: Method [%s]: parameter passed is improper, ftproot"
				" not changed\n", __FILE__, __FUNCTION__);
		return FALSE;
	}

	if(stat(path, &stat_buf) < 0) {
		rfbLog("File [%s]: Method [%s]: Reading stat for file %s failed\n", 
				__FILE__, __FUNCTION__, path);
		return FALSE;
	}

	if(S_ISDIR(stat_buf.st_mode) == 0) {
		rfbLog("File [%s]: Method [%s]: path specified is not a directory\n",
				__FILE__, __FUNCTION__);
		return FALSE;		
	}

	if((dir = opendir(path)) == NULL) {
		rfbLog("File [%s]: Method [%s]: Not able to open the directory\n",
				__FILE__, __FUNCTION__);
		return FALSE;			
	}
	else {
		closedir(dir);
		dir = NULL;
	}
	
	
	memset(ftproot, 0, PATH_MAX);
	if(path[strlen(path)-1] == '/') {
		memcpy(ftproot, path, strlen(path)-1);	
	}
	else	
		memcpy(ftproot, path, strlen(path));	

	
	return TRUE;
}


/*
 * Get the home directory for the user name
 * param: username - name of the user for whom the home directory is required.
 * returns: returns the home directory for the user, or null in case the entry 
 * is not found or any error. The returned string must be freed by calling the 
 * freehomedir function.
*/
char* 
GetHomeDir(uid_t uid)
{
	struct passwd *pwEnt = NULL;
	char *homedir = NULL;

	pwEnt = getpwuid (uid);
	if (pwEnt == NULL)
		return NULL;

	if(pwEnt->pw_dir != NULL) {
		homedir = strdup (pwEnt->pw_dir);
	}

	return homedir;
}


/*
 * Free the home directory allocated by a previous call to retrieve the home 
 * directory. param: homedir - the string returned by a previous call to 
 * retrieve home directory for a user.
 */
void 
FreeHomeDir(char *homedir)
{
    free (homedir);
}


/******************************************************************************
 * General methods.
 ******************************************************************************/
 
/*
 * When the console sends the File Transfer Request, it sends the file path with
 * ftproot as "/". So on Agent, to get the absolute file path we need to prepend
 * the ftproot to it.
 */
char*
ConvertPath(char* path)
{
	char p[PATH_MAX];
	memset(p, 0, PATH_MAX);
	
	if( (path == NULL) ||
		(strlen(path) == 0) ||
		(strlen(path)+strlen(ftproot) > PATH_MAX - 1) ) {

		rfbLog("File [%s]: Method [%s]: cannot create path for file transfer\n",
				__FILE__, __FUNCTION__);
		return NULL;
	}

	memcpy(p, path, strlen(path));
	memset(path, 0, PATH_MAX);
	sprintf(path, "%s%s", ftproot, p);

	return path;
}


void
EnableFileTransfer(rfbBool enable)
{
	fileTransferEnabled = enable;
}


rfbBool 
IsFileTransferEnabled()
{
	return fileTransferEnabled;
}


char*
GetFtpRoot()
{
	return ftproot;
}


/******************************************************************************
 * Methods to Handle File List Request.
 ******************************************************************************/
 
/*
 *  HandleFileListRequest method is called when the server receives 
 *  FileListRequest. In case of success a file list is sent to the client.
 *  For File List Request there is no failure reason sent.So here in case of any
 *  "unexpected" error no information will be sent. As these conditions should 
 *  never come. Lets hope it never arrives :)
 *  In case of dir open failure an empty list will be sent, just the header of 
 *  the message filled up. So on console you will get an Empty listing. 
 */
void
HandleFileListRequest(rfbClientPtr cl, rfbTightClientRec* data)
{
	rfbClientToServerTightMsg msg;
	int n = 0;
	char path[PATH_MAX]; /* PATH_MAX has the value 4096 and is defined in limits.h */
	FileTransferMsg fileListMsg;

	memset(&msg, 0, sizeof(rfbClientToServerTightMsg));
	memset(path, 0, PATH_MAX);
	memset(&fileListMsg, 0, sizeof(FileTransferMsg));

	if(cl == NULL) {
		rfbLog("File [%s]: Method [%s]: Unexpected error: rfbClientPtr is null\n", 
				__FILE__, __FUNCTION__);
		return;
	}

	if((n = rfbReadExact(cl, ((char *)&msg)+1, sz_rfbFileListRequestMsg-1)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Socket error while reading dir name"
					" length\n", __FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	    return;
	}
	
	msg.flr.dirNameSize = Swap16IfLE(msg.flr.dirNameSize);
	if ((msg.flr.dirNameSize == 0) ||
		(msg.flr.dirNameSize > (PATH_MAX - 1))) {
		
		rfbLog("File [%s]: Method [%s]: Unexpected error:: path length is "
				"greater that PATH_MAX\n", __FILE__, __FUNCTION__);

		return;		
	}
	
	if((n = rfbReadExact(cl, path, msg.flr.dirNameSize)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Socket error while reading dir name\n", 
							__FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	    return;
	}

	if(ConvertPath(path) == NULL) {

		/* The execution should never reach here */
    	rfbLog("File [%s]: Method [%s]: Unexpected error: path is NULL", 
    			__FILE__, __FUNCTION__);
    	return;
	}
	
    fileListMsg = GetFileListResponseMsg(path, (char) (msg.flr.flags));

    if((fileListMsg.data == NULL) || (fileListMsg.length == 0)) {

    	rfbLog("File [%s]: Method [%s]: Unexpected error:: Data to be sent is "
    		"of Zero length\n",	__FILE__, __FUNCTION__);
    	return;
	}	

    rfbWriteExact(cl, fileListMsg.data, fileListMsg.length); 

    FreeFileTransferMsg(fileListMsg);
}


/******************************************************************************
 * Methods to Handle File Download Request.
 ******************************************************************************/

void HandleFileDownloadLengthError(rfbClientPtr cl, short fNameSize);
void SendFileDownloadLengthErrMsg(rfbClientPtr cl);
void HandleFileDownload(rfbClientPtr cl, rfbTightClientPtr data);
#ifdef TODO
void HandleFileDownloadRequest(rfbClientPtr cl);
void SendFileDownloadErrMsg(rfbClientPtr cl);
void* RunFileDownloadThread(void* client);
#endif

/*
 * HandleFileDownloadRequest method is called when the server receives 
 * rfbFileDownload request message.
 */
void
HandleFileDownloadRequest(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
	int n = 0;
	char path[PATH_MAX]; /* PATH_MAX has the value 4096 and is defined in limits.h */
	rfbClientToServerTightMsg msg;

 	memset(path, 0, sizeof(path));
	memset(&msg, 0, sizeof(rfbClientToServerTightMsg));
	
	if(cl == NULL) {
		
		rfbLog("File [%s]: Method [%s]: Unexpected error:: rfbClientPtr is null\n",
				__FILE__, __FUNCTION__);
		return;
	}

	if((n = rfbReadExact(cl, ((char *)&msg)+1, sz_rfbFileDownloadRequestMsg-1)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading dir name length\n",
					__FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	    return;
	}

	msg.fdr.fNameSize = Swap16IfLE(msg.fdr.fNameSize);
	msg.fdr.position = Swap16IfLE(msg.fdr.position);

	if ((msg.fdr.fNameSize == 0) ||
		(msg.fdr.fNameSize > (PATH_MAX - 1))) {
		
		rfbLog("File [%s]: Method [%s]: Error: path length is greater than"
				" PATH_MAX\n", __FILE__, __FUNCTION__);
		
		HandleFileDownloadLengthError(cl, msg.fdr.fNameSize);
		return;
	}

	if((n = rfbReadExact(cl, rtcp->rcft.rcfd.fName, msg.fdr.fNameSize)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading dir name length\n",
							__FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	    return;
	}
	rtcp->rcft.rcfd.fName[msg.fdr.fNameSize] = '\0';

	if(ConvertPath(rtcp->rcft.rcfd.fName) == NULL) {

    	rfbLog("File [%s]: Method [%s]: Unexpected error: path is NULL",
    			__FILE__, __FUNCTION__);

		 
		 /* This condition can come only if the file path is greater than 
		    PATH_MAX. So sending file path length error msg back to client. 
		 */

    	SendFileDownloadLengthErrMsg(cl);
	return;
	}

	HandleFileDownload(cl, rtcp);

}


void
HandleFileDownloadLengthError(rfbClientPtr cl, short fNameSize)
{
	char *path = NULL;
	int n = 0;
	
	if((path = (char*) calloc(fNameSize, sizeof(char))) == NULL) {
		rfbLog("File [%s]: Method [%s]: Fatal Error: Alloc failed\n", 
				__FILE__, __FUNCTION__);
		return;
	}
	if((n = rfbReadExact(cl, path, fNameSize)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading dir name\n", 
							__FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);

	    if(path != NULL) {
			free(path);
			path = NULL;
		}
	    
	    return;
	}

    if(path != NULL) {
		free(path);
		path = NULL;
	}
    
	SendFileDownloadLengthErrMsg(cl);
}


void
SendFileDownloadLengthErrMsg(rfbClientPtr cl)
{
	FileTransferMsg fileDownloadErrMsg;
	
	memset(&fileDownloadErrMsg, 0 , sizeof(FileTransferMsg));

	fileDownloadErrMsg = GetFileDownloadLengthErrResponseMsg();

	if((fileDownloadErrMsg.data == NULL) || (fileDownloadErrMsg.length == 0)) {
		rfbLog("File [%s]: Method [%s]: Unexpected error: fileDownloadErrMsg "
				"is null\n", __FILE__, __FUNCTION__);
		return;
	}

	rfbWriteExact(cl, fileDownloadErrMsg.data, fileDownloadErrMsg.length);

	FreeFileTransferMsg(fileDownloadErrMsg);
}

extern rfbTightClientPtr rfbGetTightClientData(rfbClientPtr cl);

void*
RunFileDownloadThread(void* client)
{
	rfbClientPtr cl = (rfbClientPtr) client;
	rfbTightClientPtr rtcp = rfbGetTightClientData(cl);
	FileTransferMsg fileDownloadMsg;

	if(rtcp == NULL)
		return NULL;

	memset(&fileDownloadMsg, 0, sizeof(FileTransferMsg));
	do {
		pthread_mutex_lock(&fileDownloadMutex);
		fileDownloadMsg = GetFileDownloadResponseMsgInBlocks(cl, rtcp);
		pthread_mutex_unlock(&fileDownloadMutex);
		
		if((fileDownloadMsg.data != NULL) && (fileDownloadMsg.length != 0)) {
			if(rfbWriteExact(cl, fileDownloadMsg.data, fileDownloadMsg.length) < 0)  {
				rfbLog("File [%s]: Method [%s]: Error while writing to socket \n"
						, __FILE__, __FUNCTION__);

				if(cl != NULL) {
			    	rfbCloseClient(cl);
				CloseUndoneFileTransfer(cl, rtcp);
				}
				
				FreeFileTransferMsg(fileDownloadMsg);
				return NULL;
			}
			FreeFileTransferMsg(fileDownloadMsg);
		}
	} while(rtcp->rcft.rcfd.downloadInProgress == TRUE);
	return NULL;
}


void
HandleFileDownload(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
	pthread_t fileDownloadThread;
	FileTransferMsg fileDownloadMsg;
	
	memset(&fileDownloadMsg, 0, sizeof(FileTransferMsg));
	fileDownloadMsg = ChkFileDownloadErr(cl, rtcp);
	if((fileDownloadMsg.data != NULL) && (fileDownloadMsg.length != 0)) {
		rfbWriteExact(cl, fileDownloadMsg.data, fileDownloadMsg.length);
		FreeFileTransferMsg(fileDownloadMsg);
		return;
	}
	rtcp->rcft.rcfd.downloadInProgress = FALSE;
	rtcp->rcft.rcfd.downloadFD = -1;

	if(pthread_create(&fileDownloadThread, NULL, RunFileDownloadThread, (void*) 
	cl) != 0) {
		FileTransferMsg ftm = GetFileDownLoadErrMsg();
		
		rfbLog("File [%s]: Method [%s]: Download thread creation failed\n",
				__FILE__, __FUNCTION__);
		
		if((ftm.data != NULL) && (ftm.length != 0)) {
			rfbWriteExact(cl, ftm.data, ftm.length);
			FreeFileTransferMsg(ftm);
			return;
		}
				
	}
	
}


/******************************************************************************
 * Methods to Handle File Download Cancel Request.
 ******************************************************************************/

 
void
HandleFileDownloadCancelRequest(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
	int n = 0;
	char *reason = NULL;
	rfbClientToServerTightMsg msg;

	memset(&msg, 0, sizeof(rfbClientToServerTightMsg));
	
	if((n = rfbReadExact(cl, ((char *)&msg)+1, sz_rfbFileDownloadCancelMsg-1)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading "
					"FileDownloadCancelMsg\n", __FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	    return;
	}

	msg.fdc.reasonLen = Swap16IfLE(msg.fdc.reasonLen);

	if(msg.fdc.reasonLen == 0) {
		rfbLog("File [%s]: Method [%s]: reason length received is Zero\n",
				__FILE__, __FUNCTION__);
		return;
	}
	
	reason = (char*) calloc(msg.fdc.reasonLen + 1, sizeof(char));
	if(reason == NULL) {
		rfbLog("File [%s]: Method [%s]: Fatal Error: Memory alloc failed\n", 
				__FILE__, __FUNCTION__);
		return;
	}

	if((n = rfbReadExact(cl, reason, msg.fdc.reasonLen)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading "
					"FileDownloadCancelMsg\n", __FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	}

	rfbLog("File [%s]: Method [%s]: File Download Cancel Request received:"
					" reason <%s>\n", __FILE__, __FUNCTION__, reason);
	
	pthread_mutex_lock(&fileDownloadMutex);
	CloseUndoneFileTransfer(cl, rtcp);
	pthread_mutex_unlock(&fileDownloadMutex);
	
	if(reason != NULL) {
		free(reason);
		reason = NULL;
	}

}


/******************************************************************************
 * Methods to Handle File upload request
 ******************************************************************************/

#ifdef TODO
void HandleFileUploadRequest(rfbClientPtr cl);
#endif
void HandleFileUpload(rfbClientPtr cl, rfbTightClientPtr data);
void HandleFileUploadLengthError(rfbClientPtr cl, short fNameSize);
void SendFileUploadLengthErrMsg(rfbClientPtr cl);


void
HandleFileUploadRequest(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
	int n = 0;
	char path[PATH_MAX]; /* PATH_MAX has the value 4096 and is defined in limits.h */
	rfbClientToServerTightMsg msg;

	memset(path, 0, PATH_MAX);
	memset(&msg, 0, sizeof(rfbClientToServerTightMsg));
	
	if(cl == NULL) {
		rfbLog("File [%s]: Method [%s]: Unexpected error: rfbClientPtr is null\n",
				__FILE__, __FUNCTION__);
		return;
	}
	
	if((n = rfbReadExact(cl, ((char *)&msg)+1, sz_rfbFileUploadRequestMsg-1)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading FileUploadRequestMsg\n",
					__FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	    return;
	}

	msg.fupr.fNameSize = Swap16IfLE(msg.fupr.fNameSize);
	msg.fupr.position = Swap16IfLE(msg.fupr.position);

	if ((msg.fupr.fNameSize == 0) ||
		(msg.fupr.fNameSize > (PATH_MAX - 1))) {
		
		rfbLog("File [%s]: Method [%s]: error: path length is greater than PATH_MAX\n",
				__FILE__, __FUNCTION__);
		HandleFileUploadLengthError(cl, msg.fupr.fNameSize);
		return;
	}

	if((n = rfbReadExact(cl, rtcp->rcft.rcfu.fName, msg.fupr.fNameSize)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading FileUploadRequestMsg\n"
					__FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	    return;
	}
	rtcp->rcft.rcfu.fName[msg.fupr.fNameSize] = '\0';
	
	if(ConvertPath(rtcp->rcft.rcfu.fName) == NULL) {
    	rfbLog("File [%s]: Method [%s]: Unexpected error: path is NULL\n",
    			__FILE__, __FUNCTION__);

    	/* This may come if the path length exceeds PATH_MAX.
    	   So sending path length error to client
    	 */
    	 SendFileUploadLengthErrMsg(cl);
    	return;
	}

	HandleFileUpload(cl, rtcp);
}


void
HandleFileUploadLengthError(rfbClientPtr cl, short fNameSize)
{
	char *path = NULL;
	int n = 0;
	
	if((path = (char*) calloc(fNameSize, sizeof(char))) == NULL) {
		rfbLog("File [%s]: Method [%s]: Fatal Error: Alloc failed\n", 
				__FILE__, __FUNCTION__);
		return;
	}
	if((n = rfbReadExact(cl, path, fNameSize)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading dir name\n", 
							__FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);

	    if(path != NULL) {
			free(path);
			path = NULL;
		}
	    
	    return;
	}

	rfbLog("File [%s]: Method [%s]: File Upload Length Error occured"
			"file path requested is <%s>\n", __FILE__, __FUNCTION__, path);

    if(path != NULL) {
		free(path);
		path = NULL;
	}

    SendFileUploadLengthErrMsg(cl);
}

void
SendFileUploadLengthErrMsg(rfbClientPtr cl)
{

	FileTransferMsg fileUploadErrMsg;
	
	memset(&fileUploadErrMsg, 0, sizeof(FileTransferMsg));
	fileUploadErrMsg = GetFileUploadLengthErrResponseMsg();

	if((fileUploadErrMsg.data == NULL) || (fileUploadErrMsg.length == 0)) {
		rfbLog("File [%s]: Method [%s]: Unexpected error: fileUploadErrMsg is null\n",
				__FILE__, __FUNCTION__);
		return;
	}

	rfbWriteExact(cl, fileUploadErrMsg.data, fileUploadErrMsg.length);
	FreeFileTransferMsg(fileUploadErrMsg);
}

void
HandleFileUpload(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
	FileTransferMsg fileUploadErrMsg;

	memset(&fileUploadErrMsg, 0, sizeof(FileTransferMsg));
	
	rtcp->rcft.rcfu.uploadInProgress = FALSE;
	rtcp->rcft.rcfu.uploadFD = -1;

	fileUploadErrMsg = ChkFileUploadErr(cl, rtcp);
	if((fileUploadErrMsg.data != NULL) && (fileUploadErrMsg.length != 0)) {
		rfbWriteExact(cl, fileUploadErrMsg.data, fileUploadErrMsg.length);
		FreeFileTransferMsg(fileUploadErrMsg);
	}
}


/******************************************************************************
 * Methods to Handle File Upload Data Request
 *****************************************************************************/

void HandleFileUploadWrite(rfbClientPtr cl, rfbTightClientPtr rtcp, char* pBuf);


void
HandleFileUploadDataRequest(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
	int n = 0;
	char* pBuf = NULL;
	rfbClientToServerTightMsg msg;

	memset(&msg, 0, sizeof(rfbClientToServerTightMsg));
	
	if(cl == NULL) {
		rfbLog("File [%s]: Method [%s]: Unexpected error: rfbClientPtr is null\n",
				__FILE__, __FUNCTION__);
		return;
	}

	if((n = rfbReadExact(cl, ((char *)&msg)+1, sz_rfbFileUploadDataMsg-1)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading FileUploadRequestMsg\n",
					__FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	    return;
	}

	msg.fud.realSize = Swap16IfLE(msg.fud.realSize);
	msg.fud.compressedSize = Swap16IfLE(msg.fud.compressedSize);
	if((msg.fud.realSize == 0) && (msg.fud.compressedSize == 0)) {
		if((n = rfbReadExact(cl, (char*)&(rtcp->rcft.rcfu.mTime), sizeof(unsigned 
		long))) <= 0) {
			
			if (n < 0)
				rfbLog("File [%s]: Method [%s]: Error while reading FileUploadRequestMsg\n",
						__FILE__, __FUNCTION__);
			
		    rfbCloseClient(cl);
		    return;
		}

		FileUpdateComplete(cl, rtcp);
		return;
	}

	pBuf = (char*) calloc(msg.fud.compressedSize, sizeof(char));
	if(pBuf == NULL) {
		rfbLog("File [%s]: Method [%s]: Memory alloc failed\n", __FILE__, __FUNCTION__);
		return;
	}
	if((n = rfbReadExact(cl, pBuf, msg.fud.compressedSize)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading FileUploadRequestMsg\n",
					__FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);

	    if(pBuf != NULL) {
	    	free(pBuf);
	    	pBuf = NULL;
		}
	    
	    return;
	}	
	if(msg.fud.compressedLevel != 0) {
		FileTransferMsg ftm;
		memset(&ftm, 0, sizeof(FileTransferMsg));
		
		ftm = GetFileUploadCompressedLevelErrMsg();

		if((ftm.data != NULL) && (ftm.length != 0)) {
			rfbWriteExact(cl, ftm.data, ftm.length);
			FreeFileTransferMsg(ftm);
		}

		CloseUndoneFileTransfer(cl, rtcp);

	    if(pBuf != NULL) {
	    	free(pBuf);
	    	pBuf = NULL;
		}
		
		return;
	}

	rtcp->rcft.rcfu.fSize = msg.fud.compressedSize;
	
	HandleFileUploadWrite(cl, rtcp, pBuf);

    if(pBuf != NULL) {
    	free(pBuf);
    	pBuf = NULL;
	}

}


void
HandleFileUploadWrite(rfbClientPtr cl, rfbTightClientPtr rtcp, char* pBuf)
{
	FileTransferMsg ftm;
	memset(&ftm, 0, sizeof(FileTransferMsg));

	ftm = ChkFileUploadWriteErr(cl, rtcp, pBuf);

	if((ftm.data != NULL) && (ftm.length != 0)) {
		rfbWriteExact(cl, ftm.data, ftm.length);
		FreeFileTransferMsg(ftm);
	}
}


/******************************************************************************
 * Methods to Handle File Upload Failed Request.
 ******************************************************************************/

 
void 
HandleFileUploadFailedRequest(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
	int n = 0;
	char* reason = NULL;
	rfbClientToServerTightMsg msg;

	memset(&msg, 0, sizeof(rfbClientToServerTightMsg));
	
	if(cl == NULL) {
		rfbLog("File [%s]: Method [%s]: Unexpected error: rfbClientPtr is null\n",
				__FILE__, __FUNCTION__);
		return;
	}
	
	if((n = rfbReadExact(cl, ((char *)&msg)+1, sz_rfbFileUploadFailedMsg-1)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading FileUploadFailedMsg\n",
					__FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	    return;
	}

	msg.fuf.reasonLen = Swap16IfLE(msg.fuf.reasonLen);
	if(msg.fuf.reasonLen  == 0) {
		rfbLog("File [%s]: Method [%s]: reason length received is Zero\n",
				__FILE__, __FUNCTION__);
		return;
	}


	reason = (char*) calloc(msg.fuf.reasonLen + 1, sizeof(char));
	if(reason == NULL) {
		rfbLog("File [%s]: Method [%s]: Memory alloc failed\n", __FILE__, __FUNCTION__);
		return;		
	}
	
	if((n = rfbReadExact(cl, reason, msg.fuf.reasonLen)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading FileUploadFailedMsg\n",
					__FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);

		if(reason != NULL) {
			free(reason);
			reason = NULL;
		}

	    return;
	}

	rfbLog("File [%s]: Method [%s]: File Upload Failed Request received:"
				" reason <%s>\n", __FILE__, __FUNCTION__, reason);

	CloseUndoneFileTransfer(cl, rtcp);

	if(reason != NULL) {
		free(reason);
		reason = NULL;
	}

}


/******************************************************************************
 * Methods to Handle File Create Request.
 ******************************************************************************/

 
void 
HandleFileCreateDirRequest(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
	int n = 0;
	char dirName[PATH_MAX];
	rfbClientToServerTightMsg msg;

	memset(dirName, 0, PATH_MAX);
	memset(&msg, 0, sizeof(rfbClientToServerTightMsg));
	
	if(cl == NULL) {
		rfbLog("File [%s]: Method [%s]: Unexpected error: rfbClientPtr is null\n",
				__FILE__, __FUNCTION__);
		return;
	}
	
	if((n = rfbReadExact(cl, ((char *)&msg)+1, sz_rfbFileCreateDirRequestMsg-1)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading FileCreateDirRequestMsg\n",
					__FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	    return;
	}

	msg.fcdr.dNameLen = Swap16IfLE(msg.fcdr.dNameLen);

	/* TODO :: chk if the dNameLen is greater than PATH_MAX */	
	
	if((n = rfbReadExact(cl, dirName, msg.fcdr.dNameLen)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading FileUploadFailedMsg\n",
					__FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	    return;
	}

	if(ConvertPath(dirName) == NULL) {
    	rfbLog("File [%s]: Method [%s]: Unexpected error: path is NULL\n",
    			__FILE__, __FUNCTION__);

    	return;
	}

	CreateDirectory(dirName);
}
