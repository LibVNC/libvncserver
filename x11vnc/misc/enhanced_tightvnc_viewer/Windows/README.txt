
This is a Windows utility to automatically start up STUNNEL to redirect
SSL VNC connections to a remote host.  Then TightVNC Viewer (included)
is launched to use this SSL tunnel.

An example server would be "x11vnc -ssl", or any VNC server with a
2nd STUNNEL program running on the server side.

Just click on the program "ssvnc.exe", and then enter the remote
VNC Server and click "Connect".  Click on "Help" for more information
information.  You can also set some simple options under "Options ..."

If you want that application to run in "SSH-ONLY" mode, click on
the "sshvnc.bat" wrapper instead.  Or enter SSH_ONLY.

Note that on Windows when the TightVNC viewer disconnects you may need to
terminate the STUNNEL program manually.  To do this: Click on the STUNNEL
icon (dark green) on the System Tray and then click "Exit".  Before that,
however, you will be prompted if you want ssvnc.exe to try to terminate
STUNNEL for you. (Note that even if STUNNEL termination is successful,
the Tray Icon may not go away until the mouse hovers over it!)

With this STUNNEL and TightVNC Viewer wrapper you can also enable using
SSL Certificates with STUNNEL, and so the connection is not only encrypted
but it is also not susceptible to man-in-the-middle attacks.

See the STUNNEL and x11vnc documentation for how to create and add SSL
Certificates (PEM files) for authentication.  Click on the "Certs ..."
button to specify the certificate(s).  See the Help there for more info
and also:

	http://www.karlrunge.com/x11vnc
	http://www.tightvnc.com
	http://www.stunnel.org
	http://www.openssl.org
	http://www.chiark.greenend.org.uk/~sgtatham/putty/

You can use x11vnc to create certificates if you like:

	http://www.karlrunge.com/x11vnc/faq.html#faq-ssl-ca


Misc:

	The openssl.exe stunnel.exe vncviewer.exe libeay32.dll
	libssl32.dll programs came from the websites mentioned above.

	IMPORTANT: some of these binaries may have cryptographic
	software that you may not be allowed to download or use.
	See the above websites for more information and also the
        util/info subdirectories. 

	Also, the kill.exe and tlist.exe programs in the w98 directory
	came from diagnostic tools ftp site of Microsoft's.



Important Note for Windows Vista: One user reports that on Windows Vista
if you move or extract the "ssvnc" folder down to the "Program Files"
folder you will be prompted to do this as the Administrator. But then
when you start up ssvnc, as a regular user, it cannot create files in
that folder and so it fails to run properly. We recommend to not copy
or extract the "ssvnc" folder into "Program Files". Rather, extract
it to somewhere you have write permission (e.g. C:\ or your User dir)
and create a Shortcut to ssvnc.exe on the desktop.

If you must put a launcher file down in "Program Files", perhaps an
"ssvnc.bat" that looks like this:
C:
cd \ssvnc\Windows
ssvnc.exe

