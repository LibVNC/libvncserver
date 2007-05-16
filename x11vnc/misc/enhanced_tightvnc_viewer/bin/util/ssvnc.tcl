#!/bin/sh
# the next line restarts using wish \
exec wish "$0" "$@"

#
# Copyright (c) 2006-2007 by Karl J. Runge <runge@karlrunge.com>
#
# ssvnc.tcl: gui wrapper to the programs in this
# package. Also sets up service port forwarding.
#

set buck_zero $argv0

proc center_win {w} {
	global is_windows
	update
	set W [winfo screenwidth  $w]
	set W [expr $W + 1]
	wm geometry $w +$W+0
	update
	set x [expr [winfo screenwidth  $w]/2 - [winfo width  $w]/2]
	set y [expr [winfo screenheight $w]/2 - [winfo height $w]/2]

	if {$is_windows} {
		set y [expr "$y - 30"] 
		if {$y <= 0} {
			set y 1
		}
	}
	wm geometry $w +$x+$y
	wm deiconify $w
	update
}

proc toplev {w} {
	catch {destroy $w}
	toplevel $w
	catch {wm withdraw $w}
}

proc apply_bg {w} {
	global is_windows system_button_face
	if {$is_windows && $system_button_face != ""} {
		catch {$w configure -bg "$system_button_face"}
	}
}

proc scroll_text {fr {w 80} {h 35}} {
	global help_font is_windows scroll_text_focus

	catch {destroy $fr}
	
	frame $fr -bd 0

	eval text $fr.t -width $w -height $h $help_font \
		 -setgrid 1 -bd 2 -yscrollcommand {"$fr.y set"} -relief ridge 

	apply_bg $fr.t

	scrollbar $fr.y -orient v -relief sunken -command "$fr.t yview"
	pack $fr.y -side right -fill y
	pack $fr.t -side top -fill both -expand 1

	if {$scroll_text_focus} {
		focus $fr.t
	}
}

proc scroll_text_dismiss {fr {w 80} {h 35}} {
	global help_font

	scroll_text $fr $w $h

	set up $fr
	regsub {\.[^.]*$} $up "" up

	button $up.d -text "Dismiss" -command "destroy $up"
	bind $up <Escape> "destroy $up"
	pack $up.d -side bottom -fill x
	pack $fr -side top -fill both -expand 1
}

proc jiggle_text {w} {
	global uname
	if {$uname == "Darwin"} {
		$w yview scroll 1 pages
		update idletasks
		$w yview scroll -1 pages
		update idletasks
	}
}

proc help {} {
	toplev .h

	scroll_text_dismiss .h.f

	center_win .h
	wm title .h "SSL/SSH VNC Viewer Help"

	set msg {
 Hosts and Displays:

    Enter the VNC host and display in the 'VNC Host:Display' entry box.
    
    It is of the form "host:number", where "host" is the hostname of the
    machine running the VNC Server and "number" is the VNC display number;
    it is often "0".  Examples:

           snoopy:0
           far-away.east:0
           sunray-srv1.west:17
           24.67.132.27:0
    
    Then click on "Connect".  When you do so the STUNNEL program will be
    started locally to provide you with an outgoing SSL tunnel.

    Once the STUNNEL is running, the TightVNC Viewer (Or Chicken-of-the-VNC
    on Mac OS X) will be automatically started directed to the local SSL
    tunnel which, in turn, encrypts and redirects the connection to the
    remote VNC server.

    SSH tunnels are described below.

    If you are using a port less than the default VNC port 5900 (usually
    the VNC display = port - 5900), use the full port number itself, e.g.:

           24.67.132.27:443

    Note, however, if the number n after the colon is less than 200, then
    a port number 5900 + n is assumed; i.e. n is the VNC display number.
    If you must use a TCP port less than 200, specify a negative value,
    e.g.:  24.67.132.27:-80

    The remote VNC server must support an initial SSL handshake before
    using the VNC protocol (i.e. VNC is tunnelled through the SSL channel
    after it is established).  "x11vnc -ssl ..."  does this, and any VNC
    server can be made to do this by using, e.g., STUNNEL on the remote side.


    *IMPORTANT*: If you do not take the steps to verify the VNC Server's
    SSL Certificate, you are vulnerable to a Man-In-The-Middle attack.
    Only passive network sniffing attacks will be prevented.

    You can use the "Fetch Cert" button to retrieve the Cert and then
    after you check it is OK (say, via comparing the MD5 or other info)
    you can Save it and use it to verify future connections to servers.

    If "Verify All Certs" is checked, this check is always enforced, and
    so the first time you connect to a new server you may need to follow
    a few dialogs to inspect and save the server certificate.  See the
    "Certs... -> Help" for information on how to manage certificates.

    "Fetch Cert" and "Verify All Certs" are currently disabled in the
    "SSH + SSL" mode (e.g. SSH is used to enter a firewall gateway,
    and then SSL is tunneled through that to reach the workstation).


    Note that on Windows when the Viewer connection is finished you may
    need to terminate STUNNEL manually from the System Tray (right click
    on dark green icon) and selecting "Exit".

 VNC Password:

    On Unix or MacOSX if there is a VNC password for the server you
    can enter it in the "VNC Password:" entry box.  

    This is *REQUIRED* on MacOSX when Chicken of the VNC (the default)
    is used.  On Unix if you choose not to enter the password you will
    be prompted for it in the terminal window running TightVNC viewer.
    On Windows TightVNC viewer should prompt you.

    NOTE: when you Save a VNC profile (Options ... -> Save Profile),
    the password is not saved (you need to enter it each time).

 SSH:

    Click on "Use SSH" or go to "Options ..." if you want to use an
    *SSH* tunnel instead of SSL (then the VNC Server does not need to
    speak SSL or use STUNNEL).  You will need to be able to login to the
    remote host via SSH (e.g. via password or ssh-agent).

    Specify the hostname and VNC display in the VNC Host:Display entry.
    Use something like "username@hostname.com:0" if the remote username
    is different.  "SSH + SSL" is similar but its use is more rare. See
    the Help under Options for more info.


 Proxies:

    If an intermediate proxy is needed to make the SSL connection
    (e.g. web gateway out of a firewall) enter it in the "Proxy/Gateway"
    entry box, or Alternatively supply both hosts separated by spaces
    (with the proxy second) in the VNC Host:Display box:

           host:number   gwhost:port 

    E.g.:  far-away.east:0   mygateway.com:8080

    If the "double proxy" case is required (e.g. coming out of a web
    proxied firewall environment), separate them via a comma, e.g.:

           far-away:0   local-proxy:8080,mygateway.com:443

    (either as above, or putting the 2nd string in the "Proxy/Gateway"
    entry box).

    See the ss_vncviewer description and x11vnc FAQ for info on proxies:

           http://www.karlrunge.com/x11vnc/#ss_vncviewer
           http://www.karlrunge.com/x11vnc/#faq-ssl-java-viewer-proxy

    Proxies also apply to SSH mode, it is a usually a gateway machine to
    log into via SSH that is not the workstation running the VNC server.


 Remote SSH Command:

    In SSH or SSH + SSL mode you can also specify a remote command to run
    on the remote ssh host in the "Remote SSH Command" entry.  The default
    is just to sleep a bit (e.g. sleep 30) to make sure the port tunnels are
    active.  Alternatively you could have the remote command start the
    VNC server, e.g.  x11vnc -nopw -display :0 -rfbport 5900 -localhost

    You can also specify the remote SSH command by putting a string like
    
         cmd=x11vnc -nopw -display :0 -rfbport 5900 -localhost

    (use any command you wish to run) at the END of the VNC Host:Display
    entry.  In general, you can cram it all in the VNC Host:Display if
    you like:   host:disp  proxy:port  cmd=...  (this is the way it is
    stored internally).

    When starting the VNC server this way, note that sometimes you
    will need to correlate the VNC Display number with the "-rfbport"
    (or similar) option of the server.  E.g.:

         VNC Host:Display       username@somehost.com:2
         Remote SSH Command:    x11vnc -find -rfbport 5902

    See the the Tip below (11) for using x11vnc PORT=NNNN feature (or
    vncserver(1) output) to not need to specify the VNC display number
    or the x11vnc -rfbport option.


 SSL Certificates:

    If you want to use a SSL Certificate (PEM) file to authenticate
    yourself to the VNC server ("MyCert") or to verify the identity of
    the VNC Server ("ServerCert" or "CertsDir") select the certificate
    file by clicking the "Certs ..." button before connecting.

    Certificate verification is needed to prevent Man-In-The-Middle
    attacks; if it is not done then only passive network sniffing attacks
    are prevented.  See the x11vnc documentation:

           http://www.karlrunge.com/x11vnc/ssl.html

    for how to create and use PEM SSL certificate files.  An easy way is:

           x11vnc -ssl SAVE ...

    where it will print out its automatically generated certificate to
    the screen and that can be safely copied to the viewer side.

    You can also use the "Create Certificate" feature of this program
    under "Certs ...".  Just click on it and follow the instructions in
    the dialog.  Then copy the cert file to the VNC Server and specify
    the other one in the "Certs ..." dialog.

    Alternatively you can use the "Import Certificate" action to paste
    in a certificate or read one in from a file or use the "Fetch Cert"
    button on the main panel.  If "Verify All Certs" is checked, you
    will be forced to check Certs of any new servers the first time
    you connect.

    Note that "Verify All Certs" is on by default so that users who do
    not understand the SSL Man-In-The-Middle problem will not be left
    completely vulnerable to it (everyone still must make the effort to
    verify new certificates by an external method to be completely safe).

    To have "Verify All Certs" toggled off at startup, use "ssvnc -nv"
    or set SSVNC_NO_VERIFY_ALL=1 before starting.  If you do not even want
    to see the button, use "ssvnc -nvb" or SSVNC_NO_VERIFY_ALL_BUTTON=1.


 More Options:

    To set other Options, e.g. to use SSH instead of STUNNEL SSL, or
    View-Only usage, click on the "Options ..." button and read the Help
    there.

 Profiles:

    Use "Save Profile" under "Options ..." to save a profile (i.e. a
    host:display and its specific settings) with a name.

    To load in a saved Options profile, click on the "Load" button.
    This is the same as the "Load Profile" button under "Options"

    To list your profiles use: "ssvnc -profiles"

    You can launch ssvnc and have it immediately connect to the server
    by invoking it something like this:

        ssvnc profile1              (launches profile named "profile1")
        ssvnc hostname:0            (connect to hostname VNC disp 0 via SSL)
        ssvnc vnc+ssl://hostname:0  (same)
        ssvnc vnc+ssh://hostname:0  (connect to hostname VNC disp 0 via SSH)

    see the Tips 5 and 9 below for more about the URL-like syntax.


 More Info:

    See these links for more information:

        http://www.karlrunge.com/x11vnc/#faq-ssl-tunnel-ext
        http://www.stunnel.org
        http://www.tightvnc.com


 Tips and Tricks:

     1) On Unix to get a 2nd GUI (e.g. for a 2nd connection) press Ctrl-N
        on the GUI.  If only the xterm window is visible you can press
        Ctrl-N or try Ctrl-LeftButton -> New SSVNC_GUI.  On Windows you
        will have to manually Start a new one: Start -> Run ..., etc.

     2) If you use "user@hostname cmd=SHELL" then you get an SSH shell only:
        no VNC viewer will be launched.  On Windows "user@hostname cmd=PUTTY"
        will try to use putty.exe (better terminal emulation than
        plink.exe).  A ShortCut for this is Ctrl-S as long as user@hostname
        is present in the entry box.  You can also put the string in the
        "Remote SSH Command" entry.

     3) If you use "user@hostname cmd=KNOCK" then only the port-knocking 
        is performed.  A ShortCut for this is Ctrl-P as long as hostname
        is present in the entry box.  If it matches cmd=KNOCKF, i.e. an
        extra "F", then the port-knocking "FINISH" sequence is sent, if any.
        A ShortCut for this Shift-Ctrl-P as long as hostname is present.
        You can also put the string in the "Remote SSH Command" entry.

     4) Pressing the "Load" button or pressing Ctrl-L or Clicking the Right
        mouse button on the main GUI will invoke the Load Profile dialog.

     5) If you want to do a Direct VNC connection, WITH *NO* SSL OR SSH
        ENCRYPTION, use the "vnc://" prefix, e.g. vnc://far-away.east:0
        This also works for reverse connections (see below).

        Sorry we do not make this easy to figure out how to do (e.g. a
        button on the main panel), but the goal of SSVNC is secure 
        connections!  Set the env var SSVNC_NO_ENC_WARN=1 (or use Vnc://)
        to skip the warning prompts.

     6) Reverse VNC connections are possible as well.  Go to Options and
        select "Reverse VNC connection".  In the 'VNC Host:Display' entry
        box put in the number (e.g. "0" or ":0") that corresponds to the
        Listening display (0 -> port 5500).  See the Options Help for more
        info.

     7) On Unix to have SSVNC act as a general STUNNEL redirector (i.e. no
        VNC), put the the desired host:port in VNC Host:Display (use a
        negative port value if it is to be less than 200), then go to
        Options -> Advanced -> Change VNC Viewer.  Change the "viewer"
        command to be "xmessage OK" or "xmessage <port>" (or sleep) where
        port is the desired local listening port.  Then click Connect.
        If you didn't set the local port look for it in the terminal output.

        On Windows set it to "NOTEPAD" or similar; you can't control
        the port though.  It is usually 5930.

     8) On Unix if you are going to an older SSH server (e.g. Solaris 10),
        you will probably need to set the env. var. SS_VNCVIEWER_NO_T=1
        to disable the ssh "-t" option being used (that can prevent the
        command from being run).

     9) In the VNC Host:Display entry you can also use these "URL-like"
        prefixes:  vncs://host:0, vncssl://host:0, and vnc+ssl://host:0
        for SSL, and vncssh://host:0 and vnc+ssh://host:0 for SSH. There
        is no need to toggle the SSL/SSH setting.  These also work from
        the command line, e.g.:  ssvnc vnc+ssh://mymachine:10

    10) Mobile USB memory stick / flash drive usage:  You can unpack ssvnc
        to a flash drive for impromptu usage (e.g. from a friends computer)
        If you create a directory "Home" in the toplevel ssvnc directory,
        then that will be the default location for your VNC profiles and
        certs.  So they follow the drive this way.  If you run like this:
        "ssvnc ." or "ssvnc.exe ." the "Home" directory will be created for
        you.  WARNING: if you use ssvnc from an "Internet Cafe", i.e. an
        untrusted computer, an intruder may be capturing keystrokes, etc.

	You can also set the SSVNC_HOME env. var. to point to any
	directory you want. It can be set after starting ssvnc by putting
	HOME=/path/to/dir in the Host:Display box and clicking "Connect".

    11) Dynamic VNC Server Port determination and redirection:  If you
        are running SSVNC on Unix and are using SSH to start the remote
        VNC server and the VNC server prints out the line "PORT=NNNN"
        to indicate which dynamic port it is using (x11vnc does this),
        then if you prefix the SSH command with "PORT=" SSVNC will watch
        for the PORT=NNNN line and uses ssh's built in SOCKS proxy
        (ssh -D ...) to connect to the dynamic VNC server port through
        the SSH tunnel.  For example:

                VNC Host:Display     user@somehost.com
                Remote SSH Command:  PORT= x11vnc -find

        or "PORT= x11vnc -display :0 -localhost", etc.  Or use "P= ..."

        There is also code to detect the display of the regular Unix
        vncserver(1).  It extracts the display (and hence port) from
        the lines "New 'X' desktop is hostname:4" and also 
        "VNC server is already running as :4".  So you can use
        something like:

                PORT= vncserver; sleep 15 
        or:     PORT= vncserver :4; sleep 15 

        the latter is preferred because when you reconnect with it will
        find the already running one.  The former one will keep creating
        new X sessions if called repeatedly.
}

	.h.f.t insert end $msg
	jiggle_text .h.f.t
}

proc help_certs {} {
	toplev .ch

	scroll_text_dismiss .ch.f 90 33

	center_win .ch
	wm resizable .ch 1 0

	wm title .ch "SSL Certificates Help"

	set msg {
 Description:

    *IMPORTANT*: Only with SSL Certificate verification (either manually or via
    Certificate Authority) can Man-In-The-Middle attacks be prevented.  Otherwise,
    only passive network sniffing attacks are prevented.

    The SSL Certificate files described below can have been created externally
    (e.g. by x11vnc), you can import it via "Import Certificate" if you like.
    OR you can click on "Create Certificate ..." to use this program to generate a
    Certificate + Private Key pair.  In that case you will need to distribute one
    of the generated files to the VNC Server.

    You can also retrieve the remote VNC Server's Cert via the "Fetch Cert" button
    on the main panel.  After you check that it is the correct Cert (e.g. by
    comparing MD5 hash or other info), you can save it.  It will be set as the
    "ServerCert" to verify against for the connection.  To make this verification
    check permanent, you will need to save the profile via Options -> Save Profile.

    If "Verify All Certs" is checked, you are forced to do this check, and so the
    first time you connect to a new server you may need to follow a few dialogs to
    inspect and save the server certificate.  In this case certificates are saved
    in the 'Accepted Certs' directory.  When "Verify All Certs" is checked all
    hosts or profiles with "CertsDir" set to "ACCEPTED_CERTS" (and no "ServerCert"
    setting) will be check against the accepted certificates.

    Note that "Verify All Certs" is on by default so that users who do not
    understand the SSL Man-In-The-Middle problem will not be left completely
    vulnerable to it (everyone still must make the effort to verify new certificates
    by an external method to be completely safe)

    To have "Verify All Certs" toggled off at startup, use "ssvnc -nv" or set
    SSVNC_NO_VERIFY_ALL=1 before starting.  If you do not even want to see the
    button, use "ssvnc -nvb" or SSVNC_NO_VERIFY_ALL_BUTTON=1.

    Note: due to a deficiency in openssl "Fetch Cert" may be slow on Windows.  Also:
    "Fetch Cert" and "Verify All Certs" do not currently work in "SSH + SSL" mode.

    One can make SSL VNC server authentication "automatic" as it is in Web
    Browsers going to HTTPS sites, by using a Certificate Authority (CA) cert
    (e.g. a professional one like Verisign or Thawte, or one your company or
    organization creates).  This is described in detail here:
    http://www.karlrunge.com/x11vnc/ssl.html You simply use the CA cert in the
    entries described below.


 Your Certificate + Key:

    You can specify your own SSL certificate (PEM) file in "MyCert" in which case it
    is used to authenticate you (the viewer) to the remote VNC Server.  If this fails
    the remote VNC Server will drop the connection.


 Server Certificates:
    
    Server certs can be specified in one of two ways:
    
        - A single certificate (PEM) file for a single server
          or a single Certificate Authority (CA)
    
        - A directory of certificate (PEM) files stored in
          the special OpenSSL hash fashion.
    
    The former is set via "ServerCert" in this gui.
    The latter is set via "CertsDir" in this gui.
    
    The former corresponds to the "CAfile" STUNNEL parameter.
    The latter corresponds to the "CApath" STUNNEL parameter.
    See stunnel(8) or www.stunnel.org for more information.
    
    If the remote VNC Server fails to authenticate itself with respect to the specified
    certificate(s), then the VNC Viewer (your side) will drop the connection.

    Select which file or directory by clicking on the appropriate "Browse..."  button.
    Once selected, if you click Info or the Right Mouse button on "Browse..."
    then information about the certificate will be displayed.

    If "CertsDir" is set to the token "ACCEPTED_CERTS" (and "ServerCert" is
    unset) then the certificates accumulated in the special 'Accepted Certs'
    directory will be used.  "ACCEPTED_CERTS" is the default for every server
    ("Verify All Certs").  Note that if you ever need to clean this directory,
    each cert is saved in two files, for example:

          bf-d0-d6-9c-68-5a-fe-24-c6-60-ba-b4-14-e6-66-14=hostname-0.crt
    and
          9eb7c8be.0

    This is because of the way OpenSSL must use hash-based filenames in Cert dirs. 


 Notes:

    If "Use SSH" has been selected then SSL certs are disabled.

    See the x11vnc and STUNNEL documentation for how to create and use PEM
    certificate files:

        http://www.karlrunge.com/x11vnc/#faq-ssl-tunnel-ext
        http://www.karlrunge.com/x11vnc/ssl.html
        http://www.stunnel.org

    A common way to create and use a VNC Server certificate is:

        x11vnc -ssl SAVE ...

    and then copy the Server certificate to the local (viewer-side) machine.
    x11vnc prints out to the screen the Server certificate it generates.
    You can set "ServerCert" to it directly or use the "Import Certificate"
    action to save it to a file.  Or use the "Fetch Cert" method.

    x11vnc also has command line utilities to create server, client, and CA
    (Certificate Authority) certificates.  See the above URLs.
}

	.ch.f.t insert end $msg
	jiggle_text .ch.f.t
}

proc help_opts {} {
	toplev .oh

	scroll_text_dismiss .oh.f

	center_win .oh

	wm title .oh "SSL/SSH Viewer Options Help"

set msg {
  Use SSL:  The default, use SSL via STUNNEL (this requires SSL aware VNC
            server, e.g. x11vnc -ssl SAVE ...)

  Use SSH:  Instead of using STUNNEL SSL, use ssh(1) for the encrypted
            tunnel.  You must be able to log in via ssh to the remote host.

            On Unix the cmdline ssh(1) program (it must already be installed)
            will be run in an xterm for passphrase authentication, etc. On
            Windows the cmdline plink.exe program will be launched in
            a Windows Console window.

            You can set the "VNC Host:Display" to "user@host:disp" to indicate
            ssh should log in as "user" on "host".  NOTE: On Windows you MUST
            always supply the "user@" part (due to a plink deficiency). E.g.:

                  fred@far-away.east:0

            If an intermediate gateway machine must be used (e.g. to enter
            a firewall; the VNC Server is not running on it), put it in the
            Proxy/Gateway entry or you can put something like this in the
            "VNC Host:Display" entry box:

                  workstation:0   user@gateway-host:port
  
            ssh is used to login to user@gateway-host and then a -L port
            redirection is set up to go to workstation:0 from gateway-host.
            ":port" is optional, use it if the gateway-host SSH port is
            not the default value 22.

            One can also do a "double ssh", i.e. a first SSH to the
            gateway login machine then a 2nd ssh to the destination machine
            (presumably it is running the vnc server).  Unlike the above
            example, the "last leg" (gateway-host -> workstation) is also
            encrypted by SSH this way.  Do this by splitting the gateway
            in two with a comma, the part before it is the first SSH:

                  :0   user@gateway-host:port,user@workstation:port

            (or in the Proxy/Gateway entry).

            In the "Remote SSH Command" entry you can to indicate that a
            remote command to be run.  The default is "sleep 15".  Also, at
            the very end of the entry box, you can append a cmd=... string
            to to achieve the same thing.  E.g.

                  user@host:0   cmd=x11vnc -nopw -display :0

            (if a gateway is also needed, put it just before the cmd=...
            e.g.  host:0  user@gateway-host:port cmd=x11vnc -nopw )


            Trick: If you use "cmd=SHELL" then you get an SSH shell only:
            no VNC viewer will be launched.  On Windows "cmd=PUTTY" will
            try to use putty.exe (better terminal emulation than plink.exe)
            A shortcut for this is Ctrl-S as long as user@hostname is present
            in the "VNC Host:Display" box.

  Use SSH + SSL: Tunnel the SSL connection through a SSH tunnel.  Use this
            if you want end-to-end SSL and must use a SSH gateway (e.g. to
            enter a firewall) or if additional SSH port redirs are required
            (CUPS, Sound, SMB tunnelling: See Advanced Options).

  Reverse VNC connection: reverse (listening) VNC connections are possible.

            For SSL connections in the 'VNC Host:Display' entry box put in
            the number (e.g. "0" or ":0") that corresponds to the Listening
            display (0 -> port 5500).  For example x11vnc can then be used:
            "x11vnc ... -ssl SAVE -connect hostname:port".

            Then a VNC server should establish a reverse connection to
            that port on this machine (e.g. -connect this-machine:5500)

            For reverse connections in SSH or SSH + SSL modes it is a
            little trickier.  The SSH tunnel (with -R redirect) must be
            established and remain up waiting for reverse connections.
            The default time is "sleep 1800", i.e. 30 mins.  You can put
            a longer or shorter sleep in "Remote SSH Command" (perhaps
            after your command runs:  cmd; sleep 3600).

            For SSH reverse connections put "hostname:n" in
            'VNC Host:Display' or "user@hostname:n".  The "n" will be the
            listening display on the *REMOTE* side.  So to have the remote
            x11vnc connect use: "x11vnc ... -connect localhost:n" or
            "x11vnc -R connect:localhost:n" (-ssl will be needed for SSH+SSL
            mode).  If the -R port cannot be opened because it is in use
            by another program you will have to kill everything and start
            over using a different port.

            In reverse connections mode be careful to protect the listening
            VNC Viewer from direct connections (neither SSL nor SSH)
            connecting directly to its listening port thereby bypassing
            the tunnel.  This can be done by a host-level firewall that
            only lets in, say, port 5500 (the default one ":0" for stunnel
            to listen on).  Or for SSH reverse connections allow NO 5500+n
            ports in.  For reverse connections, the Unix enhanced tightvnc
            viewers supplied in the SSVNC package will only listen on
            localhost so these precautions are not needed.

            Note that for SSL connections use of "Proxy/Gateway" does not
            make sense: the remote side cannot initiate its reverse connection
            via the Proxy.

            Note that for SSH or SSH+SSL connections use of "Proxy/Gateway"
            does not make sense (the ssh cannot do a -R on a remote host:port),
            unless it is a double proxy where the 2nd host is the machine with
            the VNC server.

  Putty PW:  On Windows only: use the supplied password for plink SSH logins.
             Unlike the other options the value is not saved when 'Save
             Profile' is performed.  This feature is useful when options under
             "Advanced" are set that require TWO SSH's: you just have
             to type the password once in this entry box.  The bundled
             pagent.exe and puttygen.exe programs can also be used to avoid
             repeatedly entering passwords (note this requires setting up
             and distributing SSH keys).  Start up pagent.exe or puttygen.exe
             and read the instructions there.
                
  ssh-agent: On Unix only: restart the GUI in the presence of ssh-agent(1)
             (e.g. in case you forgot to start your agent before starting
             this GUI).  An xterm will be used to enter passphrases, etc.
             This can avoid repeatedly entering passphrases for the
             SSH logins (note this requires setting up and distributing
             SSH keys).


  View Only:               Have VNC Viewer ignore mouse and keyboard input.
  
  Fullscreen:              Start the VNC Viewer in fullscreen mode.
  
  Raise On Beep:           Deiconify viewer when bell rings.
  
  Use 8bit color:          Request a very low-color pixel format.
  
  Cursor Alphablending:    Use the x11vnc alpha hack for translucent cursors
                           (requires Unix, 32bpp and same endianness)
  
  Use XGrabServer:         On Unix only, use the XGrabServer workaround for
                           old window managers.

  Do not use JPEG:         Do not use the jpeg aspect of the tight encoding.

  Compress Level/Quality:  Set TightVNC encoding parameters.

  Save and Load:   You can Save the current settings by clicking on Save
                   Profile (.vnc file) and you can also read in a saved one
                   with Load Profile.  Use the Browse... button to select
                   the filename via the GUI.

                   Pressing Ctrl-L or Clicking the Right mouse button on
                   the main GUI will invoke the Load Profile dialog.

                   Note: On Windows since the TightVNC Viewer will save
                   its own settings in the Registry, some unexpected
                   behavior is possible because the viewer is nearly
                   always directed to the VNC host "localhost:30".  E.g. if
                   you specify "View Only" in this gui once but not next
                   time the Windows VNC Viewer may remember the setting.
                   Unfortunately there is not a /noreg option for the Viewer.
                   

  Clear Options:   Set all options to their defaults (i.e. unset).

  Advanced:        Bring up the Advanced Options dialog.
}
	.oh.f.t insert end $msg
	jiggle_text .oh.f.t
}

proc help_fetch_cert {} {
	toplev .fh

	scroll_text_dismiss .fh.f 85 35

	center_win .fh
	wm resizable .fh 1 0

	wm title .fh "Fetch Certificates Help"

	set msg {
  The above SSL Certificate has been retrieved from the VNC Server via the
  "Fetch Cert" action.
  
  It has merely been downloaded via the SSL Protocol: IT HAS NOT BEEN VERIFIED
  IN ANY WAY.
  
  So, in principle, it could be a fake certificate being inserted by a bad
  person attempting to perform a Man-In-The-Middle attack on your SSL connection.
  
  If, however, by some external means you can verify the authenticity of
  this SSL Certificate you can use it for your VNC SSL connection to the
  VNC server you wish to connect to.  It will provide an authenticated and
  encrypted connection.
  
  You can verify the SSL Certificate by comparing the MD5 or SHA1 hash
  value via a method/channel you know is safe (i.e. not also under control
  of a Man-In-The-Middle attacker).  You could also check the text between
  the -----BEGIN CERTIFICATE----- and -----END CERTIFICATE----- tags, etc.
  
  Once you are sure it is correct, you can press the Save button to save the
  certificate to a file on the local machine for use when you connect via
  VNC tunneled through SSL.  If you save it, then that file will be set as
  the Certificate to verify the VNC server against.  You can see this in
  the dialog started via the "Certs..." button on the main panel.
  
  NOTE: If you want to make PERMANENT the association of the saved SSL
  certificate file with the VNC server host, you MUST save the setting as
  a profile for loading later. To Save a Profile, click on Options -> Save
  Profile ..., and choose a name for the profile and then click on Save.

  If "Verify All Certs" is checked, then you are forced to check all
  new certs.  In this case the certs are saved in the 'Accepted Certs'
  directory against which all servers will be checked unless "ServerCert"
  or "CertsDir" has been set to something else.

  To reload the profile at a later time, click on the "Load" button on
  the main panel and then select the name and click "Open".  If you want
  to be sure the certificate is still associated with the loaded in host,
  click on "Certs..." button and make sure the "ServerCert" points to the
  desired SSL filename.

  See the Certs... Help for more information.  A sophisticated method
  can be set up using a Certificate Authority key to verify never before
  seen certificates (i.e. like your web browser does).
}

	.fh.f.t insert end $msg
	jiggle_text .fh.f.t
}

proc win_nokill_msg {} {
	global help_font is_windows system_button_face
	toplev .w

	eval text .w.t -width 60 -height 11 $help_font
	button .w.d -text "Dismiss" -command {destroy .w}
	pack .w.t .w.d -side top -fill x

	apply_bg .w.t

	center_win .w
	wm resizable .w 1 0

	wm title .w "SSL/SSH Viewer: Warning"

	set msg {
    The VNC Viewer has exited.
    
    You will need to terminate STUNNEL manually.
    
    To do this go to the System Tray and right-click on the STUNNEL
    icon (dark green).  Then click "Exit".
    
    You can also double click on the STUNNEL icon to view the log
    for error messages and other information.
}
	.w.t insert end $msg
}

proc win_kill_msg {pids} {
	global terminate_pids
	global help_font
	toplev .w

	eval text .w.t -width 72 -height 19 $help_font
	button .w.d -text "Dismiss" -command {destroy .w; set terminate_pids no}
	button .w.k -text "Terminate STUNNEL" -command {destroy .w; set terminate_pids yes}
	pack .w.t .w.k .w.d -side top -fill x

	apply_bg .w.t

	center_win .w
	wm resizable .w 1 0

	wm title .w "SSL/SSH Viewer: Warning"

	set msg {
    The VNC Viewer has exited.
    
    We can terminate the following still running STUNNEL process(es):
    
}
	append msg "         $pids\n"

	append msg {
    Click on the "Terminate STUNNEL" button below to do so.
    
    Before terminating STUNNEL you can double click on the STUNNEL
    Tray icon to view its log for error messages and other information.

    Note: You may STILL need to terminate STUNNEL manually if we are
    unable to kill it.  To do this go to the System Tray and right-click
    on the STUNNEL icon (dark green).  Then click "Exit".  You will
    probably also need to hover the mouse over the STUNNEL Tray Icon to
    make the Tray notice STUNNEL is gone...
}
	.w.t insert end $msg
}

proc win9x_plink_msg {file} {
	global help_font win9x_plink_msg_done
	toplev .pl

	eval text .pl.t -width 90 -height 26 $help_font
	button .pl.d -text "OK" -command {destroy .pl; set win9x_plink_msg_done 1}
	wm protocol .pl WM_DELETE_WINDOW {catch {destroy .pl}; set win9x_plink_msg_done 1}
	pack .pl.t .pl.d -side top -fill x

	apply_bg .pl.t

	center_win .pl
	wm resizable .pl 1 0

	wm title .pl "SSL/SSH Viewer: Win9x Warning"

	set msg {
    Due to limitations on Window 9x you will have to manually start up
    a COMMAND.COM terminal and paste in the following command:

}
	set pwd [pwd]
	regsub -all {/} $pwd "\\" pwd
	append msg "        $pwd\\$file\n"  

	append msg {
    The reason for this is a poor Console application implementation that
    affects many text based applications.
    
    To start up a COMMAND.COM terminal, click on the Start -> Run, and then
    type COMMAND in the entry box and hit Return or click OK.

    To select the above command, highlight it with the mouse and then press
    Ctrl-C.  Then go over to the COMMAND.COM window and click on the
    Clipboard paste button.  Once pasted in, press Return to run the script.
    
    This will start up a PLINK.EXE ssh login to the remote computer,
    and after you log in successfully and indicate (QUICKLY!!) that the
    connection is OK by clicking OK in this dialog. If the SSH connection
    cannot be autodetected you will ALSO need to click "Success" in the
    "plink ssh status?" dialog, the VNC Viewer will be started going
    through the SSH tunnel.
}
	.pl.t insert end $msg
	wm deiconify .pl
}

proc mesg {str} {
	set maxx 53
	if {[string length $str] > $maxx} {
		set str [string range $str 0 $maxx]
		append str " ..."
	}
	.l configure -text $str
	update
}

proc get_ssh_hp {str} {
	regsub {cmd=.*$} $str "" str
	set str [string trim $str]
	regsub {[ 	].*$} $str "" str
	return $str
}

proc get_ssh_cmd {str} {
	set str [string trim $str]
	if [regexp {cmd=(.*$)} $str m cmd] {
		set cmd [string trim $cmd]
		regsub -nocase {^%x11vncr$} $cmd "x11vnc -nopw -display none -rawfb rand" cmd
		regsub -nocase {^%x11vnc$}  $cmd "x11vnc -nopw -display none -rawfb null" cmd
		return $cmd
	} else {
		return ""
	}
}

proc get_ssh_proxy {str} {
	set str [string trim $str]
	regsub {cmd=.*$} $str "" str
	set str [string trim $str]
	if { ![regexp {[ 	]} $str]} {
		return ""
	}
	regsub {^.*[ 	][ 	]*} $str "" str
	return $str
}

proc set_defaults {} {
	global defs

	global mycert svcert crtdir
	global use_alpha use_grab use_ssl use_ssh use_sshssl use_viewonly use_fullscreen use_bgr233
	global use_nojpeg use_raise_on_beep use_compresslevel use_quality
	global compresslevel_text quality_text
	global use_cups use_sound use_smbmnt
	global cups_local_server cups_remote_port cups_manage_rcfile
	global cups_local_smb_server cups_remote_smb_port
	global change_vncviewer change_vncviewer_path vncviewer_realvnc4
	global additional_port_redirs additional_port_redirs_list
	global sound_daemon_remote_cmd sound_daemon_remote_port sound_daemon_kill sound_daemon_restart
	global sound_daemon_local_cmd sound_daemon_local_port sound_daemon_local_kill sound_daemon_local_start 
	global smb_su_mode smb_mount_list
	global use_port_knocking port_knocking_list
	global ycrop_string use_listen
	global include_list

	set defs(use_viewonly) 0
	set defs(use_listen) 0
	set defs(use_fullscreen) 0
	set defs(use_raise_on_beep) 0
	set defs(use_bgr233) 0
	set defs(use_alpha) 0
	set defs(use_grab) 0
	set defs(use_nojpeg) 0
	set defs(use_compresslevel) "default"
	set defs(use_quality) "default"
	set defs(compresslevel_text) "Compress Level: default"
	set defs(quality_text) "Quality: default"

	set defs(mycert) ""
	set defs(svcert) ""
	set defs(crtdir) "ACCEPTED_CERTS"

	set defs(use_cups) 0
	set defs(use_sound) 0
	set defs(use_smbmnt) 0

	set defs(change_vncviewer) 0 
	set defs(change_vncviewer_path) "" 
	set defs(cups_manage_rcfile) 0 
	set defs(vncviewer_realvnc4) 0

	set defs(additional_port_redirs) 0
	set defs(additional_port_redirs_list) ""

	set defs(cups_local_server) ""
	set defs(cups_remote_port) ""
	set defs(cups_local_smb_server) ""
	set defs(cups_remote_smb_port) ""

	set defs(smb_su_mode) "su"
	set defs(smb_mount_list) ""

	set defs(sound_daemon_remote_cmd) ""
	set defs(sound_daemon_remote_port) ""
	set defs(sound_daemon_kill) 0
	set defs(sound_daemon_restart) 0

	set defs(sound_daemon_local_cmd) ""
	set defs(sound_daemon_local_port) ""
	set defs(sound_daemon_local_start) 0
	set defs(sound_daemon_local_kill) 0

	set defs(use_port_knocking) 0
	set defs(ycrop_string) ""
	set defs(port_knocking_list) ""

	set defs(include_list) ""

	set defs(use_ssl) 1
	set defs(use_ssh) 0
	set defs(use_sshssl) 0

	foreach var [array names defs] {
		set $var $defs($var)	
	}

	global vncauth_passwd
	set vncauth_passwd ""

	ssl_ssh_adjust ssl
	listen_adjust
}

proc do_viewer_windows {n} {
	global use_alpha use_grab use_ssh use_sshssl use_viewonly use_fullscreen use_bgr233
	global use_nojpeg use_raise_on_beep use_compresslevel use_quality
	global change_vncviewer change_vncviewer_path vncviewer_realvnc4
	global use_listen

	set cmd "vncviewer"
	if {$change_vncviewer && $change_vncviewer_path != ""} {
		set cmd [string trim $change_vncviewer_path]
		regsub -all {\\} $cmd {/} cmd
		if {[regexp {[ \t]} $cmd]} {
			if {[regexp -nocase {\.exe$} $cmd]} {
				if {! [regexp {["']} $cmd]} { #"
					# hmmm, not following instructions, are they?
					set cmd "\"$cmd\""
				}
			}
		}
	}
	if {$use_viewonly} {
		if {$vncviewer_realvnc4} {
			append cmd " viewonly=1"
		} else {
			append cmd " /viewonly"
		}
	}
	if {$use_fullscreen} {
		if {$vncviewer_realvnc4} {
			append cmd " fullscreen=1"
		} else {
			append cmd " /fullscreen"
		}
	}
	if {$use_bgr233} {
		if {$vncviewer_realvnc4} {
			append cmd " lowcolourlevel=1"
		} else {
			append cmd " /8bit"
		}
	}
	if {$use_nojpeg} {
		if {! $vncviewer_realvnc4} {
			append cmd " /nojpeg"
		}
	}
	if {$use_raise_on_beep} {
		if {! $vncviewer_realvnc4} {
			append cmd " /belldeiconify"
		}
	}
	if {$use_compresslevel != "" && $use_compresslevel != "default"} {
		if {$vncviewer_realvnc4} {
			append cmd " zliblevel=$use_compresslevel"
		} else {
			append cmd " /compresslevel $use_compresslevel"
		}
	}
	if {$use_quality != "" && $use_quality != "default"} {
		if {! $vncviewer_realvnc4} {
			append cmd " /quality $use_quality"
		}
	}
	if {$use_listen} {
		if {$vncviewer_realvnc4} {
			append cmd " listen=1"
		} else {
			append cmd " /listen"
		}
		set nn $n
		if {$nn < 100} {
			set nn [expr "$nn + 5500"] 
		}
		append cmd " $nn"
		global did_listening_message
		if {$did_listening_message < 3} {
			incr did_listening_message
			global listening_name

			set ln $listening_name
			if {$ln == ""} {
				set ln "this-computer:$n"
			}

			set msg "
   About to start the Listening VNC Viewer.

   VNC Viewer command to be run:

       $cmd

   The VNC server should then Reverse connect to:

       $ln

   To stop the Viewer: right click on the VNC Icon in the tray
   and select 'Close listening daemon' (or similar).

   You will then return to this GUI.

   Click OK now to start the Listening VNC Viewer.
"
			global use_ssh use_sshssl
			if {$use_ssh || $use_sshssl} {
				set msg "${msg}   NOTE: You will probably also need to kill the SSH in the\n   terminal via Ctrl-C" 
			}

			global help_font is_windows system_button_face
			toplev .wll
			global wll_done

			set wll_done 0

			eval text .wll.t -width 60 -height 19 $help_font
			button .wll.d -text "OK" -command {destroy .wll; set wll_done 1}
			pack .wll.t .wll.d -side top -fill x

			apply_bg .wll.t

			center_win .wll
			wm resizable .wll 1 0

			wm title .wll "SSL/SSH Viewer: Listening VNC Info"

			.wll.t insert end $msg

			vwait wll_done
		}
	} else {
		if [regexp {^[0-9][0-9]*$} $n] {
			append cmd " localhost:$n"
		} else {
			append cmd " $n"
		}
	}
	
	mesg $cmd
	set emess ""
	set rc [catch {eval exec $cmd} emess]
	if {$rc != 0} {
		raise .
		tk_messageBox -type ok -icon error -message $emess -title "Error: $cmd"
	}
}

proc get_netstat {} {
	set ns ""
	catch {set ns [exec netstat -an]}
	return $ns
}

proc get_ipconfig {} {
	global is_win9x
	set ip ""
	if {! $is_win9x} {
		catch {set ip [exec ipconfig]}
		return $ip
	}

	set file "ip"
	append file [pid]
	append file ".txt"

	# VF
	catch {[exec winipcfg /Batch $file]}

	if [file exists $file] {
		set fh [open $file "r"]
		while {[gets $fh line] > -1} {
			append ip "$line\n"
		}
		close $fh
		catch {file delete $file}
	}
	return $ip
}

proc guess_nat_ip {} {
	global save_nat last_save_nat
	set s ""

	if {! [info exists save_nat]} {
		set save_nat ""
		set last_save_nat 0
	}
	if {$save_nat != ""} {
		set now [clock seconds]
		if {$now < $last_save_nat + 45} {
			return $save_nat
		}
	}
	set s ""
	catch {set s [socket "www.whatismyip.com" 80]}
	set ip "unknown"
	if {$s != ""} {
		fconfigure $s -buffering none
		puts $s "GET / HTTP/1.1"
		puts $s "Host: www.whatismyip.com"
		puts $s "Connection: close"
		puts $s ""
		flush $s
		set on 0
		while { [gets $s line] > -1 } {
			if {! $on && [regexp {<HEAD>}  $line]} {set on 1}
			if {! $on && [regexp {<HTML>}  $line]} {set on 1}
			if {! $on && [regexp {<TITLE>} $line]} {set on 1}
			if {! $on} {
				continue;
			}
			if [regexp {([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*)} $line ip] {
				break
			}
		}
		close $s
	}
	if {$ip != "unknown"} {
		set save_nat $ip
		set last_save_nat [clock seconds]
	}
	return $ip
}

proc guess_ip {} {
	global is_windows
	if {! $is_windows} {
		set out ""
		set out [get_hostname]
		if {$out != ""} {
			set hout ""
			catch {set hout [exec host $out]}
			if {$hout != ""} {
				if [regexp {has address ([.0-9][.0-9]*)} $hout mvar ip] {
					set ip [string trim $ip]
					return $ip
				}
			}
		}
		return ""
	} else {
		set out [get_ipconfig]
		set out [string trim $out]
		if {$out == ""} {
			return ""
		}
		foreach line [split $out "\n\r"] {
			if {[regexp -nocase {IP Address.*:[ \t]*([.0-9][.0-9]*)} $line mvar ip]} {
				set ip [string trim $ip]
				if [regexp {^[.0]*$} $ip] {
					continue
				}
				if [regexp {127\.0\.0\.1} $ip] {
					continue
				}
				if {$ip != ""} {
					return $ip
				}
			}
		}
	}
}

proc windows_start_sound_daemon {file} {
	global env
	global use_sound sound_daemon_local_cmd sound_daemon_local_start

	# VF
	regsub {\.bat} $file "snd.bat" file2
	set fh2 [open $file2 "w"]

	puts $fh2 $sound_daemon_local_cmd
	puts $fh2 "del $file2"
	close $fh2

	mesg "Starting SOUND daemon..."
	if [info exists env(COMSPEC)] {
		exec $env(COMSPEC) /c $file2 &
	} else {
		exec cmd.exe /c $file2 &
	}
	after 1500
}

proc windows_stop_sound_daemon {} {
	global is_win9x
	global use_sound sound_daemon_local_cmd sound_daemon_local_start

	set cmd [string trim $sound_daemon_local_cmd]

	regsub {[ \t].*$} $cmd "" cmd
	regsub {^.*\\} $cmd "" cmd
	regsub {^.*/} $cmd "" cmd

	if {$cmd == ""} {
		return
	}

	set output [get_task_list]
	
	foreach line [split $output "\n\r"] {
		if [regexp "$cmd" $line] {
			if [regexp {(-?[0-9][0-9]*)} $line m p] {
				set pids($p) $line
			}
		}
	}

	set count 0
	foreach pid [array names pids] {
		mesg "Stopping SOUND pid: $pid"
		if {$is_win9x} {
			catch {exec w98/kill.exe /f $pid}
		} else {
			catch {exec tskill.exe $pid}
		}
		if {$count == 0} {
			after 1200
		} else {
			after 500
		}
		incr count
	}
}

proc contag {} {
	global concount
	if {! [info exists concount]} {
		set concount 0
	}
	incr concount
	set str [pid]
	set str "-$str-$concount"
}

proc make_plink {} {
	toplev .plink
	#wm geometry .plink +700+500
	wm geometry .plink -40-40
	wm title .plink "plink SSH status?"
	set wd 37
	label .plink.l1 -anchor w -text "Login via plink/ssh to the remote server" -width $wd
	label .plink.l2 -anchor w -text "(supply username and password as needed)." -width $wd
	label .plink.l3 -anchor w -text "" -width $wd
	label .plink.l4 -anchor w -text "After ssh is set up, AND if the connection" -width $wd
	label .plink.l5 -anchor w -text "success is not autodetected, please click" -width $wd
	label .plink.l6 -anchor w -text "one of these buttons:" -width $wd
	global plink_status
	button .plink.fail -text "Failed" -command {destroy .plink; set plink_status no}
	button .plink.ok   -text "Success" -command {destroy .plink; set plink_status yes}
	pack .plink.l1 .plink.l2 .plink.l3 .plink.l4 .plink.l5 .plink.l6 .plink.fail .plink.ok -side top -fill x

	update
}

proc ssh_split {str} {
	if {! [regexp {:} $str]} {
		append str ":22"
	}
	regsub {:.*$} $str "" ssh_host
	regsub {^.*:} $str "" ssh_port
	if {$ssh_port == ""} {
		set ssh_port 22
	}
	if [regexp {@} $ssh_host] {
		regsub {@.*$} $ssh_host "" ssh_user
		regsub {^.*@} $ssh_host "" ssh_host
	} else {
		set ssh_user ""
	}
	return [list $ssh_user $ssh_host $ssh_port]
}

proc launch_windows_ssh {hp file n} {
	global is_win9x env
	global use_sshssl use_ssh putty_pw
	global port_knocking_list
	global use_listen listening_name

	set hpnew  [get_ssh_hp $hp]
	set proxy  [get_ssh_proxy $hp]
	set sshcmd [get_ssh_cmd $hp]

	set vnc_host "localhost"
	set vnc_disp $hpnew
	regsub {^.*:} $vnc_disp "" vnc_disp

	if {![regexp {^-?[0-9][0-9]*$} $vnc_disp]} {
		if {[regexp {cmd=SHELL} $hp]} {
			;
		} elseif {[regexp {cmd=PUTTY} $hp]} {
			;
		} else {
			mesg "Bad vncdisp, missing :0 ?, $vnc_disp"
			bell
			return 0
		}
	}

	if {$use_listen} {
		set vnc_port 5500
	} else {
		set vnc_port 5900
	}
	if {[regexp {^-[0-9][0-9]*$} $vnc_disp]} {
		set vnc_port [expr "- $vnc_disp"]
	} elseif {![regexp {^[0-9][0-9]*$} $vnc_disp]} {
		;
	} elseif {$vnc_disp < 200} {
		if {$use_listen} {
			set vnc_port [expr $vnc_disp + 5500]
		} else {
			set vnc_port [expr $vnc_disp + 5900]
		}
	} else {
		set vnc_port $vnc_disp
	}

	set ssh_port 22
	set ssh_host $hpnew
	regsub {:.*$} $ssh_host "" ssh_host

	set double_ssh ""
	set p_port ""
	if {$proxy != ""} {
		if [regexp {,} $proxy] {
			if {$is_win9x} {
				mesg "Double proxy does not work on Win9x"
				bell
				return 0
			}
			# user1@gateway:port1,user2@workstation:port2
			set proxy1 ""
			set proxy2 ""
			set s [split $proxy ","]
			set proxy1 [lindex $s 0]
			set proxy2 [lindex $s 1]

			set p_port [expr 3000 + 1000 * rand()]	
			set p_port [expr round($p_port)]

			set s [ssh_split $proxy1]
			set ssh_user1 [lindex $s 0]
			set ssh_host1 [lindex $s 1]
			set ssh_port1 [lindex $s 2]

			set s [ssh_split $proxy2]
			set ssh_user2 [lindex $s 0]
			set ssh_host2 [lindex $s 1]
			set ssh_port2 [lindex $s 2]

			set u1 ""
			if {$ssh_user1 != ""} {
				set u1 "${ssh_user1}@"
			}
			set u2 ""
			if {$ssh_user2 != ""} {
				set u2 "${ssh_user2}@"
			}
		
			set double_ssh "-L $p_port:$ssh_host2:$ssh_port2 -P $ssh_port1 $u1$ssh_host1"
			set proxy_use "${u2}localhost:$p_port"

		} else {
			# user1@gateway:port1
			set proxy_use $proxy
		}

		set ssh_host $proxy_use
		regsub {:.*$} $ssh_host "" ssh_host
		set ssh_port $proxy_use
		regsub {^.*:} $ssh_port "" ssh_port
		if {$ssh_port == ""} {
			set ssh_port 22
		}

		set vnc_host $hpnew
		regsub {:.*$} $vnc_host "" vnc_host
		if {$vnc_host == ""} {
			set vnc_host "localhost"
		}
	}

	if {![regexp {^[^ 	][^ 	]*@} $ssh_host]} {
		mesg "You must supply a username: user@host..."
		bell
		return 0
	}

	set verb "-v"

	set pwd ""
	if {$is_win9x} {
		set pwd [pwd]
		regsub -all {/} $pwd "\\" pwd
	}
	if {! [regexp {^[0-9][0-9]*$} $n]} {
		set n 0
	}

	if {$use_listen} {
		set use [expr $n + 5500]
	} else {
		set use [expr $n + 5900]
	}

	set_smb_mounts
	
	global use_smbmnt use_sound sound_daemon_kill 
	set do_pre 0
	if {$use_smbmnt}  {
		set do_pre 1
	} elseif {$use_sound && $sound_daemon_kill} {
		set do_pre 1
	}

	global skip_pre
	if {$skip_pre} {
		set do_pre 0
		set skip_pre 0
	}

	set pw ""
	if {$putty_pw != ""} {
		if {! [regexp {"} $putty_pw]} {  #"
			set pw "                                                      -pw                                                   \"$putty_pw\""
		}
	}

	set tag [contag]

	set file_double ""

	set file_pre ""
	set file_pre_cmd ""
	if {$do_pre} {
		set setup_cmds [ugly_setup_scripts pre $tag] 
		
		if {$setup_cmds != ""} {
			# VF
			regsub {\.bat} $file "pre.cmd" file_pre_cmd
			set fh [open $file_pre_cmd "w"]
			puts $fh "$setup_cmds sleep 10; "
			close $fh

			# VF
			regsub {\.bat} $file "pre.bat" file_pre
			set fh [open $file_pre "w"]
			set plink_str "plink.exe -ssh -C -P $ssh_port -m $file_pre_cmd $verb -t" 

			global smb_redir_0
			if {$smb_redir_0 != ""} {
				append plink_str " $smb_redir_0"
			}

			append plink_str "$pw $ssh_host" 

			if {$pw != ""} {
				puts $fh "echo off"
			}
			puts $fh $plink_str

			if {$file_pre_cmd != ""} {
				puts $fh "del $file_pre_cmd"
			}
			puts $fh "del $file_pre"

			close $fh
		}
	}

	if {$is_win9x} {
		set sleep 35
	} else {
		set sleep 20
	}
	if {$use_listen} {
		set sleep 1800
	}

	set setup_cmds [ugly_setup_scripts post $tag] 

	set do_shell 0
	if {$sshcmd == "SHELL"} {
		set setup_cmds ""
		set sshcmd {$SHELL}
		set do_shell 1
	} elseif {$sshcmd == "PUTTY"} {
		set setup_cmds ""
		set do_shell 1
	}

	set file_cmd ""
	if {$setup_cmds != ""} {
		# VF
		regsub {\.bat} $file ".cmd" file_cmd
		set fh_cmd [open $file_cmd "w"]

		set str $setup_cmds
		if {$sshcmd != ""} {
			append str " $sshcmd; "
		} else {
			append str " sleep $sleep; "
		}
		puts $fh_cmd $str
		close $fh_cmd

		set sshcmd $setup_cmds
	}

	if {$sshcmd == ""} {
		set pcmd "echo; echo SSH connected OK.; echo If this state is not autodetected,; echo Go Click the Success button."
		set sshcmd "$pcmd; sleep $sleep"
	}

	global use_sound sound_daemon_local_cmd sound_daemon_local_start
	if {! $do_shell && ! $is_win9x && $use_sound && $sound_daemon_local_start && $sound_daemon_local_cmd != ""} {
		windows_start_sound_daemon $file
	}

	# VF
	set fh [open $file "w"]
	if {$is_win9x} {
		puts $fh "cd $pwd"
		if {$file_pre != ""} {
			puts $fh "echo Press Ctrl-C --HERE-- when done with the Pre-Command shell work."
			puts $fh "start /w command.com /c $file_pre"
		}
	}

	global use_cups use_smbmnt
	set extra_redirs ""
	if {$use_cups} {
		append extra_redirs [get_cups_redir]
	}
	if {$use_sound} {
		append extra_redirs [get_sound_redir]
	}
	global additional_port_redirs
	if {$additional_port_redirs} {
		append extra_redirs [get_additional_redir]
	}

	if {$vnc_host == ""} {
		set vnc_host "localhost"
	}

	set redir "-L $use:$vnc_host:$vnc_port"
	if {$use_listen} {
		set redir "-R $vnc_port:$vnc_host:$use"
		set listening_name "localhost:$vnc_port  (on remote SSH side)"
	}

	set plink_str "plink.exe -ssh -P $ssh_port $verb $redir $extra_redirs -t" 
	if {$extra_redirs != ""} {
		regsub {exe} $plink_str "exe -C" plink_str
	}
	if {$do_shell} {
		if {$sshcmd == "PUTTY"} {
		    if {$is_win9x} {
			set plink_str "putty.exe -ssh -C -P $ssh_port $extra_redirs -t $pw $ssh_host" 
		    } else {
			set plink_str "start \"putty $ssh_host\" putty.exe -ssh -C -P $ssh_port $extra_redirs -t $pw $ssh_host" 
			if [regexp {FINISH} $port_knocking_list] {
				regsub {start} $plink_str "start /wait" plink_str
			}
		    }
		} else {
			set plink_str "plink.exe -ssh -C -P $ssh_port $extra_redirs -t $pw $ssh_host" 
			append plink_str { "$SHELL"}
		}
	} elseif {$file_cmd != ""} {
		append plink_str " -m $file_cmd$pw $ssh_host"
	} else {
		append plink_str "$pw $ssh_host \"$sshcmd\""
	}

	if {$pw != ""} {
		puts $fh "echo off"
	}
	puts $fh $plink_str
	if {$file_cmd != ""} {
		puts $fh "del $file_cmd"
	}
	puts $fh "del $file"
	close $fh

	catch {destroy .o}
	catch {destroy .oa}

	if { ![do_port_knock $ssh_host start]} {
		catch {file delete $file}
		if {$file_cmd != ""} {
			catch {file delete $file_cmd}
		}
		if {$file_pre != ""} {
			catch {file delete $file_pre}
		}
		return 0
	}

	if {$double_ssh != ""} {
		set plink_str_double_ssh "plink.exe -ssh -t $pw $double_ssh \"echo sleep 60 ...; sleep 60; echo done.\"" 

		# VF
		regsub {\.bat} $file "dob.bat" file_double
		set fhdouble [open $file_double "w"]
		puts $fhdouble $plink_str_double_ssh
		puts $fhdouble "del $file_double"
		close $fhdouble

		set com "cmd.exe"
		if [info exists env(COMSPEC)] {
			set com $env(COMSPEC)
		}

		exec $com /c $file_double &

		set waited 0
		set gotit 0
		while {$waited < 30000} {
			after 500
			update
			if {$use_listen} {
				set gotit 1
				break;
			}
			set ns [get_netstat]
			set re ":$p_port"
			append re {[ 	][ 	]*[0:.][0:.]*[ 	][ 	]*LISTEN}
			if [regexp $re $ns] {
				set gotit 1
				break
			}
			set waited [expr "$waited + 500"]
		}
		if {! $gotit} {
			after 5000
		}
	}

	if {$is_win9x} {
		wm withdraw .
		update
		win9x_plink_msg $file
		global win9x_plink_msg_done
		set win9x_plink_msg_done 0
		vwait win9x_plink_msg_done
	} else {
		set com "cmd.exe"
		if [info exists env(COMSPEC)] {
			set com $env(COMSPEC)
		}

		if {$file_pre != ""} {
			set sl 0
			if {$use_smbmnt}  {
				global smb_su_mode
				if {$smb_su_mode == "su"} {
					set sl [expr $sl + 15]
				} elseif {$smb_su_mode == "sudo"} {
					set sl [expr $sl + 15]
				} else {
					set sl [expr $sl + 3]
				}
			}
			if {$pw == ""} {
				set sl [expr $sl + 5]
			}

			set sl [expr $sl + 5]
			set st [clock seconds]
			set dt 0
			global entered_gui_top button_gui_top
			set entered_gui_top 0
			set button_gui_top 0

			catch {wm geometry . "-40-40"}
			catch {wm withdraw .; update; wm deiconify .; raise .; update}
			mesg "Click on *This* Label when done with 1st SSH 0/$sl"
			after 600

			exec $com /c $file_pre &

			catch {lower .; update; raise .; update}

			while {$dt < $sl} {
				after 100
				set dt [clock seconds]
				set dt [expr $dt - $st]
				mesg "Click on *This* Label when done with 1st SSH $dt/$sl"
				update
				update idletasks
				if {$dt <= 1} {
					set button_gui_top 0
				}
				if {$button_gui_top != 0 && $dt >= 3} {
					mesg "Running 2nd SSH now ..."
					after 1000
					break
				}
			}
			mesg "Running 2nd SSH ..."
		}

		if {! $do_shell} {
			make_plink
		}
		wm withdraw .
		update
		if {$do_shell && [regexp {FINISH} $port_knocking_list]} {
			catch {exec $com /c $file}
		} else {
			exec $com /c $file &
		}
		after 1000
	}

	if {$do_shell} {
		wm deiconify .
		update
		if {[regexp {FINISH} $port_knocking_list]} {
			do_port_knock $ssh_host finish
		}
		return 1
	}
	if {$is_win9x} {
		make_plink
	}
	global plink_status
	set plink_status ""
	set waited 0
	set cnt 0
	while {$waited < 30000} {
		after 500
		update
		if {$use_listen} {
			set plink_status yes
			break;
		}
		set ns [get_netstat]
		set re ":$use"
		append re {[ 	][ 	]*[0:.][0:.]*[ 	][ 	]*LISTEN}
		if [regexp $re $ns] {
			set plink_status yes
		}
		if {$plink_status != ""} {
			catch {destroy .plink}
			break
		}

		if {$waited == 0} {
			#wm deiconify .plink
		}
		set waited [expr "$waited + 500"]

		incr cnt
		if {$cnt >= 12} {
			set cnt 0
		}
	}
	if {$plink_status == ""} {
		vwait plink_status
	}

	if {$use_sshssl} {
		global launch_windows_ssh_files 
		if {$file != ""} {
			append launch_windows_ssh_files "$file "
		}
		if {$file_pre != ""} {
			append launch_windows_ssh_files "$file_pre "
		}
		if {$file_pre_cmd != ""} {
			append launch_windows_ssh_files "$file_pre_cmd "
		}
		regsub { *$} $launch_windows_ssh_files "" launch_windows_ssh_files
		return 1
	}

	if {$plink_status != "yes"} {
		wm deiconify .
	} else {
		after 1000
		do_viewer_windows $n
		wm deiconify .
		mesg "Disconnected from $hp"
	}
	update
	if [regexp {FINISH} $port_knocking_list] {
		do_port_knock $ssh_host finish
	}

	if {$file != ""} {
		catch {file delete $file}	
	}
	if {$file_pre != ""} {
		catch {file delete $file_pre}	
	}
	if {$file_pre_cmd != ""} {
		catch {file delete $file_pre_cmd}	
	}
	if {$file_double != ""} {
		catch {file delete $file_double}	
	}

	global sound_daemon_local_kill
	if {! $is_win9x && $use_sound && $sound_daemon_local_kill && $sound_daemon_local_cmd != ""} {
		windows_stop_sound_daemon
	}
	return 1
}

proc check_ssh_needed {} {
	globalize
	
	if {$use_ssh || $use_sshssl} {
		return
	}
	set must_cups 0
	set must_snd 0
	set must_smb 0
	set must_addl 0
	if {$use_cups} {
		if {$cups_local_server != ""} {set must_cups 1}
		if {$cups_remote_port != ""} {set must_cups 1}
		if {$cups_local_smb_server != ""} {set must_cups 1}
		if {$cups_remote_smb_port != ""} {set must_cups 1}
		if {$cups_manage_rcfile != ""} {set must_cups 1}
	}
	if {$use_sound} {
		if {$sound_daemon_remote_cmd != ""} {set must_snd 1}
		if {$sound_daemon_remote_port != ""} {set must_snd 1}
		if {$sound_daemon_kill} {set must_snd 1}
		if {$sound_daemon_restart} {set must_snd 1}
		if {$sound_daemon_local_cmd != ""} {set must_snd 1}
		if {$sound_daemon_local_port != ""} {set must_snd 1}
		if {$sound_daemon_local_kill} {set must_snd 1}
		if {$sound_daemon_local_start} {set must_snd 1}
	}
	if {$use_smbmnt} {
		if {[regexp {//} $smb_mount_list]} {set must_smb 1}
	}
	if {$additional_port_redirs} {
		set must_addl 1
	}
	if {$must_cups || $must_snd || $must_smb || $must_addl} {
		mesg "Cannot do Port redirs in non-SSH mode (SSL)"
		set msg ""
		if {$must_smb} {
			append msg "  - SMB Mount Port Redirection\n"
		}
		if {$must_snd} {
			append msg "  - ESD Sound Port Redirection\n"
		}
		if {$must_cups} {
			append msg "  - CUPS Port Redirection\n"
		}
		if {$must_addl} {
			append msg "  - Additional Port Redirections\n"
		}
                set msg "\"Use SSL\" mode selected (no SSH)\nThe following options will be disabled:\n\n$msg"
		bell
		update
		raise .
                tk_messageBox -type ok -icon info -message $msg
	}
}

proc set_smb_mounts {} {
	global smb_redir_0 smb_mounts use_smbmnt 
	
	set smb_redir_0 ""
	set smb_mounts ""
	if {$use_smbmnt} {
		set l2 [get_smb_redir]
		set smb_redir_0 [lindex $l2 0]
		set smb_redir_0 [string trim $smb_redir_0]
		set smb_mounts  [lindex $l2 1]
	}
}

proc darwin_terminal_cmd {{title ""} {cmd ""} {bg 0}} {
	global darwin_terminal

	set tries ""
	lappend tries "/Applications/Utilities/Terminal.app/Contents/MacOS/Terminal"

	if {! [info exists darwin_terminal]} {
		foreach try $tries {
			if [file exists $try] {
				if [file executable $try] {
					set darwin_terminal $try
					break
				}
			}
		}
		if {! [info exists darwin_terminal]} {
			set fh ""
			catch {set fh [open "| find /Applications -type f -name Terminal" "r"]}
			if {$fh != ""} {
				while {[gets $fh line] > -1} {
					if {! [file exists $line]} {
						continue
					}
					if {[file isdirectory $line]} {
						continue
					}
					if {! [regexp {/Terminal$} $line]} {
						continue
					}
					if {! [file executable $line]} {
						continue
					}
					set darwin_terminal $line
					break
				}
				close $fh
			}
		}
	}
	if {! [info exists darwin_terminal]} {
		raise .
		tk_messageBox -type ok -icon error -message "Cannot find Darwin Terminal program." -title "Cannot find Terminal program"
		return
	}

	global darwin_terminal_cnt
	set tmp /tmp/darwin_terminal_cmd.[pid]
	if {! [info exists darwin_terminal_cnt]} {
		set darwin_terminal_cnt 0
	}
	incr darwin_terminal_cnt
	append tmp ".$darwin_terminal_cnt"
	
	set fh ""
	catch {set fh [open $tmp w 0755]}
	if {$fh == ""} {
		raise .
		tk_messageBox -type ok -icon error -message "Cannot open temporary file: $tmp" -title "Cannot open file"
		return
	}
	global env
	puts $fh "#!/bin/sh"
	puts $fh "PATH=$env(PATH)"
	puts $fh "export PATH"
	puts $fh "tmp=$tmp"
	puts $fh "sleep 1"
	puts $fh {if [ "X$DDDBG" != "X" ]; then ps www; fi}
	puts $fh {termpid=`ps www | grep -w Terminal | grep $tmp | grep -v grep | awk '{print $1}' | sort -n | tail -1`}
	puts $fh {echo try-1: termpid=$termpid mypid=$$}
	puts $fh {if [ "X$termpid" = "X" ]; then}
	puts $fh {	termpid=`ps www | grep -w Terminal | grep -v grep | awk '{print $1}' | sort -n | tail -1`}
	puts $fh {	echo try-2: termpid=$termpid mypid=$$}
	puts $fh {fi}
	puts $fh {if [ "X$termpid" = "X" ]; then}
	puts $fh {	termpid=`ps wwwwaux | grep -w Terminal | grep $tmp | grep -v grep | awk '{print $2}' | sort -n | tail -1`}
	puts $fh {	echo try-3: termpid=$termpid mypid=$$}
	puts $fh {fi}
	puts $fh {if [ "X$termpid" = "X" ]; then}
	puts $fh {	termpid=$$}
	puts $fh {	echo termpid-find-fail: termpid=$termpid mypid=$$}
	puts $fh {fi}
	puts $fh {trap "rm -f $tmp; kill -TERM $termpid; kill -TERM $mypid; kill -KILL $mypid; exit 0" 0 2 15}
	puts $fh "$cmd"
	puts $fh "sleep 1"
	puts $fh {rm -f $tmp}
	puts $fh {kill -TERM $termpid}
	puts $fh {kill -TERM $mypid}
	puts $fh {kill -KILL $mypid}
	puts $fh "exit 0"
	close $fh
	if {$bg} {
		catch {exec $darwin_terminal $tmp &}
	} else {
		catch {exec $darwin_terminal $tmp}
	}
}

proc unix_terminal_cmd {{geometry "+100+100"} {title "xterm-command"} {cmd "echo test"} {bg 0} {xrm1 ""} {xrm2 ""} {xrm3 ""}} {
	global uname
	if {$uname == "Darwin"} {
		global env
		set doX  0;
		if [info exists env(DISPLAY)] {
			if {[in_path "xterm"] != ""} {
				set doX 1
			}
		}
		if {! $doX} {
			darwin_terminal_cmd $title $cmd $bg
			return
		}
	}
	if {$bg} {
		if {$xrm1 == ""} {
			exec xterm -geometry "$geometry" -title "$title" -e sh -c "$cmd" 2>@stdout &
		} else {
			exec xterm -geometry "$geometry" -title "$title" -xrm "$xrm1" -xrm "$xrm2" -xrm "$xrm3" -e sh -c "$cmd" 2>@stdout &
		}
	} else {
		if {$xrm1 == ""} {
			exec xterm -geometry "$geometry" -title "$title" -e sh -c "$cmd" 2>@stdout
		} else {
			exec xterm -geometry "$geometry" -title "$title" -xrm "$xrm1" -xrm "$xrm2" -xrm "$xrm3" -e sh -c "$cmd" 2>@stdout
		}
	}
}

proc xterm_center_geometry {} {
	set sh [winfo screenheight .]
	set sw [winfo screenwidth .]
	set gw 500
	set gh 300
	set x [expr $sw/2 - $gw/2]
	set y [expr $sh/2 - $gh/2]
	if {$x < 0} {
		set x 10
	}
	if {$y < 0} {
		set y 10
	}

	return "+$x+$y"
}

proc smbmnt_wait {tee} {
	if {$tee != ""} {
		set start [clock seconds]
		set cut 30
		while {1} {
			set now [clock seconds]
			if {$now > $start + $cut} {
				break;
			}
			if [file exists $tee] {
				set sz 0
				catch {set sz [file size $tee]}
				if {$sz > 50} {
					set cut 50
				}
			}
			set g ""
			catch {set g [exec grep vnc-helper-exiting $tee]}
			if [regexp {vnc-helper-exiting} $g] {
				break
			}
			after 1000
		}
		catch {file delete $tee}
	} else {
		global smb_su_mode
		if {$smb_su_mode == "su"} {
			after 15000
		} elseif {$smb_su_mode == "sudo"} {
			after 10000
		}
	}
}

proc do_unix_pre {tag proxy hp pk_hp}  {
	global env smb_redir_0 use_smbmnt
	global did_port_knock
	
	set setup_cmds [ugly_setup_scripts pre $tag] 
	set c "ss_vncviewer -ssh"

	if {$proxy == ""} {
		set pxy $hp
		regsub {:.*$} $pxy "" pxy
		set c "$c -proxy '$pxy'"
	} else {
		set c "$c -proxy '$proxy'"
	}

	if {$setup_cmds != ""} {
		set env(SS_VNCVIEWER_SSH_CMD) "$setup_cmds sleep 10"
		set env(SS_VNCVIEWER_SSH_ONLY) 1
		if {$smb_redir_0 != ""} {
			set c "$c -sshargs '$smb_redir_0'"
		}

		if {! [do_port_knock $pk_hp start]} {
			return
		}
		set did_port_knock 1

		if {$use_smbmnt} {
			set title "SSL/SSH VNC Viewer $hp -- SMB MOUNTS"
		} else {
			set title "SSL/SSH VNC Viewer $hp -- Pre Commands"
		}

		set tee ""
		if {$use_smbmnt} {
			set tee $env(SSVNC_HOME) 
			append tee "/.tee-etv$tag"
			set fh ""
			catch {set fh [open $tee "w"]}
			if {$fh == ""} {
				set tee ""
			} else {
				close $fh
				set c "$c | tee $tee"
			}
		}

		unix_terminal_cmd "80x25+100+100" "$title" "set -xv; $c" 1

		set env(SS_VNCVIEWER_SSH_CMD) ""
		set env(SS_VNCVIEWER_SSH_ONLY) ""

		if {$use_smbmnt} {
			smbmnt_wait $tee
		} else {
			after 2000
		}
	}
}
proc init_vncdisplay {} {
	global vncdisplay vncproxy remote_ssh_cmd
	set vncdisplay [string trim $vncdisplay] 

	if {$vncdisplay == ""} {
		set vncproxy ""
		set remote_ssh_cmd ""
		return
	}

	set hpnew  [get_ssh_hp $vncdisplay]
	set proxy  [get_ssh_proxy $vncdisplay]
	set sshcmd [get_ssh_cmd $vncdisplay]

	set vncdisplay $hpnew
	set vncproxy $proxy
	set remote_ssh_cmd $sshcmd

	if {$sshcmd != ""} {
		global use_ssl use_ssh use_sshssl
		set use_ssl 0
		if {! $use_ssh && ! $use_sshssl} {
			set use_ssh 1
		}
	}
	# ssl_ssh_adjust will be called.
}

proc get_vncdisplay {} {
	global vncdisplay vncproxy remote_ssh_cmd
	set vncdisplay [string trim $vncdisplay]

	set t $vncdisplay
	regsub {[ \t]*cmd=.*$} $t "" t
	set t [string trim $t]
	
	set str ""
	if [regexp {[ \t]} $t] {
		set str $t
	} else {
		if {$vncproxy != "" && $t == ""} {
			set str "--nohost-- $vncproxy"
		} else {
			set str "$t $vncproxy"
		}
	}
	if [regexp {cmd=.*$} $vncdisplay match] {
		if {$str == ""} {
			set str "--nohost--"
		}
		set str "$str $match"
	} else {
		if {$remote_ssh_cmd != ""} {
			if {$str == ""} {
				set str "--nohost--"
			}
			set str "$str cmd=$remote_ssh_cmd"
		}
	}
	set str [string trim $str]
	return $str
}

proc port_knock_only {hp {mode KNOCK}} {
	if {$hp == ""} {
		set hp [get_vncdisplay]
		if {$hp == ""} {
			mesg "No host port found"
			bell
			return
		}
	}
	set hpnew  [get_ssh_hp $hp]
	set proxy  [get_ssh_proxy $hp]
	set sshcmd [get_ssh_cmd $hp]
	set hp $hpnew

	set pk_hp ""
	if {$proxy != ""} {
		set pk_hp $proxy
	}
	if {$pk_hp == ""} {
		set pk_hp $hp
	}
	if {$mode == "KNOCK"} {
		do_port_knock $pk_hp start
	} elseif {$mode == "FINISH"} {
		do_port_knock $pk_hp finish
	}
}

proc direct_connect_msg {} {
	set msg ""
	global env
	globalize
	if {$use_sshssl} {
		append msg "  - SSH + SSL tunnelling\n"
	} elseif {$use_ssh} {
		append msg "  - SSH tunnelling\n"
	} else {
		append msg "  - SSL tunnelling\n"
	}
	if [info exists env(SSVNC_NO_ENC_WARN)] {
		set msg ""
	}
	if {$use_smbmnt} {
		append msg "  - SMB Mount Port Redirection\n"
	}
	if {$use_sound} {
		append msg "  - ESD Sound Port Redirection\n"
	}
	if {$use_cups} {
		append msg "  - CUPS Port Redirection\n"
	}
	if {$additional_port_redirs} {
		append msg "  - Additional Port Redirections\n"
	}
	if {$mycert != "" || $svcert != "" || $crtdir != ""} {
		append msg "  - SSL certificate authentication\n"
	}
	if {$msg != ""} {
		set msg "Direct connect via vnc://hostname\nThe following options will be disabled:\n\n$msg"
		global is_windows
		if {0 && $is_windows} {
			set msg "Direct connect mode: vnc://host:disp is not supported on Windows."
		}
		raise .
		tk_messageBox -type ok -icon info -message $msg
	}
}

proc fetch_cert {save} {
	global vncdisplay is_windows
	set hp [get_vncdisplay]

	regsub {[ 	]*cmd=.*$} $hp "" tt
	if {[regexp {^[ 	]*$} $tt]} {
		mesg "No host:disp supplied."
		bell
		catch {raise .}
		return
	}
	if {[regexp -- {--nohost--} $tt]} {
		mesg "No host:disp supplied."
		bell
		catch {raise .}
		return
	}
	if {! [regexp ":" $hp]} {
		if {! [regexp {cmd=} $hp]} {
			append hp ":0"
		}
	}
	set hpnew  [get_ssh_hp $hp]
	set proxy  [get_ssh_proxy $hp]

	mesg "Fetching $hpnew Cert..."
	global cert_text
	set cert_text ""
	.f4.getcert configure -state disabled
	update
	if {$is_windows} {
		set cert_text [fetch_cert_windows $hp]
	} else {
		catch {set cert_text [fetch_cert_unix $hp]}
	}
	.f4.getcert configure -state normal
	mesg "Fetched $hpnew Cert"

	set n 47
	set ok 1
	if {$cert_text == ""} {
		set cert_text "An Error occurred in fetching SSL Certificate from $hp"
		set ok 0
		set n 4
	} elseif {! [regexp {BEGIN CERTIFICATE} $cert_text]} {
		set cert_text "An Error occurred in fetching $hp\n\n$cert_text"
		set ok 0
	} else {
		set text "" 
		set on 0
		foreach line [split $cert_text "\n"] {
			if [regexp -- {-----BEGIN CERTIFICATE-----} $line] {
				set on 1
			}
			if {! $on} {
				continue;
			}
			append text "$line\n"
			if [regexp -- {-----END CERTIFICATE-----} $line] {
				set on 0
			}
		}
		global is_windows
		set tmp "/tmp/cert.hsh.[pid]"
		if {$is_windows} {
			# VF
			set tmp cert.hsh
		}
		set fh ""
		catch {set fh [open $tmp "w"]}
		if {$fh != ""} {
			puts $fh $text
			close $fh
			set info ""
			catch {set info [get_x509_info $tmp]}
			catch {file delete $tmp}
			if [regexp -nocase {MD5 Finger[^\n]*} $info mvar] {
				set cert_text "$mvar\n\n$cert_text"
			}
			if [regexp -nocase {SHA. Finger[^\n]*} $info mvar] {
				set cert_text "$mvar\n\n$cert_text"
			}
		}
		set cert_text "SSL Certificate from $hp\n\n$cert_text"
	}

	if {! $save} {
		return $cert_text
	}

	fetch_dialog $cert_text $hp $hpnew $ok $n
}
	
proc fetch_dialog {cert_text hp hpnew ok n} {
	toplev .fetch

	scroll_text_dismiss .fetch.f 90 $n

	if {$ok} {
		button .fetch.save -text Save -command "destroy .fetch; save_cert $hpnew"
		button .fetch.help -text Help -command "help_fetch_cert"
		pack .fetch.help .fetch.save -side bottom -fill x
	}

	center_win .fetch
	wm title .fetch "$hp Certificate"

	.fetch.f.t insert end $cert_text
	jiggle_text .fetch.f.t
}

proc fetch_cert_unix {hp} {
	set hpnew  [get_ssh_hp $hp]
	set proxy  [get_ssh_proxy $hp]
	if {$proxy != ""} {
		return [exec ss_vncviewer -proxy $proxy -showcert $hpnew 2>/dev/null]
	} else {
		return [exec ss_vncviewer -showcert $hpnew]
	}
}

proc fetch_cert_windows {hp} {

	regsub {^vnc.*://} $hp "" hp

	set hpnew  [get_ssh_hp $hp]
	set proxy  [get_ssh_proxy $hp]

	set list [split $hpnew ":"] 

	set host [lindex $list 0]
	if {$host == ""} {
		set host "localhost"
	}

	if [regexp {^.*@} $host match] {
		mesg "Trimming \"$match\" from hostname"
		regsub {^.*@} $host "" host
	}

	set disp [lindex $list 1]
	set disp [string trim $disp]
	regsub { .*$} $disp "" disp

	if {[regexp {^-[0-9][0-9]*$} $disp]} {
		;
	} elseif {$disp == "" || ! [regexp {^[0-9][0-9]*$} $disp]} {
		set disp 0
	}
	if {$disp < 0} {
		set port [expr "- $disp"]
	} elseif {$disp < 200} {
		set port [expr "$disp + 5900"]
	} else {
		set port $disp
	}

	if {$proxy != ""} {
		global env

		set port2 5991
		set env(SSVNC_PROXY) $proxy
		set env(SSVNC_LISTEN) $port2
		set env(SSVNC_DEST) "$host:$port"

		set host localhost
		set port $port2
		mesg "Starting TCP helper on port $port2 ..."
		after 600
		set proxy_pid [exec "connect_br.exe" &]
		unset -nocomplain env(SSVNC_PROXY)
		unset -nocomplain env(SSVNC_LISTEN)
		unset -nocomplain env(SSVNC_DEST)
	}

	set ossl [get_openssl]
	update
	# VF
	set tin tmpin.txt
	set tou tmpout.txt
	set fh ""
	catch {set fh [open $tin "w"]}
	if {$fh != ""} {
		puts $fh "Q"
		puts $fh "GET /WOMBAT HTTP/1.1\r\nHost: wombat.com\r\n\r\n\r\n"
		close $fh
	}
	if {1} {
		set ph ""
		set ph [open "| $ossl s_client -connect $host:$port < $tin 2>NUL" "r"]
#		set ph [open "| $ossl s_client -connect $host:$port" "r"]
		set text ""
		if {$ph != ""} {
			set pids [pid $ph]
			set got 0
			while {[gets $ph line] > -1} {
				append text "$line\n"
				if [regexp {END CERT} $line] {
					set got 1
				}
				if {$got && [regexp {^ *Verify return code} $line]} {
					break
				}
				if [regexp {^RFB } $line] {
					break
				}
				if [regexp {^DONE} $line] {
					break
				}
			}
			foreach pid $pids {
				global is_win9x
				if {$pid == ""} {
					;
				} elseif {$is_win9x} {
					catch {exec w98/kill.exe /f $pid}
				} else {
					catch {exec tskill.exe $pid}
				}
			}
			catch {close $ph}
			catch {file delete $tin $tou}
			return $text
		}
	} else {
		set pids ""
if {1} {
		set ph2 [open "| $ossl s_client -connect $host:$port > $tou 2>NUL" "w"]
		set pids [pid $ph2]
		after 500
		for {set i 0} {$i < 128} {incr i} {
			puts $ph2 "Q"
		}
		catch {close $ph2}
	
} else {
		set pids [exec $ossl s_client -connect $host:$port < $tin >& $tou &]
}
		for {set i 0} {$i < 10} {incr i} {
			after 500
			set got 0
			set ph ""
			catch {set ph [open $tou "r"]}
			if {$ph != ""} {
				while {[gets $ph line] > -1} {
					if [regexp {END CERT} $line] {
						set got 1
						break
					}
				}
				close $ph
			}
			if {$got} {
				break
			}
		}
		global is_win9x
		foreach pid $pids {
			if {$pid == ""} {
				;
			} elseif {$is_win9x} {
				catch {exec w98/kill.exe /f $pid}
			} else {
				catch {exec tskill.exe $pid}
			}
		}
		after 500
		set ph ""
		catch {set ph [open $tou "r"]}
	}
	set text ""
	if {$ph != ""} {
		while {[gets $ph line] > -1} {
			append text "$line\n"
		}
		close $ph
	}
	catch {file delete $tin $tou}
	return $text
}

proc check_accepted_certs {} {
	global cert_text always_verify_ssl
	global skip_verify_accepted_certs

	if {! $always_verify_ssl} {
		set skip_verify_accepted_certs 1
		return 1
	}

	set cert_text [fetch_cert 0]


	set from ""
	set fingerprint ""
	set fingerline ""

	set i 0
	foreach line [split $cert_text "\n"] {
		incr i
		if {$i > 4} {
			break
		}
		if [regexp {^SSL Certificate from (.*)} $line mv str] {
			set from [string trim $str]
		}
		if [regexp -nocase {Fingerprint=(.*)} $line mv str] {
			set fingerline $line
			set fingerprint [string trim $str]
		}
	}

	set fingerprint [string tolower $fingerprint]
	regsub -all {:} $fingerprint "-" fingerprint
	regsub -all {[\\/=]} $fingerprint "_" fingerprint

	set from [string tolower $from]
	regsub -all {^[+a-z]*://} $from "" from
	regsub -all {:} $from "-" from
	regsub -all {[\\/=]} $from "_" from

	if {$from == "" || $fingerprint == ""} {
		bell
		catch {raise .; update}
		mesg "WARNING: Error fetching Server Cert"
		after 2000
		return 0
	}

	set hp [get_vncdisplay]

	set adir [get_idir_certs ""]
	catch {file mkdir $adir}
	set adir "$adir/accepted"
	catch {file mkdir $adir}

	set crt "$adir/$fingerprint=$from.crt"

	if [file exists $crt] {
		mesg "OK: Certificate found in ACCEPTED_CERTS"
		after 550
		return 1
	}

	set cnt 0
	foreach f [glob -nocomplain -directory $adir "$fingerprint=*"] {
		mesg "CERT: $f"
		after 150
		incr cnt
	}
	set oth 0
	set others [list]
	foreach f [glob -nocomplain -directory $adir "*=$from.crt"] {
		if {$f == $crt}  {
			continue
		}
		set fb [file tail $f]
		mesg "OTHER CERT: $fb"
		if {$cnt > 0} {
			after 400 
		} else {
			bell
			after 800 
		}
		lappend others $f
		incr oth
	}
	foreach f [glob -nocomplain -directory $adir "*.crt"] {
		if {$f == $crt}  {
			continue
		}
		set saw 0
		foreach o $others {
			if {$f == $o} 	{
				set saw 1
				break
			}
		}
		if {$saw} {
			continue
		}
		set fh [open $f "r"]
		if {$fh == ""} {
			continue
		}
		set same 0
		while {[gets $fh line] > -1} {
			if [regexp {^Host-Display: (.*)$} mv hd] {
				if {$hd == $hp || $hd == $from} {
					set same 1
				}
			}
		}
		close $fh;

		if {! $same} {
			continue
		}

		set fb [file tail $f]
		mesg "OTHER CERT: $fb"
		if {$cnt > 0} {
			after 400 
		} else {
			bell
			after 800 
		}
		lappend others $f
		incr oth
	}
	if {$cnt > 0} {
		mesg "OK: Certificate found in ACCEPTED_CERTS"
		after 300
		return 1
	}

	set hp2 [get_vncdisplay]
	set msg "
    The SSL Certificate from host:

        $hp2

    with fingerprint:

        $fingerprint

    is not present in the 'Accepted Certs' directory:

        $adir
%WARN
    You will need to verify on your own that this is a certificate from a
    VNC server that you trust (e.g. by checking the fingerprint with that
    sent to you by the server administrator).

    Should this certificate be saved in the accepted certs directory and
    then used to SSL authenticate VNC servers?

    By clicking 'Inspect and maybe Save Cert' you will be given the opportunity
    to inspect the certificate before deciding to save it or not.

    Choose 'Ignore Cert for One Connection' to connect one time to the
    server and not require any certificate verification.
"

	if {$oth == 0} {
		regsub {%WARN} $msg "" msg
	} else {
		set warn ""
		set wfp ""
		if {$oth == 1} {
			set warn "
**WARNING** The Following Cert was previously saved FOR THE SAME HOST-DISPLAY:

"
			set wfp "BUT WITH A DIFFERENT FINGERPRINT."
			
		} else {
			set warn "
**WARNING** The Following Certs were previously saved FOR THE SAME HOST-DISPLAY:

"
			set wfp "BUT WITH DIFFERENT FINGERPRINTS."
		}

		foreach o $others {
			set fb [file tail $o]
			set warn "$warn        $fb\n"
		}
		set warn "$warn\n    $wfp\n"
		set warn "$warn\n    This could be a Man-In-The-Middle attack, or simply that the Server changed"
		set warn "$warn\n    its Certificate.  *PLEASE CHECK* before proceeding!\n"
		regsub {%WARN} $msg $warn msg
		bell
	}

	set n 0
	foreach l [split $msg "\n"] {
		incr n
	}
	toplev .acert
	scroll_text .acert.f 83 $n

	button .acert.inspect -text "Inspect and maybe Save Cert ..." -command "destroy .acert; set accept_cert_dialog 1"
	button .acert.accept  -text "Ignore Cert for One Connection  " -command "destroy .acert; set accept_cert_dialog 2"
	button .acert.cancel -text "Cancel"   -command "destroy .acert; set accept_cert_dialog 0"

	wm title .acert "Unrecognized SSL Cert!"

	.acert.f.t insert end $msg

	pack .acert.cancel .acert.accept .acert.inspect -side bottom -fill x
	pack .acert.f -side top -fill both -expand 1

	center_win .acert

	global accept_cert_dialog
	set accept_cert_dialog ""

	jiggle_text .acert.f.t

	tkwait window .acert

	if {$accept_cert_dialog == 2} {
		set skip_verify_accepted_certs 1
		return 1
	}
	if {$accept_cert_dialog != 1} {
		return 0
	}

	global accepted_cert_dialog_in_progress
	set accepted_cert_dialog_in_progress 1

	global fetch_cert_filename
	set fetch_cert_filename $crt

	fetch_dialog $cert_text $hp $hp 1 47 

	catch {tkwait window .fetch}
	after 200
	catch {tkwait window .scrt}

	set fetch_cert_filename ""

	if [file exists $crt] {
		set ossl [get_openssl]
		set hash [exec $ossl x509 -hash -noout -in $crt]
		set hash [string trim $hash]
		if [regexp {^([0-9a-f][0-9a-f]*)} $hash mv h] {
			set hashfile "$adir/$h.0"
			if [file exists $hashfile] {
				set hashfile "$adir/$h.1"
			}
			set fh [open $crt "a"]
			if {$fh != ""} {
				puts $fh ""
				puts $fh "SSVNC info:"
				puts $fh "Host-Display: $hp"
				puts $fh "$fingerline"
				puts $fh "hash filename: $h.0"
				puts $fh "full filename: $fingerprint=$from.crt"
				close $fh
			}
			catch {file copy -force $crt $hashfile}
			if [file exists $hashfile] {
				return 1
			}
		}
	}

	return 0
}

proc launch_unix {hp} {
	global smb_redir_0 smb_mounts env
	global vncauth_passwd

	globalize

	set cmd ""

	if {[regexp {^vncssh://} $hp] || [regexp {^vnc\+ssh://} $hp]} {
		set use_ssl 0
		set use_ssh 1
		sync_use_ssl_ssh
	} elseif {[regexp {^vncs://} $hp] || [regexp {^vncssl://} $hp] || [regexp {^vnc\+ssl://} $hp]} {
		set use_ssl 1
		set use_ssh 0
		sync_use_ssl_ssh
	}

	check_ssh_needed

	set_smb_mounts

	global did_port_knock
	set did_port_knock 0
	set pk_hp ""

	set skip_ssh 0
	set do_direct 0
		
	if [regexp {vnc://} $hp] {
		set skip_ssh 1
		set do_direct 1
		if {! [info exists env(SSVNC_NO_ENC_WARN)]} {
			direct_connect_msg
		}
	}

	if {$use_ssh || $use_sshssl} {
		if {$skip_ssh} {
			set cmd "ss_vncviewer"
		} elseif {$use_ssh} {
			set cmd "ss_vncviewer -ssh"
		} else {
			set cmd "ss_vncviewer -sshssl"
			if {$mycert != ""} {
				set cmd "$cmd -mycert '$mycert'"
			}
			if {$svcert != ""} {
				set cmd "$cmd -verify '$svcert'"
			} elseif {$crtdir != "" && $crtdir != "ACCEPTED_CERTS"} {
				set cmd "$cmd -verify '$crtdir'"
			}
		}
		if {$use_listen} {
			set cmd "$cmd -listen"
		}
		set hpnew  [get_ssh_hp $hp]
		set proxy  [get_ssh_proxy $hp]
		set sshcmd [get_ssh_cmd $hp]
		set hp $hpnew

		if {$proxy != ""} {
			set cmd "$cmd -proxy '$proxy'"
			set pk_hp $proxy
		}
		if {$pk_hp == ""} {
			set pk_hp $hp
		}

		set do_pre 0
		if {$use_smbmnt}  {
			set do_pre 1
		} elseif {$use_sound && $sound_daemon_kill} {
			set do_pre 1
		}
		global skip_pre
		if {$skip_pre || $skip_ssh} {
			set do_pre 0
			set skip_pre 0
		}

		set tag [contag]

		if {$do_pre} {
			do_unix_pre $tag $proxy $hp $pk_hp
		}


		set setup_cmds [ugly_setup_scripts post $tag] 
		if {$skip_ssh} {
			set setup_cmds ""
		}

		if {$sshcmd == "SHELL"} {
			set env(SS_VNCVIEWER_SSH_CMD) {$SHELL}
			set env(SS_VNCVIEWER_SSH_ONLY) 1
		} elseif {$setup_cmds != ""} {
			set env(SS_VNCVIEWER_SSH_CMD) "$setup_cmds$sshcmd"
		} else {
			if {$sshcmd != ""} {
				set cmd "$cmd -sshcmd '$sshcmd'"
			}
		}
		
		set sshargs ""
		if {$use_cups} {
			append sshargs [get_cups_redir]
		}
		if {$use_sound} {
			append sshargs [get_sound_redir]
		}
		if {$additional_port_redirs} {
			append sshargs [get_additional_redir]
		}

		set sshargs [string trim $sshargs]
		if {$skip_ssh} {
			set sshargs ""
		}
		if {$sshargs != ""} {
			set cmd "$cmd -sshargs '$sshargs'"
			set env(SS_VNCVIEWER_USE_C) 1
		}
		if {$sshcmd == "SHELL"} {
			set env(SS_VNCVIEWER_SSH_ONLY) 1
			if {$proxy == ""} {
				set hpt $hpnew
				regsub {:[0-9]*$} $hpt "" hpt
				set cmd "$cmd -proxy '$hpt'"
			}
			set geometry [xterm_center_geometry]
			if {$pk_hp == ""} {
				set pk_hp $hp
			}
			if {! $did_port_knock} {
				if {! [do_port_knock $pk_hp start]} {
					return
				}
				set did_port_knock 1
			}

			if {[regexp {FINISH} $port_knocking_list]} {
				wm withdraw .
				update
				unix_terminal_cmd $geometry "SHELL to $hp" "$cmd"
				wm deiconify .
				update
				do_port_knock $pk_hp finish
			} else {
				unix_terminal_cmd $geometry "SHELL to $hp" "$cmd" 1
			}
			set env(SS_VNCVIEWER_SSH_CMD) ""
			set env(SS_VNCVIEWER_SSH_ONLY) ""
			set env(SS_VNCVIEWER_USE_C) ""
			return
		}
	} else {
		set cmd "ssvnc_cmd"
		set hpnew  [get_ssh_hp $hp]
		set proxy  [get_ssh_proxy $hp]
		if {! $do_direct && ![regexp -nocase {ssh://} $hpnew]} {
			if {$mycert != ""} {
				set cmd "$cmd -mycert '$mycert'"
			}
			if {$svcert != ""} {
				set cmd "$cmd -verify '$svcert'"
			} elseif {$crtdir != ""} {
				if {$crtdir == "ACCEPTED_CERTS"} {
					global skip_verify_accepted_certs
					set skip_verify_accepted_certs 0
					if {! [check_accepted_certs]} {
						return
					}
					if {! $skip_verify_accepted_certs} {
						set adir [get_idir_certs ""]
						set adir "$adir/accepted"
						catch {file mkdir $adir}
						set cmd "$cmd -verify '$adir'"
					}
				} else {
					set cmd "$cmd -verify '$crtdir'"
				}
			}
		}
		if {$proxy != ""} {
			set cmd "$cmd -proxy '$proxy'"
		}
		set hp $hpnew
		if [regexp {^.*@} $hp match] {
			catch {raise .; update}
			mesg "Trimming \"$match\" from hostname"
			after 1000
			regsub {^.*@} $hp "" hp
		}
		if [regexp {@} $proxy] {
			bell
			catch {raise .; update}
			mesg "WARNING: SSL proxy contains \"@\" sign"
			after 2000
		}
	}

	if {$use_alpha} {
		set cmd "$cmd -alpha"
	}
	if {$use_grab} {
		set cmd "$cmd -grab"
	}
	if {$use_listen} {
		set cmd "$cmd -listen"
	}

	global darwin_cotvnc
	if {$darwin_cotvnc} {
		set env(DARWIN_COTVNC) 1
	}

	set cmd "$cmd $hp"

	set do_vncspacewrapper 0
	if {$change_vncviewer && $change_vncviewer_path != ""} {
		set path [string trim $change_vncviewer_path]
		if [regexp {^["'].} $path]  {	# "
			set tmp "/tmp/vncspacewrapper."
			set do_vncspacewrapper 1
			append tmp [clock clicks -milliseconds]
			catch {file delete $tmp}
			if {[file exists $tmp]} {
				catch {destroy .c}
				mesg "file still exists: $tmp"
				bell
				return
			}
			catch {set fh [open $tmp "w"]}
			catch {exec chmod 700 $tmp}
			if {! [file exists $tmp]} {
				catch {destroy .c}
				mesg "cannot create: $tmp"
				bell
				return
			}
			puts $fh "#!/bin/sh"
			puts $fh "echo $tmp; set -xv"
			puts $fh "$path \"\$@\""
			puts $fh "sleep 1; rm -f $tmp"
			close $fh
			set path $tmp
		}
		set env(VNCVIEWERCMD) $path
	} else {
		if [info exists env(VNCVIEWERCMD_OVERRIDE)] {
			set env(VNCVIEWERCMD) $env(VNCVIEWERCMD_OVERRIDE)
		} else {
			set env(VNCVIEWERCMD) ""
		}
	}

	set realvnc4 $vncviewer_realvnc4
	set realvnc3 0
	set flavor ""
	if {! $darwin_cotvnc} {
		set done 0
		if {$do_vncspacewrapper} {
			if [regexp -nocase {ultra} $change_vncviewer_path] {
				set done 1
				set flavor "ultravnc"
			} elseif [regexp -nocase {chicken.of} $change_vncviewer_path] {
				set done 1
				set flavor "cotvnc"
			}
		}
		if {! $done} {
			catch {set flavor [exec ss_vncviewer -viewerflavor 2>/dev/null]}
		}
	}
	if [regexp {realvnc4} $flavor] {
		set realvnc4 1
	}
	if [regexp {tightvnc} $flavor] {
		set realvnc4 0
	}
	if [regexp {realvnc3} $flavor] {
		set realvnc4 0
		set realvnc3 1
	}

	set passwdfile ""
	if {$vncauth_passwd != ""} {
		global use_listen
		set passwdfile "$env(SSVNC_HOME)/.vncauth_tmp.[pid]"
		catch {exec vncstorepw $vncauth_passwd $passwdfile}
		catch {exec chmod 600 $passwdfile}
		if {$use_listen} {
			global env
			set env(SS_VNCVIEWER_RM) $passwdfile
		} else {
			catch {exec sh -c "sleep 15; rm $passwdfile 2>/dev/null" &}
		}
		if {$darwin_cotvnc} {
			set cmd "$cmd --PasswordFile $passwdfile"
		} else {
			set cmd "$cmd -passwd $passwdfile"
		}
	}

	if {$use_viewonly} {
		if {$darwin_cotvnc} {
			set cmd "$cmd --ViewOnly"
		} elseif {$flavor == "ultravnc"} {
			set cmd "$cmd /viewonly"
		} else {
			set cmd "$cmd -viewonly"
		}
	}
	if {$use_fullscreen} {
		if {$darwin_cotvnc} {
			set cmd "$cmd --FullScreen"
		} elseif {$flavor == "ultravnc"} {
			set cmd "$cmd /fullscreen"
		} else {
			set cmd "$cmd -fullscreen"
		}
	}
	if {$use_bgr233} {
		if {$realvnc4} {
			set cmd "$cmd -lowcolourlevel 1"
		} elseif {$flavor == "ultravnc"} {
			set cmd "$cmd /8bit"
		} else {
			set cmd "$cmd -bgr233"
		}
	}
	if {$use_nojpeg} {
		if {$darwin_cotvnc} {
			;
		} elseif {$flavor == "ultravnc"} {
			;
		} elseif {! $realvnc4 && ! $realvnc3} {
			set cmd "$cmd -nojpeg"
		}
	}
	if {! $use_raise_on_beep} {
		if {$darwin_cotvnc} {
			;
		} elseif {$flavor == "ultravnc"} {
			;
		} elseif {! $realvnc4 && ! $realvnc3} {
			set cmd "$cmd -noraiseonbeep"
		}
	}
	if {$use_compresslevel != "" && $use_compresslevel != "default"} {
		if {$realvnc3} {
			;
		} elseif {$flavor == "ultravnc"} {
			;
		} elseif {$realvnc4} {
			set cmd "$cmd -zliblevel '$use_compresslevel'"
		} else {
			set cmd "$cmd -compresslevel '$use_compresslevel'"
		}
	}
	if {$use_quality != "" && $use_quality != "default"} {
		if {$darwin_cotvnc} {
			;
		} elseif {$flavor == "ultravnc"} {
			;
		} elseif {! $realvnc4 && ! $realvnc3} {
			set cmd "$cmd -quality '$use_quality'"
		}
	}
	if {$use_ssh || $use_sshssl} {
		# realvnc4 -preferredencoding zrle
		if {$darwin_cotvnc} {
			;
		} elseif {$flavor == "ultravnc"} {
			;
		} elseif {$realvnc4} {
			set cmd "$cmd -preferredencoding zrle"
		} else {
			set cmd "$cmd -encodings 'copyrect tight zrle zlib hextile'"
		}
	}

	global ycrop_string
	catch {unset env(VNCVIEWER_SBWIDTH)}
	catch {unset env(VNCVIEWER_YCROP)}
	if {[info exists ycrop_string] && $ycrop_string != ""}  {
		set t $ycrop_string
		if [regexp {,sb=([0-9][0-9]*)} $t m mv1]  {
			set env(VNCVIEWER_SBWIDTH) $mv1
		}
		regsub {,sb=([0-9][0-9]*)} $t "" t
		if {$t != ""} {
			set env(VNCVIEWER_YCROP) $t
		}
		#catch {puts "VNCVIEWER_SBWIDTH $env(VNCVIEWER_SBWIDTH)"}
		#catch {puts "VNCVIEWER_YCROP   $env(VNCVIEWER_YCROP)"}
	}

	catch {destroy .o}
	catch {destroy .oa}
	update

	if {$use_sound && $sound_daemon_local_start && $sound_daemon_local_cmd != ""} {
		mesg "running: $sound_daemon_local_cmd"
		exec sh -c "$sound_daemon_local_cmd" >& /dev/null </dev/null &
		update
		after 500
	}

	if {$pk_hp == ""} {
		set pk_hp $hp
	}
	if {! $did_port_knock} {
		if {! [do_port_knock $pk_hp start]} {
			wm deiconify .
			update
			return
		}
		set did_port_knock 1
	}
	wm withdraw .
	update

	set geometry [xterm_center_geometry]
	set xrm1 "*.srinterCommand:true"
	set xrm2 $xrm1
	set xrm3 $xrm1
	if {[info exists env(SSVNC_GUI_CMD)]} {
		set xrm1 "*.printerCommand:env XTERM_PRINT=1 $env(SSVNC_GUI_CMD)"
		set xrm2 "XTerm*VT100*translations:#override Shift<Btn3Down>:print()\\nCtrl<Key>N:print()"
		set xrm3 "*mainMenu*print*Label:  New SSVNC_GUI"
	}
	set m "Done. You Can X-out or Ctrl-C this Terminal if you like.  Ctrl-\\\\ to pause."
	global uname
	if {$uname == "Darwin"} {
		regsub {X-out or } $m "" m
	}
	unix_terminal_cmd $geometry "SSL/SSH VNC Viewer $hp" \
	"set -xv; $cmd; set +xv; ulimit -c 0; trap 'printf \"Paused. Press Enter to exit:\"; read x' QUIT; echo; echo $m; echo; echo sleep 5; echo; sleep 6" 0 $xrm1 $xrm2 $xrm3

	set env(SS_VNCVIEWER_SSH_CMD) ""
	set env(SS_VNCVIEWER_USE_C) ""

	if {$use_sound && $sound_daemon_local_kill && $sound_daemon_local_cmd != ""} {
		set daemon [string trim $sound_daemon_local_cmd]
		regsub {^gw[ \t]*} $daemon "" daemon
		regsub {[ \t].*$} $daemon "" daemon
		regsub {^.*/} $daemon "" daemon
		mesg "killing sound daemon: $daemon"
		if {$daemon != ""} {
			catch {exec sh -c "killall $daemon"  >/dev/null 2>/dev/null </dev/null &}
			catch {exec sh -c "pkill -x $daemon" >/dev/null 2>/dev/null </dev/null &}
		}
	}
	if {$passwdfile != ""} {
		catch {file delete $passwdfile}
	}
	wm deiconify .
	mesg "Disconnected from $hp"
	if {[regexp {FINISH} $port_knocking_list]} {
		do_port_knock $pk_hp finish
	}
}

proc kill_stunnel {pids} {
	global is_win9x

	set count 0
	foreach pid $pids {
		mesg "killing STUNNEL pid: $pid"
		if {$is_win9x} {
			catch {exec w98/kill.exe /f $pid}
		} else {
			catch {exec tskill.exe $pid}
		}
		if {$count == 0} {
			after 1200
		} else {
			after 500
		}
		incr count
	}
}

proc get_task_list {} {
	global is_win9x
	
	set output1 ""
	set output2 ""
	if {! $is_win9x} {
		# try for tasklist on XP pro
		catch {set output1 [exec tasklist.exe]}
	}
	catch {set output2 [exec w98/tlist.exe]}

	set output $output1
	append output "\n"
	append output $output2

	return $output
}

proc note_stunnel_pids {when} {
	global is_win9x pids_before pids_after pids_new

	if {$when == "before"} {
		array unset pids_before
		array unset pids_after
		set pids_new {}
		set pids_before(none) "none"
		set pids_after(none)  "none"
	}

	set output [get_task_list]
	
	foreach line [split $output "\n\r"] {
		if [regexp -nocase {stunnel} $line] {
			if [regexp {(-?[0-9][0-9]*)} $line m p] {
				if {$when == "before"} {
					set pids_before($p) $line
				} else {
					set pids_after($p) $line
				}
			}
		}
	}
	if {$when == "after"} {
		foreach new [array names pids_after] {
			if {! [info exists pids_before($new)]} {
				lappend pids_new $new
			}
		}
	}
}

proc del_launch_windows_ssh_files {} {
	global launch_windows_ssh_files
	
	if {$launch_windows_ssh_files != ""} {
		foreach tf [split $launch_windows_ssh_files] {
			if {$tf == ""} {
				continue
			}
			catch {file delete $tf}
		}
	}
}

proc launch_shell_only {} {
	global is_windows
	global skip_pre
	global use_ssl use_ssh use_sshssl

	set hp [get_vncdisplay]
	regsub {cmd=.*$} $hp "" hp
	set hp [string trim $hp]
	if {$is_windows} {
		append hp " cmd=PUTTY"
	} else {
		append hp " cmd=SHELL"
	}
	set use_ssl_save $use_ssl
	set use_ssh_save $use_ssh
	set use_sshssl_save $use_sshssl
	set skip_pre 1
	if {! $use_ssh && ! $use_sshssl} {
		set use_ssh 1
		set use_ssl 1
	}
	launch $hp

	set use_ssl $use_ssl_save
	set use_ssh $use_ssh_save
	set use_sshssl $use_sshssl_save
}

proc launch {{hp ""}} {
	global tcl_platform is_windows
	global mycert svcert crtdir
	global pids_before pids_after pids_new
	global env
	global use_ssl use_ssh use_sshssl use_listen

	set debug 0
	if {$hp == ""} {
		set hp [get_vncdisplay]
	}

	if {[regexp {^HOME=} $hp] || [regexp {^SSVNC_HOME=} $hp]} {
		set t $hp
		regsub {^.*HOME=} $t "" t
		set env(SSVNC_HOME) $t
		mesg "set SSVNC_HOME to $t"
		return 0
	}

	regsub {[ 	]*cmd=.*$} $hp "" tt

	if {[regexp {^[ 	]*$} $tt]} {
		mesg "No host:disp supplied."
		bell
		catch {raise .}
		return
	}
	if {[regexp -- {--nohost--} $tt]} {
		mesg "No host:disp supplied."
		bell
		catch {raise .}
		return
	}
	if {! [regexp ":" $hp]} {
		if {! [regexp {cmd=} $hp]} {
			set s [string trim $hp]
			if {! [regexp { } $s]} {
				append hp ":0"
			} else {
				regsub { } $hp ":0 " hp
			}
		}
	}

	mesg "Using: $hp"
	after 600

	set sc [get_ssh_cmd $hp]
	if {[regexp {^KNOCK} $sc]} {
		if [regexp {^KNOCKF} $sc] {
			port_knock_only $hp "FINISH"
		} else {
			port_knock_only $hp "KNOCK"
		}
		return
	}

	if {$debug} {
		mesg "\"$tcl_platform(os)\" | \"$tcl_platform(osVersion)\""
		after 1000
	}

	if [regexp {V[Nn][Cc]://} $hp] {
		set env(SSVNC_NO_ENC_WARN) 1
		regsub {V[Nn][Cc]://} $hp "vnc://" hp
	}
	regsub -nocase {^vnc://}	$hp "vnc://" hp
	regsub -nocase {^vncs://}	$hp "vncs://" hp
	regsub -nocase {^vncssl://}	$hp "vncssl://" hp
	regsub -nocase {^vnc\+ssl://}	$hp "vnc+ssl://" hp
	regsub -nocase {^vncssh://}	$hp "vncssh://" hp
	regsub -nocase {^vnc\+ssh://}	$hp "vnc+ssh://" hp

	if {! $is_windows} {
		launch_unix $hp
		return
	}

	##############################################################
	# WINDOWS BELOW:

	if [regexp {^vnc://} $hp] {
		direct_connect_msg
		regsub {^vnc://} $hp "" hp
		direct_connect_windows $hp
		return
	} elseif [regexp {^vncs://} $hp] {
		set use_ssl 1
		set use_ssh 0
		regsub {^vncs://} $hp "" hp
		sync_use_ssl_ssh
	} elseif [regexp {^vncssl://} $hp] {
		set use_ssl 1
		set use_ssh 0
		regsub {^vncssl://} $hp "" hp
		sync_use_ssl_ssh
	} elseif [regexp {^vnc\+ssl://} $hp] {
		set use_ssl 1
		set use_ssh 0
		regsub {^vnc\+ssl://} $hp "" hp
		sync_use_ssl_ssh
	} elseif [regexp {^vncssh://} $hp] {
		set use_ssh 1
		set use_ssl 0
		regsub {vncssh://} $hp "" hp
		sync_use_ssl_ssh
	} elseif [regexp {^vnc\+ssh://} $hp] {
		set use_ssh 1
		set use_ssl 0
		regsub {^vnc\+ssh://} $hp "" hp
		sync_use_ssl_ssh
	}

	check_ssh_needed

	if {! $use_ssh} {
		if {$mycert != ""} {
			if {! [file exists $mycert]} {
				mesg "MyCert does not exist: $mycert"
				bell
				return
			}
		}
		if {$svcert != ""} {
			if {! [file exists $svcert]} {
				mesg "ServerCert does not exist: $svcert"
				bell
				return
			}
		} elseif {$crtdir != ""} {
			if {! [file exists $crtdir] && $crtdir != "ACCEPTED_CERTS"} {
				mesg "CertsDir does not exist: $crtdir"
				bell
				return
			}
		}
	}

	# VF
	set prefix "stunnel-vnc"
	set suffix "conf"
	if {$use_ssh || $use_sshssl} {
		set prefix "plink_vnc"
		set suffix "bat"
	}

	set file ""
	set n ""
	set file2 ""
	set n2 ""
	set now [clock seconds]

	set proxy [get_ssh_proxy $hp]
	if {$use_sshssl} {
		set proxy ""
	}

	for {set i 30} {$i < 90} {incr i}  {
		set try "$prefix-$i.$suffix"
		if {[file exists $try]}  {
			set mt [file mtime $try]
			set age [expr "$now - $mt"]
			set week [expr "7 * 3600 * 24"]
			if {$age > $week} {
				catch {file delete $file}
			}
		}
		if {! [file exists $try]}  {
			if {$use_sshssl || $proxy != ""} {
				if {$file != ""} {
					set file2 $try
					set n2 $i
					break
				}
			}
			set file $try
			set n $i
			if {! $use_sshssl && $proxy == ""} {
				break
			}
		}
	}

	if {$file == ""} {
		mesg "could not find free stunnel file"
		bell
		return
	}

	global launch_windows_ssh_files 
	set launch_windows_ssh_files ""

	set did_port_knock 0

	global listening_name
	set listening_name ""

	if {$use_sshssl} {
		set rc [launch_windows_ssh $hp $file2 $n2]
		if {$rc == 0} {
			catch {file delete $file}
			catch {file delete $file2}
			del_launch_windows_ssh_files
			return
		}
		set did_port_knock 1
	} elseif {$use_ssh} {
		launch_windows_ssh $hp $file $n
		return
	}

	set list [split $hp ":"] 

	set host [lindex $list 0]
	if {$host == ""} {
		set host "localhost"
	}

	if [regexp {^.*@} $host match] {
		catch {raise .; update}
		mesg "Trimming \"$match\" from hostname"
		after 1000
		regsub {^.*@} $host "" host
	}

	set disp [lindex $list 1]
	set disp [string trim $disp]
	regsub { .*$} $disp "" disp
	if {[regexp {^-[0-9][0-9]*$} $disp]} {
		;
	} elseif {$disp == "" || ! [regexp {^[0-9][0-9]*$} $disp]} {
		set disp 0
	}

	if {$disp < 0} {
		set port [expr "- $disp"]
	} elseif {$disp < 200} {
		if {$use_listen} {
			set port [expr "$disp + 5500"]
		} else {
			set port [expr "$disp + 5900"]
		}
	} else {
		set port $disp
	}

	if {$proxy != ""} {
		if [regexp {@} $proxy] {
			bell
			catch {raise .; update}
			mesg "WARNING: SSL proxy contains \"@\" sign"
			after 2000
		}
		set env(SSVNC_PROXY) $proxy
		set env(SSVNC_LISTEN) [expr "$n2 + 5900"]
		set env(SSVNC_DEST) "$host:$port"
	}

	if {$debug} {
		mesg "file: $file"
		after 1000
	}

	set fail 0

	set fh [open $file "w"]

	if {$use_listen} {
		puts $fh "client = no"
	} else {
		puts $fh "client = yes"
	}
	puts $fh "options = ALL"
	puts $fh "taskbar = yes"
	puts $fh "RNDbytes = 2048"
	puts $fh "RNDfile = bananarand.bin"
	puts $fh "RNDoverwrite = yes"
	puts $fh "debug = 6"

	if {$mycert != ""} {
		if {! [file exists $mycert]} {
			mesg "MyCert does not exist: $mycert"
			bell
			set fail 1
		}
		puts $fh "cert = $mycert"
	} elseif {$use_listen} {
		set dummy "dummy.pem"
		set dh [open $dummy "w"]
		puts $dh [dummy_cert]
		close $dh
		puts $fh "cert = $dummy"
	}
	if {$svcert != ""} {
		if {! [file exists $svcert]} {
			mesg "ServerCert does not exist: $svcert"
			bell
			set fail 1
		}
		puts $fh "CAfile = $svcert"
		puts $fh "verify = 2"
	} elseif {$crtdir != ""} {
		if {$crtdir == "ACCEPTED_CERTS"} {
			global skip_verify_accepted_certs
			set skip_verify_accepted_certs 0
			if {! [check_accepted_certs]} {
				set fail 1
			}
			if {! $skip_verify_accepted_certs} {
				set adir [get_idir_certs ""]
				set adir "$adir/accepted"
				catch {file mkdir $adir}
				puts $fh "CApath = $adir"
				puts $fh "verify = 2"
			}
		} else {
			if {! [file exists $crtdir]} {
				mesg "CertsDir does not exist: $crtdir"
				bell
				set fail 1
			}
			puts $fh "CApath = $crtdir"
			puts $fh "verify = 2"
		}
	}

	if {$n == ""} {
		set n 10
	}
	if {$n2 == ""} {
		set n2 11
	}
	puts $fh "\[vnc$n\]"
	set port2 ""
	if {! $use_listen} {
		set port2 [expr "$n + 5900"] 
		puts $fh "accept = localhost:$port2"

		if {$use_sshssl || $proxy != ""} {
			set port [expr "$n2 + 5900"]
			puts $fh "connect = localhost:$port"
		} else {
			puts $fh "connect = $host:$port"
		}
	} else {
		set port2 [expr "$n + 5500"] 
		set hloc ""
		if {$use_ssh} {
			set hloc "localhost:"
			set listening_name "localhost:$port  (on remote SSH side)"
		} else {
			set hn [get_hostname]
			if {$hn == ""} {
				set hn "this-computer"
			}
			set listening_name "$hn:$port  (or IP:$port, etc.)"
		}
		puts $fh "accept = $hloc$port"
		puts $fh "connect = localhost:$port2"
	}

	puts $fh "delay = no"
	puts $fh ""
	close $fh

	if {! $did_port_knock} {
		if {! [do_port_knock $host start]} {
			set fail 1
		}
		set did_port_knock 1
	}

	if {$fail} {
		catch {file delete $file}
		return
	}

	set proxy_pid ""
	if {$proxy != ""} {
		mesg "Starting TCP helper on port $port ..."
		after 600
		set proxy_pid [exec "connect_br.exe" &]
		unset -nocomplain env(SSVNC_PROXY)
		unset -nocomplain env(SSVNC_LISTEN)
		unset -nocomplain env(SSVNC_DEST)
	}

	mesg "Starting STUNNEL on port $port2 ..."
	after 600

	note_stunnel_pids "before"

	set pids [exec stunnel $file &]

	after 1300

	note_stunnel_pids "after"

	if {$debug} {
		after 1000
		mesg "pids $pids"
		after 1000
	} else {
		catch {destroy .o}
		catch {destroy .oa}
		wm withdraw .
	}

	do_viewer_windows $n

	del_launch_windows_ssh_files

	catch {file delete $file}

	if {$debug} {
		;
	} else {
		wm deiconify .
	}
	mesg "Disconnected from $hp."

	global port_knocking_list
	if [regexp {FINISH} $port_knocking_list] {
		do_port_knock $host finish
	}

	if {[llength $pids_new] > 0} {
		set plist [join $pids_new ", "]
		global terminate_pids
		set terminate_pids ""
		win_kill_msg $plist
		update
		vwait terminate_pids
		if {$terminate_pids == "yes"} {
			kill_stunnel $pids_new
		}
	} else {
		win_nokill_msg
	}
	mesg "Disconnected from $hp."

	global is_win9x use_sound sound_daemon_local_kill sound_daemon_local_cmd
	if {! $is_win9x && $use_sound && $sound_daemon_local_kill && $sound_daemon_local_cmd != ""} {
		windows_stop_sound_daemon
	}
}

proc direct_connect_windows {{hp ""}} {
	global tcl_platform is_windows
	global env use_listen

	set proxy [get_ssh_proxy $hp]

	set did_port_knock 0

	global listening_name
	set listening_name ""

	set list [split $hp ":"] 

	set host [lindex $list 0]
	if {$host == ""} {
		set host "localhost"
	}

	if [regexp {^.*@} $host match] {
		catch {raise .; update}
		mesg "Trimming \"$match\" from hostname"
		after 1000
		regsub {^.*@} $host "" host
	}

	set disp [lindex $list 1]
	set disp [string trim $disp]
	regsub { .*$} $disp "" disp
	if {[regexp {^-[0-9][0-9]*$} $disp]} {
		;
	} elseif {$disp == "" || ! [regexp {^[0-9][0-9]*$} $disp]} {
		set disp 0
	}

	if {$disp < 0} {
		set port [expr "- $disp"]
	} elseif {$disp < 200} {
		if {$use_listen} {
			set port [expr "$disp + 5500"]
		} else {
			set port [expr "$disp + 5900"]
		}
	} else {
		set port $disp
	}

	if {$proxy != ""} {
		if [regexp {@} $proxy] {
			bell
			catch {raise .; update}
			mesg "WARNING: SSL proxy contains \"@\" sign"
			after 2000
		}
		set n2 45

		set env(SSVNC_PROXY) $proxy
		set env(SSVNC_LISTEN) [expr "$n2 + 5900"]
		set env(SSVNC_DEST) "$host:$port"

		set port [expr $n2 + 5900]
		set host "localhost"
	}

	set fail 0
	if {! $did_port_knock} {
		if {! [do_port_knock $host start]} {
			set fail 1
		}
		set did_port_knock 1
	}

	if {$fail} {
		return
	}

	set proxy_pid ""
	if {$proxy != ""} {
		mesg "Starting TCP helper on port $port ..."
		after 600
		set proxy_pid [exec "connect_br.exe" &]
		unset -nocomplain env(SSVNC_PROXY)
		unset -nocomplain env(SSVNC_LISTEN)
		unset -nocomplain env(SSVNC_DEST)
	}

	catch {destroy .o}
	catch {destroy .oa}
	wm withdraw .

	if {$use_listen} {
		set n $port
		if {$n >= 5500} {
			set n [expr $n - 5500]
		}
		do_viewer_windows "$n"
	} else {
		if {$port >= 5900} {
			set port [expr $port - 5900]
		}
		do_viewer_windows "$host:$port"
	}

	wm deiconify .

	mesg "Disconnected from $hp."

	global port_knocking_list
	if [regexp {FINISH} $port_knocking_list] {
		do_port_knock $host finish
	}

	mesg "Disconnected from $hp."
}

proc get_idir_certs {str} {
	global is_windows env
	set idir ""
	if {$str != ""} {
		if [file isdirectory $str] {
			set idir $str
		} else {
			set idir [file dirname $str]
		}
		if {$is_windows} {
			regsub -all {\\} $idir "/" idir
			regsub -all {//*} $idir "/" idir
		}
	}
	if {$idir == ""} {
		if {$is_windows} {
			if [info exists env(SSVNC_HOME)] {
				set t "$env(SSVNC_HOME)/ss_vnc"	
				regsub -all {\\} $t "/" t
				regsub -all {//*} $t "/" t
				if {! [file isdirectory $t]} {
					catch {file mkdir $t}
				}
				set t "$env(SSVNC_HOME)/ss_vnc/certs"	
				regsub -all {\\} $t "/" t
				regsub -all {//*} $t "/" t
				if {! [file isdirectory $t]} {
					catch {file mkdir $t}
				}
				if [file isdirectory $t] {
					set idir $t
				}
			}
			if {$idir == ""} {
				set t [file dirname [pwd]]
				set t "$t/certs"
				if [file isdirectory $t] {
					set idir $t
				}
			}
		}
		if {$idir == ""} {
			if [info exists env(SSVNC_HOME)] {
				set t "$env(SSVNC_HOME)/.vnc/certs"	
				if [file isdirectory $t] {
					set idir $t
				}
			}
		}
	}
	if {$idir == ""} {
		if {$is_windows} {
			set idir [get_profiles_dir]
		}
		if {$idir == ""} {
			set idir [pwd]
		}
	}
	return $idir
}

proc set_mycert {{parent "."}} {
	global mycert
	set idir [get_idir_certs $mycert]
	set t ""
	if {$idir != ""} {
		set t [tk_getOpenFile -parent $parent -initialdir $idir]
	} else {
		set t [tk_getOpenFile -parent $parent]
	}
	if {$t != ""} {
		set mycert $t
	}
	catch {wm deiconify .c}
	v_mycert
	update
}


proc show_cert {crt} {
	if {$crt == ""} {
		bell
		return
	}
	if {! [file exists $crt]} {
		bell
		return
	}
	set info ""
	catch {set info [get_x509_info $crt]}
	if {$info == ""} {
		bell
		return
	}

	set w .show_certificate
	toplev $w
	scroll_text $w.f
	button $w.b -text Dismiss -command "destroy $w"
	bind $w <Escape> "destroy $w"
	$w.f.t insert end $info

	pack $w.b -side bottom -fill x
	pack $w.f -side top -fill both -expand 1
	center_win $w
	catch {raise $w}
}

proc v_svcert {} {
	global svcert
	if {$svcert == "" || ! [file exists $svcert]} {
		catch {.c.svcert.i configure -state disabled}
	} else {
		catch {.c.svcert.i configure -state normal}
	}
	return 1
}

proc v_mycert {} {
	global mycert
	if {$mycert == "" || ! [file exists $mycert]} {
		catch {.c.mycert.i configure -state disabled}
	} else {
		catch {.c.mycert.i configure -state normal}
	}
	return 1
}

proc show_mycert {} {
	global mycert
	show_cert $mycert
}

proc show_svcert {} {
	global svcert
	show_cert $svcert
}

proc set_svcert {{parent "."}} {
	global svcert crtdir
	set idir [get_idir_certs $svcert]
	set t ""
	if {$idir != ""} {
		set t [tk_getOpenFile -parent $parent -initialdir $idir]
	} else {
		set t [tk_getOpenFile -parent $parent]
	}
	if {$t != ""} {
		set crtdir ""
		set svcert $t
	}
	catch {wm deiconify .c}
	v_svcert
	update
}

proc set_crtdir {{parent "."}} {
	global svcert crtdir
	set idir ""
	if {$crtdir == "ACCEPTED_CERTS"} {
		set idir [get_idir_certs ""]
	} else {
		set idir [get_idir_certs $crtdir]
	}
	set t ""
	if {$idir != ""} {
		set t [tk_chooseDirectory -parent $parent -initialdir $idir]
	} else {
		set t [tk_chooseDirectory -parent $parent]
	}
	if {$t != ""} {
		set svcert ""
		set crtdir $t
	}
	catch {wm deiconify .c}
	update
}

proc set_createcert_file {} {
	global ccert
	if {[info exists ccert(FILE)]} {
		set idir [get_idir_certs $ccert(FILE)]
	}
	if {$idir != ""} {
		set t [tk_getSaveFile -parent .ccrt -defaultextension ".pem" -initialdir $idir]
	} else {
		set t [tk_getSaveFile -parent .ccrt -defaultextension ".pem"]
	}
	if {$t != ""} {
		set ccert(FILE) $t
	}
	catch {raise .ccrt}
	update
}

proc check_pp {} {
	global ccert
	if {$ccert(ENC)} {
		catch {.ccrt.pf.e configure -state normal}
		catch {focus .ccrt.pf.e}
		catch {.ccrt.pf.e icursor end}
	} else {
		catch {.ccrt.pf.e configure -state disabled}
	}
}

proc get_openssl {} {
	global is_windows
	if {$is_windows} {
		set ossl "openssl.exe"
	} else {
		set ossl "openssl"
	}
}

proc get_x509_info {crt} {
	set ossl [get_openssl]
	set info ""
	update
	set ph [open "| $ossl x509 -text -fingerprint -in \"$crt\"" "r"]
	while {[gets $ph line] > -1} {
		append info "$line\n"
	}
	close $ph
	return $info
}

proc do_oss_create {} {
	global is_windows is_win9x

	set cfg {
[ req ]
default_bits            = 2048
encrypt_key             = yes
distinguished_name      = req_distinguished_name

[ req_distinguished_name ]
countryName                     = Country Name (2 letter code)
countryName_default             = %CO
countryName_min                 = 2
countryName_max                 = 2

stateOrProvinceName             = State or Province Name (full name)
stateOrProvinceName_default     = %ST

localityName                    = Locality Name (eg, city)
localityName_default            = %LOC

0.organizationName              = Organization Name (eg, company)
0.organizationName_default      = %ON

organizationalUnitName          = Organizational Unit Name (eg, section)
organizationalUnitName_default  = %OUN

commonName                      = Common Name (eg, YOUR name)
commonName_default              = %CN
commonName_max                  = 64

emailAddress                    = Email Address
emailAddress_default            = %EM
emailAddress_max                = 64
}

	global ccert

	if {$ccert(FILE) == ""} {
		catch {destroy .c}
		mesg "No output cert file supplied"
		bell
		return
	}
	if {! [regexp {\.pem$} $ccert(FILE)]} {
		append ccert(FILE) ".pem"
	}
	set pem $ccert(FILE)
	regsub {\.pem$} $ccert(FILE) ".crt" crt

	if {$ccert(ENC)} {
		if {[string length $ccert(PASS)] < 4} {
			catch {destroy .c}
			mesg "Passphrase must be at least 4 characters long."
			bell
			return
		}
	}
	if {[string length $ccert(CO)] != 2} {
		catch {destroy .c}
		mesg "Country Name must be at exactly 2 characters long."
		bell
		return
	}
	if {[string length $ccert(CN)] > 64} {
		catch {destroy .c}
		mesg "Common Name must be less than 65 characters long."
		bell
		return
	}
	if {[string length $ccert(EM)] > 64} {
		catch {destroy .c}
		mesg "Email Address must be less than 65 characters long."
		bell
		return
	}
		
	foreach t {EM CN OUN ON LOC ST CO} {

		set val $ccert($t)
		if {$val == ""} {
			set val "none"
		}
		regsub "%$t" $cfg "$val" cfg
	}

	global is_windows

	if {$is_windows} {
		# VF
		set tmp "cert.cfg"
	} else {
		set tmp "/tmp/cert.cfg."
		append tmp [clock clicks -milliseconds]
		catch {file delete $tmp}
		if {[file exists $tmp]} {
			catch {destroy .c}
			mesg "file still exists: $tmp"
			bell
			return
		}
		catch {set fh [open $tmp "w"]}
		catch {exec chmod 600 $tmp}
		if {! [file exists $tmp]} {
			catch {destroy .c}
			mesg "cannot create: $tmp"
			bell
			return
		}
	}
	set fh ""
	catch {set fh [open $tmp "w"]}
	if {$fh == ""} {
		catch {destroy .c}
		mesg "cannot create: $tmp"
		bell
		catch {file delete $tmp}
		return
	}

	puts $fh $cfg
	close $fh

	set ossl [get_openssl]

	set cmd "$ossl req -config $tmp -nodes -new -newkey rsa:2048 -x509 -batch"
	if {$ccert(DAYS) != ""} {
		set cmd "$cmd -days $ccert(DAYS)"
	}
	if {$is_windows} {
		set cmd "$cmd -keyout {$pem} -out {$crt}"
	} else {
		set cmd "$cmd -keyout \"$pem\" -out \"$crt\""
	}

	if {$is_windows} {
		set emess ""
		if {$is_win9x} {
			catch {file delete $pem}
			catch {file delete $crt}
			update
			eval exec $cmd &
			catch {raise .}
			set sl 0
			set max 100
			#if {$ccert(ENC)} {
			#	set max 100
			#}
			set maxms [expr $max * 1000]
			while {$sl < $maxms} {
				set s2 [expr $sl / 1000]
				mesg "running openssl ... $s2/$max"
				if {[file exists $pem] && [file exists $crt]} {
					after 2000
					break
				}
				after 500
				set sl [expr $sl + 500]
			}
			mesg ""
		} else {
			update
			set rc [catch {eval exec $cmd} emess]
			if {$rc != 0 && [regexp -nocase {error:} $emess]} {
				raise .
				tk_messageBox -type ok -icon error -message $emess -title "OpenSSL req command failed"
				return
			}
		}
	} else {
		set geometry [xterm_center_geometry]
		update
		unix_terminal_cmd $geometry "Running OpenSSL" "$cmd"
		catch {file attributes $pem -permissions go-rw}
		catch {file attributes $crt -permissions go-w}
	}
	catch {file delete $tmp}

	set bad ""
	if {! [file exists $pem]} {
		set bad "$pem "
	}
	if {! [file exists $crt]} {
		set bad "$crt"
	}
	if {$bad != ""} {
		raise .
		tk_messageBox -type ok -icon error -message "Not created: $bad" -title "OpenSSL could not create cert"
		catch {raise .c}
		return
	}

	if {$ccert(ENC) && $ccert(PASS) != ""} {
		set cmd "$ossl rsa -in \"$pem\" -des3 -out \"$pem\" -passout stdin"
		set ph ""
		set emess ""
		update
		set rc [catch {set ph [open "| $cmd" "w"]} emess]
		if {$rc != 0 || $ph == ""} {
			raise .
			tk_messageBox -type ok -icon error -message $emess -title "Count not encrypt private key"
			catch {file delete $pem}
			catch {file delete $crt}
			return
		}
		puts $ph $ccert(PASS)
		set emess ""
		set rc [catch {close $ph} emess]
		#puts $emess
		#puts $rc
	}

	set in  [open $crt "r"]
	set out [open $pem "a"]
	while {[gets $in line] > -1} {
		puts $out $line
	}
	close $in
	close $out

	catch {raise .c}
	set p .
	if [winfo exists .c] {
		set p .c
	}
	
	set reply [tk_messageBox -parent $p -type yesno -title "View Cert" -message "View Certificate and Info?"]
	catch {raise .c}
	if {$reply == "yes"} {
		set w .view_cert
		toplev $w
		scroll_text $w.f
		set cert ""
		set fh ""
		catch {set fh [open $crt "r"]}
		if {$fh != ""} {
			while {[gets $fh line] > -1} {
				append cert "$line\n"
			}
			catch {close $fh}
		}

		global yegg
		set yegg ""
		button $w.b -text Dismiss -command "destroy $w; set yegg 1"
		pack $w.b -side bottom -fill x
		bind $w <Escape> "destroy $w; set yegg 1"
		
		$w.f.t insert end "\n"
		$w.f.t insert end "$crt:\n"
		$w.f.t insert end "\n"
		$w.f.t insert end $cert
		$w.f.t insert end "\n"

		set info [get_x509_info $crt]
		$w.f.t insert end $info

		pack $w.f -side top -fill both -expand 1
		center_win $w
		catch {raise $w}
		vwait yegg
		catch {raise .c}
	}

	set p .
	if [winfo exists .c] {
		set p .c
	}
	set reply [tk_messageBox -parent $p -type yesno -title "View Private Key" -message "View Private Key?"]
	catch {raise .c}
	if {$reply == "yes"} {
		set w .view_key
		toplev $w
		scroll_text $w.f
		set key ""
		set fh [open $pem "r"]
		while {[gets $fh line] > -1} {
			append key "$line\n"
		}
		close $fh

		global yegg
		set yegg ""
		button $w.b -text Dismiss -command "destroy $w; set yegg 1"
		pack $w.b -side bottom -fill x
		bind $w <Escape> "destroy $w; set yegg 1"
		
		$w.f.t insert end "\n"
		$w.f.t insert end "$pem:\n"
		$w.f.t insert end "\n"
		$w.f.t insert end $key
		$w.f.t insert end "\n"

		pack $w.f -side top -fill both -expand 1
		center_win $w
		catch {raise $w}
		vwait yegg
		catch {raise .c}
	}
}
	
proc create_cert {} {

	toplev .ccrt
	wm title .ccrt "Create SSL Certificate"

	global uname
	if {$uname == "Darwin"} {
		scroll_text .ccrt.f 80 20
	} else {
		scroll_text .ccrt.f 80 30
	}

	set msg {
    This dialog helps you to create a simple self-signed SSL certificate.  

    On Unix the openssl(1) program must be installed and in $PATH.
    On Windows, a copy of the openssl program is provided for convenience.

    The resulting certificate files can be used for either:

       1) authenticating yourself (VNC Viewer) to a VNC Server
    or 2) your verifying the identity of a remote VNC Server.

    In either case you will need to safely copy one of the generated
    certificate files to the remote VNC Server and have the VNC Server use
    it.  Or you could send it to the system administrator of the VNC Server.

    For the purpose of description, assume that the filename selected in the
    "Save to file" entry is "vnccert.pem".  That file will be generated
    by this process and so will the "vnccert.crt" file.  "vnccert.pem"
    contains both the Private Key and the Public Certificate.  "vnccert.crt"
    only contains the Public Certificate.

    For case 1) you would copy "vnccert.crt" to the VNC Server side and 
    instruct the server to use it.  For x11vnc it would be for example:

        x11vnc -sslverify /path/to/vnccert.crt -ssl SAVE ...

    (it is also possible to handle many client certs at once in a directory,
    see the -sslverify documentation).  Then you would use "vnccert.pem"
    as the MyCert entry in the SSL Certificates dialog.

    For case 2) you would copy "vnccert.pem" to the VNC Server side and 
    instruct the server to use it.  For x11vnc it would be for example:

        x11vnc -ssl /path/to/vnccert.pem

    Then you would use "vnccert.crt" as the as the ServerCert entry in the
    "SSL Certificates" dialog.


    Creating the Certificate:
    
    Choose a output filename (ending in .pem) in the "Save to file" entry.

    Then fill in the identification information (Country, State or Province,
    etc).

    The click on "Create" to generate the certificate files.

    Encrypting the Private Key:  It is a very good idea to encrypt the
    Private Key that goes in the "vnccert.pem".  The downside is that
    whenever that key is used (e.g. starting up x11vnc using it) then
    the passphrase will need to be created.  If you do not encrypt it and
    somebody steals a copy of the "vnccert.pem" file then they can pretend
    to be you.

    After you have created the certificate files, you must copy and import
    either "vnccert.pem" or "vnccert.pem" to the remote VNC Server and
    also select the other file in the "SSL Certificates" dialog.
    See the description above.

    For more information see:

           http://www.karlrunge.com/x11vnc/ssl.html
           http://www.karlrunge.com/x11vnc/#faq-ssl-tunnel-int

    The first one describes how to use x11vnc to create Certificate
    Authority (CA) certificates in addition to self-signed ones.


    Tip: if you choose the "Common Name" to be the internet hostname
    (e.g. gateway.mydomain.com) that connections will be made to or
    from that will avoid many dialogs when connecting mentioning that
    the hostname does not match the Common Name.
}
	.ccrt.f.t insert end $msg

	global ccert ccert_init tcert


	if {! [info exists ccert_init]} {
		set ccert_init 1
		set ccert(CO) "US"
		set ccert(ST) "Massachusetts"
		set ccert(LOC) "Boston"
		set ccert(ON) "My Company"
		set ccert(OUN) "Product Development"
		set ccert(CN) "www.nowhere.none"
		set ccert(EM) "admin@nowhere.none"
		set ccert(DAYS) "365"
		set ccert(FILE) ""
	}

	set ccert(ENC) 0
	set ccert(PASS) ""
		
	set tcert(CO) "Country Name (2 letter code):"
	set tcert(ST) "State or Province Name (full name):"
	set tcert(LOC) "Locality Name (eg, city):"
	set tcert(ON) "Organization Name (eg, company):"
	set tcert(OUN) "Organizational Unit Name (eg, section):"
	set tcert(CN) "Common Name (eg, YOUR name):"
	set tcert(EM) "Email Address:"
	set tcert(DAYS) "Days until expiration:"

	if {$ccert(FILE) == ""} {
		set idir [get_idir_certs ""]
		set ccert(FILE) "$idir/vnccert.pem"
	}

	button .ccrt.cancel -text "Cancel" -command {destroy .ccrt; catch {raise .c}}
	bind .ccrt <Escape> {destroy .ccrt; catch {raise .c}}

	button .ccrt.create -text "Generate Cert" -command {destroy .ccrt; catch {raise .c}; do_oss_create}

	pack .ccrt.cancel .ccrt.create -side bottom -fill x

	set ew 40

	set w .ccrt.pf
	frame $w
	checkbutton $w.check -anchor w -variable ccert(ENC) -text \
		"Encrypt Key with Passphrase" -command {check_pp}

	entry $w.e -width $ew -textvariable ccert(PASS) -state disabled \
		-show *

	pack $w.e -side right
	pack $w.check -side left -expand 1 -fill x
	pack $w -side bottom -fill x

	set w .ccrt.fl
	frame $w
	label $w.l -anchor w -text "Save to file:"

	entry $w.e -width $ew -textvariable ccert(FILE)
	button $w.b -text "Browse..." -command {set_createcert_file; catch {raise .ccrt}}

	pack $w.e -side right
	pack $w.b -side right
	pack $w.l -side left -expand 1 -fill x
	pack $w -side bottom -fill x

	set i 0
	foreach t {DAYS EM CN OUN ON LOC ST CO} {
		set w .ccrt.f$i
		frame $w
		label $w.l -anchor w -text "$tcert($t)"
		entry $w.e -width $ew -textvariable ccert($t)
		pack $w.e  -side right
		pack $w.l  -side left -expand 1 -fill x
		pack $w -side bottom -fill x
		incr i
	}

	pack .ccrt.f -side top -fill both -expand 1

	center_win .ccrt
}

proc import_check_mode {w} {
	global import_mode
	if {$import_mode == "paste"} {
		$w.mf.b configure -state disabled
		$w.mf.e configure -state disabled
		$w.plab configure -state normal
		$w.paste.t configure -state normal
	} else {
		$w.mf.b configure -state normal
		$w.mf.e configure -state normal
		$w.plab configure -state disabled
		$w.paste.t configure -state disabled
	}
}
	
proc import_browse {} {
	global import_file

	set idir ""
	if {$import_file != ""} {
		set idir [get_idir_certs $import_file]
	}
	if {$idir != ""} {
		set t [tk_getOpenFile -parent .icrt -initialdir $idir]
	} else {
		set t [tk_getOpenFile -parent .icrt]
	}
	if {$t != ""} {
		set import_file $t
	}
	catch {raise .icrt}
	update
}

proc import_save_browse {{par ".icrt"}} {
	global import_save_file

	set idir ""
	if {$import_save_file != ""} {
		set idir [get_idir_certs $import_save_file]
	}
	if {$idir == ""} {
		set idir [get_idir_certs ""]
	}
	if {$idir != ""} {
		set t [tk_getSaveFile -parent $par -defaultextension ".crt" -initialdir $idir]
	} else {
		set t [tk_getSaveFile -parent $par -defaultextension ".crt"]
	}
	if {$t != ""} {
		set import_save_file $t
	}
	catch {raise $par}
	update
}

proc do_save {} {
	global import_mode import_file import_save_file
	
	if {$import_save_file == ""} {
		tk_messageBox -parent .icrt -type ok -icon error \
			-message "No Save File supplied" -title "Save File"
		return
	}

	set str ""
	if {$import_mode == "save_cert_text"} {
		global save_cert_text
		set str $save_cert_text
	} elseif {$import_mode == "paste"} {
		set str [.icrt.paste.t get 1.0 end]
	} else {
		if {! [file exists $import_file]} {
			tk_messageBox -parent .icrt -type ok -icon error \
				-message "Input file \"$import_file\" does not exist." -title "Import File"
			return
		}
		set fh ""
		set emess ""
		set rc [catch {set fh [open $import_file "r"]} emess]
		if {$rc != 0 || $fh == ""} {
			tk_messageBox -parent .icrt -type ok -icon error \
				-message $emess -title "Import File: $import_file"
			return
		}
		while {[gets $fh line] > -1} {
			append str "$line\n"
		}
		close $fh
	}

	if {! [regexp {BEGIN CERTIFICATE} $str]} {
		tk_messageBox -parent .icrt -type ok -icon error \
			-message "Import Text does not contain \"BEGIN CERTIFICATE\"" -title "Imported Text"
		return
	}
	if {! [regexp {END CERTIFICATE} $str]} {
		tk_messageBox -parent .icrt -type ok -icon error \
			-message "Import Text does not contain \"END CERTIFICATE\"" -title "Imported Text"
		return
	}

	set fh ""
	set emess ""
	set rc [catch {set fh [open $import_save_file "w"]} emess]
	if {$rc != 0 || $fh == ""} {
		tk_messageBox -parent .icrt -type ok -icon error \
			-message $emess -title "Save File: $import_save_file"
		return
	}
	global is_windows
	if {! $is_windows} {
		catch {file attributes $import_save_file -permissions go-w}
		if {[regexp {PRIVATE} $str] || [regexp {\.pem$} $import_save_file]} {
			catch {file attributes $import_save_file -permissions go-rw}
		}
	}
	puts -nonewline $fh $str
	close $fh
	catch {destroy .icrt}
	set p .c
	if {![winfo exists .c]} {
		global accepted_cert_dialog_in_progress
		if {! $accepted_cert_dialog_in_progress} {
			getcerts
			update	
		}
	}
	if {![winfo exists .c]} {
		set p .
	}
	catch {raise .c}
	catch {destroy .scrt}
	tk_messageBox -parent $p -type ok -icon info \
		-message "Saved to file: $import_save_file" -title "Save File: $import_save_file"
}

proc import_cert {} {

	toplev .icrt
	wm title .icrt "Import SSL Certificate"

	global scroll_text_focus
	set scroll_text_focus 0
	global uname
	if {$uname == "Darwin"} {
		scroll_text .icrt.f 90 16
	} else {
		scroll_text .icrt.f 90 20
	}
	set scroll_text_focus 1

	set msg {
    This dialog lets you import a SSL Certificate by either pasting one in or by
    loading from another file.  Choose which input mode you want to use by the toggle
    "Paste / Read from File".

    There are two types of files we use 1) Certificate only, and 2) Private Key
    and Certificate.

    Type 1) would be used to verify the identity of a remote VNC Server, whereas
    type 2) would be used to authenticate ourselves to the remote VNC Server.

    A type 1) by convention ends with file suffix ".crt" and looks like:

-----BEGIN CERTIFICATE-----
MIID2jCCAsKgAwIBAgIJALKypfV8BItCMA0GCSqGSIb3DQEBBAUAMIGgMQswCQYD
(more lines) ...
TCQ+tbQ/DOiTXGKx1nlcKoPdkG+QVQVJthlQcpam
-----END CERTIFICATE-----

    A type 2) by convention ends with file suffix ".pem" and looks like:

-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEA4sApd7WaPKQRWnFe9T04D4pglQB0Ti0/dCVHxg8WEVQ8OdcW
(more lines) ...
9kBmNotUiTpvRM+e7E/zRemhvY9qraFooqMWzi9JrgYfeLfSvvFfGw==
-----END RSA PRIVATE KEY-----
-----BEGIN CERTIFICATE-----
MIID2jCCAsKgAwIBAgIJALKypfV8BItCMA0GCSqGSIb3DQEBBAUAMIGgMQswCQYD
(more lines) ...
TCQ+tbQ/DOiTXGKx1nlcKoPdkG+QVQVJthlQcpam
-----END CERTIFICATE-----

    You do not need to use the ".crt" or ".pem" convention if you do not want to.

    First, either paste in the text or set the "Read from File" filename.

    Next, set the "Save to File" name to the file where the imported certificate
    will be saved.

    Then, click on "Save" to save the imported Certificate.

    After you have imported the Certificate (or Key + Certificate), select it to
    use for a connection via the "MyCert" or "ServerCert" dialog.
}
	.icrt.f.t insert end $msg

	global icert import_mode

	set import_mode "paste"

	set w .icrt.mf
	frame $w

	radiobutton $w.p -pady 1 -anchor w -variable import_mode -value paste \
		-text "Paste" -command "import_check_mode .icrt"

	radiobutton $w.f -pady 1 -anchor w -variable import_mode -value file \
		-text "Read from File:" -command "import_check_mode .icrt"

	global import_file
	set import_file ""
	entry $w.e -width 40 -textvariable import_file

	button $w.b -pady 1 -anchor w -text "Browse..." -command import_browse
	pack $w.b -side right
	pack $w.p $w.f -side left
	pack $w.e -side left -expand 1 -fill x

	$w.b configure -state disabled
	$w.e configure -state disabled

	label .icrt.plab -anchor w -text "Paste Certificate here:" 
	if {$uname == "Darwin"} {
		scroll_text .icrt.paste 90 11
	} else {
		scroll_text .icrt.paste 90 22
	}

	button .icrt.cancel -text "Cancel" -command {destroy .icrt; catch {raise .c}}
	bind .icrt <Escape> {destroy .icrt; catch {raise .c}}

	button .icrt.save -text "Save" -command {do_save}

	set w .icrt.sf
	frame $w

	label $w.l -text "Save to File:" -anchor w
	global import_save_file
	set import_save_file ""
	entry $w.e -width 40 -textvariable import_save_file
	button $w.b -pady 1 -anchor w -text "Browse..." -command import_save_browse

	pack $w.b -side right
	pack $w.l -side left
	pack $w.e -side left -expand 1 -fill x

	pack .icrt.cancel .icrt.save .icrt.sf .icrt.mf -side bottom -fill x
	pack .icrt.paste .icrt.plab -side bottom -fill x

	pack .icrt.f -side top -fill both -expand 1

	.icrt.paste.t insert end ""

	focus .icrt.paste.t

	center_win .icrt
}

proc save_cert {hp} {

	toplev .scrt
	wm title .scrt "Import SSL Certificate"

	global scroll_text_focus
	set scroll_text_focus 0
	global uname
	scroll_text .scrt.f 90 17
	set scroll_text_focus 1

	global accepted_cert_dialog_in_progress
	if {$accepted_cert_dialog_in_progress} {
		set mode "accepted"
	} else {
		set mode "normal"
	}

	set msg1 {
    This dialog lets you import a SSL Certificate retrieved from a VNC server.

    Be sure to have verified its authenticity via an external means (checking
    the MD5 hash value sent to you by the administrator, etc)

    Set the "Save to File" name to the file where the imported certificate
    will be saved.

    Then, click on "Save" to save the imported Certificate.

    After you have imported the Certificate it will be automatically selected 
    as the "ServerCert" for this host: %HOST

    To make the ServerCert setting to the imported cert file PERMANENT, 
    select Options -> Save Profile to save it in a profile.
}

	set msg2 {
    This dialog lets you import a SSL Certificate retrieved from a VNC server.

    Be sure to have verified its authenticity via an external means (checking
    the MD5 hash value sent to you by the administrator, etc)

    It will be added to the 'Accepted Certs' directory.  The "Save to File"
    below is already set to the correct directory and file name.

    Click on "Save" to add it to the Accepted Certs.

    It, and the others certs in that directory, will be used to authenticate
    any VNC Server that has "ACCEPTED_CERTS" as the "CertsDir" value in the
    "Certs..." dialog.
}

	set msg ""
	if {$mode == "normal"} {
		set msg $msg1
	} else {
		set msg $msg2
	}

	regsub {%HOST} $msg "$hp" msg
	.scrt.f.t insert end $msg

	set w .scrt.mf
	frame $w

	global import_file
	set import_file ""
	entry $w.e -width 40 -textvariable import_file

	scroll_text .scrt.paste 90 26

	button .scrt.cancel -text "Cancel" -command {destroy .scrt; catch {raise .c}}
	bind .scrt <Escape> {destroy .scrt; catch {raise .c}}

	global import_save_file
	if {$mode == "normal"} {
		button .scrt.save -text "Save" -command {do_save; set svcert $import_save_file}
	} else {
		button .scrt.save -text "Save" -command {do_save}
	}

	set w .scrt.sf
	frame $w

	label $w.l -text "Save to File:" -anchor w
	set import_save_file "server:$hp.crt"
	global is_windows
	regsub -all {:} $import_save_file "-" import_save_file

	set import_save_file [get_idir_certs ""]/$import_save_file

	global fetch_cert_filename
	if {$fetch_cert_filename != ""} {
		set import_save_file $fetch_cert_filename
	}

	entry $w.e -width 40 -textvariable import_save_file
	button $w.b -pady 1 -anchor w -text "Browse..." -command {import_save_browse .scrt}

	pack $w.b -side right
	pack $w.l -side left
	pack $w.e -side left -expand 1 -fill x

	pack .scrt.cancel .scrt.save .scrt.sf .scrt.mf -side bottom -fill x
	pack .scrt.paste -side bottom -fill x

	pack .scrt.f -side top -fill both -expand 1

	global cert_text
	set text "" 
	set on 0
	foreach line [split $cert_text "\n"] {
		if [regexp -- {-----BEGIN CERTIFICATE-----} $line] {
			set on 1
		}
		if {! $on} {
			continue;
		}
		append text "$line\n"
		if [regexp -- {-----END CERTIFICATE-----} $line] {
			set on 0
		}
	}
	global save_cert_text
	set save_cert_text $text
	.scrt.paste.t insert end "$text"
	global import_mode
	set import_mode "save_cert_text"

	focus .scrt.paste.t

	center_win .scrt
}


proc getcerts {} {
	global mycert svcert crtdir
	global use_ssh use_sshssl
	toplev .c
	wm title .c "SSL Certificates"
	frame .c.mycert
	frame .c.svcert
	frame .c.crtdir
	label .c.mycert.l -anchor w -width 12 -text "MyCert:"
	label .c.svcert.l -anchor w -width 12 -text "ServerCert:"
	label .c.crtdir.l -anchor w -width 12 -text "CertsDir:"
	
	entry .c.mycert.e -width 32 -textvariable mycert -vcmd v_mycert
	entry .c.svcert.e -width 32 -textvariable svcert -vcmd v_svcert
	bind .c.mycert.e <Enter> {.c.mycert.e validate}
	bind .c.mycert.e <Leave> {.c.mycert.e validate}
	bind .c.svcert.e <Enter> {.c.svcert.e validate}
	bind .c.svcert.e <Leave> {.c.svcert.e validate}
	entry .c.crtdir.e -width 32 -textvariable crtdir
	button .c.mycert.b -text "Browse..." -command {set_mycert .c; catch {raise .c}}
	button .c.svcert.b -text "Browse..." -command {set_svcert .c; catch {raise .c}}
	button .c.crtdir.b -text "Browse..." -command {set_crtdir .c; catch {raise .c}}
	button .c.mycert.i -text "Info" -command {show_mycert}
	button .c.svcert.i -text "Info" -command {show_svcert}
	button .c.crtdir.i -text "Info" -command {}
	bind .c.mycert.b <Enter> "v_mycert"
	bind .c.svcert.b <Enter> "v_svcert"
	.c.mycert.i configure -state disabled
	.c.svcert.i configure -state disabled
	.c.crtdir.i configure -state disabled
	bind .c.mycert.b <B3-ButtonRelease>   "show_mycert"
	bind .c.svcert.b <B3-ButtonRelease>   "show_svcert"

	button .c.create -text "Create Certificate ..." -command {create_cert}
	button .c.import -text "Import Certificate ..." -command {import_cert}

	frame .c.b
	button .c.b.done -text "Done" -command {catch {destroy .c}}
	bind .c <Escape> {destroy .c}
	button .c.b.help -text "Help" -command help_certs
	pack .c.b.help .c.b.done -fill x -expand 1 -side left

	foreach w [list mycert svcert crtdir] {
		pack .c.$w.l -side left
		pack .c.$w.e -side left -expand 1 -fill x
		pack .c.$w.b -side left
		pack .c.$w.i -side left
		bind .c.$w.e <Return> ".c.$w.b invoke"
		if {$use_ssh} {
			.c.$w.l configure -state disabled	
			.c.$w.e configure -state disabled	
			.c.$w.b configure -state disabled	
		}	
	}

	if {$mycert != ""} {
		v_mycert
	}
	if {$svcert != ""} {
		v_svcert
	}

	pack .c.mycert .c.svcert .c.crtdir .c.create .c.import .c.b -side top -fill x
	center_win .c
	wm resizable .c 1 0

	focus .c
}

proc get_profiles_dir {} {
	global env is_windows
	
	set dir ""
	if {$is_windows} {
		if [info exists env(SSVNC_HOME)] {
			set t "$env(SSVNC_HOME)/ss_vnc"
			regsub -all {\\} $t "/" t
			regsub -all {//*} $t "/" t
			if {! [file isdirectory $t]} {
				catch {file mkdir $t}
			}
			if [file isdirectory $t] {
				set dir $t
				set s "$t/profiles"
				if {! [file exists $s]} {
					catch {file mkdir $s}
				}
			}
		}
		if {$dir == ""} {
			set t [file dirname [pwd]]
			set t "$t/profiles"
			if [file isdirectory $t] {
				set dir $t
			}
		}
	} elseif [info exists env(SSVNC_HOME)] {
		set t "$env(SSVNC_HOME)/.vnc"
		catch {file mkdir $t}
		if [file isdirectory $t] {
			set dir $t
			set s "$t/profiles"
			if {! [file exists $s]} {
				catch {file mkdir $s}
			}
		}
	}
	
	if {$dir != ""} {
		
	} elseif [info exists env(SSVNC_BASEDIR)] {
		set dir $env(SSVNC_BASEDIR)
	} else {
		set dir [pwd]
	}
	if [file isdirectory "$dir/profiles"] {
		set dir "$dir/profiles"
	}
	return $dir
}

proc globalize {} {
	global defs
	foreach var [array names defs] {
		uplevel global $var
	}
}
	
proc load_include {include dir} {
	global include_vars defs

	if [info exists include_vars] {
		unset include_vars
	}
	
	foreach inc [split $include ", "] {
		set f [string trim $inc]
#puts "f=$f";
		if {$f == ""} {
			continue
		}
		set try ""
		if {[regexp {/} $f] || [regexp {\\} $f]} {
			set try $f;
		} else {
			set try "$dir/$f"
		}
		if {! [file exists $try]} {
			set try "$dir/$f.vnc"
		}
#puts "try: $try"
		if [file exists $try] {
			set fh ""
			catch {set fh [open $try "r"]}
			if {$fh == ""} {
				continue
			}
			while {[gets $fh line] > -1} {
				append inc_str "$line\n"
				if [regexp {^([^=]*)=(.*)$} $line m var val] {
					if {! [info exists defs($var)]} {
						continue
					}
					if {$var == "include_list"} {
						continue
					}
					set pct 0
					if {$var == "smb_mount_list"} {
						set pct 1
					}
					if {$var == "port_knocking_list"} {
						set pct 1
					}
					if {$pct} {
						regsub -all {%%%} $val "\n" val
					}
					if {$val != $defs($var)} {
#puts "include_vars $var $val"
						set include_vars($var) $val
					}
				}
			}
			catch {close $fh}
		}
	}
}

proc load_profile {{parent "."} {infile ""}} {
	global profdone
	global vncdisplay

	globalize

	set dir [get_profiles_dir]

	if {$infile != ""} {
		set file $infile
	} else {
		set file [tk_getOpenFile -parent $parent -defaultextension \
			".vnc" -initialdir $dir -title "Load VNC Profile"]
	}
	if {$file == ""} {
		set profdone 1
		return
	}
	set fh [open $file "r"]
	if {! [info exists fh]} {
		set profdone 1
		return
	}
	set str ""
	set include ""
	while {[gets $fh line] > -1} {
		append str "$line\n"
		if [regexp {^include_list=(.*)$} $line m val] {
			set include $val
		}
	}
	close $fh

	if {$include != ""} {
		load_include $include $dir
	}

	set_defaults

	global include_vars
	if [info exists include_vars] {
		foreach var [array names include_vars] {
			set $var $include_vars($var)
		}
	}

	global use_ssl use_ssh use_sshssl
	set use_ssl 0
	set use_ssh 0
	set use_sshssl 0

	global defs
	foreach line [split $str "\n"] {
		set line [string trim $line]
		if [regexp {^#} $line] {
			continue
		}
		if [regexp {^([^=]*)=(.*)$} $line m var val] {
			if {$var == "disp"} {
				set vncdisplay $val
				continue
			}
			if [info exists defs($var)] {
				set pct 0
				if {$var == "smb_mount_list"} {
					set pct 1
				}
				if {$var == "port_knocking_list"} {
					set pct 1
				}
				if {$pct} {
					regsub -all {%%%} $val "\n" val
				}
				set $var $val
			}
		}
	}

	init_vncdisplay
	if {! $use_ssl && ! $use_ssh && ! $use_sshssl} {
		set use_ssl 1
	}
	if {$use_ssl} {
		set use_ssh 0
		set use_sshssl 0
	} elseif {$use_ssh && $use_sshssl} {
		set use_ssh 0
	}
	sync_use_ssl_ssh

	set compresslevel_text "Compress Level: $use_compresslevel"
	set quality_text "Quality: $use_quality"

	set profdone 1
	putty_pw_entry check
	listen_adjust
}

proc sync_use_ssl_ssh {} {
	global use_ssl use_ssh use_sshssl ssl_ssh_adjust
	if {$use_ssl} {
		ssl_ssh_adjust ssl
	} elseif {$use_ssh} {
		ssl_ssh_adjust ssh
	} elseif {$use_sshssl} {
		ssl_ssh_adjust sshssl
	}
}

proc dummy_cert {} {
	set str {
-----BEGIN RSA PRIVATE KEY-----
MIIEowIBAAKCAQEAvkfXxb0wcxgrjV2ziFikjII+ze8iKcTBt47L0GM/c21efelN
+zZpJUUXLu4zz8Ryq8Q+sQgfNy7uTOpN9bUUaOk1TnD7gaDQnQWiNHmqbW2kL+DS
OKngJVPo9dETAS8hf7+D1e1DBZxjTc1a4RQqWJixwpYj99ixWzu8VC2m/xXsjvOs
jp4+DLBB490nbkwvstmhmiWm1CmI5O5xOkgioVNQqHvQMdVKOSz9PpbjvZiRX1Uo
qoMrk+2NOqwP90TB35yPASXb9zXKpO7DLhkube+yYGf+yk46aD707L07Eb7cosFP
S84vNZ9gX7rQ0UOwm5rYA/oZTBskgaqhtIzkLwIDAQABAoIBAD4ot/sXt5kRn0Ca
CIkU9AQWlC+v28grR2EQW9JiaZrqcoDNUzUqbCTJsi4ZkIFh2lf0TsqELbZYNW6Y
6AjJM7al4E0UqYSKJTv2WCuuRxdiRs2BMwthqyBmjeanev7bB6V0ybt7u3Y8xU/o
MrTuYnr4vrEjXPKdLirwk7AoDbKsRXHSIiHEIBOq1+dUQ32t36ukdnnza4wKDLZc
PKHiCdCk/wOGhuDlxD6RspqUAlRnJ8/aEhrgWxadFXw1hRhRsf/v1shtB0T3DmTe
Jchjwyiw9mryb9JZAcKxW+fUc4EVvj6VdQGqYInQJY5Yxm5JAlVQUJicuuJEvn6A
rj5osQECgYEA552CaHpUiFlB4HGkjaH00kL+f0+gRF4PANCPk6X3UPDVYzKnzmuu
yDvIdEETGFWBwoztUrOOKqVvPEQ+kBa2+DWWYaERZLtg2cI5byfDJxQ3ldzilS3J
1S3WgCojqcsG/hlxoQJ1dZFanUy/QhUZ0B+wlC+Zp1Q8AyuGQvhHp68CgYEA0lBI
eqq2GGCdJuNHMPFbi8Q0BnX55LW5C1hWjhuYiEkb3hOaIJuJrqvayBlhcQa2cGqp
uP34e9UCfoeLgmoCQ0b4KpL2NGov/mL4i8bMgog4hcoYuIi3qxN18vVR14VKEh4U
RLk0igAYPU+IK2QByaQlBo9OSaKkcfm7U1/pK4ECgYAxr6VpGk0GDvfF2Tsusv6d
GIgV8ZP09qSLTTJvvxvF/lQYeqZq7sjI5aJD5i3de4JhpO/IXQJzfZfWOuGc8XKA
3qYK/Y2IqXXGYRcHFGWV/Y1LFd55mCADHlk0l1WdOBOg8P5iRu/Br9PbiLpCx9oI
vrOXpnp03eod1/luZmqguwKBgQCWFRSj9Q7ddpSvG6HCG3ro0qsNsUMTI1tZ7UBX
SPogx4tLf1GN03D9ZUZLZVFUByZKMtPLX/Hi7K9K/A9ikaPrvsl6GEX6QYzeTGJx
3Pw0amFrmDzr8ySewNR6/PXahxPEuhJcuI31rPufRRI3ZLah3rFNbRbBFX+klkJH
zTnoAQKBgDbUK/aQFGduSy7WUT7LlM3UlGxJ2sA90TQh4JRQwzur0ACN5GdYZkqM
YBts4sBJVwwJoxD9OpbvKu3uKCt41BSj0/KyoBzjT44S2io2tj1syujtlVUsyyBy
/ca0A7WBB8lD1D7QMIhYUm2O9kYtSCLlUTHt5leqGaRG38DqlX36
-----END RSA PRIVATE KEY-----
-----BEGIN CERTIFICATE-----
MIIDzDCCArQCCQDSzxzxqhyqLzANBgkqhkiG9w0BAQQFADCBpzELMAkGA1UEBhMC
VVMxFjAUBgNVBAgTDU1hc3NhY2h1c2V0dHMxDzANBgNVBAcTBkJvc3RvbjETMBEG
A1UEChMKTXkgQ29tcGFueTEcMBoGA1UECxMTUHJvZHVjdCBEZXZlbG9wbWVudDEZ
MBcGA1UEAxMQd3d3Lm5vd2hlcmUubm9uZTEhMB8GCSqGSIb3DQEJARYSYWRtaW5A
bm93aGVyZS5ub25lMB4XDTA3MDMyMzE4MDc0NVoXDTI2MDUyMjE4MDc0NVowgacx
CzAJBgNVBAYTAlVTMRYwFAYDVQQIEw1NYXNzYWNodXNldHRzMQ8wDQYDVQQHEwZC
b3N0b24xEzARBgNVBAoTCk15IENvbXBhbnkxHDAaBgNVBAsTE1Byb2R1Y3QgRGV2
ZWxvcG1lbnQxGTAXBgNVBAMTEHd3dy5ub3doZXJlLm5vbmUxITAfBgkqhkiG9w0B
CQEWEmFkbWluQG5vd2hlcmUubm9uZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCC
AQoCggEBAL5H18W9MHMYK41ds4hYpIyCPs3vIinEwbeOy9BjP3NtXn3pTfs2aSVF
Fy7uM8/EcqvEPrEIHzcu7kzqTfW1FGjpNU5w+4Gg0J0FojR5qm1tpC/g0jip4CVT
6PXREwEvIX+/g9XtQwWcY03NWuEUKliYscKWI/fYsVs7vFQtpv8V7I7zrI6ePgyw
QePdJ25ML7LZoZolptQpiOTucTpIIqFTUKh70DHVSjks/T6W472YkV9VKKqDK5Pt
jTqsD/dEwd+cjwEl2/c1yqTuwy4ZLm3vsmBn/spOOmg+9Oy9OxG+3KLBT0vOLzWf
YF+60NFDsJua2AP6GUwbJIGqobSM5C8CAwEAATANBgkqhkiG9w0BAQQFAAOCAQEA
vGomHEp6TVU83X2EBUgnbOhzKJ9u3fOI/Uf5L7p//Vxqow7OR1cguzh/YEzmXOIL
ilMVnzX9nj/bvcLAuqEP7MR1A8f4+E807p/L/Sf49BiCcwQq5I966sGKYXjkve+T
2GTBNwMSq+5kLSf6QY8VZI+qnrAudEQMeJByQhTZZ0dH8Njeq8EGl9KUio+VWaiW
CQK6xJuAvAHqa06OjLmwu1fYD4GLGSrOIiRVkSXV8qLIUmzxdJaIRznkFWsrCEKR
wAH966SAOvd2s6yOHMvyDRIL7WHxfESB6rDHsdIW/yny1fBePjv473KrxyXtbz7I
dMw1yW09l+eEo4A7GzwOdw==
-----END CERTIFICATE-----
}
	return $str
}

proc save_profile {{parent "."}} {
	global is_windows uname
	global profdone
	global include_vars defs

	globalize
	
	set dir [get_profiles_dir]

	set vncdisp [get_vncdisplay]


	set disp [string trim $vncdisp]
	if {$disp != ""} {
		regsub {[ 	].*$} $disp "" disp
		regsub -all {/} $disp "" disp
	} else {
		mesg "No VNC Host:Disp supplied."
		bell
		return
	}
	if {$is_windows || $uname == "Darwin"} {
		regsub -all {:} $disp "-" disp
	} else {
		regsub -all {:} $disp "-" disp
	}

	set file [tk_getSaveFile -parent $parent -defaultextension ".vnc" \
		-initialdir $dir -initialfile "$disp" -title "Save VNC Profile"]
	if {$file == ""} {
		set profdone 1
		return
	}
	set fh [open $file "w"]
	if {! [info exists fh]} {
		set profdone 1
		return
	}
	set h [string trim $vncdisp]
	set p $h
	regsub {:.*$} $h "" h
	set host $h
	regsub {[ 	].*$} $p "" p
	regsub {^.*:} $p "" p
	regsub { .*$} $p "" p
	if {$p == ""} {
		set p 0
	}
	if {$p < 0} {
		set port $p
	} elseif {$p < 200} {
		set port [expr $p + 5900]
	} else {
		set port $p
	}

	set h [string trim $vncdisp]
	regsub {cmd=.*$} $h "" h
	set h [string trim $h]
	if {! [regexp {[ 	]} $h]} {
		set h ""
	} else {
		regsub {^.*[ 	]} $h "" h
	}
	if {$h == ""} {
		set proxy ""
		set proxyport ""
	} else {
		set p $h
		regsub {:.*$} $h "" h
		set proxy $h
		regsub {[ 	].*$} $p "" p
		regsub {^.*:} $p "" p
		if {$p == ""} {
			set proxyport 0
		} else {
			set proxyport $p
		}
	}
	
	puts $fh "\[connection\]"
	puts $fh "host=$host"
	puts $fh "port=$port"
	puts $fh "proxyhost=$proxy"
	puts $fh "proxyport=$proxyport"
	puts $fh "disp=$vncdisp"
	puts $fh "\n\[options\]"

	if {$include_list != ""} {
		load_include $include_list [get_profiles_dir]
	}

	foreach var [lsort [array names defs]] {
		eval set val \$$var
		set pre ""
		if {$val == $defs($var)} {
			set pre "#"
		}
		set pct 0
		if {$var == "smb_mount_list"} {
			set pct 1
		}
		if {$var == "port_knocking_list"} {
			set pct 1
		}
		if {$include_list != "" && [info exists include_vars($var)]} {
			if {$val == $include_vars($var)} {
				if {$pct} {
					regsub -all "\n" $val "%%%" val
				}
				puts $fh "#from include: $var=$val"
				continue
			}
		}
		if {$pct} {
			regsub -all "\n" $val "%%%" val
		}
		puts $fh "$pre$var=$val"
	}

	close $fh
	set profdone 1
}

proc set_ssh {} {
	global use_ssl
	if {$use_ssl} {
		ssl_ssh_adjust ssh
	}
}

proc expand_IP {redir} {
	if {! [regexp {:IP:} $redir]} {
		return $redir
	}
	if {! [regexp {(-R).*:IP:} $redir]} {
		return $redir
	}

	set ip [guess_ip]
	set ip [string trim $ip]
	if {$ip == ""} {
		return $redir
	}

	regsub -all {:IP:} $redir ":$ip:" redir
	return $redir
}

proc get_cups_redir {} {
	global cups_local_server cups_remote_port
	global cups_local_smb_server cups_remote_smb_port
	set redir "$cups_remote_port:$cups_local_server"
	regsub -all {['" 	]} $redir {} redir; #"
	set redir " -R $redir"
	if {$cups_local_smb_server != "" && $cups_remote_smb_port != ""} {
		set redir2 "$cups_remote_smb_port:$cups_local_smb_server"
		regsub -all {['" 	]} $redir2 {} redir2; #"
		set redir "$redir -R $redir2"
	}
	set redir [expand_IP $redir]
	return $redir
}

proc get_additional_redir {} {
	global additional_port_redirs additional_port_redirs_list
	if {! $additional_port_redirs || $additional_port_redirs_list == ""} {
		return ""
	}
	set redir [string trim $additional_port_redirs_list]
	regsub -all {['"]} $redir {} redir; #"
	set redir " $redir"
	set redir [expand_IP $redir]
	return $redir
}

proc get_sound_redir {} {
	global sound_daemon_remote_port sound_daemon_local_port
	set loc $sound_daemon_local_port
	if {! [regexp {:} $loc]} {
		set loc "localhost:$loc"
	}
	set redir "$sound_daemon_remote_port:$loc"
	regsub -all {['" 	]} $redir {} redir; #"
	set redir " -R $redir"
	set redir [expand_IP $redir]
	return $redir
}

proc get_smb_redir {} {
	global smb_mount_list

	set s [string trim $smb_mount_list]
	if {$s == ""} {
		return ""
	}

	set did(0) 1
	set redir ""
	set mntlist ""

	foreach line [split $s "\r\n"] {
		set str [string trim $line] 
		if {$str == ""} {
			continue
		}
		if {[regexp {^#} $str]} {
			continue
		}

		set port ""
		if [regexp {^([0-9][0-9]*)[ \t][ \t]*(.*)} $str mvar port rest] {
			# leading port
			set str [string trim $rest]
		}

		# grab:  //share /dest [host[:port]]
		set share ""
		set dest ""
		set hostport ""
		foreach item [split $str] {
			if {$item == ""} {
				continue
			}
			if {$share == ""} {
				set share [string trim $item]
			} elseif {$dest == ""} {
				set dest [string trim $item]
			} elseif {$hostport == ""} {
				set hostport [string trim $item]
			}
		}

		regsub {^~/} $dest {$HOME/} dest

		# work out the local host:port
		set lhost ""
		set lport ""
		if {$hostport != ""} {
			if [regexp {(.*):(.*)} $hostport mvar lhost lport] {
				;
			} else {
				set lhost $hostport
				set lport 139
			}
		} else {
			if [regexp {//([^/][^/]*)/} $share mvar h] {
				if [regexp {(.*):(.*)} $h mvar lhost lport] {
					;
				} else {
					set lhost $h
					set lport 139
				}
			} else {
				set lhost localhost
				set lport 139
			}
		}

		if {$port == ""} {
			if [info exists did("$lhost:$lport")] {
				# reuse previous one:
				set port $did("$lhost:$lport")
			} else {
				# choose one at random:
				for {set i 0} {$i < 3} {incr i} {
					set port [expr 20100 + 9000 * rand()]	
					set port [expr round($port)]
					if { ! [info exists did($port)] } {
						break
					}
				}
			}
			set did($port) 1
		}

		if {$mntlist != ""} {
			append mntlist " "
		}
		append mntlist "$share,$dest,$port"

		if { ! [info exists did("$lhost:$lport")] } {
			append redir " -R $port:$lhost:$lport"
			set did("$lhost:$lport") $port
		}
	}

	regsub -all {['"]} $redir {} redir; #"
	set redir [expand_IP $redir]

	regsub -all {['"]} $mntlist {} mntlist; #"

	set l [list]
	lappend l $redir
	lappend l $mntlist
	return $l
}

proc ugly_setup_scripts {mode tag} {

set cmd(1) {
	SSHD_PID=""
	FLAG=$HOME/.vnc-helper-flag__PID__

	if [ "X$USER" = "X" ]; then
		USER=$LOGNAME
	fi

	DO_CUPS=0
	cups_dir=$HOME/.cups
	cups_cfg=$cups_dir/client.conf
	cups_host=localhost
	cups_port=NNNN

	DO_SMB=0
	DO_SMB_SU=0
	DO_SMB_WAIT=0
	smb_mounts=
	DONE_PORT=NNNN
	smb_script=$HOME/.smb-mounts__PID__.sh

	DO_SOUND=0
	DO_SOUND_KILL=0
	DO_SOUND_RESTART=0
	sound_daemon_remote_prog=
	sound_daemon_remote_args=

	findpid() {
		i=1
		back=10
		touch $FLAG

		if [ "X$TOPPID" = "X" ]; then
			TOPPID=$$
			back=50
		fi

		while [ $i -lt $back ]
		do
			try=`expr $TOPPID - $i`
			if ps $try 2>/dev/null | grep sshd >/dev/null; then
				SSHD_PID="$try"	
				echo SSHD_PID=$try
				echo
				break
			fi
			i=`expr $i + 1`
		done
		echo MY_PID=$$
		tty
		echo
	}

	wait_til_ssh_gone() {
		try_perl=""
		if type perl >/dev/null 2>&1; then
			try_perl=1
		fi
		uname=`uname`
		if [ "X$uname" != "XLinux" -a "X$uname" != "XSunOS" ]; then
			try_perl=""
		fi
		if [ "X$try_perl" = "X1" ]; then
			# try to avoid wasting pids:
			perl -e "while (1) {if(! -e \"/proc/$SSHD_PID\"){exit} if(! -f \"$FLAG\"){exit} sleep 1;}"
		else
			while [ 1 ]
			do
				ps $SSHD_PID > /dev/null 2>&1
				if [ $? != 0 ]; then
					break
				fi
				if [ ! -f $FLAG ]; then
					break
				fi
				sleep 1
			done
		fi
		rm -f $FLAG
		if [ "X$DO_SMB_WAIT" = "X1" ]; then
			rm -f $smb_script
		fi
	}
};

set cmd(2) {
	update_client_conf() {
		mkdir -p $cups_dir
		if [ -f $cups_cfg ]; then
			cp -p $cups_cfg $cups_cfg.back
		else
			touch $cups_cfg.back
		fi
		sed -e "s/^ServerName/#-etv-#ServerName/" $cups_cfg.back > $cups_cfg
		echo "ServerName $cups_host:$cups_port" >> $cups_cfg
		echo
		echo "--------------------------------------------------------------"
		echo "The CUPS $cups_cfg config file has been set to:"
		echo
		cat $cups_cfg
		echo
		echo "If there are problems automatically restoring it, edit or"
		echo "remove the file to go back to local CUPS settings."
		echo
		echo "A backup has been placed in: $cups_cfg.back"
		echo
		echo "See the help description for more details on printing."
		echo
		echo "done."
		echo "--------------------------------------------------------------"
		echo
	}

	reset_client_conf() {
		cp -p $cups_cfg $cups_cfg.tmp
		grep -v "^ServerName" $cups_cfg.tmp | sed -e "s/^#-etv-#ServerName/ServerName/" > $cups_cfg
		rm -f $cups_cfg.tmp
	}

	cupswait() {
		trap "" INT QUIT HUP
		wait_til_ssh_gone
		reset_client_conf
	}
};

#		if [ "X$DONE_PORT" != "X" ]; then
#			if type perl >/dev/null 2>&1; then
#				perl -e "use IO::Socket::INET; \$SIG{INT} = \"IGNORE\"; \$SIG{QUIT} = \"IGNORE\"; \$SIG{HUP} = \"INGORE\"; my \$client = IO::Socket::INET->new(Listen => 5, LocalAddr => \"localhost\", LocalPort => $DONE_PORT, Proto => \"tcp\")->accept(); \$line = <\$client>; close \$client; unlink \"$smb_script\";" </dev/null >/dev/null 2>/dev/null &
#				if [ $? = 0 ]; then
#					have_perl_done="1"
#				fi
#			fi
#		fi

set cmd(3) {
	smbwait() {
		trap "" INT QUIT HUP
		wait_til_ssh_gone
	}
	do_smb_mounts() {
		if [ "X$smb_mounts" = "X" ]; then
			return
		fi
		echo > $smb_script
		have_perl_done=""
		echo "echo" >> $smb_script 
		dests=""
		for mnt in $smb_mounts
		do
			smfs=`echo "$mnt" | awk -F, "{print \\\$1}"`
			dest=`echo "$mnt" | awk -F, "{print \\\$2}"`
			port=`echo "$mnt" | awk -F, "{print \\\$3}"`
			dest=`echo "$dest" | sed -e "s,__USER__,$USER,g" -e "s,__HOME__,$HOME,g"`
			if [ ! -d $dest ]; then
				mkdir -p $dest
			fi
			echo "echo SMBMOUNT:" >> $smb_script
			echo "echo smbmount $smfs $dest -o uid=$USER,ip=127.0.0.1,ttl=20000,port=$port" >> $smb_script
			echo "smbmount \"$smfs\" \"$dest\" -o uid=$USER,ip=127.0.0.1,ttl=20000,port=$port" >> $smb_script
			echo "echo; df \"$dest\"; echo" >> $smb_script
			dests="$dests $dest"
		done
		#}
};

set cmd(4) {
		echo "(" >> $smb_script
		echo "trap \"\" INT QUIT HUP" >> $smb_script

		try_perl=""
		if type perl >/dev/null 2>&1; then
			try_perl=1
		fi
		uname=`uname`
		if [ "X$uname" != "XLinux" -a "X$uname" != "XSunOS" ]; then
			try_perl=""
		fi

		if [ "X$try_perl" = "X" ]; then
			echo "while [ -f $smb_script ]" >> $smb_script
			echo "do" >> $smb_script
			echo "     sleep 1" >> $smb_script
			echo "done" >> $smb_script
		else
			echo "perl -e \"while (-f \\\\\"$smb_script\\\\\") {sleep 1;} exit 0;\"" >> $smb_script
		fi
		for dest in $dests
		do
			echo "echo smbumount $dest" >> $smb_script
			echo "smbumount \"$dest\"" >> $smb_script
		done
		echo ") &" >> $smb_script
		echo "--------------------------------------------------------------"
		if [ "$DO_SMB_SU" = "0" ]; then
			echo "We now run the smbmount script as user $USER"
			echo
			echo sh $smb_script
			sh $smb_script
			rc=0
		elif [ "$DO_SMB_SU" = "1" ]; then
			echo "We now run the smbmount script via su(1)"
			echo
			echo "The first \"Password:\" will be for that of root to run the smbmount script."
			echo
			echo "Subsequent \"Password:\" will be for the SMB share(s) (hit Return if no passwd)"
			echo
			echo SU:
			echo "su root -c \"sh $smb_script\""
			su root -c "sh $smb_script"
			rc=$?
		elif [ "$DO_SMB_SU" = "2" ]; then
			echo "We now run the smbmount script via sudo(8)"
			echo
			echo "The first \"Password:\" will be for that of the sudo(8) password."
			echo
			echo "Subsequent \"Password:\" will be for the SMB shares (hit enter if no passwd)"
			echo
			echo SUDO:
			echo sudo sh $smb_script
			sudo sh $smb_script
			rc=$?
		fi
};

set cmd(5) {
		#{
		echo
		if [ "$rc" = 0 ]; then
			if [ "X$have_perl_done" = "X1" -o 1 = 1 ] ; then
				echo
				echo "Your SMB shares will be unmounted when the VNC connection closes,"
				echo "*As Long As* No Applications have any of the share files opened or are"
				echo "cd-ed into any of the share directories."
				echo
				echo "Try to make sure nothing is accessing the SMB shares before disconnecting"
				echo "the VNC session.  If you fail to do that follow these instructions:"
			fi
			echo
			echo "To unmount your SMB shares make sure no applications are still using any of"
			echo "the files and no shells are still cd-ed into the share area, then type:"
			echo 
			echo "   rm -f $smb_script"
			echo 
			echo "In the worst case run: smbumount /path/to/mount/point for each mount as root"
			echo "Even with the remote redirection gone the kernel should umount after a timeout."
		else
			echo 
			if [ "$DO_SMB_SU" = "1" ]; then
				echo "su(1) to run smbmount(8) failed."
			elif [ "$DO_SMB_SU" = "2" ]; then
				echo "sudo(8) to run smbmount(8) failed."
			fi
			rm -f $smb_script
		fi
		echo
		echo "done."
		echo "--------------------------------------------------------------"
		echo
	}
};

set cmd(6) {

	setup_sound() {
		dpid=""
		d=$sound_daemon_remote_prog
		if type pgrep >/dev/null 2>/dev/null; then
			dpid=`pgrep -U $USER -x $d | head -1`
		else
			dpid=`env PATH=/usr/ucb:$PATH ps wwwwaux | grep -w $USER | grep -w $d | grep -v grep | head -1`
		fi
		echo "--------------------------------------------------------------"
		echo "Setting up Sound: pid=$dpid"
		if [ "X$dpid" != "X" ]; then
			dcmd=`env PATH=/usr/ucb:$PATH ps wwwwaux | grep -w $USER | grep -w $d | grep -w $dpid | grep -v grep | head -1 | sed -e "s/^.*$d/$d/"`
			if [ "X$DO_SOUND_KILL" = "X1" ]; then
				echo "Stopping sound daemon: $sound_daemon_remote_prog $dpid"
				echo "sound cmd: $dcmd"
				kill -TERM $dpid
			fi
		fi
		echo
		echo "done."
		echo "--------------------------------------------------------------"
		echo
	}

	reset_sound() {
		if [ "X$DO_SOUND_RESTART" = "X1" ]; then
			d=$sound_daemon_remote_prog
			a=$sound_daemon_remote_args
			echo "Restaring sound daemon: $d $a"
			$d $a </dev/null >/dev/null 2>&1 &
		fi
	}

	soundwait() {
		trap "" INT QUIT HUP
		wait_til_ssh_gone
		reset_sound
	}

	findpid

	if [ $DO_SMB = 1 ]; then
		do_smb_mounts
	fi

	waiter=0

	if [ $DO_CUPS = 1 ]; then
		update_client_conf
		cupswait </dev/null >/dev/null 2>/dev/null &
		waiter=1
	fi

	if [ $DO_SOUND = 1 ]; then
		setup_sound
		soundwait </dev/null >/dev/null 2>/dev/null &
		waiter=1
	fi
	if [ $DO_SMB_WAIT = 1 ]; then
		if [ $waiter != 1 ]; then
			smbwait </dev/null >/dev/null 2>/dev/null &
			waiter=1
		fi
	fi


	#FINMSG
	echo
	echo "--vnc-helper-exiting--"
	echo
	#cat $0
	rm -f $0
	exit 0
};

	set cmdall ""

	for {set i 1} {$i <= 6} {incr i} {
		set v $cmd($i);
		regsub -all "\n" $v "%" v
		set cmd($i) $v
		append cmdall "echo "
		if {$i == 1} {
			append cmdall {TOPPID=$$%} 
		}
		append cmdall {'}
		append cmdall $cmd($i)
		append cmdall {' | tr '%' '\n'}
		if {$i == 1} {
			append cmdall {>}
		} else {
			append cmdall {>>}
		}
		append cmdall {$HOME/.vnc-helper-cmd__PID__; }
	}
	append cmdall {sh $HOME/.vnc-helper-cmd__PID__; }

	regsub -all {vnc-helper-cmd} $cmdall "vnc-helper-cmd-$mode" cmdall
	if {$tag == ""} {
		set tag [pid]
	}
	regsub -all {__PID__} $cmdall "$tag" cmdall

	set orig $cmdall

	global use_cups cups_local_server cups_remote_port cups_manage_rcfile
	if {$use_cups && $cups_manage_rcfile} {
		if {$mode == "post"} {
			regsub {DO_CUPS=0} $cmdall {DO_CUPS=1} cmdall
			regsub {cups_port=NNNN} $cmdall "cups_port=$cups_remote_port" cmdall
		}
	}
	
	global use_smbmnt smb_su_mode smb_mounts 
	if {$use_smbmnt} {
		if {$smb_mounts != ""} {
			set smbm $smb_mounts
			regsub -all {%USER} $smbm "__USER__" smbm
			regsub -all {%HOME} $smbm "__HOME__" smbm
			if {$mode == "pre"} {
				regsub {DO_SMB=0} $cmdall {DO_SMB=1} cmdall
				if {$smb_su_mode == "su"} {
					regsub {DO_SMB_SU=0} $cmdall {DO_SMB_SU=1} cmdall
				} elseif {$smb_su_mode == "sudo"} {
					regsub {DO_SMB_SU=0} $cmdall {DO_SMB_SU=2} cmdall
				} elseif {$smb_su_mode == "none"} {
					regsub {DO_SMB_SU=0} $cmdall {DO_SMB_SU=0} cmdall
				} else {
					regsub {DO_SMB_SU=0} $cmdall {DO_SMB_SU=1} cmdall
				}
				regsub {smb_mounts=} $cmdall "smb_mounts=\"$smbm\"" cmdall
			} elseif {$mode == "post"} {
				regsub {DO_SMB_WAIT=0} $cmdall {DO_SMB_WAIT=1} cmdall
			}
		}
	}

	global use_sound
	if {$use_sound} {
		if {$mode == "pre"} {
			global sound_daemon_remote_cmd sound_daemon_kill sound_daemon_restart
			if {$sound_daemon_kill} {
				regsub {DO_SOUND_KILL=0} $cmdall {DO_SOUND_KILL=1} cmdall
				regsub {DO_SOUND=0} $cmdall {DO_SOUND=1} cmdall
			}
			if {$sound_daemon_restart} {
				regsub {DO_SOUND_RESTART=0} $cmdall {DO_SOUND_RESTART=1} cmdall
				regsub {DO_SOUND=0} $cmdall {DO_SOUND=1} cmdall
			}
			set sp [string trim $sound_daemon_remote_cmd]
			regsub {[ \t].*$} $sp "" sp
			set sa [string trim $sound_daemon_remote_cmd]
			regsub {^[^ \t][^ \t]*[ \t][ \t]*} $sa "" sa
			regsub {sound_daemon_remote_prog=} $cmdall "sound_daemon_remote_prog=\"$sp\"" cmdall
			regsub {sound_daemon_remote_args=} $cmdall "sound_daemon_remote_args=\"$sa\"" cmdall
		}
	}
	
	if {$mode == "pre"} {
		set dopre 0
		if {$use_smbmnt && $smb_mounts != ""} {
			set dopre 1
		}
		if {$use_sound && $sound_daemon_kill} {
			set dopre 1
		}
		if {$dopre} {
			global is_windows
			if {$is_windows} {
				regsub {#FINMSG} $cmdall {echo "Now Go Click on the Label to Start the 2nd SSH"} cmdall
			} else {
				regsub {#FINMSG} $cmdall {echo "Finished with the 1st SSH tasks, the 2nd SSH should start shortly..."} cmdall
			}
		}
	}

	if {"$orig" == "$cmdall"} {
		return ""
	} else {
		return $cmdall
	}
}

proc cups_dialog {} {

	toplev .cups
	wm title .cups "CUPS Tunnelling"
	global cups_local_server cups_remote_port cups_manage_rcfile
	global cups_local_smb_server cups_remote_smb_port

	global uname
	if {$uname == "Darwin"} {
		scroll_text .cups.f 80 25
	} else {
		scroll_text .cups.f
	}
		

	set msg {
    CUPS Printing requires SSH be used to set up the Print service port
    redirection.  This will be either of the "Use SSH" or "SSH + SSL"
    modes under "Options".  Pure SSL tunnelling will not work.

    This method requires working CUPS software setups on both the remote
    and local sides of the connection.

    (See Method #1 below for perhaps the easiest way to get applications to
    print through the tunnel; it requires printing admin privileges however).

    You choose an actual remote CUPS port below under "Use Remote CUPS
    Port:" (6631 is just our default and used in the examples below).
    Note that the normal default CUPS server port is 631.

    The port you choose must be unused on the VNC server machine (n.b. no
    checking is done).  Print requests connecting to it are redirected to
    your local machine through the SSH tunnel.  Note: root permission is
    needed for ports less than 1024 (this is not recommended).

    Then enter the VNC Viewer side (i.e. where you are sitting) CUPS server
    under "Local CUPS Server".  E.g. use "localhost:631" if there is one
    on the viewer machine, or, say, "my-print-srv:631" for a nearby CUPS
    print server.

    Several methods are now described for how to get applications to
    print through the port redirected tunnel.

    Method #0: Create or edit the file $HOME/.cups/client.conf on the VNC
    server side by putting in something like this in it:

    	ServerName localhost:6631

    based on the port you selected above.
    
    NOTE: For this client.conf ServerName setting to work with lp(1)
    and lpr(1) CUPS 1.2 or greater is required.  The cmdline option 
    "-h localhost:6631" can be used for older versions.  For client.conf to
    work in general (e.g. Openoffice, Firefox), a bugfix found in CUPS 1.2.3
    is required.  Two Workarounds (Methods #1 and #2) are described below.

    After the remote VNC Connection is finished, to go back to the non-SSH
    tunnelled CUPS server and either remove the client.conf file or comment
    out the ServerName line.  This restores the normal CUPS server for
    you on the remote machine.

    Select "Manage ServerName in the $HOME/.cups/client.conf file for me" to
    attempt to do this editing of the CUPS config file for you automatically.

    Method #1: If you have admin permission on the VNC Server machine you
    can likely "Add a Printer" via a GUI dialog, wizard, lpadmin(8), etc.
    This makes the client.conf ServerName parameter unnecessary.  You will
    need to tell the GUI dialog that the printer is at, e.g., localhost:6631,
    and anything else needed to identify the printer (type, model, etc).

    Method #2: Restarting individual applications with the IPP_PORT
    set will enable redirected printing for them, e.g.:

       env IPP_PORT=6631 firefox

    If you can only get Method #2 to work, an extreme application would
    be to run the whole desktop, e.g. env IPP_PORT=6631 gnome-session, but
    then you would need some sort of TCP redirector (ssh -L comes to mind),
    to direct it to 631 when not connected remotely.

    Windows/SMB Printers:  Under "Local SMB Print Server" you can set
    a port redirection for a Windows (non-CUPS) SMB printer.  E.g. port
    6632 -> localhost:139.  If localhost:139 does not work, try IP:139,
    etc. or put in the IP address manually.  Then at the least you can
    print using the smbspool(8) program like this:

       smbspool smb://localhost:6632/lp job user title 1 "" myfile.ps

    You could put this in a script, "myprinter".  It appears for the URI,
    only the number of copies ("1" above) and the file itself are important.
    (XXX this might only work for Samba printers...)

    If you have root or print admin permission you can configure CUPS to
    know about this printer via lpadmin(8), etc.  You basically give it
    the smb://... URI.

    For more info see: http://www.karlrunge.com/x11vnc/#faq-cups
}
	.cups.f.t insert end $msg

	if {$cups_local_server == ""} {
		set cups_local_server "localhost:631"
	}
	if {$cups_remote_port == ""} {
		set cups_remote_port "6631"
	}
	if {$cups_local_smb_server == ""} {
		global is_windows
		if {$is_windows} {
			set cups_local_smb_server "IP:139"
		} else {
			set cups_local_smb_server "localhost:139"
		}
	}
	if {$cups_remote_smb_port == ""} {
		set cups_remote_smb_port "6632"
	}

	frame .cups.serv
	label .cups.serv.l -anchor w -text "Local CUPS Server:      "
	entry .cups.serv.e -width 40 -textvariable cups_local_server
	pack .cups.serv.e -side right
	pack .cups.serv.l -side left -expand 1 -fill x

	frame .cups.port
	label .cups.port.l -anchor w -text "Use Remote CUPS Port:"
	entry .cups.port.e -width 40 -textvariable cups_remote_port
	pack .cups.port.e -side right
	pack .cups.port.l -side left -expand 1 -fill x

	frame .cups.smbs
	label .cups.smbs.l -anchor w -text "Local SMB Print Server:      "
	entry .cups.smbs.e -width 40 -textvariable cups_local_smb_server
	pack .cups.smbs.e -side right
	pack .cups.smbs.l -side left -expand 1 -fill x

	frame .cups.smbp
	label .cups.smbp.l -anchor w -text "Use Remote SMB Print Port:"
	entry .cups.smbp.e -width 40 -textvariable cups_remote_smb_port
	pack .cups.smbp.e -side right
	pack .cups.smbp.l -side left -expand 1 -fill x

	checkbutton .cups.cupsrc -anchor w -variable cups_manage_rcfile -text \
		"Manage ServerName in the remote \$HOME/.cups/client.conf file for me"

	button .cups.cancel -text "Cancel" -command {destroy .cups; set use_cups 0}
	bind .cups <Escape> {destroy .cups; set use_cups 0}
	button .cups.done -text "Done" -command {destroy .cups; if {$use_cups} {set_ssh}}

	button .cups.guess -text "Help me decide ..." -command {}
	.cups.guess configure -state disabled

	pack .cups.done .cups.cancel .cups.guess .cups.cupsrc .cups.smbp .cups.smbs .cups.port .cups.serv -side bottom -fill x
	pack .cups.f -side top -fill both -expand 1

	center_win .cups
}

proc sound_dialog {} {

	global is_windows

	toplev .snd
	wm title .snd "ESD/ARTSD Sound Tunnelling"

	global uname
	if {$uname == "Darwin"} {
		scroll_text .snd.f 80 20
	} else {
		scroll_text .snd.f 80 30
	}

	set msg {
    Sound tunnelling to a sound daemon requires SSH be used to set up
    the service port redirection.  This will be either of the "Use SSH"
    or "SSH + SSL" modes under "Options".  Pure SSL tunnelling
    will not work.

    This method requires working Sound daemon (e.g. ESD or ARTSD) software
    setups on both the remote and local sides of the connection.

    Often this means you want to run your ENTIRE remote desktop with ALL
    applications instructed to use the sound daemon's network port.  E.g.

        esddsp -s localhost:16001  startkde
        esddsp -s localhost:16001  gnome-session

    and similarly for artsdsp, etc.  You put this in your ~/.xession,
    or other startup file.  This is non standard.  If you do not want to
    do this you still can direct *individual* sound applications through
    the tunnel, for example "esddsp -s localhost:16001 soundapp", where
    "soundapp" is some application that makes noise (say xmms or mpg123).

    Also, usually the remote Sound daemon must be killed BEFORE the SSH port
    redir is established (because it is listening on the port we want to use
    for the SSH redir), and, presumably, restarted when the VNC connection
    finished.

    One may also want to start and kill a local sound daemon that will
    play the sound received over the network on the local machine.

    You can indicate the remote and local Sound daemon commands below and
    how they should be killed and/or restart.  Some examples:

        esd -promiscuous -as 5 -port 16001 -tcp -bind 127.0.0.1
        artsd -n -p 7265 -F 10 -S 4096 -n -s 5 -m artsmessage -l 3 -f

    or you can leave some or all blank and kill/start them manually.

    For convenience, a Windows port of ESD is provided in the util/esound
    directory, and so this might work for a Local command:

        esound\esd -promiscuous -as 5 -port 16001 -tcp -bind 127.0.0.1

    NOTE: If you indicate "Remote Sound daemon: Kill at start." below,
    then THERE WILL BE TWO SSH'S: THE FIRST ONE TO KILL THE DAEMON.
    So you may need to supply TWO SSH PASSWORDS, unless you are using
    something like ssh-agent(1), the Putty PW setting, etc.

    You will also need to supply the remote and local sound ports for
    the SSH redirs.  For esd the default port is 16001, but you can choose
    another one if you prefer.

    For "Local Sound Port" you can also supply "host:port" instead of just
    a numerical port to specify non-localhost connections, e.g. to another
    nearby machine.

    For more info see: http://www.karlrunge.com/x11vnc/#faq-sound
}
	.snd.f.t insert end $msg

	global sound_daemon_remote_port sound_daemon_local_port sound_daemon_local_cmd
	if {$sound_daemon_remote_port == ""} {
		set sound_daemon_remote_port 16001
	}
	if {$sound_daemon_local_port == ""} {
		set sound_daemon_local_port 16001
	}

	if {$sound_daemon_local_cmd == ""} {
		global is_windows
		if {$is_windows} {
			set sound_daemon_local_cmd {esound\esd -promiscuous -as 5 -port %PORT -tcp -bind 127.0.0.1}
		} else {
			set sound_daemon_local_cmd {esd -promiscuous -as 5 -port %PORT -tcp -bind 127.0.0.1}
		}
		regsub {%PORT} $sound_daemon_local_cmd $sound_daemon_local_port sound_daemon_local_cmd
	}


	frame .snd.remote
	label .snd.remote.l -anchor w -text "Remote Sound daemon cmd: "
	entry .snd.remote.e -width 45 -textvariable sound_daemon_remote_cmd
	pack .snd.remote.e -side right
	pack .snd.remote.l -side left -expand 1 -fill x

	frame .snd.local
	label .snd.local.l -anchor w -text "Local Sound daemon cmd:     "
	entry .snd.local.e -width 45 -textvariable sound_daemon_local_cmd
	pack .snd.local.e -side right
	pack .snd.local.l -side left -expand 1 -fill x

	frame .snd.rport
	label .snd.rport.l -anchor w -text "Remote Sound Port: "
	entry .snd.rport.e -width 45 -textvariable sound_daemon_remote_port
	pack .snd.rport.e -side right
	pack .snd.rport.l -side left -expand 1 -fill x

	frame .snd.lport
	label .snd.lport.l -anchor w -text "Local Sound Port:     "
	entry .snd.lport.e -width 45 -textvariable sound_daemon_local_port
	pack .snd.lport.e -side right
	pack .snd.lport.l -side left -expand 1 -fill x


	checkbutton .snd.sdk -anchor w -variable sound_daemon_kill -text \
		"Remote Sound daemon: Kill at start."

	checkbutton .snd.sdr -anchor w -variable sound_daemon_restart -text \
		"Remote Sound daemon: Restart at end."

	checkbutton .snd.sdsl -anchor w -variable sound_daemon_local_start -text \
		"Local Sound daemon: Run at start."

	checkbutton .snd.sdkl -anchor w -variable sound_daemon_local_kill -text \
		"Local Sound daemon: Kill at end."

	button .snd.guess -text "Help me decide ..." -command {}
	.snd.guess configure -state disabled

	global is_win9x 
	if {$is_win9x} {
		.snd.local.e configure -state disabled
		.snd.local.l configure -state disabled
		.snd.sdsl configure -state disabled
		.snd.sdkl configure -state disabled
	}

	button .snd.cancel -text "Cancel" -command {destroy .snd; set use_sound 0}
	bind .snd <Escape> {destroy .snd; set use_sound 0}
	button .snd.done -text "Done" -command {destroy .snd; if {$use_sound} {set_ssh}}

	pack .snd.done .snd.cancel .snd.guess .snd.sdkl .snd.sdsl .snd.sdr .snd.sdk .snd.lport .snd.rport \
		.snd.local .snd.remote -side bottom -fill x
	pack .snd.f -side bottom -fill both -expand 1

	center_win .snd
}

# Share ideas.
# 
# Unix:
# 
# if type smbclient
# first parse smbclient -L localhost -N
# and/or      smbclient -L `hostname` -N
# Get Sharenames and Servers and Domain.
# 
# loop over servers, doing smbclient -L server -N
# pile this into a huge list, sep by disk and printers.
# 
# WinXP:
# 
# parse "NET VIEW" output similarly.
# 
# Have checkbox for each disk.  Set default root to /var/tmp/${USER}-mnts
# Let them change that at once and have it populate. 
# 
# use   //hostname/share  /var/tmp/runge-mnts/hostname/share
# 
# 
# Printers, hmmm.  Can't add to remote cups list...  I guess have the list
# ready for CUPS dialog to suggest which SMB servers they want to redirect
# to...

proc get_hostname {} {
	global is_windows is_win9x
	set str ""
	if {$is_windows} {
		if {1} {
			catch {set str [exec hostname]}
			regsub -all {[\r]} $str "" str
		} else {
			catch {set str [exec net config]}
			if [regexp -nocase {Computer name[ \t]+\\\\([^ \t]+)} $str mv str] {
				;
			} else {
				set str ""
			}
		}
	} else {
		catch {set str [exec hostname]}
	}
	set str [string trim $str]
	return $str
}

proc smb_list_windows {smbhost} {
	global smb_local smb_local_hosts smb_this_host
	global is_win9x
	set dbg 0

	set domain ""

	if {$is_win9x} {
		# exec net view ... doesn't work.
		set smb_this_host "unknown"
		return
	}

	set this_host [get_hostname]
	set This_host [string toupper $this_host]
	set smb_this_host $This_host

	if {$smbhost == $smb_this_host} {
		catch {set out0 [exec net view]}
		regsub -all {[\r]} $out0 "" out0
		foreach line [split $out0 "\n"] {
			if [regexp -nocase {in workgroup ([^ \t]+)} $line mv wg] {
				regsub -all {[.]} $wg "" wg
				set domain $wg
			} elseif [regexp {^\\\\([^ \t]+)[ \t]*(.*)} $line mv host comment] {
				set smb_local($smbhost:server:$host) $comment
			}
		}
	}

	set out1 ""
	set h "\\\\$smbhost"
	catch {set out1 [exec net view $h]}
	regsub -all {[\r]} $out1 "" out1

	if {$dbg} {puts "SMBHOST: $smbhost"}

	set mode ""
	foreach line [split $out1 "\n"] {
		if [regexp {^[ \t]*---} $line] {
			continue
		}
		if [regexp -nocase {The command} $line] {
			continue
		}
		if [regexp -nocase {Shared resources} $line] {
			continue
		}
		if [regexp -nocase {^[ \t]*Share[ \t]*name} $line] {
			set mode "shares"
			continue
		}
		set line [string trim $line]
		if {$line == ""} {
			continue
		}
		if {$mode == "shares"} {
			if [regexp {^([^ \t]+)[ \t]+([^ \t]+)[ \t]*(.*)$} $line mv name type comment] {
				if {$dbg} {
					puts "SHR: $name"
					puts "---: $type"
					puts "---: $comment"
				}
				if [regexp -nocase {^Disk$} $type] {
					set smb_local($smbhost:disk:$name) $comment
				} elseif [regexp -nocase {^Print} $type] {
					set smb_local($smbhost:printer:$name) $comment
				}
			}
		}
	}

	set smb_local($smbhost:domain) $domain
}

proc smb_list_unix {smbhost} {
	global smb_local smb_local_hosts smb_this_host
	set smbclient [in_path smbclient]
	if {[in_path smbclient] == ""} {
		return ""
	}
	set dbg 0

	set this_host [get_hostname]
	set This_host [string toupper $this_host]
	set smb_this_host $This_host

	set out1 ""
	catch {set out1 [exec smbclient -N -L $smbhost 2>@ stdout]}

	if {$dbg} {puts "SMBHOST: $smbhost"}
	if {$smbhost == $this_host || $smbhost == $This_host} {
		if {$out1 == ""} {
			catch {set out1 [exec smbclient -N -L localhost 2>@ stdout]}
		}
	}

	set domain ""
	set mode ""
	foreach line [split $out1 "\n"] {
		if [regexp {^[ \t]*---} $line] {
			continue
		}
		if [regexp {Anonymous login} $line] {
			continue
		}
		if {$domain == "" && [regexp {Domain=\[([^\]]+)\]} $line mv domain]} {
			if {$dbg} {puts "DOM: $domain"}
			continue
		}
		if [regexp {^[ \t]*Sharename} $line] {
			set mode "shares"
			continue
		}
		if [regexp {^[ \t]*Server} $line] {
			set mode "server"
			continue
		}
		if [regexp {^[ \t]*Workgroup} $line] {
			set mode "workgroup"
			continue
		}
		set line [string trim $line]
		if {$mode == "shares"} {
			if [regexp {^([^ \t]+)[ \t]+([^ \t]+)[ \t]*(.*)$} $line mv name type comment] {
				if {$dbg} {
					puts "SHR: $name"
					puts "---: $type"
					puts "---: $comment"
				}
				if [regexp -nocase {^Disk$} $type] {
					set smb_local($smbhost:disk:$name) $comment
				} elseif [regexp -nocase {^Printer$} $type] {
					set smb_local($smbhost:printer:$name) $comment
				}
			}
		} elseif {$mode == "server"} {
			if [regexp {^([^ \t]+)[ \t]*(.*)$} $line mv host comment] {
				if {$dbg} {
					puts "SVR: $host"
					puts "---: $comment"
				}
				set smb_local($smbhost:server:$host) $comment
			}
		} elseif {$mode == "workgroup"} {
			if [regexp {^([^ \t]+)[ \t]+(.*)$} $line mv work host] {
				if {$dbg} {
					puts "WRK: $work"
					puts "---: $host"
				}
				if {$host != ""} {
					set smb_local($smbhost:master:$work) $host
				}
			}
		}
	}

	set smb_local($smbhost:domain) $domain
}

proc smb_list {} {
	global is_windows smb_local smb_local_hosts
	global smb_host_list

	set smb_local(null) ""

	if {! [info exists smb_host_list]} {
		set smb_host_list ""
	}
	if [info exists smb_local] {
		unset smb_local
	}
	if [info exists smb_local_hosts] {
		unset smb_local_hosts
	}
			
	set this_host [get_hostname]
	set this_host [string toupper $this_host]
	if {$is_windows} {
		smb_list_windows $this_host
	} else {
		smb_list_unix $this_host
	}
	set did($this_host) 1 
	set keys [array names smb_local]
	foreach item [split $smb_host_list] {
		if {$item != ""} {
			set item [string toupper $item]
			lappend keys "$this_host:server:$item"
		}
	}
	foreach key $keys {
		if [regexp "^$this_host:server:(.*)\$" $key mv host]  {
			if {$host == ""} {
				continue
			}
			set smb_local_hosts($host) 1
			if {! [info exists did($host)]} {
				if {$is_windows} {
					smb_list_windows $host
				} else {
					smb_list_unix $host
				}
				set did($host) 1 
			}
		}
	}
}

proc smb_check_selected {} {
	global smbmount_exists smbmount_sumode
	global smb_selected smb_selected_mnt smb_selected_cb smb_selected_en

	set ok 0
	if {$smbmount_exists && $smbmount_sumode != "dontknow"} {
		set ok 1
	}
	set state disabled
	if {$ok} {
		set state normal
	}

	foreach cb [array names smb_selected_cb] {
		catch {$cb configure -state $state}
	}
	foreach en [array names smb_selected_en] {
		catch {$en configure -state $state}
	}
}

proc make_share_widgets {w} {
	
	set share_label $w.f.hl
	catch {$share_label configure -text "Share Name: PROBING ..."}
	update

	smb_list

	set saw_f 0
	foreach child [winfo children $w] {
		if {$child == "$w.f"} {
			set saw_f 1
			continue
		}
		catch {destroy $child}
	}

	set w1 47
	set w2 44

	if {! $saw_f} {
		set wf $w.f
		frame $wf
		label $wf.hl -width $w1 -text "Share Name:" -anchor w
		label $wf.hr -width $w2 -text "  Mount Point:" -anchor w

		pack $wf.hl $wf.hr -side left -expand 1
		pack $wf -side top -fill x

		.smbwiz.f.t window create end -window $w
	}

	global smb_local smb_local_hosts smb_this_host smb_selected smb_selected_mnt
	global smb_selected_host smb_selected_name
	global smb_selected_cb smb_selected_en
	global smb_host_list
	if [info exists smb_selected]      {array unset smb_selected }
	if [info exists smb_selected_mnt]  {array unset smb_selected_mnt}
	if [info exists smb_selected_cb]   {array unset smb_selected_cb}
	if [info exists smb_selected_en]   {array unset smb_selected_en}
	if [info exists smb_selected_host] {array unset smb_selected_host}
	if [info exists smb_selected_name] {array unset smb_selected_name}

	set hosts [list $smb_this_host]
	lappend hosts [lsort [array names smb_local_hosts]]

	set smb_host_list ""
	set i 0

	global smb_mount_prefix
	set smb_mount_prefix "/var/tmp/%USER-mnts"

	foreach host [lsort [array names smb_local_hosts]] {

		if [info exists did($host)] {
			continue
		}
		set did($host) 1

		append smb_host_list "$host "

		foreach key [lsort [array names smb_local]] {
			if [regexp {^([^:]+):([^:]+):(.*)$} $key mv host2 type name] {
				if {$host2 != $host}  {
					continue
				}
				if {$type != "disk"} {
					continue
				}
				set wf $w.f$i
				frame $wf
				checkbutton $wf.c -anchor w -width $w1 -variable smb_selected($i) \
					-text "//$host/$name" -relief ridge 
				if {! [info exists smb_selected($i)]} {
					set smb_selected($i) 0
				}

				entry $wf.e -width $w2 -textvariable smb_selected_mnt($i)
				set smb_selected_mnt($i) "$smb_mount_prefix/$host/$name"

				set smb_selected_host($i) $host
				set smb_selected_name($i) $name

				set smb_selected_cb($wf.c) $i
				set smb_selected_en($wf.e) $i
				set comment $smb_local($key)

				bind $wf.c <Enter> "$share_label configure -text {Share Name: $comment}"
				bind $wf.c <Leave> "$share_label configure -text {Share Name:}"

				$wf.c configure -state disabled
				$wf.e configure -state disabled

				pack $wf.c $wf.e -side left -expand 1
				pack $wf -side top -fill x
				incr i
			}
		}
	}
	if {$i == 0} {
		global is_win9x
		$share_label configure -text {Share Name: No SMB Share Hosts were found!}
		if {$is_win9x} {
			.smbwiz.f.t insert end "\n(this feature does not work on Win9x you have have to enter them manually: //HOST/share /var/tmp/mymnt)\n"
		}
	} else {
		$share_label configure -text "Share Name: Found $i SMB Shares"
	}
	smb_check_selected
}

proc smb_help_me_decide {} {
	global is_windows
	global smb_local smb_local_hosts smb_this_host smb_selected smb_selected_mnt
	global smb_selected_host smb_selected_name
	global smb_selected_cb smb_selected_en
	global smb_host_list

	toplev .smbwiz
	set title "SMB Filesystem Tunnelling -- Help Me Decide"
	wm title .smbwiz $title
	set id "  "

	scroll_text .smbwiz.f 100 40

	set msg {
For now you will have to verify the following information manually.

You can do this by either logging into the remote machine to find the info or asking the sysadmin for it.  

}

	if {! $is_windows} {
		.smbwiz.f.t configure -font {Helvetica -12 bold}
	}
	.smbwiz.f.t insert end $msg

	set w .smbwiz.f.t.f1
	frame $w -bd 1 -relief ridge -cursor {top_left_arrow}

	.smbwiz.f.t insert end "\n"

	.smbwiz.f.t insert end "1) Indicate the existence of the 'smbmount' command on the remote system:\n"
	.smbwiz.f.t insert end "\n$id"
	global smbmount_exists
	set smbmount_exists 0

	checkbutton $w.smbmount_exists -pady 1 -anchor w -variable smbmount_exists \
		-text "Yes, the 'smbmount' command exists on the remote system." \
		-command smb_check_selected

	pack $w.smbmount_exists
	.smbwiz.f.t window create end -window $w

	.smbwiz.f.t insert end "\n\n\n"

	set w .smbwiz.f.t.f2
	frame $w -bd 1 -relief ridge -cursor {top_left_arrow}

	.smbwiz.f.t insert end "2) Indicate your authorization to run 'smbmount' on the remote system:\n"
	.smbwiz.f.t insert end "\n$id"
	global smbmount_sumode
	set smbmount_sumode "dontknow"

	radiobutton $w.dk -pady 1 -anchor w -variable smbmount_sumode -value dontknow \
		-text "I do not know if I can mount SMB shares on the remote system via 'smbmount'" \
		-command smb_check_selected
	pack $w.dk -side top -fill x

	radiobutton $w.su -pady 1 -anchor w -variable smbmount_sumode -value su \
		-text "I know the Password to run commands as root on the remote system via 'su'" \
		-command smb_check_selected
	pack $w.su -side top -fill x

	radiobutton $w.sudo -pady 1 -anchor w -variable smbmount_sumode -value sudo \
		-text "I know the Password to run commands as root on the remote system via 'sudo'" \
		-command smb_check_selected
	pack $w.sudo -side top -fill x

	radiobutton $w.ru -pady 1 -anchor w -variable smbmount_sumode -value none \
		-text "I do not need to be root on the remote system to mount SMB shares via 'smbmount'" \
		-command smb_check_selected
	pack $w.ru -side top -fill x

	.smbwiz.f.t window create end -window $w

	global smb_wiz_done
	set smb_wiz_done 0

	button .smbwiz.cancel -text "Cancel" -command {set smb_wiz_done 1}
	button .smbwiz.done -text "Done" -command {set smb_wiz_done 1}
	pack .smbwiz.done -side bottom -fill x 
	pack .smbwiz.f -side top -fill both -expand 1

	wm protocol .smbwiz WM_DELETE_WINDOW {set smb_wiz_done 1}
	center_win .smbwiz

	wm title .smbwiz "Searching for Local SMB shares..."
	update
	wm title .smbwiz $title

	global smb_local smb_this_host
	.smbwiz.f.t insert end "\n\n\n"

	set w .smbwiz.f.t.f3
	catch {destroy $w}
	frame $w -bd 1 -relief ridge -cursor {top_left_arrow}

	.smbwiz.f.t insert end "3) Select SMB shares to mount and their mount point on the remote system:\n"
	.smbwiz.f.t insert end "\n${id}"

	make_share_widgets $w

	.smbwiz.f.t insert end "\n(%USER will be expanded to the username on the remote system and %HOME the home directory)\n"

	.smbwiz.f.t insert end "\n\n\n"

	.smbwiz.f.t insert end "You can change the list of Local SMB hosts to probe and the mount point prefix here:\n"
	.smbwiz.f.t insert end "\n$id"
	set w .smbwiz.f.t.f4
	frame $w -bd 1 -relief ridge -cursor {top_left_arrow}
	set wf .smbwiz.f.t.f4.f
	frame $wf
	label $wf.l -text "SMB Hosts:  "  -anchor w
	entry $wf.e -textvariable smb_host_list -width 60
	button $wf.b -text "Apply" -command {make_share_widgets .smbwiz.f.t.f3}
	bind $wf.e <Return> "$wf.b invoke"
	pack $wf.l $wf.e $wf.b -side left
	pack $wf
	pack $w

	.smbwiz.f.t window create end -window $w

	.smbwiz.f.t insert end "\n$id"

	set w .smbwiz.f.t.f5
	frame $w -bd 1 -relief ridge -cursor {top_left_arrow}
	set wf .smbwiz.f.t.f5.f
	frame $wf
	label $wf.l -text "Mount Prefix:"  -anchor w
	entry $wf.e -textvariable smb_mount_prefix -width 60
	button $wf.b -text "Apply" -command {apply_mount_point_prefix .smbwiz.f.t.f5.f.e}
	bind $wf.e <Return> "$wf.b invoke"
	pack $wf.l $wf.e $wf.b -side left
	pack $wf
	pack $w

	.smbwiz.f.t window create end -window $w

	.smbwiz.f.t insert end "\n\n\n"

	.smbwiz.f.t see 1.0
	.smbwiz.f.t configure -state disabled
	update

	vwait smb_wiz_done
	catch {destroy .smbwiz}

	if {! $smbmount_exists || $smbmount_sumode == "dontknow"} {
		tk_messageBox -type ok -parent .oa -icon warning -message "Sorry we couldn't help out!\n'smbmount' info on the remote system is required for SMB mounting" -title "SMB mounting -- aborting"
		global use_smbmnt
		set use_smbmnt 0
		catch {raise .oa}
		return
	}
	global smb_su_mode
	set smb_su_mode $smbmount_sumode

	set max 0
	foreach en [array names smb_selected_en] {
		set i $smb_selected_en($en)
		set host $smb_selected_host($i)
		set name $smb_selected_name($i)

		set len [string length "//$host/$name"]
		if {$len > $max} {
			set max $len
		}
	}

	set max [expr $max + 8]

	set strs ""
	foreach en [array names smb_selected_en] {
		set i $smb_selected_en($en)
		if {! $smb_selected($i)} {
			continue
		}
		set host $smb_selected_host($i)
		set name $smb_selected_name($i)
		set mnt $smb_selected_mnt($i)

		set share "//$host/$name"
		set share [format "%-${max}s" $share]
		
		lappend strs "$share $mnt"
	}
	set text ""
	foreach str [lsort $strs] {
		append text "$str\n"
	}

	global smb_mount_list
	set smb_mount_list $text

	smb_dialog
}

proc apply_mount_point_prefix {w} {
	global smb_selected_host smb_selected_name
	global smb_selected_en smb_selected_mnt

	set prefix ""
	catch {set prefix [$w get]}
	if {$prefix == ""} {
		mesg "No mount prefix."
		bell
		return
	}

	foreach en [array names smb_selected_en] {
		set i $smb_selected_en($en)
		set host $smb_selected_host($i)
		set name $smb_selected_name($i)
		set smb_selected_mnt($i) "$prefix/$host/$name"
	}
}

proc smb_dialog {} {
	toplev .smb
	wm title .smb "SMB Filesystem Tunnelling"
	global smb_su_mode smb_mount_list
	global use_smbmnt

	global help_font

	global uname
	if {$uname == "Darwin"} {
		scroll_text .smb.f 80 25
	} else {
		scroll_text .smb.f
	}

	set msg {
    Windows/Samba Filesystem mounting requires SSH be used to set up the
    SMB service port redirection.  This will be either of the "Use SSH"
    or "SSH + SSL" modes under "Options".  Pure SSL tunnelling
    will not work.

    This method requires a working Samba software setup on the remote
    side of the connection (VNC server) and existing Samba or Windows file
    server(s) on the local side (VNC viewer).

    The smbmount(8) program MUST be installed on the remote side. This
    evidently limits the mounting to Linux systems.  Let us know of similar
    utilities on other Unixes.  Mounting onto remote Windows machines is
    currently not supported (our SSH mode with services setup only works
    to Unix).

    Depending on how smbmount is configured you may be able to run it
    as a regular user, or it may require running under su(1) or sudo(8)
    (root password or user password required, respectively).  You select
    which one you want via the checkbuttons below.

    In addition to a possible su(1) or sudo(8) password, you may ALSO
    need to supply passwords to mount each SMB share. This is an SMB passwd.
    If it has no password just hit enter after the "Password:" prompt.

    The passwords are supplied when the 1st SSH connection starts up;
    be prepared to respond to them.

    NOTE: USE OF SMB TUNNELLING MODE WILL REQUIRE TWO SSH'S, AND SO YOU
    MAY NEED TO SUPPLY TWO LOGIN PASSWORDS UNLESS YOU ARE USING SOMETHING
    LIKE ssh-agent(1) or the Putty PW setting.
    %WIN

    To indicate the Windows/Samba shares to mount enter them one per line
    in one of the forms:

      //machine1/share   ~/Desktop/my-mount1
      //machine2/fubar   /var/tmp/my-foobar2  192.168.100.53:3456
      1139  //machine3/baz  /var/tmp/baz      [...]

    The first part is the standard SMB host and share name //hostname/dir
    (note this share is on the local viewer-side not on the remote end).
    A leading '#' will cause the entire line to be skipped.

    The second part, e.g. /var/tmp/my-foobar2, is the directory to mount
    the share on the remote (VNC Server) side.  You must be able to
    write to this directory.  It will be created if it does not exist.
    A leading character ~ will be expanded to $HOME.  So will the string
    %HOME.  The string %USER will get expanded to the remote username.

    An optional part like 192.168.100.53:3456 is used to specify the real
    hostname or IP address, and possible non-standard port, on the local
    side if for some reason the //hostname is not sufficient.

    An optional leading numerical value, 1139 in the above example, indicates
    which port to use on the Remote side to SSH redirect to the local side.
    Otherwise a random one is tried (a unique one is needed for each SMB
    server:port combination).  A fixed one is preferred: choose a free
    remote port.

    The standard SMB service ports (local side) are 445 and 139.  139 is
    used by this application.

    Sometimes "localhost" will not work on Windows machines for a share
    hostname, and you will have to specify a different network interface
    (e.g. the machine's IP address).  If you use the literal string "IP"
    it will be attempted to replace it with the numerical IP address, e.g.:

      //machine1/share   ~/Desktop/my-mount1   IP

    VERY IMPORTANT: Before terminating the VNC Connection, make sure no
    applications are using any of the SMB shares (or shells are cd-ed
    into the share).  This way the shares will be automatically unmounted.
    Otherwise you will need to log in again, stop processes from using
    the share, become root and umount the shares manually ("smbumount
    /path/to/share", etc.)

    For more info see: http://www.karlrunge.com/x11vnc/#faq-smb-shares
}

	set msg2 {
    To speed up moving to the next step, iconify the first SSH console
    when you are done entering passwords, etc. and then click on the
    main panel 'VNC Host:Display' label.
}

	global is_windows
	if {! $is_windows} {
		regsub { *%WIN} $msg "" msg
	} else {
		set msg2 [string trim $msg2]
		regsub { *%WIN} $msg "    $msg2" msg
	}
	.smb.f.t insert end $msg

	frame .smb.r
	label .smb.r.l -text "smbmount(8) auth mode:" -relief ridge
	radiobutton .smb.r.none -text "None" -variable smb_su_mode -value "none"
	radiobutton .smb.r.su   -text "su(1)" -variable smb_su_mode -value "su"
	radiobutton .smb.r.sudo -text "sudo(8)" -variable smb_su_mode -value "sudo"

	pack .smb.r.l .smb.r.none .smb.r.sudo .smb.r.su -side left -fill x

	label .smb.info -text "Supply the mounts (one per line) below:" -anchor w -relief ridge

	eval text .smb.mnts -width 80 -height 5 $help_font
	.smb.mnts insert end $smb_mount_list

	button .smb.guess -text "Help me decide ..." -command {destroy .smb; smb_help_me_decide}

	button .smb.cancel -text "Cancel" -command {set use_smbmnt 0; destroy .smb}
	bind .smb <Escape> {set use_smbmnt 0; destroy .smb}
	button .smb.done -text "Done" -command {if {$use_smbmnt} {set_ssh; set smb_mount_list [.smb.mnts get 1.0 end]}; destroy .smb}

	pack .smb.done .smb.cancel .smb.guess .smb.mnts .smb.info .smb.r -side bottom -fill x
	pack .smb.f -side top -fill both -expand 1

	center_win .smb
}

proc help_advanced_opts {} {
	toplev .ah

	scroll_text_dismiss .ah.f

	center_win .ah

	wm title .ah "Advanced Options Help"

	set msg {
    These Advanced Options that may require extra software installed on
    the VNC server-side (the remote server machine) and/or on the VNC
    client-side (where this gui is running).

    The Service redirection options, CUPS, ESD/ARTSD, and SMB will
    require that you use SSH for tunneling so that they can use the -R
    port redirection will be enabled for each service.  I.e. "Use SSH"
    or "SSH + SSL" mode.

    These options may also require additional configuration to get them
    to work properly.  Please submit bug reports if it appears it should
    be working for your setup but is not.

    Brief descriptions:

         CUPS Print tunnelling: redirect localhost:6631 (say) on the VNC
         server to your local CUPS server.

         ESD/ARTSD Audio tunnelling: redirect localhost:16001 (say) on
         the VNC server to your local ESD, etc. sound server.

         SMB mount tunnelling: redirect localhost:1139 (say) on the VNC
         server and through that mount SMB file shares from your local
         server.  The remote machine must be Linux with smbmount installed.

         Change vncviewer: specify a non-bundled VNC Viewer (e.g.
         UltraVNC or RealVNC) to run instead of the bundled TightVNC Viewer.

         Extra Redirs: specify additional -L port:host:port and 
         -R port:host:port cmdline options for SSH to enable additional
         services.

         Port Knocking: for "closed port" services, first "knock" on the
         firewall ports in a certain way to open the door for SSH or SSL.
         The port can also be closed when the encrypted VNC connection
         finishes.

	 Y Crop: this is for x11vnc's -ncache client side caching scheme
	 with our Unix TightVNC viewer.  Sets the Y value to "crop" the
	 viewer size at (below the cut is the pixel cache region you do
	 not want to see).  If the screen is tall (H > 2*W) ycropping
	 will be autodetected, or you can set to -1 to force autodection.
	 Otherwise, set it to the desired Y value.  You can also set
	 the scrollbar width (very thin by default) by appending ",sb=N"
	 (or use ",sb=N" by itself to just set the scrollbar width).

         Include:  Profile template(s) to load before loading a profile
         (see Load Profile under "Options").  For example if you Save a
         profile called "globals" that has some settings you use often,
         then just supply "Include: globals" to have them applied.
         You may supply a comma or space separated list of templates
         to include.  They can be full path names or basenames relative
         to the profiles directory.  You do not need to supply the .vnc
         suffix.  The non-default settings in them will be applied first,
         and then any values in the loaded Profile will override them.


    About the CheckButtons:

         Ahem, Well...., yes quite a klunky UI: you have to toggle the
         CheckButton to pull up the Dialog box a 2nd, etc. time... don't
         worry your settings will still be there!
}

	.ah.f.t insert end $msg
	jiggle_text .ah.f.t
}

proc set_viewer_path {} {
	global change_vncviewer_path
	set change_vncviewer_path [tk_getOpenFile -parent .chviewer]
	catch {raise .chviewer}
	update
}

proc change_vncviewer_dialog {} {
	global change_vncviewer change_vncviewer_path vncviewer_realvnc4
	
	toplev .chviewer
	wm title .chviewer "Change VNC Viewer"

	global help_font
	eval text .chviewer.t -width 90 -height 29 $help_font
	apply_bg .chviewer.t

	set msg {
    To use your own VNC Viewer (i.e. one installed by you, not included in this
    package), e.g. UltraVNC or RealVNC, type in the program name, or browse for
    the full path to it.  You can put command line arguments after the program.

    Note that due to incompatibilities with respect to command line options
    there may be issues, especially if many command line options are supplied.
    You can specify your own command line options below if you like (and try to
    avoid setting any others in this GUI under "Options").

    If the path to the program name has any spaces it in, please surround it with
    double quotes, e.g.

        "C:\Program Files\My Vnc Viewer\VNCVIEWER.EXE"

    Make sure the very first character is a quote.  You should quote the command
    even if it is only the command line arguments that need extra protection:

        "wine" -- "/home/fred/Program Flies/UltraVNC-1.0.2.exe" /64colors

    Since the command line options differ between them greatly, if you know it
    is of the RealVNC 4.x flavor, indicate on the check box. Otherwise we guess.

    To have SSVNC act as a general STUNNEL redirector (no VNC) set the viewer to
    be "xmessage OK" or "xmessage <port>" or "sleep n" or "sleep n <port>" (or
    "NOTEPAD" on Windows).  The default listen port is 5930.  The destination is
    set in "VNC Host:Display" (for a remote port less then 200 use the negative
    of the port value).
}
	.chviewer.t insert end $msg

	frame .chviewer.path
	label .chviewer.path.l -text "VNC Viewer:"
	entry .chviewer.path.e -width 40 -textvariable change_vncviewer_path
	button .chviewer.path.b -text "Browse..." -command set_viewer_path
	checkbutton .chviewer.path.r -anchor w -variable vncviewer_realvnc4 -text \
		"RealVNC 4.x"

	pack .chviewer.path.l -side left
	pack .chviewer.path.e -side left -expand 1 -fill x
	pack .chviewer.path.b -side left
	pack .chviewer.path.r -side left

	button .chviewer.cancel -text "Cancel" -command {destroy .chviewer; set change_vncviewer 0}
	bind .chviewer <Escape> {destroy .chviewer; set change_vncviewer 0}
	button .chviewer.done -text "Done" -command {destroy .chviewer; catch {raise .oa}}

	pack .chviewer.t .chviewer.path .chviewer.cancel .chviewer.done -side top -fill x

	center_win .chviewer
	wm resizable .chviewer 1 0

	focus .chviewer.path.e 
}

proc port_redir_dialog {} {
	global additional_port_redirs additional_port_redirs_list
	
	toplev .redirs
	wm title .redirs "Additional Port Redirections"

	global help_font uname
	if {$uname == "Darwin"} {
		eval text .redirs.t -width 80 -height 35 $help_font
	} else {
		eval text .redirs.t -width 80 -height 35 $help_font
	}
	apply_bg .redirs.t

	set msg {
    Specify any additional SSH port redirections you desire for the
    connection.  Put as many as you want separated by spaces.  These only
    apply to SSH and SSH+SSL connections, they do not apply to Pure SSL
    connections.

    -L port1:host:port2  will listen on port1 on the local machine (where
                         you are sitting) and redirect them to port2 on
                         "host".  "host" is relative to the remote side
                         (VNC Server).  Use "localhost" for the remote
                         machine itself.

    -R port1:host:port2  will listen on port1 on the remote machine
                         (where the VNC server is running) and redirect
                         them to port2 on "host".  "host" is relative
                         to the local side (where you are sitting).
                         Use "localhost" for this machine.

    Perhaps you want a redir to a web server inside an intranet:

        -L 8001:web-int:80

    Or to redir a remote port to your local SSH daemon:

        -R 5022:localhost:22

    etc.  There are many interesting possibilities.

    Sometimes, especially for Windows Shares, you cannot do a -R redir to
    localhost, but need to supply the IP address of the network interface
    (e.g. by default the Shares do not listen on localhost:139).  As a
    convenience you can do something like -R 1139:IP:139 (for any port
    numbers) and the IP will be attempted to be expanded.  If this fails
    for some reason you will have to use the actual numerical IP address.
}
	.redirs.t insert end $msg

	frame .redirs.path
	label .redirs.path.l -text "Port Redirs:"
	entry .redirs.path.e -width 40 -textvariable additional_port_redirs_list

	pack .redirs.path.l -side left
	pack .redirs.path.e -side left -expand 1 -fill x

	button .redirs.cancel -text "Cancel" -command {set additional_port_redirs 0; destroy .redirs}
	bind .redirs <Escape> {set additional_port_redirs 0; destroy .redirs}
	button .redirs.done -text "Done" -command {destroy .redirs}

	pack .redirs.t .redirs.path .redirs.cancel .redirs.done -side top -fill x

	center_win .redirs
	wm resizable .redirs 1 0

	focus .redirs.path.e
}

proc find_netcat {} {
	global is_windows

	set nc ""

	if {! $is_windows} {
		set nc [in_path "netcat"]
		if {$nc == ""} {
			set nc [in_path "nc"]
		}
	} else {
		set try "netcat.exe"
		if [file exists $try] {
			set nc $try
		}
	}
	return $nc
}

proc pk_expand {cmd host} {
	global tcl_platform
	set secs [clock seconds]
	set msecs [clock clicks -milliseconds]
	set user $tcl_platform(user)
	if [regexp {%IP} $cmd] {
		set ip [guess_ip]
		if {$ip == ""} {
			set ip "unknown"
		}
		regsub -all {%IP} $cmd $ip cmd
	}
	if [regexp {%NAT} $cmd] {
		set ip [guess_nat_ip]
		regsub -all {%NAT} $cmd $ip cmd
	}
	regsub -all {%HOST} $cmd $host cmd
	regsub -all {%USER} $cmd $user cmd
	regsub -all {%SECS} $cmd $secs cmd
	regsub -all {%MSECS} $cmd $msecs cmd

	return $cmd
}

proc backtick_expand {str} {
	set str0 $str
	set collect ""
	set count 0
	while {[regexp {^(.*)`([^`]+)`(.*)$} $str mv p1 cmd p2]} {
		set out [eval exec $cmd]
		set str "$p1$out$p2"
		incr count
		if {$count > 10}  {
			break
		}
	}
	return $str
}

proc read_from_pad {file} {
	set fh ""
	if {[catch {set fh [open $file "r"]}] != 0} {
		return "FAIL"
	}

	set accum ""
	set match ""
	while {[gets $fh line] > -1} {
		if [regexp {^[ \t]*#} $line] {
			append accum "$line\n"
		} elseif [regexp {^[ \t]*$} $line] {
			append accum "$line\n"
		} elseif {$match == ""} {
			set match $line
			append accum "# $line\n"
		} else {
			append accum "$line\n"
		}
	}

	close $fh

	if {$match == ""} {
		return "FAIL"
	}
	
	if {[catch {set fh [open $file "w"]}] != 0} {
		return "FAIL"
	}

	puts -nonewline $fh $accum

	return $match
}

proc do_port_knock {hp mode} {
	global use_port_knocking port_knocking_list
	global is_windows

	if {! $use_port_knocking} {
		return 1
	}
	if {$port_knocking_list == ""} {
		return 1
	}
	set list $port_knocking_list

	if {$mode == "finish"} {
		if {! [regexp {FINISH} $list]} {
			mesg "PortKnock(finish): done"
			return 1
		} else {
			regsub {^.*FINISH} $list "" list
		}
	} elseif {$mode == "start"} {
		if {[regexp {FINISH} $list]} {
			regsub {FINISH.*$} $list "" list
		}
	}

	set default_delay 150

	set host [string trim $hp]
	regsub {^vnc://} $host "" host
	regsub {^.*@} $host "" host
	regsub {:.*$} $host "" host
	set host0 [string trim $host]

	if {$host0 == ""} {
		bell
		mesg "PortKnock: No host: $hp"
		return 0
	}

	set m ""
	
	if [regexp {PAD=([^\n]+)} $list mv padfile] {
		set tlist [read_from_pad $padfile] 
		set tlist [string trim $tlist]
		if {$tlist == "" || $tlist == "FAIL"} {
			raise .
			tk_messageBox -type ok -icon error \
				-message "Failed to read entry from $padfile" \
				-title "Error: Padfile $padfile"
			return 0
		}
		regsub -all {PAD=([^\n]+)} $list $tlist list
	}

	set spl ",\n\r"
	if [regexp {CMD=}   $list] {set spl "\n\r"}
	if [regexp {CMDX=}  $list] {set spl "\n\r"}
	if [regexp {SEND=}  $list] {set spl "\n\r"}
	if [regexp {SENDX=} $list] {set spl "\n\r"}

	set i 0
	set pi 0

	foreach line [split $list $spl] {
		set line [string trim $line]
		set line0 $line

		if {$line == ""} {
			continue
		}
		if [regexp {^#} $line] {
			continue
		}

		if [regexp {^sleep[ \t][ \t]*([0-9][0-9]*)} $line mv sl] {
			set m "PortKnock: sleep $sl"
			mesg $m
			after $sl
			continue
		}
		if [regexp {^delay[ \t][ \t]*([0-9][0-9]*)} $line mv sl] {
			set m "PortKnock: delay=$sl"
			mesg $m
			set default_delay $sl
			continue
		}

		if [regexp {^CMD=(.*)} $line mv cmd] {
			set m "PortKnock: CMD: $cmd"
			mesg $m
			eval exec $cmd
			continue
		}
		if [regexp {^CMDX=(.*)} $line mv cmd] {
			set cmd [pk_expand $cmd $host0]
			set m "PortKnock: CMDX: $cmd"
			mesg $m
			eval exec $cmd
			continue
		}
	
		if [regexp {`} $line] {
			#set line [backtick_expand $line]
		}

		set snd ""
		if [regexp {^(.*)SEND=(.*)$} $line mv line snd]  {
			set line [string trim $line]
			set snd [string trim $snd]
			regsub -all {%NEWLINE} $snd "\n" snd
		} elseif [regexp {^(.*)SENDX=(.*)$} $line mv line snd]  {
			set line [string trim $line]
			set snd [string trim $snd]
			set snd [pk_expand $snd $host0]
			regsub -all {%NEWLINE} $snd "\n" snd
		}

		set udp 0
		if [regexp -nocase {[/:]udp} $line] {
			set udp 1
			regsub -all -nocase {[/:]udp} $line " " line
			set line [string trim $line]
		}
		regsub -all -nocase {[/:]tcp} $line " " line
		set line [string trim $line]

		set delay 0
		if [regexp {^(.*)[ \t][ \t]*([0-9][0-9]*)$} $line mv first delay] {
			set line [string trim $first]
		}

		if {[regexp {^(.*):(.*)$} $line mv host port]} {
			;
		} else {
			set host $host0
			set port $line
		}
		set host [string trim $host]
		set port [string trim $port]

		if {$host == ""} {
			set host $host0
		}

		if {$port == ""} {
			bell
			set m "PortKnock: No port found: \"$line0\""
			mesg $m
			return 0
		}
		if {! [regexp {^[0-9][0-9]*$} $port]} {
			bell
			set m "PortKnock: Invalid port: \"$port\""
			mesg $m
			return 0
		}
		regsub {,.*$} $host "" host
		if {[regexp {[ \t]} $host]} {
			bell
			set m "PortKnock: Invalid host: \"$host\""
			mesg $m
			return 0
		}
		if {! [regexp {^[-A-z0-9_.][-A-z0-9_.]*$} $host]} {
			bell
			set m "PortKnock: Invalid host: \"$host\""
			mesg $m
			return 0
		}

		set nc ""
		if {$udp || $snd != ""} {
			set nc [find_netcat]
			if {$nc == ""} {
				bell
				set m "PortKnock: UDP: netcat(1) not found"
				mesg $m
				after 1000
				continue
			}
		}

		if {$snd != ""} {
			global env
			set pfile "payload$pi.txt" 
			if {! $is_windows} {
				set pfile "$env(SSVNC_HOME)/.$pfile"
			}
			set pfiles($pi) $pfile
			incr pi
			set fh [open $pfile "w"]
			puts -nonewline $fh "$snd"
			close $fh

			set m "PortKnock: SEND: $host $port"
			mesg $m
			if {$is_windows} {
				if {$udp} {
					catch {exec $nc -d -u -w 1 "$host" "$port" < $pfile &}
				} else {
					catch {exec $nc -d    -w 1 "$host" "$port" < $pfile &}
				}
			} else {
				if {$udp} {
					catch {exec $nc    -u -w 1 "$host" "$port" < $pfile &}
				} else {
					catch {exec $nc       -w 1 "$host" "$port" < $pfile &}
				}
			}
			catch {after 50; file delete $pfile}
			
		} elseif {$udp} {
			set m "PortKnock: UDP: $host $port"
			mesg $m
			if {! $is_windows} {
				catch {exec echo a | $nc -u -w 1 "$host" "$port" &}
			} else {
				set fh [open "nc_in.txt" "w"]
				puts $fh "a"
				close $fh
				catch {exec $nc -d -u -w 1 "$host" "$port" < "nc_in.txt" &}
			}
		} else {
			set m "PortKnock: TCP: $host $port"
			mesg $m
			set s ""
			set emess ""
			set rc [catch {set s [socket -async $host $port]} emess]
			if {$rc != 0} {
				raise .
				tk_messageBox -type ok -icon error -message $emess -title "Error: socket -async $host $port"
			}
			set socks($i) $s
			# seems we have to close it immediately to avoid multiple SYN's.
			# does not help on Win9x.
			catch {after 30; close $s};
			incr i
		}

		if {$delay == 0} {
			if {$default_delay > 0} {
				after $default_delay
			}
		} elseif {$delay > 0} {
			after $delay
		}
	}

	if {0} {
		for {set j 0} {$j < $i} {incr j} {
			set $s $socks($j)
			if {$s != ""} {
				catch {close $s}	
			}
		}
	}
	for {set j 0} {$j < $pi} {incr j} {
		set f $pfiles($j)
		if {$f != ""} {
			if [file exists $f] {
				after 100
			}
			catch {file delete $f}	
		}
	}
	if {$is_windows} {
		catch {file delete "nc_in.txt"}
	}
	if {$m != ""} {
		set m "$m,"
	}
	if {$mode == "finish"} {
		mesg "PortKnock(finish): done"
	} else {
		mesg "PortKnock: done"
	}
	return 1
}

proc port_knocking_dialog {} {
	toplev .pk
	wm title .pk "Port Knocking"
	global use_port_knocking port_knocking_list

	global help_font

	global uname
	if {$uname == "Darwin"} {
		scroll_text .pk.f 85 25
	} else {
		scroll_text .pk.f 85
	}

	set msg {
 Description:

    Port Knocking is where a network connection to a service is not provided
    to just any client, but rather only to those that immediately prior to
    connecting send a more or less secret pattern of connections to other
    ports on the firewall.

    Somewhat like "knocking" on the door with the correct sequence before it
    being opened (but not necessarily letting you in yet).  It is also possible
    to have a single encrypted packet (e.g. UDP) payload communicate with the
    firewall instead of knocking on a sequence of ports.

    Only after the correct sequence of ports is observed by the firewall does
    it allow the IP address of the client to attempt to connect to the service.

    So, for example, instead of allowing any host on the internet to connect
    to your SSH service and then try to login with a username and password, the
    client first must "tickle" your firewall with the correct sequence of ports.
    Only then will it be allowed to connect to your SSH service at all.

    This does not replace the authentication and security of SSH, it merely
    puts another layer of protection around it. E.g., suppose an exploit for
    SSH was discovered, you would most likely have more time to fix/patch
    the problem than if any client could directly connect to your SSH server.

    For more information http://www.portknocking.org/ and
    http://www.linuxjournal.com/article/6811


 Tip:

    If you just want to use the Port Knocking for an SSH shell and not
    for a VNC tunnel, then specify something like "user@hostname cmd=SHELL"
    (or "user@hostname cmd=PUTTY" on Windows) in the VNC Host:Display entry box
    on the main panel.  This will do everything short of starting the viewer.
    A shortcut for this is Ctrl-S as long as user@hostname is present.
    

 Specifying the Knocks:

    In the text area below "Supply port knocking pattern" you put in the pattern
    of "knocks" needed for this connection.  You can separate the knocks by
    commas or put them one per line.

    Each "knock" is of this form:

           [host:]port[/udp] [delay]

    In the simplest form just a numerical port, e.g. 5433, is supplied.
    Items inside [...] are optional and described below.

    The packet is sent to the same host that the VNC (or SSH) connection will
    be made to.  If you want it to go to a different host or IP use the [host:]
    prefix.  It can be either a hostname or numerical IP.

    A TCP packet is sent by default.

    If you need to send a UDP packet, the netcat (aka "nc") program must be
    installed on Unix (tcl/tk does not support udp connections).  Indicate this
    with "/udp" following the port number (you can also use "/tcp", but since
    it is the default it is not necessary).  (You can also use ":udp" to match
    the knockd syntax).  See the example below.  For convenience a Windows netcat
    binary is supplied.

    The last field, [delay], is an optional number of milliseconds to delay
    before continuing on to the next knock.


 Examples:

           5433, 12321, 1661

           fw.example.com:5433, 12321/udp 3000, 1661 2000

           fw.example.com:5433
           12321/udp 3000
           1661 2000

    Note how the first two examples separate their knocks via commas ",".
    The 3rd example is equivalent to the 2nd and splits them up by new lines.

    Note for each knock any second number (e.g. the "2000" in "1661 2000") is
    a DELAY in milliseconds, not a port number.  If you had a comma separating
    them: "1661, 2000" that would mean two separate knocks: one to port 1661
    followed by one to 2000 (with basically no delay between them).

    In examples 2 and 3, "fw.example.com" represents some machine other than
    the VNC/SSH host.  By default, the VNC/SSH host is the one the packet is
    sent to.

    If one of the items is the string "FINISH", then the part before it is
    used prior to connecting and the part after is used once the connection
    is finished.  This can be used, say, to close the firewall port.  Example:

           5433, 12321, FINISH, 7659, 2314

    (or one can split them up via lines as above.)


 Advanced port knock actions:

    If the string in the text field contains anywhere the strings "CMD=", "CMDX=",
    or "SEND=", then splitting on commas is not done: it is only split on lines.

    Then, if a line begins CMD=... the string after the = is run as an
    external command.  The command could be anything you want, e.g. it could
    be a port-knocking client that does the knocking, perhaps encrypting the
    "knocks" pattern somehow or using a Single Packet Authorization method such
    as http://www.cipherdyne.com/fwknop/

    Extra quotes (sometimes "'foo bar'") may be needed to preserve spaces in
    command line arguments because the tcl/tk eval(n) command is used.  You
    can also use {...} for quoting strings with spaces.

    If a line begins CMDX=... then before the command is run the following
    tokens are expanded to strings:

      %IP       Current machine's IP address (NAT may make this not useful).
      %NAT      Try to get effective IP by contacting http://www.whatismyip.com
      %HOST     The remote host of the connection.
      %USER     The current user.
      %SECS     The current time in seconds (platform dependent).
      %MSECS    Platform dependent time having at least millisecond granularity.

   Lines not matching CMD= or CMDX= are treated as normal port knocks but with
   one exception.  If a line ends in SEND=... (i.e. after the [host:]port,
   etc., part) then the string after the = is sent as a payload for the tcp
   or udp connection to [host:]port.  netcat is used for these SEND cases
   (and must be available on Unix).  If newlines (\n) are needed in the
   SEND string, use %NEWLINE.  Sending binary data is not yet supported;
   use CMD= with your own program.


 Advanced Examples:

      CMD=port_knock_client -password wombat33
      CMDX=port_knock_client -password wombat33 -host %HOST -src %NAT

      fw.example.com:5433/udp SEND=ASDLFKSJDF


 More tricks:

      To temporarily "comment out" a knock, insert a leading "#" character.

      Use "sleep N" to insert a raw sleep for N milliseconds (e.g. between
      CMD=... items or at the very end of the knocks to wait).

      If a knock entry matches "delay N" the default delay is set to
      N milliseconds (it is 150 initially).


 One Time Pads:

      If the text contains a (presumably single) line of the form:

           PAD=/path/to/a/one/time/pad/file

      then that file is opened and the first non-blank line not beginning
      with "#" is used as the knock pattern.  The pad file is rewritten
      with that line starting with a "#" (so it will be skipped next time).

      The PAD=... string is replaced with the read-in knock pattern line.
      So, if needed, one can preface the PAD=... with "delay N" to set the
      default delay, and one can also put a "sleep N" after the PAD=...
      line to indicate a final sleep.  One can also surround the PAD=
      line with other knock and CMD= CMDX= lines, but that usage sounds
      a bit rare.  Example:

           delay 1000
           PAD=C:\My Pads\work-pad1.txt
           sleep 4000


 Port knock only:

      If, in the 'VNC Host:Display' entry, you use "user@hostname cmd=KNOCK"
      then only the port-knocking is performed.  A shortcut for this is
      Ctrl-P as long as hostname is present in the entry box.  If it
      matches cmd=KNOCKF, i.e. an extra "F", then the port-knocking
      "FINISH" sequence is sent, if any.  A shortcut for this Shift-Ctrl-P
      as long as hostname is present.
}
	.pk.f.t insert end $msg

	label .pk.info -text "Supply port knocking pattern:" -anchor w -relief ridge

	eval text .pk.rule -width 80 -height 5 $help_font
	.pk.rule insert end $port_knocking_list

	button .pk.cancel -text "Cancel" -command {set use_port_knocking 0; destroy .pk}
	bind .pk <Escape> {set use_port_knocking 0; destroy .pk}
	button .pk.done -text "Done" -command {if {$use_port_knocking} {set port_knocking_list [.pk.rule get 1.0 end]}; destroy .pk}

	pack .pk.done .pk.cancel .pk.rule .pk.info -side bottom -fill x
	pack .pk.f -side top -fill both -expand 1

	center_win .pk
}


proc set_advanced_options {} {
	global use_cups use_sound use_smbmnt
	global change_vncviewer
	global use_port_knocking port_knocking_list

	catch {destroy .o}
	toplev .oa
	wm title .oa "Advanced Options"

	set i 1

	checkbutton .oa.b$i -anchor w -variable use_cups -text \
		"Enable CUPS Print tunnelling" \
		-command {if {$use_cups} {cups_dialog}}
	incr i

	checkbutton .oa.b$i -anchor w -variable use_sound -text \
		"Enable ESD/ARTSD Audio tunnelling" \
		-command {if {$use_sound} {sound_dialog}}
	incr i

	checkbutton .oa.b$i -anchor w -variable use_smbmnt -text \
		"Enable SMB mount tunnelling" \
		-command {if {$use_smbmnt} {smb_dialog}}
	incr i


	checkbutton .oa.b$i -anchor w -variable change_vncviewer -text \
		"Change VNC Viewer" \
		-command {if {$change_vncviewer} {change_vncviewer_dialog}}
	incr i

	checkbutton .oa.b$i -anchor w -variable additional_port_redirs -text \
		"Additional Port Redirs" \
		-command {if {$additional_port_redirs} {port_redir_dialog}}
	incr i

	checkbutton .oa.b$i -anchor w -variable use_port_knocking -text \
		"Port Knocking" \
		-command {if {$use_port_knocking} {port_knocking_dialog}}
	incr i

	global ycrop_string
	frame .oa.b$i
	label .oa.b$i.l -text "Y Crop: "
	entry .oa.b$i.e -width 10 -textvariable ycrop_string
	pack .oa.b$i.l -side left
	pack .oa.b$i.e -side right -expand 1 -fill x

	incr i

	global include_list
	frame .oa.b$i
	label .oa.b$i.l -text "Include:"
	entry .oa.b$i.e -width 10 -textvariable include_list
	pack .oa.b$i.l -side left
	pack .oa.b$i.e -side right -expand 1 -fill x

	incr i

	for {set j 1} {$j < $i} {incr j} {
		pack .oa.b$j -side top -fill x
	}

#	button .oa.connect -text "Connect" -command launch
#	pack .oa.connect -side top -fill x 

	frame .oa.b
	button .oa.b.done -text "Done" -command {destroy .oa}
	bind .oa <Escape> {destroy .oa}
	button .oa.b.help -text "Help" -command help_advanced_opts

	global use_listen
	if {$use_listen} {
		button .oa.b.connect -text "Listen" -command launch
	} else {
		button .oa.b.connect -text "Connect" -command launch
	}

	pack .oa.b.help .oa.b.connect .oa.b.done -fill x -expand 1 -side left

	pack .oa.b -side top -fill x 

	center_win .oa
	wm resizable .oa 1 0
	focus .oa
}

proc in_path {cmd} {
	global env
	set p $env(PATH)
	foreach dir [split $p ":"] {
		set try "$dir/$cmd"
		if [file exists $try] {
			return "$try"
		}
	}
	return ""
}

proc ssh_agent_restart {} {
	global env 

	set got_ssh_agent 0
	set got_ssh_add 0
	set got_ssh_agent2 0
	set got_ssh_add2 0

	if {[in_path "ssh-agent"]  != ""} {set got_ssh_agent 1}
	if {[in_path "ssh-agent2"] != ""} {set got_ssh_agent2 1}
	if {[in_path "ssh-add"]    != ""} {set got_ssh_add 1}
	if {[in_path "ssh-add2"]   != ""} {set got_ssh_add2 1}

	set ssh_agent ""
	set ssh_add ""
	if {[info exists env(USER)] && $env(USER) == "runge"} {
		if {$got_ssh_agent2} {
			set ssh_agent "ssh-agent2"
		}
		if {$got_ssh_add2} {
			set ssh_add "ssh-add2"
		}
	}
	if {$ssh_agent == "" && $got_ssh_agent} {
		set ssh_agent "ssh-agent"
	}
	if {$ssh_add == "" && $got_ssh_add} {
		set ssh_add "ssh-add"
	}
	if {$ssh_agent == ""} {
		bell
		mesg "could not find ssh-agent in PATH"
		return
	}
	if {$ssh_add == ""} {
		bell
		mesg "could not find ssh-add in PATH"
		return
	}
	set tmp $env(SSVNC_HOME)/.vnc-sa[pid]
	set fh ""
	catch {set fh [open $tmp "w"]}
	if {$fh == ""} {
		bell
		mesg "could not open tmp file $tmp"
		return
	}

	puts $fh "#!/bin/sh"
	puts $fh "eval `$ssh_agent -s`"
	puts $fh "$ssh_add"
	puts $fh "SSVNC_GUI_CHILD=\"\"" 
	puts $fh "export SSVNC_GUI_CHILD" 

	global buck_zero
	set cmd $buck_zero
	
	if [info exists env(SSVNC_GUI_CMD)] {
		set cmd $env(SSVNC_GUI_CMD) 
	}
	#puts $fh "$cmd </dev/null 1>/dev/null 2>/dev/null &"
	puts $fh "nohup $cmd &"
	puts $fh "sleep 1"
	puts $fh "#rm -f $tmp"
	close $fh

	wm withdraw .
	catch {wm withdraw .o}
	catch {wm withdraw .oa}

	unix_terminal_cmd "+200+200" "Restarting with ssh-agent/ssh-add" "sh $tmp" 1
	after 10000
	destroy .
	exit
}

proc putty_pw_entry {mode} {
	if {$mode == "check"} {
		global use_sshssl use_ssh
		if {$use_sshssl || $use_ssh} {
			putty_pw_entry enable
		} else {
			putty_pw_entry disable
		}
		return
	}
	if {$mode == "disable"} {
		catch {.o.pw.l configure -state disabled}
		catch {.o.pw.e configure -state disabled}
	} else {
		catch {.o.pw.l configure -state normal}
		catch {.o.pw.e configure -state normal}
	}
}

proc ssl_ssh_adjust {which} {
	global use_ssl use_ssh use_sshssl sshssl_sw	
	global remote_ssh_cmd_list

	if {$which == "ssl"} {
		set use_ssl 1
		set use_ssh 0
		set use_sshssl 0
		set sshssl_sw "ssl"
		catch {.f4.getcert configure -state normal}
		catch {.f4.always  configure -state normal}
	} elseif {$which == "ssh"} {
		set use_ssl 0
		set use_ssh 1
		set use_sshssl 0
		set sshssl_sw "ssh"
		catch {.f4.getcert configure -state disabled}
		catch {.f4.always  configure -state disabled}
	} elseif {$which == "sshssl"} {
		set use_ssl 0
		set use_ssh 0
		set use_sshssl 1
		set sshssl_sw "sshssl"
		catch {.f4.getcert configure -state disabled}
		catch {.f4.always  configure -state disabled}
	}

	if [info exists remote_ssh_cmd_list] {
		if {$use_ssh || $use_sshssl} {
			foreach w $remote_ssh_cmd_list {
				$w configure -state normal
			}
		}
		if {$use_ssl} {
			foreach w $remote_ssh_cmd_list {
				$w configure -state disabled
			}
		}
	}

	if {! $use_ssl && ! $use_ssh && ! $use_sshssl} {
		set use_ssl 1
		set sshssl_sw "ssl"
	}
	
	putty_pw_entry check
}

proc listen_adjust {} {
	global use_listen revs_button
	if {$use_listen} {
		catch {.b.conn configure -text "Listen"}
		catch {.o.b.connect configure -text "Listen"}
	} else {
		catch {.b.conn configure -text "Connect"}
		catch {.o.b.connect configure -text "Connect"}
	}
}

proc set_options {} {
	global use_alpha use_grab use_ssh use_sshssl use_viewonly use_fullscreen use_bgr233
	global use_nojpeg use_raise_on_beep use_compresslevel use_quality
	global compresslevel_text quality_text
	global env is_windows darwin_cotvnc
	global use_listen

	toplev .o
	wm title .o "SSL/SSH VNC Options"

	set i 1

	radiobutton .o.b$i -anchor w -variable sshssl_sw -value ssl -text \
		"Use SSL" -command {ssl_ssh_adjust ssl}
	incr i

	radiobutton .o.b$i -anchor w -variable sshssl_sw -value ssh -text \
		"Use SSH" -command {ssl_ssh_adjust ssh}
	incr i

	radiobutton .o.b$i -anchor w -variable sshssl_sw -value sshssl -text \
		"Use SSH + SSL" -command {ssl_ssh_adjust sshssl}
	set iss $i
	incr i

	checkbutton .o.b$i -anchor w -variable use_listen -text \
		"Reverse VNC Connection (-listen)" -command {listen_adjust; if {$vncdisplay == ""} {set vncdisplay ":0"}}
	#if {$is_windows} {.o.b$i configure -state disabled}
	if {$darwin_cotvnc} {.o.b$i configure -state disabled}
	incr i

	checkbutton .o.b$i -anchor w -variable use_viewonly -text \
		"View Only"
	incr i

	checkbutton .o.b$i -anchor w -variable use_fullscreen -text \
		"Fullscreen"
	incr i

	checkbutton .o.b$i -anchor w -variable use_raise_on_beep -text \
		"Raise On Beep"
	if {$darwin_cotvnc} {.o.b$i configure -state disabled}
	incr i

	checkbutton .o.b$i -anchor w -variable use_bgr233 -text \
		"Use 8bit color (-bgr233)"
	if {$darwin_cotvnc} {.o.b$i configure -state disabled}
	incr i

	checkbutton .o.b$i -anchor w -variable use_alpha -text \
		"Cursor alphablending (32bpp required)"
	if {$darwin_cotvnc} {.o.b$i configure -state disabled}
	set ia $i
	incr i

	checkbutton .o.b$i -anchor w -variable use_grab -text \
		"Use XGrabServer"
	if {$darwin_cotvnc} {.o.b$i configure -state disabled}
	set ix $i
	incr i

	checkbutton .o.b$i -anchor w -variable use_nojpeg -text \
		"Do not use JPEG (-nojpeg)"
	if {$darwin_cotvnc} {.o.b$i configure -state disabled}
	incr i

	menubutton .o.b$i -anchor w -menu .o.b$i.m -textvariable compresslevel_text
	set compresslevel_text "Compress Level: $use_compresslevel"
	if {$darwin_cotvnc} {.o.b$i configure -state disabled}

	menu .o.b$i.m -tearoff 0
	for {set j -1} {$j < 10} {incr j} {
		set v $j
		set l $j
		if {$j == -1} {
			set v "default"
			set l "default"
		}
		.o.b$i.m add radiobutton -variable use_compresslevel \
			-value $v -label $l -command \
			{set compresslevel_text "Compress Level: $use_compresslevel"}
	}
	incr i

	menubutton .o.b$i -anchor w -menu .o.b$i.m -textvariable quality_text
	set quality_text "Quality: $use_quality"
	if {$darwin_cotvnc} {.o.b$i configure -state disabled}

	menu .o.b$i.m -tearoff 0
	for {set j -1} {$j < 10} {incr j} {
		set v $j
		set l $j
		if {$j == -1} {
			set v "default"
			set l "default"
		}
		.o.b$i.m add radiobutton -variable use_quality \
			-value $v -label $l -command \
			{set quality_text "Quality: $use_quality"}
	}
	incr i

	for {set j 1} {$j < $i} {incr j} {
		pack .o.b$j -side top -fill x
	}

	if {$is_windows} {
		.o.b$ia configure -state disabled
		.o.b$ix configure -state disabled
	}

	if {$is_windows} {
		frame .o.pw	
		label .o.pw.l -text "Putty PW:"
		entry .o.pw.e -width 10 -show * -textvariable putty_pw
		pack .o.pw.l -side left
		pack .o.pw.e -side left -expand 1 -fill x
		pack .o.pw -side top -fill x 
		putty_pw_entry check
	} else {
		button .o.sa -text "Use ssh-agent" -command ssh_agent_restart
		pack .o.sa -side top -fill x 
	}

	button .o.s_prof -text "Save Profile ..." -command {save_profile .o; raise .o}
	button .o.l_prof -text " Load Profile ..." -command {load_profile .o; raise .o}
	button .o.advanced -text "Advanced ..." -command set_advanced_options
#	button .o.connect -text "Connect" -command launch
	button .o.clear -text "Clear Options" -command set_defaults
#	pack .o.connect -side top -fill x 
	pack .o.clear -side top -fill x 
	pack .o.s_prof -side top -fill x 
	pack .o.l_prof -side top -fill x 
	pack .o.advanced -side top -fill x 

	frame .o.b
	button .o.b.done -text "Done" -command {destroy .o}
	bind .o <Escape> {destroy .o}
	button .o.b.help -text "Help" -command help_opts
	global use_listen
	if {$use_listen} {
		button .o.b.connect -text "Listen" -command launch
	} else {
		button .o.b.connect -text "Connect" -command launch
	}

	pack .o.b.help .o.b.connect .o.b.done -fill x -expand 1 -side left

	pack .o.b -side top -fill x 

	center_win .o
	wm resizable .o 1 0
	focus .o
}

proc check_writable {} {
	set test test[pid].txt
	catch {set f [open $test "w"]; puts $f "test"; close $f}

	###catch {file delete -force $test}	# testing.

	if ![file exists $test] {
		global env
		if [info exists env(SSVNC_HOME)] {
			set dir "$env(SSVNC_HOME)/ss_vnc/cache"	
			catch {file mkdir $dir}
			if ![file exists $dir] {
				return
			}
			foreach f [glob -type f * */* */*/*] {
				set dest "$dir/$f"
				set dirn [file dirname $dest]
				catch {file mkdir $dirn}
				catch {file copy -force -- $f $dest}
			}
			cd $dir
			###catch {set f [open $test "w"]; puts $f "test"; close $f}
		}
	} else {
		catch {file delete -force $test}
	}
}

global env
set is_windows 0
set help_font "-font fixed"
if { [regexp -nocase {Windows} $tcl_platform(os)]} {
	cd util
	set help_font ""
	set is_windows 1
}

if {[regexp -nocase {Windows.9} $tcl_platform(os)]} {
	set is_win9x 1
} else {
	set is_win9x 0
}

# set SSVNC_HOME to HOME in case we modify it for mobile use:
if [info exists env(HOME)] {
	if {! [info exists env(SSVNC_HOME)]} {
		set env(SSVNC_HOME) $env(HOME)
	}
}

# For mobile use, e.g. from a USB flash drive, we look for a "home" or "Home"
# directory relative to this script where the profiles and certs will be kept
# by default.
if [file exists $buck_zero] {
	#puts "$buck_zero"
	set up [file dirname $buck_zero]

	if {$up == "."} {
		# this is actually bad news on windows because we cd'd to util.
		set up ".."
	} else {
		set up [file dirname $up]
	}
	set dirs [list $up]

	if {! $is_windows && $up != ".."} {
		# get rid of bin
		set up [file dirname $up]
		lappend dirs $up
	}

	for {set i 0} {$i < $argc} {incr i} {
		set it0 [lindex $argv $i]
		if {$it0 == "."} {
			if {![file isdirectory "$up/home"] && ![file isdirectory "$up/Home"]} {
				catch {file mkdir "$up/Home"}
			}
			break
		}
	}

	set gotone 0

	foreach d $dirs {
		set try "$d/home"
		#puts "$try"
		if [file isdirectory $try] {
			set env(SSVNC_HOME) $try
			set gotone 1
			break
		}
		set try "$d/Home"
		#puts "$try"
		if [file isdirectory $try] {
			set env(SSVNC_HOME) $try
			set gotone 1
			break
		}
	}
	if {$gotone} {
		set b ""
		if {$is_windows} {
			set b "$env(SSVNC_HOME)/ss_vnc"
		} else {
			set b "$env(SSVNC_HOME)/.vnc"
		}
		catch {file mkdir $b}
		catch {file mkdir "$b/certs"}
		catch {file mkdir "$b/profiles"}
	}
	#puts "HOME: $env(SSVNC_HOME)"
}

for {set i 0} {$i < $argc} {incr i} {
	set item [lindex $argv $i]
	regsub {^--} $item "-" item
	if {$item == "-profiles"} {
		set dir [get_profiles_dir]
		puts stderr "VNC Profiles:"
		puts stderr " "
		set profs [list]
		foreach prof [glob -nocomplain -directory $dir "*.vnc"] {
			set s [file tail $prof]
			regsub {\.vnc$} $s "" s
			lappend profs [file tail $s]
		}
		foreach prof [lsort $profs] {
			puts "$prof"
		}
		exit
	} elseif {$item == "-nvb"} {
		global env
		set env(SSVNC_NO_VERIFY_ALL_BUTTON) 1
	}
}

if {$is_windows} {
	check_writable
}

set uname ""
if {! $is_windows} {
	catch {set uname [exec uname]}
}

set darwin_cotvnc 0
if {$uname == "Darwin"} {
	if {! [info exists env(DISPLAY)]} {
		set darwin_cotvnc 1
	}
	if [info exists env(SSVNC_HOME)] {
		set t "$env(SSVNC_HOME)/.vnc"
		if {! [file exists $t]} {
			catch {file mkdir $t}
		}
	}
	set help_font "-font {Monaco 10}"
}

set putty_pw ""

global scroll_text_focus
set scroll_text_focus 1

set multientry 1

wm withdraw .
wm title . "SSL/SSH VNC Viewer"
wm resizable . 1 0

set_defaults
set skip_pre 0

set vncdisplay ""
set vncproxy ""
set remote_ssh_cmd ""
set vncauth_passwd ""

global did_listening_message
set did_listening_message 0

global accepted_cert_dialog_in_progress
set accepted_cert_dialog_in_progress 0

label .l -text "SSL/SSH VNC Viewer" -relief ridge

set wl 21
set we 40
frame .f0
if {$multientry} {
	label .f0.l -width $wl -anchor w -text "VNC Host:Display" -relief ridge
} else {
	label .f0.l -anchor w -text "VNC Host:Display" -relief ridge
}
entry .f0.e -width $we -textvariable vncdisplay
pack .f0.l -side left 
pack .f0.e -side left -expand 1 -fill x
bind .f0.e <Return> launch

frame .f1
label .f1.l -width $wl -anchor w -text "VNC Password:" -relief ridge
entry .f1.e -width $we -textvariable vncauth_passwd -show *
pack .f1.l -side left 
pack .f1.e -side left -expand 1 -fill x
bind .f1.e <Return> launch

frame .f2
label .f2.l -width $wl -anchor w -text "Proxy/Gateway:" -relief ridge
entry .f2.e -width $we -textvariable vncproxy
pack .f2.l -side left 
pack .f2.e -side left -expand 1 -fill x
bind .f2.e <Return> launch

frame .f3
label .f3.l -width $wl -anchor w -text "Remote SSH Command:" -relief ridge
entry .f3.e -width $we -textvariable remote_ssh_cmd
pack .f3.l -side left 
pack .f3.e -side left -expand 1 -fill x
.f3.l configure -state disabled
.f3.e configure -state disabled
bind .f3.e <Return> launch

set remote_ssh_cmd_list {.f3.e .f3.l} 

frame .f4
radiobutton .f4.ssl -anchor w    -variable sshssl_sw -value ssl    -command {ssl_ssh_adjust ssl}    -text "Use SSL"
radiobutton .f4.ssh -anchor w    -variable sshssl_sw -value ssh    -command {ssl_ssh_adjust ssh}    -text "Use SSH"
radiobutton .f4.sshssl -anchor w -variable sshssl_sw -value sshssl -command {ssl_ssh_adjust sshssl} -text "SSH + SSL  "

pack .f4.ssl .f4.ssh .f4.sshssl -side left -fill x

global skip_verify_accepted_certs
set skip_verify_accepted_certs 0

global always_verify_ssl
set always_verify_ssl 1;
if {[info exists env(SSVNC_NO_VERIFY_ALL)]} {
	set always_verify_ssl 0;
}

button .f4.getcert -command {fetch_cert 1} -text "Fetch Cert"
checkbutton .f4.always -variable always_verify_ssl -text "Verify All Certs"
pack .f4.getcert -side right -fill x
if {[info exists env(SSVNC_NO_VERIFY_ALL_BUTTON)]} {
	set always_verify_ssl 0;
} else {
	pack .f4.always -side right -fill x
}

ssl_ssh_adjust ssl

frame .b
button .b.help  -text "Help" -command help
button .b.certs -text "Certs ..." -command getcerts
button .b.opts  -text "Options ..." -command set_options
button .b.load  -text "Load" -command {load_profile}
button .b.save  -text "Save" -command {save_profile}
button .b.conn  -text "Connect" -command launch
button .b.exit  -text "Exit" -command {destroy .; exit}


pack .b.certs .b.opts .b.save .b.load .b.conn .b.help .b.exit -side left -expand 1 -fill x

if {$multientry} {
	if {! $is_windows} {
		pack .l .f0 .f1 .f2 .f3 .f4 .b -side top -fill x
	} else {
		pack .l .f0     .f2 .f3 .f4 .b -side top -fill x
	}
} else {
	pack .l .f0 .b -side top -fill x
}
if {![info exists env(SSVNC_GUI_CHILD)] || $env(SSVNC_GUI_CHILD) == ""} {
	center_win .
}
focus .f0.e

wm deiconify .

global system_button_face
set system_button_face ""
foreach item [.b.help configure -bg] {
	set system_button_face $item
}

if {[info exists env(SSVNC_GUI_CMD)]} {
	set env(SSVNC_GUI_CHILD) 1
	bind . <Control-n> "exec $env(SSVNC_GUI_CMD) &"
}
bind . <Control-q> "destroy .; exit"
bind . <Shift-Escape> "destroy .; exit"
bind . <Control-s> "launch_shell_only"
bind . <Control-p> {port_knock_only "" "KNOCK"}
bind . <Control-P> {port_knock_only "" "FINISH"}
bind . <Control-l> {load_profile}
bind . <B3-ButtonRelease> {load_profile}

global entered_gui_top button_gui_top
set entered_gui_top 0
set button_gui_top 0
bind . <Enter> {set entered_gui_top 1}
bind .l <ButtonPress> {set button_gui_top 1}
bind .f0.l <ButtonPress> {set button_gui_top 1}

update

for {set i 0} {$i < $argc} {incr i} {
	set item [lindex $argv $i]
	regsub {^--} $item "-" item
	if {$item == "."} {
		;
	} elseif {$item == "-nv"} {
		set always_verify_ssl 0
	} elseif {$item == "-help"} {
		help
	} elseif {$item != ""} {
		if [file exists $item] {
			load_profile . $item
		} else {
			set ok 0
			set dir [get_profiles_dir]
			set try "$dir/$item"
			foreach try [list $dir/$item $dir/$item.vnc] {
				if [file exists $try] {
					load_profile . $try
					set ok 1
					break;
				}
			}
			if {! $ok && [regexp {:} $item]} {
				global vncdisplay
				set vncdisplay $item
				set ok 1
			}
			if {$ok} {
				update 
				after 750
				launch
			}
		}
	}
}
