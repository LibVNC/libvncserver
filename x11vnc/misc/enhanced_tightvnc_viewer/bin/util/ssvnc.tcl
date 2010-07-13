#!/bin/sh
# the next line restarts using wish \
exec wish "$0" "$@"

#
# Copyright (c) 2006-2010 by Karl J. Runge <runge@karlrunge.com>
#
# ssvnc.tcl: gui wrapper to the programs in this
# package. Also sets up service port forwarding.
#
set version 1.0.28

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

proc small_height {} {
	set H [winfo screenheight .]
	if {$H < 700} {
		return 1
	} else {
		return 0
	}
}

proc mac_raise {} {
	global uname
	if {$uname == "Darwin"} {
		catch {exec /bin/sh -c {osascript -e 'tell application "Wish Shell" to activate' >/dev/null 2>&1 &}}
		after 150
		update
		update idletasks
	}
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

proc line_count {{str ""} {pad 0}} {
	set n $pad
	foreach l [split $str "\n"] {
		incr n
	}
	return $n
}

proc scroll_text {fr {w 80} {h 35}} {
	global help_font is_windows scroll_text_focus

	if {$h == 35 && [small_height]} {
		set h 28
	}
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

	if {$h == 35 && [small_height]} {
		set h 28
	}
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

proc ts_help {} {
	toplev .h

	scroll_text_dismiss .h.f

	center_win .h
	wm title .h "Terminal Services VNC Viewer Help"

	set msg {
 Terminal Services:

    The Terminal Services VNC Viewer uses SSH to establish an encrypted
    and authenticated connection to the remote server.

    Through the SSH channel, it automatically starts x11vnc in terminal
    services mode on the remote server to find or create your desktop
    session.  x11vnc is used for both the session management and the
    VNC transport.

    You MUST be able to log in via SSH to the remote terminal server.
    Ask your administrator to set this up for you if it isn't already.
    x11vnc must also be installed on the remote server machine.
    See "Requirements" below.

    This mode is started by the commands 'tsvnc' or 'ssvnc -ts' or
    toggled by pressing Ctrl-t.  "SSVNC Mode" under Options -> Advanced
    will also return to the full SSVNC.

    Or in your ~/.ssvncrc (or ~/ssvnc_rc on Windows) put "mode=tsvnc"
    to have the tool always start up in that mode.  To constrain the UI,
    run with -tso or SSVNC_TS_ALWAYS set to prevent leaving the Terminal
    Services mode.


 Hosts and Displays:

    Enter the remote VNC Terminal Services hostname in the
    'VNC Terminal Server' entry.

    Examples:

           24.67.132.27
           far-away.east
           fred@someplace.no
    
    Then click on "Connect".

    Once the SSH is running (you may need to type a password or accept
    a new ssh key in the terminal window that pops up), the VNC Viewer
    will be automatically started directed to the local port of the SSH
    tunnel which, in turn, encrypts and redirects the connection to the
    remote VNC server.

    x11vnc is run remotely to find or create your terminal services desktop
    session.  It must be installed and accessible on the remote system.

    Enter "user@hostname.com" in 'VNC Terminal Server' if the remote
    username is different from the yours on this machine.  On Windows
    you *MUST* supply the remote username due to a deficiency in Plink.
    This entry is passed to SSH; it could also be an SSH alias you have
    created (in ~/.ssh/config).

    If the remote SSH server is run on a non-standard port, e.g. 2222, use
    something like one of these:

           far-away.east:2222
           fred@someplace.no:2222

    (unlike SSVNC mode, the number is the SSH port, not the VNC display)

    If you find yourself in the unfortunate circumstance that your ssh 
    username has a space in it, use %SPACE (or %TAB) like this:

           fred%SPACEflintstone@xyzzy.net


 Zeroconf/Bonjour:

    On Unix or Mac OS X, if the 'avahi-browse' or 'dns-sd' command is
    available on the system and in your PATH, a 'Find' button is placed by
    'VNC Host:Display'.  Clicking on Find will try to find VNC Servers
    on your Local Network that advertize via the Zeroconf protocol.
    A menu of found hosts is presented for you to select from.


 Profiles:

    Use "Save" to save a profile (i.e. a host:display and its specific
    settings) with a name.  The "TS-" prefix will be suggested to help
    you distinguish between Terminal Services and regular profiles.

    To load in a saved Options profile, click on the "Load" button,
    and choose which one you want.

    To list your profiles from the command line use:

         tsvnc -profiles    (or -list)

    To launch profile1 directly from the command-line, or to a server
    use things like: 

         tsvnc profile1
         tsvnc /path/to/profile1.vnc
         tsvnc hostname
         tsvnc user@hostname

    Note that the 'Verify All Certs' setting is NOT saved in profiles.


 Proxies/Gateways:

    Proxy/Gateway is usually a gateway machine to log into via SSH that is
    not the machine running the VNC terminal services.  However, Web and
    SOCKS proxies can also be used (see below).

    For example if a company had a central login server: "ssh.company.com"
    (accessible from the internet) and the internal server name was
    "ts-server", one could put in

           VNC Terminal Server:   ts-server
           Proxy/Gateway:         ssh.company.com

    It is OK if the hostname "ts-server" only resolves inside the firewall.

    The 2nd host, ts-server in this example, MUST also be running an SSH
    server and you must be able to log into it.  You may need to supply
    a 2nd password to it to login.

    Use username@host (e.g. joe@ts-server or jsmith@ssh.company.com)
    if the user name differs between machines.

    NOTE: On Windows you MUST always supply the username@ because putty's
    plink requires it.


    NON-STANDARD SSH PORT: To use a non-standard ssh port (i.e. a port other
    than 22) you need to use the Proxy/Gateways as well.  E.g. something
    like this for port 2222:
    
           VNC Terminal Server:  ts-server
           Proxy/Gateway:        jsmith@ssh.company.com:2222

    On Unix/MacOSX the username@ is not needed if it is the same as on this
    machine.


    A Web or SOCKS proxy can also be used.  Use this if you are inside a
    firewall that prohibits direct connections to remote SSH servers.
    In Terminal Services SSH mode, the "http://" prefix is required for
    web proxies.

           VNC Terminal Server:  fred@someplace.no
           Proxy/Gateway:        http://myproxy.west:8080

    or for SOCKS:

           VNC Terminal Server:  fred@someplace.no
           Proxy/Gateway:        socks://mysocks.west:1080

    use socks5://... to force the SOCKS5 version.  For a non-standard
    port the above would be, e.g., fred@someplace.no:2222

    As with a username that contains a space, use %SPACE (or %TAB) to
    indicate it in the SSH proxies, e.g. john%SPACEsmith@ssh.company.com

    One can also chain proxies and other things.  See the section
    "SSH Proxies/Gateways" in the Main SSVNC Help for full details.


 Options:

    Click on Options to get to dialog boxes to:

           - Desktop Type        (kde, gnome, failsafe, twm...) 
           - Desktop Size        (Geometry WxH and pixel depth) 
           - X Server Type       (Xvfb, Xdummy, Xvnc) 
           - Enable Printing     (CUPS and/or SMB/Windows)
           - Enable Sound        (TBD, ESD partially working)
           - File Transfer       (Ultra or TightVNC filexfer)
           - View Only           (View only client)
           - Change VNC Viewer   (Realvnc, ultra, etc...)
           - X11 viewer MacOSX   (use bundled X11 vncviewer)
           - Delete Profile...   (Delete a saved profile)

           - Advanced Options:

           - VNC Shared          (optional traditional VNC sharing)
           - Multiple Sessions   (more than 1 session per server)
           - X Login Greeter     (Connect to Login/Greeter Display)
           - Other VNC Server    (redirect to 3rd party VNC Server)
           - Use unixpw          (optional x11vnc login mode)
           - Client 8bit Color   (VNC Viewer requests low color mode)
           - Client-Side Caching (experimental x11vnc speedup)
           - X11VNC Options      (set any extra x11vnc options)
           - Extra Sleep         (delay a bit before starting viewer)
           - Putty Args          (Windows: string for plink/putty cmd)
           - Putty Agent         (Windows: launch pageant)
           - Putty Key-Gen       (Windows: launch puttygen)
           - SSH Local Protections  (a bit of safety on local side)
           - SSH KnownHosts file (to avoid SSH 'localhost' collisions)
           - SSVNC Mode          (Return to full SSVNC mode)

           - Unix ssvncviewer    (set options for supplied Unix viewer)


 Requirements:

    When running this application on Unix/MacOSX the ssh(1) program must
    be installed locally.  On Windows a plink/putty binary is included.

    On the remote VNC Terminal Services host, x11vnc must be installed
    (0.9.3 or higher), and at least one virtual X server: Xvfb, Xdummy,
    or Xvnc must be available.  Xvfb is the most often used one.  All of
    these programs must be available in $PATH on the remote server when
    logged in via SSH.

    The VNC terminal services administrator can make "x11vnc" be a wrapper
    script that sets everything up correctly and then runs the real x11vnc. 


 Real X servers:

    As a *BONUS*, if on the remote host, say a workstation, you have a
    regular X session running on the physical hardware that you are
    ALREADY logged into you can access to that display as well (x11vnc
    will find it).

    So this tool can be used as a simple way to launch x11vnc to find
    your real X display on your workstation and connect to it.

    The Printing and Sound redirection won't work for this mode however.
    You will need to use the full SSVNC application to attempt that.

    If you (mistakenly) have not logged into an X session on the real
    X server on the workstation, a VIRTUAL (Xvfb, etc.) server will be
    created for you (that may or may not be what you want).

    The X Login Advanced setting can be used to connect to a X Display
    Manger Greeter login panel (no one is logged in yet).  This requires
    sudo(1) privileges on the remote machine.

 More Info:

    See these links for more information:

        http://www.karlrunge.com/x11vnc/#tunnelling
}

	global version
	set msg "                             SSVNC version: $version\n$msg"

	.h.f.t insert end $msg
	jiggle_text .h.f.t
}

proc help {} {
	global ts_only
	if {$ts_only} {
		ts_help
		return
	}
	toplev .h

	set h 37
	if [small_height] {
		set h 26
	}
	scroll_text_dismiss .h.f 82 $h

	center_win .h
	wm title .h "SSL/SSH VNC Viewer Help"

	global help_main help_prox help_misc help_tips
	
	set help_main {
 Hosts and Displays:

    Enter the VNC host and display in the  'VNC Host:Display'  entry box.
    
    It is of the form "host:number", where "host" is the hostname of the
    machine running the VNC Server and "number" is the VNC display number;
    it is often "0".  Some Examples:

           snoopy:0

           far-away.east:0

           sunray-srv1.west:17

           24.67.132.27:0
    
    Then click on "Connect".  When you do the STUNNEL program will be started
    locally to provide you with an outgoing SSL tunnel.

    Once the STUNNEL is running, the TightVNC Viewer (Or perhaps Chicken of
    the VNC on Mac OS X, or one you set under Options) will be automatically
    started and directed to the local port of the SSL tunnel which, in turn,
    encrypts and redirects the connection to the remote VNC server.

    The remote VNC server **MUST** support an initial SSL/TLS handshake before
    using the VNC protocol (i.e. VNC is tunnelled through the SSL channel
    after it is established).  "x11vnc -ssl ..."  does this, and any VNC server
    can be made to do this by using, e.g., STUNNEL or socat on the remote side.
    SSVNC also supports VeNCrypt and ANONTLS SSL/TLS VNC servers (see below.)

    * Automatic SSH Tunnels are described below.

    * The 'No Encryption' / 'None' option provides a direct connection without
      encryption (disable the button with the -enc option, or Options menu.)
      More info in Tip 5.

 Port numbers:

    If you are using a port less than the default VNC port 5900 (usually
    the VNC display = port - 5900), use the full port number itself, e.g.:
    
         24.67.132.27:443
    
    Note, however, if the number n after the colon is < 200, then a
    port number 5900 + n is assumed; i.e. n is the VNC display number.
    If you must use a TCP port less than 200, specify a negative value,
    e.g.:  24.67.132.27:-80
 
    For Reverse VNC connections (listening viewer, See Tip 2 and
    Options -> Help), the port mapping is similar, except "listening
    display :0" corresponds to port 5500, :1 to 5501, etc.
    Specify a specific interface, e.g. 192.168.1.1:0 to have stunnel
    listen on that interface only.  Listening on IPv6 can also be done, use
    e.g. :::0 or ::1:0  This listening on IPv6 (:::0) works for UN-encrypted
    reverse connections as well (mode 'None').


 Zeroconf/Bonjour:

    On Unix or Mac OS X, if the 'avahi-browse' or 'dns-sd' command is
    available on the system and in your PATH, a 'Find' button is placed by
    'VNC Host:Display'.  Clicking on Find will try to find VNC Servers on
    your Local Network that advertize via the Zeroconf protocol.  A menu of
    found hosts is presented for you to select from.


 VNC Password:

    On Unix or MacOSX IF there is a VNC password for the server you can
    enter it in the "VNC Password:" entry box.

    This is *REQUIRED* on MacOSX when Chicken of the VNC is used, because
    that viewer does not put up a user password prompt when it learns
    that a password is needed.

    On Unix (including MacOSX using the X11 viewer) if you choose not to
    enter the password you will simply be prompted for it in the terminal
    window running TightVNC viewer if one is required.

    On Windows TightVNC viewer will prompt you if a password is required.

    NOTE: when you Save a VNC profile, the password is NOT saved (you need
    to enter it each time).  Nor is the 'Verify All Certs' setting.


 Profiles:

    Use "Save" to save a profile (i.e. a host:display and its specific
    settings) with a name.

    To load in a saved Options profile, click on the "Load" button.

    To list your profiles from the command line use: 

        ssvnc -profiles    (or -list)

    You can launch ssvnc and have it immediately connect to the server
    by invoking it something like this:

        ssvnc profile1              (launches profile named "profile1")
        ssvnc /path/to/profile.vnc  (loads the profile file, no launching)
        ssvnc hostname:0            (connect to hostname VNC disp 0 via SSL)
        ssvnc vnc+ssl://hostname:0  (same)
        ssvnc vnc+ssh://hostname:0  (connect to hostname VNC disp 0 via SSH)

    see the Tips 5 and 7 for more about the URL-like syntax.

    If you don't want "ssvnc profile1" to immediately launch the connection
    to the VNC server set the SSVNC_PROFILE_LOADONLY env. var. to 1.
    (or specify the full path to the profile.vnc as shown above.)


 SSL Certificate Verification:

    *** IMPORTANT ***: If you do not take the steps to VERIFY the VNC Server's
    SSL Certificate, you are in principle vulnerable to a Man-In-The-Middle
    attack.  Without SSL Certificate verification, only passive network
    sniffing attacks will be guaranteed to be prevented.  There are hacker
    tools like dsniff/webmitm and cain that implement SSL Man-In-The-Middle
    attacks.  They rely on the client user not bothering to check the cert.

    Some people may be confused by the above because they are familiar with
    their Web Browser using SSL (i.e. https://... websites) and those sites
    are authenticated securely without the user's need to verify anything
    manually.  The reason why this happens automatically is because 1) their
    web browser comes with a bundle of Certificate Authority certificates
    and 2) the https sites have paid money to the Certificate Authorities to
    have their website certificate signed by them.  When using SSL in VNC we
    normally do not do something this sophisticated, and so we have to verify
    the certificates manually.  However, it is possible to use Certificate
    Authorities with SSVNC; that method is described below.

    You can use the "Fetch Cert" button to retrieve the Cert and then
    after you check it is OK (say, via comparing the MD5 or other info)
    you can "Save" it and use it to verify future connections to servers.
    (However, see the note at the end of this section about CA certificates.)

    When "Verify All Certs" is checked, this check is always enforced,
    and so the first time you connect to a new server you may need to
    follow a few dialogs to inspect and save the server certificate.
    See the "Certs... -> Help" for information on how to manage certificates.

    "Verify All Certs" is on by default.

    Note, however, "Fetch Cert" and "Verify All Certs" are currently disabled
    in the very rare "SSH + SSL" usage mode to avoid SSHing in twice.
    You can manually set a ServerCert or CertsDir in this case if you like.


    Advanced Method: Certificate Authority (CA):

    If you, or your site administrator, goes though the steps of setting up
    a Certificate Authority (CA) to sign the VNC server and/or VNC client
    Certs, that can be used instead and avoids the need to manually verify
    every cert while still authenticating every connection.  More info:
    http://www.karlrunge.com/x11vnc/faq.html#faq-ssl-ca

    See the cmdline option -cacert file below in 'SSL Certificates'
    for setting a default ServerCert/CA Cert.

    You may also Import the CA Cert and save it to the 'Accepted Certs'
    directory so the "Verify All Certs" automatic checking will find it.

    Note that if a Server is using a CA signed certificate instead of
    its own Self-Signed one, then the default "Verify All Certs/Fetch Cert"
    saving mechanism will NOT succeed.  You must obtain the CA certificate
    and explicitly set it as the ServerCert or Import it to Accepted Certs.


 SSL/TLS Variants; VeNCrypt and ANONTLS:

    SSVNC can also connect to VNC SSL/TLS variants; namely the VeNCrypt and
    "TLS" VNC Security types.  Vino uses the latter (we call it "ANONTLS");
    and a growing number use VeNCrypt (QEMU, ggi, virt-manager, VeNCrypt, Xen.)

    Via the VeNCrypt bridge that SSVNC provides, the VeNCrypt/ANONTLS
    support ALSO works with ANY 3rd party VNC Viewers you specify via
    'Change VNC Viewer' (e.g. RealVNC, TightVNC, UltraVNC, etc.) that do
    not directly support VeNCrypt or ANONTLS.  This works on all platforms:
    Unix, MacOSX, and Windows.


    Notes on VeNCrypt/ANONTLS Auto-detection:

    IMPORTANT: VeNCrypt Server Auto-detection *ONLY* occurs in SSL mode
    and when an initial fetch-cert action takes place.

    While the initial certificate fetch is taking place SSVNC applies
    heuristics to try to automatically detect the VeNCrypt or ANONTLS
    protocol use by the VNC server.  This way it learns that the server
    is using it and then knows to switch to VeNCrypt encrypted SSL/TLS at
    the right point.  Then SSVNC makes a second (the real) connection to 
    VNC server and connects the VNC viewer to it.

    In the default "Verify All Certs" mode, a fetch cert action always
    takes place, and so VeNCrypt/ANONTLS will be autodected.

    However, if you have specified an explicit ServerCert or disabled
    "Verify All Certs" then even though the initial fetch cert action is no
    longer needed, it is performed anyway because it allows VeNCrypt/ANONTLS
    auto-detection.

    To disabled this initial fetch (e.g. you know the VNC server is normal
    SSL and not VeNCrypt/ANONTLS and want to connect more quickly) then
    select "Do not Probe for VeNCrypt" in the Advanced Options menu.

    On the other hand, if you know the VNC server ONLY supports VeNCrypt or
    ANONTLS, to improve the accuracy and speed with which the connection
    takes place, you can specify the one or both of the 'Server uses
    VeNCrypt SSL encryption' and 'Server uses Anonymous Diffie-Hellman'
    in the 'Advanced' options panel.  That way guessing via an initial
    probe is not needed or performed.  See each options's Advanced Options
    Help for more info.

    Note that if you are using VeNCrypt or ANONTLS for REVERSE connections
    (Listen) then you *MUST* set the 'Server uses VeNCrypt SSL encryption'
    (and the ANON-DH if it applies) option in Advanced.  Note also that
    REVERSE VeNCrypt and ANONTLS connections currently do not work on
    Windows.

    Also, if you are using the "Use SSH+SSL" double tunnel, you MUST set
    'Server uses VeNCrypt SSL encryption' (and the ANON-DH if it applies)
    because the initial fetch cert is disabled in SSH+SSL mode.


 Deciphering SSL Negotiation Success or Failure:

    Since SSVNC is a "glue program", in this case gluing VNCViewer and stunnel
    together (with possibly a proxy helper) reporting is clumsy at best.
    (In SSH encryption mode, it glues to ssh instead of stunnel.)  In most
    cases the programs being "glued" are run in a terminal window where you
    can see the program's output.  On Windows you will need to double click
    on the stunnel tray icon to view its log.

    Although the output is quite cryptic, you are encouraged to learn to
    recognize some of the errors reported in it.

    Here is stunnel output for a case of successfully verifying the VNC
    Server's Certificate:

      2008.11.20 08:09:39 LOG5[1472]: VERIFY OK: depth=0, /C=AU/L=...
      2008.11.20 08:09:39 LOG6[1472]: SSL connected: new session negotiated
      2008.11.20 08:09:39 LOG6[1472]: Negotiated ciphers: AES256-SHA SSLv3 ...

    Here is a case where the Server's Cert did not match the ServerCert
    we set:

      2008.11.20 08:12:31 LOG4[1662]: VERIFY ERROR: depth=0, error=self ...
      2008.11.20 08:12:31 LOG3[1662]: SSL_connect: 14090086: error:14090086:SSL
           routines:SSL3_GET_SERVER_CERTIFICATE:certificate verify failed

    Here is a case where the Server's Cert has expired:

      2009.12.27 12:20:25 LOG4[25500]: VERIFY ERROR: depth=0, error=certificate
           has expired: /C=AU/L=...
      2009.12.27 12:20:25 LOG3[25500]: SSL_connect: 14090086: error:14090086:SSL
           routines:SSL3_GET_SERVER_CERTIFICATE:certificate verify failed


    If you disable "Verify All Certs" and do not supply a ServerCert,
    then there will be no 'VERIFY ...' in the output because the SSVNC
    stunnel accepts the server's cert without question (this is insecure.)

    Also in the output will be messages about whether the SSL VNC server
    rejected your connection because it requires you to authenticate
    yourself with a certificate (MyCert).  Here is the case when you
    supplied no MyCert:

      2008.11.20 08:16:29 LOG3[1746]: SSL_connect: 14094410: error:14094410:
          SSL routines:SSL3_READ_BYTES:sslv3 alert handshake failure

    or you used a certificate the server did not recognize:

      2008.11.20 08:18:46 LOG3[1782]: SSL_connect: 14094412: error:14094412:
          SSL routines:SSL3_READ_BYTES:sslv3 alert bad certificate

    or your certificate has been revoked:

     2008.11.20 08:20:08 LOG3[1913]: SSL_connect: 14094414: error:14094414:
         SSL routines:SSL3_READ_BYTES:sslv3 alert certificate revoked


 SSH:

    Click on "Use SSH" if you want to use an *SSH* tunnel instead of SSL
    (then the VNC Server does not need to speak SSL or use STUNNEL or socat).

    You will need to be able to login to your account on the remote host
    via SSH (e.g. via password, ssh keys, or ssh-agent).

    Specify the SSH hostname and VNC display in the VNC Host:Display entry.
    Use something like:

           username@far-away.east:0

    if your remote username is different from the one on the local viewer
    machine.

    On Windows you *MUST* supply the "username@" part because Putty/Plink
    needs it to work correctly.

    "SSH + SSL" is similar but its use is more rare because it requires 2
    encrypted tunnels to reach the VNC server. See the Help under Options
    for more info.

    To connect to a non-standard SSH port, see SSH Proxies/Gateways section.

    See Tip 8) for how to make this application be SSH-only with the -ssh
    command line option or "sshvnc".

    If you find yourself in the unfortunate circumstance that your ssh 
    username has a space in it, use %SPACE (or %TAB) like this:

           fred%SPACEflintstone@xyzzy.net:0

 Remote SSH Command:

    In SSH or SSH + SSL mode you can also specify a remote command to run
    on the remote ssh host in the "Remote SSH Command" entry.  The default
    is just to sleep a bit (e.g. sleep 15) to make sure the tunnel ports
    are established.  Alternatively you could have the remote command start
    the VNC server, e.g.

         x11vnc -display :0 -rfbport 5900 -localhost -nopw

    When starting the VNC server this way, note that sometimes you will need
    to correlate the VNC Display number with the "-rfbport" (or similar)
    option of the server.  E.g. for VNC display :2

         VNC Host:Display       username@somehost.com:2
         Remote SSH Command:    x11vnc -find -rfbport 5902 -nopw

    See the Tip 18) for using x11vnc PORT=NNNN feature (or vncserver(1)
    output) to not need to specify the VNC display number or the x11vnc
    -rfbport option.

    Windows SSH SERVER: if you are ssh'ing INTO Windows (e.g. CYGWIN SSHD
    server) there may be no "sleep" command so put in something like
    "ping localhost" or "ping -n 10 -w 1000 localhost" to set a short
    delay to let the tunnel ports get established.


 SSL Certificates:

    If you want to use a SSL Certificate (PEM) file to authenticate YOURSELF to
    the VNC server ("MyCert") and/or to verify the identity of the VNC Server
    ("ServerCert" or "CertsDir") select the certificate file by clicking the
    "Certs ..." button before connecting.

    Certificate verification is needed to prevent Man-In-The-Middle attacks;
    if it is not done then only passive network sniffing attacks are prevented.
    There are hacker tools like dsniff/webmitm and cain that implement SSL
    Man-In-The-Middle attacks.  They rely on the client user not bothering to
    check the cert.


    See the x11vnc documentation:

           http://www.karlrunge.com/x11vnc/ssl.html

    for how to create and use PEM SSL certificate files.  An easy way is:

           x11vnc -ssl SAVE ...

    where it will print out its automatically generated certificate to the
    screen and that can be copied safely to the viewer side.

    You can also use the "Create Certificate" feature of this program under
    "Certs ...".  Just click on it and follow the instructions in the dialog.
    Then copy the cert file to the VNC Server and specify the other one in
    the "Certs ..." dialog.

    Alternatively you can use the "Import Certificate" action to paste in a
    certificate or read one in from a file.  Or you can use the "Fetch Cert"
    button on the main panel.  If "Verify All Certs" is checked, you will
    be forced to check Certs of any new servers the first time you connect.

    Note that "Verify All Certs" is on by default so that users who do not
    understand the SSL Man-In-The-Middle problem will not be left completely
    vulnerable to it (everyone still must make the effort to verify new
    certificates by an external method to be completely safe).

    To have "Verify All Certs" toggled off at startup, use "ssvnc -nv" or
    set SSVNC_NO_VERIFY_ALL=1 before starting.  If you do not even want to
    see the button, use "ssvnc -nvb" or SSVNC_NO_VERIFY_ALL_BUTTON=1.

    Use the "-mycert file" option (same as "-cert file") to set a default
    MyCert.  This is the same as "mycert=file" (also "cert=file") in the
    ~/.ssvncrc file.  See Certs -> Help for more info.

    Use the "-cacert file" option (same as "-ca file") to set a default
    ServerCert (or CA).  This is the same as "cacert=file" (also "ca=file")
    in the ~/.ssvncrc file.  See Certs -> Help for more info.

    Use the "-crl file" option to set a default CRL File.  This is the same
    as "crl=file" in the ~/.ssvncrc file.  See Certs -> Help for more info.

    Prefix any of these files with "FORCE:" to make them immutable.



 More Options:

    To set other Options, e.g. for View-Only usage or to limit the number
    of colors used, click on the "Options ..." button and read the Help there.

 More Info:

    Press the 'Proxies', 'Misc', and 'Tips' buttons below.

    See also these links for more information:

        http://www.karlrunge.com/x11vnc/faq.html#faq-ssl-tunnel-ext
        http://stunnel.mirt.net
        http://www.tightvnc.com
}

	set help_misc {
 Windows STUNNEL problems:

    Note that on Windows when the Viewer connection is finished by default
    SSVNC will try to kill the STUNNEL process for you.

    If Options -> Kill Stunnel Automatically is not set you will be
    prompted if you want SSVNC to try to kill the STUNNEL process for you.
    Usually you will say Yes, however if there are problems connecting
    you may want to look at the STUNNEL Log first.

    Before it is killed, double clicking the STUNNEL tray icon (dark green)
    will show you its Log file (useful for debugging connection problems).

    Even though SSVNC will kill the STUNNEL process for you, you will
    still need to move the mouse over the icon to make the little picture
    go away!!!  This is unfortunate but there does not seem to be a way
    to avoid it.

    In some cases you may need to terminate STUNNEL manually from the System
    Tray (right click on dark green icon) and selecting "Exit".

    Use -nokillstunnel or killstunnel=0 in ~/.ssvncrc to have SSVNC
    start up with stunnel killing disabled.

 Untrusted Local Users:

    *IMPORTANT WARNING*:  If you run SSVNC on a workstation or computer
    that other users can log into and you DO NOT TRUST these users
    (it is a shame but sometimes one has to work in an environment like
    this), then please note the following warning.

    By 'do not trust' we mean they might try to gain access to remote
    machines you connect to via SSVNC.  Note that an untrusted local
    user can often obtain root access in a short amount of time; if a
    user has achieved that, then all bets are off for ANYTHING that you
    do on the workstation.  It is best to get rid of Untrusted Local
    Users as soon as possible.

    Both the SSL and SSH tunnels set up by SSVNC listen on certain ports
    on the 'localhost' address and redirect TCP connections to the remote
    machine; usually the VNC server running there (but it could also be
    another service, e.g. CUPS printing).  These are the stunnel(8) SSL
    redirection and the ssh(1) '-L' port redirection.  Because 'localhost'
    is used only users or programs on the same workstation that is
    running SSVNC can connect to these ports, however this includes any
    local users (not just the user running SSVNC.)

    If the untrusted local user tries to connect to these ports, he may
    succeed by varying degrees to gain access to the remote machine.
    We now list some safeguards one can put in place to try to make this
    more difficult to achieve.

    It probably pays to have the VNC server require a password, even
    though there has already been SSL or SSH authentication (via
    certificates or passwords).  In general if the VNC Server requires
    SSL authentication of the viewer that helps, unless the untrusted
    local user has gained access to your SSVNC certificate keys.

    If the VNC server is configured to only allow one viewer connection
    at a time, then the window of opportunity that the untrusted local
    user can use is greatly reduced: he might only have a second or two
    between the tunnel being set up and the SSVNC vncviewer connecting
    to it (i.e. if the VNC server only allows a single connection, the
    untrusted local user cannot connect once your session is established).
    Similarly, when you disconnect the tunnel is torn down quickly and
    there is little or no window of opportunity to connect (e.g. x11vnc
    in its default mode exits after the first client disconnects).

    Also for SSL tunnelling with stunnel(8) on Unix using one of the SSVNC
    prebuilt 'bundles', a patched stunnel is provided that denies all
    connections after the first one, and exits when the first one closes.
    This is not true if the system installed stunnel(8) is used and is
    not true when using SSVNC on Windows.

    The following are experimental features that are added to SSVNC to
    improve the situation for the SSL/stunnel and SSH cases.  Set them
    via Options -> Advanced -> "STUNNEL Local Port Protections" or
    "SSH Local Port Protections".

    STUNNEL:

    1) For SSL tunnelling with stunnel(8) on Unix there is a setting
       'Use stunnel EXEC mode' that will try to exec(2) stunnel
       instead of using a listening socket.  This will require using
       the specially modified vncviewer unix viewer provided by SSVNC.
       The mode works well and is currently set as the default.
       Disable it if it causes problems or conflicts.

    2) For SSL tunnelling with stunnel(8) on Unix there is a setting
       'Use stunnel IDENT check' (experimental) to limit socket
       connections to be from you (this assumes the untrusted local
       user has not become root on your workstation and has modified
       your local IDENT check service; if he has you have much bigger
       problems to worry about...)

       Neither of the above methods are available on Windows.

    SSH:

    1) There is also a simple LD_PRELOAD trick for SSH to limit the
       number of accepted port redirection connections.  This makes the
       window of time the untrusted local user can connect to the tunnel
       much smaller.  Enable it via Options -> Advanced -> "SSH Local
       Port Protections".  You will need to have the lim_accept.so file
       in your SSVNC package.  The mode works well and is currently set
       as the default.  Disable it if it causes problems or conflicts.

       The above method is not available on Windows.

    The main message is to 'Watch your Back' when you connect via the
    SSVNC tunnels and there are users you don't trust on your workstation.
    The same applies to ANY use of SSH '-L' port redirections or outgoing
    stunnel SSL redirection services.
}

	set help_prox {
 Here are a number of long sections on all sorts of proxies, Web, SOCKS,
 SSH tunnels/gateways, UltraVNC, Single Click, etc., etc.


 Proxies/Gateways:

    If an intermediate proxy is needed to make the SSL connection
    (e.g. a web gateway out of a firewall) enter it in the "Proxy/Gateway"
    entry box:

           VNC Host-Display:   host:number
           Proxy/Gateway:      proxy-host:port
    e.g.:
           VNC Host-Display:   far-away.east:0
           Proxy/Gateway:      myproxy.west:8080


    If the "double proxy" case is required (e.g. coming out of a web
    proxied firewall environment and then INTO a 2nd proxy to ultimately
    reach the VNC server), separate them via a comma, e.g.:

           VNC Host-Display:   far-away:0
           Proxy/Gateway:      myproxy.west:8080,myhome.net:443

    So it goes: viewer -> myproxy.west -> myhome.net -> far-away (VNC)

    The proxies are assumed to be Web proxies.  To use SOCKS proxies:

           VNC Host-Display:   far-away.east:0
           Proxy/Gateway:      socks://mysocks.west:1080

    Use socks5:// to force the SOCKS5 proxy protocol (e.g. for ssh -D).

    You can prefix web proxies with http:// in SSL mode but it doesn't matter
    since that is the default for a proxy.  (NOTE that in SSH or SSH+SSL
    mode you MUST supply the http:// prefix for web proxies because in those
    modes an SSH tunnel is the default proxy type: see the next section.)

    Note that Web proxies are often configured to ONLY allow outgoing
    connections to ports 443 (HTTPS) and 563 (SNEWS), so you might
    have run the VNC server (or router port redirector) on those ports.
    SOCKS proxies usually have no restrictions on port number.

    You can chain up to 3 proxies (any combination of web (http://) and
    socks://) by separating them with commas (i.e. first,second,third).

    Proxies also work for un-encrypted connections ("None" or vnc://, Tip 5)

    See the ss_vncviewer description and x11vnc FAQ for info on proxies:

        http://www.karlrunge.com/x11vnc/faq.html#ss_vncviewer
        http://www.karlrunge.com/x11vnc/faq.html#faq-ssl-java-viewer-proxy


 SSH Proxies/Gateways:

    Proxy/Gateway also applies to SSH mode, it is a usually a gateway SSH
    machine to log into via ssh that is not the workstation running the
    VNC server.  However, Web and SOCKS proxies can also be used (see below).

    For example if a company had a central login server: "ssh.company.com"
    (accessible from the internet) and the internal workstation with VNC was
    named "joes-pc", then to create an SSH tunnel one could put this in:

           VNC Host:Display:   joes-pc:0
           Proxy/Gateway:      ssh.company.com

    It is OK if the hostname "joes-pc" only resolves inside the firewall.

    The 2nd leg, from ssh.company.com -> joes-pc is done by a ssh -L
    redir and is not encrypted (but the viewer -> ssh.company.com 1st leg is
    an encrypted tunnel). 

    To SSH encrypt BOTH legs, try the "double SSH gateway" method using
    the "comma" notation:

           VNC Host:Display:   localhost:0
           Proxy/Gateway:      ssh.company.com,joes-pc

    this requires an SSH server also running on joes-pc.  So an initial SSH
    login is done to ssh.company.com, then a 2nd SSH is performed (through
    port a redirection of the first) to login straight to joes-pc where
    the VNC server is running.

    Use username@host (e.g. joe@joes-pc  jsmith@ssh.company.com) if the
    user names differ between the various machines.  

    NOTE: On Windows you MUST always supply the username@ because putty's
    plink requires it.


    NON-STANDARD SSH PORT: To use a non-standard ssh port (i.e. a port other
    than 22) you need to use the Proxy/Gateways as well.  E.g. something
    like this for port 2222:

           VNC Host:Display:   localhost:0
           Proxy/Gateway:      joe@far-away.east:2222

    On Unix/MacOSX the username@ is not needed if it is the same as on
    the client.  This will also work going to a different internal machine,
    e.g. "joes-pc:0" instead of "localhost:0", as in the first example.


    A Web or SOCKS proxy can also be used with SSH.  Use this if you are
    inside a firewall that prohibits direct connections to remote SSH servers.

           VNC Host:Display:   joe@far-away.east:0
           Proxy/Gateway:      http://myproxy.west:8080

    or for SOCKS:

           VNC Host:Display:   joe@far-away.east:0
           Proxy/Gateway:      socks://mysocks.west:1080

    Use socks5://... to force the SOCKS5 version.  Note that the http://
    prefix is REQUIRED for web proxies in SSH or SSH+SSL modes (but it is
    the default proxy type in SSL mode.)

    You can chain up to 3 proxies (any combination of http://, socks://
    and ssh) by separating them with commas (i.e. first,second,third).

    Note: the Web and/or SOCKS proxies must come before any SSH gateways.

    For a non-standard SSH port and a Web or SOCKS proxy try:

           VNC Host:Display:   localhost:0
           Proxy/Gateway:      http://myproxy.west:8080,joe@far-away.east:2222

    Even the "double SSH gateway" method (2 SSH encrypted legs) described
    above works with an initial Web or SOCKS proxy, e.g.:

           VNC Host:Display:   localhost:0
           Proxy/Gateway:      socks://mysocks.west:1080,ssh.company.com,joes-pc



    Some Notes on SSH localhost tunnelling with SSH options
      NoHostAuthenticationForLocalhost=yes and UserKnownHostsFile=file:

    Warning:  Note that for proxy use with ssh(1), tunnels going through
    'localhost' are used.  This means ssh(1) thinks the remote hostname is
    'localhost', which may cause collisions and confusion when storing
    and checking SSH keys.

    By default on Unix when a 'localhost' ssh host is involved the
    ssh option -o NoHostAuthenticationForLocalhost=yes is applied (see
    ssh_config(1) for details.)  This avoids the warnings and ssh refusing
    to connect, but it reduces security.  A man in the middle attack may
    be possible.  SSVNC prints out a warning in the terminal every time
    the NoHostAuthenticationForLocalhost option is used.

    On Unix to disable the use of NoHostAuthenticationForLocalhost set the env.
    variable SSVNC_SSH_LOCALHOST_AUTH=1. This may induce extra ssh(1) dialogs.

    On Unix a MUCH SAFER and more convenient way to proceed is to set the
    known hosts option in Options -> Advanced -> 'Private SSH KnownHosts file'
    Then, only for the host in the current profile, a private known_hosts
    file will be used and so there will be no 'localhost' collisions.
    This method is secure (assuming you verify the SSH key fingerprint)
    and avoids the man in the middle attack.

    On Windows, Putty/Plink is used and does not have the UserKnownHosts
    or NoHostAuthenticationForLocalhost features.  Keys are stored in
    the registry as localhost:port pairs and so it is possible to use the
    'Port Slot' option to keep the keys separate to avoid the dialogs and
    also maintain good security.

    Note that for the "double SSH gateway" method the risk from using
    NoHostAuthenticationForLocalhost is significantly less because the first
    ssh connection does not use the option (it connects directly to the remote
    host) and the second one is only exposed for the leg inside the first
    gateway (but is still vulnerable there when NoHostAuthenticationForLocalhost
    is used.)

    As with a username that contains a space, use %SPACE (or %TAB) to
    indicate it in the SSH proxies, e.g. john%SPACEsmith@ssh.company.com

 UltraVNC Proxies/Gateways:

    UltraVNC has a "repeater" tool (http://www.uvnc.com/addons/repeater.html
    and http://koti.mbnet.fi/jtko/) that acts as a VNC proxy.  SSVNC can
    work with both mode I and mode II schemes of this repeater.

    For Unix and MacOS X there is another re-implementation of the
    UltraVNC repeater:

        http://www.karlrunge.com/x11vnc/ultravnc_repeater.pl

    So one does not need to run the repeater on a Windows machine.

    Note that even though the UltraVNC repeater tool is NOT SSL enabled,
    it can nevertheless act as a proxy for SSVNC SSL connections.
    This is because, just as with a Web proxy, the proxy negotiations
    occur before the SSL traffic starts.  (There is a separate UltraVNC
    tool, repeater_SSL.exe, that is SSL enabled and is discussed below.)

    Note: it seems only SSL SSVNC connections make sense with the
    UltraVNC repeater.  SSH connections (previous section) do not seem to
    and so are not enabled to (let us know if you find a way to use it.)

    Unencrypted (aka Direct) SSVNC VNC connections (Vnc:// prefix in
    'VNC Host:Display'; see Tip 5) also work with the UltraVNC repeater.

    MODE I REPEATER:

    For the mode I UltraVNC repeater the Viewer initiates the connection
    and passes a string that is the VNC server's IP address (or hostname)
    and port or display to the repeater (the repeater then makes the
    connection to the server host and then exchanges data back and forth.)
    To do this in SSVNC:

           VNC Host:Display:   :0
           Proxy/Gateway:      repeater://myuvncrep.west:5900+joes-pc:1

    Where "myuvncrep.west" is running the UltraVNC repeater and 
    "joes-pc:1" is the VNC server the repeater will connect us to.

    Note here that the VNC Host:Display can be anything because it is
    not used; we choose :0.  You cannot leave VNC Host:Display empty.

    The Proxy/Gateway format is repeater://proxy:port+vncserver:display.
    The string after the "+" sign is passed to the repeater server for
    it to interpret (and so does not have to be the UltraVNC repeater;
    you could create your own if you wanted to.)  For this example,
    instead of joes-pc:1 it could be joes-pc:5901 or 192.168.1.4:1,
    192.168.1.4:5901, etc.

    If you do not supply a proxy port, then the default 5900 is assumed,
    e.g. use repeater://myuvncrep.west+joes-pc:1 for port 5900 on
    myuvncrep.west then connecting to port 5901 on joes-pc.

    X11VNC: For mode I operation the VNC server x11vnc simply runs as
    a normal SSL/VNC server:

       x11vnc -ssl SAVE

    because the repeater will connect to it as a VNC client would.
    For mode II operation additional options are needed (see below.)


    MODE II REPEATER:

    For the mode II repeater both the VNC viewer and VNC server initiate
    TCP connections to the repeater proxy.  In this case they pass a string
    that identifies their mutual connection via "ID:NNNN", for example:

           VNC Host:Display:   :0
           Proxy/Gateway:      repeater://myuvncrep.west:5900+ID:2345

    again, the default proxy port is 5900 if not supplied.  And we need
    to supply a placeholder display ":0".

    The fact that BOTH the VNC viewer and VNC server initiate outgoing
    TCP connections to the repeater makes some things tricky, especially
    for the SSL aspect.  In SSL one side takes the 'client' role and
    the other side must take the 'server' role.  These roles must be
    coordinated correctly or otherwise the SSL handshake will fail.

    We now describe two scenarios: 1) SSVNC in Listening mode with STUNNEL
    in 'SSL server' role; and 2) SSVNC in Forward mode with STUNNEL in
    'SSL client' role.  For both cases we show how the corresponding
    VNC server x11vnc would be run.

    SSVNC Listening mode / STUNNEL 'SSL server' role:

      By default, when using SSL over a reverse connection the x11vnc VNC
      server will take the 'SSL client' role.  This way it can connect to a
      standard STUNNEL (SSL server) redirecting connections to a VNC viewer
      in Listen mode.  This is how SSVNC with SSL is normally intended to
      be used for reverse connections (i.e. without the UltraVNC Repeater.)

      To do it this way with the mode II UltraVNC Repeater; you set
      Options -> Reverse VNC Connection, i.e. a "Listening Connection".
      You should disable 'Verify All Certs' unless you have already
      saved the VNC Server's certificate to Accepted Certs.  Or you can
      set ServerCert to the saved certificate.  Then click 'Listen'.
      In this case an outgoing connection is made to the UltraVNC
      repeater, but everything else is as for a Reverse connection.

      Note that in Listening SSL mode you must supply a MyCert or use the
      "listen.pem" one you are prompted by SSVNC to create.

      X11VNC command:

        x11vnc -ssl -connect_or_exit repeater://myuvncrep.west+ID:2345


    SSVNC Forward mode / STUNNEL 'SSL client' role:

      x11vnc 0.9.10 and later can act in the 'SSL server' role for Reverse
      connections (i.e. as it does for forward connections.)  Set these
      x11vnc options: '-env X11VNC_DISABLE_SSL_CLIENT_MODE=1 -sslonly'

      The -sslonly option is to prevent x11vnc from thinking the delay in
      connection implies VeNCrypt instead of VNC over SSL.  With x11vnc
      in X11VNC_DISABLE_SSL_CLIENT_MODE mode, you can then have SSVNC make
      a regular forward connection to the UltraVNC repeater.

      Note that SSVNC may attempt to do a 'Fetch Cert' action in forward
      connection mode to either retrieve the certificate or probe for
      VeNCrypt and/or ANONDH.  After that 'Fetch Cert' is done the
      connection to the UltraVNC repeater will be dropped.  This is a
      problem for the subsequent real VNC connection.  You can disable
      'Verify All Certs' AND also set 'Do not Probe for VeNCrypt'
      to avoid the 'Fetch Cert' action.  Or, perhaps better, add to
      x11vnc command line '-connect_or_exit repeater://... -loop300,2'
      (in addition to the options in the previous paragraphs.)  That way
      x11vnc will reconnect once to the Repeater after the 'Fetch Cert'
      action.  Then things should act pretty much as a normal forward
      SSL connection.

      X11VNC 0.9.10 command (split into two lines):

        x11vnc -ssl -connect_or_exit repeater://myuvncrep.west+ID:2345 \ 
             -env X11VNC_DISABLE_SSL_CLIENT_MODE=1 -loop300,2 -sslonly

    We recommend using "SSVNC Forward mode / STUNNEL 'SSL client' role"
    if you are connecting to x11vnc 0.9.10 or later.  Since this does
    not use Listen mode it should be less error prone and less confusing
    and more compatible with other features.  Be sure to use all of
    the x11vnc options in the above command line.  To enable VeNCrypt,
    replace '-sslonly' with '-vencrypt force'.  If you do not indicate
    them explicitly to SSVNC, SSVNC may have to probe multiple times for
    VeNCrypt and/or ANONDH.  So you may need '-loop300,4' on the x11vnc
    cmdline so it will reconnect to the UltraVNC repeater 3 times.


    Note that for UNENCRYPTED (i.e. direct) SSVNC connections (see vnc://
    in Tip 5) using the UltraVNC Repeater mode II there is no need to
    use a reverse "Listening connection" and so you might as well use
    a forward connection.

    For Listening connections, on Windows after the VNC connection you
    MUST manually terminate the listening VNC Viewer (and connect again
    if desired.)  Do this by going to the System Tray and terminating
    the Listening VNC Viewer.  Subsequent connection attempts using the
    repeater will fail unless you do this and restart the Listen.

    On Unix and MacOS X after the VNC connection the UltraVNC repeater
    proxy script will automatically restart and reconnect to the repeater
    for another connection.  So you do not need to manually restart it.
    To stop the listening, kill the listening VNC Viewer with Ctrl-C.

    In the previous sections it was mentioned one can chain up to 3
    proxies together by separating them with commas: proxy1,proxy2,proxy3.
    Except where explicitly noted below this should work for "repeater://..."
    as the final proxy.  E.g. you could use a web proxy to get out of a
    firewall, and then connect to a remote repeater.

    The UltraVNC SSL enabled repeater_SSL.exe is discussed below.


 UltraVNC Single Click:

    UltraVNC has Single Click (SC) Windows VNC servers that allow naive
    users to get them running very easily (a EXE download and a few
    mouse clicks).  See http://sc.uvnc.com/ for details on how to create
    these binaries.  Also there is a how-to here:
    http://www.simply-postcode-lookup.com/SingleClickUltraVNC/SingleClickVNC.htm

    The SC EXE is a VNC *server* that starts up a Reverse VNC connection
    to a Listening Viewer (e.g. the viewer address/port/ID is hardwired
    into the SC EXE).  So SC is not really a proxy, but it can be used
    with UltraVNC repeater proxies and so we describe it here.

    One important point for SC III binary creation: do NOT include 
    "-id N" in the helpdesk.txt config file.  This is because the with
    SSVNC the Ultra VNC repeater IS NOT USED (see below for how to
    use it).  Use something like for helpdesk.txt:

       [TITLE]
       My UltraVNC SC III

       [HOST]
       Internet Support XYZ
       -sslproxy -connect xx.xx.xx.xx:5500 -noregistry

    (replace xx.xx.xx.xx with IP address or hostname of the SSVNC machine.)

    The Unix SSVNC vncviewer supports the both the unencrypted "SC I"
    mode and the SSL encrypted "SC III" mode.  For both cases SSVNC
    must be run in Listening mode (Options -> Reverse VNC Connection)

    For SC I, enable Reverse VNC Connection and put Vnc://0 (see Tip 5
    below) in the VNC Host:Display to disable encryption (use a different
    number if you are not using the default listening port 5500).
    Then click on the "Listen" button and finally have the user run your
    Single Click I EXE.

    BTW, we used this for a SC I helpdesk.txt:

       [TITLE]
       My UltraVNC SC I

       [HOST]
       Internet Support XYZ
       -connect xx.xx.xx.xx:5500 -noregistry

    For SC III (SSL), enable Reverse VNC Connection and then UNSET "Verify
    All Certs" (this is required).  Let the VNC Host:Display be ":0"
    (use a different number if you are not using the default listening
    port 5500).  Then click on the "Listen" button and finally have the
    user run your Single Click III EXE.

    Note that in Listening SSL mode you MUST supply a MyCert or use the 
    "listen.pem" one you are prompted by SSVNC to create.


 UltraVNC repeater_SSL.exe proxy:

    For repeater_SSL.exe SSL usage, with Single Click III or otherwise
    (available at http://www.uvnc.com/pchelpware/SCIII/index.html)
    it helps to realize that the ENTIRE connection is SSL encrypted,
    even the proxy host:port/ID:NNNN negotiation, and so a different
    approach needs to be taken from that described above in 'UltraVNC
    Proxies/Gateways'.  In this case do something like this:

           VNC Host:Display:   :0
           Proxy/Gateway:      sslrepeater://myuvncrep.west:443+ID:2345

    The sslrepeater:// part indicates the entire ID:XYZ negotiation must
    occur inside the SSL tunnel.  Listening mode is not required in this
    case: a forward VNC connection works fine (and is recommended).
    As before, the ":0" is simply a placeholder and is not used.
    Note that the UltraVNC repeater_SSL.exe listens on port 443 (HTTPS),
    (it is not clear that it can be modified to use another port.)

    Non-ID connections sslrepeater://myuvncrep.west:443+host:disp also
    work, but the 2nd leg repeater <-> host:disp must be unencrypted.
    The first leg SSVNC <-> repeater is, however, SSL encrypted.

    sslrepeater:// only works on Unix or MacOSX using the provided
    SSVNC vncviewer.  The modified viewer is needed; stock VNC viewers
    will not work.  Also, proxy chaining (bouncing off of more than one
    proxy) currently does not work for repeater_SSL.exe.


 VeNCrypt is treated as a proxy:

    SSVNC supports the VeNCrypt VNC security type.  You will find out more
    about this security type in the other parts of the Help documentation.
    In short, it does a bit of plain-text VNC protocol negotiation before
    switching to SSL/TLS encryption and authentication.

    SSVNC implements its VeNCrypt support as final proxy in a chain
    of proxies.  You don't need to know this or specify anything, but
    it is good to know since it uses up one of the 3 proxies you are
    allowed to chain together.  If you watch the command output you will
    see the vencrypt:// proxy item.

    You can specify that a VNC server uses VeNCrypt (Options -> Advanced)
    or you can let SSVNC try to autodetect VeNCrypt.


 IPv6 can be treated as a proxy for UN-ENCRYPTED connections:

    Read Tip 20 about SSVNC's IPv6 (128 bit IP addresses) support.
    In short, because stunnel and ssh support IPv6 hostnames and
    addresses, SSVNC does too without you needing to do anything.

    However, in some rare usage modes you will need to specify the IPv6
    server destination in the Proxy/Gateway entry box.  The only case
    this appears to be needed is when making an un-encrypted connection
    to an IPv6 VNC server.  In this case neither stunnel nor ssh are
    used and you need to specify something like this:

              VNC Host:Display:       localhost:0
              Proxy/Gateway:          ipv6://2001:4860:b009::68:5900

    and then select 'None' as the encryption type.  Note that the above
    'localhost:0' setting can be anything; it is basically ignored.

    Note that on Unix, MacOSX, and Windows un-encrypted ipv6 connections
    are AUTODETECTED and so you likely NEVER need to supply ipv6://
    Only try it if you encounter problems.  Also note that the ipv6://
    proxy type does not work on Windows, so only the autodetection is
    available there.

    Note that if there is some other proxy, e.g. SOCKS or HTTP and that
    proxy server is an IPv6 host (or will connect you to one) then any
    sort of connection through that proxy will work OK: un-encrypted as
    well as SSL or SSH connections, etc.

    Unencrypted connection is the only special case where you may need
    to specify an ipv6:// proxy.  If you find another use let us know.

    See Tip 20 for more info.
}

	set help_tips {
 Tips and Tricks:

     Table of Contents:

      1) Connect to Non-Standard SSH port.
      2) Reverse VNC connections (Listening)
      3) Global options in ~/.ssvncrc
      4) Fonts
      5) vnc://host for un-encrypted connection
      6) Home directory for memory stick usage, etc.
      7) vncs:// vncssl:// vnc+ssl:// vnc+ssh:// URL-like prefixes
      8) sshvnc / -ssh SSH only GUI
      9) tsvnc / -ts Terminal services only GUI (SSH+x11vnc)
     10) 2nd GUI window on Unix/MacOSX
     11) Ctrl-L or Button3 to Load profile
     12) SHELL command or Ctrl-S for SSH terminal w/o VNC
     13) KNOCK command for port-knock sequence
     14) Unix/MacOSX general SSL redirector (not just VNC)
     15) Environment variables
     16) Bigger "Open File" dialog window
     17) Unix/MacOSX extra debugging output
     18) Dynamic VNC Server Port determination with SSH
     19) No -t ssh cmdline option for older sshd
     20) IPv6 support.

     1) To connect in SSH-Mode to a server running SSH on a non-standard
        port (22 is the standard port) you need to use the Proxy/Gateway
        setting.  The following is from the Proxies Help panel:

        NON-STANDARD SSH PORT: To use a non-standard ssh port (i.e. a port other
        than 22) you need to use the Proxy/Gateways as well.  E.g. something
        like this for port 2222: 
    
               VNC Host:Display:   localhost:0
               Proxy/Gateway:      joe@far-away.east:2222
    
        The username@ is not needed if it is the same as on the client.  This
        will also work going to a different internal machine, e.g. "joes-pc:0"
        instead of "localhost:0", as in the first example.
    
     2) Reverse VNC connections (Listening) are possible as well.
        In this case the VNC Server initiates the connection to your
        waiting (i.e. listening) SSVNC viewer.

        Go to Options and select "Reverse VNC connection".  In the 'VNC
        Host:Display' entry box put in the number (e.g. "0" or ":0", or
        ":1", etc) that corresponds to the Listening display (0 -> port
        5500, 1 -> port 5501, etc.) you want to use.  Then clicking on
        'Listen' puts your SSVNC viewer in a "listening" state on that
        port number, waiting for a connection from the VNC Server.

        On Windows or using a 3rd party VNC Viewer multiple, simultaneous
        reverse connections are always enabled.  On Unix/MacOSX with the
        provided ssvncviewer they are disabled by default.  To enable them:
        Options -> Advanced -> Unix ssvncviewer -> Multiple LISTEN Connections

        Specify a specific interface, e.g. 192.168.1.1:0 to have stunnel
        only listen on that interface.  IPv6 works too, e.g. :::0 or ::1:0
        This also works for UN-encrypted reverse connections as well ('None').

        See the Options Help for more info.

     3) You can put global options in your ~/.ssvncrc file (ssvnc_rc on
        Windows). Currently they are:

	Put "mode=tsvnc" or "mode=sshvnc" in the ~/.ssvncrc file to have
	the application start up in the given mode.

        desktop_type=wmaker    (e.g.) to switch the default Desktop Type.

        desktop_size=1280x1024 (e.g.) to switch the default Desktop Size.

        desktop_depth=24       (e.g.) to switch the default Desktop Color Depth

        xserver_type=Xdummy    (e.g.) to switch the default X Server Type.

        (The above 4 settings apply only to the Terminal Services Mode.)

        noenc=1  (same as the -noenc option for a 'No Encryption' option)
        noenc=0  (do not show the 'No Encryption' option)

        killstunnel=1 (same as -killstunnel), on Windows automatically kills
        the STUNNEL process when the viewer exits.  Disable via killstunnel=0
        and -nokillstunnel.

        ipv6=0   act as though IPv6 was not detected.
        ipv6=1   act as though IPv6 was detected.

        cotvnc=1 have the default vncviewer on Mac OS X be the Chicken of
        the VNC.  By default the included ssvnc X11 vncviewer is used
        (requires Mac OS X X11 server to be running.)

        mycert=file (same as -mycert file option).  Set your default MyCert
        to "file".  If file does not exist ~/.vnc/certs/file is used.

        cacert=file (same as -cacert file option).  Set your default ServerCert
        to "file".  If file does not exist ~/.vnc/certs/file is used.  If
        file is "CA" then ~/.vnc/certs/CA/cacert.pem is used.

        crl=file (same as -crl file option).  Set your default CRL File
        to "file".  If file does not exist ~/.vnc/certs/file is used.

        Prefix any of these cert/key files with "FORCE:" to make them
        immutable, e.g.  "cacert=FORCE:CA".

        You can set any environment variable in ~/.ssvncrc by using a line
        like env=VAR=value, for example:  env=SSVNC_FINISH_SLEEP=2

        To change the fonts (see Tip 4 below for examples):

        font_default=tk-font-name     (sets the font for menus and buttons)
        font_fixed=tk-font-name       (sets the font for help text)

     4) Fonts: To change the tk fonts, set these environment variables
        before starting up ssvnc: SSVNC_FONT_DEFAULT and SSVNC_FONT_FIXED.
        For example:

            % env SSVNC_FONT_DEFAULT='helvetica -20 bold' ssvnc
            % env SSVNC_FONT_FIXED='courier -14' ssvnc

        or set both of them at once.  You can also set 'font_default' and
        'font_fixed' in your ~/.ssvncrc.  E.g.:

        font_default=helvetica -16 bold
        font_fixed=courier -12

     5) If you want to make a Direct VNC connection, WITH *NO* SSL OR
        SSH ENCRYPTION or authentication, use the "vnc://" prefix in the
        VNC Host:Display entry box, e.g. "vnc://far-away.east:0"  This
        also works for reverse connections, e.g. vnc://0

        Use Vnc:// (i.e. capital 'V') to avoid being prompted if you are
        sure you want no encryption.  For example, "Vnc://far-away.east:0"
        Shift+Ctrl-E in the entry box is a short-cut to add or remove
        the prefix "Vnc://" from the host:disp string.

        You can also run ssvnc with the '-noenc' cmdline option (now
        the default) to have a check option 'None' that lets you turn off
        Encryption (and profiles will store this setting).  Pressing Ctrl-E
        on the main panel is a short-cut to toggle between the -noenc 'No
        Encryption' mode and normal mode.  The option "Show 'No Encryption'
        Option" under Options also toggles it.

        The '-enc' option disables the button (and so makes it less obvious
        to naive users how to disable encryption.)

        Note as of SSVNC 1.0.25 the '-noenc' mode is now the default. I.e.
        the 'No Encryption' option ('None') is shown by default.  When
        you select 'None' you do not need to supply the "vnc://" prefix.
        To disable the button supply the '-enc' cmdline option.

        Setting SSVNC_DISABLE_ENCRYPTION_BUTTON=1 in your environment is
        the same as -noenc.  You can also put noenc=1 in your ~/.ssvncrc file.

        Setting SSVNC_DISABLE_ENCRYPTION_BUTTON=0 in your environment is
        the same as -enc.  You can also put noenc=0 in your ~/.ssvncrc file.

        Please be cautious/thoughtful when you make a VNC connection with
        encryption disabled.  You may send sensitive information (e.g. a
        password) over the network that can be sniffed.

        It is also possible (although difficult) for someone to hijack an
        existing unencrypted VNC session.

        Often SSVNC is used to connect to x11vnc where the Unix username and
        password is sent over the channel.  It would be a very bad idea to
        let that data be sent over an unencrypted connection!  In general,
        it is not wise to have a plaintext VNC connection.

        Note that even the VNC Password challenge-response method (the password
        is not sent in plaintext) leaves your VNC password susceptible to a
        dictionary attack unless encryption is used to hide it.

        So (well, before we made the button visible by default!) we forced
        you to learn about and supply the "vnc://" or "Vnc://" prefix to
        the host:port or use -noenc or the "Show 'No Encryption' Option"
        to disable encryption.  This is a small hurdle, but maybe someone
        will think twice.  It is a shame that VNC has been around for
        over 10 years and still does not have built-in strong encryption.

        Note the Vnc:// or vnc:// prefix will be stored in any profile that
        you save so you do not have to enter it every time.

        Set the env var SSVNC_NO_ENC_WARN=1 to skip the warning prompts the
        same as the capitalized Vnc:// does.

     6) Mobile USB memory stick / flash drive usage:  You can unpack
        ssvnc to a flash drive for impromptu usage (e.g. from a friends
        computer). 

        If you create a directory "Home" in the toplevel ssvnc directory,
        then that will be the default location for your VNC profiles
        and certs.  So they follow the drive this way.  If you run like
        this: "ssvnc ." or "ssvnc.exe ." the "Home" directory will be
        created for you.

        WARNING: if you use ssvnc from an "Internet Cafe", i.e. an
        untrusted computer, an unscrupulous person may be capturing
        keystrokes, etc.!

	You can also set the SSVNC_HOME env. var. to point to any
	directory you want. It can be set after starting ssvnc by putting
	HOME=/path/to/dir in the Host:Display box and clicking "Connect".

        For a Windows BAT file to get the "Home" directory correct
        something like this might be needed:

         cd \ssvnc\Windows
         start \ssvnc\Windows\ssvnc.exe 

     7) In the VNC Host:Display entry you can also use these "URL-like"
        prefixes:

           vncs://host:0, vncssl://host:0, vnc+ssl://host:0  for SSL

        and

           vncssh://host:0, vnc+ssh://host:0                 for SSH

        There is no need to toggle the SSL/SSH setting.  These also work
        from the command line, e.g.:  ssvnc vnc+ssh://mymachine:10

     8) If you want this application to be SSH only, then supply the
        command line option "-ssh" or set the env. var SSVNC_SSH_ONLY=1.

        Then no GUI elements specific to SSL will appear (the
        documentation wills still refer to the SSL mode, however).
        To convert a running app to ssh-only select "Mode: SSH-Only"
        in Options.

        The wrapper scripts "sshvnc" and "sshvnc.bat" will start it up
        automatically this way.

        Or in your ~/.ssvncrc (or ~/ssvnc_rc on Windows) put "mode=sshvnc"
        to have the tool always start up in that mode.

     9) For an even simpler "Terminal Services" mode use "tsvnc" or
        "tsvnc.bat" (or "-ts" option).  This mode automatically launches
        x11vnc on the remote side to find or create your Desktop session
        (usually the Xvfb X server).  So x11vnc must be available on the
        remote server machines under "Terminal Services" mode.

        From a full ssvnc you can press Ctrl-h to go into ssh-only mode
        and Ctrl-t to toggle between "tsvnc" and "ssvnc" modes.  The
        Options Mode menu also let you switch.

        Or in your ~/.ssvncrc (or ~/ssvnc_rc on Windows) put "mode=tsvnc"
        to have the tool always start up in that mode.

    10) On Unix to get a 2nd GUI (e.g. for a 2nd connection) press Ctrl-N
        on the GUI.  If only the xterm window is visible you can press
        Ctrl-N or try Ctrl-LeftButton -> New SSVNC_GUI.  On Windows you
        will have to manually Start a new one: Start -> Run ..., etc.

    11) Pressing the "Load" button or pressing Ctrl-L or Clicking the Right
        mouse button on the main GUI will invoke the Load dialog.

        Pressing Ctrl-O on the main GUI will bring up the Options Panel.
        Pressing Ctrl-A on the main GUI will bring up the Advanced Options.

    12) If you use "SHELL" for the "Remote SSH Command" (or in the display
        line: "user@hostname cmd=SHELL") then you get an SSH shell only:
        no VNC viewer will be launched.  On Windows "PUTTY" will try
        to use putty.exe (better terminal emulation than plink.exe).

        A ShortCut for this is Ctrl-S with user@hostname in the entry box.

    13) If you use "KNOCK" for the "Remote SSH Command" (or in the display
        line "user@hostname cmd=KNOCK") then only the port-knocking is done.

        A ShortCut for this is Ctrl-P with hostname the entry box.

        If it is KNOCKF, i.e. an extra "F", then the port-knocking
        "FINISH" sequence is sent, if any.  A ShortCut for this
        Shift-Ctrl-P as long as hostname is present.

    14) On Unix to have SSVNC act as a general STUNNEL redirector (i.e. no
        VNC), put the desired host:port in VNC Host:Display (use a
        negative port value if it is to be less than 200), then go to
        Options -> Advanced -> Change VNC Viewer.  Change the "viewer"
        command to be "xmessage OK" or "xmessage <port>" (or sleep) where
        port is the desired local listening port.  Then click Connect.
        If you didn't set the local port look for it in the terminal output.

        On Windows set 'viewer' to "NOTEPAD" or similar; you can't
        control the port though.  It is usually 5930, 5931, ... Watch
        the messages or look at the stunnel log.

    15) Tricks with environment variables:

        You can change the X DISPLAY variable by typing DISPLAY=... into
        VNC Host:Display and hitting Return or clicking Connect. Same
        for HOME=.  On Mac, you can set DYLD_LIBRARY_PATH=... too.
        It should propagate down the viewer.

        Setting SLEEP=n increases the amount of time waited before
        starting the viewer.  The env. var. SSVNC_EXTRA_SLEEP also does
        this (and also Sleep: Option setting) Setting FINISH=n sets the
        amount of time slept before the Terminal window exits on Unix
        and MacOS X.  (same as SSVNC_FINISH_SLEEP env. var.)

        Full list of parameters HOME/SSVNC_HOME, DISPLAY/SSVNC_DISPLAY
        DYLD_LIBRARY_PATH/SSVNC_DYLD_LIBRARY_PATH, SLEEP/SSVNC_EXTRA_SLEEP
        FINISH/SSVNC_FINISH_SLEEP, DEBUG_NETSTAT, REPEATER_FORCE,
        SSH_ONLY, TS_ONLY, NO_DELETE, BAT_SLEEP, IPV6/SSVNC_IPV6=0 or 1.
        See below for more info.  (the ones joined by "/" are equivalent
        names, and the latter can be set as an env. var. as well.)

        After you set the parameter, clear out the 'VNC Host:Display'
        entry and replace it with the actual host and display number.

        To replace the xterm terminal where most of the external commands
        are run set SSVNC_XTERM_REPLACEMENT to a command that will run
        a command in a terminal.  I.e.:  "$SSVNC_XTERM_REPLACEMENT cmd"
        will run cmd.  If present, %GEOMETRY is expanded to a desired
        +X+Y geometry.  If present, %TITLE is expanded to a desired title.
        Examples: SSVNC_XTERM_REPLACEMENT='gnome-terminal -e'
                  SSVNC_XTERM_REPLACEMENT='gnome-terminal -t "%TITLE" -e'
                  SSVNC_XTERM_REPLACEMENT='konsole -e'

        More info: EXTRA_SLEEP: seconds of extra sleep in scripts; 
        FINISH_SLEEP: final extra sleep at end; DEBUG_NETSTAT put up a
        window showing what netstat reports; NO_DELETE: do not delete tmp
        bat files on Windows (for debugging); BAT_SLEEP: sleep this many
        seconds at the end of each Windows bat file (for debugging.) 

        You can also set any environment variable by entering in something
        like ENV=VAR=VAL  e.g. ENV=SSH_AUTH_SOCK=/tmp/ssh-BF2297/agent.2297
        Use an empty VAL to unset the variable.

        There are also a HUGE number of env. vars. that apply to the Unix
        and MacOS X wrapper script 'ss_vncviewer' and/or the ssvncviewer
        binary.  See Options -> Advanced -> Unix ssvncviewer -> Help for
        all of them.

    16) On Unix you can make the "Open File" and "Save File" dialogs
        bigger by setting the env. var. SSVNC_BIGGER_DIALOG=1 or
        supplying the -bigger option.  If you set it to a Width x Height,
        e.g. SSVNC_BIGGER_DIALOG=500x200, that size will be used.

    17) On Unix / MacOSX to enable debug output you can set these env.
        vars to 1: SSVNC_STUNNEL_DEBUG, SSVNC_VENCRYPT_DEBUG, and
        SS_DEBUG (very verbose)

    18) Dynamic VNC Server Port determination and redirection:  If you
        are running SSVNC on Unix and are using SSH to start the remote
        VNC server and the VNC server prints out the line "PORT=NNNN"
        to indicate which dynamic port it is using (x11vnc does this),
        then if you prefix the SSH command with "PORT=" SSVNC will watch
        for the PORT=NNNN line and uses ssh's built in SOCKS proxy
        (ssh -D ...) to connect to the dynamic VNC server port through
        the SSH tunnel.  For example:

                VNC Host:Display     user@somehost.com
                Remote SSH Command:  PORT= x11vnc -find -nopw

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

        On Windows if PORT= is supplied SOCKS proxying is not used, but
        rather a high, random value of the VNC port is chosen (e.g. 8453)
        and assumed to be free, and is passed to x11vnc's -rfbport option.
        This only works with x11vnc (not vncserver).

    19) On Unix if you are going to an older SSH server (e.g. Solaris 10),
        you will probably need to set the env. var. SS_VNCVIEWER_NO_T=1
        to disable the ssh "-t" option being used (that can prevent the
        command from being run).

    20) SSVNC is basically a wrapper for the stunnel and ssh programs,
        and because those two programs have good IPv6 support SSVNC will
        for most usage modes support it as well.  IPv6 is 128 bit internet
        addresses (as opposed to IPv4 with its 32 bit xxx.yyy.zzz.nnn IPs.

        So for basic SSL and SSH connections if you type in an IPv6 IP
        address, e.g. '2001:4860:b009::68', or a hostname with only an
        IPv6 lookup, e.g. ipv6.l.google.com, the connection will work
        because stunnel and ssh handle these properly.

        Note that you often need to supply a display number or port after
        the address so put it, e.g. ':0' at the end: 2001:4860:b009::68:0
        You can also use the standard notation [2001:4860:b009::68]:0
        that is more clear.  You MUST specify the display if you use
        the IPv6 address notation (but :0 is still the default for a
        non-numeric hostname string.)

        IPv4 addresses encoded in IPv6 notation also work, e.g.
        ::ffff:192.168.1.100 should work for the most part.

        SSVNC on Unix and MacOSX also has its own Proxy helper tool
        (pproxy)  This script has been modified to handle IPv6 hostnames
        and addresses as long as the IO::Socket::INET6 Perl module
        is available.  On Windows the relay6.exe tool is used.

        So for the most part IPv6 should work without you having to do
        anything special.  However, for rare usage, the proxy helper tool
        can also treat and IPv6 address as a special sort of 'proxy'.
        So in the entry Proxy/Gateway you can include ipv6://host:port
        and the IPv6 host will simply be connected to and the data
        transferred.  In this usage mode, set the VNC Host:Display
        to anything, e.g. 'localhost:0'; it is ignored if the ipv6://
        endpoint is specified as a proxy.  Need for ipv6:// usage proxy
        should be rare.

        Note that for link local (not global) IPv6 addresses you may
        need to include the network interface at the end of the address,
        e.g. fe80::a00:20ff:fefd:53d4%eth0

        Note that one can use a 3rd party VNC Viewer with SSVNC (see
        Options -> Advanced -> Change VNC Viewer.)  IPv6 will work for
        them as well even if they do not support IPv6.

        IPv6 support on Unix, MacOSX, and Windows is essentially complete
        for all types of connections (including proxied, unencrypted and
        reverse connections.)  Let us know if you find a scenario that
        does not work (see the known exception for putty/plink below.)

        You can set ipv6=0 in your ssvncrc, then no special relaying for
        IPv6 will be done (do this if there are problems or slowness in
        trying to relay ipv6 and you know you will not connect to any
        such hosts.)  Set ipv6=1 to force the special processing even if
        IPv6 was not autodetected.  To change this dynamically, you also
        enter IPV6=... in the VNC Host:Display entry box and press Enter.
        Also on Unix or MacOSX you can set the env. var. SSVNC_IPV6=0
        to disable the wrapper script from checking if hosts have ipv6
        addresses (this is the same as setting ipv6=0 in ssvncrc or by
        the setting ipv6 in the Entry box.)

        On Windows plink.exe (SSH client) currently doesn't work for
        IPv6 address strings (e.g. 2001:4860:b009::68) but it does work
        for hostname strings that resolve to IPv6 addresses.

        Note that one can make a home-brew SOCKS5 ipv4-to-ipv6 gateway
        proxy using ssh like this:

          ssh -D '*:1080' localhost "printf 'Press Enter to Exit: '; read x"
  
        then specify a proxy like socks5://hostname:1080 where hostname
        is the machine running the above ssh command.  Add '-v' to the
        ssh cmdline for verbose output.  See also the x11vnc inet6to4 tool
        (a direct ipv4/6 relay, not socks.)
}

	global version
	set help_main "                             SSVNC version: $version\n$help_main"
	set help_misc "                             SSVNC version: $version\n$help_misc"
	set help_prox "                             SSVNC version: $version\n$help_prox"
	set help_tips "                             SSVNC version: $version\n$help_tips"

	frame .h.w
	button .h.w.b1 -text "Main"    -command {help_text main}
	button .h.w.b2 -text "Proxies" -command {help_text prox}
	button .h.w.b3 -text "Misc"    -command {help_text misc}
	button .h.w.b4 -text "Tips"    -command {help_text tips}

	pack .h.w.b1 .h.w.b2 .h.w.b3 .h.w.b4 -side left -fill x -expand 1

	pack .h.w -side bottom -after .h.d -fill x

	.h.f.t insert end $help_main
	jiggle_text .h.f.t
}

proc help_text {which} {
	global help_main help_misc help_prox help_tips
	set txt ""
	if {$which == "main"} {
		set txt $help_main
	}
	if {$which == "misc"} {
		set txt $help_misc
	}
	if {$which == "prox"} {
		set txt $help_prox
	}
	if {$which == "tips"} {
		set txt $help_tips
	}
	catch {.h.f.t delete 0.0 end; .h.f.t insert end $txt; jiggle_text .h.f.t}
}

proc ssvnc_escape_help {} {
	toplev .ekh

	scroll_text_dismiss .ekh.f

	center_win .ekh
	wm title .ekh "SSVNC Escape Keys Help"

	set msg {
 SSVNC Escape Keys:

   The Unix SSVNC VNC Viewer, ssvncviewer(1), has an 'Escape Keys'
   mechanism that enables using keystrokes that are bound as 'Hot Keys'
   to specific actions.

   So, when you have all of the modifier keys ('escape keys') pressed down,
   then subsequent keystrokes are interpreted as local special actions
   instead of being sent to the remote VNC server.
   
   This enables quick parameter changing and also panning of the viewport.
   E.g. the keystroke 'r' is mapped to refresh the screen.
   
   Enter 'default' in the entry box to enable this feature and to use the
   default modifier list (Alt_L,Super_L on unix and Control_L,Meta_L on
   macosx) or set it to a list of modifier keys, e.g. Alt_L,Control_L.
   Note that _L means left side of keyboard and _R means right side.

   Alt_L is the 'Alt' key on the left side of the keyboard, and Super_L
   is usually the 'WindowsFlaggie(TM)' on the left side of the keyboard,
   so when both of those are pressed, the escape keys mapping take effect.


   Here is info from the ssvncviewer(1) manual page:

     -escape str    This sets the 'Escape Keys' modifier sequence and enables
                    escape keys mode.  When the modifier keys escape sequence
                    is held down, the next keystroke is interpreted locally
                    to perform a special action instead of being sent to the
                    remote VNC server.

                    Use '-escape default' for the default modifier sequence.
                    (Unix: Alt_L,Super_L and MacOSX: Control_L,Meta_L)

    Here are the 'Escape Keys: Help+Set' instructions from the Popup Menu:

    Escape Keys:  Enter a comma separated list of modifier keys to be the
    'escape sequence'.  When these keys are held down, the next keystroke is
    interpreted locally to invoke a special action instead of being sent to
    the remote VNC server.  In other words, a set of 'Hot Keys'.
    
    To enable or disable this, click on 'Escape Keys: Toggle' in the Popup.
    
    Here is the list of hot-key mappings to special actions:
    
       r: refresh desktop  b: toggle bell   c: toggle full-color
       f: file transfer    x: x11cursor     z: toggle Tight/ZRLE
       l: full screen      g: graball       e: escape keys dialog
       s: scale dialog     +: scale up (=)  -: scale down (_)
       t: text chat                         a: alphablend cursor
       V: toggle viewonly  Q: quit viewer   1 2 3 4 5 6: UltraVNC scale 1/n
    
       Arrow keys:         pan the viewport about 10% for each keypress.
       PageUp / PageDown:  pan the viewport by a screenful vertically.
       Home   / End:       pan the viewport by a screenful horizontally.
       KeyPad Arrow keys:  pan the viewport by 1 pixel for each keypress.
       Dragging the Mouse with Button1 pressed also pans the viewport.
       Clicking Mouse Button3 brings up the Popup Menu.
    
    The above mappings are *always* active in ViewOnly mode, unless you set the
    Escape Keys value to 'never'.
    
    If the Escape Keys value below is set to 'default' then a default list of
    of modifier keys is used.  For Unix it is: Alt_L,Super_L and for MacOSX it
    is Control_L,Meta_L.  Note: the Super_L key usually has a Windows(TM) Flag
    on it.  Also note the _L and _R mean the key is on the LEFT or RIGHT side
    of the keyboard.
    
    On Unix   the default is Alt and Windows keys on Left side of keyboard.
    On MacOSX the default is Control and Command keys on Left side of keyboard.
    
    Example: Press and hold the Alt and Windows keys on the LEFT side of the
    keyboard and then press 'c' to toggle the full-color state.  Or press 't'
    to toggle the ultravnc Text Chat window, etc.
    
    To use something besides the default, supply a comma separated list (or a
    single one) from: Shift_L Shift_R Control_L Control_R Alt_L Alt_R Meta_L
    Meta_R Super_L Super_R Hyper_L Hyper_R or Mode_switch.
}

	.ekh.f.t insert end $msg
	jiggle_text .ekh.f.t
}

#    Or Alternatively one can supply both hosts separated by
#    spaces (with the proxy second) in the VNC Host:Display box:
#
#           VNC Host-Display:   far-away.east:0    theproxy.net:8080
#
#    This looks a little strange, but it actually how SSVNC stores the
#    host info internally.

#    You can also specify the remote SSH command by putting a string like
#    
#         cmd=x11vnc -nopw -display :0 -rfbport 5900 -localhost
#
#    (use any command you wish to run) at the END of the VNC Host:Display
#    entry.  In general, you can cram it all in the VNC Host:Display if
#    you like:   host:disp  proxy:port  cmd=...  (this is the way it is
#    stored internally).

proc help_certs {} {
	toplev .ch

	set h 33
	if [small_height] {
		set h 28
	}
	scroll_text_dismiss .ch.f 87 $h

	center_win .ch
	wm resizable .ch 1 0

	wm title .ch "SSL Certificates Help"

	set msg {
 Description:

    *** IMPORTANT ***: Only with SSL Certificate verification (either manually
    or via a Certificate Authority certificate) can Man-In-The-Middle attacks be
    prevented.  Otherwise, only passive network sniffing attacks are prevented.
    There are hacker tools like dsniff/webmitm and cain that implement SSL
    Man-In-The-Middle attacks.  They rely on the client user not bothering to
    check the cert.

    Some people may be confused by the above because they are familiar with
    their Web Browser using SSL (i.e. https://... websites) and those sites
    are authenticated securely without the user's need to verify anything
    manually.  The reason why this happens automatically is because 1) their
    web browser comes with a bundle of Certificate Authority certificates
    and 2) the https sites have paid money to the Certificate Authorities to
    have their website certificate signed by them.  When using SSL in VNC we
    normally do not do something this sophisticated, and so we have to verify
    the certificates manually.  However, it is possible to use Certificate
    Authorities with SSVNC; that method is described below.

    The SSL Certificate files described below may have been created externally
    (e.g. by x11vnc or openssl): you can import them via "Import Certificate".
    OR you can click on "Create Certificate ..." to use THIS program to generate
    a Certificate + Private Key pair for you (in this case you will need to
    distribute one of the generated files to the VNC Server).

    Then you associate the Saved cert with the VNC server, see the panel entry
    box description below.  Then click Connect.  You will usually want to Save
    this association in a VNC Server profile for the next time you connect.

 Expiration:

    SSL Certificates will Expire after a certain period (usually 1-2 years;
    if you create a cert with this tool you can set it to any length you want).
    So if for a particular Cert you find you can no longer connect, check the
    STUNNEL log output to see if the cert has expired.  Then create and distribute
    a new one.

 Fetch Cert:

    You can also retrieve and view the VNC Server's Cert via the "Fetch Cert"
    button on the main panel.  After you check that it is the correct Cert (e.g. by
    comparing MD5 hash or other info), you can save it.  The file it was saved
    as will be set as the "ServerCert" to verify against for the next connection.
    To make this verification check permanent, you will need to save the profile
    via 'Save'.

    NOTE: See the CA section below for how "Fetch Cert/Verify All Certs" WILL NOT
    WORK when a Certificate Authority (CA) is used (i.e. you need to save the CA's
    cert instead.)  It will work if the certificate is Self-Signed.

 Verify All Certs:

    If "Verify All Certs" is checked on the main panel, you are always forced
    to check unrecognized server certs, and so the first time you connect to
    a new server you may need to follow a few dialogs to inspect and save the
    server certificate.

    Under "Verify All Certs", new certificates are saved in the 'Accepted Certs'
    directory.  When the checkbox is set all host profiles with "CertsDir" set to
    "ACCEPTED_CERTS" (and an empty "ServerCert" setting) will be checked against
    the pool of accepted certificates in the 'Accepted Certs' directory.

    Note that we have "Verify All Certs" on by default so that users who do not
    understand the SSL Man-In-The-Middle problem will not be left completely
    vulnerable to it.  Everyone still must make the effort to verify new
    certificates by an external method to be completely safe.

    To have "Verify All Certs" toggled off at startup, use "ssvnc -nv" or set
    SSVNC_NO_VERIFY_ALL=1 before starting.  If you do not even want to see the
    button, use "ssvnc -nvb" or SSVNC_NO_VERIFY_ALL_BUTTON=1.

    Note: "Fetch Cert" and "Verify All Certs" are currently not implemented in
    "SSH + SSL" mode.  In this case to have server authentication "ServerCert"
    must be set explicitly to a file (or "CertsDir" to a directory).

    Also note that "Fetch Cert" only works in a limited fashion in "Listen"
    mode (it is the VNC Server that initiates the connection), and so you
    may need to be set via "ServerCert" as well.

    NOTE: See the CA section below for how "Fetch Cert/Verify All Certs"
    WILL NOT WORK when a Certificate Authority (CA) is used (i.e. you need
    to save the CA's cert instead.)  The "Fetch Cert" saving method will
    work if the certificate is Self-Signed.

 CA:

    One can make SSL VNC server authentication more "automatic" as it is in
    Web Browsers going to HTTPS sites, by using a Certificate Authority (CA)
    cert (e.g. a professional one like Verisign or Thawte, or one your company
    or organization creates) for the "ServerCert".  This is described in detail
    here: http://www.karlrunge.com/x11vnc/ssl.html

    CA's are not often used, but if the number of VNC Servers scales up it can
    be very convenient because the viewers (i.e. SSVNC) only need the CA cert,
    not all of the Server certs.

    IMPORTANT NOTE: if a VNC Server is using a CA signed certificate instead
    of its own Self-Signed one, then "Fetch Cert", etc. saving mechanism
    WILL NOT WORK.  You must obtain the CA certificate and explicitly set
    it as the ServerCert or import it to 'Accepted Certs'.


 Now what goes into the panel's entry boxes is described.


 Your Certificate + Key (MyCert):

    You can specify YOUR own SSL certificate (PEM) file in "MyCert" in which
    case it is used to authenticate YOU (the viewer) to the remote VNC Server.
    If this fails the remote VNC Server will drop the connection.

    So the Server could use this method to authenticate Viewers instead of the
    more common practice of using a VNC password or x11vnc's -unixpw mode.


 Server Certificates (ServerCert/CertsDir):
    
    Server certs can be specified in one of two ways:
    
        - A single certificate (PEM) file for a single server
          or a single Certificate Authority (CA)
    
        - A directory of certificate (PEM) files stored in
          the special OpenSSL hash fashion.
    
    The former is set via "ServerCert" in this gui.
    The latter is set via "CertsDir" in this gui.
    
    The former corresponds to the "CAfile" STUNNEL parameter.
    The latter corresponds to the "CApath" STUNNEL parameter.

    See stunnel(8) or stunnel.mirt.net for more information.
    
    If the remote VNC Server fails to authenticate itself with respect to the
    specified certificate(s), then the VNC Viewer (your side) will drop the
    connection.

    Select which file or directory by clicking on the appropriate "Browse..."
    button.  Once selected, if you click Info or the Right Mouse button on
    "Browse..."  then information about the certificate will be displayed.

    If, as is the default, "CertsDir" is set to the token "ACCEPTED_CERTS"
    (and "ServerCert" is unset) then the certificates accumulated in the special
    'Accepted Certs' directory will be used.  "ACCEPTED_CERTS" is the default for
    every server ("Verify All Certs").  Note that if you ever need to clean this
    directory, each cert is saved in two files, for example:

          hostname-0=bf-d0-d6-9c-68-5a-fe-24-c6-60-ba-b4-14-e6-66-14.crt
    and
          9eb7c8be.0

    This is because of the way OpenSSL must use hash-based filenames in Cert dirs. 
    The file will have a "full filename:" line indicating the fingerprint and
    hostname associated with it.  Be sure to remove both files.  The Delete Certs
    dialog should automatically find the matching one for you and prompt you to
    remove it as well.

 Certificate Revocation List (CRL File):

    For large scale deployments, usually involving a CA Cert, it is worthwhile
    to be able to revoke individual certs (so that a new CA cert does not need to
    be created and new keys distributed).  Set CRL File to the path to the
    file containing the revoked certificates (or a directory containing 
    OpenSSL style hash-based filenames.)  See the x11vnc -sslCRL documentation
    for how to create CRL's.  In short, the commands 'openssl ca -revoke ...'
    and 'openssl ca -gencrl ...' are the ones to look for; See the ca(1) manpage.

 Create Certificate:

    A simple dialog to create a Self-Signed Certificate.  See the x11vnc
    -sslGenCA, -sslGenCert options for creating a CA Cert and signing with it.

 Import Certificate:

    You can paste in a Certificate or read one in from a file to add to your
    list of Server Certificates.  If (also) saved in the 'Accepted Certs'
    directory, it will be automatically used to verify any Server when in
    'Verify All Certs' Mode. 

 Deleting Certificates:

    To delete a Certificate+private_key pair click on "Delete Certificate"
    and select one in the menu.  You will be prompted to remove it,
    and also any corresponding .pem or .crt file.  For "ACCEPTED_CERTS"
    it will find the matching "HASH" file and prompt you to remove that too.


 Default Certs and Keys:

    Use the "-mycert file" option (same as "-cert file") to set a default
    MyCert.  The user will then have to manually clear the field to not
    use a certificate.  This is the same as "mycert=file" (also "cert=file")
    in the ~/.ssvncrc file.  If "file" does not exist, then ~/.vnc/certs is
    prepended to it.

    Use the "-cacert file" option (same as "-ca file") to set a default
    ServerCert.  The user will then have to manually clear the field to not
    set a server cert.  This is the same as "cacert=file" (also "ca=file")
    in the ~/.ssvncrc file.  If "file" does not exist, then ~/.vnc/certs is
    prepended to it.  Use "-cacert CA" to set it to ~/.vnc/certs/CA/cacert.pem

    Use the "-crl file" option to set a default CRL File.  The user will
    then have to manually clear the field to not use a CRL.  This is the
    same as "crl=file" in the ~/.ssvncrc file.  If "file" does not exist,
    then ~/.vnc/certs is prepended to it.

    A sys-admin might set up an SSVNC deployment for user's workstations or
    laptops using one or more of -cacert (authenticate VNC server to the
    user) or -mycert (authenticate user to VNC server) or -crl (supply a
    list of revoked certificates).  Prefix either one with "FORCE:" to make
    the setting unchangable.


 Notes:

    If "Use SSH" has been selected then SSL certs are disabled.

    See the x11vnc and STUNNEL documentation for how to create and use PEM
    certificate files:

        http://www.karlrunge.com/x11vnc/faq.html#faq-ssl-tunnel-ext
        http://www.karlrunge.com/x11vnc/ssl.html
        http://stunnel.mirt.net

    A common way to create and use a VNC Server certificate is:

        x11vnc -ssl SAVE ...

    and then copy the Server certificate to the local (viewer-side) machine.
    x11vnc prints out to the screen the Server certificate it generates
    (stored in ~/.vnc/certs/server.crt).  You can set "ServerCert" to it
    directly or use the "Import Certificate" action to save it to a file.
    Or use the "Fetch Cert" method to retrieve it (be sure to verify the
    MD5 fingerprint, etc).

    x11vnc also has command line utilities to create server, client, and CA
    (Certificate Authority) certificates and sign with it. See the above URLs.
}

	.ch.f.t insert end $msg
	jiggle_text .ch.f.t
}

proc help_ts_opts {} {
	toplev .oh

	scroll_text_dismiss .oh.f

	center_win .oh

	wm title .oh "Terminal Services VNC Options Help"

set msg {
 Options:  Click on a checkbox to enable a feature and bring up its Dialog.
 Deselecting a checkbox will disable the feature (but settings from the
 Dialog are remembered).  Click on it again to re-enable.


 Desktop Type:

    The default type of remote Desktop type is the "kde" (The K Desktop
    Environment) You can choose a different type: gnome, failsafe,
    twm, etc.

    This setting will ONLY be used if the desktop needs to be created.
    If an existing session of yours is found it will be used instead
    (log out of that session if you want to create a new Desktop type
    or see the Multiple Sessions option under Advanced).

 Desktop Size:

    The default size of remote Desktop type is the "1280x1024" with a
    Color depth of 16 bits per pixel (BPP).  Choose one of the standard
    WxH values or enter a custom one (TBD).

    This setting will ONLY be used if the desktop needs to be created.
    If an existing session of yours is found it will be used instead
    (log out of that session if you want to create a new Desktop size
    or see the Multiple Sessions option under Advanced).

    Some X servers, Xdummy or a real X server, will allow dynamic screen
    size changing after the session has started via a GUI configuration
    tool (or xrandr(1) from the command line).

 X Server Type:

    The default type of remote X session is the "Xvfb" (X virtual frame
    buffer) X server.  It is available on most systems.  To choose a
    different type, select "Xdummy", "Xvnc", "Xvnc.redirect".

    Xdummy is part of the x11vnc project and is a virtual X server with
    some nice features, but it Linux only and requires root permission
    to run.  One user put 'ALL ALL = NOPASSWD: /usr/local/bin/Xdummy*'
    in his sudo(1) configuration (via visudo).

    For Xvnc that server is started up, and x11vnc polls it in its
    normal way.  Use Xvnc.redirect if you want x11vnc to find and/or
    create the Xvnc session, but after that merely transfer packets back
    and forth between VNC viewer and Xvnc (I.e. x11vnc does no polling
    or VNC protocol).


 Enable Printing:

    This sets up a SSH port redirection for you from your remote session
    to your local print server.  The CUPS mechanism is used.  The local
    print server can also be SMB/Windows.

 Enable Sound:

    Not completely implemented yet.  A partially working ESD method
    is provided.  It may change over to http://nas.sourceforge.net in
    the future.  As with printing, it uses a SSH port redirection to a
    server running locally.

 File Transfer:

    x11vnc supports both the UltraVNC and TightVNC file transfer
    extensions.  On Windows both viewers support their file transfer
    protocol.  On Unix only the SSVNC VNC Viewer has filexfer support;
    it supports the UltraVNC flavor via a Java helper program.

    Choose the one you want based on VNC viewer you will use.
    The defaults for the SSVNC viewer package are TightVNC on Windows
    and UltraVNC on Unix.

 View Only:

    Start the VNC Viewer in View-Only mode (it may be switched to full
    access later in the session).

 Change VNC Viewer:

    If you do not like the VNC Viewer bundled in the package, you can
    indicate another one here.

 X11 viewer MacOSX:

    On MacOSX try to use the bundled X11 vncviewer instead of the
    Chicken of the VNC viewer; the Xquartz X server must be installed
    (it is by default on 10.5.x) and the DISPLAY variable must be set
    (see Tip 15 of SSVNC Help to do this manually.)


 Advanced Options:

 VNC Shared:

    Normal use of this program, 'tsvnc', *ALREADY* allows simultaneous
    shared access of the remote desktop:   You simply log in as many
    times from as many different locations with 'tsvnc' as you like.

    Select this option for the traditional VNC server shared mode of
    operation using a single x11vnc server.  SSH access is still required.

 Multiple Sessions:

    To enable one user to have more than one Terminal Services Desktop
    X session on a single machine, this option lets you create Tags for
    multiple ones (e.g. KDE_BIG, TWM_800x600)

 X Login Greeter:

    If you have root (sudo(1)) permission on the remote machine,
    you can have x11vnc try to connect to X displays that have nobody
    logged in yet.  This is most likely the login greeter running on
    the Physical console.  sudo(1) is used to run x11vnc with FD_XDM=1.

    An initial ssh running 'sudo id' is performed to try to 'prime'
    sudo so the 2nd one that starts x11vnc does not need a password.

    Note that if someone is already logged into the console of the XDM
    display you will see their X session.

 Other VNC Server:

    The x11vnc program running on the remote machine can be instructed to
    immediately redirect to some other (3rd party, e.g. Xvnc or vnc.so)
    VNC server.

 Use unixpw:

    This enables the x11vnc unixpw mode.  A Login: and Password: dialog
    will be presented in the VNC Viewer for the user to provide any Unix
    username and password whose session he wants to connect to.

    This mode is useful if a shared terminal services user (e.g. 'tsuser')
    is used for the SSH login part (say via the SSH authorized_keys
    mechanism and all users share the same private SSH key for 'tsuser').

    In normal usage the per-user SSH login should be the simplest and
    sufficient, in which case the unixpw option should NOT be selected.

 Client 8bit Color:

    Have the VNC Viewer request low color mode (8 bits per pixel) for
    slow links.  This may be disabled or further tuned (e.g. 64 color
    mode) in the viewer during the session.

 Client-Side Caching:

    x11vnc has an experiment Client-Side caching scheme "-ncache n"
    that can give nice speedups.  But there are some drawbacks
    because the cache-region is visible and uses much RAM.
    http://www.karlrunge.com/x11vnc/faq.html#faq-client-caching

 X11VNC Options:

    If you are familiar with x11vnc, you can specify any of its features
    that you would like enabled.

 SSVNC Mode:

    Clicking on this button will return you to the full SSVNC Mode.

 Unix ssvncviewer:

    Clicking on this button will popup a menu for setting options
    of the Unix (and Mac OS X) provided SSVNC vncviewer.


 ~/.ssvncrc file:

    You can put global options in your ~/.ssvncrc file (ssvnc_rc on
    Windows). Currently they are:

    Put "mode=tsvnc" or "mode=sshvnc" in the ~/.ssvncrc file to have
    the application start up in the given mode.

    desktop_type=wmaker  (e.g.) to switch the default Desktop Type.

    desktop_size=1280x1024  (e.g.) to switch the default Desktop Size.

    desktop_depth=24  (e.g.) to switch the default Desktop Color Depth.

    xserver_type=Xdummy  (e.g.) to switch the default X Server Type.

    (The above 4 settings apply only to the Terminal Services Mode.)

    noenc=1  (same as the -noenc option for a 'No Encryption' option)
    noenc=0  (do not show the 'No Encryption' option)

    font_default=tk-font-name     (sets the font for menus and buttons)
    font_fixed=tk-font-name       (sets the font for help text)
}
	.oh.f.t insert end $msg
	jiggle_text .oh.f.t
}

proc help_opts {} {
	toplev .oh

	scroll_text_dismiss .oh.f

	center_win .oh

	wm title .oh "SSL/SSH Viewer Options Help"

set msg {
  Use SSL:  The default, use SSL via STUNNEL (this requires SSL aware VNC
            server, e.g. x11vnc -ssl SAVE ...)  See the description in the
            main Help panel.

  Use SSH:  Instead of using STUNNEL SSL, use ssh(1) for the encrypted
            tunnel.  You must be able to log in via ssh to the remote host.

            On Unix the cmdline ssh(1) program (it must already be installed)
            will be run in an xterm for passphrase authentication, prompts
            about RSA keys, etc.  On Windows the cmdline plink.exe program
            will be launched in a Windows Console window. (Apologies for
            the klunkiness..)

            You can set the "VNC Host:Display" to "user@host:disp" to
            indicate ssh should log in as "user" on "host".  NOTE: On
            Windows you *MUST* always supply the "user@" part (due to a
            plink deficiency). E.g.:

                VNC Host:Display:    fred@far-away.east:0


            Gateway:  If an intermediate gateway machine must be used
            (e.g. to enter a firewall; the VNC Server is not running on it),
            put it in the Proxy/Gateway entry, e.g.:

                VNC Host:Display:    workstation:0
                Proxy/Gateway:       user@gateway-host:port
  
            ssh is used to login to user@gateway-host and then a -L port
            redirection is set up to go to workstation:0 from gateway-host.
            ":port" is optional, use it if the gateway-host SSH port is
            not the default value 22.

            Chaining 2 ssh's:  One can also do a "double ssh", i.e. a
            first SSH to the gateway login machine then a 2nd ssh to the
            destination machine (presumably it is running the vnc server).

            Unlike the above example, the "last leg" (gateway-host ->
            workstation) is also encrypted by SSH this way.  Do this by
            splitting the gateway in two with a comma, the part before it
            is the first SSH:

                VNC Host:Display: localhost:0
                Proxy/Gateway:    user@gateway-host:port,user@workstation:port

            Web and SOCKS proxies can also be used with SSH:

                VNC Host:Display: user@workstation:0
                Proxy/Gateway:    socks://socks.server:1080

            See the "SSH Proxies/Gateways" in the Main Help document for full
            details.


            Remote Command:  In the "Remote SSH Command" entry you can to
            indicate that a remote command to be run.  The default is
            "sleep 15" to make sure port redirections get established. But you
            can run anything else, for example, to run x11vnc on your X :0
            workstation display:

                x11vnc -display :0 -nopw


            Windows SSH SERVER: if you are ssh'ing INTO Windows (e.g. CYGWIN
            SSHD server) there may be no "sleep" command so put in something
            like "ping localhost" or "ping -n 10 -w 1000 localhost" to 
            set a short delay to let the port redir get established.


            Trick:  If you use "SHELL" asl the "Remote SSH Command" then
            you get an SSH shell only: no VNC viewer will be launched.
            On Windows "PUTTY" will try to use putty.exe (better terminal
            emulation than plink.exe)  A shortcut for this is Ctrl-S as
            long as user@hostname is present in the "VNC Host:Display" box.


  Use SSH + SSL:

            Tunnel the SSL connection through a SSH tunnel.  Use this
            if you want end-to-end SSL and must use a SSH gateway (e.g. to
            enter a firewall) or if additional SSH port redirs are required
            (CUPS, Sound, SMB tunnelling: See Advanced Options).

            This is a RARELY used mode, but included in case the need arises.


  No Encryption:

            In '-noenc' mode, which is now the default, (Ctrl-E also toggles
            this mode), use this to make a Direct connection to the VNC Server
            with no encryption whatsoever.  (Be careful about passwords, etc.)

            The -noenc mode is now the default since SSVNC 1.0.25, use
            the '-enc' cmdline option to disable the button.


  Automatically Find X Session:

            When using SSH mode to connect, you can select this option.  It
            simply sets the Remote SSH Command to:

                 PORT= x11vnc -find -localhost

            This requires that x11vnc is installed on the remote computer
            and is available in $PATH for the ssh login.  The command
            "x11vnc -find -localhost" command is run on the remote
            machine.

            The -find option causes x11vnc to try to find an existing X
            session owned by the user (i.e. who you ssh in as).  If it
            does it attaches to it; otherwise the x11vnc VNC server exits
            immediately followed by your VNC Viewer.

            The PORT= option just means to let x11vnc pick its own
            VNC port and then connect to whatever it picked.  Use P=
            for more debugging output.

            The idea for this mode is you simply type 'username@workstation'
            in the VNC Host:Display box, Select 'Options -> Automatically
            Find X Session', and then click Connect.  The tsvnc mode is
            similar (it runs x11vnc on the remote side with the intent
            of automatically finding, or creating, your desktop).


  Unix Username & Password:

            This is only available on Unix and MacOSX and when using
            the SSVNC enhanced TightVNC viewer (it has been modified to
            do Unix logins).  It supports a login dialog with servers
            doing something like x11vnc's "-unixpw" mode.  After any
            regular VNC authentication takes place (VNC Password), then
            it sends the Unix Username, a Return, the Unix Password and
            a final Return.  This saves you from typing them into the
            "login:" and "Password:" prompts in the viewer window.

            Note that the x11vnc -unixpw login mode is external to the
            VNC protocol, so you need to be sure the VNC server is in
            this mode and will be waiting for the dialog.  Otherwise the
            username and password will be typed directly into the desktop
            application that happens to have the focus!

            When you select this option "Unix Username:" and "Unix
            Password:" entry boxes appear on the main panel where you can
            type them in.  x11vnc has settings that can be specified after
            a ":" in the Unix username; they may be used here as well.
            (For example: username:3/4,nc for a smaller screen and -nocache)

            If the Unix Username is not set when you click Connect, then
            any SSH username@host is used.  Otherwise the environment
            variable $USER or $LOGNAME and finally whoami(1) is used.

            Also Note that the Unix Password is never saved in a VNC
            profile (so you have to type it each time).  Also, the remote
            x11vnc server is instructed to not echo the Username string
            by sending an initial Escape.  Set the SSVNC_UNIXPW_NOESC=1
            environment variable to override this.

  Reverse VNC Connection:

            Reverse (listening) VNC connections are possible as well.
            Enable with this button "Reverse VNC Connection (-LISTEN)"

            In this case the VNC Server initiates the connection to your
            waiting (i.e. listening) SSVNC viewer.

            For SSL connections in the 'VNC Host:Display' entry box put in
            the number (e.g. "0" or ":0" or ":1", etc.) that corresponds to
            the Listening display (0 -> port 5500, 1 -> port 5501, etc.) you
            want to use.  For example x11vnc can then be used via:
            "x11vnc ... -ssl SAVE -connect hostname:port" using the "port"
            with the one you chose.

            Clicking on the 'Listen' button puts your SSVNC viewer
            in a "listening" state on that port number, waiting for a
            connection from the VNC Server.

            Then a VNC server should establish a reverse connection to
            that port on this machine (e.g. -connect this-machine:5500
            or -connect this-machine:5503, etc.)

            Server SSL certificates will be verified, however you WILL
            NOT be prompted about unrecognized ones; rather, you MUST
            set up the correct Server certificate (e.g. by importing).
            prior to any connections.

            If the connection is failing in Reverse VNC (listening) mode,
            check the STUNNEL log output to see if STUNNEL is unable to
            authenticate the VNC Server.  If you want to allow in a
            reverse connection with NO Server authentication, unset the
            'Verify All Certs' option. 

            When listening in SSL, you will ALSO need to specify YOUR
            OWN SSL cert, "MyCert", or otherwise let the GUI prompt you
            to create a "listen.pem" and use that.

            The "listen.pem" will be reused in later SSL Listening
            connections unless you specify a different one with MyCert.

            On Windows or using a 3rd party VNC Viewer multiple,
            simultaneous reverse connections are always enabled.
            On Unix/MacOSX with the provided ssvncviewer they are disabled
            by default.  To enable them:
            Options -> Advanced -> Unix ssvncviewer -> Multiple LISTEN Conns.

            For reverse connections in SSH or SSH + SSL modes it is a
            little trickier.  The SSH tunnel (with -R tunnel) must be
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

            Specify a specific interface, e.g. 192.168.1.1:0 to have stunnel
            only listen on that interface.  IPv6 works too, e.g. :::0 or ::1:0
            Also works for UN-encrypted reverse connections as well ('None').

            Note that for SSL connections use of "Proxy/Gateway" does not
            make sense: the remote side cannot initiate its reverse connection
            via the Proxy.

            Note that for SSH or SSH+SSL connections use of "Proxy/Gateway"
            does not make sense (the ssh cannot do a -R on a remote host:port),
            unless it is a double proxy where the 2nd host is the machine with
            the VNC server.


  View Only:               Have VNC Viewer ignore mouse and keyboard input.
  
  Fullscreen:              Start the VNC Viewer in fullscreen mode.
  
  Raise On Beep:           Deiconify viewer when bell rings.
  
  Use 8bit color:          Request a very low-color pixel format.
  
  Do not use JPEG:         Do not use the jpeg aspect of the tight encoding.

  Use X11 vncviewer on MacOSX:
                           On MacOSX try to use the bundled X11 vncviewer
                           instead of the Chicken of the VNC viewer;
                           The Xquartz X server must be installed (it is by
                           default on 10.5.x) and the DISPLAY variable must
                           be set (see Tip 15 of Help to do this manually.)
                           Put cotvnc=1 in ~/.ssvncrc to switch the default.

  Kill Stunnel Automatically:
                           On Windows, automatically try to kill the STUNNEL
                           process when the VNC Viewer exits.  This is a
                           global setting (not per-profile); it can be also
                           set via either the -killstunnel cmdline option,
                           or killstunnel=1 in ssvnc_rc.  To disable it supply
                           -nokillstunnel or put killstunnel=0 in ssvnc_rc.
                           As of 1/2009 this option is on by default.

                           The main drawback to having STUNNEL automatically
                           killed is that you will not be able to view its
                           logfile.  If you are having trouble connecting via
                           SSL, disable this option and double click on the
                           dark green STUNNEL icon in the tray to view the log.


  Compress Level/Quality:  Set TightVNC encoding parameters.


  Putty PW:  On Windows only: use the supplied password for plink SSH
             logins.  Unlike the other options the value is not saved
             when 'Save' is performed.  This feature is useful when
             options under "Advanced" are set that require TWO SSH's:
             you just have to type the password once in this entry box.
             The bundled pageant.exe and puttygen.exe programs can also
             be used to avoid repeatedly entering passwords (note this
             requires setting up and distributing SSH keys).  Start up
             pageant.exe or puttygen.exe and read the instructions there.

             Note, that there is a small exposure to someone seeing the
             putty password on the plink command line.

             Note that the Putty PW is not cleared if you load in a
             new VNC profile.


  Port Slot: On Windows ports cannot be selected or checked as easily as
             on Unix.  So listening ports for ssh redirs, proxy tunnelling,
             and etc. things are picked via finding a free "slot".
             The slots run from 30 to 99 and are locked based on the
             existence of a file with the slot number in it.  When the
             connection is about to be made, a free slot is found and used
             to work out some ports (e.g. 5930 for the local VNC port,
             etc.)  This way simultaneous SSVNC connections can take place.

             One drawback of this is that Putty/Plink stores SSH keys based
             on hostname:port, and with a proxy tunnel the hostname is
             "localhost".  So the Putty key store may have key collisions
             for the localhost tunnels, and plink will prompt you to
             resolve the conflict WRT a different SSH key being discovered.

             To work around this to some degree you can select a unique
             Port Slot (in the range 50-99) for a specific host.  Then the
             ssh redir port to this host will never change and so the
             Putty localhost:fixed-port key should remain valid.


  Mode:      To change the GUI Mode, select between the full SSVNC
             (i.e. SSL and SSH), SSHVNC (i.e. SSH-Only), and Terminal
             Services mode (TSVNC; uses x11vnc)

             Note: You can put "mode=tsvnc" or "mode=sshvnc" in your
             ~/.ssvncrc file (ssvnc_rc on Windows) to have the application
             start up in the given mode.


  Show 'No Encryption' Option:

             Note: since SSVNC 1.0.25 the 'No Encryption' Option is
             enabled by default.

             Select this to display a button that disables both SSL and
             SSH encryption.  This is the same as Ctrl+E.  This puts
             a check item "None" on the main panel and also a "No
             Encryption" check item in the "Options" panel.  If you
             select this item, there will be NO encryption for the VNC
             connection (use cautiously) See Tip 5) under Help for more
             information about disabling encryption.


  Buttons:
                
  Use Defaults:    Set all options to their defaults (i.e. unset).

  Delete Profile:  Delete a saved profile.

  Advanced:        Bring up the Advanced Options dialog.

  Save and Load:

             You can Save the current settings by clicking on Save
             (.vnc file) and you can also read in a saved one with Load
             Profile.  Use the Browse... button to select the filename
             via the GUI.

             Pressing Ctrl-L or Clicking the Right mouse button on the
             main GUI will invoke the Load dialog.

             Note: On Windows since the TightVNC Viewer will save its own
             settings in the Registry, some unexpected behavior is possible
             because the viewer is nearly always directed to the VNC host
             "localhost:30".  E.g. if you specify "View Only" in this gui
             once but not next time the Windows VNC Viewer may remember
             the setting.  Unfortunately there is not a /noreg option for
             the Viewer.
}
	.oh.f.t insert end $msg
	jiggle_text .oh.f.t
}

proc help_fetch_cert {{selfsigned 1}} {
	toplev .fh

	set h 35
	if [small_height] {
		set h 28
	}
	scroll_text_dismiss .fh.f 85 $h

	center_win .fh
	wm resizable .fh 1 0

	wm title .fh "Fetch Certificates Help"

	set msg {
  The displayed SSL Certificate has been retrieved from the VNC Server via the
  "Fetch Cert" action.
  
  It has merely been downloaded via the SSL Protocol:

         *** IT HAS NOT BEEN VERIFIED OR AUTHENTICATED IN ANY WAY ***
  
  So, in principle, it could be a fake certificate being inserted by a bad
  person attempting to perform a Man-In-The-Middle attack on your SSL connection.
  
  If, however, by some external means you can verify the authenticity of this SSL
  Certificate you can use it for your VNC SSL connection to the VNC server you
  wish to connect to.  It will provide an authenticated and encrypted connection.
  
  You can verify the SSL Certificate by comparing the MD5 or SHA1 hash value
  via a method/channel you know is safe (i.e. not also under control of a
  Man-In-The-Middle attacker).  You could also check the text between the
  -----BEGIN CERTIFICATE----- and -----END CERTIFICATE----- tags, etc.
  
  Once you are sure it is correct, you can press the Save button to save the
  certificate to a file on the local machine for use when you connect via VNC
  tunneled through SSL.  If you save it, then that file will be set as the
  Certificate to verify the VNC server against.  You can see this in the dialog
  started via the "Certs..." button on the main panel.
  
  NOTE: If you want to make Permanent the association of the saved SSL certificate
  file with the VNC server host, you MUST save the setting as a profile for
  loading later. To Save a Profile, click on Options -> Save Profile ...,
  and choose a name for the profile and then click on Save.

  If "Verify All Certs" is checked, then you are forced to check all new certs.
  In this case the certs are saved in the 'Accepted Certs' directory against
  which all servers will be checked unless "ServerCert" or "CertsDir" has been
  set to something else.

  To reload the profile at a later time, click on the "Load" button on the
  main panel and then select the name and click "Open".  If you want to be
  sure the certificate is still associated with the loaded in host, click on
  "Certs..." button and make sure the "ServerCert" points to the desired SSL
  filename.

  See the Certs... Help for more information.  A sophisticated method can be set
  up using a Certificate Authority key to verify never before seen certificates
  (i.e. like your web browser does).
}

	set msg2 {
  --------------------------------------------------------------------------
  NOTE: The certificate that was just downloaded IS NOT a Self-Signed
  certificate.  It was signed by a Certificate Authority (CA) instead.
  So saving it does not make sense because it cannot be used to authenticate
  anything.

  You need to Obtain and Save the CA's certificate instead.

  The remainder of this Help description applies ONLY to Self-Signed
  certificates (i.e. NOT the most recently downloaded one.)
  --------------------------------------------------------------------------


}

	if {!$selfsigned} {
		regsub {  If, however,} $msg "$msg2  If, however," msg
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

	eval text .w.t -width 72 -height 21 $help_font
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

    To have STUNNEL automatically killed when the Viewer exits use the
    -killstunnel cmdline option, or set it under Options or in ssvnc_rc.
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
	set maxx 60
	if [regexp {^INFO: without Certificate} $str] {
		set maxx 72
	}
	if {[string length $str] > $maxx} {
		set lend [expr $maxx - 1]
		set str [string range $str 0 $lend]
		append str " ..."
	}
	.l configure -text $str
	update
	global env
	if [info exists env(SSVNC_MESG_DELAY)] {
		after $env(SSVNC_MESG_DELAY)
	}
}

proc get_ssh_hp {str} {
	regsub {cmd=.*$} $str "" str
	set str [string trim $str]
	regsub {[ 	].*$} $str "" str
	return $str
}

proc get_ssh_cmd {str} {
	set str [string trim $str]
	global ts_only
	if {$ts_only} {
		return [ts_x11vnc_cmd]	
	}
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

proc ts_x11vnc_cmd {} {
	global is_windows
	global ts_xserver_type choose_xserver ts_desktop_type choose_desktop ts_unixpw ts_vncshared
	global ts_desktop_size ts_desktop_depth choose_desktop_geom
	global choose_filexfer ts_filexfer
	global ts_x11vnc_opts  ts_x11vnc_path ts_x11vnc_autoport choose_x11vnc_opts
	global ts_othervnc choose_othervnc ts_xlogin
	global choose_sleep extra_sleep

	set cmd ""
	if {$choose_x11vnc_opts && $ts_x11vnc_path != ""} {
		set cmd $ts_x11vnc_path
	} else {
		set cmd "x11vnc"
	}
	if {! $is_windows} {
		set cmd "PORT= $cmd"
	} else {
		set cmd "PORT= $cmd"
	}

	set type $ts_xserver_type;
	if {! $choose_xserver} {
		set type ""
	}
	if {$choose_othervnc && $ts_othervnc == "find"} {
		set type "Xvnc.redirect"
	}

	if [info exists choose_sleep] {
		if {! $choose_sleep} {
			set extra_sleep ""
		}
	}

	if {$choose_othervnc && $ts_othervnc != "find"} {
		set cmd "$cmd -redirect $ts_othervnc"
	} elseif {$type == ""} {
		global ts_xserver_type_def
		if {$ts_xserver_type_def != ""} {
			set cmd "$cmd -display WAIT:cmd=FINDCREATEDISPLAY-$ts_xserver_type_def";
		} else {
			set cmd "$cmd -display WAIT:cmd=FINDCREATEDISPLAY-Xvfb";
		}
	} elseif {$type == "Xvfb"} {
		set cmd "$cmd -display WAIT:cmd=FINDCREATEDISPLAY-Xvfb";
	} elseif {$type == "Xdummy"} {
		set cmd "$cmd -display WAIT:cmd=FINDCREATEDISPLAY-Xdummy";
	} elseif {$type == "Xvnc"} {
		set cmd "$cmd -display WAIT:cmd=FINDCREATEDISPLAY-Xvnc";
	} elseif {$type == "Xvnc.redirect"} {
		set cmd "$cmd -display WAIT:cmd=FINDCREATEDISPLAY-Xvnc.redirect";
	}

	# TBD: Cups + sound

	set cmd "$cmd -localhost";
	set cmd "$cmd -nopw";
	global ts_ncache choose_ncache
	if {$choose_ncache && [regexp {^[0-9][0-9]*$} $ts_ncache]} {
		set cmd "$cmd -ncache $ts_ncache";
	} else {
		#set cmd "$cmd -nonc";
	}
	set cmd "$cmd -timeout 120";
	global ts_multisession choose_multisession
	regsub -all {[^A-z0-9_-]} $ts_multisession "" ts_multisession
	if {$choose_multisession && $ts_multisession != ""} {
		set cmd "$cmd -env FD_TAG='$ts_multisession'";
	}
	if {$choose_filexfer && $ts_filexfer != ""} {
		if {$ts_filexfer == "tight"} {
			set cmd "$cmd -tightfilexfer";
		} else {
			set cmd "$cmd -ultrafilexfer";
		}
	}
	if {$ts_unixpw} {
		set cmd "$cmd -unixpw";
	}
	if {$ts_vncshared} {
		set cmd "$cmd -shared";
	}
	set u "unknown"
	global env
	if {[info exists env(USER)]} {
		regsub -all {[^A-z]} $env(USER) "_" u
	}
	set cmd "$cmd -o \$HOME/.tsvnc.log.$u";	# XXX perms

	set sess "kde"
	global ts_desktop_type_def
	if {$ts_desktop_type_def != ""} {
		set sess $ts_desktop_type_def
	}
	if {$choose_desktop && $ts_desktop_type != ""} {
		set sess $ts_desktop_type
	}
	set cmd "$cmd -env FD_SESS=$sess";

	if {$choose_desktop_geom} {
		set geom "1280x1024"
		set dep 16
		global ts_desktop_size_def ts_desktop_depth_def
		if {$ts_desktop_size_def != ""} {
			set geom $ts_desktop_size_def
		}
		if {$ts_desktop_depth_def != ""} {
			set dep $ts_desktop_depth_def
		}
		if {$ts_desktop_size != ""} {
			if [regexp {^[0-9][0-9]*x[0-9][0-9]*$} $ts_desktop_size] {
				set geom $ts_desktop_size
			}
			if {$ts_desktop_depth != ""} {
				set geom "${geom}x$ts_desktop_depth"
			} else {
				set geom "${geom}x$dep"
			}
		} else {
			set geom "${geom}x$dep"
		}
		set cmd "$cmd -env FD_GEOM=$geom";
	}
	if {$is_windows} {
		;
	} elseif {$choose_x11vnc_opts && $ts_x11vnc_autoport != "" && [regexp {^[0-9][0-9]*$} $ts_x11vnc_autoport]} {
		set cmd "$cmd -autoport $ts_x11vnc_autoport";
	} else {
		set cmd "$cmd -env AUTO_PORT=5950";
	}
	if {$choose_x11vnc_opts && $ts_x11vnc_opts != ""} {
		set cmd "$cmd $ts_x11vnc_opts";
	}
	if {$ts_xlogin} {
		regsub {PORT= } $cmd "PORT= sudo " cmd
		regsub {P= } $cmd "P= sudo " cmd
		regsub { -o [^ ][^ ]*} $cmd "" cmd
		
		set cmd "$cmd -env FD_XDM=1";
	}

	return $cmd
}

proc set_defaults {} {
	global defs env

	global mycert svcert crtdir crlfil
	global use_alpha use_turbovnc disable_pipeline use_grab use_ssl use_ssh use_sshssl use_viewonly use_fullscreen use_bgr233
	global use_send_clipboard use_send_always
	global disable_all_encryption
	global use_nojpeg use_raise_on_beep use_compresslevel use_quality use_x11_macosx
	global compresslevel_text quality_text
	global use_cups use_sound use_smbmnt
	global cups_local_server cups_remote_port cups_manage_rcfile ts_cups_manage_rcfile cups_x11vnc
	global cups_local_smb_server cups_remote_smb_port
	global change_vncviewer change_vncviewer_path vncviewer_realvnc4
	global choose_xserver ts_xserver_type choose_desktop ts_desktop_type ts_unixpw ts_vncshared
	global choose_filexfer ts_filexfer
	global ts_x11vnc_opts choose_x11vnc_opts ts_x11vnc_path ts_x11vnc_autoport ts_xlogin
	global ts_othervnc choose_othervnc choose_sleep
	global choose_ncache ts_ncache choose_multisession ts_multisession
	global ts_mode ts_desktop_size ts_desktop_depth choose_desktop_geom
	global additional_port_redirs additional_port_redirs_list
	global stunnel_local_protection stunnel_local_protection_type ssh_local_protection multiple_listen listen_once listen_accept_popup listen_accept_popup_sc
	global ssh_known_hosts ssh_known_hosts_filename
	global ultra_dsm ultra_dsm_type ultra_dsm_file ultra_dsm_noultra ultra_dsm_salt
	global sound_daemon_remote_cmd sound_daemon_remote_port sound_daemon_kill sound_daemon_restart
	global sound_daemon_local_cmd sound_daemon_local_port sound_daemon_local_kill sound_daemon_x11vnc sound_daemon_local_start 
	global smb_su_mode smb_mount_list
	global use_port_knocking port_knocking_list port_slot putty_args
	global ycrop_string ssvnc_scale ssvnc_escape sbwid_string rfbversion ssvnc_encodings ssvnc_extra_opts use_x11cursor use_nobell use_rawlocal use_notty use_popupfix extra_sleep use_listen use_unixpw use_x11vnc_find unixpw_username
	global disable_ssl_workarounds disable_ssl_workarounds_type
	global no_probe_vencrypt server_vencrypt server_anondh
	global include_list
	global svcert_default mycert_default crlfil_default


	set defs(use_viewonly) 0
	set defs(use_listen) 0
	set defs(disable_ssl_workarounds) 0
	set defs(disable_ssl_workarounds_type) "none"
	set defs(use_unixpw) 0
	set defs(unixpw_username) ""
	set defs(use_x11vnc_find) 0
	set defs(use_fullscreen) 0
	set defs(use_raise_on_beep) 0
	set defs(use_bgr233) 0
	set defs(use_alpha) 0
	set defs(use_send_clipboard) 0
	set defs(use_send_always) 0
	set defs(use_turbovnc) 0
	set defs(disable_pipeline) 0
	set defs(no_probe_vencrypt) 0
	set defs(server_vencrypt) 0
	set defs(server_anondh) 0
	set defs(use_grab) 0
	set defs(use_nojpeg) 0
	set defs(use_x11_macosx) 1
	if [info exists env(SSVNC_COTVNC)] {
		if {$env(SSVNC_COTVNC) != 0} {
			set defs(use_x11_macosx) 0
		}
	} elseif {![info exists env(DISPLAY)]} {
		set defs(use_x11_macosx) 0
	}
	set defs(use_compresslevel) "default"
	set defs(use_quality) "default"
	set defs(compresslevel_text) "Compress Level: default"
	set defs(quality_text) "Quality: default"

	set defs(mycert) $mycert_default
	set defs(svcert) $svcert_default
	set defs(crtdir) "ACCEPTED_CERTS"
	set defs(crlfil) $crlfil_default

	set defs(use_cups) 0
	set defs(use_sound) 0
	set defs(use_smbmnt) 0

	set defs(choose_xserver) 0 
	set defs(ts_xserver_type) "" 
	set defs(choose_desktop) 0 
	set defs(ts_desktop_type) "" 
	set defs(ts_desktop_size) "" 
	set defs(ts_desktop_depth) "" 
	set defs(choose_desktop_geom) 0
	set defs(ts_unixpw) 0 
	set defs(ts_vncshared) 0 
	set defs(ts_ncache) 8
	set defs(choose_ncache) 0
	set defs(ts_multisession) "" 
	set defs(choose_multisession) 0
	set defs(ts_filexfer) "" 
	set defs(choose_filexfer) 0
	set defs(choose_x11vnc_opts) 0
	set defs(ts_x11vnc_opts) "" 
	set defs(ts_x11vnc_path) "" 
	set defs(ts_x11vnc_autoport) "" 
	set defs(ts_othervnc) "" 
	set defs(choose_othervnc) 0
	set defs(ts_xlogin) 0
	set defs(ts_mode) 0

	set defs(change_vncviewer) 0 
	set defs(change_vncviewer_path) "" 
	set defs(cups_manage_rcfile) 1
	set defs(ts_cups_manage_rcfile) 0
	set defs(cups_x11vnc) 0 
	set defs(vncviewer_realvnc4) 0

	set defs(additional_port_redirs) 0
	set defs(additional_port_redirs_list) ""

	set defs(stunnel_local_protection) 1
	set defs(stunnel_local_protection_type) "exec"
	set defs(ssh_local_protection) 1
	set defs(ssh_known_hosts) 0
	set defs(ssh_known_hosts_filename) ""
	set defs(multiple_listen) 0
	set defs(listen_once) 0
	set defs(listen_accept_popup) 0
	set defs(listen_accept_popup_sc) 0

	set defs(ultra_dsm) 0
	set defs(ultra_dsm_file) ""
	set defs(ultra_dsm_type) "guess"
	set defs(ultra_dsm_noultra) 0
	set defs(ultra_dsm_salt) ""

	set defs(port_slot) ""
	set defs(putty_args) ""

	set defs(cups_local_server) ""
	set defs(cups_remote_port) ""
	set defs(cups_local_smb_server) ""
	set defs(cups_remote_smb_port) ""

	set defs(smb_su_mode) "sudo"
	set defs(smb_mount_list) ""

	set defs(sound_daemon_remote_cmd) ""
	set defs(sound_daemon_remote_port) ""
	set defs(sound_daemon_kill) 0
	set defs(sound_daemon_restart) 0

	set defs(sound_daemon_local_cmd) ""
	set defs(sound_daemon_local_port) ""
	set defs(sound_daemon_local_start) 0
	set defs(sound_daemon_local_kill) 0
	set defs(sound_daemon_x11vnc) 0

	set defs(ycrop_string) ""
	set defs(ssvnc_scale) ""
	set defs(ssvnc_escape) ""
	set defs(sbwid_string) ""
	set defs(rfbversion) ""
	set defs(ssvnc_encodings) ""
	set defs(ssvnc_extra_opts) ""
	set defs(use_x11cursor) 0
	set defs(use_nobell) 0
	set defs(use_rawlocal) 0
	set defs(use_notty) 0
	set defs(use_popupfix) 0
	set defs(extra_sleep) ""
	set defs(use_port_knocking) 0
	set defs(port_knocking_list) ""

	set defs(include_list) ""

	set dir [get_profiles_dir]
	set deffile ""
	if [file exists "$dir/defaults"] {
		set deffile "$dir/defaults"
	} elseif [file exists "$dir/defaults.vnc"] {
		set deffile "$dir/defaults.vnc"
	}
	if {$deffile != ""} {
		set fh ""
		catch {set fh [open $deffile "r"]}
		if {$fh != ""} {
			while {[gets $fh line] > -1} {
				set line [string trim $line]
				if [regexp {^#} $line] {
					continue
				}
				if [regexp {^([^=]*)=(.*)$} $line m var val] {
					if {$var == "disp"} {
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
						set defs($var) $val
					}
				}
			}
			close $fh
		}
	}

	global ssh_only ts_only
	if {$ssh_only || $ts_only} {
		set defs(use_ssl) 0
		set defs(use_ssh) 1
		set defs(use_sshssl) 0
	} else {
		set defs(use_ssl) 1
		set defs(use_ssh) 0
		set defs(use_sshssl) 0
	}
	set defs(disable_all_encryption) 0

	foreach var [array names defs] {
		set $var $defs($var)	
	}

	global vncauth_passwd unixpw_passwd
	set vncauth_passwd ""
	set unixpw_passwd ""

	if {$ssh_only || $ts_only} {
		ssl_ssh_adjust ssh
	} else {
		ssl_ssh_adjust ssl
	}
	listen_adjust
	unixpw_adjust

	global last_load
	set last_load ""
}

proc windows_listening_message {n} {
	global did_listening_message

	global extra_cmd
	set extra_cmd ""
	set cmd [get_cmd $n]
	
	if {$did_listening_message < 2} {
		incr did_listening_message
		global listening_name

		set ln $listening_name
		if {$ln == ""} {
			set ln "this-computer:$n"
		}

		set msg "
   About to start the Listening VNC Viewer (Reverse Connection).

   The VNC Viewer command to be run is:

       $cmd

   After the Viewer starts listening, the VNC server should
   then Reverse connect to:

       $ln

   When the VNC Connection has ended **YOU MUST MANUALLY STOP**
   the Listening VNC Viewer.

   To stop the Listening Viewer: right click on the VNC Icon in
   the tray and select 'Close listening daemon' (or similar).

   ONLY AFTER THAT will you return to the SSVNC GUI.

   Click OK now to start the Listening VNC Viewer.$extra_cmd
"
		global use_ssh use_sshssl
		if {$use_ssh || $use_sshssl} {
			set msg "${msg}   NOTE: You will probably also need to kill the SSH in the\n   terminal via Ctrl-C" 
		}

		global help_font is_windows system_button_face
		toplev .wll
		global wll_done

		set wll_done 0

		eval text .wll.t -width 64 -height 22 $help_font
		button .wll.d -text "OK" -command {destroy .wll; set wll_done 1}
		pack .wll.t .wll.d -side top -fill x

		apply_bg .wll.t

		center_win .wll
		wm resizable .wll 1 0

		wm title .wll "SSL/SSH Viewer: Listening VNC Info"

		.wll.t insert end $msg

		vwait wll_done
	}
}

proc get_cmd {n} {
	global use_alpha use_grab use_x11cursor use_nobell use_ssh
	global use_sshssl use_viewonly use_fullscreen use_bgr233
	global use_nojpeg use_raise_on_beep use_compresslevel use_quality
	global use_send_clipboard use_send_always change_vncviewer
	global change_vncviewer_path vncviewer_realvnc4 use_listen
	global disable_ssl_workarounds disable_ssl_workarounds_type env

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

	global extra_cmd
	set extra_cmd ""
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
		global direct_connect_reverse_host_orig is_win9x
		if {![info exists direct_connect_reverse_host_orig]} {
			set direct_connect_reverse_host_orig ""
		}
		if {$direct_connect_reverse_host_orig != "" && !$is_win9x} {
			set nn2 [expr $nn + 15]
			set h0 $direct_connect_reverse_host_orig
			global win_localhost
			set extra_cmd "\n\nrelay6.exe $nn $win_localhost $nn2 /b:$h0"
			set nn $nn2
		}

		append cmd " $nn"

	} else {
		if [regexp {^[0-9][0-9]*$} $n] {
			global win_localhost
			append cmd " $win_localhost:$n"
		} else {
			append cmd " $n"
		}
	}
	return $cmd
}

proc do_viewer_windows {n} {
	global use_listen env

	set cmd [get_cmd $n]

	set ipv6_pid2 ""
	if {$use_listen} {
		set nn $n
		if {$nn < 100} {
			set nn [expr "$nn + 5500"] 
		}
		global direct_connect_reverse_host_orig is_win9x
		if {![info exists direct_connect_reverse_host_orig]} {
			set direct_connect_reverse_host_orig ""
		}
		if {$direct_connect_reverse_host_orig != "" && !$is_win9x} {
			set nn2 [expr $nn + 15]
			set h0 $direct_connect_reverse_host_orig
			global win_localhost
			set ipv6_pid2 [exec relay6.exe $nn $win_localhost $nn2 /b:$h0 &]
			set nn $nn2
		}
	}

	if [info exists env(SSVNC_EXTRA_SLEEP)] {
		set t $env(SSVNC_EXTRA_SLEEP)
		mesg "sleeping an extra $t seconds..."
		set t [expr "$t * 1000"]
		after $t
	}
	global extra_sleep
	if {$extra_sleep != ""} {
		set t $extra_sleep
		mesg "sleeping an extra $t seconds..."
		set t [expr "$t * 1000"]
		after $t
	}
	
	mesg $cmd
	set emess ""
	set rc [catch {eval exec $cmd} emess]

	if {$ipv6_pid2 != ""} {
		winkill $ipv6_pid2
	}

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

proc read_file {file} {
	set str ""
	if [file exists $file] {
		set fh ""
		catch {set fh [open $file "r"]}
		if {$fh != ""} {
			while {[gets $fh line] > -1} {
				append str "$line\n"
			}
			close $fh
		}
	}
	return $str
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
		#puts $s "GET / HTTP/1.1"
		puts $s "GET /automation/n09230945.asp HTTP/1.1"
		puts $s "Host: www.whatismyip.com"
		puts $s "Connection: close"
		puts $s ""
		flush $s
		set on 0
		while { [gets $s line] > -1 } {
			if {! $on && [regexp {<HEAD>}  $line]} {set on 1}
			if {! $on && [regexp {<HTML>}  $line]} {set on 1}
			if {! $on && [regexp {<TITLE>} $line]} {set on 1}
			if {! $on && [regexp {^[0-9][0-9]*\.[0-9]} $line]} {set on 1}
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

proc check_for_ipv6 {} {
	global is_windows have_ipv6
	if {$have_ipv6 != ""} {
		return
	}
	if {! $is_windows} {
		set out ""
		catch {set out [exec netstat -an]}
		if [regexp {tcp6} $out] {
			set have_ipv6 1
		} elseif [regexp {udp6} $out] {
			set have_ipv6 1
		} elseif [regexp {:::} $out] {
			set have_ipv6 1
		} elseif [regexp {::1} $out] {
			set have_ipv6 1
		} elseif [regexp {TCP: IPv6.*LISTEN} $out] {
			set have_ipv6 1
		} else {
			set have_ipv6 0
		}
	} else {
		set out [get_ipconfig]
		set out [string trim $out]
		if {$out == ""} {
			catch {set out [exec ping6 -n 1 -w 2000 ::1]}
			if [regexp {Reply from.*bytes} $out] {
				if [regexp {Received = 1} $out] {
					set have_ipv6 1
					return
				}
			}
			set have_ipv6 0
			return
		}
		foreach line [split $out "\n\r"] {
			if {[regexp -nocase {IP Address.*:[ \t]*[a-f0-9]*:[a-f0-9]*:} $line]} {
				set have_ipv6 1
				return
			}
		}
		set have_ipv6 0
	}
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
		foreach line [split $out "\n\r"] {
			if {[regexp -nocase {IP Address.*:[ \t]*([:a-f0-9][%:a-f0-9]*)} $line mvar ip]} {
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

proc bat_sleep {fh} {
	global env
	if [info exists env(SSVNC_BAT_SLEEP)] {
		puts $fh "@echo ."
		puts $fh "@echo -----"
		puts $fh "@echo Debug: BAT SLEEP for $env(SSVNC_BAT_SLEEP) seconds ..."
		puts $fh "@ping -n $env(SSVNC_BAT_SLEEP) -w 1000 0.0.0.1 > NUL"
		puts $fh "@echo BAT SLEEP done."
	}
}

proc windows_start_sound_daemon {file} {
	global env
	global use_sound sound_daemon_local_cmd sound_daemon_local_start

	# VF
	regsub {\.bat} $file "snd.bat" file2
	set fh2 [open $file2 "w"]

	puts $fh2 $sound_daemon_local_cmd
	bat_sleep $fh2
	puts $fh2 "del $file2"
	close $fh2

	mesg "Starting SOUND daemon..."
	if [info exists env(COMSPEC)] {
		if [info exists env(SSVNC_BAT_SLEEP)] {
			exec $env(COMSPEC) /c start $env(COMSPEC) /c $file2 &
		} else {
			exec $env(COMSPEC) /c $file2 &
		}
	} else {
		if [info exists env(SSVNC_BAT_SLEEP)] {
			exec cmd.exe /c start cmd.exe /c $file2 &
		} else {
			exec cmd.exe /c $file2 &
		}
	}
	after 1500
}

proc winkill {pid} {
	global is_win9x

	if {$pid == ""} {
		return
	}
	if {! $is_win9x} {
		catch {exec tskill.exe $pid}
		after 100
		catch {exec taskkill.exe /PID $pid}
		after 100
	}
	catch {exec w98/kill.exe /f $pid}
}

proc windows_stop_sound_daemon {} {
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
		winkill $pid
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
	regsub { .*$} $str "" str
	if {! [regexp {:[0-9][0-9]*$} $str]} {
		append str ":22"
	}
	regsub {:[0-9][0-9]*$} $str "" ssh_host
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

proc check_debug_netstat {port str wn} {
	global debug_netstat
	if {! [info exists debug_netstat]} {
		return
	}
	if {$debug_netstat == "0" || $debug_netstat == ""} {
		return
	}
	mesg "DBG: $wn"

	toplev .dbns

	set h 35
	if [small_height] {
		set h 28
	}
	scroll_text_dismiss .dbns.f 82 $h
	center_win .dbns
	.dbns.f.t insert end "LOOKING FOR PORT: $port\n\n$str"
	jiggle_text .dbns.f.t
	update
	after 1000
}

proc launch_windows_ssh {hp file n} {
	global is_win9x env
	global use_sshssl use_ssh putty_pw putty_args
	global port_knocking_list
	global use_listen listening_name
	global disable_ssl_workarounds disable_ssl_workarounds_type
	global ts_only
	global debug_netstat

	set hpnew  [get_ssh_hp $hp]
	set proxy  [get_ssh_proxy $hp]
	set sshcmd [get_ssh_cmd $hp]

	global win_localhost

	set vnc_host $win_localhost
	set vnc_disp $hpnew
	regsub {^.*:} $vnc_disp "" vnc_disp

	regsub {\.bat} $file ".flg" flag

	if {$ts_only} {
		regsub {:0$} $hpnew "" hpnew 
		if {$proxy == ""} {
			if {[regexp {^(.*):([0-9][0-9]*)$} $hpnew mv sshhst sshpt]} {
				set proxy "$sshhst:$sshpt"
				set hpnew $win_localhost
			}
		} else {
			if {![regexp {,} $proxy]} {
				if {$hpnew != $win_localhost} {
					set proxy "$proxy,$hpnew"
					set hpnew $win_localhost
				}
			}
		}
	} elseif {![regexp {^-?[0-9][0-9]*$} $vnc_disp]} {
		if {[regexp {cmd=SHELL} $hp]} {
			;
		} elseif {[regexp {cmd=PUTTY} $hp]} {
			;
		} else {
			# XXX add :0 instead?
			if {1} {
				set vnc_disp "vnc_disp:0"
				mesg "Added :0 to $vnc_disp"
			} else {
				mesg "Bad vncdisp, missing :0 ?, $vnc_disp"
				bell
				return 0
			}
		}
	}

	if {$use_listen} {
		set vnc_port 5500
	} else {
		set vnc_port 5900
	}

	if {$ts_only || [regexp {PORT= .*x11vnc} $sshcmd] || [regexp {P= .*x11vnc} $sshcmd]} {
		regsub {PORT= [ 	]*} $sshcmd "" sshcmd
		regsub {P= [ 	]*} $sshcmd "" sshcmd
		set vnc_port [expr "8100 + int(4000 * rand())"]
		set sshcmd "$sshcmd -rfbport $vnc_port"
	} elseif {[regexp {^-[0-9][0-9]*$} $vnc_disp]} {
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

	global ssh_ipv6_pid
	set ssh_ipv6_pid ""

	set ssh_port 22
	set ssh_host [host_part $hpnew]

	set double_ssh ""
	set p_port ""
	if {$proxy != ""} {
		if [regexp -nocase {(http|https|socks|socks4|socks5|repeater)://} $proxy] {
			set pproxy ""
			set sproxy1 ""
			set sproxy_rest ""
			set sproxy1_host ""
			set sproxy1_user ""
			set sproxy1_port ""
			foreach part [split $proxy ","] {
				if {[regexp {^[ 	]*$} $part]} {
					continue
				}
				if [regexp -nocase {^(http|https|socks|socks4|socks5|repeater)://} $part] {
					if {$pproxy == ""} {
						set pproxy $part
					} else {
						set pproxy "$pproxy,$part"
					}
				} else {
					if {$sproxy1 == ""} {
						set sproxy1 $part
					} else {
						if {$sproxy_rest == ""} {
							set sproxy_rest $part
						} else {
							set sproxy_rest "$sproxy_rest,$part"
						}
					}
				}
			}

#mesg "pproxy: $pproxy"; after 2000
#mesg "sproxy1: $sproxy1"; after 2000
#mesg "sproxy_rest: $sproxy_rest"; after 2000
#mesg "ssh_host: $ssh_host"; after 2000
#mesg "ssh_port: $ssh_port"; after 2000

			if {$sproxy1 != ""} {
				regsub {:[0-9][0-9]*$} $sproxy1 "" sproxy1_host
				regsub {^.*@} $sproxy1_host "" sproxy1_host
				regsub {@.*$} $sproxy1 "" sproxy1_user
				regsub {^.*:} $sproxy1 "" sproxy1_port
			} else {
				regsub {:[0-9][0-9]*$} $ssh_host "" sproxy1_host
				regsub {^.*@} $sproxy1_host "" sproxy1_host
				regsub {@.*$} $ssh_host "" sproxy1_user
				regsub {^.*:} $ssh_host "" sproxy1_port
			}
			if {![regexp {^[0-9][0-9]*$} $sproxy1_port]} {
				set sproxy1_port 22
			}
			if {$sproxy1_user != ""} {
				set sproxy1_user "$sproxy1_user@"
			}

#mesg "sproxy1_host: $sproxy1_host"; after 2000
#mesg "sproxy1_user: $sproxy1_user"; after 2000
#mesg "sproxy1_port: $sproxy1_port"; after 2000

			set port2 ""
			if [regexp -- {-([0-9][0-9]*)} [file tail $file] mv dport] {
				set port2 [expr 21000 + $dport]
			} else {
				set port2 [rand_port]
			}

			global have_ipv6
			if {$have_ipv6} {
				set res [ipv6_proxy $pproxy "" ""]
				set pproxy    [lindex $res 0]
				set ssh_ipv6_pid [lindex $res 3]
			}

			set env(SSVNC_PROXY) $pproxy
			set env(SSVNC_LISTEN) $port2
			set env(SSVNC_DEST) "$sproxy1_host:$sproxy1_port"

			mesg "Starting Proxy TCP helper on port $port2 ..."
			after 300
			# ssh br case:
			set proxy_pid [exec "connect_br.exe" &]

			catch { unset env(SSVNC_PROXY)  }
			catch { unset env(SSVNC_LISTEN) }
			catch { unset env(SSVNC_DEST)   }

			if {$sproxy1 == ""} {
				set proxy "$win_localhost:$port2"
				if [regexp {^(.*)@} $ssh_host mv u] {
					set proxy "$u@$proxy"
				}
			} else {
				set proxy "${sproxy1_user}$win_localhost:$port2"
			}
			if {$sproxy_rest != ""} {
				set proxy "$proxy,$sproxy_rest"
			}
			mesg "Set proxy to: $proxy"
			after 300
		}
		if [regexp {,} $proxy] {
			if {$is_win9x} {
				mesg "Double proxy does not work on Win9x"
				bell
				winkill $ssh_ipv6_pid
				set ssh_ipv6_pid ""
				return 0
			}
			# user1@gateway:port1,user2@workstation:port2
			set proxy1 ""
			set proxy2 ""
			set s [split $proxy ","]
			set proxy1 [lindex $s 0]
			set proxy2 [lindex $s 1]

			set p_port ""
			if [regexp -- {-([0-9][0-9]*)} [file tail $file] mv dport] {
				set p_port [expr 4000 + $dport]
			} else {
				set p_port [expr 3000 + 1000 * rand()]	
				set p_port [expr round($p_port)]
			}

			set s [ssh_split $proxy1]
			set ssh_user1 [lindex $s 0]
			set ssh_host1 [lindex $s 1]
			set ssh_port1 [lindex $s 2]

			set s [ssh_split $proxy2]
			set ssh_user2 [lindex $s 0]
			set ssh_host2 [lindex $s 1]
			set ssh_port2 [lindex $s 2]

			if {! [regexp {^[0-9][0-9]*$} $ssh_port1]} {
				set ssh_port1 22
			}
			if {! [regexp {^[0-9][0-9]*$} $ssh_port2]} {
				set ssh_port2 22
			}

			set u1 ""
			if {$ssh_user1 != ""} {
				set u1 "${ssh_user1}@"
			}
			set u2 ""
			if {$ssh_user2 != ""} {
				set u2 "${ssh_user2}@"
			}
		
			set double_ssh "-L $p_port:$ssh_host2:$ssh_port2 -P $ssh_port1 $u1$ssh_host1"
			set proxy_use "${u2}$win_localhost:$p_port"

		} else {
			# user1@gateway:port1
			set proxy_use $proxy
		}

		set ssh_host [host_part $proxy_use]

		set ssh_port [port_part $proxy_use]
		if {! [regexp {^[0-9][0-9]*$} $ssh_port]} {
			set ssh_port 22
		}

		set vnc_host [host_part $hpnew]
		if {$vnc_host == ""} {
			set vnc_host $win_localhost
		}
	}

	if {![regexp {^[^ 	][^ 	]*@} $ssh_host]} {
		mesg "You must supply a username: user@host..."
		bell
		winkill $ssh_ipv6_pid
		set ssh_ipv6_pid ""
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
			bat_sleep $fh
			close $fh

			# VF
			regsub {\.bat} $file "pre.bat" file_pre
			set fh [open $file_pre "w"]
			set plink_str "plink.exe -ssh -C -P $ssh_port -m $file_pre_cmd $verb -t" 
			if {$putty_args != ""} {
				append plink_str " $putty_args"
			}

			global smb_redir_0
			if {$smb_redir_0 != ""} {
				append plink_str " $smb_redir_0"
			}

			if [regexp {%} $ssh_host] {
				set uath ""
				regsub -all {%SPACE} $ssh_host " " uath
				regsub -all {%TAB} $uath "	" uath
				append plink_str "$pw \"$uath\"" 
			} else {
				append plink_str "$pw $ssh_host" 
			}

			if {$pw != ""} {
				puts $fh "echo off"
			}
			puts $fh $plink_str

			bat_sleep $fh
			if {![info exists env(SSVNC_NO_DELETE)]} {
				if {$file_pre_cmd != ""} {
					puts $fh "del $file_pre_cmd"
				}
				puts $fh "del $file_pre"
			}
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

	if {$sshcmd != "SHELL" && [regexp -nocase {x11vnc} $sshcmd]} {
		global use_cups cups_x11vnc cups_remote_port
		global cups_remote_smb_port
		global use_sound sound_daemon_x11vnc sound_daemon_remote_port 
		global ts_only
		if {$ts_only} {
			set cups_x11vnc 1
			set sound_daemon_x11vnc 1
		}
		if {$use_cups && $cups_x11vnc && $cups_remote_port != ""} {
			set crp $cups_remote_port
			if {$ts_only} {
				set cups_remote_port [rand_port]
				set crp "DAEMON-$cups_remote_port"
			}
			set sshcmd "$sshcmd -env FD_CUPS=$crp"
		}
		if {$use_cups && $cups_x11vnc && $cups_remote_smb_port != ""} {
			set csp $cups_remote_smb_port
			if {$ts_only} {
				set cups_remote_smb_port [rand_port]
				set csp "DAEMON-$cups_remote_smb_port"
			}
			set sshcmd "$sshcmd -env FD_SMB=$csp"
		}
		if {$use_sound && $sound_daemon_x11vnc && $sound_daemon_remote_port != ""} {
			set srp $sound_daemon_remote_port
			if {$ts_only} {
				set sound_daemon_remote_port [rand_port]
				set srp "DAEMON-$sound_daemon_remote_port"
			}
			set sshcmd "$sshcmd -env FD_ESD=$srp"
		}
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
		bat_sleep $fh_cmd
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
		set vnc_host $win_localhost
	}
	regsub {^.*@} $vnc_host "" vnc_host

	set redir "-L $use:$vnc_host:$vnc_port"
	if {$use_listen} {
		set redir "-R $vnc_port:$vnc_host:$use"
		set listening_name "localhost:$vnc_port  (on remote SSH side)"
	}

	set plink_str "plink.exe -ssh -P $ssh_port $verb $redir $extra_redirs -t" 
	if {$putty_args != ""} {
		append plink_str " $putty_args"
	}
	if {$extra_redirs != ""} {
		regsub {exe} $plink_str "exe -C" plink_str
	} else {
		# hmm we used to have it off... why?
		# ssh typing response?
		regsub {exe} $plink_str "exe -C" plink_str
	}
	set uath $ssh_host
	if [regexp {%} $uath] {
		regsub -all {%SPACE} $uath " " uath
		regsub -all {%TAB} $uath "	" uath
		set uath "\"$uath\""
	}
	if {$do_shell} {
		if {$sshcmd == "PUTTY"} {
		    if [regexp {^".*@} $uath] { #"
			    regsub {@} $uath {" "} uath
			    set uath "-l $uath"
		    }
		    if {$is_win9x} {
			set plink_str "putty.exe -ssh -C -P $ssh_port $extra_redirs $putty_args -t $pw $uath" 
		    } else {
			set plink_str "start \"putty $ssh_host\" putty.exe -ssh -C -P $ssh_port $extra_redirs $putty_args -t $pw $uath" 
			if [regexp {FINISH} $port_knocking_list] {
				regsub {start} $plink_str "start /wait" plink_str
			}
		    }
		} else {
			set plink_str "plink.exe -ssh -C -P $ssh_port $extra_redirs $putty_args -t $pw $uath" 
			append plink_str { "$SHELL"}
		}
	} elseif {$file_cmd != ""} {
		append plink_str " -m $file_cmd$pw $uath"
	} else {
		append plink_str "$pw $uath \"$sshcmd\""
	}

	if {$pw != ""} {
		puts $fh "echo off"
	}
	if {$ts_only && [regexp {sudo } $sshcmd]} {
		puts $fh "echo \" \""
		puts $fh "echo \"Doing Initial SSH with sudo id to prime sudo...\""
		puts $fh "echo \" \""
		puts $fh "plink.exe -ssh $putty_args -t $uath \"sudo id; tty\""
		puts $fh "echo \" \""
	}
	puts $fh $plink_str
	bat_sleep $fh
	puts $fh "del $flag"
	if {![info exists env(SSVNC_NO_DELETE)]} {
		if {$file_cmd != ""} {
			puts $fh "del $file_cmd"
		}
		puts $fh "del $file"
	}
	close $fh

	catch {destroy .o}
	catch {destroy .oa}
	catch {destroy .os}

	if { ![do_port_knock $ssh_host start]} {
		if {![info exists env(SSVNC_NO_DELETE)]} {
			catch {file delete $file}
			if {$file_cmd != ""} {
				catch {file delete $file_cmd}
			}
			if {$file_pre != ""} {
				catch {file delete $file_pre}
			}
		}
		winkill $ssh_ipv6_pid
		set ssh_ipv6_pid ""
		return 0
	}

	if {$double_ssh != ""} {
		set plink_str_double_ssh "plink.exe -ssh $putty_args -t $pw $double_ssh \"echo sleep 60 ...; sleep 60; echo done.\"" 

		# VF
		regsub {\.bat} $file "dob.bat" file_double
		set fhdouble [open $file_double "w"]
		puts $fhdouble $plink_str_double_ssh
		bat_sleep $fhdouble
		puts $fhdouble "del $flag"
		if {![info exists env(SSVNC_NO_DELETE)]} {
			puts $fhdouble "del $file_double"
		}
		close $fhdouble

		set com "cmd.exe"
		if [info exists env(COMSPEC)] {
			set com $env(COMSPEC)
		}

		set ff [open $flag "w"]
		puts $ff "flag"
		close $ff

		global env
		if [info exists env(SSVNC_BAT_SLEEP)] {
			exec $com /c start $com /c $file_double &
		} else {
			exec $com /c $file_double &
		}

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
			check_debug_netstat $p_port $ns $waited
			append re {[ 	][ 	]*[0:.][0:.]*[ 	][ 	]*LISTEN}
			if [regexp $re $ns] {
				set gotit 1
				break
			}
			set waited [expr "$waited + 500"]
			if {![file exists $flag]} {
				break
			}
		}
		catch {file delete $flag}	
		if {! $gotit} {
			after 5000
		}
	}

	vencrypt_tutorial_mesg

	set wdraw 1
	#set wdraw 0
	if [info exists debug_netstat] {
		if {$debug_netstat != "" && $debug_netstat != "0"} {
			set wdraw 0
		}
	}

	set ff [open $flag "w"]
	puts $ff "flag"
	close $ff

	if {$is_win9x} {
		if {$wdraw} {
			wm withdraw .
		}
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

			global env
			if [info exists env(SSVNC_BAT_SLEEP)] {
				exec $com /c start $com /c $file_pre &
			} else {
				exec $com /c $file_pre &
			}

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
		if {$wdraw} {
			wm withdraw .
		}

		update
		if {$do_shell && [regexp {FINISH} $port_knocking_list]} {
			catch {exec $com /c $file}
		} else {
			global env
			if [info exists env(SSVNC_BAT_SLEEP)] {
				exec $com /c start $com /c $file &
			} else {
				exec $com /c $file &
			}
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
	set made_plink 0
	if {$is_win9x} {
		make_plink
		set made_plink 1
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
		check_debug_netstat $use $ns $waited
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
		if {![file exists $flag]} {
			set plink_status flag_gone
			break
		}
	}
	catch {file delete $flag}	
	if {$plink_status == ""} {
		if {! $made_plink} {
			make_plink
			set made_plink 1
		}
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
		set m "unknown"
		if {$plink_status == "flag_gone"} {
			set m "plink script failed"
		} elseif {$plink_status == ""} {
			set m "timeout"
		}
		mesg "Error ($m) to $hp"
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

	if {![info exists env(SSVNC_NO_DELETE)]} {
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
	}

	winkill $ssh_ipv6_pid
	set ssh_ipv6_pid ""

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

proc mytmp {tmp} {
	global is_windows mktemp env

	if {$is_windows} {
		return $tmp
	}

	if {! [info exists mktemp]} {
		set mktemp ""
		foreach dir {/bin /usr/bin /usr/local/bin} {
			if [file exists "$dir/mktemp"] {
				set mktemp "$dir/mktemp"
				break
			}
		}
	}
	if {$mktemp != ""} {
		set tmp2 ""
		catch {set tmp2 [exec $mktemp "$tmp.XXXXXX"]}
		if [file exists $tmp2] {
			if [info exists env(DEBUG_MKTEMP)] {
				puts stderr "mytmp: $tmp2"
			}
			return $tmp2
		}
	}
	catch {exec rm -f $tmp}
	catch {file delete $tmp}
	if [file exists $tmp] {
		puts stderr "tmp file still exists: $tmp"
		exit 1
	}
	catch {exec touch $tmp}
	catch {exec chmod 600 $tmp}
	if [info exists env(DEBUG_MKTEMP)] {
		puts stderr "mytmp: $tmp"
	}
	return $tmp
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
		mac_raise
		return
	}

	global darwin_terminal_cnt
	set tmp /tmp/darwin_terminal_cmd.[tpid]
	if {! [info exists darwin_terminal_cnt]} {
		set darwin_terminal_cnt 0
	}
	incr darwin_terminal_cnt
	append tmp ".$darwin_terminal_cnt"
	set tmp [mytmp $tmp]
	
	set fh ""
	catch {set fh [open $tmp w 0755]}
	catch {[exec chmod 755 $tmp]}
	if {$fh == ""} {
		raise .
		tk_messageBox -type ok -icon error -message "Cannot open temporary file: $tmp" -title "Cannot open file"
		mac_raise
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
	puts $fh {osascript -e 'tell application "Terminal" to activate' >/dev/null 2>&1 &}
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
	global uname env
	if {$uname == "Darwin"} {
		global env
		set doX  0;
		if {! $doX} {
			darwin_terminal_cmd $title $cmd $bg
			return
		}
	}

	global checked_for_xterm
	if {![info exists checked_for_xterm]} {
		set p ""
		set r [catch {set p [exec /bin/sh -c {type xterm}]}]
		set checked_for_xterm 1
		if {$r != 0} {
			set p [exec /bin/sh -c {type xterm 2>&1; exit 0}]
			set txt "Problem finding the 'xterm' command:\n\n$p\n\n"
			append txt "Perhaps you need to install a package containing 'xterm'  (Sigh...)\n\n"
			fetch_dialog $txt "xterm" "xterm" 0 [line_count $txt]
			update
			after 1000
			catch {tkwait window .fetch}
			update
		}
	}

	if [info exists env(SSVNC_XTERM_REPLACEMENT)] {
		set tcmd $env(SSVNC_XTERM_REPLACEMENT)
		if {$tcmd != ""} {
			regsub -all {%GEOMETRY} $tcmd $geometry tcmd
			regsub -all {%TITLE} $tcmd $title tcmd

			set tmp1 /tmp/xterm_replacement1.[tpid]
			set tmp1 [mytmp $tmp1]
			set fh1 ""
			catch {set fh1 [open $tmp1 "w"]}

			set tmp2 /tmp/xterm_replacement2.[tpid]
			set tmp2 [mytmp $tmp2]
			set fh2 ""
			catch {set fh2 [open $tmp2 "w"]}
			if {$fh1 != "" && $fh2 != ""} {
				puts $fh1 "#!/bin/sh";
				puts $fh1 "$cmd"
				puts $fh1 "rm -f $tmp1"
				close $fh1
				catch {exec chmod 755 $tmp1}
				puts $fh2 "#!/bin/sh"
				puts $fh2 "$tcmd $tmp1"
				puts $fh2 "rm -f $tmp2"
				close $fh2
				catch {exec chmod 755 $tmp2}
				if {$bg} {
					exec $tmp2 2>@stdout &
				} else {
					exec $tmp2 2>@stdout
				}
				return
			}
			catch {close $fh1}
			catch {close $fh2}
		}
	}

	if {$bg} {
		if {$xrm1 == ""} {
			exec xterm -sb -sl 2000 -geometry "$geometry" -title "$title" -e sh -c "$cmd" 2>@stdout &
		} else {
			exec xterm -sb -sl 2000 -geometry "$geometry" -title "$title" -xrm "$xrm1" -xrm "$xrm2" -xrm "$xrm3" -e sh -c "$cmd" 2>@stdout &
		}
	} else {
		if {$xrm1 == ""} {
			exec xterm -sb -sl 2000 -geometry "$geometry" -title "$title" -e sh -c "$cmd" 2>@stdout
		} else {
			exec xterm -sb -sl 2000 -geometry "$geometry" -title "$title" -xrm "$xrm1" -xrm "$xrm2" -xrm "$xrm3" -e sh -c "$cmd" 2>@stdout
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
			catch {set g [exec grep main-vnc-helper-finished $tee]}
			if [regexp {main-vnc-helper-finished} $g] {
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
		regsub {:[0-9][0-9]*$} $pxy "" pxy
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

	global ssh_only ts_only
	if {$sshcmd != "" || $ssh_only || $ts_only} {
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
		raise .
		tk_messageBox -type ok -icon info -message $msg
	}
}

proc fetch_cert {save} {
	global env vncdisplay is_windows
	set hp [get_vncdisplay]

	global vencrypt_detected
	set vencrypt_detected ""

	global use_listen
	if {$use_listen} {
		if {$is_windows} {
			mesg "Fetch Cert not enabled for Reverse Connections"
			bell
			catch {raise .}
			mac_raise
			return
		}
		toplev .fcr
		global help_font
		wm title .fcr "Fetch Cert for Reverse Connections"
		global fcr_result
		set fcr_result 0
		eval text .fcr.t -width 55 -height 17 $help_font
		.fcr.t insert end {
   In Reverse VNC Connections (-LISTEN) mode, the
   Fetch Cert operation requires that the Remote
   VNC Server makes an initial connection NOW so
   we can collect its SSL Certificate.  Note that
   this method does not work for VeNCrypt servers.
   (If there are problems Fetching, one can always
   copy and import the Cert file manually.)

   Do you want to Continue with this operation?
   If so, press "Continue" and Then instruct the
   remote VNC Server to make a Reverse Connection
   to us.

   Otherwise, press "Cancel" to cancel the Fetch
   Cert operation.  
}

		button .fcr.cancel   -text Cancel   -command {set fcr_result 0; destroy .fcr}
		button .fcr.continue -text Continue -command {set fcr_result 1; destroy .fcr}
		button .fcr.continu2 -text Continue -command {set fcr_result 1; destroy .fcr}
		global uname
		if {$uname == "Darwin"} {
			pack .fcr.t .fcr.continu2 .fcr.continue .fcr.cancel -side top -fill x
			
		} else {
			pack .fcr.t .fcr.continue .fcr.cancel -side top -fill x
		}
		center_win .fcr

		tkwait window .fcr
		update
		after 50

		if {$fcr_result != 1}  {
			return
		}
		update idletasks
		after 50
	}

	regsub {[ 	]*cmd=.*$} $hp "" tt
	if {[regexp {^[ 	]*$} $tt]} {
		mesg "No host:disp supplied."
		bell
		catch {raise .}
		mac_raise
		return
	}
	if {[regexp -- {--nohost--} $tt]} {
		mesg "No host:disp supplied."
		bell
		catch {raise .}
		mac_raise
		return
	}
	if {! [regexp ":" $hp]} {
		if {! [regexp {cmd=} $hp]} {
			append hp ":0"
		}
	}
	set hpnew  [get_ssh_hp $hp]
	set proxy  [get_ssh_proxy $hp]


	set pstr 1
	mesg "Fetching $hpnew Cert..."
	global cert_text
	set cert_text ""
	.f4.getcert configure -state disabled
	update
	if {! $is_windows} {
		catch {set cert_text [fetch_cert_unix $hp]}
	} else {
		set cert_text [fetch_cert_windows $hp]
	}

	if [info exists env(CERTDBG)] {puts "\nFetch-0-\n$cert_text"}

	set vencrypt 0
	set anondh 0
	if {![regexp {BEGIN CERTIFICATE} $cert_text]} {
		if [regexp {CONNECTED} $cert_text] {
			set m 0
			if {![regexp -nocase {GET_SERVER_HELLO} $cert_text]} {
				set m 1
			}
			if [regexp -nocase -line {GET_SERVER_HELLO.*unknown protocol} $cert_text] {
				set m 1
			}
			if {![regexp -nocase {show_cert: SSL_connect failed} $cert_text]} {
				set m 1
			}
			if {!$m && $is_windows} {
				if [regexp -nocase {write:errno} $cert_text] {
					if [regexp -nocase {no peer certificate} $cert_text] {
						set m 1
					}
				}
			}
			if {$m} {
				# suspect VeNCrypt or ANONTLS plaintext RFB
				set cert_text ""
				set vencrypt 1
				incr pstr
				mesg "#${pstr} Fetching $hpnew Cert... $vencrypt/$anondh"
				if {! $is_windows} {
					catch {set cert_text [fetch_cert_unix    $hp $vencrypt $anondh]}
				} else {
					after 600
					catch {set cert_text [fetch_cert_windows $hp $vencrypt $anondh]}
				}
	if [info exists env(CERTDBG)] {puts "\nFetch-1-\n$cert_text"}
			}
		}
	}
	if {![regexp {BEGIN CERTIFICATE} $cert_text]} {
		if [regexp {CONNECTED} $cert_text] {
			set m 0
			if [regexp -nocase -line {error.*handshake failure} $cert_text] {
				set m 1
			}
			if [regexp -nocase -line {error.*unknown protocol} $cert_text] {
				set m 1
			}
			if {![regexp -nocase {show_cert: SSL_connect failed} $cert_text]} {
				set m 1
			}
			if {!$m && $is_windows} {
				if [regexp -nocase {no peer certificate} $cert_text] {
					set m 1
				}
			}
			if {$m} {
				# suspect Anonymous Diffie Hellman
				set cert_text ""
				set anondh 1
				incr pstr
				mesg "#${pstr} Fetching $hpnew Cert... $vencrypt/$anondh"
				if {! $is_windows} {
					catch {set cert_text [fetch_cert_unix    $hp $vencrypt $anondh]}
				} else {
					after 600
					catch {set cert_text [fetch_cert_windows $hp $vencrypt $anondh]}
				}
	if [info exists env(CERTDBG)] {puts "\nFetch-2-\n$cert_text"}
			}
		}
	}
	if {![regexp {BEGIN CERTIFICATE} $cert_text]} {
		if [regexp {CONNECTED} $cert_text] {
			if {[regexp -nocase -line {cipher.*ADH} $cert_text]} {
				# it is Anonymous Diffie Hellman
				mesg "WARNING: Anonymous Diffie Hellman Server detected (NO CERT)"
				after 300
				.f4.getcert configure -state normal
				return $cert_text
			} else {
				global vencrypt_detected
				set vencrypt_detected ""
			}
		}
	}

	global vencrypt_detected server_vencrypt
	if {$vencrypt_detected != "" && !$server_vencrypt} {
		mesg "VeNCrypt or ANONTLS server detected."
		after 600
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
		set n [line_count $cert_text 1]
		set ok 0
	} else {
		if [regexp -- {-----BEGIN SSL SESSION PARAMETERS-----} $cert_text] {
			set new ""
			set off 0
			foreach line [split $cert_text "\n"] {
				if [regexp -- {RFB 00} $line] {
					continue
				}
				if [regexp -- {Using default temp} $line] {
					continue
				}
				if [regexp -- {-----BEGIN SSL SESSION PARAMETERS-----} $line] {
					set off 1
				}
				if [regexp -- {-----END SSL SESSION PARAMETERS-----} $line] {
					set off 0
					continue
				}
				if {$off} {
					continue;
				}
				append new "$line\n"
			}
			if [regexp -- {-----BEGIN CERTIFICATE-----} $new] {
				set cert_text $new
			}
		}
		set text "" 
		set on 0
		set subject ""
		set curr_subject ""
		set chain_n -1
		set chain(__empty__) ""
		foreach line [split $cert_text "\n"] {
			if [regexp -- {-----BEGIN CERTIFICATE-----} $line] {
				incr on
			}
			if {$chain_n < -1} {
				;
			} elseif [regexp {^ *([0-9]) *s:(.*/[A-Z][A-Z]*=.*$)} $line m cn sb] {
				set cn [string trim $cn]
				set sb [string trim $sb]
				#puts cn=$cn
				#puts sb=$sb
				if {$subject == ""} {
					set subject $sb
				}
				if {$cn > $chain_n} {
					set chain_n $cn
					set curr_subject $sb
				} else {
					set chain_n -2
				}
			} elseif [regexp {^ *i:(.*/[A-Z][A-Z]*=.*$)} $line m is] {
				set is [string trim $is]
				#puts is=$is
				if {$curr_subject != ""} {
					set chain($curr_subject) $is
				}
			}
			if {$on != 1} {
				continue;
			}
			append text "$line\n"
			if [regexp -- {-----END CERTIFICATE-----} $line] {
				set on 2
			}
		}
		set chain_str "subject: not-known\n"
		set curr_subject $subject
		set self_signed 0
		set top_issuer ""
		for {set i 0} {$i < 10} {incr i} {
			if {$curr_subject != ""} {
				if {$i == 0} {
					set chain_str "- subject: $curr_subject\n\n"
				} else {
					set chain_str "${chain_str}- issuer$i: $curr_subject\n\n"
					set top_issuer $curr_subject;
				}
				if {![info exists chain($curr_subject)]} {
					break
				} elseif {$chain($curr_subject) == ""} {
					break
				} elseif {$curr_subject == $chain($curr_subject)} {
					set j [expr $i + 1]
					set chain_str "${chain_str}- issuer$j: $curr_subject\n\n"
					set top_issuer $curr_subject;
					if {$i == 0} {
						set self_signed 1
					}
					break;
				}
				set curr_subject $chain($curr_subject)
			}
		}
		set chain_str "${chain_str}INFO: SELF_SIGNED=$self_signed\n\n"
		if {$self_signed} {
			set chain_str "${chain_str}INFO: Certificate is Self-Signed.\n"
			set chain_str "${chain_str}INFO: It will successfully authenticate when used as a ServerCert or Accepted-Cert.\n"
			set chain_str "${chain_str}INFO: Be sure to check carefully that you trust this certificate before saving it.\n"
		} else {
			set chain_str "${chain_str}INFO: Certificate is signed by a Certificate Authority (CA).\n"
			set chain_str "${chain_str}INFO: It *WILL NOT* successfully authenticate when used as a ServerCert or Accepted-Cert.\n"
			set chain_str "${chain_str}INFO: You need to Obtain and Save the CA's Certificate (issuer) instead"
			if {$top_issuer != ""} {
				set chain_str "${chain_str}:\nINFO: CA: $top_issuer\n"
			} else {
				set chain_str "${chain_str}.\n"
			}
		}
		#puts "\n$chain_str\n"

		global is_windows
		set tmp "/tmp/cert.hsh.[tpid]"
		set tmp [mytmp $tmp]
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
			set cert_text "$cert_text\n\n----------------------------------\nOutput of  openssl x509 -text -fingerprint:\n\n$info"
		}
		set cert_text "==== SSL Certificate from $hp ====\n\n$chain_str\n$cert_text"
	}

	if {! $save} {
		return $cert_text
	}

	fetch_dialog $cert_text $hp $hpnew $ok $n
}

proc skip_non_self_signed {w hp} {
	set msg "Certificate from $hp is not Self-Signed, it was signed by a Certificate Authority (CA).  Saving it does not make sense because it cannot be used to authenticate anything.  You need to Obtain and Save the CA Certificate instead.  Save it anyway?"
	set reply [tk_messageBox -type okcancel -default cancel -parent $w -icon warning -message $msg -title "CA Signed Certificate"]
	if {$reply == "cancel"} {
		return 1
	} else {
		return 0
	}
}
	
proc fetch_dialog {cert_text hp hpnew ok n} {
	toplev .fetch

	if [small_height] {
		set n 28
	}

	scroll_text_dismiss .fetch.f 90 $n

	if {$ok} {
		set ss 0
		if [regexp {INFO: SELF_SIGNED=1} $cert_text] {
			button .fetch.save -text Save -command "destroy .fetch; save_cert {$hpnew}"
			set ss 1
		} else {
			button .fetch.save -text Save -command "if \[skip_non_self_signed .fetch {$hpnew}\] {return} else {destroy .fetch; save_cert {$hpnew}}"
			set ss 0
		}
		button .fetch.help -text Help -command "help_fetch_cert $ss"
		pack .fetch.help .fetch.save -side bottom -fill x
		.fetch.d configure -text "Cancel"
	}

	center_win .fetch
	wm title .fetch "$hp Certificate"

	.fetch.f.t insert end $cert_text
	jiggle_text .fetch.f.t
}


proc host_part {hp} {
	regsub {^ *}  $hp "" hp
	regsub { .*$} $hp "" hp
	if [regexp {^[0-9][0-9]*$} $hp] {
		return ""
	}
	set h $hp
	regsub {:[0-9][0-9]*$} $hp "" h
	return $h
}

proc port_part {hp} {
	regsub { .*$} $hp "" hp
	set p ""
	if [regexp {:([0-9][0-9]*)$} $hp m val] {
		set p $val
	}
	return $p
}

proc get_vencrypt_proxy {hpnew} {
	if [regexp -nocase {^vnc://} $hpnew] {
		return ""
	}
	set hpnew  [get_ssh_hp $hpnew]
	regsub -nocase {^[a-z0-9+]*://} $hpnew "" hpnew
	set h [host_part $hpnew]
	set p [port_part $hpnew]

	if {$p == ""} {
		# might not matter, i.e. SSH+SSL only...
		set p 5900
	}
	set hp2 $h
	if {$p < 0} {
		set hp2 "$hp2:[expr - $p]"
	} elseif {$p < 200} {
		set hp2 "$hp2:[expr $p + 5900]"
	} else {
		set hp2 "$hp2:$p"
	}
	return "vencrypt://$hp2"
}

proc fetch_cert_unix {hp {vencrypt 0} {anondh 0}} {
	global use_listen

	set hpnew  [get_ssh_hp $hp]
	set proxy  [get_ssh_proxy $hp]

	if {$vencrypt} {
		global vencrypt_detected
		set vencrypt_detected [get_vencrypt_proxy $hpnew]
		if {$proxy != ""} {
			set proxy "$proxy,$vencrypt_detected"
		} else {
			set proxy $vencrypt_detected
		}
	}

	set cmd [list ss_vncviewer]
	if {$anondh} {
		lappend cmd "-anondh"
	}
	if {$proxy != ""} {
		lappend cmd "-proxy"
		lappend cmd $proxy
	}
	if {$use_listen} {
		lappend cmd "-listen"
	}
	lappend cmd "-showcert"
	lappend cmd $hpnew

	if {$proxy != ""} {
		lappend cmd "2>/dev/null"
	}
	global env
	if [info exists env(CERTDBG)] {puts "\nFetch-cmd: $cmd"}
	set env(SSVNC_SHOWCERT_EXIT_0) 1

	return [eval exec $cmd]
}

proc win_nslookup {host} {
	global win_nslookup_cache
	if [info exists win_nslookup_cache($host)] {
		return $win_nslookup_cache($host)
	}
	if [regexp -nocase {[^a-z0-9:._-]} $host]  {
		set win_nslookup_cache($host) "invalid"
		return $win_nslookup_cache($host)
	}
	if [regexp {^[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$} $host] {
		set win_nslookup_cache($host) $host
		return $win_nslookup_cache($host)
	}
	if [regexp -nocase {^[a-f0-9]*:[a-f0-9:]*:[a-f0-9:]*$} $host] {
		set win_nslookup_cache($host) $host
		return $win_nslookup_cache($host)
	}
	set nsout ""
	catch {set nsout [exec nslookup $host]}
	if {$nsout == "" || [regexp -nocase {server failed} $nsout]} {
		after 250
		set nsout ""
		catch {set nsout [exec nslookup $host]}
	}
	if {$nsout == "" || [regexp -nocase {server failed} $nsout]} {
		set win_nslookup_cache($host) "unknown"
		return $win_nslookup_cache($host)
	}
	regsub -all {Server:[^\n]*\nAddress:[^\n]*} $nsout "" nsout
	regsub {^.*Name:} $nsout "" nsout
	if [regexp {Address:[ \t]*([^\n]+)} $nsout mv addr] {
		set addr [string trim $addr]
		if {$addr != ""} {
			set win_nslookup_cache($host) $addr
			return $win_nslookup_cache($host)
		}
	}
	set win_nslookup_cache($host) "unknown"
	return $win_nslookup_cache($host)
}

proc win_ipv4 {host} {
	global win_localhost
	set ip [win_nslookup $host];
	if [regexp {^[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$} $ip] {
		return 1
	}
	return 0
}

proc ipv6_proxy {proxy host port} {
	global is_windows win_localhost have_ipv6

	if {!$have_ipv6} {
		return [list $proxy $host $port ""]
	} elseif {!$is_windows} {
		return [list $proxy $host $port ""]
	} else {
		set h0 ""
		set p0 ""
		set port3 ""
		set ipv6_pid ""
		set proxy0 $proxy
		if {$proxy == ""} {
			if [win_ipv4 $host] {
				return [list $proxy $host $port ""]
			}
			set port3 [rand_port] 
			set h0 $host
			set p0 $port
			set host $win_localhost
			set port $port3
		} else {
			set parts [split $proxy ","] 
			set n [llength $parts]
			for {set i 0} {$i < $n} {incr i} {
				set part [lindex $parts $i]
				set prefix ""
				set repeater 0
				regexp -nocase {^[a-z0-9+]*://} $part prefix
				regsub -nocase {^[a-z0-9+]*://}	$part "" part
				if [regexp {^repeater://} $prefix] {
					regsub {\+.*$} $part "" part
					if {![regexp {:([0-9][0-9]*)$} $part]} {
						set part "$part:5900"
					}
				}
				set modit 0
				set h1 ""
				set p1 ""
				if [regexp {^(.*):([0-9][0-9]*)$} $part mvar h1 p1] {
					if {$h1 == "localhost" || $h1 == $win_localhost} {
						continue
					} elseif [win_ipv4 $h1] {
						break
					}
					set modit 1
				} else {
					break
				}
				if {$modit} {
					set port3 [rand_port] 
					set h0 $h1
					set p0 $p1
					lset parts $i "$prefix$win_localhost:$port3"
					break
				}
			}
			if {$h0 != "" && $p0 != "" && $port3 != ""} {
				set proxy [join $parts ","]
				#mesg "Reset proxy: $proxy"; after 3000
			}
		}
		if {$h0 != "" && $p0 != "" && $port3 != ""} {
			mesg "Starting IPV6 helper on port $port3 ..."
			set ipv6_pid [exec relay6.exe $port3 "$h0" "$p0" /b:$win_localhost &]
			after 400
			#mesg "r6 $port3 $h0 $p0"; after 3000
		}
		return [list $proxy $host $port $ipv6_pid]
	}
}

proc fetch_cert_windows {hp {vencrypt 0} {anondh 0}} {
	global have_ipv6

	regsub {^vnc.*://} $hp "" hp

	set hpnew  [get_ssh_hp $hp]
	set proxy  [get_ssh_proxy $hp]

	if {$vencrypt} {
		global vencrypt_detected
		set vencrypt_detected [get_vencrypt_proxy $hpnew]
		if {$proxy != ""} {
			set proxy "$proxy,$vencrypt_detected"
		} else {
			set proxy $vencrypt_detected
		}
	}

	set host [host_part $hpnew]

	global win_localhost

	if {$host == ""} {
		set host $win_localhost
	}

	if [regexp {^.*@} $host match] {
		mesg "Trimming \"$match\" from hostname"
		regsub {^.*@} $host "" host
	}

	set disp [port_part $hpnew]

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

	set ipv6_pid ""
	if {$have_ipv6} {
		set res [ipv6_proxy $proxy $host $port]
		set proxy    [lindex $res 0]
		set host     [lindex $res 1]
		set port     [lindex $res 2]
		set ipv6_pid [lindex $res 3]
	}

	if {$proxy != ""} {
		global env

		set port2 [rand_port] 

		set sp ""
		if [info exists env(SSVNC_PROXY)] {
			set sp $env(SSVNC_PROXY)
		}
		set sl ""
		if [info exists env(SSVNC_LISTEN)] {
			set sl $env(SSVNC_LISTEN)
		}
		set sd ""
		if [info exists env(SSVNC_DEST)] {
			set sd $env(SSVNC_DEST)
		}

		set env(SSVNC_PROXY) $proxy
		set env(SSVNC_LISTEN) $port2
		set env(SSVNC_DEST) "$host:$port"

		set host $win_localhost
		set port $port2

		mesg "Starting Proxy TCP helper on port $port2 ..."
		after 300
		# fetch cert br case:
		set proxy_pid [exec "connect_br.exe" &]

		if {$sp == ""} {
			catch { unset env(SSVNC_PROXY) }
		} else {
			set env(SSVNC_PROXY) $sp
		}
		if {$sl == ""} {
			catch { unset env(SSVNC_LISTEN) }
		} else {
			set env(SSVNC_LISTEN) $sl
		}
		if {$sd == ""} {
			catch { unset env(SSVNC_DEST) }
		} else {
			set env(SSVNC_DEST) $sd
		}
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
		if {$anondh} {
			set ph [open "| $ossl s_client -prexit -connect $host:$port -cipher ALL:RC4+RSA:+SSLv2:@STRENGTH < $tin 2>NUL" "r"]
		} else {
			set ph [open "| $ossl s_client -prexit -connect $host:$port < $tin 2>NUL" "r"]
		}

		set text ""
		if {$ph != ""} {
			set pids [pid $ph]
			set got 0
			while {[gets $ph line] > -1} {
				append text "$line\n"
				if [regexp {END CERT} $line] {
					set got 1
				}
				if {$anondh && [regexp -nocase {cipher.*ADH} $line]} {
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
				winkill $pid
			}
			if {$ipv6_pid != ""} {
				winkill $ipv6_pid
			}

			catch {close $ph}
			catch {file delete $tin $tou}
			return $text
		}
	} else {
		set pids ""

		if {1} {
			if {$anondh} {
				set ph2 [open "| $ossl s_client -prexit -connect $host:$port -cipher ALL:RC4+RSA:+SSLv2:@STRENGTH > $tou 2>NUL" "w"]
			} else {
				set ph2 [open "| $ossl s_client -prexit -connect $host:$port > $tou 2>NUL" "w"]
			}
			set pids [pid $ph2]
			after 500
			for {set i 0} {$i < 128} {incr i} {
				puts $ph2 "Q"
			}
			catch {close $ph2}
		} else {
			if {$anondh} {
				set pids [exec $ossl s_client -prexit -connect $host:$port -cipher ALL:RC4+RSA:+SSLv2:@STRENGTH < $tin >& $tou &]
			} else {
				set pids [exec $ossl s_client -prexit -connect $host:$port < $tin >& $tou &]
			}
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
		foreach pid $pids {
			winkill $pid
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
	if {$ipv6_pid != ""} {
		winkill $ipv6_pid
	}
	return $text
}

proc check_accepted_certs {{probe_only 0}} {
	global cert_text always_verify_ssl
	global skip_verify_accepted_certs use_listen
	global ultra_dsm env
	global server_vencrypt server_anondh no_probe_vencrypt

	if {! $always_verify_ssl} {
		set skip_verify_accepted_certs 1
		if {$server_vencrypt} {
			return 1
		}
		if {$no_probe_vencrypt} {
			return 1
		}
	}
	if {$server_anondh} {
		mesg "WARNING: Anonymous Diffie Hellman (SKIPPING CERT CHECK)"
		after 1000
		set skip_verify_accepted_certs 1
		return 1
	}
	if {$ultra_dsm} {
		return 1;
	}
	if {$use_listen} {
		return 1;
	}

	global anon_dh_detected
	set anon_dh_detected 0

	set cert_text [fetch_cert 0]

	set mvar ""
	if {[regexp -nocase -line {cipher.*ADH} $cert_text mvar]} {

		if [info exists env(CERTDBG)] {puts "\nFetch-MSG-\n$cert_text"}
		if [info exists env(CERTDBG)] {puts "\nBEGIN_MVAR: $mvar\nEND_MVAR\n"}

		set msg "Anonymous Diffie-Hellman server detected.  There will be encryption, but no SSL/TLS authentication. Continue?"
		set reply [tk_messageBox -type okcancel -default ok -icon warning -message $msg -title "Anonymous Diffie-Hellman Detected"]
		set anon_dh_detected 1
		if {$reply == "cancel"} {
			return 0
		} else {
			global skip_verify_accepted_certs
			set skip_verify_accepted_certs 1
			return 1
		}
	}

	if {$probe_only} {
		return 1
	}
	if {! $always_verify_ssl} {
		return 1
	}

	set from ""
	set fingerprint ""
	set fingerline ""
	set self_signed 1
	set subject_issuer ""
	set subject ""
	set issuer ""

	set i 0
	foreach line [split $cert_text "\n"] {
		incr i
		if {$i > 50} {
			break
		}
		if [regexp {^- subject: *(.*)$} $line m val] {
			set val [string trim $val]
			set subject_issuer "${subject_issuer}subject:$val\n"
			set subject $val
		}
		if [regexp {^- (issuer[0-9][0-9]*): *(.*)$} $line m is val] {
			set val [string trim $val]
			set subject_issuer "${subject_issuer}$is:$val\n"
			set issuer $val
		}
		if [regexp {^INFO: SELF_SIGNED=(.*)$} $line m val] {
			set subject_issuer "${subject_issuer}SELF_SIGNED:$val\n"
		}
		if [regexp {^depth=} $line] {
			break
		}
		if [regexp {^verify } $line] {
			break
		}
		if [regexp {^CONNECTED} $line] {
			break
		}
		if [regexp {^Certificate chain} $line] {
			break
		}
		if [regexp {^==== SSL Certificate from (.*) ====} $line mv str] {
			set from [string trim $str]
		}
		if [regexp -nocase {Fingerprint=(.*)} $line mv str] {
			set fingerline $line
			set fingerprint [string trim $str]
		}
		if [regexp -nocase {^INFO: SELF_SIGNED=([01])} $line mv str] {
			set self_signed $str
		}
	}

	set fingerprint [string tolower $fingerprint]
	regsub -all {:} $fingerprint "-" fingerprint
	regsub -all {[\\/=]} $fingerprint "_" fingerprint

	set from [string tolower $from]
	regsub -all {[\[\]]} $from "" from
	regsub -all {^[+a-z]*://} $from "" from
	regsub -all {:} $from "-" from
	regsub -all {[\\/=]} $from "_" from
	regsub -all {[ 	]} $from "_" from

	if {$from == "" || $fingerprint == ""} {
		bell
		catch {raise .; update}
		mesg "WARNING: Error fetching Server Cert"
		after 500
		set hp [get_vncdisplay]
		set n [line_count $cert_text 1]
		fetch_dialog $cert_text $hp $hp 0 $n
		update
		after 2000
		return 0
	}

	set hp [get_vncdisplay]

	set adir [get_idir_certs ""]
	catch {file mkdir $adir}
	set adir "$adir/accepted"
	catch {file mkdir $adir}

	set crt "$adir/$from=$fingerprint.crt"

	if [file exists $crt] {
		if {$self_signed} {
			mesg "OK: Certificate found in ACCEPTED_CERTS"
			after 750
			return 1
		}
	}

	set cnt 0
	foreach f [glob -nocomplain -directory $adir "*$fingerprint*.crt"] {
		mesg "CERT: $f"
		after 150
		if {$self_signed} {
			incr cnt
		}
	}

	set oth 0
	set others [list]
	foreach f [glob -nocomplain -directory $adir "*$from*.crt"] {
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
		set sub ""
		set iss ""
		set isn -1;
		while {[gets $fh line] > -1} {
			if [regexp {^Host-Display: (.*)$} $line mv hd] {
				if {$hd == $hp || $hd == $from} {
					set same 1
				}
			}
			if [regexp {^subject:(.*)$} $line mv val] {
				set sub $val
			}
			if [regexp {^issue([0-9][0-9]*):(.*)$} $line mv in val] {
				if {$in > $isn} {
					set isn $in
					set iss $val
				}
			}
		}
		close $fh;

		if {!$self_signed} {
			if {$sub == ""} {
				set ossl [get_openssl]
				set si_txt [exec $ossl x509 -subject -issuer -noout -in $f]
				foreach line [split $si_txt "\n"] {
					if [regexp -nocase {^subject= *(.*)$} $line mv str] {
						set str [string trim $str]
						if {$str != ""} {
							set sub $str
						}
					} elseif [regexp -nocase {^issuer= *(.*)$} $line mv str] {
						set str [string trim $str]
						if {$iss != ""} {
							set iss $str
						}
					}
				}
			}
			if {$issuer != "" && $sub != ""} {
				global env
				if [info exists env(CERTDBG)] {
					puts "f: $f"
					puts "s: $sub"
					puts "i: $issuer"
					puts "==================="
				}
				if {$issuer == $sub} {
					set fb [file tail $f]
					mesg "Certificate Authority (CA) CERT: $fb"
					incr cnt
					after 500 
				}
			}
			continue
		}

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
		if {$self_signed} {
			mesg "OK: Server Certificate found in ACCEPTED_CERTS"
			after 400
		} else {
			mesg "OK: CA Certificate found in ACCEPTED_CERTS"
			after 800
		}
		return 1
	}

	set hp2 [get_vncdisplay]
	set msg "
    The Self-Signed SSL Certificate from host:

        $hp2

    Fingerprint: $fingerprint

    Subject: $subject

    is not present in the 'Accepted Certs' directory:

        $adir
%WARN
    You will need to verify on your own that this is a certificate from a
    VNC server that you trust (e.g. by checking the fingerprint with that
    sent to you by the server administrator).


    THE QUESTION: Do you want this certificate to be saved in the Accepted Certs
    directory and then used to SSL authenticate VNC servers?


    By clicking 'Inspect and maybe Save Cert' you will be given the opportunity
    to inspect the certificate before deciding to save it or not.
"

	set msg_bottom "
    Choose 'Ignore Cert for One Connection' to connect a single time to the
    server with *NO* certificate authentication.  You will see this dialog again
    the next time you connect to the same server.

    Choose 'Continue as though I saved it' to launch stunnel and the VNC viewer.
    Do this if you know the correct Certificate is in the 'Accepted Certs'
    directory.  If it is not, stunnel will fail and report 'VERIFY ERROR:...'

    Choose 'Cancel' to not connect to the VNC Server at all.
"

	set msg_ca "
    The CA-signed SSL Certificate from host:

        $hp2

    Fingerprint: $fingerprint

    Subject: $subject

    Issuer:  $issuer

    is signed by a Certificate Authority (CA) (the 'Issuer' above.)

    However, the certificate of the CA 'Issuer' is not present in the
    'Accepted Certs' directory:

        $adir

    You will need to obtain the certificate of the CA 'Issuer' via some means
    (perhaps ask the VNC server administrator for it.)  Then, after you have
    verified that the CA certificate is one that you trust, import the 
    certificate via Certs -> Import Certificate.  Be sure to select to also
    save it to the Accepted Certs directory so it will automatically be used.
"
	set msg "$msg$msg_bottom"
	set msg_ca "$msg_ca$msg_bottom"

	if {!$self_signed} {
		set msg $msg_ca
	}

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
	if {!$self_signed} {
		set n [expr $n + 2]
	} else {
		set n [expr $n + 1]
	}
	if [small_height] {
		if {$n > 26} {
			set n 26
		}
	}
	toplev .acert
	scroll_text .acert.f 83 $n

	button .acert.inspect -text "Inspect and maybe Save Cert ..." -command "destroy .acert; set accept_cert_dialog 1"
	button .acert.accept  -text "Ignore Cert for One Connection  " -command "destroy .acert; set accept_cert_dialog 2"
	button .acert.continue -text "Continue as though I saved it     " -command "destroy .acert; set accept_cert_dialog 3"
	button .acert.cancel -text "Cancel"   -command "destroy .acert; set accept_cert_dialog 0"

	wm title .acert "Unrecognized SSL Cert!"

	.acert.f.t insert end $msg

	pack .acert.cancel .acert.continue .acert.accept .acert.inspect -side bottom -fill x
	pack .acert.f -side top -fill both -expand 1

	if {! $self_signed} {
		catch {.acert.inspect configure -state disabled}
	}

	center_win .acert

	global accept_cert_dialog
	set accept_cert_dialog ""

	jiggle_text .acert.f.t

	tkwait window .acert

	if {$accept_cert_dialog == 2} {
		set skip_verify_accepted_certs 1
		return 1
	}
	if {$accept_cert_dialog == 3} {
		return 1
	}
	if {$accept_cert_dialog != 1} {
		return 0
	}

	global accepted_cert_dialog_in_progress
	set accepted_cert_dialog_in_progress 1

	global fetch_cert_filename
	set fetch_cert_filename $crt

	global do_save_saved_it
	set do_save_saved_it 0
	global do_save_saved_hash_it
	set do_save_saved_hash_it 0

	fetch_dialog $cert_text $hp $hp 1 47 
	update; after 150

	catch {tkwait window .fetch}
	update; after 250
	catch {tkwait window .scrt}
	update; after 250
	if [winfo exists .scrt] {
		catch {tkwait window .scrt}
	}

	set fetch_cert_filename ""
	set accepted_cert_dialog_in_progress 0

	if {!$do_save_saved_hash_it} {
		save_hash $crt $adir $hp $fingerline $from $fingerprint $subject_issuer
	}

	if {$do_save_saved_it} {
		return 1
	} else {
		return 0
	}
}

proc save_hash {crt adir hp fingerline from fingerprint {subject_issuer ""}} {
	if ![file exists $crt] {
		return
	}
	set ossl [get_openssl]
	set hash [exec $ossl x509 -hash -noout -in $crt]
	set hash [string trim $hash]
	if [regexp {^([0-9a-f][0-9a-f]*)} $hash mv h] {
		set hashfile "$adir/$h.0"
		set hn "$h.0"
		if [file exists $hashfile] {
			set hashfile "$adir/$h.1"
			set hn "$h.1"
			if [file exists $hashfile] {
				set hashfile "$adir/$h.2"
				set hn "$h.2"
			}
		}
		set fh [open $crt "a"]
		if {$fh != ""} {
			puts $fh ""
			puts $fh "SSVNC-info:"
			puts $fh "Host-Display: $hp"
			puts $fh "$fingerline"
			puts $fh "hash-filename: $hn"
			puts $fh "full-filename: $from=$fingerprint.crt"
			puts -nonewline $fh $subject_issuer
			close $fh
		}
		catch {file copy -force $crt $hashfile}
		if [file exists $hashfile] {
			return 1
		}
	}
}

proc tpid {} {
	global is_windows
	set p ""

	if {!$is_windows} {
		catch {set p [exec sh -c {echo $$}]}
	}
	if {$p == ""} {
		set p [pid];
	}
	append p [clock clicks]
	return $p
}

proc repeater_proxy_check {proxy} {
	if [regexp {^repeater://.*\+ID:[0-9]} $proxy] {
		global env rpc_m1 rpc_m2 
		if {![info exists rpc_m1]} {
			set rpc_m1 0
			set rpc_m2 0
		}
		set force 0
		if [info exists env(REPEATER_FORCE)] {
			if {$env(REPEATER_FORCE) != "" && $env(REPEATER_FORCE) != "0"} {
				# no longer makes a difference.
				set force 1
			}
		}
		global use_listen ultra_dsm
		if {! $use_listen} {
			if {$ultra_dsm} {
				return 1;
			} else {
				if {0} {
					mesg "WARNING: repeater:// ID:nnn proxy might need Listen Mode"
					incr rpc_m1
					if {$rpc_m1 <= 2} {
						after 1000
					} else {
						after 200
					}
				}
				if {0} {
					# no longer required by x11vnc (X11VNC_DISABLE_SSL_CLIENT_MODE)
					bell
					mesg "ERROR: repeater:// ID:nnn proxy must use Listen Mode"
					after 1000
					return 0
				}
			}
		}
		global always_verify_ssl
		if [info exists always_verify_ssl] {
			if {$always_verify_ssl} {
				mesg "WARNING: repeater:// ID:nnn Verify All Certs may fail"
				incr rpc_m2
				if {$rpc_m2 == 1} {
					after 1500
				} elseif {$rpc_m2 == 2} {
					after 500
				} else {
					after 200
				}
			}
		}
	}
	return 1
}

proc fini_unixpw {} {
	global named_pipe_fh unixpw_tmp
	
	if {$named_pipe_fh != ""} {
		catch {close $named_pipe_fh}
	}
	if {$unixpw_tmp  != ""} {
		catch {file delete $unixpw_tmp}
	}
}

proc init_unixpw {hp} {
	global use_unixpw unixpw_username unixpw_passwd
	global named_pipe_fh unixpw_tmp env
	
	set named_pipe_fh ""
	set unixpw_tmp ""

	if {$use_unixpw} {
		set name $unixpw_username
		set env(SSVNC_UNIXPW) ""
		if {$name == ""} {
			regsub {^.*://} $hp "" hp
			set hptmp [get_ssh_hp $hp]
			if [regexp {^(.*)@} $hptmp mv m1] {
				set name $m1
			}
		}
		if {$name == ""} {
			if [info exists env(USER)] {
				set name $env(USER)
			}
		}
		if {$name == ""} {
			if [info exists env(LOGNAME)] {
				set name $env(LOGNAME)
			}
		}
		if {$name == ""} {
			set name [exec whoami]
		}
		if {$name == ""} {
			set name "unknown"
		}

		set tmp "/tmp/unixpipe.[tpid]"
		set tmp [mytmp $tmp]
		# need to make it a pipe
		catch {file delete $tmp}
		if {[file exists $tmp]} {
			mesg "file still exists: $tmp"
			bell
			return
		}

		catch {exec mknod $tmp p}
		set fh ""
		if {! [file exists $tmp]} {
			catch {set fh [open $tmp "w"]}
		} else {
			catch {set fh [open $tmp "r+"]}
			set named_pipe_fh $fh
		}
		catch {exec chmod 600 $tmp}
		if {! [file exists $tmp]} {
			mesg "cannot create: $tmp"
			if {$named_pipe_fh != ""} {catch close $named_pipe_fh}
			bell
			return
		}
		#puts [exec ls -l $tmp]
		set unixpw_tmp $tmp
		puts $fh $name
		puts $fh $unixpw_passwd
		if {$named_pipe_fh != ""} {
			flush $fh
		} else {
			close $fh
		}
		exec sh -c "sleep 60; /bin/rm -f $tmp" &
		if {$unixpw_passwd == ""} {
			set env(SSVNC_UNIXPW) "."
		} else {
			set env(SSVNC_UNIXPW) "rm:$tmp"
		}
	} else {
		if [info exists env(SSVNC_UNIXPW)] {
			set env(SSVNC_UNIXPW) ""
		}
	}
}

proc check_for_listen_ssl_cert {} {
	global mycert use_listen use_ssh ultra_dsm
	if {! $use_listen} {
		return 1
	}
	if {$use_ssh} {
		return 1
	}
	if {$ultra_dsm} {
		return 1
	}
	if {$mycert != ""} {
		return 1
	}

	set name [get_idir_certs ""]
	set name "$name/listen.pem"
	if {[file exists $name]} {
		set mycert $name
		mesg "Using Listen Cert: $name"
		after 700
		return 1
	}

	set title "SSL Listen requires MyCert";
	set msg "In SSL Listen mode a cert+key is required, but you have not specified  'MyCert'.\n\nCreate a cert+key 'listen' now?"
	set reply [tk_messageBox -type okcancel -default ok -icon warning -message $msg -title $msg]
	if {$reply == "cancel"} {
		return 0
	}
	create_cert $name
	tkwait window .ccrt
	if {[file exists $name]} {
		set mycert $name
		mesg "Using Listen Cert: $name"
		after 700
		return 1
	}
	return 0
}

proc listen_verify_all_dialog {hp} {
	global use_listen always_verify_ssl
	global did_listen_verify_all_dialog
	global svcert
	global sshssl_sw ultra_dsm

	if {!$use_listen} {
		return 1
	}
	if {!$always_verify_ssl} {
		return 1
	}
	if {$svcert != ""} {
		return 1
	}
	if {$ultra_dsm} {
		return 1
	}
	if [regexp -nocase {^vnc://} $hp] {
		return 1
	}
	if [info exists sshssl_sw] {
		if {$sshssl_sw == "none"} {
			return 1
		}
		if {$sshssl_sw == "ssh"} {
			return 1
		}
	}
	if [info exists did_listen_verify_all_dialog] {
		return 1
	}

	toplev .lvd
	global help_font
	wm title .lvd "Verify All Certs for Reverse Connections"
	eval text .lvd.t -width 55 -height 22 $help_font
	.lvd.t insert end {
  Information: 

    You have the 'Verify All Certs' option enabled
    in Reverse VNC Connections (-LISTEN) mode.
   
    For this to work, you must have ALREADY saved
    the remote VNC Server's Certificate to the
    'Accepted Certs' directory.  Otherwise the
    incoming Reverse connection will be rejected.
    
    You can save the Server's Certificate by using
    the 'Import Certificate' dialog or on Unix
    and MacOSX by pressing 'Fetch Cert' and then
    have the Server make an initial connection.

    If you do not want to save the certificate of
    the VNC Server making the Reverse connection,
    you must disable 'Verify All Certs' (note that
    this means the server authenticity will not be
    checked.)
}

	button .lvd.ok   -text OK   -command {destroy .lvd}
	button .lvd.ok2  -text OK   -command {destroy .lvd}
	button .lvd.disable   -text "Disable 'Verify All Certs'"  -command {set always_verify_ssl 0; destroy .lvd}
	global uname
	if {$uname == "Darwin"} {
		pack .lvd.t .lvd.ok2 .lvd.disable .lvd.ok -side top -fill x
	} else {
		pack .lvd.t .lvd.disable .lvd.ok -side top -fill x
	}
	center_win .lvd
	update

	tkwait window .lvd
	update
	after 50
	update

	set did_listen_verify_all_dialog 1
	return 1
}

proc reset_stunnel_extra_opts {} {
	global stunnel_extra_opts0 stunnel_extra_svc_opts0 env
	global ssvnc_multiple_listen0
	if {$stunnel_extra_opts0 != "none"} {
		set env(STUNNEL_EXTRA_OPTS) $stunnel_extra_opts0
	}
	if {$stunnel_extra_svc_opts0 != "none"} {
		set env(STUNNEL_EXTRA_SVC_OPTS) $stunnel_extra_svc_opts0
	}
	set env(SSVNC_LIM_ACCEPT_PRELOAD) ""
	if {$ssvnc_multiple_listen0 != "none"} {
		set env(SSVNC_MULTIPLE_LISTEN) $ssvnc_multiple_listen0
	}
	set env(SSVNC_ULTRA_DSM) ""
	set env(SSVNC_TURBOVNC) ""
	catch { unset env(VNCVIEWER_NO_PIPELINE_UPDATES) }
	catch { unset env(VNCVIEWER_NOTTY) }
	catch { unset env(SSVNC_ACCEPT_POPUP)    }
	catch { unset env(SSVNC_ACCEPT_POPUP_SC) }
	catch { unset env(SSVNC_KNOWN_HOSTS_FILE)    }
}

proc maybe_add_vencrypt {proxy hp} {
	global vencrypt_detected server_vencrypt
	set vpd ""
	if {$vencrypt_detected != ""} {
		set vpd $vencrypt_detected
		set vencrypt_detected ""
	} elseif {$server_vencrypt} {
		set vpd [get_vencrypt_proxy $hp]
	}
	if {$vpd != ""} {
		mesg "vencrypt proxy: $vpd"
		if {$proxy != ""} {
			set proxy "$proxy,$vpd"
		} else {
			set proxy "$vpd"
		}
	}
	return $proxy
}

proc no_certs_tutorial_mesg {} {
	global svcert crtdir
	global server_anondh 
	global always_verify_ssl

	set doit 0
	if {!$always_verify_ssl} {
		if {$svcert == ""} {
			if {$crtdir == "" || $crtdir == "ACCEPTED_CERTS"} {
				set doit 1
			}
		}
	} elseif {$server_anondh} {
		set doit 1
	}
	if {$doit} {
		mesg "INFO: without Certificate checking man-in-the-middle attack is possible."
	} else {
		set str ""
		catch {set str [.l cget -text]}
		if {$str != "" && [regexp {^INFO: without Certificate} $str]} {
			mesg ""
		}
	}
}

proc vencrypt_tutorial_mesg {} {
	global use_ssh use_sshssl use_listen
	global server_vencrypt no_probe_vencrypt
	global ultra_dsm 

	set m ""
	if {$use_ssh} {
		;
	} elseif {$server_vencrypt} {
		;
	} elseif {$ultra_dsm} {
		;
	} elseif {$use_listen} {
		set m "No VeNCrypt Auto-Detection:  Listen mode."
	} elseif {$use_sshssl} {
		set m "No VeNCrypt Auto-Detection:  SSH+SSL mode."
	} elseif {$no_probe_vencrypt} {
		set m "No VeNCrypt Auto-Detection:  Disabled."
	}
	if {$m != ""} {
		mesg $m
		after 1000
	}
	return $m

	#global svcert always_verify_ssl
	#$svcert != "" || !$always_verify_ssl
	#	set m "No VeNCrypt Auto-Detection: 'Verify All Certs' disabled"
}

proc launch_unix {hp} {
	global smb_redir_0 smb_mounts env
	global vncauth_passwd use_unixpw unixpw_username unixpw_passwd
	global ssh_only ts_only use_x11cursor use_nobell use_rawlocal use_notty use_popupfix ssvnc_scale ssvnc_escape
	global ssvnc_encodings ssvnc_extra_opts

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
	if {[regexp {^rsh:/?/?} $hp]} {
		set use_ssl 0
		set use_ssh 1
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

	listen_verify_all_dialog $hp

	if {! $do_direct} {
		if {! [check_for_listen_ssl_cert]} {
			return
		}
	}

	global stunnel_extra_opts0 stunnel_extra_svc_opts0
	set stunnel_extra_opts0 ""
	set stunnel_extra_svc_opts0 ""
	global ssvnc_multiple_listen0
	set ssvnc_multiple_listen0 ""

	if {[regexp -nocase {sslrepeater://} $hp]} {
		if {$disable_ssl_workarounds} {
			set disable_ssl_workarounds 0
			mesg "Disabling SSL workarounds for 'UVNC Single Click III Bug'"
			after 400
		}
	}

	if [info exists env(STUNNEL_EXTRA_OPTS)] {
		set stunnel_extra_opts0 $env(STUNNEL_EXTRA_OPTS)
		if {$disable_ssl_workarounds} {
			if {$disable_ssl_workarounds_type == "none"} {
				;
			} elseif {$disable_ssl_workarounds_type == "noempty"} {
				set env(STUNNEL_EXTRA_OPTS) "$env(STUNNEL_EXTRA_OPTS)\noptions = DONT_INSERT_EMPTY_FRAGMENTS"
			}
		} else {
			set env(STUNNEL_EXTRA_OPTS) "$env(STUNNEL_EXTRA_OPTS)\noptions = ALL"
		}
	} else {
		if {$disable_ssl_workarounds} {
			if {$disable_ssl_workarounds_type == "none"} {
				;
			} elseif {$disable_ssl_workarounds_type == "noempty"} {
				set env(STUNNEL_EXTRA_OPTS) "options = DONT_INSERT_EMPTY_FRAGMENTS"
			}
		} else {
			set env(STUNNEL_EXTRA_OPTS) "options = ALL"
		}
	}
	if {$stunnel_local_protection && ! $use_listen} {
		if {$stunnel_local_protection_type == "ident"} {
			set user ""
			if {[info exists env(USER)]} {
				set user $env(USER)
			} elseif {[info exists env(LOGNAME)]} {
				set user $env(USER)
			}
			if {$user != ""} {
				if [info exists env(STUNNEL_EXTRA_SVC_OPTS)] {
					set stunnel_extra_svc_opts0 $env(STUNNEL_EXTRA_SVC_OPTS)
					set env(STUNNEL_EXTRA_SVC_OPTS) "$env(STUNNEL_EXTRA_SVC_OPTS)\nident = $user"
				} else {
					set env(STUNNEL_EXTRA_SVC_OPTS) "ident = $user"
				}
			}
		} elseif {$stunnel_local_protection_type == "exec"} {
			if [info exists env(STUNNEL_EXTRA_SVC_OPTS)] {
				set stunnel_extra_svc_opts0 $env(STUNNEL_EXTRA_SVC_OPTS)
				set env(STUNNEL_EXTRA_SVC_OPTS) "$env(STUNNEL_EXTRA_SVC_OPTS)\n#stunnel-exec"
			} else {
				set env(STUNNEL_EXTRA_SVC_OPTS) "#stunnel-exec"
			}
		}
	}
	if {$ultra_dsm} {
		if {$ultra_dsm_type == "securevnc"} {
			;
		} elseif {![file exists $ultra_dsm_file] && ![regexp {pw=} $ultra_dsm_file]} {
			mesg "DSM key file does exist: $ultra_dsm_file" 
			bell
			after 1000
			return
		}
		global vncauth_passwd
		if {$ultra_dsm_file == "pw=VNCPASSWORD" || $ultra_dsm_file == "pw=VNCPASSWD"} {
			if {![info exists vncauth_passwd] || $vncauth_passwd == ""} {
				mesg "For DSM pw=VNCPASSWD you must supply the VNC Password" 
				bell
				after 1000
				return
			}
			if [regexp {'} $vncauth_passwd] {
				mesg "For DSM pw=VNCPASSWD password must not contain single quotes." 
				bell
				after 1000
				return
			}
		}
		set dsm "ultravnc_dsm_helper "
		if {$ultra_dsm_noultra} {
			append dsm "noultra:"
		}
		if {$use_listen} {
			append dsm "rev:"
		}
		if {$ultra_dsm_type == "guess"} {
			append dsm "."
		} else {
			append dsm $ultra_dsm_type
		}
		if {$ultra_dsm_noultra} {
			if {$ultra_dsm_salt != ""} {
				append dsm "@$ultra_dsm_salt"
			}
		}
		if {$ultra_dsm_file == "pw=VNCPASSWORD" || $ultra_dsm_file == "pw=VNCPASSWD"} {
			append dsm " pw='$vncauth_passwd'"
		} else {
			if {$ultra_dsm_file == "" && $ultra_dsm_type == "securevnc"} {
				append dsm " none"
			} else {
				append dsm " $ultra_dsm_file"
			}
		}
		set env(SSVNC_ULTRA_DSM) $dsm
	}
	if {$multiple_listen && $use_listen} {
		if [info exists env(SSVNC_MULTIPLE_LISTEN)] {
			set ssvnc_multiple_listen0 $env(SSVNC_MULTIPLE_LISTEN)
		}
		set env(SSVNC_MULTIPLE_LISTEN) "1"
	}

	if {$use_ssh} {
		;
	} elseif {$use_sshssl} {
		;
	} elseif {$use_ssl} {
		set prox  [get_ssh_proxy $hp]
		if {$prox != "" && [regexp {@} $prox]} {
			mesg "Error: proxy contains '@'  Did you mean to use SSH mode?"
			bell
			return
		}
		if [regexp {@} $hp] {
			mesg "Error: host contains '@'  Did you mean to use SSH mode?"
			bell
			return
		}
	}

	if {$use_ssh || $use_sshssl} {
		if {$ssh_local_protection} {
			if {![info exists env(LIM_ACCEPT)]} {
				set env(LIM_ACCEPT) 1
			}
			if {![info exists env(LIM_ACCEPT_TIME)]} {
				set env(LIM_ACCEPT_TIME) 35
			}
			set env(SSVNC_LIM_ACCEPT_PRELOAD) "lim_accept.so"
			mesg "SSH LIM_ACCEPT($env(LIM_ACCEPT),$env(LIM_ACCEPT_TIME)): lim_accept.so"
			after 700
		}
		if {$skip_ssh || $ultra_dsm} {
			set cmd "ss_vncviewer"
		} elseif {$use_ssh} {
			set cmd "ss_vncviewer -ssh"
		} else {
			set cmd "ss_vncviewer -sshssl"
			if {$mycert != ""} {
				set cmd "$cmd -mycert '$mycert'"
			}
			if {$crlfil != ""} {
				set cmd "$cmd -crl '$crlfil'"
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
		if {$ssh_local_protection} {
			regsub {ss_vncviewer} $cmd "ssvnc_cmd" cmd
		}
		set hpnew  [get_ssh_hp $hp]
		set proxy  [get_ssh_proxy $hp]
		set sshcmd [get_ssh_cmd $hp]

		if {$use_sshssl} {
			if {!$do_direct} {
				set proxy [maybe_add_vencrypt $proxy $hp]
			}
		}

		if {$ts_only} {
			regsub {:0$} $hpnew "" hpnew 
			if {$proxy == ""} {
				# XXX host_part
				if {[regexp {^([^:]*):([0-9][0-9]*)$} $hpnew mv sshhst sshpt]} {
					set proxy "$sshhst:$sshpt"
					set hpnew "localhost"
				}
			} else {
				if {![regexp {,} $proxy]} {
					if {$hpnew != "localhost"} {
						set proxy "$proxy,$hpnew"
						set hpnew "localhost"
					}
				}
			}
		}

#puts hp=$hp
#puts hpn=$hpnew
#puts pxy=$proxy
#puts cmd=$sshcmd

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
		if {$sshcmd != "SHELL" && [regexp -nocase {x11vnc} $sshcmd]} {
			global use_cups cups_x11vnc cups_remote_port
			global cups_remote_smb_port
			global use_sound sound_daemon_x11vnc sound_daemon_remote_port 
			global ts_only
			if {$ts_only} {
				set cups_x11vnc 1
				set sound_daemon_x11vnc 1
			}
			if {$use_cups && $cups_x11vnc && $cups_remote_port != ""} {
				set crp $cups_remote_port
				if {$ts_only} {
					set cups_remote_port [rand_port]
					set crp "DAEMON-$cups_remote_port"
				}
				set sshcmd "$sshcmd -env FD_CUPS=$crp"
			}
			if {$use_cups && $cups_x11vnc && $cups_remote_smb_port != ""} {
				set csp $cups_remote_smb_port
				if {$ts_only} {
					set cups_remote_smb_port [rand_port]
					set csp "DAEMON-$cups_remote_smb_port"
				}
				set sshcmd "$sshcmd -env FD_SMB=$csp"
			}
			if {$use_sound && $sound_daemon_x11vnc && $sound_daemon_remote_port != ""} {
				set srp $sound_daemon_remote_port
				if {$ts_only} {
					set sound_daemon_remote_port [rand_port]
					set srp "DAEMON-$sound_daemon_remote_port"
				}
				set sshcmd "$sshcmd -env FD_ESD=$srp"
			}
		}

		if {$sshcmd == "SHELL"} {
			set env(SS_VNCVIEWER_SSH_CMD) {$SHELL}
			set env(SS_VNCVIEWER_SSH_ONLY) 1
		} elseif {$setup_cmds != ""} {
			if {$sshcmd == ""} {
				set sshcmd "sleep 15"
			}
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
		} else {
			# hmm we used to have it off... why?
			# ssh typing response?
			set env(SS_VNCVIEWER_USE_C) 1
		}
		if {$sshcmd == "SHELL"} {
			set env(SS_VNCVIEWER_SSH_ONLY) 1
			if {$proxy == ""} {
				set hpt $hpnew
				# XXX host_part
				regsub {:[0-9][0-9]*$} $hpt "" hpt
				set cmd "$cmd -proxy '$hpt'"
			}
			set geometry [xterm_center_geometry]
			if {$pk_hp == ""} {
				set pk_hp $hp
			}
			if {! $did_port_knock} {
				if {! [do_port_knock $pk_hp start]} {
					reset_stunnel_extra_opts
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
			reset_stunnel_extra_opts
			return
		}
	} else {
		set cmd "ssvnc_cmd"
		set hpnew  [get_ssh_hp $hp]
		set proxy  [get_ssh_proxy $hp]

		if {!$do_direct && ![repeater_proxy_check $proxy]} {
			reset_stunnel_extra_opts
			return
		}

		if {! $do_direct && ! $ultra_dsm && ![regexp -nocase {ssh://} $hpnew]} {
			set did_check 0
			if {$mycert != ""} {
				set cmd "$cmd -mycert '$mycert'"
			}
			if {$crlfil != ""} {
				set cmd "$cmd -crl '$crlfil'"
			}
			if {$svcert != ""} {
				set cmd "$cmd -verify '$svcert'"
			} elseif {$crtdir != ""} {
				if {$crtdir == "ACCEPTED_CERTS"} {
					global skip_verify_accepted_certs
					set skip_verify_accepted_certs 0

					set did_check 1
					if {! [check_accepted_certs 0]} {
						reset_stunnel_extra_opts
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
			if {! $did_check} {
				check_accepted_certs 1
			}
		}

		if {!$do_direct} {
			set proxy [maybe_add_vencrypt $proxy $hp]
		}

		if {$proxy != ""} {
			set cmd "$cmd -proxy '$proxy'"
		}
		set hp $hpnew
		if [regexp {^.*@} $hp match] {
			catch {raise .; update}
			mesg "Trimming \"$match\" from hostname"
			after 700
			regsub {^.*@} $hp "" hp
		}
		if [regexp {@} $proxy] {
			bell
			catch {raise .; update}
			mesg "WARNING: SSL proxy contains \"@\" sign"
			after 1500
		}
	}

	global anon_dh_detected
	if {$anon_dh_detected || $server_anondh} {
		if {!$do_direct} {
			set cmd "$cmd -anondh"
		}
		set anon_dh_detected 0
	}
	if {$use_alpha} {
		set cmd "$cmd -alpha"
	}
	if {$use_send_clipboard} {
		set cmd "$cmd -sendclipboard"
	}
	if {$use_send_always} {
		set cmd "$cmd -sendalways"
	}
	if {$use_turbovnc} {
		set env(SSVNC_TURBOVNC) 1
	}
	if {$disable_pipeline} {
		set env(VNCVIEWER_NO_PIPELINE_UPDATES) 1
	}
	if {$ssh_known_hosts_filename != ""} {
		set env(SSVNC_KNOWN_HOSTS_FILE) $ssh_known_hosts_filename
	}
	if {$use_grab} {
		set cmd "$cmd -grab"
	}
	if {$use_x11cursor} {
		set cmd "$cmd -x11cursor"
	}
	if {$use_nobell} {
		set cmd "$cmd -nobell"
	}
	if {$use_rawlocal} {
		set cmd "$cmd -rawlocal"
	}
	if {$use_notty} {
		set env(VNCVIEWER_NOTTY) 1
	}
	if {$use_popupfix} {
		set cmd "$cmd -popupfix"
	}
	if {$ssvnc_scale != ""} {
		set cmd "$cmd -scale '$ssvnc_scale'"
	}
	if {$ssvnc_escape != ""} {
		set cmd "$cmd -escape '$ssvnc_escape'"
	}
	if {$ssvnc_encodings != ""} {
		set cmd "$cmd -ssvnc_encodings '$ssvnc_encodings'"
	}
	if {$ssvnc_extra_opts != ""} {
		set cmd "$cmd -ssvnc_extra_opts '$ssvnc_extra_opts'"
	}
	if {$rfbversion != ""} {
		set cmd "$cmd -rfbversion '$rfbversion'"
	}
	if {$vncviewer_realvnc4} {
		set cmd "$cmd -realvnc4"
	}
	if {$use_listen} {
		set cmd "$cmd -listen"
		if {$listen_once} {
			set cmd "$cmd -onelisten"
		}
		if {$listen_accept_popup} {
			if {$listen_accept_popup_sc} {
				set env(SSVNC_ACCEPT_POPUP_SC) 1
			} else {
				set env(SSVNC_ACCEPT_POPUP) 1
			}
		}
	}

	global darwin_cotvnc
	if {$darwin_cotvnc} {
		set env(DARWIN_COTVNC) 1
	} else {
		if [info exists env(DISPLAY)] {
			if {$env(DISPLAY) != ""} {
				set env(DARWIN_COTVNC) 0
			} else {
				set env(DARWIN_COTVNC) 1
			}
		} else {
			set env(DARWIN_COTVNC) 1
		}
	}

	set do_vncspacewrapper 0
	if {$change_vncviewer && $change_vncviewer_path != ""} {
		set path [string trim $change_vncviewer_path]
		if [regexp {^["'].} $path]  {	# "
			set tmp "/tmp/vncspacewrapper.[tpid]"
			set tmp [mytmp $tmp]
			set do_vncspacewrapper 1
			if {0} {
				catch {file delete $tmp}
				if {[file exists $tmp]} {
					catch {destroy .c}
					mesg "file still exists: $tmp"
					bell
					reset_stunnel_extra_opts
					return
				}
			}
			catch {set fh [open $tmp "w"]}
			catch {exec chmod 700 $tmp}
			if {! [file exists $tmp]} {
				catch {destroy .c}
				mesg "cannot create: $tmp"
				bell
				reset_stunnel_extra_opts
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
	if {$realvnc4} {
		set cmd "$cmd -realvnc4"
	}

	set cmd "$cmd $hp"

	set passwdfile ""
	if {$vncauth_passwd != ""} {
		global use_listen
		set footest [mytmp /tmp/.check.[tpid]]
		catch {file delete $footest}
		global mktemp
		set passwdfile "/tmp/.vncauth_tmp.[tpid]"
		if {$mktemp == ""} {
			set passwdfile "$env(SSVNC_HOME)/.vncauth_tmp.[tpid]"
		}
		
		set passwdfile [mytmp $passwdfile]
		catch {exec vncstorepw $vncauth_passwd $passwdfile}
		catch {exec chmod 600 $passwdfile}
		if {$use_listen} {
			global env
			set env(SS_VNCVIEWER_RM) $passwdfile
		} else {
			if {$darwin_cotvnc} {
				catch {exec sh -c "sleep 60; rm $passwdfile 2>/dev/null" &}
			} else {
				catch {exec sh -c "sleep 20; rm $passwdfile 2>/dev/null" &}
			}
		}
		if {$darwin_cotvnc} {
			set cmd "$cmd --PasswordFile $passwdfile"
		} elseif {$flavor == "unknown"} {
			;
		} else {
			set cmd "$cmd -passwd $passwdfile"
		}
	}

	if {$use_viewonly} {
		if {$darwin_cotvnc} {
			set cmd "$cmd --ViewOnly"
		} elseif {$flavor == "unknown"} {
			;
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
		} elseif {$flavor == "unknown"} {
			if [regexp {vinagre} $change_vncviewer_path] {
				set cmd "$cmd -f"
			}
		} else {
			set cmd "$cmd -fullscreen"
		}
	}
	if {$use_bgr233} {
		if {$realvnc4} {
			set cmd "$cmd -lowcolourlevel 1"
		} elseif {$flavor == "ultravnc"} {
			set cmd "$cmd /8bit"
		} elseif {$flavor == "ultravnc"} {
			;
		} elseif {$flavor == "unknown"} {
			;
		} else {
			set cmd "$cmd -bgr233"
		}
	}
	if {$use_nojpeg} {
		if {$darwin_cotvnc} {
			;
		} elseif {$flavor == "ultravnc"} {
			;
		} elseif {$flavor == "unknown"} {
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
		} elseif {$flavor == "unknown"} {
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
		} elseif {$flavor == "unknown"} {
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
		} elseif {$flavor == "unknown"} {
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
		} elseif {$flavor == "unknown"} {
			;
		} elseif {$realvnc4} {
			set cmd "$cmd -preferredencoding zrle"
		} else {
			set cmd "$cmd -encodings 'copyrect tight zrle zlib hextile'"
		}
	}

	global ycrop_string
	global sbwid_string
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
	}
	if {[info exists sbwid_string] && $sbwid_string != ""}  {
		set t $sbwid_string
		set env(VNCVIEWER_SBWIDTH) $sbwid_string
		if {$t != ""} {
			set env(VNCVIEWER_SBWIDTH) $t
		}
	}

	catch {destroy .o}
	catch {destroy .oa}
	catch {destroy .os}
	update

	if {$use_sound && $sound_daemon_local_start && $sound_daemon_local_cmd != ""} {
		mesg "running: $sound_daemon_local_cmd"
		global sound_daemon_local_pid
		set sound_daemon_local_pid ""
		#exec sh -c "$sound_daemon_local_cmd " >& /dev/null </dev/null &
		set sound_daemon_local_pid [exec sh -c "echo \$\$; exec $sound_daemon_local_cmd </dev/null 1>/dev/null 2>/dev/null &"]
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
			reset_stunnel_extra_opts
			return
		}
		set did_port_knock 1
	}

	init_unixpw $hp

	if {! $do_direct} {
		vencrypt_tutorial_mesg
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
	set m "Done. You Can X-out or Ctrl-C this Terminal if you like.  Use Ctrl-\\\\ to pause."
	global uname
	if {$uname == "Darwin"} {
		regsub {X-out or } $m "" m
	}
	set te "set -xv; "
	if {$ts_only} {
		set te ""
	}

	global extra_sleep
	set ssvnc_extra_sleep_save ""
	if {$extra_sleep != ""} {
		if [info exists env(SSVNC_EXTRA_SLEEP)] {
			set ssvnc_extra_sleep_save $env(SSVNC_EXTRA_SLEEP)
		}
		set env(SSVNC_EXTRA_SLEEP) $extra_sleep
	}

	set sstx "SSL/SSH VNC Viewer"
	set hptx $hp
	global use_listen
	if {$use_listen} {
		set sstx "SSVNC"
		set hptx "$hp (Press Ctrl-C to Stop Listening)"
	}


	set s1 5
	set s2 4
	if [info exists env(SSVNC_FINISH_SLEEP)] {
		set s1 $env(SSVNC_FINISH_SLEEP);
		set s2 $s1
	}

	unix_terminal_cmd $geometry "$sstx $hptx" \
	"$te$cmd; set +xv; ulimit -c 0; trap 'printf \"Paused. Press Enter to exit:\"; read x' QUIT; echo; echo $m; echo; echo sleep $s1; echo; sleep $s2" 0 $xrm1 $xrm2 $xrm3

	set env(SS_VNCVIEWER_SSH_CMD) ""
	set env(SS_VNCVIEWER_USE_C) ""

	if {$extra_sleep != ""} {
		if {$ssvnc_extra_sleep_save != ""} {
			set env(SSVNC_EXTRA_SLEEP) $ssvnc_extra_sleep_save
		} else {
			catch {unset env(SSVNC_EXTRA_SLEEP)}
		}
	}

	if {$use_sound && $sound_daemon_local_kill && $sound_daemon_local_cmd != ""} {
		# XXX need to kill just one...
		set daemon [string trim $sound_daemon_local_cmd]
		regsub {^gw[ \t]*} $daemon "" daemon
		regsub {[ \t].*$} $daemon "" daemon
		regsub {^.*/} $daemon "" daemon
		mesg "killing sound daemon: $daemon"
		global sound_daemon_local_pid
		if {$sound_daemon_local_pid != ""} {
#puts pid=$sound_daemon_local_pid
			catch {exec sh -c "kill $sound_daemon_local_pid"  >/dev/null 2>/dev/null </dev/null &}
			incr sound_daemon_local_pid
			catch {exec sh -c "kill $sound_daemon_local_pid"  >/dev/null 2>/dev/null </dev/null &}
			set sound_daemon_local_pid ""
		} elseif {$daemon != ""} {
			catch {exec sh -c "killall $daemon"  >/dev/null 2>/dev/null </dev/null &}
			catch {exec sh -c "pkill -x $daemon" >/dev/null 2>/dev/null </dev/null &}
		}
	}
	if {$passwdfile != ""} {
		catch {file delete $passwdfile}
	}
	wm deiconify .
	mac_raise
	mesg "Disconnected from $hp"
	if {[regexp {FINISH} $port_knocking_list]} {
		do_port_knock $pk_hp finish
	}

	reset_stunnel_extra_opts

	fini_unixpw
}

proc kill_stunnel {pids} {
	set count 0
	foreach pid $pids {
		mesg "killing STUNNEL pid: $pid"
		winkill $pid
		if {$count == 0} {
			after 600
		} else {
			after 300
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
		set m 0
		if [regexp -nocase {stunnel} $line] {
			set m 1
		} elseif [regexp -nocase {connect_br} $line] {
			set m 1
		}
		if {$m} {
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
	global env

	if {[info exists env(SSVNC_NO_DELETE)]} {
		return
	}
	
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

proc to_sshonly {} {
	global ssh_only ts_only env
	global showing_no_encryption
	#if {$showing_no_encryption} {
	#	toggle_no_encryption
	#}
	if {$ssh_only && !$ts_only} {
		return
	}
	if {[info exists env(SSVNC_TS_ALWAYS)]} {
		return
	}
	set ssh_only 1
	set ts_only 0
	
	set t "SSH VNC Viewer"
	wm title . $t
	catch {pack forget .f4}
	catch {pack forget .b.certs}
	catch {.l configure -text $t}

	global vncdisplay vncauth_passwd unixpw_username vncproxy remote_ssh_cmd
	set vncdisplay ""
	set vncauth_passwd ""
	set unixpw_username ""
	set vncproxy ""
	set remote_ssh_cmd ""

	set_defaults
}

proc toggle_tsonly {} {
	global ts_only env
	if {$ts_only} {
		if {![info exists env(SSVNC_TS_ALWAYS)]} {
			to_ssvnc
		}
	} else {
		to_tsonly
	}
}

proc toggle_sshonly {} {
	global ssh_only env
	if {$ssh_only} {
		to_ssvnc
	} else {
		to_sshonly
	}
}

proc to_tsonly {} {
	global ts_only
	global showing_no_encryption
	#if {$showing_no_encryption} {
	#	toggle_no_encryption
	#}
	if {$ts_only} {
		return
	}
	set ts_only 1
	set ssh_only 1
	
	set t "Terminal Services VNC Viewer"
	wm title . $t
	catch {pack forget .f4}
	catch {pack forget .f3}
	catch {pack forget .f1}
	catch {pack forget .b.certs}
	catch {.l configure -text $t}
	catch {.f0.l configure -text "VNC Terminal Server:"}

	global vncdisplay vncauth_passwd unixpw_username vncproxy remote_ssh_cmd
	set vncdisplay ""
	set vncauth_passwd ""
	set unixpw_username ""
	set vncproxy ""
	set remote_ssh_cmd ""

	set_defaults
}

proc to_ssvnc {} {
	global ts_only ssh_only env

	if {!$ts_only && !$ssh_only} {
		return;
	}
	if {[info exists env(SSVNC_TS_ALWAYS)]} {
		return
	}
	set ts_only 0
	set ssh_only 0
	
	set t "SSL/SSH VNC Viewer"
	wm title . $t
	catch {pack configure .f1 -after .f0 -side top -fill x}
	catch {pack configure .f3 -after .f2 -side top -fill x}
	catch {pack configure .f4 -after .f3 -side top -fill x}
	catch {pack configure .b.certs -before .b.opts -side left -expand 1 -fill x}
	catch {.l configure -text $t}
	catch {.f0.l configure -text "VNC Host:Display"}

	#global started_with_noenc
	#if {$started_with_noenc} {
	#	toggle_no_encryption
	#}

	global vncdisplay vncauth_passwd unixpw_username vncproxy remote_ssh_cmd
	set vncdisplay ""
	set vncauth_passwd ""
	set unixpw_username ""
	set vncproxy ""
	set remote_ssh_cmd ""

	set_defaults
}

proc launch {{hp ""}} {
	global tcl_platform is_windows
	global mycert svcert crtdir crlfil
	global pids_before pids_after pids_new
	global env
	global use_ssl use_ssh use_sshssl sshssl_sw use_listen disable_ssl_workarounds
	global vncdisplay

	set debug 0
	if {$hp == ""} {
		set hp [get_vncdisplay]
	}

	set hpt [string trim $hp]
	regsub {[ 	].*$} $hpt "" hpt
	

	if {[regexp {^HOME=} $hpt] || [regexp {^SSVNC_HOME=} $hpt]} {
		set t $hpt
		regsub {^.*HOME=} $t "" t
		set t [string trim $t]
		set env(SSVNC_HOME) $t
		mesg "Set SSVNC_HOME to $t"
		set vncdisplay ""
		return 0
	}
	if {[regexp {^DISPLAY=} $hpt] || [regexp {^SSVNC_DISPLAY=} $hpt]} {
		set t $hpt
		regsub {^.*DISPLAY=} $t "" t
		set t [string trim $t]
		set env(DISPLAY) $t
		mesg "Set DISPLAY to $t"
		set vncdisplay ""
		global uname darwin_cotvnc
		if {$uname == "Darwin"} {
			if {$t != ""} {
				set darwin_cotvnc 0
			} else {
				set darwin_cotvnc 1
			}
		}
		return 0
	}
	if {[regexp {^DYLD_LIBRARY_PATH=} $hpt] || [regexp {^SSVNC_DYLD_LIBRARY_PATH=} $hpt]} {
		set t $hpt
		regsub {^.*DYLD_LIBRARY_PATH=} $t "" t
		set t [string trim $t]
		set env(DYLD_LIBRARY_PATH) $t
		set env(SSVNC_DYLD_LIBRARY_PATH) $t
		mesg "Set DYLD_LIBRARY_PATH to $t"
		set vncdisplay ""
		return 0
	}
	if {[regexp {^SLEEP=} $hpt] || [regexp {^SSVNC_EXTRA_SLEEP=} $hpt]} {
		set t $hpt
		regsub {^.*SLEEP=} $t "" t
		set t [string trim $t]
		set env(SSVNC_EXTRA_SLEEP) $t
		mesg "Set SSVNC_EXTRA_SLEEP to $t"
		set vncdisplay ""
		return 0
	}
	if {[regexp {^SSH=} $hpt]} {
		set t $hpt
		regsub {^.*SSH=} $t "" t
		set t [string trim $t]
		set env(SSH) $t
		mesg "Set SSH to $t"
		set vncdisplay ""
		return 0
	}
	if {[regexp {^FINISH=} $hpt] || [regexp {^SSVNC_FINISH_SLEEP=} $hpt]} {
		set t $hpt
		regsub {^.*=} $t "" t
		set t [string trim $t]
		set env(SSVNC_FINISH_SLEEP) $t
		mesg "Set SSVNC_FINISH_SLEEP to $t"
		set vncdisplay ""
		return 0
	}
	if {[regexp {^NO_DELETE=} $hpt] || [regexp {^SSVNC_NO_DELETE=} $hpt]} {
		set t $hpt
		regsub {^.*=} $t "" t
		set t [string trim $t]
		set env(SSVNC_NO_DELETE) $t
		mesg "Set SSVNC_NO_DELETE to $t"
		set vncdisplay ""
		return 0
	}
	if {[regexp {^BAT_SLEEP=} $hpt] || [regexp {^SSVNC_BAT_SLEEP=} $hpt]} {
		set t $hpt
		regsub {^.*=} $t "" t
		set t [string trim $t]
		set env(SSVNC_BAT_SLEEP) $t
		mesg "Set SSVNC_BAT_SLEEP to $t"
		set vncdisplay ""
		return 0
	}
	if {[regexp {^DEBUG_NETSTAT=} $hpt]} {
		set t $hpt
		regsub {^.*DEBUG_NETSTAT=} $t "" t
		global debug_netstat
		set debug_netstat $t
		mesg "Set DEBUG_NETSTAT to $t"
		set vncdisplay ""
		return 0
	}
	if {[regexp {^REPEATER_FORCE=} $hpt]} {
		set t $hpt
		regsub {^.*REPEATER_FORCE=} $t "" t
		set env(REPEATER_FORCE) $t
		mesg "Set REPEATER_FORCE to $t"
		set vncdisplay ""
		return 0
	}
	if {[regexp -nocase {^SSH.?ONLY} $hpt]} {
		global ssh_only
		if {$ssh_only} {
			return 0;
		}
		to_sshonly

		return 0
	}
	if {[regexp -nocase {^TS.?ONLY} $hpt]} {
		global ts_only
		if {$ts_only} {
			return 0;
		}
		to_tsonly

		return 0
	}
	if {[regexp -nocase {^IPV6=([01])} $hpt mv val]} {
		global env have_ipv6
		set have_ipv6 $val
		set env(SSVNC_IPV6) $val
		mesg "Set have_ipv6 to $val"
		set vncdisplay ""
		return 0
	}
	if {[regexp {^ENV=([A-z0-9][A-z0-9]*)=(.*)$} $hpt mv var val]} {
		global env
		if {$val == ""} {
			catch {unset env($var)}
			mesg "Unset $var"
		} else {
			set env($var) "$val"
			mesg "Set $var to $val"
		}
		set vncdisplay ""
		return 0
	}

	regsub {[ 	]*cmd=.*$} $hp "" tt

	if {[regexp {^[ 	]*$} $tt]} {
		mesg "No host:disp supplied."
		bell
		catch {raise .}
		mac_raise
		return
	}
	if {[regexp -- {--nohost--} $tt]} {
		mesg "No host:disp supplied."
		bell
		catch {raise .}
		mac_raise
		return
	}
	# XXX host_part
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

	if {!$use_ssl && !$use_ssh && !$use_sshssl && $sshssl_sw == "none"} {
		regsub -nocase {^[a-z0-9+]*://}	$hp "" hp
		set hp "Vnc://$hp"
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
		if {! [info exists env(SSVNC_NO_ENC_WARN)]} {
			direct_connect_msg
		}
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
		if {$crlfil != ""} {
			if {! [file exists $crlfil]} {
				mesg "CRL File does not exist: $crlfil"
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

	set file1 ""
	set n1 ""
	set file2 ""
	set n2 ""
	set n3 ""
	set n4 ""
	set now [clock seconds]

	set proxy [get_ssh_proxy $hp]
	if {$use_sshssl} {
		set proxy ""
	}
	if {! [repeater_proxy_check $proxy]} {
		return
	}

	global port_slot
	if {$port_slot != ""} {
		set file1 "$prefix-$port_slot.$suffix"
		set n1 $port_slot
		set ps [expr $port_slot + 200]
		set file2 "$prefix-$ps.$suffix"
		set n2 $ps
		mesg "Using Port Slot: $port_slot"
		after 700
	}

	for {set i 30} {$i <= 99} {incr i}  {
		set try "$prefix-$i.$suffix"
		if {$i == $port_slot} {
			continue
		}
		if {[file exists $try]}  {
			set mt [file mtime $try]
			set age [expr "$now - $mt"]
			set week [expr "7 * 3600 * 24"]
			if {$age > $week} {
				catch {file delete $try}
			}
		}
		if {! [file exists $try]}  {
			if {$file1 == ""} {
				set file1 $try
				set n1 $i
			} elseif {$file2 == ""} {
				set file2 $try
				set n2 $i
			} else {
				break
			}
		}
	}

	if {$file1 == ""} {
		mesg "could not find free stunnel file"
		bell
		return
	}

	if {$n1 == ""} {
		set n1 10
	}
	if {$n2 == ""} {
		set n2 11
	}
	set n3 [expr $n1 + 100]
	set n4 [expr $n2 + 100]

	global launch_windows_ssh_files 
	set launch_windows_ssh_files ""

	set did_port_knock 0

	global listening_name
	set listening_name ""

	if {$use_ssh} {
		;
	} elseif {$use_sshssl} {
		;
	} elseif {$use_ssl} {
		if {$proxy != "" && [regexp {@} $proxy]} {
			mesg "Error: proxy contains '@'  Did you mean to use SSH mode?"
			bell
			return
		}
		if [regexp {@} $hp] {
			mesg "Error: host contains '@'  Did you mean to use SSH mode?"
			bell
			return
		}
	}

	global ssh_ipv6_pid
	set ssh_ipv6_pid ""

	if {$use_sshssl} {
		set rc [launch_windows_ssh $hp $file2 $n2]
		if {$rc == 0} {
			if {![info exists env(SSVNC_NO_DELETE)]} {
				catch {file delete $file1}
				catch {file delete $file2}
			}
			del_launch_windows_ssh_files
			return
		}
		set did_port_knock 1
	} elseif {$use_ssh} {
		launch_windows_ssh $hp $file1 $n1
		# WE ARE DONE.
		return
	}

	set host [host_part $hp];
	set host_orig $host

	global win_localhost

	if {$host == ""} {
		set host $win_localhost
	}

	if [regexp {^.*@} $host match] {
		catch {raise .; update}
		mesg "Trimming \"$match\" from hostname"
		after 700
		regsub {^.*@} $host "" host
	}

	set disp [port_part $hp]
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

	if {$debug} {
		mesg "file: $file1"
		after 1000
	}

	listen_verify_all_dialog $hp

	if {$use_listen && $mycert == ""} {
		if {! [check_for_listen_ssl_cert]} {
			return;
		}
	}

	set fail 0

	set fh [open $file1 "w"]

	if {$use_listen} {
		puts $fh "client = no"
	} else {
		puts $fh "client = yes"
	}
	global disable_ssl_workarounds disable_ssl_workarounds_type
	if {$disable_ssl_workarounds} {
		if {$disable_ssl_workarounds_type == "noempty"} {
			puts $fh "options = DONT_INSERT_EMPTY_FRAGMENTS"
		}
	} else {
		puts $fh "options = ALL"
	}

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
		# see above, this should not happen.
		puts $fh "cert = _nocert_"
	}
	if {$crlfil != ""} {
		if [file isdirectory $crlfil] {
			puts $fh "CRLpath = $crlfil"
		} else {
			puts $fh "CRLfile = $crlfil"
		}
	}

	set did_check 0

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
			set did_check 1
			if {$use_sshssl} {
				set skip_verify_accepted_certs 1
				set did_check 0
			} elseif {! [check_accepted_certs 0]} {
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

	if {!$did_check} {
		check_accepted_certs 1
	}

	if {$use_sshssl} {
		set p [expr "$n2 + 5900"]
		set proxy [maybe_add_vencrypt $proxy "$win_localhost:$p"]
	} else {
		set proxy [maybe_add_vencrypt $proxy $hp]
	}

	set ipv6_pid ""
	global have_ipv6
	if {$have_ipv6} {
		if {$proxy == "" && $use_ssl} {
			# stunnel can handle ipv6
		} else {
			set res [ipv6_proxy $proxy $host $port]
			set proxy    [lindex $res 0]
			set host     [lindex $res 1]
			set port     [lindex $res 2]
			set ipv6_pid [lindex $res 3]
		}
	}

	set p_reverse 0

	if {$proxy != ""} {
		if {$use_sshssl} {
			;
		} elseif [regexp {@} $proxy] {
			bell
			catch {raise .; update}
			mesg "WARNING: SSL proxy contains \"@\" sign"
			after 1500
		}
		set env(SSVNC_PROXY) $proxy
		set env(SSVNC_DEST) "$host:$port"
		if {$use_listen} {
			set env(SSVNC_REVERSE) "$win_localhost:$port"
			set env(CONNECT_BR_SLEEP) 3
			set p_reverse 1
		} else {
			if {$use_sshssl && [regexp {vencrypt:} $proxy]} {
				set env(SSVNC_LISTEN) [expr "$n4 + 5900"]
			} else {
				set env(SSVNC_LISTEN) [expr "$n2 + 5900"]
			}
		}
		if {[info exists env(PROXY_DEBUG)]} {
			foreach var [list SSVNC_PROXY SSVNC_DEST SSVNC_REVERSE CONNECT_BR_SLEEP SSVNC_LISTEN] {
				if [info exists env($var)] {
					mesg "$var $env($var)"; after 2500;
				}
			}
		}
	}

	global anon_dh_detected server_anondh
	if {$anon_dh_detected || $server_anondh} {
		puts $fh "ciphers = ALL:RC4+RSA:+SSLv2:@STRENGTH"
		set anon_dh_detected 0
	}


	puts $fh "\[vnc$n1\]"
	set port2 ""
	set port3 ""
	if {! $use_listen} {
		set port2 [expr "$n1 + 5900"] 
		if [regexp {vencrypt:} $proxy] {
			set port3 [expr "$n3 + 5900"] 
			set port2 $port3
			puts $fh "accept = $win_localhost:$port3"
		} else {
			puts $fh "accept = $win_localhost:$port2"
		}

		if {$use_sshssl && [regexp {vencrypt:} $proxy]} {
			set port [expr "$n4 + 5900"]
			puts $fh "connect = $win_localhost:$port"
		} elseif {$use_sshssl || $proxy != ""} {
			set port [expr "$n2 + 5900"]
			puts $fh "connect = $win_localhost:$port"
		} else {
			puts $fh "connect = $host:$port"
		}
	} else {
		set port2 [expr "$n1 + 5500"] 
		set hloc ""
		if {$use_ssh} {
			# not reached?
			set hloc "$win_localhost:"
			set listening_name "$win_localhost:$port  (on remote SSH side)"
		} else {
			set hn [get_hostname]
			if {$hn == ""} {
				set hn "this-computer"
			}
			set listening_name "$hn:$port  (or nn.nn.nn.nn:$port, etc.)"
		}
		if {$host_orig != "" && $hloc == ""} {
			set hloc "$host_orig:"
		}
		puts $fh "accept = $hloc$port"
		puts $fh "connect = $win_localhost:$port2"
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
		if {![info exists env(SSVNC_NO_DELETE)]} {
			catch {file delete $file1}
		}
		catch { unset env(SSVNC_PROXY) }
		catch { unset env(SSVNC_LISTEN) }
		catch { unset env(SSVNC_REVERSE) }
		catch { unset env(SSVNC_DEST) }
		catch { unset env(SSVNC_PREDIGESTED_HANDSHAKE) }
		catch { unset env(CONNECT_BR_SLEEP) }
		winkill $ipv6_pid
		winkill $ssh_ipv6_pid
		set ssh_ipv6_pid ""
		return
	}

	note_stunnel_pids "before"

	set proxy_pid ""
	set proxy_pid2 ""

	if {$use_listen} {
		windows_listening_message $n1
	}

	if {$proxy != ""} {
		if [regexp {vencrypt:} $proxy] {
			set vport [expr "$n1 + 5900"]
			mesg "Starting VeNCrypt helper on port $vport,$port3 ..."
			after 500
			if {![info exists env(SSVNC_NO_DELETE)]} {
				catch {file delete "$file1.pre"}
			}
			set env(SSVNC_PREDIGESTED_HANDSHAKE) "$file1.pre"
			set env(SSVNC_VENCRYPT_VIEWER_BRIDGE) "$vport,$port3"
			set proxy_pid2 [exec "connect_br.exe" &]
			catch { unset env(SSVNC_VENCRYPT_VIEWER_BRIDGE) }
		}
		mesg "Starting TCP helper on port $port ..."
		after 400
		# ssl br case:
		set proxy_pid [exec "connect_br.exe" &]
		catch { unset env(SSVNC_PROXY) }
		catch { unset env(SSVNC_LISTEN) }
		catch { unset env(SSVNC_REVERSE) }
		catch { unset env(SSVNC_DEST) }
		catch { unset env(SSVNC_PREDIGESTED_HANDSHAKE) }
		catch { unset env(CONNECT_BR_SLEEP) }
	}

	mesg "Starting STUNNEL on port $port2 ..."
	after 500

	set pids [exec stunnel $file1 &]

	if {! $p_reverse} {
		after 300
		set vtm [vencrypt_tutorial_mesg]
		if {$vtm == ""} {
			after 300
		}
	}

	note_stunnel_pids "after"

	if {$debug} {
		after 1000
		mesg "pids $pids"
		after 1000
	} else {
		catch {destroy .o}
		catch {destroy .oa}
		catch {destroy .os}
		wm withdraw .
	}

	do_viewer_windows $n1

	del_launch_windows_ssh_files

	if {![info exists env(SSVNC_NO_DELETE)]} {
		catch {file delete $file1}
	}

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
		global kill_stunnel
		if {$kill_stunnel} {
			set terminate_pids yes
		} else {
			win_kill_msg $plist
			update
			vwait terminate_pids
		}
		if {$terminate_pids == "yes"} {
			kill_stunnel $pids_new
		}
	} else {
		win_nokill_msg
	}
	mesg "Disconnected from $hp."
	winkill $ipv6_pid
	winkill $ssh_ipv6_pid
	set ssh_ipv6_pid ""

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

	set host [host_part $hp]

	set host_orig $host

	global win_localhost
	if {$host == ""} {
		set host $win_localhost
	}

	if [regexp {^.*@} $host match] {
		catch {raise .; update}
		mesg "Trimming \"$match\" from hostname"
		after 700
		regsub {^.*@} $host "" host
	}

	set disp [port_part $hp]
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

	global have_ipv6
	set ipv6_pid ""
	if {$have_ipv6 && !$use_listen} {
		set res [ipv6_proxy $proxy $host $port]
		set proxy    [lindex $res 0]
		set host     [lindex $res 1]
		set port     [lindex $res 2]
		set ipv6_pid [lindex $res 3]
	}

	if {$proxy != ""} {
		if [regexp {@} $proxy] {
			bell
			catch {raise .; update}
			mesg "WARNING: SSL proxy contains \"@\" sign"
			after 1500
		}
		set n2 45

		set env(SSVNC_PROXY) $proxy
		set env(SSVNC_LISTEN) [expr "$n2 + 5900"]
		set env(SSVNC_DEST) "$host:$port"

		set port [expr $n2 + 5900]
		set host $win_localhost
	}

	set fail 0
	if {! $did_port_knock} {
		if {! [do_port_knock $host start]} {
			set fail 1
		}
		set did_port_knock 1
	}

	if {$fail} {
		catch { unset env(SSVNC_PROXY) }
		catch { unset env(SSVNC_LISTEN) }
		catch { unset env(SSVNC_DEST) }
		winkill $ipv6_pid
		return
	}

	set proxy_pid ""
	if {$proxy != ""} {
		mesg "Starting Proxy TCP helper on port $port ..."
		after 400
		# unencrypted br case:
		set proxy_pid [exec "connect_br.exe" &]
		catch { unset env(SSVNC_PROXY) }
		catch { unset env(SSVNC_LISTEN) }
		catch { unset env(SSVNC_DEST) }
	}

	vencrypt_tutorial_mesg

	catch {destroy .o}
	catch {destroy .oa}
	catch {destroy .os}
	wm withdraw .

	if {$use_listen} {
		set n $port
		if {$n >= 5500} {
			set n [expr $n - 5500]
		}
		global direct_connect_reverse_host_orig
		set direct_connect_reverse_host_orig $host_orig

		do_viewer_windows "$n"

		set direct_connect_reverse_host_orig ""
	} else {
		if {$port >= 5900 && $port < 6100} {
			set port [expr $port - 5900]
		}
		do_viewer_windows "$host:$port"
	}

	wm deiconify .

	mesg "Disconnected from $hp."

	winkill $ipv6_pid

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
				set t "$env(SSVNC_HOME)/.vnc"	
				if {! [file isdirectory $t]} {
					catch {file mkdir $t}
				}
				set t "$env(SSVNC_HOME)/.vnc/certs"	
				if {! [file isdirectory $t]} {
					catch {file mkdir $t}
				}
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

proc delete_cert {{parent "."}} {
	set idir [get_idir_certs ""]
	set f ""
	unix_dialog_resize $parent
	if {$idir != ""} {
		set f [tk_getOpenFile -parent $parent -initialdir $idir]
	} else {
		set f [tk_getOpenFile -parent $parent]
	}
	if {$f != "" && [file exists $f]} {
		set reply [tk_messageBox -parent $parent -type yesno -icon question -title "Delete Cert" -message "Delete $f"]
		if {$reply == "yes"} {
			global mycert svcert crlfil
			set f_text [read_file $f]
			set f2 "" 
			catch {file delete $f}	
			if {$f == $mycert} { set mycert "" }
			if {$f == $svcert} { set svcert "" }
			if {$f == $crlfil} { set crlfil "" }
			if [regexp {\.crt$} $f] {
				regsub {\.crt$} $f ".pem" f2
			} elseif [regexp {\.pem$} $f] {
				regsub {\.pem$} $f ".crt" f2
			}
			if {$f2 != "" && [file exists $f2]} {
				set reply [tk_messageBox -parent $parent -type yesno -icon question -title "Delete Cert" -message "Delete $f2"]
				if {$reply == "yes"} {
					catch {file delete $f2}	
					if {$f2 == $mycert} { set mycert "" }
					if {$f2 == $svcert} { set svcert "" }
					if {$f2 == $crlfil} { set crlfil "" }
				}
			}
			set dir [file dirname $f]
			if {$f_text != "" && [regexp {accepted$} $dir]} {
				foreach crt [glob -nocomplain -directory $dir {*.crt} {*.pem} {*.[0-9]}] {
					#puts "try $crt"
					set c_text [read_file $crt]
					if {$c_text == ""} {
						continue
					}
					if {$c_text != $f_text} {
						continue
					}
					set reply [tk_messageBox -parent $parent -type yesno -icon question -title "Delete Identical Cert" -message "Delete Identical $crt"]
					if {$reply == "yes"} {
						catch {file delete $crt}	
					}
				}
			}
		}
	}
	catch {wm deiconify .c}
	update
}

proc set_mycert {{parent "."}} {
	global mycert
	set idir [get_idir_certs $mycert]
	set t ""
	unix_dialog_resize $parent
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

proc set_crlfil {{parent "."}} {
	global crlfil
	set idir [get_idir_certs $crlfil]
	set t ""
	unix_dialog_resize $parent
	if {$idir != ""} {
		set t [tk_getOpenFile -parent $parent -initialdir $idir]
	} else {
		set t [tk_getOpenFile -parent $parent]
	}
	if {$t != ""} {
		set crlfil $t
	}
	catch {wm deiconify .c}
	v_crlfil
	update 
}

proc set_ultra_dsm_file {{parent "."}} {
	global ultra_dsm_file
	set idir [get_idir_certs $ultra_dsm_file]
	set t ""
	unix_dialog_resize $parent
	if {$idir != ""} {
		set t [tk_getOpenFile -parent $parent -initialdir $idir]
	} else {
		set t [tk_getOpenFile -parent $parent]
	}
	if {$t != ""} {
		set ultra_dsm_file $t
	}
	update 
}

proc set_ssh_known_hosts_file {{parent "."}} {
	global ssh_known_hosts_filename is_windows uname

	if {$ssh_known_hosts_filename == ""} {
		set pdir [get_profiles_dir]
		set pdir "$pdir/ssh_known_hosts"
		catch {file mkdir $pdir}

		global last_load
		if {![info exists last_load]} {
			set last_load ""
		}
		if {$last_load != ""} {
			set dispf [string trim $last_load]
			set dispf [file tail $dispf]
			
			regsub {\.vnc$} $dispf "" dispf
			if {![regexp {\.known$} $dispf]} {
				set dispf "$dispf.known"
			}
			set guess $dispf
		} else {
			set vncdisp [get_vncdisplay]
			set dispf [string trim $vncdisp]
			if {$dispf != ""} {
				regsub {[ 	].*$} $dispf "" dispf
				regsub -all {/} $dispf "" dispf
			} else {
				set dispf "unique-name-here"
			}
			if {$is_windows || $uname == "Darwin"} {
				regsub -all {:} $dispf "-" dispf
			} else {
				regsub -all {:} $dispf "-" dispf
			}
			if {![regexp {\.known$} $dispf]} {
				set dispf "$dispf.known"
			}
			set guess $dispf
		}
	} else {
		set pdir [file dirname $ssh_known_hosts_filename]
		set guess [file tail $ssh_known_hosts_filename]
	}

	set t ""
	unix_dialog_resize $parent
	if {$pdir != ""} {
		set t [tk_getSaveFile -parent $parent -initialdir $pdir -initialfile $guess]
	} else {
		set t [tk_getSaveFile -parent $parent -initialfile $guess]
	}
	if {$t != ""} {
		set ssh_known_hosts_filename $t
	}
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

proc show_crl {crl} {
	if {$crl == ""} {
		bell
		return
	}
	if {! [file exists $crl]} {
		bell
		return
	}

	set flist [list]

	if [file isdirectory $crl] {
		foreach cfile [glob -nocomplain -directory $crl "*"] {
			if [file isfile $cfile] {
				lappend flist $cfile
			}
		}
	} else {
		lappend flist $crl
	}

	set ossl [get_openssl]
	set info ""

	foreach cfile $flist {
		catch {
			set ph [open "| $ossl crl -fingerprint -text -noout -in \"$cfile\"" "r"]
			while {[gets $ph line] > -1} {
				append info "$line\n"
			}
			close $ph
			append info "\n"
		}
	}

	set w .show_crl
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
	no_certs_tutorial_mesg
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

proc v_crlfil {} {
	global crlfil
	if {$crlfil == "" || ! [file exists $crlfil]} {
		catch {.c.crlfil.i configure -state disabled}
	} else {
		catch {.c.crlfil.i configure -state normal}
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

proc show_crlfil {} {
	global crlfil
	show_crl $crlfil
}

proc set_svcert {{parent "."}} {
	global svcert crtdir
	set idir [get_idir_certs $svcert]
	set t ""
	unix_dialog_resize $parent
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
	unix_dialog_resize $parent
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
	unix_dialog_resize .ccrt 
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
		set tmp "/tmp/cert.cfg.[tpid]"
		set tmp [mytmp $tmp]
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
			tk_messageBox -type ok -icon error -message $emess -title "Could not encrypt private key"
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
	
proc create_cert {{name ""}} {

	toplev .ccrt
	wm title .ccrt "Create SSL Certificate"

	global uname
	set h 27
	if [small_height] {
		set h 14
	} elseif {$uname == "Darwin"} {
		set h 20
	}
	scroll_text .ccrt.f 80 $h

	set msg {
    This dialog helps you to create a simple Self-Signed SSL certificate.  

    On Unix the openssl(1) program must be installed and in $PATH.
    On Windows, a copy of the openssl program is provided for convenience.

    The resulting certificate files can be used for either:

       1) authenticating yourself (VNC Viewer) to a VNC Server
    or 2) your verifying the identity of a remote VNC Server.

    In either case you will need to safely copy one of the generated key or
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
           http://www.karlrunge.com/x11vnc/faq.html#faq-ssl-tunnel-int

    The first one describes how to use x11vnc to create Certificate
    Authority (CA) certificates in addition to Self-Signed ones.


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
		set ccert(DAYS) "730"
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

	set idir [get_idir_certs ""]
	if {$name != ""} {
		if {[regexp {/} $name] || [regexp {\.pem$} $name] || [regexp {\.crt$} $name]} {
			set ccert(FILE) $name
		} else {
			set ccert(FILE) "$idir/$name.pem"
		}
	} elseif {$ccert(FILE) == ""} {
		set ccert(FILE) "$idir/vnccert.pem"
	}

	button .ccrt.cancel -text "Cancel" -command {destroy .ccrt; catch {raise .c}}
	bind .ccrt <Escape> {destroy .ccrt; catch {raise .c}}
	wm protocol .ccrt WM_DELETE_WINDOW {destroy .ccrt; catch {raise .c}}

	button .ccrt.create -text "Generate Cert" -command {destroy .ccrt; catch {raise .c}; do_oss_create}

	pack .ccrt.create .ccrt.cancel -side bottom -fill x

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
	if {$name != ""} {
		$w.b configure -state disabled
	}

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
	
proc import_browse {par} {
	global import_file

	set idir ""
	if {$import_file != ""} {
		set idir [get_idir_certs $import_file]
	}
	unix_dialog_resize $par
	if {$idir != ""} {
		set t [tk_getOpenFile -parent $par -initialdir $idir]
	} else {
		set t [tk_getOpenFile -parent $par]
	}
	if {$t != ""} {
		set import_file $t
	}
	catch {raise $par}
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
	unix_dialog_resize $par
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

proc do_save {par} {
	global import_mode import_file import_save_file
	global also_save_to_accepted_certs

	if {![info exists also_save_to_accepted_certs]} {
		set also_save_to_accepted_certs 0
	}
	
	if {$import_save_file == "" && ! $also_save_to_accepted_certs} {
		tk_messageBox -parent $par -type ok -icon error \
			-message "No Save File supplied" -title "Save File"
		return
	}

	set str ""
	set subject_issuer ""
	if {$import_mode == "save_cert_text"} {
		global save_cert_text
		set str $save_cert_text
		set i 0
		foreach line [split $str "\n"] {
			incr i
			if {$i > 50} {
				break
			}
			if [regexp {^- subject: *(.*)$} $line m val] {
				set subject_issuer "${subject_issuer}subject:$val\n"
			}
			if [regexp {^- (issuer[0-9][0-9]*): *(.*)$} $line m is val] {
				set subject_issuer "${subject_issuer}$is:$val\n"
			}
			if [regexp {^INFO: SELF_SIGNED=(.*)$} $line m val] {
				set subject_issuer "${subject_issuer}SELF_SIGNED:$val\n"
			}
		}
	} elseif {$import_mode == "paste"} {
		set str [$par.paste.t get 1.0 end]
	} else {
		if {! [file exists $import_file]} {
			tk_messageBox -parent $par -type ok -icon error \
				-message "Input file \"$import_file\" does not exist." -title "Import File"
			return
		}
		set fh ""
		set emess ""
		set rc [catch {set fh [open $import_file "r"]} emess]
		if {$rc != 0 || $fh == ""} {
			tk_messageBox -parent $par -type ok -icon error \
				-message $emess -title "Import File: $import_file"
			return
		}
		while {[gets $fh line] > -1} {
			append str "$line\n"
		}
		close $fh
	}

	if {! [regexp {BEGIN CERTIFICATE} $str]} {
		tk_messageBox -parent $par -type ok -icon error \
			-message "Import Text does not contain \"BEGIN CERTIFICATE\"" -title "Imported Text"
		return
	}
	if {! [regexp {END CERTIFICATE} $str]} {
		tk_messageBox -parent $par -type ok -icon error \
			-message "Import Text does not contain \"END CERTIFICATE\"" -title "Imported Text"
		return
	}

	global is_windows
	set fh ""
	set emess ""
	set deltmp ""
	if {$import_save_file == ""} {
		if {! $is_windows} {
			set deltmp /tmp/import.[tpid]
		} else {
			set deltmp import.[tpid]
		}
		set deltmp [mytmp $deltmp]
		set import_save_file $deltmp
	}
	set rc [catch {set fh [open $import_save_file "w"]} emess]
	if {$rc != 0 || $fh == ""} {
		tk_messageBox -parent $par -type ok -icon error \
			-message $emess -title "Save File: $import_save_file"
		return
	}
	if {! $is_windows} {
		catch {file attributes $import_save_file -permissions go-w}
		if {[regexp {PRIVATE} $str] || [regexp {\.pem$} $import_save_file]} {
			catch {file attributes $import_save_file -permissions go-rw}
		}
	}

	puts -nonewline $fh $str
	close $fh

	global do_save_saved_it
	set do_save_saved_it 1

	if {$also_save_to_accepted_certs} {
		set ossl [get_openssl]
		set fp_txt ""
		set fp_txt [exec $ossl x509 -fingerprint -noout -in $import_save_file]

		set adir [get_idir_certs ""]
		set adir "$adir/accepted"
		catch {file mkdir $adir}

		set fingerprint ""
		set fingerline ""

		set i 0
		foreach line [split $fp_txt "\n"] {
			incr i
			if {$i > 5} {
				break
			}
			if [regexp -nocase {Fingerprint=(.*)} $line mv str] {
				set fingerline $line
				set fingerprint [string trim $str]
			}
		}

		set fingerprint [string tolower $fingerprint]
		regsub -all {:} $fingerprint "-" fingerprint
		regsub -all {[\\/=]} $fingerprint "_" fingerprint

		if {$subject_issuer == ""} {
			set si_txt ""
			set si_txt [exec $ossl x509 -subject -issuer -noout -in $import_save_file]
			set sub ""
			set iss ""
			foreach line [split $si_txt "\n"] {
				if [regexp -nocase {^subject= *(.*)$} $line mv str] {
					set str [string trim $str]
					set sub $str
				} elseif [regexp -nocase {^issuer= *(.*)$} $line mv str] {
					set str [string trim $str]
					set iss $str
				}
			}
			if {$sub != "" && $iss != ""} {
				set subject_issuer "subject:$sub\nissuer1:$iss\n"
				if {$sub == $iss} {
					set subject_issuer "${subject_issuer}SELF_SIGNED:1\n"
				} else {
					set subject_issuer "${subject_issuer}SELF_SIGNED:0\n"
				}
			}
		}

		global vncdisplay
		set from [get_ssh_hp $vncdisplay]
		if {$from == ""} {
			set from [file tail $import_save_file]
			regsub {\..*$} $from "" from
		}
		if {$from == ""} {
			set from "import"
		}
		if [regexp -- {^:[0-9][0-9]*$} $from] {
			set from "listen$from"
		}
		set hp $from

		set from [string tolower $from]
		regsub -all {^[+a-z]*://} $from "" from
		regsub -all {:} $from "-" from
		regsub -all {[\\/=]} $from "_" from
		regsub -all {[ 	]} $from "_" from

		set crt "$adir/$from=$fingerprint.crt"
		catch {file copy -force $import_save_file $crt}

		global do_save_saved_hash_it
		set do_save_saved_hash_it 1
		save_hash $crt $adir $hp $fingerline $from $fingerprint $subject_issuer
	}

	catch {destroy $par}
	set p .c
	if {![winfo exists .c]} {
		global accepted_cert_dialog_in_progress
		if {! $accepted_cert_dialog_in_progress} {
			if {$deltmp == ""} {
				getcerts
				update	
			}
		}
	}
	if {![winfo exists .c]} {
		set p .
	}
	catch {raise .c}
	catch {destroy .scrt}
	if {$deltmp != ""} {
		catch {file delete $deltmp}
		set import_save_file ""
		return;
	}
	tk_messageBox -parent $p -type ok -icon info \
		-message "Saved to file: $import_save_file" -title "Save File: $import_save_file"
}

proc import_cert {} {

	toplev .icrt
	wm title .icrt "Import SSL Certificate"

	global scroll_text_focus
	set scroll_text_focus 0
	global uname
	set h 19
	if [small_height] {
		set h 12
	} elseif {$uname == "Darwin"} {
		set h 16
	}
	scroll_text .icrt.f 90 $h
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

	button $w.b -pady 1 -anchor w -text "Browse..." -command {import_browse .icrt}
	pack $w.b -side right
	pack $w.p $w.f -side left
	pack $w.e -side left -expand 1 -fill x

	$w.b configure -state disabled
	$w.e configure -state disabled

	label .icrt.plab -anchor w -text "Paste Certificate here:     (extra blank lines above or below are OK)" 
	set h 22
	if [small_height] {
		set h 11
	} elseif {$uname == "Darwin"} {
		set h 11
	}
	scroll_text .icrt.paste 90 $h

	button .icrt.cancel -text "Cancel" -command {destroy .icrt; catch {raise .c}}
	bind .icrt <Escape> {destroy .icrt; catch {raise .c}}
	wm protocol .icrt WM_DELETE_WINDOW {destroy .icrt; catch {raise .c}}

	button .icrt.save -text "Save" -command {do_save .icrt}

	set w .icrt.sf
	frame $w

	label $w.l -text "Save to File:" -anchor w
	global import_save_file
	set import_save_file ""
	entry $w.e -width 40 -textvariable import_save_file
	button $w.b -pady 1 -anchor w -text "Browse..." -command import_save_browse

	global also_save_to_accepted_certs
	set also_save_to_accepted_certs 0
	checkbutton .icrt.ac -anchor w -variable also_save_to_accepted_certs -text \
	    "Also Save to the 'Accepted Certs' directory" -relief raised

	pack $w.b -side right
	pack $w.l -side left
	pack $w.e -side left -expand 1 -fill x

	pack .icrt.save .icrt.cancel .icrt.ac .icrt.sf .icrt.mf -side bottom -fill x
	pack .icrt.paste .icrt.plab -side bottom -fill x

	pack .icrt.f -side top -fill both -expand 1

	.icrt.paste.t insert end ""

	focus .icrt.paste.t

	center_win .icrt
}

proc save_cert {hp} {

	global cert_text

	toplev .scrt
	wm title .scrt "Import/Save SSL Certificate"

	global scroll_text_focus
	set scroll_text_focus 0
	global uname

	global accepted_cert_dialog_in_progress
	set h 20
	if {$accepted_cert_dialog_in_progress} {
		set mode "accepted"
		set h 15
		if [small_height] {
			set h 11
		}
	} else {
		set mode "normal"
		set h 20
		if [small_height] {
			set h 16
		}
	}
	scroll_text .scrt.f 90 $h

	set scroll_text_focus 1

	set msg1 {
    This dialog lets you import a SSL Certificate retrieved from a VNC server.

    Be sure to have verified its authenticity via an external means (checking
    the MD5 hash value sent to you by the administrator, etc)

    Set "Save to File" to the filename where the imported cert will be saved.

    If you also want the Certificate to be saved to the pool of certs in the
    'Accepted Certs' directory, select the checkbox.  By default all Servers are
    verified against the certificates in this pool.

    Then, click on "Save" to save the imported Certificate.

    After you have imported the Certificate it will be automatically selected as
    the "ServerCert" for the next connection to this host: %HOST

    To make the ServerCert setting to the imported cert file PERMANENT, select
    'Save' to save it in the profile for this host.
}

	set msg2 {
    This dialog lets you import a SSL Certificate retrieved from a VNC server.

    Be sure to have verified its authenticity via an external means (checking
    the MD5 hash value sent to you by the administrator, etc)

    It will be added to the 'Accepted Certs' directory.  The "Save to File"
    below is already set to the correct directory and file name.

    Click on "Save" to add it to the Accepted Certs.

    It, and the other certs in that directory, will be used to authenticate
    any VNC Server that has "ACCEPTED_CERTS" as the "CertsDir" value in the
    "Certs..." dialog.  This is the default checking policy.
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

	set h 22
	if [small_height] {
		set h 10
	}
	scroll_text .scrt.paste 90 $h

	button .scrt.cancel -text "Cancel" -command {destroy .scrt; catch {raise .c}}
	bind .scrt <Escape> {destroy .scrt; catch {raise .c}}
	wm protocol .scrt WM_DELETE_WINDOW {destroy .scrt; catch {raise .c}}

	global import_save_file
	if {$mode == "normal"} {
		button .scrt.save -text "Save" -command {do_save .scrt; set svcert $import_save_file}
	} else {
		button .scrt.save -text "Save" -command {do_save .scrt}
	}

	if [regexp -nocase -- {ACCEPT} $cert_text] {
		if [regexp -nocase -- {Client certificate} $cert_text] {
			if [regexp -- {^:[0-9][0-9]*$} $hp] {
				if [regexp -nocase {subject=.*CN=([^/][^/]*)/} $cert_text mv0 mv1] {
					regsub -all {[ 	]} $mv1 "" mv1
					set hp "$mv1$hp"
				} else {
					set hp "listen$hp"
				}
			}
		}
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

	global also_save_to_accepted_certs
	set also_save_to_accepted_certs 0
	if [regexp -nocase -- {ACCEPT} $cert_text] {
		if [regexp -nocase -- {Client certificate} $cert_text] {
			set also_save_to_accepted_certs 1
		}
	}
	checkbutton .scrt.ac -anchor w -variable also_save_to_accepted_certs -text \
	    "Also Save to the 'Accepted Certs' directory" -relief raised

	if {$mode == "normal"} {
		pack .scrt.cancel .scrt.save .scrt.sf .scrt.ac .scrt.mf -side bottom -fill x
	} else {
		pack .scrt.cancel .scrt.save .scrt.sf          .scrt.mf -side bottom -fill x
	}
	pack .scrt.paste -side bottom -fill x

	pack .scrt.f -side top -fill both -expand 1

	set text "" 
	set on 0
	foreach line [split $cert_text "\n"] {
		if [regexp -- {-----BEGIN CERTIFICATE-----} $line] {
			incr on
		}
		if {$on != 1} {
			continue;
		}
		append text "$line\n"
		if [regexp -- {-----END CERTIFICATE-----} $line] {
			set on 2
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
	global mycert svcert crtdir crlfil
	global use_ssh use_sshssl
	toplev .c
	wm title .c "SSL Certificates"
	frame .c.mycert
	frame .c.svcert
	frame .c.crtdir
	frame .c.crlfil
	label .c.mycert.l -anchor w -width 12 -text "MyCert:"
	label .c.svcert.l -anchor w -width 12 -text "ServerCert:"
	label .c.crtdir.l -anchor w -width 12 -text "CertsDir:"
	label .c.crlfil.l -anchor w -width 12 -text "CRL File:"
	
	entry .c.mycert.e -width 32 -textvariable mycert -vcmd v_mycert
	entry .c.svcert.e -width 32 -textvariable svcert -vcmd v_svcert
	entry .c.crtdir.e -width 32 -textvariable crtdir
	entry .c.crlfil.e -width 32 -textvariable crlfil -vcmd v_crlfil

	bind .c.mycert.e <Enter> {.c.mycert.e validate}
	bind .c.mycert.e <Leave> {.c.mycert.e validate}
	bind .c.svcert.e <Enter> {.c.svcert.e validate}
	bind .c.svcert.e <Leave> {.c.svcert.e validate}

	button .c.mycert.b -text "Browse..." -command {set_mycert .c; catch {raise .c}}
	button .c.svcert.b -text "Browse..." -command {set_svcert .c; catch {raise .c}}
	button .c.crtdir.b -text "Browse..." -command {set_crtdir .c; catch {raise .c}}
	button .c.crlfil.b -text "Browse..." -command {set_crlfil .c; catch {raise .c}}

	button .c.mycert.i -text "Info" -command {show_mycert}
	button .c.svcert.i -text "Info" -command {show_svcert}
	button .c.crtdir.i -text "Info" -command {}
	button .c.crlfil.i -text "Info" -command {show_crlfil}

	bind .c.mycert.b <Enter> "v_mycert"
	bind .c.svcert.b <Enter> "v_svcert"
	bind .c.crlfil.b <Enter> "v_crlfil"

	.c.mycert.i configure -state disabled
	.c.svcert.i configure -state disabled
	.c.crtdir.i configure -state disabled
	.c.crlfil.i configure -state disabled

	bind .c.mycert.b <B3-ButtonRelease>   "show_mycert"
	bind .c.svcert.b <B3-ButtonRelease>   "show_svcert"
	bind .c.crlfil.b <B3-ButtonRelease>   "show_crlfil"

	set do_crl 1
	set do_row 1

	set c .c
	if {$do_row} {
		frame .c.b0
		set c .c.b0
	}

	button $c.create -text "Create Certificate ..." -command {create_cert}
	button $c.import -text "Import Certificate ..." -command {import_cert}
	button $c.delete -text "Delete Certificate ..." -command {delete_cert .c}

	if {$c != ".c"} {
		pack $c.create $c.import $c.delete  -fill x -expand 1 -side left
	}

	frame .c.b
	button .c.b.done -text "Done" -command {catch {destroy .c}}
	bind .c <Escape> {destroy .c}
	button .c.b.help -text "Help" -command help_certs
	pack .c.b.help .c.b.done -fill x -expand 1 -side left

	set wlist [list mycert svcert crtdir]
	lappend wlist crlfil

	foreach w $wlist {
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

	global svcert_default_force mycert_default_force crlfil_default_force
	if {$mycert_default_force} {
		.c.mycert.e configure -state readonly
		.c.mycert.b configure -state disabled
	}
	if {$svcert_default_force} {
		.c.svcert.e configure -state readonly
		.c.svcert.b configure -state disabled
		.c.crtdir.e configure -state readonly
		.c.crtdir.b configure -state disabled
	}
	if {$crlfil_default_force} {
		.c.crlfil.e configure -state readonly
		.c.crlfil.b configure -state disabled
	}

	if {$mycert != ""} {
		v_mycert
	}
	if {$svcert != ""} {
		v_svcert
	}
	if {$crlfil != ""} {
		v_crlfil
	}

	set wlist [list .c.mycert .c.svcert .c.crtdir]
	if {$do_crl} {
		lappend wlist .c.crlfil
	}
	if {$c != ".c"} {
		lappend wlist $c
	} else {
		lappend wlist .c.create .c.import .c.delete
	}
	lappend wlist .c.b

	eval pack $wlist -side top -fill x

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
			mesg "Applying template: $inc"
			after 100
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

proc unix_dialog_resize {{w .}} {
	global env is_windows uname unix_dialog_size
	set ok 0
	set width 600
	set height 300
	if {[info exists env(SSVNC_BIGGER_DIALOG)]} {
		set ok 1
		if {[regexp {([0-9][0-9]*)x([0-9][0-9]*)} $env(SSVNC_BIGGER_DIALOG) m wi he]} {
			set width $wi;
			set height $he;
		}
	} elseif {[info exists env(USER)] && $env(USER) == "runge"} {
		set ok 1
	}
	if {$ok} {
		# this is a personal hack because tk_getOpenFile size is not configurable.
		if {!$is_windows && $uname != "Darwin"} {
			if {$w == "."} {
				set w2 .__tk_filedialog
			} else {
				set w2 $w.__tk_filedialog
			}
			set w3 $w2.icons.canvas
			global udr_w4
			set udr_w4 $w2.f2.cancel
			if {! [info exists unix_dialog_size($w)]} {
				after 50 {global udr_w4; catch {$udr_w4 invoke}}
				tk_getOpenFile -parent $w -initialdir /
				set unix_dialog_size($w) 1
			}
			if [winfo exists $w3] {
				catch {$w3 configure -width $width}
				catch {$w3 configure -height $height}
			}
		}
	}
}

proc delete_profile {{parent "."}} {

	globalize

	set dir [get_profiles_dir]

	unix_dialog_resize $parent
	set file [tk_getOpenFile -parent $parent -initialdir $dir -title "DELETE VNC Profile"]

	if {$file == ""} {
		return
	}

	set tail [file tail $file]

	set ans [tk_messageBox -type okcancel -title "Delete $tail" -message "Really Delete $file?" -icon warning]

	if {$ans == "ok"} {
		catch {file delete $file}
		mesg "Deleted $tail"
	} else {
		mesg "Delete Skipped."
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
		unix_dialog_resize
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

	set goto_mode "";
	set str ""
	set include ""
	set sw 1
	while {[gets $fh line] > -1} {
		append str "$line\n"
		if [regexp {^include_list=(.*)$} $line m val] {
			set include $val
		}
		global ssh_only ts_only
		if {$ssh_only || $ts_only} {
			if [regexp {use_ssh=0} $line] {
				if {$sw} {
					mesg "Switching to SSVNC mode."
					set goto_mode "ssvnc"
					update
					after 300
				} else {
					bell
					mesg "Cannot Load an SSL profile in SSH-ONLY mode."
					set profdone 1
					close $fh
					return
				}
			}
		}
		if {! $ts_only} {
			if [regexp {ts_mode=1} $line] {
				if {$sw} {
					mesg "Switching to Terminal Services mode."
					set goto_mode "tsonly"
					update
					after 300
				} else {
					bell
					mesg "Cannot Load a Terminal Svcs profile SSVNC mode."
					set profdone 1
					close $fh
					return
				}
			}
		} else {
			if [regexp {ts_mode=0} $line] {
				if {$sw} {
					mesg "Switching to SSVNC mode."
					set goto_mode "ssvnc"
					update
					after 300
				} else {
					bell
					mesg "Cannot Load a Terminal Svcs profile SSVNC mode."
					set profdone 1
					close $fh
					return
				}
			}
		}
	}
	close $fh

	if {$include != ""} {
		load_include $include $dir
	}

	if {$goto_mode == "tsonly"} {
		to_tsonly
	} elseif {$goto_mode == "ssvnc"} {
		to_ssvnc
	} elseif {$goto_mode == "sshvnc"} {
		to_sshvnc
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
		if {! $disable_all_encryption} {
			set use_ssl 1
		}
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
	unixpw_adjust

	global last_load
	set last_load [file tail $file]

	global uname darwin_cotvnc
	if {$uname == "Darwin"} {
		if {$use_x11_macosx} {
			set darwin_cotvnc 0;
		} else {
			set darwin_cotvnc 1;
		}
	}

	mesg "Loaded [file tail $file]"
}

proc sync_use_ssl_ssh {} {
	global use_ssl use_ssh use_sshssl
	global disable_all_encryption
	if {$use_ssl} {
		ssl_ssh_adjust ssl
	} elseif {$use_ssh} {
		ssl_ssh_adjust ssh
	} elseif {$use_sshssl} {
		ssl_ssh_adjust sshssl
	} elseif {$disable_all_encryption} {
		ssl_ssh_adjust none
	} else {
		ssl_ssh_adjust ssl
	}
}

proc save_profile {{parent "."}} {
	global is_windows uname
	global profdone
	global include_vars defs
	global ts_only
	global last_load

	globalize
	
	set dir [get_profiles_dir]

	set vncdisp [get_vncdisplay]

	set dispf [string trim $vncdisp]
	if {$dispf != ""} {
		regsub {[ 	].*$} $dispf "" dispf
		regsub -all {/} $dispf "" dispf
	} else {
		global ts_only
		if {$ts_only} {
			mesg "No VNC Terminal Server supplied."
		} else {
			mesg "No VNC Host:Disp supplied."
		}
		bell
		return
	}
	if {$is_windows || $uname == "Darwin"} {
		regsub -all {:} $dispf "-" dispf
	} else {
		regsub -all {:} $dispf "-" dispf
	}
	regsub -all {[\[\]]} $dispf "" dispf
	if {$ts_only && ![regexp {^TS-} $dispf]} {
		set dispf "TS-$dispf"
	}
	if {![regexp {\.vnc$} $dispf]} {
		set dispf "$dispf.vnc"
	}

	set guess $dispf
	if {$last_load != ""} {
		set guess $last_load
	}

	unix_dialog_resize
	set file [tk_getSaveFile -parent $parent -defaultextension ".vnc" \
		-initialdir $dir -initialfile "$guess" -title "Save VNC Profile"]
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
	# XXX host_part
	regsub {:[0-9][0-9]*$} $h "" h
	set host $h
	regsub {[ 	].*$} $p "" p
	regsub {^.*:} $p "" p
	regsub { .*$} $p "" p
	if {$p == ""} {
		set p 0
	} elseif {![regexp {^[-0-9][0-9]*$} $p]} {
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
		regsub {:[0-9][0-9]*$} $h "" h
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
	puts $fh "# parameters commented out with '#' indicate the default setting."

	if {$include_list != ""} {
		load_include $include_list [get_profiles_dir]
	}
	global sshssl_sw
	if {! $use_ssl && ! $use_ssh && ! $use_sshssl} {
		if {$sshssl_sw == "none"} {
			set disable_all_encryption 1
		}
	}

	global ts_only ssh_only
	if {$ts_only} {
		set ts_mode 1
	} else {
		set ts_mode 0
	}
	foreach var [lsort [array names defs]] {
		eval set val \$$var
		set pre ""
		if {$val == $defs($var)} {
			set pre "#"
		}
		if {$ssh_only && $var == "use_ssh"} {
			set pre ""
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

	mesg "Saved Profile: [file tail $file]"

	set last_load [file tail $file]

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

proc rand_port {} {
	global rand_port_list

	set p ""
	for {set i 0} {$i < 30} {incr i} {
		set p [expr 25000 + 35000 * rand()]	
		set p [expr round($p)]
		if {![info exists rand_port_list($p)]} {
			break
		}
	}
	if {$p == ""} {
		unset rand_port_list
		set p [expr 25000 + 35000 * rand()]	
		set p [expr round($p)]
	}
	set rand_port_list($p) 1
	return $p
}

proc get_cups_redir {} {
	global cups_local_server cups_remote_port
	global cups_local_smb_server cups_remote_smb_port

	regsub -all {[ 	]} $cups_local_server "" cups_local_server
	regsub -all {[ 	]} $cups_remote_port "" cups_remote_port
	regsub -all {[ 	]} $cups_local_smb_server "" cups_local_smb_server
	regsub -all {[ 	]} $cups_remote_smb_port "" cups_remote_smb_port

	set redir ""

	if {$cups_local_server != "" && $cups_remote_port != ""} {
		set redir "$cups_remote_port:$cups_local_server"
		regsub -all {['" 	]} $redir {} redir; #"
		set redir " -R $redir"
	}
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
	global ts_only choose_x11vnc_opts
	if {! $additional_port_redirs || $additional_port_redirs_list == ""} {
		return ""
	}
	if {$ts_only && !$choose_x11vnc_opts} {
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
	global sound_daemon_x11vnc

	regsub -all {[ 	]} $sound_daemon_remote_port "" sound_daemon_remote_port
	regsub -all {[ 	]} $sound_daemon_local_port "" sound_daemon_local_port

	set redir ""
	if {$sound_daemon_local_port == "" || $sound_daemon_remote_port == ""} {
		return $redir
	}

	set loc $sound_daemon_local_port
	if {! [regexp {:} $loc]} {
		global uname
		if {$uname == "Darwin"} {
			set loc "127.0.0.1:$loc"
		} else {
			global is_windows
			if {$is_windows} {
				global win_localhost
				set loc "$win_localhost:$loc"
			} else {
				set loc "localhost:$loc"
			}
		}
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
			if [regexp {(.*):([0-9][0-9]*)} $hostport mvar lhost lport] {
				;
			} else {
				set lhost $hostport
				set lport 139
			}
		} else {
			if [regexp {//([^/][^/]*)/} $share mvar h] {
				if [regexp {(.*):([0-9][0-9]*)} $h mvar lhost lport] {
					;
				} else {
					set lhost $h
					set lport 139
				}
			} else {
				global is_windows win_localhost
				set lhost "localhost"
				if {$is_windows} {
					set lhost $win_localhost
				}
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
	DONE_PORT_CHECK=NNNN
	smb_script=$HOME/.smb-mounts__PID__.sh

	DO_SOUND=0
	DO_SOUND_KILL=0
	DO_SOUND_RESTART=0
	sound_daemon_remote_prog=
	sound_daemon_remote_args=

	findpid() {
		db=0
		back=30
		touch $FLAG
		tty=`tty | sed -e "s,/dev/,,"`

		if [ "X$TOPPID" = "X" ]; then
			TOPPID=$$
			if [ $db = 1 ]; then echo "set TOPPID to $TOPPID"; fi
			back=70
		fi
		#back=5
		if [ $db = 1 ]; then echo "TOPPID=$TOPPID THISPID=$$ back=$back"; fi

		i=1
		while [ $i -lt $back ]
		do
			try=`expr $TOPPID - $i`
			if [ $try -lt 1 ]; then
				try=`expr 32768 + $try`
			fi
			if [ $db = 1 ]; then echo try-1=$try; ps $try; fi
			if ps $try 2>/dev/null | grep "sshd.*$USER" | grep "$tty" >/dev/null; then
				if [ $db = 1 ]; then echo Found=$try; fi
				SSHD_PID="$try"	
				echo
				ps $try
				echo
				break
			fi
			i=`expr $i + 1`
		done

		if [ "X$SSHD_PID" = "X" ]; then
			back=`expr $back + 20`
			#back=5

			for fallback in 2 3
			do
				i=1
				while [ $i -lt $back ]
				do
					try=`expr $TOPPID - $i`
					if [ $try -lt 1 ]; then
						try=`expr 32768 + $try`
					fi
					match="sshd.*$USER"
					if [ $fallback = 3 ]; then
						match="sshd"
					fi
					if [ $db = 1 ]; then echo "try-$fallback=$try match=$match"; ps $try; fi
					if ps $try 2>/dev/null | grep "$match" >/dev/null; then
						if [ $db = 1 ]; then echo Found=$try; fi
						SSHD_PID="$try"	
						echo
						ps $try
						echo
						break
					fi
					i=`expr $i + 1`
				done
				if [ "X$SSHD_PID" != "X" ]; then
					break
				fi
			done
		fi
		#curlie}
};

set cmd(2) {
		#curlie{
		if [ "X$SSHD_PID" = "X" ]; then
			if [ $db = 1 ]; then
				echo 
				pstr=`ps -elf | grep "$USER" | grep "$tty" | grep -v grep | grep -v PID | grep -v "ps -elf"`
				echo "$pstr"
			fi
			plist=`ps -elf | grep "$USER" | grep "$tty" | grep -v grep | grep -v PID | grep -v "ps -elf" | awk "{print \\\$4}" | sort -n`
			if [ $db = 1 ]; then
				echo 
				echo "$plist"
			fi
			for try in $plist
			do
				if [ $db = 1 ]; then echo try-final=$try; ps $try; fi
				if echo "$try" | grep "^[0-9][0-9]*\\\$" > /dev/null; then
					:
				else
					continue
				fi
				if ps $try | egrep vnc-helper > /dev/null; then
					:
				else
					if [ $db = 1 ]; then echo Found=$try; fi
					SSHD_PID=$try
					echo
					ps $try
					echo
					break
				fi
			done
		fi
		if [ "X$SSHD_PID" = "X" ]; then
			#ugh
			SSHD_PID=$$
		fi

		echo "vnc-helper: [for cups/smb/esd]      SSHD_PID=$SSHD_PID MY_PID=$$ TTY=$tty"
		echo "vnc-helper: To force me to finish:  rm $FLAG" 
	}

	wait_til_ssh_gone() {
		try_perl=""
		if type perl >/dev/null 2>&1; then
			if [ -d /proc -a -e /proc/$$ ]; then
				try_perl="1"
			fi
		fi
		if [ "X$try_perl" = "X1" ]; then
			# try to avoid wasting pids:
			perl -e "while (1) {if(-d \"/proc\" && ! -e \"/proc/$SSHD_PID\"){exit} if(! -f \"$FLAG\"){exit} sleep 1;}"
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

set cmd(3) {
	update_client_conf() {
		mkdir -p $cups_dir

		if [ ! -f $cups_cfg.back ]; then
			touch $cups_cfg.back
		fi
		if [ ! -f $cups_cfg ]; then
			touch $cups_cfg
		fi

		if grep ssvnc-auto $cups_cfg > /dev/null; then
			:
		else
			cp -p $cups_cfg $cups_cfg.back
		fi

		echo "#-ssvnc-auto:" > $cups_cfg
		sed -e "s/^ServerName/#-ssvnc-auto-#ServerName/" $cups_cfg.back >> $cups_cfg
		echo "ServerName $cups_host:$cups_port" >> $cups_cfg

		echo
		echo "-----------------------------------------------------------------"
		echo "On `hostname`:"
		echo
		echo "The CUPS $cups_cfg config file has been set to:"
		echo
		cat $cups_cfg | grep -v "^#-ssvnc-auto:" | sed -e "s/^/  /"
		echo
		echo "If there are problems automatically restoring it, edit or remove"
		echo "the file to go back to the local CUPS settings."
		echo
		echo "A backup has been placed in: $cups_cfg.back"
		echo
		echo "See the SSVNC CUPS dialog for more details on printing."
		if type lpstat >/dev/null 2>&1; then
			echo
			echo "lpstat -a output:"
			echo
			(lpstat -a 2>&1 | sed -e "s/^/  /") &
			sleep 0.5 >/dev/null 2>&1
		fi
		echo "-----------------------------------------------------------------"
		echo
	}

	reset_client_conf() {
		cp $cups_cfg $cups_cfg.tmp
		grep -v "^ServerName" $cups_cfg.tmp | grep -v "^#-ssvnc-auto:" | sed -e "s/^#-ssvnc-auto-#ServerName/ServerName/" > $cups_cfg
		rm -f $cups_cfg.tmp
	}

	cupswait() {
		trap "" INT QUIT HUP
		trap "reset_client_conf; rm -f $FLAG; exit" TERM
		wait_til_ssh_gone
		reset_client_conf
	}
};

#		if [ "X$DONE_PORT_CHECK" != "X" ]; then
#			if type perl >/dev/null 2>&1; then
#				perl -e "use IO::Socket::INET; \$SIG{INT} = \"IGNORE\"; \$SIG{QUIT} = \"IGNORE\"; \$SIG{HUP} = \"INGORE\"; my \$client = IO::Socket::INET->new(Listen => 5, LocalAddr => \"localhost\", LocalPort => $DONE_PORT_CHECK, Proto => \"tcp\")->accept(); \$line = <\$client>; close \$client; unlink \"$smb_script\";" </dev/null >/dev/null 2>/dev/null &
#				if [ $? = 0 ]; then
#					have_perl_done="1"
#				fi
#			fi
#		fi

set cmd(4) {
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
			echo "echo smbmount $smfs $dest -o uid=$USER,ip=127.0.0.1,port=$port" >> $smb_script
			echo "smbmount \"$smfs\" \"$dest\" -o uid=$USER,ip=127.0.0.1,port=$port" >> $smb_script
			echo "echo; df \"$dest\"; echo" >> $smb_script
			dests="$dests $dest"
		done
		#curlie}
};

set cmd(5) {
		echo "(" >> $smb_script
		echo "un_mnt() {" >> $smb_script
		for dest in $dests
		do
			echo "  echo smbumount $dest" >> $smb_script
			echo "  smbumount \"$dest\"" >> $smb_script
		done
		echo "}" >> $smb_script
		echo "trap \"\" INT QUIT HUP" >> $smb_script
		echo "trap \"un_mnt; exit\" TERM" >> $smb_script

		try_perl=""
		if type perl >/dev/null 2>&1; then
			try_perl=1
		fi
		uname=`uname`
		if [ "X$uname" != "XLinux" -a "X$uname" != "XSunOS" -a "X$uname" != "XDarwin" ]; then
			try_perl=""
		fi

		if [ "X$try_perl" = "X" ]; then
			echo "while [ -f $smb_script ]" >> $smb_script
			echo "do" >> $smb_script
			echo "     sleep 1" >> $smb_script
			echo "done" >> $smb_script
		else
			echo "perl -e \"while (-f \\\"$smb_script\\\") {sleep 1;} exit 0;\"" >> $smb_script
		fi
		echo "un_mnt" >> $smb_script
		echo ") &" >> $smb_script
		echo "-----------------------------------------------------------------"
		echo "On `hostname`:"
		echo
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
			sd="sudo"
			echo "$sd sh $smb_script"
			$sd sh $smb_script
			rc=$?
		fi
};

set cmd(6) {
		#curlie{
		echo
		if [ "$rc" = 0 ]; then
			if [ "X$have_perl_done" = "X1" -o 1 = 1 ] ; then
				echo
				echo "Your SMB shares will be unmounted when the VNC connection closes,"
				echo "*AS LONG AS* No Applications have any of the share files opened or are"
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
			echo 
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
		echo "-----------------------------------------------------------------"
		echo
	}
};

set cmd(7) {

	setup_sound() {
		dpid=""
		d=$sound_daemon_remote_prog
		if type pgrep >/dev/null 2>/dev/null; then
			dpid=`pgrep -U $USER -x $d | head -1`
		else
			dpid=`env PATH=/usr/ucb:$PATH ps wwwwaux | grep -w $USER | grep -w $d | grep -v grep | head -1`
		fi
		echo "-----------------------------------------------------------------"
		echo "On `hostname`:"
		echo
		echo "Setting up Sound: pid=$dpid"
		if [ "X$dpid" != "X" ]; then
			dcmd=`env PATH=/usr/ucb:$PATH ps wwwwaux | grep -w $USER | grep -w $d | grep -w $dpid | grep -v grep | head -1 | sed -e "s/^.*$d/$d/"`
			if [ "X$DO_SOUND_KILL" = "X1" ]; then
				echo "Stopping sound daemon: $sound_daemon_remote_prog $dpid"
				echo "sound cmd: $dcmd"
				kill -TERM $dpid
			fi
		fi
		echo "-----------------------------------------------------------------"
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
		trap "reset_sound; rm -f $FLAG; exit" TERM
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
	echo "--main-vnc-helper-finished--"
	#cat $0
	rm -f $0
	exit 0
};

	set cmdall ""

	for {set i 1} {$i <= 7} {incr i} {
		set v $cmd($i);
		regsub -all "\n" $v "%" v
		regsub -all {.curlie.} $v "" v
		set cmd($i) $v
		append cmdall "echo "
		if {$i == 1} {
			append cmdall {TOPPID=$$ %} 
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

	global use_cups cups_local_server cups_remote_port cups_manage_rcfile ts_only ts_cups_manage_rcfile cups_x11vnc
	regsub -all {[ 	]} $cups_local_server "" cups_local_server
	regsub -all {[ 	]} $cups_remote_port "" cups_remote_port
	if {$use_cups} {
		set dorc 0
		if {$ts_only} {
			if {$ts_cups_manage_rcfile} {
				set dorc 1
			}
		} else {
			if {$cups_manage_rcfile} {
				set dorc 1
			}
		}
		if {$dorc && $mode == "post"} {
			if {$cups_local_server != "" && $cups_remote_port != ""} {
				regsub {DO_CUPS=0} $cmdall {DO_CUPS=1} cmdall
				regsub {cups_port=NNNN} $cmdall "cups_port=$cups_remote_port" cmdall
			}
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

	set cmdstr $cmdall

	if {"$orig" == "$cmdall"} {
		set cmdstr ""
	}
	global env
	if [info exists env(SSVNC_DEBUG_CUPS)] {
		regsub -all {db=0} $cmdstr "db=1" cmdstr
		set pout ""
		regsub -all {%} $cmdstr "\n" pout
		puts stderr "\nSERVICE REDIR COMMAND:\n\n$pout\n"
	}
	return $cmdstr
}

proc ts_unixpw_dialog {} {

	toplev .uxpw
	wm title .uxpw "Use unixpw"

	scroll_text .uxpw.f 80 14

	global ts_unixpw

	set msg {
    This enables the x11vnc unixpw mode.  A Login: and Password: dialog
    will be presented in the VNC Viewer for the user to provide any Unix
    username and password whose session he wants to connect to.  So this
    may require typing in the password a 2nd time after the one for SSH.

    This mode is useful if a shared terminal services user (e.g. 'tsuser')
    is used for the SSH login part (say via the SSH authorized_keys
    mechanism and all users share the same private SSH key for 'tsuser').

    Note, However that the default usage of a per-user SSH login should
    be the simplest and also sufficient for most situations, in which
    case this "Use unixpw" option should NOT be selected.
}
	.uxpw.f.t insert end $msg

	button .uxpw.cancel -text "Cancel" -command {destroy .uxpw; set ts_unixpw 0}
	bind .uxpw <Escape> {destroy .uxpw; set ts_unixpw 0}
	wm protocol .uxpw WM_DELETE_WINDOW {destroy .uxpw; set ts_unixpw 0}

	button .uxpw.done -text "Done" -command {destroy .uxpw; set ts_unixpw 1}

	pack .uxpw.done .uxpw.cancel -side bottom -fill x
	pack .uxpw.f -side top -fill both -expand 1

	center_win .uxpw
}

proc ts_vncshared_dialog {} {

	toplev .vncs
	wm title .vncs "VNC Shared"

	scroll_text .vncs.f 80 23

	global ts_vncshared

	set msg {
    Normal use of this program, 'tsvnc', *ALREADY* allows simultaneous
    shared access of the remote desktop:   You simply log in as many
    times from as many different locations with 'tsvnc' as you like.

    However, doing it that way starts up a new x11vnc for each connection.
    In some circumstances you may want a single x11vnc running but allow
    multiple VNC viewers to access it simultaneously.

    This option (VNC Shared) enables that rarer usage case by passing
    '-shared' to the remote x11vnc command.

    With this option enabled, the new shared connections must
    still connect to the Terminal Server via SSH for encryption and
    authentication.  They must also do the normal SSH port redirection
    to access the x11vnc port (usually 5900, but look for the PORT=
    output for the actual value).

    They could use SSVNC for that, or do it manually in terminal
    windows, more information:

       http://www.karlrunge.com/x11vnc/#tunnelling
}
	.vncs.f.t insert end $msg

	button .vncs.cancel -text "Cancel" -command {destroy .vncs; set ts_vncshared 0}
	bind .vncs <Escape> {destroy .vncs; set ts_vncshared 0}
	wm protocol .vncs WM_DELETE_WINDOW {destroy .vncs; set ts_vncshared 0}
	button .vncs.done -text "Done" -command {destroy .vncs; set ts_vncshared 1}

	pack .vncs.done .vncs.cancel -side bottom -fill x
	pack .vncs.f -side top -fill both -expand 1

	center_win .vncs
}

proc ts_multi_dialog {} {

	toplev .mult
	wm title .mult "Multiple Sessions"

	scroll_text .mult.f 80 21

	global ts_multisession choose_multisession

	set msg {
    Normally in Terminal Services mode (tsvnc) your user account (the
    one you SSH in as) can only have a single Terminal Services X session
    running at a time on one server machine.  

    This is simply because x11vnc chooses the first Desktop (X session)
    of yours that it can find.  It will never create a 2nd X session
    because it keeps finding the 1st one.

    To have Multiple Sessions for one username on a single machine,
    choose a unique Session "Tag", that will be associated with the X
    session and x11vnc will only choose the one that has this Tag. 

    For this to work ALL of your sessions on the server machine must
    have a different tag (that is, if you have an existing session with
    no tag, x11vnc might find a tagged one first instead of it).

    The tag must be made of only letters, numbers, dash, or underscore.

    Examples:  KDE_SMALL,  gnome-2,  test1
}
	.mult.f.t insert end $msg

	frame .mult.c
	label .mult.c.l -anchor w -text "Tag:"
	entry .mult.c.e -width 20 -textvariable ts_multisession
	pack .mult.c.l -side left
	pack .mult.c.e -side left -expand 1 -fill x

	button .mult.cancel -text "Cancel" -command {destroy .mult; set choose_multisession 0}
	bind .mult <Escape> {destroy .mult; set choose_multisession 0}
	wm protocol .mult WM_DELETE_WINDOW {destroy .mult; set choose_multisession 0}

	bind .mult.c.e <Return> {destroy .mult; set choose_multisession 1}
	button .mult.done -text "Done" -command {destroy .mult; set choose_multisession 1}

	pack .mult.done .mult.cancel .mult.c -side bottom -fill x
	pack .mult.f -side top -fill both -expand 1

	center_win .mult
	focus .mult.c.e 
}

proc ts_xlogin_dialog {} {

	toplev .xlog
	wm title .xlog "X Login Greeter"

	set h 33
	if [small_height] {
		set h 28
	}
	scroll_text .xlog.f 80 $h

	global ts_xlogin

	set msg {
    If you have root (sudo(1)) permission on the remote machine, you
    can have x11vnc try to connect to a X display(s) that has No One
    Logged In Yet.  This is most likely the login greeter running on
    the Physical console.  sudo(1) is used to run x11vnc with FD_XDM=1.

    This is different from tsvnc's regular Terminal Services mode where
    usually a virtual (RAM only, e.g. Xvfb) X server used.  With this option
    it is the physical graphics hardware that will be connected to.

    Note that if your user is ALREADY logged into the physical display,
    you don't need to use this X Login option because x11vnc should find
    it in its normal find-display procedure and not need sudo(1).

    An initial ssh running 'sudo id' is performed to try to 'prime' 
    sudo so the 2nd one that runs x11vnc does not need a password.
    This may not always succeed...

    Note that if someone is already logged into the display console
    via XDM (GDM, KDM etc.) you will see and control their X session.

    Otherwise, you will get the Greeter X login screen where you can
    log in via username and password.  Your SSVNC 'Terminal Services'
    Desktop Type, Size, Printing etc. settings will be ignored in this
    case of course because XDM, GDM, or KDM is creating your X session,
    not x11vnc.

    Note that the GDM display manager has a setting KillInitClients in
    gdm.conf that will kill x11vnc right after you log in, and so you would
    have to repeat the whole process ('Connect' button) to attach to your
    session. See http://www.karlrunge.com/x11vnc/faq.html#faq-display-manager
    for more info.
}
	.xlog.f.t insert end $msg

	button .xlog.cancel -text "Cancel" -command {destroy .xlog; set ts_xlogin 0}
	bind .xlog <Escape> {destroy .xlog; set ts_xlogin 0}
	wm protocol .xlog WM_DELETE_WINDOW {destroy .xlog; set ts_xlogin 0}

	button .xlog.done -text "Done" -command {destroy .xlog; set ts_xlogin 1}

	pack .xlog.done .xlog.cancel -side bottom -fill x
	pack .xlog.f -side top -fill both -expand 1

	center_win .xlog
}


proc ts_othervnc_dialog {} {

	toplev .ovnc
	wm title .ovnc "Other VNC Server"

	scroll_text .ovnc.f 80 21

	global ts_othervnc choose_othervnc

	set msg {
    The x11vnc program running on the remote machine can be instructed to
    immediately redirect to some other (3rd party, e.g. Xvnc or vnc.so)
    VNC server.

    It should be a little faster to have x11vnc forward the VNC protocol
    rather than having it poll the corresponding X server for changes
    in the way it normally does and translate to VNC.

    This mode also enables a simple way to add SSL or find X display
    support to a 3rd party VNC Server lacking these features.

    In the entry box put the other vnc display, e.g. "localhost:0" or
    "somehost:5".

    The string "find" in the entry will have x11vnc try to find an X
    display in its normal way, and then redirect to the corresponding VNC
    server port.  This assumes if the X display is, say, :2 (i.e. port
    6002) then the VNC display is also :2 (i.e. port 5902).  This mode is
    the same as an "X Server Type" of "Xvnc.redirect" (and overrides it).
}
	.ovnc.f.t insert end $msg

	frame .ovnc.c
	label .ovnc.c.l -anchor w -text "Other VNC Server:"
	entry .ovnc.c.e -width 20 -textvariable ts_othervnc
	pack .ovnc.c.l -side left
	pack .ovnc.c.e -side left -expand 1 -fill x

	button .ovnc.cancel -text "Cancel" -command {destroy .ovnc; set choose_othervnc 0}
	bind .ovnc <Escape> {destroy .ovnc; set choose_othervnc 0}
	wm protocol .ovnc WM_DELETE_WINDOW {destroy .ovnc; set choose_othervnc 0}
	button .ovnc.done -text "Done" -command {destroy .ovnc; set choose_othervnc 1}
	bind .ovnc.c.e <Return> {destroy .ovnc; set choose_othervnc 1}

	if {$ts_othervnc == ""} {
		set ts_othervnc "find"
	}

	pack .ovnc.done .ovnc.cancel .ovnc.c -side bottom -fill x
	pack .ovnc.f -side top -fill both -expand 1

	center_win .ovnc
	focus .ovnc.c.e 
}

proc ts_sleep_dialog {} {

	toplev .eslp
	wm title .eslp "Extra Sleep"

	scroll_text .eslp.f 80 5

	global extra_sleep

	set msg {
    Sleep: Enter a number to indicate how many extra seconds to sleep
    while waiting for the VNC viewer to start up.  On Windows this
    can give extra time to enter the Putty/Plink password, etc.
}
	.eslp.f.t insert end $msg

	frame .eslp.c
	label .eslp.c.l -anchor w -text "Extra Sleep:"
	entry .eslp.c.e -width 20 -textvariable extra_sleep
	pack .eslp.c.l -side left
	pack .eslp.c.e -side left -expand 1 -fill x

	button .eslp.cancel -text "Cancel" -command {destroy .eslp; set choose_sleep 0}
	bind .eslp <Escape> {destroy .eslp; set choose_sleep 0}
	wm protocol .eslp WM_DELETE_WINDOW {destroy .eslp; set choose_sleep 0}
	button .eslp.done -text "Done" -command {destroy .eslp; set choose_sleep 1}
	bind .eslp.c.e <Return> {destroy .eslp; set choose_sleep 1}

	global choose_sleep
	if {! $choose_sleep} {
		set extra_sleep ""
	}

	pack .eslp.done .eslp.cancel .eslp.c -side bottom -fill x
	pack .eslp.f -side top -fill both -expand 1

	center_win .eslp
	focus .eslp.c.e 
}

proc ts_putty_args_dialog {} {

	toplev .parg
	wm title .parg "Putty Args"

	scroll_text .parg.f 80 5

	global putty_args

	set msg {
    Putty Args: Enter a string to be added to every plink.exe and putty.exe
    command line.  For example: -i C:\mykey.ppk
}
	.parg.f.t insert end $msg

	frame .parg.c
	label .parg.c.l -anchor w -text "Putty Args:"
	entry .parg.c.e -width 20 -textvariable putty_args
	pack .parg.c.l -side left
	pack .parg.c.e -side left -expand 1 -fill x

	button .parg.cancel -text "Cancel" -command {destroy .parg; set choose_parg 0}
	bind .parg <Escape> {destroy .parg; set choose_parg 0}
	wm protocol .parg WM_DELETE_WINDOW {destroy .parg; set choose_parg 0}
	button .parg.done -text "Done" -command {destroy .parg; set choose_parg 1}
	bind .parg.c.e <Return> {destroy .parg; set choose_parg 1}

	global choose_parg
	if {! $choose_parg} {
		set putty_args ""
	}

	pack .parg.done .parg.cancel .parg.c -side bottom -fill x
	pack .parg.f -side top -fill both -expand 1

	center_win .parg
	focus .parg.c.e 
}

proc ts_ncache_dialog {} {

	toplev .nche
	wm title .nche "Client-Side Caching"

	scroll_text .nche.f 80 22

	global ts_ncache choose_ncache

	set msg {
    This enables the *experimental* x11vnc client-side caching mode.
    It often gives nice speedups, but can sometimes lead to painting
    errors or window "flashing". (you can repaint the screen by tapping
    the Left Alt key 3 times in a row)

    It is a very simple but hoggy method: uncompressed image pixmaps are
    stored in the viewer in a large (20-100MB) display region beneath
    the actual display screen.  You may need also to adjust your VNC Viewer
    to not show this region (the SSVNC Unix viewer does it automatically).

    The scheme uses a lot of RAM, but at least it has the advantage that
    it works with every VNC Viewer.  Otherwise the VNC protocol would
    need to be modified, changing both the server and the viewer.

    Set the x11vnc "-ncache" parameter to an even integer between 2
    and 20.  This is the increase in area factor over the normal screen
    for the caching region.  So 10 means use 10 times the RAM to store
    pixmaps.  The default is 8.

    More info: http://www.karlrunge.com/x11vnc/faq.html#faq-client-caching
}
	.nche.f.t insert end $msg

	frame .nche.c
	label .nche.c.l -anchor w -text "ncache:"
	radiobutton .nche.c.r2  -text "2"  -variable ts_ncache -value "2"
	radiobutton .nche.c.r4  -text "4"  -variable ts_ncache -value "4"
	radiobutton .nche.c.r6  -text "6"  -variable ts_ncache -value "6"
	radiobutton .nche.c.r8  -text "8"  -variable ts_ncache -value "8"
	radiobutton .nche.c.r10 -text "10" -variable ts_ncache -value "10"
	radiobutton .nche.c.r12 -text "12" -variable ts_ncache -value "12"
	radiobutton .nche.c.r14 -text "14" -variable ts_ncache -value "14"
	radiobutton .nche.c.r16 -text "16" -variable ts_ncache -value "16"
	radiobutton .nche.c.r18 -text "18" -variable ts_ncache -value "18"
	radiobutton .nche.c.r20 -text "20" -variable ts_ncache -value "20"
	pack .nche.c.l -side left
	pack .nche.c.r2 .nche.c.r4 .nche.c.r6 .nche.c.r8 .nche.c.r10 \
		.nche.c.r12 .nche.c.r14 .nche.c.r16 .nche.c.r18  .nche.c.r20 -side left
	button .nche.cancel -text "Cancel" -command {destroy .nche; set choose_ncache 0}
	bind .nche <Escape> {destroy .nche; set choose_ncache 0}
	wm protocol .nche WM_DELETE_WINDOW {destroy .nche; set choose_ncache 0}
	button .nche.done -text "Done" -command {destroy .nche; set choose_ncache 1}

	pack .nche.done .nche.cancel .nche.c -side bottom -fill x
	pack .nche.f -side top -fill both -expand 1

	center_win .nche
}

proc ts_x11vnc_opts_dialog {} {

	toplev .x11v
	wm title .x11v "x11vnc Options"

	set h 23
	if [small_height] {
		set h 21
	}
	scroll_text .x11v.f 80 $h

	global ts_x11vnc_opts ts_x11vnc_path ts_x11vnc_autoport choose_x11vnc_opts
	global additional_port_redirs_list

	set msg {
    If you are an expert with x11vnc's endless options and tweaking
    parameters feel free to specify any you want here in "Options".

    Also, if you need to specify the path to the x11vnc program on the
    remote side because it will not be in $PATH, put it in the "Full
    Path" entry.

    Port Redirs are additional SSH "-L port:host:port" or "-R port:host:port"
    (forward or reverse, resp.) port redirections you want.  In SSVNC mode,
    see the detailed description under: Options -> Advanced -> Port Redirs.

    Some potentially useful options:

	-solid		-scale		-scale_cursor
	-passwd		-rfbauth	-http
	-xrandr		-rotate		-noxdamage
	-xkb		-skip_lockkeys	-nomodtweak
	-repeat		-cursor		-wmdt
	-nowireframe	-ncache_cr	-speeds

    More info: http://www.karlrunge.com/x11vnc/faq.html#faq-cmdline-opts
}
#    In Auto Port put a starting port for x11vnc to try autoprobing
#    instead of the default 5900.  It starts at the value you supply and
#    works upward until a free one is found. (x11vnc 0.9.3 or later).

	.x11v.f.t insert end $msg

	frame .x11v.c
	label .x11v.c.l -width 10 -anchor w -text "Options:"
	entry .x11v.c.e -textvariable ts_x11vnc_opts
	pack .x11v.c.l -side left
	pack .x11v.c.e -side left -expand 1 -fill x

	frame .x11v.c2
	label .x11v.c2.l -width 10 -anchor w -text "Full Path:"
	entry .x11v.c2.e -textvariable ts_x11vnc_path
	pack .x11v.c2.l -side left
	pack .x11v.c2.e -side left -expand 1 -fill x

#	frame .x11v.c3
#	label .x11v.c3.l -width 10 -anchor w -text "Auto Port:"
#	entry .x11v.c3.e -textvariable ts_x11vnc_autoport
#	pack .x11v.c3.l -side left
#	pack .x11v.c3.e -side left -expand 1 -fill x

	frame .x11v.c4
	label .x11v.c4.l -width 10 -anchor w -text "Port Redirs:"
	entry .x11v.c4.e -textvariable additional_port_redirs_list
	pack .x11v.c4.l -side left
	pack .x11v.c4.e -side left -expand 1 -fill x

	button .x11v.cancel -text "Cancel" -command {destroy .x11v; set choose_x11vnc_opts 0}
	bind .x11v <Escape> {destroy .x11v; set choose_x11vnc_opts 0}
	wm protocol .x11v WM_DELETE_WINDOW {destroy .x11v; set choose_x11vnc_opts 0}
	button .x11v.done -text "Done" -command {destroy .x11v; set choose_x11vnc_opts 1;
			if {$additional_port_redirs_list != ""} {set additional_port_redirs 1} else {set additional_port_redirs 0}}

#	pack .x11v.done .x11v.cancel .x11v.c4 .x11v.c3 .x11v.c2 .x11v.c -side bottom -fill x
	pack .x11v.done .x11v.cancel .x11v.c4 .x11v.c2 .x11v.c -side bottom -fill x
	pack .x11v.f -side top -fill both -expand 1

	center_win .x11v
	focus .x11v.c.e 
}


proc ts_filexfer_dialog {} {

	toplev .xfer
	wm title .xfer "File Transfer"
	global choose_filexfer ts_filexfer

	scroll_text .xfer.f 70 13

	set msg {
    x11vnc supports both the UltraVNC and TightVNC file transfer
    extensions.  On Windows both viewers support their file transfer
    protocol.  On Unix only the SSVNC VNC Viewer can do filexfer; it
    supports the UltraVNC flavor via a Java helper program (and so
    java(1) is required on the viewer-side).

    Choose the one you want based on VNC viewer you will use.
    The defaults for the SSVNC viewer package are TightVNC on
    Windows and UltraVNC on Unix.

    For more info see: http://www.karlrunge.com/x11vnc/faq.html#faq-filexfer
}
	.xfer.f.t insert end $msg

	global is_windows
	if {$ts_filexfer == ""} {
		if {$is_windows} {
			set ts_filexfer "tight"
		} else {
			set ts_filexfer "ultra"
		}
	}

	frame .xfer.c
	radiobutton .xfer.c.tight  -text "TightVNC"  -variable ts_filexfer -value "tight" -relief ridge
	radiobutton .xfer.c.ultra  -text "UltraVNC"  -variable ts_filexfer -value "ultra" -relief ridge

	pack .xfer.c.ultra .xfer.c.tight -side left -fill x -expand 1

	button .xfer.cancel -text "Cancel" -command {destroy .xfer; set choose_filexfer 0}
	bind .xfer <Escape> {destroy .xfer; set choose_filexfer 0}
	wm protocol .xfer WM_DELETE_WINDOW {destroy .xfer; set choose_filexfer 0}
	button .xfer.done -text "Done" -command {destroy .xfer; set choose_filexfer 1}

	pack .xfer.done .xfer.cancel -side bottom -fill x
	pack .xfer.c -side bottom -fill x -expand 1
	pack .xfer.f -side top -fill both -expand 1

	center_win .xfer
}

proc ts_cups_dialog {} {

	toplev .cups
	wm title .cups "CUPS and SMB Printing"
	global cups_local_server cups_remote_port cups_manage_rcfile ts_cups_manage_rcfile cups_x11vnc
	global cups_local_smb_server cups_remote_smb_port

	set h 30
	if [small_height] {
		set h 24
	}
	scroll_text .cups.f 80 $h
		

	set msg {
    This method requires a working CUPS Desktop setup on the remote side
    of the connection and working CUPS (or possibly Windows SMB or IPP)
    printing on the local viewer-side of the connection.

    For CUPS printing redirection to work properly, you MUST enable it for
    the connection that *creates* your terminal services X session (i.e. the
    first connection.)  You cannot retroactively enable CUPS redirection
    on an already existing terminal services X session.  (See CUPS printing
    for normal SSVNC mode for how you might do that.)

    Enter the VNC Viewer side (i.e. where you are sitting) CUPS server
    under "Local CUPS Server".  Use "localhost:631" if there is one
    on your viewer machine (normally the case if you set up a printer
    on your unix or macosx system), or, e.g., "my-print-srv:631" for a
    nearby CUPS print server.  Note that 631 is the default CUPS port.

    (On MacOSX it seems better to use "127.0.0.1" instead of "localhost".)

    The SSVNC Terminal Services created remote Desktop session will have
    the variables CUPS_SERVER and IPP_PORT set so all printing applications
    will be redirected to your local CUPS server.  So your locally available
    printers should appear in the remote print dialogs.


    Windows/SMB Printers:  Under "Local SMB Print Server" you can set a
    port redirection for a Windows (non-CUPS) SMB printer.  If localhost:139
    does not work, try the literal string "IP:139", or use the known value
    of the IP address manually.  139 is the default SMB port; nowadays 445
    might be a better possibility.

    For Windows/SMB Printers if there is no local CUPS print server, it is
    usually a very good idea to make the CUPS Server setting EMPTY (to avoid
    desktop apps trying incessantly to reach the nonexistent CUPS server.)

    On the remote side, in the Desktop session the variables $SMB_SERVER,
    $SMB_HOST, and $SMB_PORT will be set for you to use.

    Unfortunately, printing to Windows may only ve partially functional due
    to the general lack PostScript support on Windows.

    If you have print admin permission on the remote machine you can
    configure CUPS to know about your Windows printer via lpadmin(8) or
    a GUI tool.  You give it the URI:

        smb://localhost:port/printername

    or possibly:

        smb://localhost:port/computer/printername

    "port" will be found in the $SMB_PORT.  You also need to identify
    the printer type.  NOTE: You will leave "Local CUPS Server" blank in
    this case.  The smbspool(1) command should also work as well, at least
    for PostScript printers.

    A similar thing can be done with CUPS printers if you are having problems
    with the above default mechanism.  Use

        http://localhost:port/printers/printername

    For more info see: http://www.karlrunge.com/x11vnc/faq.html#faq-cups
}

#    The "Manage 'ServerName' in .cups/client.conf for me" setting is usually
#    NOT needed unless you are using Terminal Services to connect to an
#    existing Session that did NOT have CUPS print redirection set at session
#    start time (i.e. IPP_PORT and CUPS_SERVER were not set up).  In that
#    case, select this option as a workaround: NOTE that the client.conf
#    setting will REDIRECT ALL PRINTING for apps with the same $HOME/.cups
#    directory (which you probably do not want), however it will be reset
#    when the SSVNC viewer disconnects.

	.cups.f.t insert end $msg

	global uname
	if {$cups_local_server == ""} {
		if {$uname == "Darwin"} {
			set cups_local_server "127.0.0.1:631"
		} else {
			set cups_local_server "localhost:631"
		}
	}
	if {$cups_remote_port == ""} {
		set cups_remote_port [expr "6731 + int(1000 * rand())"]
	}
	if {$cups_local_smb_server == ""} {
		global is_windows
		if {$is_windows} {
			set cups_local_smb_server "IP:139"
		} elseif {$uname == "Darwin"} {
			set cups_local_smb_server "127.0.0.1:139"
		} else {
			set cups_local_smb_server "localhost:139"
		}
	}
	if {$cups_remote_smb_port == ""} {
		set cups_remote_smb_port [expr "7731 + int(1000 * rand())"]
	}

	frame .cups.serv
	label .cups.serv.l -anchor w -text "Local CUPS Server:      "
	entry .cups.serv.e -width 40 -textvariable cups_local_server
	pack .cups.serv.e -side right
	pack .cups.serv.l -side left -expand 1 -fill x

	frame .cups.smbs
	label .cups.smbs.l -anchor w -text "Local SMB Print Server:      "
	entry .cups.smbs.e -width 40 -textvariable cups_local_smb_server
	pack .cups.smbs.e -side right
	pack .cups.smbs.l -side left -expand 1 -fill x

	# not working with x11vnc:
	checkbutton .cups.cupsrc -anchor w -variable ts_cups_manage_rcfile -text \
		"Manage 'ServerName' in the remote \$HOME/.cups/client.conf file for me"

	button .cups.cancel -text "Cancel" -command {destroy .cups; set use_cups 0}
	bind .cups <Escape> {destroy .cups; set use_cups 0}
	wm protocol .cups WM_DELETE_WINDOW {destroy .cups; set use_cups 0}
	button .cups.done -text "Done" -command {destroy .cups; if {$use_cups} {set_ssh}}

	pack .cups.done .cups.cancel .cups.smbs .cups.serv -side bottom -fill x
	pack .cups.f -side top -fill both -expand 1

	center_win .cups
	focus .cups.serv.e
}


proc cups_dialog {} {

	toplev .cups
	wm title .cups "CUPS Tunnelling"
	global cups_local_server cups_remote_port cups_manage_rcfile cups_x11vnc
	global cups_local_smb_server cups_remote_smb_port
	global ts_only
	if {$ts_only} {
		ts_cups_dialog
		return
	}

	global uname
	set h 33
	if [small_height] {
		set h 17
	} elseif {$uname == "Darwin"} {
		set h 24
	}
	scroll_text .cups.f 80 $h
		

	set msg {
    CUPS Printing requires SSH be used to set up the CUPS Print service TCP
    port redirection.  This will be either of the "Use SSH" or "SSH+SSL" modes.
    NOTE:  For pure SSL tunnelling it currently will not work.

    This method requires working CUPS software setups on BOTH the remote
    and local sides of the connection.

    If the remote VNC server is Windows you probably cannot SSH into it
    anyway...  If you can, you will still need to set up a special printer
    TCP port redirection on your own.  Perhaps adding and configuring a
    "Unix Printer" under Windows (like Method #2 below) will work.

    If the local machine (SSVNC side) is Windows, see the bottom of this
    help for redirecting to SMB printers.

    If the remote VNC server is Mac OS X this method may or may not work.
    Sometimes applications need to be restarted to get them to notice the
    new printers.  Adding and configuring a special "Unix Printer",
    (Method #2) below, might yield more reliable results at the cost of
    additional setup and permissions.

    For Unix/Linux remote VNC servers, applications may also need to be
    restarted to notice the new printers.  The only case known to work
    well is the one where the remote side has no CUPS printers configured.
    As mentioned above, see Method #2 for another method.

    *************************************************************************
    *** Directions:

    You choose your own remote CUPS redir port below under "Use Remote
    CUPS Port".  6631 is our default and is used in the examples below.
    Use it or some random value greater than 1024.  Note that the standard
    CUPS server port is 631.

    The port you choose must be unused on the VNC server machine (it is NOT
    checked for you).  Print requests connecting to it are redirected to
    your local VNC viewer-side CUPS server through the SSH tunnel.

    (Note: root SSH login permission is needed for ports less than 1024,
    e.g. 631; this is not recommended, use something around 6631 instead).

    Then enter the VNC Viewer side (i.e. where you are sitting) CUPS server
    into "Local CUPS Server".  A good choice is the default "localhost:631"
    if there is a cups server on your viewer machine (this is usually the case
    if you have set up a printer).  Otherwise enter, e.g., "my-print-srv:631"
    for your nearby (viewer-side) CUPS print server.


    The "Manage 'ServerName' in the $HOME/.cups/client.conf file for me"
    setting below is enabled by default.  It should handle most situations.

    What it does is modify the .cups/client.conf file on the VNC server-side
    to redirect the print requests while the SSVNC viewer is connected.  When
    SSVNC disconnects .cups/client.conf is restored to its previous setting.

    If, for some reason, the SSVNC CUPS script fails to restore this file
    after SSVNC disconnects, run this command on the remote machine:

        cp $HOME/.cups/client.conf.back $HOME/.cups/client.conf

    to regain your initial printing configuration.


    You can also use CUPS on the VNC server-side to redirect to Windows
    (SMB) printers.  See the additional info for Windows Printing at the
    bottom of this help.


    In case the default method (automatic .cups/client.conf modification)
    fails, we describe below all of the possible methods that can be tried.

    As noted above, you may need to restart applications for them to notice
    the new printers or for them to revert to the original printers.  If this
    is not acceptable, consider Method #2 below if you have the permission
    and ability to alter the print queues for this.


    *************************************************************************
    *** Method #1:  Manually create or edit the file $HOME/.cups/client.conf
    on the VNC server side by putting in something like this in it:

    	ServerName localhost:6631

    based on the port you set in this dialog's entry box.
    
    After the remote VNC Connection is finished, to go back to the non-SSH
    tunnelled CUPS server and either remove the client.conf file or comment
    out the ServerName line.  This restores the normal CUPS server for
    you on the remote VNC server machine.

    Select "Manage 'ServerName' in the $HOME/.cups/client.conf file for me"
    to do this editing of the VNC server-side CUPS config file for you
    automatically.  NOTE: It is now on by default (deselect it if you want
    to manage the file manually; e.g. you print through the tunnel only very
    rarely, or often print locally when the tunnel is up, etc.)

    Select "Pass -env FD_CUPS=<Port> to x11vnc command line" if you are
    starting x11vnc as the Remote SSH Command, and x11vnc is running in
    -create mode (i.e. FINDCREATEDISPLAY).  That way, when your X session
    is created IPP_PORT will be set correctly for the entire session.
    This is the mode used for 'Terminal Services' printing.

    NOTE: You probably would never select both of the above two options
    at the same time, since they conflict with each other to some degree.


    *************************************************************************
    *** Method #2:  If you have admin permission on the VNC Server machine
    you can likely "Add a Printer" via a GUI dialog, a Wizard, CUPS Web
    interface (i.e. http://localhost:631/), lpadmin(8), etc.

    You will need to tell the dialog that the network printer located
    is at, e.g., localhost:6631, and anything else needed to identify
    the printer (type, model, etc).  NOTE: sometimes it is best to set
    the model/type as "Generic / Postscript Printer" to avoid problems
    with garbage being printed out.

    For the URI to use, we have successfully used ones like this with CUPS:

       http://localhost:6631/printers/Deskjet-3840
        ipp://localhost:6631/printers/Deskjet-3840

    for an HP Deskjet-3840 printer.  See the CUPS documentation for more
    about the URI syntax and pathname.

    This mode makes the client.conf ServerName parameter unnecessary
    (BE SURE TO DISABLE the "Manage 'ServerName' ... for me"  option.)


    *************************************************************************
    *** Method #3:  Restarting individual applications with the IPP_PORT
    set will enable redirected printing for them, e.g.:

       env IPP_PORT=6631 firefox

    If you can only get this method to work, an extreme application would
    be to run the whole desktop, e.g. "env IPP_PORT=6631 gnome-session", but
    then you would need some sort of TCP redirector (ssh -L comes to mind),
    to direct it to 631 when not connected remotely.


    *************************************************************************
    *** Windows/SMB Printers:  Under "Local SMB Print Server" you can set
    a port redirection for a Windows (non-CUPS) SMB printer.  E.g. port
    6632 -> localhost:139.

    If localhost:139 does not work, try the literal string "IP:139", or
    insert the actual IP address manually.  NOTE: Nowadays on Windows port
    445 might be a better choice.

    For Windows printers, if there is no local CUPS print server, set the
    'Local CUPS Server' and 'Use Remote CUPS Port' to be EMPTY (to avoid
    desktop apps trying incessantly to reach the nonexistent CUPS server.)

    You must enable Sharing for your local Windows Printer.  Use Windows
    Printer configuration dialogs to do this.

    Next, you need to have sudo or print admin permission so that you can
    configure the *remote* CUPS to know about this Windows printer via
    lpadmin(8) or GUI Printer Configuration dialog, etc (Method #2 above).
    You basically give it the URI:

        smb://localhost:6632/printername

    For example, we have had success with GNOME CUPS printing configuration
    using:

	smb://localhost:6632/HPOffice
	smb://localhost:6632/COMPUTERNAME/HPOffice

    where "HPOffice" was the name Windows shares the printer as.

    Also with this SMB port redir mode, as a last resort you can often print
    using the smbspool(8) program like this:

       smbspool smb://localhost:6632/printer job user title 1 "" myfile.ps

    You could put this in a script.  For this URI, it appears only the number
    of copies ("1" above) and the file itself are important.

    If on the local (SSVNC viewer) side there is some nearby CUPS print server
    that knows about your Windows printer, you might have better luck with
    that instead of using SMB.  Set 'Local CUPS Server' to it.

    For more info see: http://www.karlrunge.com/x11vnc/faq.html#faq-cups
}
	.cups.f.t insert end $msg

	global uname
	set something_set 0

	if {$cups_local_server != ""} {
		set something_set 1
	}
	if {$cups_local_smb_server != ""} {
		set something_set 1
	}

	if {$cups_local_server == "" && ! $something_set} {
		if {$uname == "Darwin"} {
			set cups_local_server "127.0.0.1:631"
		} else {
			set cups_local_server "localhost:631"
		}
	}
	if {$cups_remote_port == "" && ! $something_set} {
		set cups_remote_port "6631"
	}
	if {$cups_local_smb_server == "" && ! $something_set} {
		global is_windows
		if {$is_windows} {
			set cups_local_smb_server "IP:139"
		} elseif {$uname == "Darwin"} {
			set cups_local_smb_server "127.0.0.1:139"
		} else {
			set cups_local_smb_server "localhost:139"
		}
	}
	if {$cups_remote_smb_port == "" && ! $something_set} {
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
		"Manage 'ServerName' in the remote \$HOME/.cups/client.conf file for me"

	checkbutton .cups.x11vnc -anchor w -variable cups_x11vnc -text \
		"Pass -env FD_CUPS=<Port> to x11vnc command line."

	button .cups.cancel -text "Cancel" -command {destroy .cups; set use_cups 0}
	bind .cups <Escape> {destroy .cups; set use_cups 0}
	wm protocol .cups WM_DELETE_WINDOW {destroy .cups; set use_cups 0}
	button .cups.done -text "Done" -command {destroy .cups; if {$use_cups} {set_ssh}}

	button .cups.guess -text "Help me decide ..." -command {}
	.cups.guess configure -state disabled

	pack .cups.done .cups.cancel .cups.guess .cups.x11vnc .cups.cupsrc .cups.smbp .cups.smbs .cups.port .cups.serv -side bottom -fill x
	pack .cups.f -side top -fill both -expand 1

	center_win .cups
	focus .cups.serv.e
}

proc ts_sound_dialog {} {

	global is_windows
	global ts_only

	toplev .snd
	wm title .snd "Sound Tunnelling"

	scroll_text .snd.f 80 21

	set msg {
    Your remote Desktop will be started in an Enlightenment Sound Daemon
    (ESD) environment (esddsp(1), which must be installed on the remote
    machine), and a local ESD sound daemon (esd(1)) will be started to
    play the sounds for you to hear.

    In the entry box below you can choose the port that the local esd
    will use to listen on.  The default ESD port is 16001.  You will
    need to choose different values if you will have more than one esd
    running locally.

    The command run (with port replaced by your choice) will be:

      %RCMD

    Note: Unfortunately not all applications work with ESD.
          And esd's LD_PRELOAD is broken on 64+32bit Linux (x86_64).
          And so this mode is not working well currently... 
 
    For more info see: http://www.karlrunge.com/x11vnc/faq.html#faq-sound
}


	global sound_daemon_remote_port sound_daemon_local_port sound_daemon_local_cmd
	global sound_daemon_local_start sound_daemon_local_kill

	set sound_daemon_local_start 1
	set sound_daemon_local_kill 1

	if {$sound_daemon_remote_port == ""} {
		set sound_daemon_remote_port 16010
	}
	if {$sound_daemon_local_port == ""} {
		set sound_daemon_local_port 16010
	}

	if {$sound_daemon_local_cmd == ""} {
		global is_windows
		if {$is_windows} {
			set sound_daemon_local_cmd {esound\esd -promiscuous -as 5 -port %PORT -tcp -bind 127.0.0.1}
		} else {
			set sound_daemon_local_cmd {esd -promiscuous -as 5 -port %PORT -tcp -bind 127.0.0.1}
		}
	}
	regsub {%PORT} $sound_daemon_local_cmd $sound_daemon_local_port sound_daemon_local_cmd

	regsub {%RCMD} $msg $sound_daemon_local_cmd msg
	.snd.f.t insert end $msg

	frame .snd.lport
	label .snd.lport.l -anchor w -text "Local Sound Port:     "
	entry .snd.lport.e -width 45 -textvariable sound_daemon_local_port
	pack .snd.lport.e -side right
	pack .snd.lport.l -side left -expand 1 -fill x

	button .snd.cancel -text "Cancel" -command {destroy .snd; set use_sound 0}
	bind .snd <Escape> {destroy .snd; set use_sound 0}
	wm protocol .snd WM_DELETE_WINDOW {destroy .snd; set use_sound 0}
	button .snd.done -text "Done" -command {destroy .snd; if {$use_sound} {set_ssh}}
	bind .snd.lport.e <Return> {destroy .snd; if {$use_sound} {set_ssh}}

	pack .snd.done .snd.cancel .snd.lport -side bottom -fill x
	pack .snd.f -side bottom -fill both -expand 1

	center_win .snd
	focus .snd.lport.e
}

proc sound_dialog {} {

	global is_windows
	global ts_only
	if {$ts_only} {
		ts_sound_dialog;
		return
	}

	toplev .snd
	wm title .snd "ESD/ARTSD Sound Tunnelling"

	global uname
	set h 28
	if [small_height] {
		set h 14
	} elseif {$uname == "Darwin"} {
		set h 20
	}
	scroll_text .snd.f 80 $h

	set msg {
    Sound tunnelling to a sound daemon requires SSH be used to set up the
    service port redirection.  This will be either of the "Use SSH" or
    "SSH+SSL" modes. NOTE: For pure SSL tunnelling it currently will not work.

    This method requires working Sound daemon (e.g. ESD or ARTSD) software
    setups on BOTH the remote and local sides of the connection.

    Often this means you want to run your ENTIRE remote desktop with ALL
    applications instructed to use the sound daemon's network port.  E.g.

        esddsp -s localhost:16001  startkde
        esddsp -s localhost:16001  gnome-session

    and similarly for artsdsp, etc.  You put this in your ~/.xession,
    or other startup file.  This is non standard.  If you do not want to
    do this you still can direct *individual* sound applications through
    the tunnel, for example "esddsp -s localhost:16001 soundapp", where
    "soundapp" is some application that makes noise (say xmms or mpg123).

    Select "Pass -env FD_ESD=<Port> to x11vnc command line."  if you are
    starting x11vnc as the Remote SSH Command, and x11vnc is running in
    -create mode (i.e. FINDCREATEDISPLAY).  That way, your X session is
    started via "esddsp -s ... <session>"  and the ESD variables will be
    set correctly for the entire session.  (This mode make most sense for
    a virtual, e.g. Xvfb or Xdummy session, not one a physical display).

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

    For more info see: http://www.karlrunge.com/x11vnc/faq.html#faq-sound
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

	checkbutton .snd.x11vnc -anchor w -variable sound_daemon_x11vnc -text \
		"Pass -env FD_ESD=<Port> to x11vnc command line."

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
	wm protocol .snd WM_DELETE_WINDOW {destroy .snd; set use_sound 0}
	button .snd.done -text "Done" -command {destroy .snd; if {$use_sound} {set_ssh}}

	pack .snd.done .snd.cancel .snd.guess .snd.x11vnc .snd.sdkl .snd.sdsl .snd.sdr .snd.sdk .snd.lport .snd.rport \
		.snd.local .snd.remote -side bottom -fill x
	pack .snd.f -side bottom -fill both -expand 1

	center_win .snd
	focus .snd.remote.e
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

	set h 40
	if [small_height] {
		set h 30
	}
	scroll_text .smbwiz.f 100 $h

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
	set h 33
	if [small_height] {
		set h 17
	} elseif {$uname == "Darwin"} {
		set h 24
	}
	scroll_text .smb.f 80 $h

	set msg {
    Windows/Samba Filesystem mounting requires SSH be used to set up the SMB
    service port redirection.  This will be either of the "Use SSH" or
    "SSH+SSL" modes. NOTE: For pure SSL tunnelling it currently will not work.

    This method requires a working Samba software setup on the remote
    side of the connection (VNC server) and existing Samba or Windows file
    server(s) on the local side (VNC viewer).

    The smbmount(8) program MUST be installed on the remote side. This
    evidently limits the mounting to Linux systems.  Let us know of similar
    utilities on other Unixes.  Mounting onto remote Windows machines is
    currently not supported (our SSH mode with services setup only works
    to Unix).  On Debian and Ubuntu the smbmount program is currently in
    the package named 'smbfs'.

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

    For more info see: http://www.karlrunge.com/x11vnc/faq.html#faq-smb-shares
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
	wm protocol .smb WM_DELETE_WINDOW {set use_smbmnt 0; destroy .smb}
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

    Brief (and some not so brief) descriptions:

      CUPS Print tunnelling:

         Redirect localhost:6631 (say) on the VNC server to your local
         CUPS server.  SSH mode is required.

      ESD/ARTSD Audio tunnelling:

         Redirect localhost:16001 (say) on the VNC server to your local
         ESD, etc. sound server.  SSH mode is required.

      SMB mount tunnelling:

         Redirect localhost:1139 (say) on the VNC server and through that
         mount SMB file shares from your local server.  The remote machine
         must be Linux with smbmount installed. SSH mode is required.

      Additional Port Redirs (via SSH):

         Specify additional -L port:host:port and -R port:host:port
         cmdline options for SSH to enable additional services.
         SSH mode is required.

      Automatically Find X Login/Greeter:

         This mode is similar to "Automatically Find X Session" except
         that it will attach to a X Login/Greeter screen that no one
         has logged into yet.  It requires root privileges via sudo(1)
         on the remote machine.  SSH mode is required.

         As with "Automatically Find X Session" it works only with SSH
         mode and requires x11vnc be installed on the remote computer.

         It simply sets the Remote SSH Command to:

              PORT= sudo x11vnc -find -localhost -env FD_XDM=1

         An initial ssh running 'sudo id' is performed to try to
         'prime' sudo so the 2nd one that runs x11vnc does not need
         a password.  This may not always succeed... please mail us
         the details if it doesn't.

         See the 'X Login' description in 'Terminal Services' Mode
         Help for more info.

      Private SSH KnownHosts file:

         On Unix in SSH mode, let the user specify a non-default
         ssh known_hosts file to be used only by the current profile.
         This is the UserKnownHostsFile ssh option and is described in the
         ssh_config(1) man page.  This is useful to avoid proxy 'localhost'
         SSH key collisions.

         Normally one should simply let ssh use its default file
         ~/.ssh/known_hosts for tracking SSH keys.  The only problem that
         happens is when multiple SSVNC connections use localhost tunnel
         port redirections.  These make ssh connect to 'localhost' on some
         port (where the proxy is listening.)  Then the different keys
         from the multiple ssh servers collide when ssh saves them under
         'localhost' in ~/.ssh/known_hosts.

         So if you are using a proxy with SSVNC or doing a "double SSH
         gateway" your ssh will connect to a proxy port on localhost, and you
         should set a private KnownHosts file for that connection profile.
         This is secure and avoids man-in-the-middle attack (as long as
         you actually verify the initial save of the SSH key!)

         The default file location will be:

                  ~/.vnc/ssh_known_hosts/profile-name.known

         but you can choose any place you like.  It must of course be
         unique and not shared with another ssh connection otherwise they
         both may complain about the key for 'localhost' changing, etc.

      SSH Local Port Protections:

         An LD_PRELOAD hack to limit the number of SSH port redirections
         to 1 and within the first 35 seconds.  So there is a smaller
         window when the user can try to use your tunnel compared to
         the duration of your session.  SSH mode is required.

      STUNNEL Local Port Protections:

         Try to prevent Untrusted Local Users (see the main Help panel)
         from using your STUNNEL tunnel to connect to the remote VNC
         Server.

      Change VNC Viewer:

         Specify a non-bundled VNC Viewer (e.g.  UltraVNC or RealVNC)
         to run instead of the bundled TightVNC Viewer.

      Port Knocking:

         For "closed port" services, first "knock" on the firewall ports
         in a certain way to open the door for SSH or SSL.  The port
         can also be closed when the encrypted VNC connection finishes.

      UltraVNC DSM Encryption Plugin:

         On Unix only, by using the supplied tool, ultravnc_dsm_helper,
         encrypted connections to UltraVNC servers using their plugins
         is enabled.  Support for secret key encryption to Non-UltraVNC
         DSM servers is also supported, e.g. x11vnc -enc blowfish:my.key

      Do not Probe for VeNCrypt:

         Disable VeNCrypt auto-detection probe when not needed.

         By default in SSL mode an initial probe for the use of the
         VeNCrypt or ANONTLS protocol is performed.  This is done
         during the initial fetch-cert action.  Once auto-detected in
         the initial probe, the real connection to the VNC Server will
         use this information to switch to SSL/TLS at the right point in
         the VeNCrypt/ANONTLS handshake.

         In "Verify All Certs" mode initial the fetch-cert action is
         required so the automatic probing for VeNCrypt is always done.
         The fetch-cert is not needed if you specified a ServerCert or if
         you disabled "Verify All Certs".  But by default the fetch-cert
         is done anyway to try to auto-detect VeNCrypt/ANONTLS.

         Set 'Do not Probe for VeNCrypt' to skip this unneeded fetch-cert
         action (and hence speed up connecting.)  Use this if you
         know the VNC Server uses normal SSL and not VeNCrypt/ANONTLS.

         See also the next option, 'Server uses VeNCrypt SSL encryption'
         to if you know it uses VeNCrypt/ANONTLS (the probing will also
         be skipped if that option is set.)
         
      Server uses VeNCrypt SSL encryption:

         Indicate that the VNC server uses the VeNCrypt extension to VNC;
         it switches to an SSL/TLS tunnel at a certain point in the
         VNC Handshake.  This is in constrast to the default ssvnc/x11vnc
         SSL tunnel behavior where the *entire* VNC traffic goes through
         SSL (i.e. it is vncs:// in the way https:// uses SSL)

         Enable this option if you know the server supports VeNCrypt.
         Also use this option for the older ANONTLS extension (vino).
         Doing so will give the quickest and most reliable connection
         to VeNCrypt/ANONTLS servers.  If set, any probing to try to
         auto-detect VeNCrypt/ANONTLS will be skipped.

         Some VNC servers supporting VeNCrypt: VeNCrypt, QEMU, ggi,
         virt-manager, and Xen.  Vino supports ANONTLS.

         The SSVNC VeNCrypt/ANONTLS support even works with 3rd party
         VNC Viewers you specify via 'Change VNC Viewer' (e.g. RealVNC,
         TightVNC, UltraVNC etc.) that do not directly support it.

         Note: many VeNCrypt servers only support Anonymous Diffie Hellman
         TLS which has NO built in authentication and you will also need
         to set the option described in the next section.

         If you are using VeNCrypt or ANONTLS for REVERSE connections
         (Listen) then you *MUST* set this 'Server uses VeNCrypt SSL
         encryption' option.   Note also that REVERSE connections using
         VeNCrypt/ANONTLS currently do not work on Windows.

         Also, if you are using the "Use SSH+SSL" double tunnel to a
         VeNCrypt/ANONTLS server, you MUST set 'Server uses VeNCrypt
         SSL encryption' because "Verify All Certs" is disabled in
         SSH+SSL mode.

      Server uses Anonymous Diffie-Hellman

         Anonymous Diffie-Hellman can be used for SSL/TLS connections but
         there are no Certificates for authentication.  Therefore only
         passive eavesdropping attacks are prevented, not Man-In-The-Middle
         attacks.  Not recommended; try to use verified X509 certs instead.

         Enable this option if you know the server only supports Anon DH.
         When you do so, remember that ALL Certificate checking will be
         skipped (even if you have 'Verify All Certs' selected or set
         a ServerCert.)

         SSVNC may be able to autodetect Anon DH even if you haven't
         selected 'Server uses Anonymous Diffie-Hellman'. Once detected, it
         will prompt you whether it should continue.  Set the 'Server uses
         Anonymous Diffie-Hellman' option to avoid trying autodetection
         (i.e. forcing the issue.)

         Note that most Anonymous Diffie-Hellman VNC Servers do so
         via the VeNCrypt or ANONTLS VNC extensions (see the previous
         section.)  For these servers if you select 'Server uses Anonymous
         Diffie-Hellman' you *MUST* ALSO select 'Server uses VeNCrypt SSL
         encryption', otherwise SSVNC may have no chance to auto-detect
         the VeNCrypt/ANONTLS protocol.

         Also note, if you are using the "Use SSH+SSL" double tunnel to
         a VeNCrypt/ANONTLS server using Anon DH you MUST set 'Server
         uses Anonymous Diffie-Hellman' because "Verify All Certs"
         is disabled in SSH+SSL mode.

      Include:

       Default settings and Include Templates:

         Before explaining how Include works, first note that if you
         do not prefer some of SSVNC's default settings you can start
         up SSVNC and then change the settings for the options that you
         want to have a different default value.  Then type "defaults"
         in VNC Host:Display entry box and press "Save" to save them in
         the "defaults.vnc" profile.  After this, SSVNC will initialize
         all of the default values and then apply your override values
         in "defaults".

         For example, suppose you always want to use a different, 3rd
         party VNC Viewer.  Set Options -> Advanced -> Change VNC Viewer
         to what you want, and then save it as the "defaults" profile.
         Now that default setting will apply to all profiles, and SSVNC
         in its startup state.

         To edit the defaults Load it, make changes, and then Save it.
         Delete the "defaults" profile to go back to no modifications.
         Note that defaults created and saved while defaults.vnc existed
         will NOT be automatically adjusted.

       Include Templates:

         Now suppose you have a certain class of settings that you do
         not want to always be applied, but you want them to apply to a
         group of profiles.

         For example, suppose you have some settings for very low
         bandwidth connections (e.g. low color modes and/or aggressive
         compression and quality settings.)  Set these values in SSVNC
         and then in the VNC Host:Display entry box type in, say,
         "slowlink" and then press Save.  This will save those settings
         in the template profile named "slowlink.vnc".

         Now to create a real profile that uses this template type the
         host:disp in "VNC Host:Display" and in Options -> Advanced
         -> Includes type in "slowlink".  Then press Save to save the
         host profile.  Then re-Load it.  The "slowlink" settings will
         be applied after the defaults.  Make any other changes to the
         setting for this profile and Save it again.  Next time you load
         it in, the Include template settings will override the defaults
         and then the profile itself is read in.

         You may supply a comma or space separated list of templates
         to include.  They are applied in the order listed.  They can be
         full path names or basenames relative to the profiles directory.
         You do not need to supply the .vnc suffix.  The non-default
         settings in them will be applied first, and then any values in
         the loaded Profile will override them.

      Sleep:

         Enter a number to indicate how many extra seconds to sleep
         while waiting for the VNC viewer to start up.  On Windows this
         can give extra time to enter the Putty/Plink password, etc.

      Putty Args:

         Windows only, supply a string to be added to all plink.exe
         and putty.exe commands.  Example: -i C:\mykey.ppk

      Launch Putty Pagent:

         Windows only, launch the Putty key agent tool (pageant) to hold
         your SSH private keys for automatic logging in by putty/plink.

      Launch Putty Key-Gen:

         Windows only, launch the Putty key generation tool (puttygen)
         to create new SSH private keys.

      Unix ssvncviewer:

         Display a popup menu with options that apply to the special 
         Unix SSVNC VNC Viewer (perhaps called 'ssvncviewer') provided by
         this SSVNC package.  This only applies to Unix or Mac OS X.
     
      Use ssh-agent:

         On Unix only: restart the GUI in the presence of ssh-agent(1)
         (e.g. in case you forgot to start your agent before starting
         this GUI).  An xterm will be used to enter passphrases, etc.
         This can avoid repeatedly entering passphrases for the SSH logins
         (note this requires setting up and distributing SSH keys).


    About the CheckButtons:

         Ahem, Well...., yes quite a klunky UI: you have to toggle the
         CheckButton to pull up the Dialog box a 2nd, etc. time... don't
         worry your settings will still be there!
}

	.ah.f.t insert end $msg
	jiggle_text .ah.f.t
}

proc help_ssvncviewer_opts {} {
	toplev .av

	scroll_text_dismiss .av.f

	center_win .av

	wm title .av "Unix SSVNC viewer Options Help"

	set msg {
    These Unix SSVNC VNC Viewer Options apply only on Unix or Mac OS X
    when using the viewer (ssvncviewer) supplied by this SSVNC package.

    Brief descriptions:

      Multiple LISTEN Connections:

         Allow multiple VNC servers to reverse connect at the same time
         and so display each of their desktops on your screen at the
         same time.

      Listen Once:

         Try to have the VNC Viewer exit after the first listening
         connection. (It may not always be detected; use Ctrl-C to exit)

      Listen Accept Popup Dialog:

         In -listen (reverse connection listening) mode when a reverse
         VNC connection comes in show a popup asking whether to Accept
         or Reject the connection. (-acceptpopup vncviewer option.)

      Accept Popup UltraVNC Single Click:

         As in 'Listen Accept Popup Dialog', except assume the remote
         VNC server is UltraVNC Single Click and force the execution of
         the protocol to retrieve the extra remote-side info (Windows
         User, ComputerName, etc) which is then also displayed in the
         Popup window. (-acceptpopupsc vncviewer option.)

      Use X11 Cursor:

         When drawing the mouse cursor shape locally, use an X11 cursor
         instead of drawing it directly into the framebuffer.  This
         can sometimes give better response, and avoid problems under
         'Scaling'. 

      Disable Bell:

         Disable beeps coming from remote side.

      Use Raw Local:

         Use the VNC Raw encoding for 'localhost' connections (instead
         of assuming there is a local tunnel, SSL or SSH, going to the
         remote machine.

      Avoid Using Terminal:

         By default the Unix ssvncviewer will prompt for usernames,
         passwords, etc. in the terminal it is running inside of.
         Set this option to use windows for messages and prompting as
         much as possible.  Messages will also go to the terminal, but
         all prompts will be done via popup window.

         Note that stunnel(1) may prompt for a passphrase to unlock a
         private SSL key.  This is fairly rare because it is usually
         for Client-side SSL authentication.  stunnel will prompt from
         the terminal; there seems to be no way around this.

         Also, note that ssh(1) may prompt for an ssh key passphrase
         or Unix password.  This can be avoided in a number of ways,
         the simplest one is to use ssh-agent(1) and ssh-add(1).
         However ssh(1) may also prompt you to accept a new public key
         for a host or warn you if the key has changed, etc. 

      Use Popup Fix:

         Enable a fix that warps the popup (F8) to the mouse pointer.

      Use XGrabServer (for fullscreen):

         On Unix only, use the XGrabServer workaround for older window
         managers.  Sometimes also needed on recent (2008) GNOME.  This
         workaround can make going into/out-of Fullscreen work better.

      Cursor Alphablending:

         Use the x11vnc alpha hack for translucent cursors (requires Unix,
         32bpp and same endianness)

      TurboVNC:

         If available on your platform, use a ssvncviewer compiled with
         TurboVNC support.  This is based on the VirtualGL project:
         http://www.sourceforge.net/projects/virtualgl	You will need
         to install the VirtualGL's TurboJPEG library too.

         Currently (May/2009) only Linux.i686, Linux.x86_64, and
         Darwin.i386 have vncviewer.turbovnc binaries shipped in the
         ssvnc bundles.  See the build instructions for how you might
         compile your own.

      Disable Pipelined Updates:

         Disable the TurboVNC-like pipelined updates mode.  Pipelined
         updates is the default even when not TurboVNC enabled.  They
         ask for the next screen update before the current one has 
         finished downloading, and so this might reduce the slowdown
         due to high latency or low bandwidth by 2X or so.  Disable
         them if they cause problems with the remote VNC Server or
         use too much bandwidth.

      Send CLIPBOARD not PRIMARY:

         When sending locally selected text to the VNC server side,
         send the CLIPBOARD selection instead of the PRIMARY selection.

      Send Selection Every time:

         Send selected text to the VNC server side every time the mouse
         focus enters the main VNC Viewer window instead only when it
         appears to have changed since the last send.

      Scaling:

         Use viewer-side (i.e. local) scaling of the VNC screen.  Supply
         a fraction, e.g. 0.75 or 3/4, or a WxH geometry, e.g. 1280x1024,
         or the string 'fit' to fill the current screen.  Use 'auto'
         to scale the desktop to match the viewer window size.

         If you observe mouse trail painting errors try using X11 Cursor.

         Note that since the local scaling is done in software it can
         be slow.  Since ZRLE is better than Tight in this regard, when
         scaling is detected, the encoding will be switched to ZRLE.
         Use the Popup to go back to Tight if you want to, or set the
         env. var. SSVNC_PRESERVE_ENCODING=1 to disable the switch.

         For additional speedups under local scaling: try having a solid
         desktop background on the remote side (either manually or using
         'x11vnc -solid ...'); and also consider using client side caching
         'x11vnc -ncache 10 ...' if the remote server is x11vnc.

      Escape Keys:

         Enable 'Escape Keys', a set of modifier keys that, if all are
         pressed down, enable local Hot Key actions.  Set to 'default'
         to use the default (Alt_L,Super_L on unix, Control_L,Meta_L
         on macosx) or set to a list of modifier keys.

      Y Crop:

         This is for x11vnc's -ncache client side caching scheme with our
         Unix TightVNC viewer.  Sets the Y value to "crop" the viewer
         size at (below the cut is the pixel cache region you do not
         want to see).  If the screen is tall (H > 2*W) ycropping will
         be autodetected, or you can set to -1 to force autodection.
         Otherwise, set it to the desired Y value.  You can also set
         the scrollbar width (very thin by default) by appending ",sb=N"
         (or use ",sb=N" by itself to just set the scrollbar width).

      ScrollBar Width:

         This is for x11vnc's -ncache client side caching scheme with our
         Unix TightVNC viewer.  For Y-Crop mode, set the size of the
         scrollbars (often one want it to be very narrow, e.g. 2 pixels
         to be less distracting.

      RFB Version:

         Set the numerical version of RFB (VNC) protocol to pretend to
         be, 3.x.  Usually only needed with UltraVNC servers.

      Encodings:

         List encodings in preferred order, for example
         'copyrect zrle tight'   The list of encodings is:
         copyrect tight zrle zywrle hextile zlib corre rre raw

      Extra Options:

         String of extra Unix ssvncviewer command line options.  I.e. for
         ones like -16bpp that cannot be set inside this SSVNC GUI.  For a
         list click Help then 'SSVNC vncviewer -help Output'. 


    These are environment variables one may set to affect the options
    of the SSVNC vncviewer and also the ss_vncviewer wrapper script
    (and hence may apply to 3rd party vncviewers too)

         VNCVIEWER_ALPHABLEND     (-alpha, see Cursor Alphablending above)
         VNCVIEWER_POPUP_FIX      (-popupfix, warp popup to mouse location)
         VNCVIEWER_GRAB_SERVER    (-graball, see Use XGrabServer above)
         VNCVIEWER_YCROP          (-ycrop, see Y Crop above)
         VNCVIEWER_SBWIDTH        (-sbwidth, see ScrollBar Width above)
         VNCVIEWER_RFBVERSION     (-rfbversion, e.g. 3.6)
         VNCVIEWER_ENCODINGS      (-encodings, e.g. "copyrect zrle hextile")
         VNCVIEWER_NOBELL         (-nobell)
         VNCVIEWER_X11CURSOR      (-x11cursor, see Use X11 Cursor above)
         VNCVIEWER_RAWLOCAL       (-rawlocal, see Use Raw Local above)
         VNCVIEWER_NOTTY          (-notty, see Avoid Using Terminal above)
         VNCVIEWER_ESCAPE         (-escape, see Escape Keys above)
         VNCVIEWER_ULTRADSM       (-ultradsm)
         VNCVIEWER_PIPELINE_UPDATES (-pipeline, see above)
         VNCVIEWER_SEND_CLIPBOARD (-sendclipboard)
         VNCVIEWER_SEND_ALWAYS    (-sendalways)
         VNCVIEWER_RECV_TEXT      (-recvtext clipboard/primary/both)
         VNCVIEWER_NO_CUTBUFFER   (do not send CUTBUFFER0 as fallback)
         VNCVIEWER_NO_PIPELINE_UPDATES (-nopipeline)
         VNCVIEWER_ALWAYS_RECENTER (set to avoid(?) recentering on resize)
         VNCVIEWER_IS_REALVNC4    (indicate vncviewer is realvnc4 flavor.)
         VNCVIEWER_NO_IPV4        (-noipv4)
         VNCVIEWER_NO_IPV6        (-noipv6)
         VNCVIEWER_FORCE_UP       (force raise on fullscreen graball)
         VNCVIEWER_PASSWORD       (danger: set vnc passwd via env. var.)
         VNCVIEWER_MIN_TITLE      (minimum window title (appshare))

         VNCVIEWERCMD             (unix viewer command, default vncviewer)
         VNCVIEWERCMD_OVERRIDE    (force override of VNCVIEWERCMD)
         VNCVIEWERCMD_EXTRA_OPTS  (extra options to pass to VNCVIEWERCMD)
         VNCVIEWER_LISTEN_LOCALHOST (force ssvncviewer to -listen on localhost)
         VNCVIEWER_NO_SEC_TYPE_TIGHT(force ssvncviewer to skip rfbSecTypeTight)
         HEXTILE_YCROP_TOO        (testing: nosync_ycrop for hextile updates.)

         SS_DEBUG                 (very verbose debug printout by script.)
         SS_VNCVIEWER_LISTEN_PORT (force listen port.)
         SS_VNCVIEWER_NO_F        (no -f for SSH.)
         SS_VNCVIEWER_NO_T        (no -t for SSH.)
         SS_VNCVIEWER_USE_C       (force -C compression for SSH.)
         SS_VNCVIEWER_SSH_CMD     (override SSH command to run.)
         SS_VNCVIEWER_NO_MAXCONN  (no maxconn for stunnel (obsolete))
         SS_VNCVIEWER_RM          (file containing vnc passwd to remove.)
         SS_VNCVIEWER_SSH_ONLY    (run the SSH command, then exit.)

         SSVNC_MULTIPLE_LISTEN    (-multilisten, see Multiple LISTEN above)
         SSVNC_ACCEPT_POPUP       (-acceptpopup, see Accept Popup Dialog)
         SSVNC_ACCEPT_POPUP_SC    (-acceptpopupsc, see Accept Popup Dialog)
         SSVNC_TURBOVNC           (see TurboVNC above)
         SSVNC_UNIXPW             (-unixpw)
         SSVNC_UNIXPW_NOESC       (do not send escape in -unixpw mode)
         SSVNC_SCALE              (-scale, see Scaling above)
         SSVNC_NOSOLID            (do not do solid region speedup in
                                   scaling mode.)
         SSVNC_PRESERVE_ENCODING  (do not switch to ZRLE when scaling)
         SSVNC_FINISH_SLEEP       (on unix/macosx sleep this many seconds
                                   before exiting the terminal, default 5)

         Misc (special usage or debugging or ss_vncviewer settings):

         SSVNC_MESG_DELAY         (sleep this many millisec between messages)
         SSVNC_NO_ENC_WARN        (do not print out a NO ENCRYPTION warning)
         SSVNC_EXTRA_SLEEP        (same as Sleep: window)
         SSVNC_NO_ULTRA_DSM       (disable ultravnc dsm encryption)
         SSVNC_ULTRA_DSM          (the ultravnc_dsm_helper command)
         SSVNC_ULTRA_FTP_JAR      (file location of ultraftp.jar jar file)
         SSVNC_KNOWN_HOSTS_FILE   (file for per-connection ssh known hosts)
         SSVNC_SCALE_STATS        (print scaling stats)
         SSVNC_NOSOLID            (disable solid special case while scaling)
         SSVNC_DEBUG_RELEASE      (debug printout for keyboard modifiers.)
         SSVNC_DEBUG_ESCAPE_KEYS  (debug printout for escape keys)
         SSVNC_NO_MAYBE_SYNC      (skip XSync() calls in certain painting)
         SSVNC_MAX_LISTEN         (number of time to listen for reverse conn.)
         SSVNC_LISTEN_ONCE        (listen for reverse conn. only once)
         STUNNEL_LISTEN           (stunnel interface for reverse conn.
         SSVNC_NO_MESSAGE_POPUP   (do not place info messages in popup.)
         SSVNC_SET_SECURITY_TYPE  (force VeNCrypt security type)
         SSVNC_PREDIGESTED_HANDSHAKE (string used for VeNCrypt, etc. connect)
         SSVNC_SKIP_RFB_PROTOCOL_VERSION (force viewer to be RFB 3.8)
         SSVNC_DEBUG_SEC_TYPES    (debug security types for VeNCrypt)
         SSVNC_DEBUG_MSLOGON      (extra printout for ultravnc mslogon proto)
         SSVNC_DEBUG_RECTS        (printout debug for RFB rectangles.)
         SSVNC_DEBUG_CHAT         (printout debug info for chat mode.)
         SSVNC_DELAY_SYNC         (faster local drawing delaying XSync)
         SSVNC_DEBUG_SELECTION    (printout debug for selection/clipboard)
         SSVNC_REPEATER           (URL-ish sslrepeater:// thing for UltraVNC)
         SSVNC_VENCRYPT_DEBUG     (debug printout for VeNCrypt mode.)
         SSVNC_VENCRYPT_USERPASS  (force VeNCrypt user:pass)
         SSVNC_STUNNEL_DEBUG      (increase stunnel debugging printout)
         SSVNC_STUNNEL_VERIFY3    (increase stunnel verify from 2 to 3)
         SSVNC_LIM_ACCEPT_PRELOAD (preload library to limit accept(2))
         SSVNC_SOCKS5             (socks5 for x11vnc PORT= mode, default)
         SSVNC_SOCKS4		  (socks4 for x11vnc PORT= mode)
         SSVNC_NO_IPV6_PROXY      (do not setup a ipv6:// proxy)
         SSVNC_NO_IPV6_PROXY_DIRECT (do not setup a ipv6:// proxy unencrypted)
         SSVNC_PORT_IPV6          (x11vnc PORT= mode is to ipv6-only)
         SSVNC_IPV6               (0 to disable ss_vncviewer ipv6 check)
         SSVNC_FETCH_TIMEOUT      (ss_vncviewer cert fetch timeout)
         SSVNC_USE_S_CLIENT       (force cert fetch to be 'openssl s_client')
         SSVNC_SHOWCERT_EXIT_0    (force showcert to exit with success)
         SSVNC_SSH_LOCALHOST_AUTH (force SSH localhost auth check.)
         SSVNC_TEST_SEC_TYPE      (force PPROXY VeNCrypt type; testing)
         SSVNC_TEST_SEC_SUBTYPE   (force PPROXY VeNCrypt subtype; testing)
         SSVNC_EXIT_DEBUG         (testing: prompt to exit at end.)
         SSVNC_UP_DEBUG           (gui user/passwd debug mode.)
         SSVNC_UP_FILE            (gui user/passwd file.)

         STUNNEL_EXTRA_OPTS       (extra options for stunnel.)

         X11VNC_APPSHARE_DEBUG    (for debugging -appshare mode.)
         NO_X11VNC_APPSHARE       (shift down for escape keys.)
         DEBUG_HandleFileXfer     (ultravnc filexfer)
         DEBUG_RFB_SMSG           (RFB server message debug.)
}

	.av.f.t insert end $msg
	button .av.htext -text "SSVNC vncviewer -help Output" -command show_viewer_help
	pack .av.htext -side bottom -fill x
	jiggle_text .av.f.t
}

proc show_viewer_help {} {
	toplev .vhlp

	set h 35
	if [small_height] {
		set h 30
	}
	scroll_text_dismiss .vhlp.f 83 $h

	center_win .vhlp
	wm resizable .vhlp 1 0

	wm title .vhlp "SSVNC vncviewer -help Output"

	set msg "-- No Help Output --"
	catch {set msg [exec ss_vncviewer -viewerhelp 2>/dev/null]}

	.vhlp.f.t insert end $msg
	jiggle_text .vhlp.f.t
}

proc set_viewer_path {} {
	global change_vncviewer_path
	unix_dialog_resize .chviewer
	set change_vncviewer_path [tk_getOpenFile -parent .chviewer]
	catch {raise .chviewer}
	update
}

proc change_vncviewer_dialog {} {
	global change_vncviewer change_vncviewer_path vncviewer_realvnc4
	global ts_only
	
	toplev .chviewer
	wm title .chviewer "Change VNC Viewer"

	global help_font
	if {$ts_only} {
		eval text .chviewer.t -width 90 -height 16 $help_font
	} else {
		eval text .chviewer.t -width 90 -height 27 $help_font
	}
	apply_bg .chviewer.t

	set msg {
    To use your own VNC Viewer (i.e. one installed by you, not included in this
    package), e.g. UltraVNC or RealVNC, type in the program name, or browse for
    the full path to it.  You can put command line arguments after the program.

    Note that due to incompatibilities with respect to command line options
    there may be issues, especially if many command line options are supplied.
    You can specify your own command line options below if you like (and try to
    avoid setting any others in this GUI under "Options").

    If the path to the program name has spaces it in, surround it with double quotes:

        "C:\Program Files\My Vnc Viewer\VNCVIEWER.EXE"

    Make sure the very first character is a quote.  You should quote the command
    even if it is only the command line arguments that need extra protection:

        "wine" -- "/home/fred/Program Flies/UltraVNC-1.0.2.exe" /64colors

    Since the command line options differ between them greatly, if you know it
    is of the RealVNC 4.x flavor, indicate on the check box. Otherwise we guess.

    To have SSVNC act as a general STUNNEL redirector (no VNC) set the viewer to be
    "xmessage OK" or "xmessage <port>" or "sleep n" or "sleep n <port>" (or "NOTEPAD"
    on Windows).  The default listen port is 5930.  The destination is set in "VNC
    Host:Display" (for a remote port less than 200 use the negative of the port value).
}

	if {$ts_only} {
		regsub {Note that due(.|\n)*If the} $msg "If the" msg
		regsub {To have SSVNC act(.|\n)*} $msg "" msg
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
	wm protocol .chviewer WM_DELETE_WINDOW {destroy .chviewer; set change_vncviewer 0}
	button .chviewer.done -text "Done" -command {destroy .chviewer; catch {raise .oa}}
	bind .chviewer.path.e <Return> {destroy .chviewer; catch {raise .oa}}

	pack .chviewer.t .chviewer.path .chviewer.cancel .chviewer.done -side top -fill x

	center_win .chviewer
	wm resizable .chviewer 1 0

	focus .chviewer.path.e 
}

proc port_redir_dialog {} {
	global additional_port_redirs additional_port_redirs_list
	
	toplev .redirs
	wm title .redirs "Additional Port Redirections (via SSH)"

	global help_font uname
	set h 35
	if [small_height] {
		set h 27
	}
	eval text .redirs.t -width 80 -height $h $help_font
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
	wm protocol .redirs WM_DELETE_WINDOW {set additional_port_redirs 0; destroy .redirs}
	button .redirs.done -text "Done" -command {destroy .redirs}

	pack .redirs.t .redirs.path .redirs.cancel .redirs.done -side top -fill x

	center_win .redirs
	wm resizable .redirs 1 0

	focus .redirs.path.e
}

proc stunnel_sec_dialog {} {
	global stunnel_local_protection
	
	toplev .stlsec
	wm title .stlsec "STUNNEL Local Port Protections"

	global help_font uname
	
	set h 37
	if [small_height] {
		set h 26
	}
	scroll_text .stlsec.f 82 $h

	apply_bg .stlsec.f

	set msg {
    See the discussion of "Untrusted Local Users" in the main 'Help'
    panel for info about users who are able to log into the workstation
    you run SSVNC on and might try to use your encrypted tunnel to gain
    access to the remote VNC machine.

    On Unix, for STUNNEL SSL tunnels we provide two options as extra
    safeguards against untrusted local users.  Both only apply to Unix/MacOSX.
    Note that Both options are *IGNORED* in reverse connection (Listen) mode.

    1) The first one 'Use stunnel EXEC mode' (it is mutually exclusive with
       option 2).  For this case the modified SSVNC Unix viewer must be
       used: it execs the stunnel program instead of connecting to it via
       TCP/IP.  Thus there is no localhost listening port involved at all.

       This is the best solution for SSL stunnel tunnels, it works well and
       is currently enabled by default.  Disable it if there are problems.

    2) The second one 'Use stunnel IDENT check', uses the stunnel(8)
       'ident = username' to use the local identd daemon (IDENT RFC 1413
       http://www.ietf.org/rfc/rfc1413.txt) to check that the locally
       connecting program (the SSVNC vncviewer) is being run by your userid.
       See the stunnel(8) man page for details.

       Normally the IDENT check service cannot be trusted much when used
       *remotely* (the remote host may be have installed a modified daemon).
       However when using the IDENT check service *locally* it should be
       reliable.  If not, it means the local machine (where you run SSVNC)
       has already been root compromised and you have a serious problem.

       Enabling 'Use stunnel IDENT check' requires a working identd on the
       local machine.  Often it is not installed or enabled (because it is not
       deemed to be useful, etc).  identd is usually run out of the inetd(8)
       super-server.  Even when installed and running it is often configured
       incorrectly.  On a Debian/lenny system we actually found that the
       kernel module 'tcp_diag' needed to be loaded! ('modprobe tcp_diag')
}
	.stlsec.f.t insert end $msg

	radiobutton .stlsec.ident -relief ridge -anchor w -variable stunnel_local_protection_type -value "ident" -text "Use stunnel IDENT check"
	radiobutton .stlsec.exec  -relief ridge -anchor w -variable stunnel_local_protection_type -value "exec"  -text "Use stunnel EXEC mode"

	button .stlsec.cancel -text "Cancel" -command {set stunnel_local_protection 0; destroy .stlsec}
	bind .stlsec <Escape> {set stunnel_local_protection 0; destroy .stlsec}
	wm protocol .stlsec WM_DELETE_WINDOW {set stunnel_local_protection 0; destroy .stlsec}
	button .stlsec.done -text "Done" -command {if {$stunnel_local_protection_type == "none"} {set stunnel_local_protection 0}; destroy .stlsec}

	pack .stlsec.f .stlsec.exec .stlsec.ident .stlsec.cancel .stlsec.done -side top -fill x

	center_win .stlsec
	wm resizable .stlsec 1 0
}

proc disable_ssl_workarounds_dialog {} {
	global disable_ssl_workarounds disable_ssl_workarounds_type
	
	toplev .sslwrk
	wm title .sslwrk "Disable SSL Workarounds"

	global help_font uname
	set h 36
	if [small_height] {
		set h 24
	}
	scroll_text .sslwrk.f 86 $h

	apply_bg .sslwrk.f

	set msg {
    Some SSL implementations are incomplete or buggy or do not work properly
    with other implementations.  SSVNC uses STUNNEL for its SSL encryption,
    and STUNNEL uses the OpenSSL SSL implementation.

    This causes some problems with non-OpenSSL implementations on the VNC server
    side.  The most noticable one is the UltraVNC Single Click III (SSL) server:

       http://www.uvnc.com/pchelpware/SCIII/index.html

    It can make a reverse connection to SSVNC via an encrypted SSL tunnel.

    Unfortunately, in the default operation with STUNNEL the connection will be
    dropped after 2-15 minutes due to an unexpected packet.

    Because of this, by default SSVNC will enable some SSL workarounds to make
    connections like these work.  This is the STUNNEL 'options = ALL' setting:
    it enables a basic set of SSL workarounds.

    You can read all about these workarounds in the stunnel(8) manpage and the
    OpenSSL SSL_CTX_set_options(3) manpage.

    Why are we mentioning this?  STUNNELS's 'options = ALL' lowers the SSL
    security a little bit.  If you know you do not have an incompatible SSL
    implementation on the server side (e.g. any one using OpenSSL is compatible,
    x11vnc in particular), then you can regain that little bit of security by
    selecting the "Disable SSL Workarounds" option.

    "Disable All SSL Workarounds" selected below will do that.  On the other hand,
    choose "Keep the DONT_INSERT_EMPTY_FRAGMENTS Workaround" to retain that one,
    commonly needed workaround.

    BTW, you can set the environment variable STUNNEL_EXTRA_OPTS_USER to add
    any lines to the STUNNEL global config that you want to.  See the stunnel(8)
    man page for more details.
}
	.sslwrk.f.t insert end $msg

	radiobutton .sslwrk.none    -relief ridge -anchor w -variable disable_ssl_workarounds_type -value "none" -text "Disable All Workarounds"
	radiobutton .sslwrk.noempty  -relief ridge -anchor w -variable disable_ssl_workarounds_type -value "noempty"  -text "Keep the DONT_INSERT_EMPTY_FRAGMENTS Workaround"

	button .sslwrk.cancel -text "Cancel" -command {set disable_ssl_workarounds 0; destroy .sslwrk}
	bind .sslwrk <Escape> {set disable_ssl_workarounds 0; destroy .sslwrk}
	wm protocol .sslwrk WM_DELETE_WINDOW {set disable_ssl_workarounds 0; destroy .sslwrk}
	button .sslwrk.done -text "Done" -command {destroy .sslwrk}

	pack .sslwrk.f .sslwrk.none .sslwrk.noempty .sslwrk.cancel .sslwrk.done -side top -fill x

	center_win .sslwrk
	wm resizable .sslwrk 1 0
}

proc update_no_ultra_dsm {} {
	global ultra_dsm_noultra
	global ultra_dsm_type

	foreach b {bf des3 aes aes256 l e} {
		if {! $ultra_dsm_noultra} {
			.ultradsm.nou.$b configure -state disabled
		} else {
			.ultradsm.nou.$b configure -state normal
		}
	}
	if {! $ultra_dsm_noultra} {
		if {$ultra_dsm_type == "arc4"} {
			;
		} elseif {$ultra_dsm_type == "aesv2"} {
			;
		} elseif {$ultra_dsm_type == "msrc4"} {
			;
		} elseif {$ultra_dsm_type == "msrc4_sc"} {
			;
		} elseif {$ultra_dsm_type == "securevnc"} {
			;
		} else {
			set ultra_dsm_type guess
		}
		catch {.ultradsm.key.securevnc configure -state normal}
		catch {.ultradsm.key.msrc4_sc  configure -state normal}
	} else {
		catch {.ultradsm.key.securevnc configure -state disabled}
		catch {.ultradsm.key.msrc4_sc  configure -state disabled}
	}
}

proc ultra_dsm_dialog {} {
	global ultra_dsm ultra_dsm_file ultra_dsm_type
	
	toplev .ultradsm
	wm title .ultradsm "UltraVNC DSM Encryption Plugin"

	global help_font
	set h 40
	if [small_height] {
		set h 22
	}
	scroll_text .ultradsm.f 85 $h

	set msg {
    On Unix and MacOSX with the provided SSVNC vncviewer, you can connect to an
    UltraVNC server that is using one of its DSM encryption plugins: MSRC4, ARC4,
    AESV2, and SecureVNC. More info at: http://www.uvnc.com/features/encryption.html

    IMPORTANT: The UltraVNC DSM MSRC4, ARC4, and AESV2 implementations contain
    unfixed errors that could allow an eavesdropper to recover the session
    key or traffic easily.  They often do not provide strong encryption, but
    only provide basic obscurity instead.  Do not use them with critical data.
    The newer SecureVNC Plugin does not suffer from these problems.

    See the bottom of this help text for how to use symmetric encryption with
    Non-UltraVNC servers (for example, x11vnc 0.9.5 or later).  This mode does not
    suffer the shortcomings of the UltraVNC MSRC4, ARC4, and AESV2 implementations.

    You will need to specify the corresponding UltraVNC encryption key (created
    by you using an UltraVNC server or viewer).  It is usually called 'rc4.key'
    (for MSRC4), 'arc4.key' (for ARC4), and 'aesv2.key' (for AESV2).  Specify the
    path to it or Browse for it.  Also, specify which type of plugin it is (or use
    'guess' to have it guess via the before mentioned filenames).

    The choice "UVNC SC" enables a special workaround for use with UltraVNC Single
    Click and the MSRC4 plugin.  It may not be needed on recent SC (e.g. from
    ~2009 and later; select "MSRC4" for these newer ones.)

    You can also specify pw=my-password instead of a keyfile.  Use single quotes
    pw='....' if the password contains shell meta-characters `!$&*(){}[]|;<>?

    Use the literal string 'pw=VNCPASSWD' to have the VNC password that you
    entered into the 'VNC Password:' be used for the pw=...

    SSL and SSH tunnels do not apply in this mode (any settings are ignored.)

    Proxying works in this mode, as well as Reverse Connections (Listen)

    The choice "SecureVNC" refers to the SecureVNC Plugin using 128 bit AES or
    ARC4 with 2048 bit RSA key exchange described here:

          http://adamwalling.com/SecureVNC

    Note in its default mode SecureVNC is *Vulnerable* to Man-In-The-Middle attacks
    (encryption but no server authentication) so do not use it with critical data.
    In SecureVNC mode you do not need to supply a 'Ultra DSM Keyfile'.  However,
    if you DO supply a keyfile filename (recommended) if that file does not exist
    you will be prompted if you want to save the UltraVNC server's RSA key in it.
    The key's MD5 checksum is displayed so that you can verify that the key is
    trusted.  One way to print out the SecureVNC public key MD5 checksum is:

    openssl rsa -inform DER -outform DER -pubout -in ./Server_SecureVNC.pkey | dd bs=1 skip=24 | md5sum

    Then on subsequent connections, if you continue to specify this filename, the
    SecureVNCPlugin server's RSA key will be checked against the file's contents
    and if they differ the connection will be dropped.

    NOTE, However, if the SecureVNC keyfile ends in the string 'ClientAuth.pkey'
    then its contents are used for SecureVNC's normal Client Authentication dialog
    (you need to use Windows SecureVNCPlugin to generate this file on the server
    side, it is usually called "Viewer_ClientAuth.pkey", and then safely copy it
    to the viewer side.)  If you want to do BOTH Client Auth and server RSA key
    storing (recommended), have the keyfile end in 'ClientAuth.pkey.rsa'; that way
    the file will be used for storing the server RSA key and then the '.rsa' is
    trimmed off and the remainder used for the SecureVNC Client Auth data filename.

    Note that despite its intentions, Client Authentication in the FIRST release of
    SecureVNC is still susceptible to Man-In-The-Middle attacks.  Even when that
    is fixed, SecureVNC Client Authentication is still susceptible to "spoofing"
    attacks where the viewer user may be tricked into revealing his VNC or MS-Logon
    password if his connection is intercepted.  It is recommended you verify and
    save the Server key (see above) in addition to using Client Authentication.

    UltraVNC DSM encryption modes are currently experimental because unfortunately
    the UltraVNC DSM plugin also modifies the RFB protocol(!), and so the SSVNC
    vncviewer had to be modified to support it.  The tight, zlib, and some minor
    encodings currently do not work in this mode and are disabled.

    Note that this mode also requires the utility tool named 'ultravnc_dsm_helper'
    that should be included in your SSVNC kit.

    Select 'Non-Ultra DSM' to use symmetric encryption to a Non-UltraVNC server via
    a supported symmetric key cipher.  x11vnc supports symmetric encryption via,
    e.g., "x11vnc -enc aesv2:./my.key".  Extra ciphers are enabled for this mode
    (e.g. blowfish and 3des).  'UVNC SC' and SecureVNC do not apply in this mode.

    Note for the Non-Ultra DSM case it will also work with any VNC Viewer
    (i.e. selected by Options -> Advanced -> Change VNC Viewer) not only the
    supplied SSVNC vncviewer.

    For experts: You can also set the random salt size and initialization vector
    size in Salt,IV for example "8,16".  See the x11vnc and 'ultravnc_dsm_helper
    -help' documentation for more info on this.
}

	.ultradsm.f.t insert end $msg

	frame .ultradsm.path
	label .ultradsm.path.l -text "Ultra DSM Keyfile:"
	entry .ultradsm.path.e -width 40 -textvariable ultra_dsm_file
	button .ultradsm.path.b -text "Browse..." -command {set_ultra_dsm_file .ultradsm}

	pack .ultradsm.path.l -side left
	pack .ultradsm.path.e -side left -expand 1 -fill x
	pack .ultradsm.path.b -side left

	frame .ultradsm.key
	label .ultradsm.key.l -text "Type of Key:        "
	radiobutton .ultradsm.key.guess -pady 1 -anchor w -variable ultra_dsm_type -value guess \
		-text "Guess"
	radiobutton .ultradsm.key.arc4 -pady 1 -anchor w -variable ultra_dsm_type -value arc4 \
		-text "ARC4"

	radiobutton .ultradsm.key.aesv2 -pady 1 -anchor w -variable ultra_dsm_type -value aesv2 \
		-text "AESV2"

	radiobutton .ultradsm.key.msrc4 -pady 1 -anchor w -variable ultra_dsm_type -value msrc4 \
		-text "MSRC4"

	radiobutton .ultradsm.key.msrc4_sc -pady 1 -anchor w -variable ultra_dsm_type -value msrc4_sc \
		-text "UVNC SC"

	radiobutton .ultradsm.key.securevnc -pady 1 -anchor w -variable ultra_dsm_type -value securevnc \
		-text "SecureVNC"

	pack .ultradsm.key.l -side left
	pack .ultradsm.key.guess -side left
	pack .ultradsm.key.arc4 -side left
	pack .ultradsm.key.aesv2 -side left
	pack .ultradsm.key.msrc4 -side left
	pack .ultradsm.key.msrc4_sc -side left
	pack .ultradsm.key.securevnc -side left

	frame .ultradsm.nou
	checkbutton .ultradsm.nou.cb -text "Non-Ultra DSM" -variable ultra_dsm_noultra -command update_no_ultra_dsm
	radiobutton .ultradsm.nou.bf -pady 1 -anchor w -variable ultra_dsm_type -value blowfish \
		-text "Blowfish"

	radiobutton .ultradsm.nou.des3 -pady 1 -anchor w -variable ultra_dsm_type -value 3des \
		-text "3DES"

	radiobutton .ultradsm.nou.aes -pady 1 -anchor w -variable ultra_dsm_type -value "aes-cfb" \
		-text "AES-CFB"

	radiobutton .ultradsm.nou.aes256 -pady 1 -anchor w -variable ultra_dsm_type -value "aes256" \
		-text "AES-256"

	label .ultradsm.nou.l -text " Salt,IV"
	entry .ultradsm.nou.e -width 6 -textvariable ultra_dsm_salt

	pack .ultradsm.nou.cb -side left
	pack .ultradsm.nou.bf -side left
	pack .ultradsm.nou.des3 -side left
	pack .ultradsm.nou.aes -side left
	pack .ultradsm.nou.aes256 -side left
	pack .ultradsm.nou.l -side left
	pack .ultradsm.nou.e -side left -expand 0

	update_no_ultra_dsm

	button .ultradsm.cancel -text "Cancel" -command {destroy .ultradsm; set ultra_dsm 0}
	bind .ultradsm <Escape> {destroy .ultradsm; set ultra_dsm 0}
	wm protocol .ultradsm WM_DELETE_WINDOW {destroy .ultradsm; set ultra_dsm 0}
	button .ultradsm.done -text "Done" -command {destroy .ultradsm; catch {raise .oa}}
	bind .ultradsm.path.e <Return> {destroy .ultradsm; catch {raise .oa}}

	pack .ultradsm.f .ultradsm.path .ultradsm.key .ultradsm.nou .ultradsm.cancel .ultradsm.done -side top -fill x

	center_win .ultradsm
	wm resizable .ultradsm 1 0

	focus .ultradsm.path.e 
}

proc ssh_known_hosts_dialog {} {
	global ssh_known_hosts ssh_known_hosts_filename
	
	toplev .sshknownhosts
	wm title .sshknownhosts "Private SSH KnownHosts file"

	global help_font
	set h 31
	if [small_height] {
		set h 23
	}
	scroll_text .sshknownhosts.f 80 $h

	set msg {
      Private SSH KnownHosts file:

         On Unix in SSH mode, let the user specify a non-default
         ssh known_hosts file to be used only by the current profile.
         This is the UserKnownHostsFile ssh option and is described in the
         ssh_config(1) man page.  This is useful to avoid proxy 'localhost'
         SSH key collisions.

         Normally one should simply let ssh use its default file
         ~/.ssh/known_hosts for tracking SSH keys.  The only problem with
         that happens when multiple SSVNC connections use localhost tunnel
         port redirections.  These make ssh connect to 'localhost' on some
         port (where the proxy is listening.)  Then the different keys
         from the multiple ssh servers collide when ssh saves them under
         'localhost' in ~/.ssh/known_hosts.

         So if you are using a proxy with SSVNC or doing a "double SSH
         gateway" your ssh will connect to a proxy port on localhost, and you
         should set a private KnownHosts file for that connection profile.
         This is secure and avoids man-in-the-middle attack (as long as
         you actually verify the initial save of the SSH key!)

         The default file location will be:

                  ~/.vnc/ssh_known_hosts/profile-name.known

         but you can choose any place you like.  It must of course be
         unique and not shared with another ssh connection otherwise they
         both may complain about the key for 'localhost' changing, etc.
}

	.sshknownhosts.f.t insert end $msg

	frame .sshknownhosts.path
	label .sshknownhosts.path.l -text "SSH KnownHosts file:"
	entry .sshknownhosts.path.e -width 40 -textvariable ssh_known_hosts_filename
	button .sshknownhosts.path.b -text "Browse..." -command {set_ssh_known_hosts_file .sshknownhosts}

	pack .sshknownhosts.path.l -side left
	pack .sshknownhosts.path.e -side left -expand 1 -fill x
	pack .sshknownhosts.path.b -side left

	button .sshknownhosts.cancel -text "Cancel" -command {destroy .sshknownhosts; set ssh_known_hosts 0}
	bind .sshknownhosts <Escape> {destroy .sshknownhosts; set ssh_known_hosts 0}
	wm protocol .sshknownhosts WM_DELETE_WINDOW {destroy .sshknownhosts; set ssh_known_hosts 0}
	button .sshknownhosts.done -text "Done" -command {destroy .sshknownhosts; catch {raise .oa}}
	bind .sshknownhosts.path.e <Return> {destroy .sshknownhosts; catch {raise .oa}}

	pack .sshknownhosts.f .sshknownhosts.path .sshknownhosts.cancel .sshknownhosts.done -side top -fill x

	center_win .sshknownhosts
	wm resizable .sshknownhosts 1 0

	focus .sshknownhosts.path.e 
}

proc ssh_sec_dialog {} {
	global ssh_local_protection
	
	toplev .sshsec
	wm title .sshsec "SSH Local Port Protections"

	global help_font
	eval text .sshsec.t -width 80 -height 28 $help_font

	apply_bg .sshsec.t

	set msg {
    See the discussion of "Untrusted Local Users" in the main 'Help'
    panel for info about users who are able to log into the workstation
    you run SSVNC on and might try to use your encrypted tunnel to gain
    access to the remote VNC machine.

    On Unix, for SSH tunnels we have an LD_PRELOAD hack (lim_accept.so)
    that will limit ssh from accepting any local redirection connections
    after the first one or after 35 seconds, whichever comes first.
    The first SSH port redirection connection is intended to be the one
    that tunnels your VNC Viewer to reach the remote server.

    You can adjust these defaults LIM_ACCEPT=1 LIM_ACCEPT_TIME=35 by
    setting those env. vars. to different values. 

    Note that there is still a window of a few seconds the Untrusted
    Local User can try to connect before your VNC Viewer does.  So this
    method is far from perfect.  But once your VNC session is established,
    he should be blocked out.  Test to make sure blocking is taking place.

    Do not use this option if you are doing SSH Service redirections
    'Additional Port Redirections (via SSH)' that redirect a local port
    to the remote server via ssh -L.

    Note that if the shared object "lim_accept.so" cannot be found,
    this option has no effect.  Watch the output in the terminal for
    the "SSVNC_LIM_ACCEPT_PRELOAD" setting.
}
	.sshsec.t insert end $msg

	button .sshsec.cancel -text "Cancel" -command {set ssh_local_protection 0; destroy .sshsec}
	bind .sshsec <Escape> {set ssh_local_protection 0; destroy .sshsec}
	wm protocol .sshsec WM_DELETE_WINDOW {set ssh_local_protection 0; destroy .sshsec}
	button .sshsec.done -text "Done" -command {destroy .sshsec}

	pack .sshsec.t .sshsec.cancel .sshsec.done -side top -fill x

	center_win .sshsec
	wm resizable .sshsec 1 0
}

proc multilisten_dialog {} {
	global multiple_listen
	
	toplev .multil
	wm title .multil "Multiple LISTEN Connections"

	global help_font
	set h 36
	if [small_height] {
		set h 30
	}
	eval text .multil.t -width 84 -height $h $help_font

	apply_bg .multil.t

	set msg {
    Set this option to allow SSVNC (when in LISTEN / Reverse connections
    mode) to allow multiple VNC servers to connect at the same time and
    so display each of their desktops on your screen at the same time.

    This option only applies on Unix or MaOSX when using the supplied
    SSVNC vncviewer.  If you specify your own VNC Viewer it has no effect.

    On Windows (only the stock TightVNC viewer is provided) it has no effect
    because the Windows SSVNC can ONLY do "Multiple LISTEN Connections". 
    Similarly on MacOSX if the COTVNC viewer is used there is no effect.

    Rationale:  To play it safe, the Unix vncviewer provided by SSVNC
    (ssvncviewer) only allows one LISTEN reverse connection at a time.
    This is to prohibit malicious people on the network from depositing
    as many desktops on your screen as he likes, even if you are already
    connected to VNC server you desire.

    For example, perhaps the malicious user could trick you into typing
    a password into the desktop he displays on your screen.

    This protection is not perfect, because the malicious user could
    try to reverse connect to you before the correct VNC server reverse
    connects to you.  This is even more of a problem if you keep your
    SSVNC viewer in LISTEN mode but unconnected for long periods of time.
    Pay careful attention in this case if you are to supplying sensitive
    information to the remote desktop.

    Enable 'Multiple LISTEN Connections' if you want to disable the default
    protection in the Unix SSVNC vncviewer; i.e. allow multiple reverse
    connections simultaneously (all vnc viewers we know of do this by default)

    For more control, do not select 'Multiple LISTEN Connections', but
    rather set the env. var SSVNC_MULTIPLE_LISTEN=MAX:n to limit the number
    of simultaneous reverse connections to "n" 
}
	.multil.t insert end $msg

	button .multil.cancel -text "Cancel" -command {set multiple_listen 0; destroy .multil}
	bind .multil <Escape> {set multiple_listen 0; destroy .multil}
	wm protocol .multil WM_DELETE_WINDOW {set multiple_listen 0; destroy .multil}
	button .multil.done -text "Done" -command {destroy .multil}

	pack .multil.t .multil.cancel .multil.done -side top -fill x

	center_win .multil
	wm resizable .multil 1 0
}

proc use_grab_dialog {} {
	global usg_grab
	
	toplev .usegrb
	wm title .usegrb "Use XGrabServer (for fullscreen)"

	global help_font
	eval text .usegrb.t -width 85 -height 29 $help_font

	apply_bg .usegrb.t

	set msg {
    On Unix, some Window managers and some Desktops make it difficult for the
    SSVNC Unix VNC viewer to go into full screen mode (F9) and/or return.

    Sometimes one can go into full screen mode, but then your keystrokes or
    Mouse actions do not get through.  This can leave you trapped because you
    cannot inject input (F9 again) to get out of full screen mode.  (Tip:
    press Ctrl-Alt-F2 for a console login shell; then kill your vncviewer
    process, e.g. pkill vncviewer; then Alt-F7 to get back to your desktop)

    We have seen this in some very old Window managers (e.g. fvwm2 circa
    1998) and some very new Desktops (e.g. GNOME circa 2008).  We try
    to work around the problem on recent desktops by using the NEW_WM
    interface, but if you use Fullscreen, you may need to use this option.

    The default for the SSVNC Unix VNC viewer is '-grabkbd' mode where it will
    try to exclusively grab the keyboard.  This often works correctly.

    However if Fullscreen is not working properly, try setting this
    'Use XGrabServer' option to enable '-graball' mode where it tries to grab
    the entire X server.  This usually works, but can be a bit flakey.

    Sometimes toggling F9 a few times gets lets the vncviewer fill the whole
    screen.  Sometimes tapping F9 very quickly gets it to snap in.  If GNOME
    (or whatever desktop) is still showing its taskbars, it is recommended
    you toggle F9 until it isn't. Otherwise, it is not clear who gets the input.

    Best of luck.
}
	.usegrb.t insert end $msg

	button .usegrb.cancel -text "Cancel" -command {set use_grab 0; destroy .usegrb}
	bind .usegrb <Escape> {set use_grab 0; destroy .usegrb}
	wm protocol .usegrb WM_DELETE_WINDOW {set use_grab 0; destroy .usegrb}
	button .usegrb.done -text "Done" -command {destroy .usegrb}

	pack .usegrb.t .usegrb.cancel .usegrb.done -side top -fill x

	center_win .usegrb
	wm resizable .usegrb 1 0
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
	# XXX host_part
	regsub {^vnc://} $host "" host
	regsub {^.*@} $host "" host
	regsub {:[0-9][0-9]*$} $host "" host
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

		if {[regexp {^(.*):([0-9][0-9]*)$} $line mv host port]} {
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
			set sockets($i) $s
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
			set $s $sockets($j)
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

	set h 35
	if [small_height] {
		set h 22
	} elseif {$uname == "Darwin"} {
		set h 25
	}
	scroll_text .pk.f 85 $h

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
	wm protocol .pk WM_DELETE_WINDOW {set use_port_knocking 0; destroy .pk}
	button .pk.done -text "Done" -command {if {$use_port_knocking} {set port_knocking_list [.pk.rule get 1.0 end]}; destroy .pk}

	pack .pk.done .pk.cancel .pk.rule .pk.info -side bottom -fill x
	pack .pk.f -side top -fill both -expand 1

	center_win .pk
}

proc choose_desktop_dialog {} {
	toplev .sd
	wm title .sd "Desktop Type"
	global ts_desktop_type choose_desktop

	global ts_desktop_type_def
	set def "kde"
	if {$ts_desktop_type_def != ""} {
		set def $ts_desktop_type_def
	}

	if {$ts_desktop_type == ""} {
		set ts_desktop_type $def
	}

	label .sd.l1 -anchor w -text "Select the type of remote Desktop"
	label .sd.l2 -anchor w -text "for your session (default: $def)"

	radiobutton .sd.b1 -anchor w -variable ts_desktop_type -value kde      -text kde
	radiobutton .sd.b2 -anchor w -variable ts_desktop_type -value gnome    -text gnome
	radiobutton .sd.b3 -anchor w -variable ts_desktop_type -value Xsession -text cde
	radiobutton .sd.b4 -anchor w -variable ts_desktop_type -value mwm      -text mwm
	radiobutton .sd.b5 -anchor w -variable ts_desktop_type -value wmaker   -text wmaker
	radiobutton .sd.b6 -anchor w -variable ts_desktop_type -value xfce     -text xfce
	radiobutton .sd.b7 -anchor w -variable ts_desktop_type -value enlightenment   -text enlightenment
	radiobutton .sd.b8 -anchor w -variable ts_desktop_type -value twm      -text twm
	radiobutton .sd.b9 -anchor w -variable ts_desktop_type -value failsafe -text failsafe

	button .sd.cancel -text "Cancel" -command {destroy .sd; set choose_desktop 0; set ts_desktop_type ""}
	bind .sd <Escape> {destroy .sd; set choose_desktop 0; set ts_desktop_type ""}
	wm protocol .sd WM_DELETE_WINDOW {destroy .sd; set choose_desktop 0; set ts_desktop_type ""}
	button .sd.done -text "Done" -command {destroy .sd}

	pack .sd.l1 .sd.l2 .sd.b1 .sd.b2 .sd.b3 .sd.b4 .sd.b5 .sd.b6 .sd.b7 .sd.b8 .sd.b9 .sd.cancel .sd.done -side top -fill x

	center_win .sd
}

proc choose_size_dialog {} {
	toplev .sz
	wm title .sz "Desktop Size"
	global ts_desktop_size ts_desktop_depth choose_desktop_geom

	set def1 "1280x1024"
	set def2 "16"

	global ts_desktop_size_def ts_desktop_depth_def
	if {$ts_desktop_size_def != ""} {
		set def1 $ts_desktop_size_def
	}
	if {$ts_desktop_depth_def != ""} {
		set def2 $ts_desktop_depth_def
	}

	if {$ts_desktop_size == ""} {
		set ts_desktop_size $def1
	}
	if {$ts_desktop_depth == ""} {
		set ts_desktop_depth $def2
	}

	label .sz.l1 -anchor w -text "Select the Size and Color depth"
	label .sz.l2 -anchor w -text "for your Desktop session."
	label .sz.l3 -anchor w -text "Default: $def1 and $def2 bits/pixel."

	label .sz.g0 -anchor w -text "Width x Height:" -relief groove

	radiobutton .sz.g1 -anchor w -variable ts_desktop_size -value "640x480"   -text "    640x480"
	radiobutton .sz.g2 -anchor w -variable ts_desktop_size -value "800x600"   -text "    800x600"
	radiobutton .sz.g3 -anchor w -variable ts_desktop_size -value "1024x768"  -text "  1024x768"
	radiobutton .sz.g4 -anchor w -variable ts_desktop_size -value "1280x1024" -text "1280x1024"
	radiobutton .sz.g5 -anchor w -variable ts_desktop_size -value "1400x1050" -text "1400x1050"
	radiobutton .sz.g6 -anchor w -variable ts_desktop_size -value "1600x1200" -text "1600x1200"
	radiobutton .sz.g7 -anchor w -variable ts_desktop_size -value "1920x1200" -text "1920x1200"

	frame .sz.c
	label .sz.c.l -anchor w -text "Custom:"
	entry .sz.c.e -width 10 -textvariable ts_desktop_size
	pack .sz.c.l -side left
	pack .sz.c.e -side left -expand 1 -fill x
	bind .sz.c.e <Return> {destroy .sz}

	label .sz.d0 -anchor w -text "Color Depth:" -relief groove

	radiobutton .sz.d1 -anchor w -variable ts_desktop_depth -value "8" -text  "  8 bits/pixel"
	radiobutton .sz.d2 -anchor w -variable ts_desktop_depth -value "16" -text "16 bits/pixel"
	radiobutton .sz.d3 -anchor w -variable ts_desktop_depth -value "24" -text "24 bits/pixel"

	button .sz.cancel -text "Cancel" -command {destroy .sz; set choose_desktop_geom 0; set ts_desktop_size ""; set ts_desktop_depth ""}
	bind .sz <Escape> {destroy .sz; set choose_desktop_geom 0; set ts_desktop_size ""; set ts_desktop_depth ""}
	wm protocol .sz WM_DELETE_WINDOW {destroy .sz; set choose_desktop_geom 0; set ts_desktop_size ""; set ts_desktop_depth ""}
	button .sz.done -text "Done" -command {destroy .sz}

	pack .sz.l1 .sz.l2 .sz.l3 \
		.sz.g0 .sz.g1 .sz.g2 .sz.g3 .sz.g4 .sz.g5 .sz.g6 .sz.g7 \
		.sz.c \
		.sz.d0 .sz.d1 .sz.d2 .sz.d3 \
		.sz.cancel .sz.done -side top -fill x

	center_win .sz
	focus .sz.c.e
}

proc choose_xserver_dialog {} {
	toplev .st
	wm title .st "X Server Type"
	global ts_xserver_type choose_xserver

	set def "Xvfb"
	global ts_xserver_type_def
	if {$ts_xserver_type_def != ""} {
		set def $ts_xserver_type_def
	}

	if {$ts_xserver_type == ""} {
		set ts_xserver_type $def
	}

	label .st.l1 -anchor w -text "Select the type of remote X server"
	label .st.l2 -anchor w -text "for your session (default: $def)"

	radiobutton .st.b1 -anchor w -variable ts_xserver_type -value Xvfb -text "Xvfb"

	radiobutton .st.b2 -anchor w -variable ts_xserver_type -value Xdummy -text "Xdummy"

	radiobutton .st.b3 -anchor w -variable ts_xserver_type -value Xvnc -text "Xvnc"

	radiobutton .st.b4 -anchor w -variable ts_xserver_type -value Xvnc.redirect -text "Xvnc.redirect"

	button .st.cancel -text "Cancel" -command {destroy .st; set choose_xserver 0; set ts_xserver_type ""}
	bind .st <Escape> {destroy .st; set choose_xserver 0; set ts_xserver_type ""}
	wm protocol .st WM_DELETE_WINDOW {destroy .st; set choose_xserver 0; set ts_xserver_type ""}
	button .st.done -text "Done" -command {destroy .st}

	pack .st.l1 .st.l2 .st.b1 .st.b2 .st.b3 .st.b4 .st.cancel .st.done -side top -fill x

	center_win .st
}

proc set_ts_options {} {
	global use_cups use_sound use_smbmnt
	global change_vncviewer choose_xserver
	global ts_only is_windows
	global darwin_cotvnc use_x11_macosx uname
	if {! $ts_only} {
		return
	}
	catch {destroy .o}
	toplev .ot
	wm title .ot "Options"

	set i 1

	checkbutton .ot.b$i -anchor w -variable choose_desktop -text \
		"Desktop Type" \
		-command {if {$choose_desktop} {choose_desktop_dialog}}
	incr i

	checkbutton .ot.b$i -anchor w -variable choose_desktop_geom -text \
		"Desktop Size" \
		-command {if {$choose_desktop_geom} {choose_size_dialog}}
	incr i

	checkbutton .ot.b$i -anchor w -variable choose_xserver -text \
		"X Server Type" \
		-command {if {$choose_xserver} {choose_xserver_dialog}}
	incr i

	checkbutton .ot.b$i -anchor w -variable use_cups -text \
		"Enable Printing" \
		-command {if {$use_cups} {cups_dialog}}
	incr i

	checkbutton .ot.b$i -anchor w -variable use_sound -text \
		"Enable Sound" \
		-command {if {$use_sound} {sound_dialog}}
	incr i

#	checkbutton .ot.b$i -anchor w -variable use_smbmnt -text \
#		"Enable SMB mount tunnelling" \
#		-command {if {$use_smbmnt} {smb_dialog}}
#	incr i

	checkbutton .ot.b$i -anchor w -variable choose_filexfer -text \
		"File Transfer" \
		-command {if {$choose_filexfer} {ts_filexfer_dialog}}
	incr i

	checkbutton .ot.b$i -anchor w -variable use_viewonly -text \
		"View Only"
	incr i

	checkbutton .ot.b$i -anchor w -variable change_vncviewer -text \
		"Change VNC Viewer" \
		-command change_vncviewer_dialog_wrap
	incr i

	if {!$is_windows && $uname == "Darwin"} {
		checkbutton .ot.b$i -anchor w -variable use_x11_macosx -text \
			"X11 viewer MacOSX" \
			-command {if {$use_x11_macosx} {set darwin_cotvnc 0} else {set darwin_cotvnc 1}; set_darwin_cotvnc_buttons}
		incr i
	}

	button .ot.b$i -anchor w -text "   Delete Profile..." \
		-command {destroy .ot; delete_profile}
	incr i

	button .ot.b$i -anchor w -text "   Advanced ..." -command {set_ts_adv_options}
	incr i

	for {set j 1} {$j < $i} {incr j} {
		pack .ot.b$j -side top -fill x
	}

	frame .ot.b
	button .ot.b.done -text "Done" -command {destroy .ot}
	button .ot.b.help -text "Help" -command help_ts_opts
	pack .ot.b.help .ot.b.done -fill x -expand 1 -side left

	bind .ot <Escape> {destroy .ot}
	wm protocol .ot WM_DELETE_WINDOW {destroy .ot}

	pack .ot.b -side top -fill x 

	center_win .ot
	wm resizable .ot 1 0
	focus .ot
}

proc set_ts_adv_options {} {
	global ts_only ts_unixpw ts_vncshared
	global ts_ncache ts_multisession
	global choose_othervnc darwin_cotvnc choose_sleep
	global is_windows

	if {! $ts_only} {
		return
	}
	catch {destroy .ot}
	toplev .ot2
	wm title .ot2 "Advanced"

	set i 1

	checkbutton .ot2.b$i -anchor w -variable ts_vncshared -text \
		"VNC Shared" \
		-command {if {$ts_vncshared} {ts_vncshared_dialog}}
	incr i

	checkbutton .ot2.b$i -anchor w -variable choose_multisession -text \
		"Multiple Sessions" \
		-command {if {$choose_multisession} {ts_multi_dialog}}
	incr i

	checkbutton .ot2.b$i -anchor w -variable ts_xlogin -text \
		"X Login Greeter" \
		-command {if {$ts_xlogin} {ts_xlogin_dialog}}
	incr i

	checkbutton .ot2.b$i -anchor w -variable choose_othervnc -text \
		"Other VNC Server" \
		-command {if {$choose_othervnc} {ts_othervnc_dialog}}
	incr i

	checkbutton .ot2.b$i -anchor w -variable ts_unixpw -text \
		"Use unixpw" \
		-command {if {$ts_unixpw} {ts_unixpw_dialog}}
	incr i

	checkbutton .ot2.b$i -anchor w -variable use_bgr233 -text \
		"Client 8bit Color"
	if {$darwin_cotvnc} {.ot2.b$i configure -state disabled}
	global darwin_cotvnc_blist
	set darwin_cotvnc_blist(.ot2.b$i) 1
	incr i

	checkbutton .ot2.b$i -anchor w -variable choose_ncache -text \
		"Client-Side Caching" \
		-command {if {$choose_ncache} {ts_ncache_dialog}}
	incr i

	checkbutton .ot2.b$i -anchor w -variable choose_x11vnc_opts -text \
		"X11VNC Options" \
		-command {if {$choose_x11vnc_opts} {ts_x11vnc_opts_dialog}}
	incr i

	checkbutton .ot2.b$i -anchor w -variable choose_sleep -text \
		"Extra Sleep" \
		-command {if {$choose_sleep} {ts_sleep_dialog}}
	incr i

        if {$is_windows} {
		checkbutton .ot2.b$i -anchor w -variable choose_parg -text \
			"Putty Args" \
			-command {if {$choose_parg} {ts_putty_args_dialog}}
		incr i
        }

	if {!$is_windows} {
		checkbutton .ot2.b$i -anchor w -variable ssh_local_protection -text \
			"SSH Local Protections" \
			-command {if {$ssh_local_protection} {ssh_sec_dialog}}
		if {$is_windows} {.ot2.b$i configure -state disabled}
		incr i

		checkbutton .ot2.b$i -anchor w -variable ssh_known_hosts -text \
			"SSH KnownHosts file" \
			-command {if {$ssh_known_hosts} {ssh_known_hosts_dialog}}
		if {$is_windows} {.ot2.b$i configure -state disabled}
		incr i
	}

	if {$is_windows} {
		button .ot2.b$i -anchor w -text "   Putty Agent" \
			-command {catch {exec pageant.exe &}}
		incr i

		button .ot2.b$i -anchor w -text "   Putty Key-Gen" \
			-command {catch {exec puttygen.exe &}}
		incr i
	}

	global env
	if {![info exists env(SSVNC_TS_ALWAYS)]} {
		button .ot2.b$i -anchor w -text "   SSVNC Mode" \
			-command {destroy .ot2; to_ssvnc}
		incr i
	}

	if {!$is_windows} {
		button .ot2.b$i -anchor w -text "   Unix ssvncviewer ..." \
			-command {set_ssvncviewer_options}
		if {$is_windows} {
			.ot2.b$i configure -state disabled
		}
		global change_vncviewer
		if {$change_vncviewer} {
			.ot2.b$i configure -state disabled
		}
		global ts_uss_button
		set ts_uss_button .ot2.b$i
		incr i
	}

	for {set j 1} {$j < $i} {incr j} {
		pack .ot2.b$j -side top -fill x
	}

	frame .ot2.b
	button .ot2.b.done -text "Done" -command {destroy .ot2}
	button .ot2.b.help -text "Help" -command help_ts_opts
	pack .ot2.b.help .ot2.b.done -fill x -expand 1 -side left

	bind .ot2 <Escape> {destroy .ot2}
	wm protocol .ot2 WM_DELETE_WINDOW {destroy .ot2}

	pack .ot2.b -side top -fill x 

	center_win .ot2
	wm resizable .ot2 1 0
	focus .ot2
}

proc change_vncviewer_dialog_wrap {} {
	global change_vncviewer ts_uss_button is_windows
	if {$change_vncviewer} {
		change_vncviewer_dialog
		catch {tkwait window .chviewer}
	}
	if {$change_vncviewer || $is_windows} {
		catch {.oa.ss configure -state disabled}
	} else {
		catch {.oa.ss configure -state normal}
	}
	if [info exists ts_uss_button] {
		if {$change_vncviewer || $is_windows} {
			catch {$ts_uss_button configure -state disabled}
		} else {
			catch {$ts_uss_button configure -state normal}
		}
	}
}

proc set_advanced_options {} {
	global use_cups use_sound use_smbmnt
	global change_vncviewer
	global use_port_knocking port_knocking_list
	global is_windows darwin_cotvnc
	global use_ssh use_sshssl
	global use_x11_macosx
	global adv_ssh
	global showing_no_encryption
	global x11vnc_xlogin_widget

	catch {destroy .o}
	toplev .oa
	wm title .oa "Advanced Options"

	set i 1

	checkbutton .oa.b$i -anchor w -variable use_cups -text \
		"Enable CUPS Print tunnelling" \
		-command {if {$use_cups} {cups_dialog}}
	if {!$use_ssh && !$use_sshssl} {.oa.b$i configure -state disabled}
	set adv_ssh(cups) .oa.b$i
	incr i

	checkbutton .oa.b$i -anchor w -variable use_sound -text \
		"Enable ESD/ARTSD Audio tunnelling" \
		-command {if {$use_sound} {sound_dialog}}
	if {!$use_ssh && !$use_sshssl} {.oa.b$i configure -state disabled}
	set adv_ssh(snd) .oa.b$i
	incr i

	checkbutton .oa.b$i -anchor w -variable use_smbmnt -text \
		"Enable SMB mount tunnelling" \
		-command {if {$use_smbmnt} {smb_dialog}}
	if {!$use_ssh && !$use_sshssl} {.oa.b$i configure -state disabled}
	set adv_ssh(smb) .oa.b$i
	incr i

	checkbutton .oa.b$i -anchor w -variable use_x11vnc_xlogin -text \
		"Automatically Find X Login/Greeter" -command {x11vnc_find_adjust "xlogin"}
	if {!$use_ssh && !$use_sshssl} {.oa.b$i configure -state disabled}
	set x11vnc_xlogin_widget ".oa.b$i"
	incr i

	checkbutton .oa.b$i -anchor w -variable additional_port_redirs -text \
		"Additional Port Redirs (via SSH)" \
		-command {if {$additional_port_redirs} {port_redir_dialog}}
	if {!$use_ssh && !$use_sshssl} {.oa.b$i configure -state disabled}
	set adv_ssh(redirs) .oa.b$i
	incr i

	global use_ssl use_ssh use_sshssl

	if {!$is_windows} {
		checkbutton .oa.b$i -anchor w -variable ssh_known_hosts -text \
			"Private SSH KnownHosts file" \
			-command {if {$ssh_known_hosts} {ssh_known_hosts_dialog}}
		set adv_ssh(knownhosts) .oa.b$i
		if {$use_ssl}    {.oa.b$i configure -state disabled}
		if {$is_windows} {.oa.b$i configure -state disabled}
		incr i

		checkbutton .oa.b$i -anchor w -variable ssh_local_protection -text \
			"SSH Local Port Protections" \
			-command {if {$ssh_local_protection} {ssh_sec_dialog}}
		global ssh_local_protection_button
		set ssh_local_protection_button .oa.b$i
		if {$use_ssl}    {.oa.b$i configure -state disabled}
		if {$is_windows} {.oa.b$i configure -state disabled}
		incr i
	}

   global ssh_only
   if {!$ssh_only} {
	if {!$is_windows} {
		checkbutton .oa.b$i -anchor w -variable stunnel_local_protection -text \
			"STUNNEL Local Port Protections" \
			-command {if {$stunnel_local_protection} {stunnel_sec_dialog}}
		global stunnel_local_protection_button
		set stunnel_local_protection_button .oa.b$i
		if {$use_ssh}    {.oa.b$i configure -state disabled}
		if {$is_windows} {.oa.b$i configure -state disabled}
		incr i
	}

	checkbutton .oa.b$i -anchor w -variable disable_ssl_workarounds -text \
		"Disable SSL Workarounds" \
		-command {if {$disable_ssl_workarounds} {disable_ssl_workarounds_dialog}}
	global disable_ssl_workarounds_button
	set disable_ssl_workarounds_button .oa.b$i
	if {$use_ssh}    {.oa.b$i configure -state disabled}
	incr i

	if {!$is_windows} {
		checkbutton .oa.b$i -anchor w -variable ultra_dsm -text \
			"UltraVNC DSM Encryption Plugin" \
			-command {if {$ultra_dsm} {ultra_dsm_dialog}}
		global ultra_dsm_button
		set ultra_dsm_button .oa.b$i
		if {$is_windows} {.oa.b$i configure -state disabled}
		if {$use_ssh}    {.oa.b$i configure -state disabled}
		incr i
	}

	checkbutton .oa.b$i -anchor w -variable no_probe_vencrypt -text \
		"Do not Probe for VeNCrypt"
	global no_probe_vencrypt_button
	set no_probe_vencrypt_button .oa.b$i
	if {$use_ssh}    {.oa.b$i configure -state disabled}
	incr i

	checkbutton .oa.b$i -anchor w -variable server_vencrypt -text \
		"Server uses VeNCrypt SSL encryption"
	global vencrypt_button
	set vencrypt_button .oa.b$i
	if {$use_ssh}    {.oa.b$i configure -state disabled}
	incr i

	checkbutton .oa.b$i -anchor w -variable server_anondh -text \
		"Server uses Anonymous Diffie-Hellman" -command no_certs_tutorial_mesg
	global anondh_button
	set anondh_button .oa.b$i
	if {$use_ssh}    {.oa.b$i configure -state disabled}
	incr i
   }

	checkbutton .oa.b$i -anchor w -variable change_vncviewer -text \
		"Change VNC Viewer" \
		-command change_vncviewer_dialog_wrap
	incr i

	checkbutton .oa.b$i -anchor w -variable use_port_knocking -text \
		"Port Knocking" \
		-command {if {$use_port_knocking} {port_knocking_dialog}}
	incr i

	for {set j 1} {$j < $i} {incr j} {
		pack .oa.b$j -side top -fill x
	}

	global include_list extra_sleep
	frame .oa.fis
	frame .oa.fis.fL
	frame .oa.fis.fR
	label .oa.fis.fL.la -anchor w -text "Include:"
	label .oa.fis.fL.lb -anchor w -text "Sleep:"
	if {$is_windows} {
		label .oa.fis.fL.lc -anchor w -text "Putty Args:"
		pack .oa.fis.fL.la .oa.fis.fL.lb .oa.fis.fL.lc -side top -fill x
	} else {
		pack .oa.fis.fL.la .oa.fis.fL.lb -side top -fill x
	}

	entry .oa.fis.fR.ea -width 10 -textvariable include_list
	entry .oa.fis.fR.eb -width 10 -textvariable extra_sleep
	if {$is_windows} {
		entry .oa.fis.fR.ec -width 10 -textvariable putty_args
		pack .oa.fis.fR.ea .oa.fis.fR.eb .oa.fis.fR.ec -side top -fill x
	} else {
		pack .oa.fis.fR.ea .oa.fis.fR.eb -side top -fill x
	}

	pack .oa.fis.fL -side left
	pack .oa.fis.fR -side right -expand 1 -fill x

	pack .oa.fis -side top -fill x


	if {!$is_windows} {
		global uname
		set t1 "         Unix ssvncviewer ..."
		if {$uname == "Darwin" } { regsub {^ *} $t1 "" t1 }
		button .oa.ss -anchor w -text $t1 -command set_ssvncviewer_options
		pack   .oa.ss -side top -fill x
		if {$is_windows} {
			.oa.ss configure -state disabled
		}
		global change_vncviewer
		if {$change_vncviewer} {
			.oa.ss configure -state disabled
		}

		set t2 "         Use ssh-agent"
		if {$uname == "Darwin" } { regsub {^ *} $t2 "" t2 }

		button .oa.sa -anchor w -text $t2 -command ssh_agent_restart
		pack .oa.sa -side top -fill x
		if {$is_windows} {
			.oa.sa configure -state disabled
		}
	} else {
		set t1 "         Launch Putty Agent"
		button .oa.pa -anchor w -text $t1 -command {catch {exec pageant.exe &}}
		pack   .oa.pa -side top -fill x

		set t2 "         Launch Putty Key-Gen"
		button .oa.pg -anchor w -text $t2 -command {catch {exec puttygen.exe &}}
		pack   .oa.pg -side top -fill x
	}

	frame .oa.b
	button .oa.b.done -text "Done" -command {destroy .oa}
	bind .oa <Escape> {destroy .oa}
	wm protocol .oa WM_DELETE_WINDOW {destroy .oa}
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

proc set_ssvncviewer_options {} {
	global is_windows darwin_cotvnc
	global use_ssh use_sshssl use_x11cursor use_rawlocal use_notty use_popupfix use_alpha use_turbovnc disable_pipeline use_grab use_nobell
	global use_send_clipboard use_send_always
	global ssvnc_scale ssvnc_escape
	global server_vencrypt server_anondh

	if {$is_windows} {
		return
	}

	catch {destroy .oa}
	toplev .os
	wm title .os "Unix ssvncviewer Options"

	set darwinlist [list]

	set f0 .os.f
	frame $f0
	set fl $f0.fl
	frame $fl
	set fr $f0.fr
	frame $fr

	set i 1
	set j 1

	checkbutton $fl.b$i -anchor w -variable multiple_listen -text \
		"Multiple LISTEN Connections" \
		-command {if {$multiple_listen} {multilisten_dialog}}
	global multiple_listen_button use_listen
	set multiple_listen_button $fl.b$i
	if {$is_windows}  {$fl.b$i configure -state disabled}
	if {!$use_listen} {$fl.b$i configure -state disabled}
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	checkbutton $fl.b$i -anchor w -variable listen_once -text \
		"Listen Once"
	global listen_once_button
	set listen_once_button $fl.b$i
	if {!$use_listen} {$fl.b$i configure -state disabled}
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	checkbutton $fl.b$i -anchor w -variable listen_accept_popup -text \
		"Listen Accept Popup Dialog" \
		-command { if {$listen_accept_popup} { catch {$listen_accept_popup_button_sc configure -state normal} } else { catch {$listen_accept_popup_button_sc  configure -state disabled} } }
	global listen_accept_popup_button
	set listen_accept_popup_button $fl.b$i
	if {!$use_listen} {$fl.b$i configure -state disabled}
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	global listen_accept_popup
	checkbutton $fl.b$i -anchor w -variable listen_accept_popup_sc -text \
		"   Accept Popup UltraVNC Single Click"
	global listen_accept_popup_button_sc
	set listen_accept_popup_button_sc $fl.b$i
	if {!$use_listen} {$fl.b$i configure -state disabled}
	if {!$listen_accept_popup} {$fl.b$i configure -state disabled}
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	checkbutton $fl.b$i -anchor w -variable use_x11cursor -text \
		"Use X11 Cursor"
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	checkbutton $fl.b$i -anchor w -variable use_nobell -text \
		"Disable Bell"
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	checkbutton $fl.b$i -anchor w -variable use_rawlocal -text \
		"Use Raw Local"
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	checkbutton $fl.b$i -anchor w -variable use_notty -text \
		"Avoid Using Terminal"
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	checkbutton $fl.b$i -anchor w -variable use_popupfix -text \
		"Use Popup Fix"
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	checkbutton $fl.b$i -anchor w -variable use_grab -text \
		"Use XGrabServer (for fullscreen)" \
		-command {if {$use_grab} {use_grab_dialog}}
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	checkbutton $fl.b$i -anchor w -variable use_alpha -text \
		"Cursor Alphablending (32bpp required)     "
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	checkbutton $fl.b$i -anchor w -variable use_turbovnc -text \
		"TurboVNC (if available on platform)"
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	checkbutton $fl.b$i -anchor w -variable disable_pipeline -text \
		"Disable Pipelined Updates"
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	checkbutton $fl.b$i -anchor w -variable use_send_clipboard -text \
		"Send CLIPBOARD not PRIMARY"
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	checkbutton $fl.b$i -anchor w -variable use_send_always -text \
		"Send Selection Every time"
	lappend darwinlist $fl.b$i; if {$darwin_cotvnc} {$fl.b$i configure -state disabled}
	incr i

	set relief ridge

	frame $fr.b$j -height 2; incr j

	frame $fr.b$j -relief $relief -borderwidth 2

	global ffont
	label $fr.b$j.l -font $ffont -anchor w -text "Examples: '0.75', '1024x768', 'fit' (fill screen), or 'auto' ";

	global ssvnc_scale
	frame $fr.b$j.f
	label $fr.b$j.f.l -text "Scaling: "
	lappend darwinlist $fr.b$j.f.l; if {$darwin_cotvnc} {$fr.b$j.f.l configure -state disabled}
	entry $fr.b$j.f.e -width 10 -textvariable ssvnc_scale
	lappend darwinlist $fr.b$j.f.e; if {$darwin_cotvnc} {$fr.b$j.f.e configure -state disabled}
	pack $fr.b$j.f.l -side left
	pack $fr.b$j.f.e -side right -expand 1 -fill x

	pack $fr.b$j.f $fr.b$j.l -side top -fill x

	incr j

	frame $fr.b$j -height 2; incr j

	frame $fr.b$j -relief $relief -borderwidth 2

	label $fr.b$j.l -font $ffont -anchor w -text "Examples: 'default', 'Control_L,Alt_L', 'never'";

	global ssvnc_escape
	frame $fr.b$j.f
	label $fr.b$j.f.l -text "Escape Keys: "
	lappend darwinlist $fr.b$j.f.l; if {$darwin_cotvnc} {$fr.b$j.f.l configure -state disabled}
	entry $fr.b$j.f.e -width 10 -textvariable ssvnc_escape
	lappend darwinlist $fr.b$j.f.e; if {$darwin_cotvnc} {$fr.b$j.f.e configure -state disabled}
	button $fr.b$j.f.b -relief ridge -text Help -command ssvnc_escape_help
	lappend darwinlist $fr.b$j.f.b; if {$darwin_cotvnc} {$fr.b$j.f.b configure -state disabled}
	pack $fr.b$j.f.l -side left
	pack $fr.b$j.f.b -side right
	pack $fr.b$j.f.e -side right -expand 1 -fill x

	pack $fr.b$j.f $fr.b$j.l -side top -fill x

	incr j

	frame $fr.b$j -height 2; incr j

	frame $fr.b$j -relief $relief -borderwidth 2

	label $fr.b$j.l -font $ffont -anchor w -text "Enter the max height in pixels, e.g. '900'";

	global ycrop_string
	frame $fr.b$j.f
	label $fr.b$j.f.l -text "Y Crop: "
	lappend darwinlist $fr.b$j.f.l; if {$darwin_cotvnc} {$fr.b$j.f.l configure -state disabled}
	entry $fr.b$j.f.e -width 10 -textvariable ycrop_string
	lappend darwinlist $fr.b$j.f.e; if {$darwin_cotvnc} {$fr.b$j.f.e configure -state disabled}
	pack $fr.b$j.f.l -side left
	pack $fr.b$j.f.e -side right -expand 1 -fill x

	pack $fr.b$j.f $fr.b$j.l -side top -fill x

	incr j

	frame $fr.b$j -height 2; incr j

	frame $fr.b$j -relief $relief -borderwidth 2

	label $fr.b$j.l -font $ffont -anchor w -text "Enter the scrollbar width in pixels, e.g. '4'";

	global sbwid_string
	frame $fr.b$j.f
	label $fr.b$j.f.l -text "ScrollBar Width: "
	lappend darwinlist $fr.b$j.f.l; if {$darwin_cotvnc} {$fr.b$j.f.l configure -state disabled}
	entry $fr.b$j.f.e -width 10 -textvariable sbwid_string
	lappend darwinlist $fr.b$j.f.e; if {$darwin_cotvnc} {$fr.b$j.f.e configure -state disabled}
	pack $fr.b$j.f.l -side left
	pack $fr.b$j.f.e -side right -expand 1 -fill x

	pack $fr.b$j.f $fr.b$j.l -side top -fill x

	incr j

	frame $fr.b$j -height 2; incr j

	frame $fr.b$j -relief $relief -borderwidth 2

	label $fr.b$j.l  -font $ffont -anchor w -text "Enter the RFB version to pretend to be using, e.g. '3.4'";
	label $fr.b$j.l2 -font $ffont -anchor w -text "Sometimes needed for UltraVNC: 3.4, 3.6, 3.14, 3.16";

	global rfbversion
	frame $fr.b$j.f
	label $fr.b$j.f.l -text "RFB Version: "
	lappend darwinlist $fr.b$j.f.l; if {$darwin_cotvnc} {$fr.b$j.f.l configure -state disabled}
	entry $fr.b$j.f.e -width 10 -textvariable rfbversion
	lappend darwinlist $fr.b$j.f.e; if {$darwin_cotvnc} {$fr.b$j.f.e configure -state disabled}
	pack $fr.b$j.f.l -side left
	pack $fr.b$j.f.e -side right -expand 1 -fill x

	pack $fr.b$j.f $fr.b$j.l $fr.b$j.l2  -side top -fill x

	incr j

	frame $fr.b$j -height 2; incr j

	frame $fr.b$j -relief $relief -borderwidth 2

	label $fr.b$j.l1 -font $ffont -anchor w -text "List encodings in preferred order, for example";
	label $fr.b$j.l2 -font $ffont -anchor w -text "'copyrect zrle tight'   The full list of encodings is:";
	label $fr.b$j.l3 -font $ffont -anchor w -text "copyrect tight zrle zywrle hextile zlib corre rre raw";

	global ssvnc_encodings
	frame $fr.b$j.f
	label $fr.b$j.f.l -text "Encodings: "
	lappend darwinlist $fr.b$j.f.l; if {$darwin_cotvnc} {$fr.b$j.f.l configure -state disabled}
	entry $fr.b$j.f.e -width 10 -textvariable ssvnc_encodings
	lappend darwinlist $fr.b$j.f.e; if {$darwin_cotvnc} {$fr.b$j.f.e configure -state disabled}
	pack $fr.b$j.f.l -side left
	pack $fr.b$j.f.e -side right -expand 1 -fill x

	pack $fr.b$j.f $fr.b$j.l1 $fr.b$j.l2 $fr.b$j.l3 -side top -fill x

	incr j

	frame $fr.b$j -height 2; incr j

	frame $fr.b$j -relief $relief -borderwidth 2

	label $fr.b$j.l1 -font $ffont -anchor w -text "Add any extra options for ssvncviewer that you want.";
	label $fr.b$j.l2 -font $ffont -anchor w -text "For example: -16bpp -appshare -noshm etc. See Help for a list.";

	global ssvnc_extra_opts
	frame $fr.b$j.f
	label $fr.b$j.f.l -text "Extra Options: "
	lappend darwinlist $fr.b$j.f.l; if {$darwin_cotvnc} {$fr.b$j.f.l configure -state disabled}
	entry $fr.b$j.f.e -width 10 -textvariable ssvnc_extra_opts
	lappend darwinlist $fr.b$j.f.e; if {$darwin_cotvnc} {$fr.b$j.f.e configure -state disabled}
	pack $fr.b$j.f.l -side left
	pack $fr.b$j.f.e -side right -expand 1 -fill x

	pack $fr.b$j.f $fr.b$j.l1 $fr.b$j.l2 -side top -fill x

	incr j

	frame $fr.b$j -height 2; incr j

	for {set k 1} {$k < $i} {incr k} {
		pack $fl.b$k -side top -fill x
	}
	for {set k 1} {$k < $j} {incr k} {
		pack $fr.b$k -side top -fill x
	}

	pack $fl -side left -fill both
	pack $fr -side left -fill both -expand 1

	pack $f0 -side top -fill both

	frame .os.b
	button .os.b.done -text "Done" -command {destroy .os}
	bind .os <Escape> {destroy .os}
	wm protocol .os WM_DELETE_WINDOW {destroy .os}
	button .os.b.help -text "Help" -command help_ssvncviewer_opts

	global use_listen
	if {$use_listen} {
		button .os.b.connect -text "Listen" -command launch
	} else {
		button .os.b.connect -text "Connect" -command launch
	}

	pack .os.b.help .os.b.connect .os.b.done -fill x -expand 1 -side left

	pack .os.b -side top -fill x 

	global darwin_cotvnc_blist
	foreach b $darwinlist {
		set darwin_cotvnc_blist($b) 1
	}

	center_win .os
	wm resizable .os 1 0
	wm minsize .os [winfo reqwidth .os] [winfo reqheight .os] 
	focus .os
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
	set tmp $env(SSVNC_HOME)/.vnc-sa[tpid]
	set tmp [mytmp $tmp]
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
	puts $fh "rm -f $tmp"
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
proc adv_ssh_tog {on} {
	global adv_ssh
	foreach b {cups snd smb redirs knownhosts} {
		if [info exists adv_ssh($b)] {
			if {$on} {
				catch {$adv_ssh($b) configure -state normal}
			} else {
				catch {$adv_ssh($b) configure -state disabled}
			}
		}
	}
}

proc adv_listen_ssl_tog {on} {
	global stunnel_local_protection_button is_windows
	global disable_ssl_workarounds_button
	global vencrypt_button no_probe_vencrypt_button anondh_button ultra_dsm_button

	set blist [list] 
	if [info exists stunnel_local_protection_button] {
		lappend blist $stunnel_local_protection_button
	}
	if [info exists disable_ssl_workarounds_button] {
		lappend blist $disable_ssl_workarounds_button
	}
	if [info exists ultra_dsm_button] {
		lappend blist $ultra_dsm_button
	}
	if [info exists no_probe_vencrypt_button] {
		lappend blist $no_probe_vencrypt_button
	}
	if [info exists vencrypt_button] {
		lappend blist $vencrypt_button
	}
	if [info exists anondh_button] {
		lappend blist $anondh_button
	}
	foreach b $blist {
		if {$on} {
			catch {$b configure -state normal}
		} else {
			catch {$b configure -state disabled}
		}
	}

	if {$is_windows} {
		catch {$stunnel_local_protection_button configure -state disabled}
		catch {$ultra_dsm_button                configure -state disabled}
	}
}

proc adv_listen_ssh_tog {on} {
	global ssh_local_protection_button is_windows
	if [info exists ssh_local_protection_button] {
		if {$on} {
			catch {$ssh_local_protection_button configure -state normal}
		} else {
			catch {$ssh_local_protection_button configure -state disabled}
		}
	}
	if {$is_windows} {
		catch {$ssh_local_protection_button configure -state disabled}
	}
}

proc ssl_ssh_adjust {which} {
	global use_ssl use_ssh use_sshssl sshssl_sw	
	global remote_ssh_cmd_list
	global x11vnc_find_widget x11vnc_xlogin_widget uvnc_bug_widget

	if {$which == "ssl"} {
		set use_ssl 1
		set use_ssh 0
		set use_sshssl 0
		set sshssl_sw "ssl"
		catch {.f4.getcert configure -state normal}
		catch {.f4.always  configure -state normal}
		if [info exists x11vnc_find_widget] {
			catch {$x11vnc_find_widget configure -state disabled}
		}
		if [info exists x11vnc_xlogin_widget] {
			catch {$x11vnc_xlogin_widget configure -state disabled}
		}
		if [info exists uvnc_bug_widget] {
			catch {$uvnc_bug_widget configure -state normal}
		}
		adv_ssh_tog 0
		adv_listen_ssl_tog 1
		adv_listen_ssh_tog 0
	} elseif {$which == "none"} {
		set use_ssl 0
		set use_ssh 0
		set use_sshssl 0
		set sshssl_sw "none"
		catch {.f4.getcert configure -state disabled}
		catch {.f4.always  configure -state disabled}
		if [info exists x11vnc_find_widget] {
			catch {$x11vnc_find_widget configure -state disabled}
		}
		if [info exists x11vnc_xlogin_widget] {
			catch {$x11vnc_xlogin_widget configure -state disabled}
		}
		if [info exists uvnc_bug_widget] {
			catch {$uvnc_bug_widget configure -state normal}
		}
		adv_ssh_tog 0
		adv_listen_ssl_tog 0
		adv_listen_ssh_tog 0
	} elseif {$which == "ssh"} {
		set use_ssl 0
		set use_ssh 1
		set use_sshssl 0
		set sshssl_sw "ssh"
		catch {.f4.getcert configure -state disabled}
		catch {.f4.always  configure -state disabled}
		if [info exists x11vnc_find_widget] {
			catch {$x11vnc_find_widget configure -state normal}
		}
		if [info exists x11vnc_xlogin_widget] {
			catch {$x11vnc_xlogin_widget configure -state normal}
		}
		if [info exists uvnc_bug_widget] {
			catch {$uvnc_bug_widget configure -state disabled}
		}
		adv_ssh_tog 1
		adv_listen_ssl_tog 0
		adv_listen_ssh_tog 1
	} elseif {$which == "sshssl"} {
		set use_ssl 0
		set use_ssh 0
		set use_sshssl 1
		set sshssl_sw "sshssl"
		catch {.f4.getcert configure -state disabled}
		catch {.f4.always  configure -state disabled}
		if [info exists x11vnc_find_widget] {
			catch {$x11vnc_find_widget configure -state normal}
		}
		if [info exists x11vnc_xlogin_widget] {
			catch {$x11vnc_xlogin_widget configure -state normal}
		}
		if [info exists uvnc_bug_widget] {
			catch {$uvnc_bug_widget configure -state normal}
		}
		adv_ssh_tog 1
		adv_listen_ssl_tog 1
		adv_listen_ssh_tog 1
	}

	if [info exists remote_ssh_cmd_list] {
		if {$use_ssh || $use_sshssl} {
			foreach w $remote_ssh_cmd_list {
				$w configure -state normal
			}
		}
		if {$use_ssl || $sshssl_sw == "none"} {
			foreach w $remote_ssh_cmd_list {
				$w configure -state disabled
			}
		}
	}

	if {! $use_ssl && ! $use_ssh && ! $use_sshssl} {
		if {$sshssl_sw != "none"} {
			set use_ssl 1
			set sshssl_sw "ssl"
		}
	}
	global ssh_only ts_only
	if {$ssh_only || $ts_only} {
		set use_ssl 0
		set use_sshssl 0
		set use_ssh 1
		set sshssl_sw "ssh"
	}
	
	putty_pw_entry check
}

proc listen_adjust {} {
	global use_listen revs_button multiple_listen_button is_windows
	global listen_once_button listen_accept_popup_button listen_accept_popup_button_sc
	if {![info exists multiple_listen_button]} {
		set multiple_listen_button "none"
	}
	if {$use_listen} {
		catch {.b.conn configure -text "Listen"}
		catch {.o.b.connect configure -text "Listen"}
		catch {$multiple_listen_button configure -state normal}
		catch {$listen_once_button configure -state normal}
		catch {$listen_accept_popup_button configure -state normal}
		catch {$listen_accept_popup_button_sc configure -state normal}
		catch {mesg "Listen :N -> Port 5500+N, i.e. :0 -> 5500, :1 -> 5501, :2 -> 5502 ..."}
	} else {
		catch {.b.conn configure -text "Connect"}
		catch {.o.b.connect configure -text "Connect"}
		catch {$multiple_listen_button configure -state disabled}
		catch {$listen_once_button configure -state disabled}
		catch {$listen_accept_popup_button configure -state disabled}
		catch {$listen_accept_popup_button_sc configure -state disabled}
		catch {mesg "Switched to Forward Connection mode."}
	}
	if {$is_windows} {
		catch {$multiple_listen_button configure -state disabled}
		catch {$listen_once_button configure -state disabled}
		catch {$listen_accept_popup_button configure -state disabled}
		catch {$listen_accept_popup_button_sc configure -state disabled}
	}
}

proc unixpw_adjust {} {
	global is_windows use_unixpw darwin_cotvnc
	if {$is_windows || $darwin_cotvnc} {
		return;
	}
	if {$use_unixpw} {
		pack configure .fu -after .f1 -fill x
		catch {focus .fu.e}
	} else {
		pack forget .fu
	}
}

proc x11vnc_find_adjust {which} {
	global remote_ssh_cmd
	global use_x11vnc_find x11vnc_find_widget
	global use_x11vnc_xlogin x11vnc_xlogin_widget

	if {$which == "find"} {
		if {$use_x11vnc_find} {
			set use_x11vnc_xlogin 0
		}
	} elseif {$which == "xlogin"} {
		if {$use_x11vnc_xlogin} {
			set use_x11vnc_find 0
		}
	}
	if {! $use_x11vnc_find && ! $use_x11vnc_xlogin} {
		set remote_ssh_cmd "";
		return
	}
	if {![regexp {x11vnc} $remote_ssh_cmd]} {
		set remote_ssh_cmd "";
	}
	regsub {^[ 	]*PORT= [ 	]*} $remote_ssh_cmd "" remote_ssh_cmd
	regsub {^[ 	]*P= [ 	]*} $remote_ssh_cmd "" remote_ssh_cmd
	regsub {^[ 	]*sudo x11vnc[ 	]*} $remote_ssh_cmd "" remote_ssh_cmd
	regsub {^[ 	]*x11vnc[ 	]*} $remote_ssh_cmd "" remote_ssh_cmd
	regsub -all {[ 	]*-find[ 	]*} $remote_ssh_cmd " " remote_ssh_cmd
	regsub -all {[ 	]*-localhost[ 	]*} $remote_ssh_cmd " " remote_ssh_cmd
	regsub -all {[ 	]*-env FD_XDM=1[ 	]*} $remote_ssh_cmd " " remote_ssh_cmd
	if {$use_x11vnc_find} {
		set remote_ssh_cmd "PORT= x11vnc -find -localhost -nopw $remote_ssh_cmd"
	} else {
		set remote_ssh_cmd "PORT= sudo x11vnc -find -localhost -env FD_XDM=1 -nopw $remote_ssh_cmd"
	}
	regsub {[ 	]*$} $remote_ssh_cmd "" remote_ssh_cmd
	regsub {^[ 	]*} $remote_ssh_cmd "" remote_ssh_cmd
	regsub -all {[ 	][ 	]*} $remote_ssh_cmd " " remote_ssh_cmd
}

proc set_darwin_cotvnc_buttons {} {
	global darwin_cotvnc uname darwin_cotvnc_blist 
	
	if {$uname == "Darwin" && [info exists darwin_cotvnc_blist]} {
		foreach b [array names darwin_cotvnc_blist] {
			if {$darwin_cotvnc} {
				catch {$b configure -state disabled}
			} else {
				catch {$b configure -state normal}
			}
		}
	}
}

proc disable_encryption {} {
	global env
	if {[info exists env(SSVNC_DISABLE_ENCRYPTION_BUTTON)]} {
		set s $env(SSVNC_DISABLE_ENCRYPTION_BUTTON)
		if {$s != "" && $s != "0"} {
			return 1;
		}
	}
	return 0;
}
proc set_options {} {
	global use_alpha use_grab use_ssh use_sshssl use_viewonly use_fullscreen use_bgr233
	global use_nojpeg use_raise_on_beep use_compresslevel use_quality use_x11_macosx
	global use_send_clipboard use_send_always
	global compresslevel_text quality_text
	global env is_windows darwin_cotvnc uname
	global use_listen
	global use_x11vnc_find x11vnc_find_widget
	global use_x11vnc_xlogin x11vnc_xlogin_widget uvnc_bug_widget
	global ts_only
	global darwin_cotvnc_blist
	global showing_no_encryption no_enc_button no_enc_prev

	if {$ts_only} {
		set_ts_options
		return
	}

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
		"Use SSH+SSL" -command {ssl_ssh_adjust sshssl}
	set iss $i
	set no_enc_prev .o.b$i
	incr i

	radiobutton .o.b$i -anchor w -variable sshssl_sw -value none -text \
		"No Encryption" -command {ssl_ssh_adjust none}
	set no_enc_button .o.b$i
	set ine $i
	incr i

	checkbutton .o.b$i -anchor w -variable use_x11vnc_find -text \
		"Automatically Find X Session" -command {x11vnc_find_adjust "find"}
	if {!$use_ssh && !$use_sshssl} {.o.b$i configure -state disabled}
	set x11vnc_find_widget ".o.b$i"
	incr i

	if {! $is_windows} {
		checkbutton .o.b$i -anchor w -variable use_unixpw -text \
			"Unix Username & Password" -command {unixpw_adjust}
		if {$darwin_cotvnc} {.o.b$i configure -state disabled}
		set darwin_cotvnc_blist(.o.b$i) 1
		incr i
	}

	checkbutton .o.b$i -anchor w -variable use_listen -text \
		"Reverse VNC Connection (-LISTEN)" -command {listen_adjust; if {$vncdisplay == ""} {set vncdisplay ":0"} else {set vncdisplay ""}; if {0 && $use_listen} {destroy .o}}
	#if {$is_windows} {.o.b$i configure -state disabled}
	#if {$darwin_cotvnc} {.o.b$i configure -state disabled}
	#set darwin_cotvnc_blist(.o.b$i) 1
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
	set darwin_cotvnc_blist(.o.b$i) 1
	incr i

	checkbutton .o.b$i -anchor w -variable use_bgr233 -text \
		"Use 8bit color (-bgr233)"
	if {$darwin_cotvnc} {.o.b$i configure -state disabled}
	set darwin_cotvnc_blist(.o.b$i) 1
	incr i

	checkbutton .o.b$i -anchor w -variable use_nojpeg -text \
		"Do not use JPEG (-nojpeg)"
	if {$darwin_cotvnc} {.o.b$i configure -state disabled}
	set darwin_cotvnc_blist(.o.b$i) 1
	incr i

	if {$uname == "Darwin"} {
		checkbutton .o.b$i -anchor w -variable use_x11_macosx -text \
			"Use X11 vncviewer on MacOSX" \
			-command {if {$use_x11_macosx} {set darwin_cotvnc 0} else {set darwin_cotvnc 1}; set_darwin_cotvnc_buttons}
		if {$uname != "Darwin"} {.o.b$i configure -state disabled}
		incr i
	}

	if {$is_windows} {
		global kill_stunnel
		checkbutton .o.b$i -anchor w -variable kill_stunnel -text \
			"Kill Stunnel Automatically"
		incr i
	}
	

	menubutton .o.b$i -anchor w -menu .o.b$i.m -textvariable compresslevel_text -relief groove
	set compresslevel_text "Compress Level: $use_compresslevel"
	if {$darwin_cotvnc} {.o.b$i configure -state disabled}
	set darwin_cotvnc_blist(.o.b$i) 1

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

	menubutton .o.b$i -anchor w -menu .o.b$i.m -textvariable quality_text -relief groove
	set quality_text "Quality: $use_quality"
	if {$darwin_cotvnc} {.o.b$i configure -state disabled}
	set darwin_cotvnc_blist(.o.b$i) 1

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

	global use_mode ts_only ssh_only
	if {$ts_only} {
		set use_mode "Terminal Services (tsvnc)"
	} elseif {$ssh_only} {
		set use_mode "SSH-Only (sshvnc)"
	} else {
		set use_mode "SSVNC"
	}
	global mode_text
	set mode_text "Mode: $use_mode"

	menubutton .o.b$i -anchor w -menu .o.b$i.m -textvariable mode_text -relief groove

	menu .o.b$i.m -tearoff 0
	.o.b$i.m add radiobutton -variable use_mode -value "SSVNC"  \
		-label "SSVNC" -command { if {$ts_only || $ssh_only} {to_ssvnc; set mode_text "Mode: SSVNC"; destroy .o}}
	.o.b$i.m add radiobutton -variable use_mode -value "SSH-Only (sshvnc)" \
		-label "SSH-Only (sshvnc)" -command { if {$ts_only || ! $ssh_only} {to_sshonly; set mode_text "Mode: SSH-Only (sshvnc)"; destroy .o}}
	.o.b$i.m add radiobutton -variable use_mode -value "Terminal Services (tsvnc)" \
		-label "Terminal Services (tsvnc)" -command {to_tsonly; set mode_text "Mode: Terminal Services (tsvnc)"; destroy .o}
	incr i

	global started_with_noenc

	if {0 && $started_with_noenc && $showing_no_encryption} {
		;
	} elseif {$ssh_only} {
		;
	} else {
		checkbutton .o.b$i -anchor w -variable showing_no_encryption -text \
			"Show 'No Encryption' Option" -pady 5 \
			-command {toggle_no_encryption 1}
		# -relief raised
		incr i
	}

	for {set j 1} {$j < $i} {incr j} {
		global ssh_only ts_only 
		if {$ssh_only && $j <= 3} {
			continue;
		}
		if {$ts_only && $j <= 3} {
			continue;
		}
		if {!$showing_no_encryption && $j == $ine} {
			continue;
		}

		pack .o.b$j -side top -fill x
	}

	if {$is_windows} {
		global port_slot putty_pw

		frame .o.pp	
		frame .o.pp.fL	
		frame .o.pp.fR	
		label .o.pp.fL.la -anchor w -text "Putty PW:"
		label .o.pp.fL.lb -anchor w -text "Port Slot:"
		pack .o.pp.fL.la .o.pp.fL.lb -side top -fill x
		
		entry .o.pp.fR.ea -width 10 -show * -textvariable putty_pw
		entry .o.pp.fR.eb -width 10 -textvariable port_slot
		pack .o.pp.fR.ea .o.pp.fR.eb  -side top -fill x

		pack .o.pp.fL -side left
		pack .o.pp.fR -side right -expand 1 -fill x

		pack .o.pp -side top -fill x

		putty_pw_entry check
	} 

	global uname
	set t1 "             Advanced ..."
	set t2 "             Use Defaults"
	set t3 "             Delete Profile ..."
	if {$uname == "Darwin"} {
		regsub {^ *} $t1 "" t1
		regsub {^ *} $t2 "" t2
		regsub {^ *} $t3 "" t3
	}

	button .o.advanced -anchor w -text $t1 -command set_advanced_options
	button .o.clear    -anchor w -text $t2 -command {set_defaults; init_vncdisplay}
	button .o.delete   -anchor w -text $t3 -command {destroy .o; delete_profile}

	pack .o.clear -side top -fill x 
	pack .o.delete -side top -fill x 
	pack .o.advanced -side top -fill x 

#	pack .o.s_prof -side top -fill x 
#	pack .o.l_prof -side top -fill x 

	frame .o.b
	button .o.b.done -text "Done" -command {destroy .o}
	bind .o <Escape> {destroy .o}
	wm protocol .o WM_DELETE_WINDOW {destroy .o}
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

proc print_help {} {

	global help_main help_prox help_misc help_tips
	set b "\n============================================================================\n"
	help
	#set str [.h.f.t get 1.0 end]
	#puts "${b}Help:\n$str"
	puts "${b}Help Main:\n$help_main"
	puts "${b}Help Proxies:\n$help_prox"
	puts "${b}Help Misc:\n$help_misc"
	puts "${b}Help Tips:\n$help_tips"
	destroy .h

	help_opts
	set str [.oh.f.t get 1.0 end]
	puts "${b}SSL/SSH Viewer Options Help:\n$str"
	destroy .oh

	help_advanced_opts
	set str [.ah.f.t get 1.0 end]
	puts "${b}Advanced Options Help:\n$str"
	destroy .ah

	help_ssvncviewer_opts
	set str [.av.f.t get 1.0 end]
	puts "${b}ssvncviewer Options Help:\n$str"
	destroy .av

	help_certs
	set str [.ch.f.t get 1.0 end]
	puts "${b}SSL Certificates Help:\n$str"
	destroy .ch

	help_fetch_cert
	set str [.fh.f.t get 1.0 end]
	puts "${b}Fetch Certificates Help:\n$str"
	destroy .fh

	create_cert
	set str [.ccrt.f.t get 1.0 end]
	puts "${b}Create SSL Certificate Dialog:\n$str"
	destroy .ccrt

	import_cert
	set str [.icrt.f.t get 1.0 end]
	puts "${b}Import SSL Certificate Dialog:\n$str"
	destroy .icrt

	global cert_text
	set cert_text "empty"
	save_cert "help:0"
	set str [.scrt.f.t get 1.0 end]
	puts "${b}Save SSL Certificate Dialog:\n$str"
	destroy .scrt

	ts_help
	set str [.h.f.t get 1.0 end]
	puts "${b}Terminal Services Help:\n$str"
	destroy .h

	help_ts_opts	
	set str [.oh.f.t get 1.0 end]
	puts "${b}Terminal Services VNC Options Help:\n$str"
	destroy .oh

	ts_unixpw_dialog	
	set str [.uxpw.f.t get 1.0 end]
	puts "${b}Terminal Services Use unixpw Dialog:\n$str"
	destroy .uxpw

	ts_vncshared_dialog	
	set str [.vncs.f.t get 1.0 end]
	puts "${b}Terminal Services VNC Shared Dialog:\n$str"
	destroy .vncs

	ts_multi_dialog	
	set str [.mult.f.t get 1.0 end]
	puts "${b}Terminal Services Multiple Sessions Dialog:\n$str"
	destroy .mult

	ts_xlogin_dialog	
	set str [.xlog.f.t get 1.0 end]
	puts "${b}Terminal Services X Login Dialog:\n$str"
	destroy .xlog

	ts_othervnc_dialog	
	set str [.ovnc.f.t get 1.0 end]
	puts "${b}Terminal Services Other VNC Server Dialog:\n$str"
	destroy .ovnc

	ts_ncache_dialog	
	set str [.nche.f.t get 1.0 end]
	puts "${b}Terminal Services Client-Side Caching Dialog:\n$str"
	destroy .nche

	ts_x11vnc_opts_dialog	
	set str [.x11v.f.t get 1.0 end]
	puts "${b}Terminal Services x11vnc Options Dialog:\n$str"
	destroy .x11v

	ts_filexfer_dialog	
	set str [.xfer.f.t get 1.0 end]
	puts "${b}Terminal Services File Transfer Dialog:\n$str"
	destroy .xfer

	ts_sound_dialog	
	set str [.snd.f.t get 1.0 end]
	puts "${b}Terminal Services Sound Tunnelling Dialog:\n$str"
	destroy .snd

	ts_cups_dialog	
	set str [.cups.f.t get 1.0 end]
	puts "${b}Terminal Services CUPS Dialog:\n$str"
	destroy .cups

	help_ssvncviewer_opts
	set str [.av.f.t get 1.0 end]
	puts "${b}Unix SSVNC viewer Options Help:\n$str"
	destroy .av

	change_vncviewer_dialog
	set str [.chviewer.t get 1.0 end]
	puts "${b}Unix Change VNC Viewer Dialog:\n$str"
	destroy .chviewer

	cups_dialog
	set str [.cups.f.t get 1.0 end]
	puts "${b}CUPS Dialog:\n$str"
	destroy .cups

	sound_dialog
	set str [.snd.f.t get 1.0 end]
	puts "${b}ESD Audio Tunnelling Dialog:\n$str"
	destroy .snd

	smb_dialog
	set str [.smb.f.t get 1.0 end]
	puts "${b}SMB Mounting Dialog:\n$str"
	destroy .smb

	port_redir_dialog
	set str [.redirs.t get 1.0 end]
	puts "${b}Additional Port Redirections Dialog:\n$str"
	destroy .redirs

	port_knocking_dialog
	set str [.pk.f.t get 1.0 end]
	puts "${b}Port Knocking Dialog:\n$str"
	destroy .pk

	ssvnc_escape_help
	set str [.ekh.f.t get 1.0 end]
	puts "${b}SSVNC Escape Keys Help:\n$str"
	destroy .ekh

	stunnel_sec_dialog
	set str [.stlsec.f.t get 1.0 end]
	puts "${b}STUNNEL Local Port Protections Dialog:\n$str"
	destroy .stlsec

	disable_ssl_workarounds_dialog
	set str [.sslwrk.f.t get 1.0 end]
	puts "${b}Disable SSL Workarounds Dialog:\n$str"
	destroy .sslwrk

	ultra_dsm_dialog
	set str [.ultradsm.f.t get 1.0 end]
	puts "${b}UltraVNC DSM Encryption Plugin Dialog:\n$str"
	destroy .ultradsm

	ssh_known_hosts_dialog
	set str [.sshknownhosts.f.t get 1.0 end]
	puts "${b}Private SSH KnownHosts file Dialog:\n$str"
	destroy .sshknownhosts

	ssh_sec_dialog
	set str [.sshsec.t get 1.0 end]
	puts "${b}SSH Local Port Protections Dialog:\n$str"
	destroy .sshsec

	multilisten_dialog
	set str [.multil.t get 1.0 end]
	puts "${b}Multiple LISTEN Connections Dialog:\n$str"
	destroy .multil

	use_grab_dialog
	set str [.usegrb.t get 1.0 end]
	puts "${b}Use XGrabServer (for fullscreen) Dialog:\n$str"
	destroy .usegrb
}

proc zeroconf_fill {b m} {
	global is_windows zeroconf_command last_post

	if {$is_windows} {
		return;
	}

	if {![info exists last_post]} {
		set last_post 0
	}
	set now [clock seconds]
	if {$now < [expr $last_post + 10]} {
		# cache menu for 10 secs.
		return
	}

	.  config -cursor {watch}
	$b config -cursor {watch}
	$b configure -state disabled

	$m delete 0 end
	update

	set emsg ""
	set output ""
	set none "No VNC servers detected"

	set rc 1
	set rd 0
	if {$zeroconf_command == "avahi-browse"} {
		set rc [catch {set output [exec avahi-browse -r -t -p -k _rfb._tcp 2>/dev/null]} emsg]
	} elseif {$zeroconf_command == "dns-sd"} {
		set rc [catch {set output [exec /bin/sh -c {pid=$$; export pid; (sleep 1; kill $pid) & exec dns-sd -B _rfb._tcp} 2>/dev/null]} emsg]
		set rd 1
	} elseif {$zeroconf_command == "mDNS"} {
		set rc [catch {set output [exec /bin/sh -c {pid=$$; export pid; (sleep 1; kill $pid) & exec mDNS   -B _rfb._tcp} 2>/dev/null]} emsg]
		set rd 1
	}

	#puts "rc=$rc output=$output"
	if {$rd == 1 && $rc != 0} {
		if [regexp {_rfb} $emsg] {
			set rc 0
			set output $emsg
		}
	}

	set count 0

	if {$rc != 0} {
		$m add command -label $none
		incr count

	} elseif {$output == "" || [regexp {^[ \t\n]*$} $output]} {
		$m add command -label $none
		incr count

	} elseif {$zeroconf_command == "avahi-browse"} {
		set lines [split $output "\n"]
		set saw("__none__") 1
		foreach line $lines {
			set items [split $line ";"]
			if {[llength $items] != 10} {
				continue
			}
			if {[lindex $items 0] != "="} {
				continue
			}

			# =;eth0;IPv4;tmp2\0582;_rfb._tcp;local;tmp2.local;10.0.2.252;5902;
			set eth  [lindex $items 1]
			set ipv  [lindex $items 2]
			set name [lindex $items 3]
			set type [lindex $items 4]
			set loc  [lindex $items 5]
			set host [lindex $items 6]
			set ip   [lindex $items 7]
			set port [lindex $items 8]

			if {![regexp -nocase {ipv4} $ipv]} {
				continue
			}

			set name0 $name
			regsub -all {\\\\} $name "__bockslosh__" name
			regsub -all {\\\.} $name "." name

			set n 0
			while {1} {
				incr n
				if {$n > 100} {
					break
				}
				if {[regexp {\\[0-9][0-9][0-9]} $name match]} {
					#puts "match1=$match"
					regsub {\\} $match "" match
					set d $match
					regsub {^0*} $d "" d
					set c [format "%c" $d]
					if {"$c" == "&"}  {
						set c "\\$c"
					}
					regsub "\\\\$match" $name $c name
					#puts "match: $match  c='$c'\nname=$name"
				} else {
					break
				}
			}

			regsub -all {__bockslosh__} $name "\\" name

			set hp $host
			if {$port >= 5900 && $port <= 6100} {
				set d [expr $port - 5900] 
				set hp "$host:$d"
			} else {
				set hp "$host:$port"
			}
			if {![info exists saw($name)]} {
				regsub -all {[^[:alnum:],./:@%_=+-]} $hp "" hp
				$m add command -label "$name - $hp" -command "set vncdisplay \"$hp\""
				incr count
				set p $port
				if {$p <= 200} {
					set p "-$port"
				}
				regsub -all {[^[:alnum:],./:@%_=+-]} "$ip:$p" "" ipp
				$m add command -label "$name - $ipp" -command "set vncdisplay \"$ipp\""
				incr count
				set saw($name) 1
			}
		}
	} else {
		set lines [split $output "\n"]
		set saw("__none__") 1
		global dns_sd_cache last_dns_sd
		if {![info exists last_dns_sd]} {
			set last_dns_sd 0
		}
		if {[clock seconds] > [expr $last_dns_sd + 1800]} {
			catch { unset dns_sd_cache }
			set last_dns_sd [clock seconds]
		}
		foreach line $lines {
			if [regexp -nocase {^Browsing} $line] {
				continue;
			}
			if [regexp -nocase {^Timestamp} $line] {
				continue;
			}
			if [regexp -nocase {killed:} $line] {
				continue;
			}
			if {![regexp {_rfb\._tcp} $line]} {
				continue;
			}
			regsub {[ \t\n]*$} $line "" line
			regsub {^.*_rfb\._tcp[^ ]*  *} $line "" name

			if {[info exists saw($name)]} {
				continue
			}
			set saw($name) 1

			set hp "$name"
			if {[info exists dns_sd_cache($name)]} {
				set hp $dns_sd_cache($name)
			} else {
				global env
				regsub -all {"} $name "" name2
				set env(DNS_SD_LU) $name2
				set emsg ""
				if {$zeroconf_command == "dns-sd"} {
					set rc [catch {set output [exec /bin/sh -c {pid=$$; export pid; (sleep 1; kill $pid) & exec dns-sd -L "$DNS_SD_LU" _rfb._tcp .} 2>/dev/null]} emsg]
				} elseif {$zeroconf_command == "mDNS"} {
					set rc [catch {set output [exec /bin/sh -c {pid=$$; export pid; (sleep 1; kill $pid) & exec mDNS   -L "$DNS_SD_LU" _rfb._tcp .} 2>/dev/null]} emsg]
					regsub -all {[ \t][ \t]*:} $emsg ":" emsg
				}
				regsub -all {  *} $emsg " " emsg
				if [regexp -nocase {be reached at  *([^ \t\n][^ \t\n]*)} $emsg match hpm] {
					if [regexp {^(.*):([0-9][0-9]*)$} $hpm mv hm pm] {
						if {$pm >= 5900 && $pm <= 6100} {
							set pm [expr $pm - 5900] 
						}
						set hp "$hm:$pm"
					} else {
						set hp $hpm
					}
					set dns_sd_cache($name) $hp
				} else {
					set hp "$name" 
					if {![regexp {:[0-9][0-9]*$} $hp]} {
						set hp "$name:0" 
					}
				}
			}
			regsub -all {[^[:alnum:],./:@%_=+-]} $hp "" hp
			$m add command -label "$name - $hp" -command "set vncdisplay \"$hp\""
			incr count
		}
	}
	$b configure -state normal
	.  config -cursor {}
	$b config -cursor {}
	if {$count == 0}  {
		$m add command -label $none
	}
	set last_post [clock seconds]
}

proc check_zeroconf_browse {} {
	global is_windows zeroconf_command

	set zeroconf_command ""
	if {$is_windows} {
		return 0;
	}
	set p ""
	set r [catch {set p [exec /bin/sh -c {type avahi-browse}]}]
	if {$r == 0} {
		regsub {^.* is  *} $p "" p
		regsub -all {[ \t\n\r]} $p "" p
		if [file exists $p] {
			set zeroconf_command "avahi-browse"
			return 1
		}
	}
	set p ""
	set r [catch {set p [exec /bin/sh -c {type dns-sd}]}]
	if {$r == 0} {
		regsub {^.* is  *} $p "" p
		regsub -all {[ \t\n\r]} $p "" p
		if [file exists $p] {
			set zeroconf_command "dns-sd"
			global env
			if [info exists env(USE_MDNS)] {
				# testing
				set zeroconf_command "mDNS"
			}
			return 1
		}
	}
	set p ""
	set r [catch {set p [exec /bin/sh -c {type mDNS}]}]
	if {$r == 0} {
		regsub {^.* is  *} $p "" p
		regsub -all {[ \t\n\r]} $p "" p
		if [file exists $p] {
			set zeroconf_command "mDNS"
			return 1
		}
	}
	return 0
}

proc toggle_no_encryption {{rev 0}} {
	global showing_no_encryption
	global no_enc_button no_enc_prev
	global ts_only ssh_only
	global use_ssl use_ssh use_sshssl

	if {$rev} {
		# reverse it first
		if {$showing_no_encryption} {
			set showing_no_encryption 0
		} else {
			set showing_no_encryption 1
		}
	}

	if {$showing_no_encryption} {
		catch {pack forget .f4.none}
		catch {pack forget $no_enc_button}
		if {!$use_ssl && !$use_ssh && !$use_sshssl} {
			set use_ssl 1
			sync_use_ssl_ssh
		}
		set showing_no_encryption 0
	} else {
		if {$ts_only || $ssh_only} {
			return
		}
		catch {pack .f4.none -side left}
		if {![info exists no_enc_button]} {
			catch {destroy .o}
		} elseif {![winfo exists $no_enc_button]} {
			catch {destroy .o}
		} else {
			catch {pack $no_enc_button -after $no_enc_prev -fill x}
		}
		set showing_no_encryption 1
	}
}

proc toggle_vnc_prefix {} {
	global vncdisplay
	if [regexp -nocase {^vnc://} $vncdisplay] {
		regsub -nocase {^vnc://} $vncdisplay "" vncdisplay
	} else {
		regsub -nocase {^[a-z0-9+]*://} $vncdisplay "" vncdisplay
		set vncdisplay "Vnc://$vncdisplay"
	}
	catch {.f0.e icursor end}
}

############################################

global env

if {[regexp -nocase {Windows.9} $tcl_platform(os)]} {
	set is_win9x 1
} else {
	set is_win9x 0
}

set is_windows 0
if { [regexp -nocase {Windows} $tcl_platform(os)]} {
	set is_windows 1
}

set uname ""
if {! $is_windows} {
	catch {set uname [exec uname]}
}

set ffont "fixed"

global have_ipv6
set have_ipv6 ""
check_for_ipv6

# need to check if "fixed" font under XFT on tk8.5 is actually fixed width!!
if {$tcl_platform(platform) == "unix"} {
	set ls ""
	catch {set ls [font metrics $ffont -linespace]}
	set fs ""
	catch {set fs [font metrics $ffont -fixed]}
	set redo 0
	if {$fs != "" && $fs != "1"} {
		set redo 1
	}
	if {$ls != "" && $ls > 14} {
		set redo 1
	}
	if {$redo} {
		foreach fn [font names] {
			if {$fn == "TkFixedFont"} {
				set ffont $fn
				break
			}
		}
	}
	catch {option add *Dialog.msg.font {helvetica -14 bold}}
	catch {option add *Dialog.msg.wrapLength 4i}
}

if {$uname == "Darwin"} {
	set ffont "Monaco 10"

	#option add *Button.font Helvetica widgetDefault
	catch {option add *Button.font {System 10} widgetDefault}
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

global svcert_default mycert_default crlfil_default
global svcert_default_force mycert_default_force crlfil_default_force
set svcert_default ""
set mycert_default ""
set crlfil_default ""
set svcert_default_force 0
set mycert_default_force 0
set crlfil_default_force 0

set saw_ts_only 0
set saw_ssh_only 0

set ssvncrc $env(SSVNC_HOME)/.ssvncrc
if {$is_windows} {
	set ssvncrc $env(SSVNC_HOME)/ssvnc_rc
}

global ts_desktop_size_def ts_desktop_depth_def ts_desktop_type_def ts_xserver_type_def
set ts_desktop_size_def ""
set ts_desktop_depth_def ""
set ts_desktop_type_def ""
set ts_xserver_type_def ""

global win_localhost
set win_localhost "127.0.0.1"

global kill_stunnel
set kill_stunnel 1

global started_with_noenc

if {! [info exists env(SSVNC_DISABLE_ENCRYPTION_BUTTON)]} {
	set env(SSVNC_DISABLE_ENCRYPTION_BUTTON) 1
	set started_with_noenc 1
} else {
	if {$env(SSVNC_DISABLE_ENCRYPTION_BUTTON) == "0"} {
		set started_with_noenc 0
	} elseif {$env(SSVNC_DISABLE_ENCRYPTION_BUTTON) == "1"} {
		set started_with_noenc 1
	} else {
		set env(SSVNC_DISABLE_ENCRYPTION_BUTTON) 1
		set started_with_noenc 1
	}
}

if [file exists $ssvncrc] {
	set fh ""
	catch {set fh [open $ssvncrc "r"]}
	if {$fh != ""} {
		while {[gets $fh line] > -1} {
			set str [string trim $line]
			if [regexp {^#} $str] {
				continue
			}
			if [regexp {^mode=tsvnc} $str] {
				set saw_ts_only 1
				set saw_ssh_only 0
			} elseif [regexp {^mode=sshvnc} $str] {
				set saw_ts_only 0
				set saw_ssh_only 1
			} elseif [regexp {^mode=ssvnc} $str] {
				set saw_ts_only 0
				set saw_ssh_only 0
			}
			if [regexp {^desktop_type=(.*)$} $str m val] {
				set val [string trim $val]
				set ts_desktop_type_def $val
			}
			if [regexp {^desktop_size=(.*)$} $str m val] {
				set val [string trim $val]
				set ts_desktop_size_def $val
			}
			if [regexp {^desktop_depth=(.*)$} $str m val] {
				set val [string trim $val]
				set ts_desktop_depth_def $val
			}
			if [regexp {^xserver_type=(.*)$} $str m val] {
				set val [string trim $val]
				set ts_xserver_type_def $val
			}
			if [regexp {^font_default=(.*)$} $str m val] {
				set val [string trim $val]
				catch {option add *font $val}
				catch {option add *Dialog.msg.font $val}
			}
			if [regexp {^font_fixed=(.*)$} $str m val] {
				set val [string trim $val]
				set ffont $val
			}
			if [regexp {^noenc=1} $str] {
				global env
				set env(SSVNC_DISABLE_ENCRYPTION_BUTTON) 1
				set started_with_noenc 1
			}
			if [regexp {^noenc=0} $str] {
				global env
				set env(SSVNC_DISABLE_ENCRYPTION_BUTTON) 0
				set started_with_noenc 0
			}
			if [regexp {^cotvnc=1} $str] {
				global env
				set env(SSVNC_COTVNC) 1
			}
			if [regexp {^cotvnc=0} $str] {
				global env
				set env(SSVNC_COTVNC) 0
			}
			if [regexp {^killstunnel=1} $str] {
				set kill_stunnel 1
			}
			if [regexp {^killstunnel=0} $str] {
				set kill_stunnel 0
			}
			global have_ipv6
			if [regexp {^ipv6=1} $str] {
				set have_ipv6 1
				set env(SSVNC_IPV6) 1
			}
			if [regexp {^ipv6=0} $str] {
				set have_ipv6 0
				set env(SSVNC_IPV6) 0
			}
			if [regexp {^mycert=(.*)$} $str m val] {
				set val [string trim $val]
				set mycert_default $val
			}
			if [regexp {^cert=(.*)$} $str m val] {
				set val [string trim $val]
				set mycert_default $val
			}
			if [regexp {^cacert=(.*)$} $str m val] {
				set val [string trim $val]
				set svcert_default $val
			}
			if [regexp {^ca=(.*)$} $str m val] {
				set val [string trim $val]
				set svcert_default $val
			}
			if [regexp {^crl=(.*)$} $str m val] {
				set val [string trim $val]
				set crlfil_default $val
			}
			if [regexp {^env=([^=]*)=(.*)$} $str m var val] {
				global env
				set env($var) $val
			}
		}
		close $fh
	}
}

for {set i 0} {$i < $argc} {incr i} {
	set item [lindex $argv $i]
	regsub {^--} $item "-" item
	if {$item == "-profiles" || $item == "-list"} {
		set dir [get_profiles_dir]
		#puts stderr "VNC Profiles:"
		#puts stderr " "
		if {[info exists env(SSVNC_TS_ONLY)]} {
			set saw_ts_only 1
		} elseif {[info exists env(SSVNC_SSH_ONLY)]} {
			set saw_ssh_only 1
		}
		set profs [list]
		foreach prof [glob -nocomplain -directory $dir "*.vnc"] {
			set s [file tail $prof]
			regsub {\.vnc$} $s "" s
			if {$saw_ts_only || $saw_ssh_only} {
				set ok 0;
				set tsok 0;
				set fh ""
				catch {set fh [open $prof "r"]}
				if {$fh != ""} {
					while {[gets $fh line] > -1} {
						if {[regexp {use_ssh=1} $line]} {
							set ok 1
						}
						if {[regexp {ts_mode=1} $line]} {
							set tsok 1
						}
					}
					close $fh
				}
				if {$saw_ts_only && !$tsok} {
					continue;
				} elseif {! $ok} {
					continue
				}
			}
			lappend profs $s
		}
		foreach prof [lsort $profs] {
			puts "$prof"
		}
		exit
	} elseif {$item == "-nvb"} {
		global env
		set env(SSVNC_NO_VERIFY_ALL_BUTTON) 1
	} elseif {$item == "-noenc"} {
		global env
		set env(SSVNC_DISABLE_ENCRYPTION_BUTTON) 1
		set started_with_noenc 1
	} elseif {$item == "-enc"} {
		global env
		set env(SSVNC_DISABLE_ENCRYPTION_BUTTON) 0
	} elseif {$item == "-bigger"} {
		global env
		if {![info exists env(SSVNC_BIGGER_DIALOG)]} {
			set env(SSVNC_BIGGER_DIALOG) 1
		}
	} elseif {$item == "-ssh"} {
		set saw_ssh_only 1
		set saw_ts_only 0
	} elseif {$item == "-ts"} {
		set saw_ts_only 1
		set saw_ssh_only 0
	} elseif {$item == "-ssl" || $item == "-ss"} {
		set saw_ts_only 0
		set saw_ssh_only 0
	} elseif {$item == "-tso"} {
		global env
		set env(SSVNC_TS_ALWAYS) 1
		set saw_ts_only 1
	} elseif {$item == "-killstunnel"} {
		set kill_stunnel 1
	} elseif {$item == "-nokillstunnel"} {
		set kill_stunnel 0
	} elseif {$item == "-mycert" || $item == "-cert"} {
		incr i
		set mycert_default [lindex $argv $i]
	} elseif {$item == "-cacert" || $item == "-ca"} {
		incr i
		set svcert_default [lindex $argv $i]
	} elseif {$item == "-crl"} {
		incr i
		set crlfil_default [lindex $argv $i]
	}
}

if [info exists env(SSVNC_FONT_FIXED)] {
	set ffont $env(SSVNC_FONT_FIXED)
}

if [info exists env(SSVNC_FONT_DEFAULT)] {
	catch {option add *font $env(SSVNC_FONT_DEFAULT)}
	catch {option add *Dialog.msg.font $env(SSVNC_FONT_DEFAULT)}
}

if [regexp {[ 	]} $ffont] {
	set help_font "-font \"$ffont\""
} else {
	set help_font "-font $ffont"
}

if { [regexp -nocase {Windows} $tcl_platform(os)]} {
	cd util
	if {$help_font == "-font fixed"} {
		set help_font ""
	}
}

if {$saw_ts_only && $saw_ssh_only} {
	set saw_ssh_only 0
}

global ssh_only
set ssh_only 0
if {[info exists env(SSVNC_SSH_ONLY)] || $saw_ssh_only} {
	set ssh_only 1
}

global ts_only
set ts_only 0
if {[info exists env(SSVNC_TS_ONLY)] || $saw_ts_only} {
	set ts_only 1
}

if {$mycert_default != ""} {
	if [regexp -nocase {^FORCE:} $mycert_default] {
		set mycert_default_force 1
		regsub -nocase {^FORCE:} $mycert_default "" mycert_default
	}
	if {![file exists $mycert_default]} {
		set idir [get_idir_certs ""]
		set mycert_default "$idir/$mycert_default"
	}
}

if {$svcert_default != ""} {
	if [regexp -nocase {^FORCE:} $svcert_default] {
		set svcert_default_force 1
		regsub -nocase {^FORCE:} $svcert_default "" svcert_default
	}
	if {![file exists $svcert_default]} {
		set idir [get_idir_certs ""]
		if {$svcert_default == "CA"} {
			set svcert_default "$idir/CA/cacert.pem"
		} else {
			set svcert_default "$idir/$svcert_default"
		}
	}
}

if {$crlfil_default != ""} {
	if [regexp -nocase {^FORCE:} $crlfil_default] {
		set crlfil_default_force 1
		regsub -nocase {^FORCE:} $crlfil_default "" crlfil_default
	}
	if {![file exists $crlfil_default]} {
		set idir [get_idir_certs ""]
		set crlfil_default "$idir/$crlfil_default"
	}
}

if {$is_windows} {
	check_writable
}


set darwin_cotvnc 0
if {$uname == "Darwin"} {
	if {! [info exists env(DISPLAY)]} {
		set darwin_cotvnc 1
	} elseif {[regexp {/tmp/} $env(DISPLAY)]} {
		set darwin_cotvnc 1
	}
	if [info exists env(SSVNC_HOME)] {
		set t "$env(SSVNC_HOME)/.vnc"
		if {! [file exists $t]} {
			catch {file mkdir $t}
		}
	}
}

##for testing macosx
if [info exists env(FORCE_DARWIN)] {
	set uname Darwin
	set darwin_cotvnc 1
}

set putty_pw ""

global scroll_text_focus
set scroll_text_focus 1

set multientry 1

wm withdraw .
if {$ssh_only} {
	wm title . "SSH VNC Viewer"
} elseif {$ts_only} {
	wm title . "Terminal Services VNC Viewer"
} else {
	wm title . "SSL/SSH VNC Viewer"
}

wm resizable . 1 0

set_defaults
if {$uname == "Darwin"} {
	if [info exists use_x11_macosx] {
		if {$use_x11_macosx} {
			set darwin_cotvnc 0
		}
	}
}
set skip_pre 0

set vncdisplay ""
set last_load ""
set vncproxy ""
set remote_ssh_cmd ""
set vncauth_passwd ""

global did_listening_message
set did_listening_message 0

global accepted_cert_dialog_in_progress
set accepted_cert_dialog_in_progress 0

global fetch_cert_filename
set fetch_cert_filename ""

set vhd "VNC Host:Display"
if {$ssh_only} {
	label .l -text "SSH VNC Viewer" -relief ridge
} elseif {$ts_only} {
	label .l -text "Terminal Services VNC Viewer" -relief ridge
	set vhd "VNC Terminal Server:"
} else {
	label .l -text "SSL/SSH VNC Viewer" -relief ridge
}

set wl 21
set we 40
frame .f0
if {$multientry} {
	label .f0.l -width $wl -anchor w -text "$vhd" -relief ridge
} else {
	label .f0.l -anchor w -text "$vhd" -relief ridge
}
entry .f0.e -width $we -textvariable vncdisplay
pack  .f0.l -side left 
bind  .f0.e <Return> launch
bind  .f0.e <Control-E> {toggle_vnc_prefix} 
pack  .f0.e -side left -expand 1 -fill x

if {[check_zeroconf_browse]} {
	menubutton .f0.mb -relief ridge -menu .f0.mb.m -text "Find"
	menu .f0.mb.m -tearoff 0 -postcommand {zeroconf_fill .f0.mb .f0.mb.m}
	pack  .f0.mb -side left 
}

frame .f1
label .f1.l -width $wl -anchor w -text "VNC Password:" -relief ridge
entry .f1.e -width $we -textvariable vncauth_passwd -show *
pack  .f1.l -side left 
pack  .f1.e -side left -expand 1 -fill x
bind  .f1.e <Return> launch

frame .fu
label .fu.l -width $wl -anchor w -text "Unix Username:" -relief ridge
entry .fu.e -width 14 -textvariable unixpw_username
label .fu.m -anchor w -text "Unix Password:" -relief ridge
entry .fu.f -textvariable unixpw_passwd -show *
pack  .fu.l -side left 
pack  .fu.e .fu.m -side left
pack  .fu.f -side left -expand 1 -fill x
bind  .fu.f <Return> launch

frame .f2
label .f2.l -width $wl -anchor w -text "Proxy/Gateway:" -relief ridge
entry .f2.e -width $we -textvariable vncproxy
pack  .f2.l -side left 
pack  .f2.e -side left -expand 1 -fill x
bind  .f2.e <Return> launch

frame .f3
label .f3.l -width $wl -anchor w -text "Remote SSH Command:" -relief ridge
entry .f3.e -width $we -textvariable remote_ssh_cmd
pack  .f3.l -side left 
pack  .f3.e -side left -expand 1 -fill x
.f3.l configure -state disabled
.f3.e configure -state disabled
bind  .f3.e <Return> launch

set remote_ssh_cmd_list {.f3.e .f3.l} 

frame .f4
radiobutton .f4.ssl -anchor w    -variable sshssl_sw -value ssl    -command {ssl_ssh_adjust ssl}    -text "Use SSL"
radiobutton .f4.ssh -anchor w    -variable sshssl_sw -value ssh    -command {ssl_ssh_adjust ssh}    -text "Use SSH"
radiobutton .f4.sshssl -anchor w -variable sshssl_sw -value sshssl -command {ssl_ssh_adjust sshssl} -text "SSH+SSL"
pack .f4.ssl .f4.ssh .f4.sshssl -side left -fill x

set showing_no_encryption 0
radiobutton .f4.none   -anchor w -variable sshssl_sw -value none   -command {ssl_ssh_adjust none}   -text "None   "
if [disable_encryption] {
	pack .f4.none -side left
	set showing_no_encryption 1
}

global skip_verify_accepted_certs
set skip_verify_accepted_certs 0
global anon_dh_detected
set anon_dh_detected 0
global vencrypt_detected
set vencrypt_detected ""

global always_verify_ssl
set always_verify_ssl 1;
if {[info exists env(SSVNC_NO_VERIFY_ALL)]} {
	set always_verify_ssl 0;
}

if {$uname == "Darwin"} {
	button .f4.getcert -command {fetch_cert 1} -text "Fetch Cert"
} else {
	button .f4.getcert -command {fetch_cert 1} -text "Fetch Cert" -padx 3
}
checkbutton .f4.always -variable always_verify_ssl -text "Verify All Certs" -command no_certs_tutorial_mesg
pack .f4.getcert -side right -fill x
if {[info exists env(SSVNC_NO_VERIFY_ALL_BUTTON)]} {
	set always_verify_ssl 0;
} else {
	pack .f4.always -side right -fill x
}

if {$ssh_only || $ts_only} {
	ssl_ssh_adjust ssh
} else {
	ssl_ssh_adjust ssl
}

frame .b
button .b.help  -text "Help" -command help
button .b.certs -text "Certs ..." -command getcerts
button .b.opts  -text "Options ..." -command set_options
button .b.load  -text "Load" -command {load_profile}
button .b.save  -text "Save" -command {save_profile}
button .b.conn  -text "Connect" -command launch
button .b.exit  -text "Exit" -command {destroy .; exit}


if {$ssh_only || $ts_only} {
	pack          .b.opts .b.save .b.load .b.conn .b.help .b.exit -side left -expand 1 -fill x
} else {
	pack .b.certs .b.opts .b.save .b.load .b.conn .b.help .b.exit -side left -expand 1 -fill x
}

if {$multientry} {
	if {! $is_windows} {
		if {$ssh_only} {
			pack .l .f0 .f1 .f2 .f3     .b -side top -fill x
		} elseif {$ts_only} {
			pack .l .f0     .f2         .b -side top -fill x
		} else {
			pack .l .f0 .f1 .f2 .f3 .f4 .b -side top -fill x
		}
	} else {
		if {$ssh_only} {
			pack .l .f0     .f2 .f3     .b -side top -fill x
		} elseif {$ts_only} {
			pack .l .f0     .f2         .b -side top -fill x
		} else {
			pack .l .f0     .f2 .f3 .f4 .b -side top -fill x
		}
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

bind . <Control-t> {toggle_tsonly}
bind . <Control-d> {delete_profile}
bind . <Shift-B3-ButtonRelease> {toggle_tsonly}
bind . <Shift-B2-ButtonRelease> {toggle_tsonly}
bind .l <Shift-ButtonRelease> {toggle_tsonly}
bind . <Control-h> {toggle_sshonly}
bind . <Control-T> {to_ssvnc}
bind . <Control-a> {set_advanced_options}
bind . <Control-o> {set_options}
bind . <Control-u> {set_ssvncviewer_options}
bind . <Control-e> {toggle_no_encryption}

global entered_gui_top button_gui_top
set entered_gui_top 0
set button_gui_top 0
bind . <Enter> {set entered_gui_top 1}
bind .l <ButtonPress> {set button_gui_top 1}
bind .f0.l <ButtonPress> {set button_gui_top 1}

update

mac_raise

set didload 0

for {set i 0} {$i < $argc} {incr i} {
	set item [lindex $argv $i]
	regsub {^--} $item "-" item
	if {$item == "."} {
		;
	} elseif {$item == "-nv"} {
		set always_verify_ssl 0
	} elseif {$item == "-help"} {
		help
	} elseif {$item == "-ssh"} {
		;
	} elseif {$item == "-bigger"} {
		;
	} elseif {$item == "-ts"} {
		;
	} elseif {$item == "-ss"} {
		;
	} elseif {$item == "-ssl"} {
		;
	} elseif {$item == "-tso"} {
		;
	} elseif {$item == "-mycert" || $item == "-cert"} {
		incr i
	} elseif {$item == "-cacert" || $item == "-ca"} {
		incr i
	} elseif {$item == "-crl"} {
		incr i
	} elseif {$item == "-printhelp"} {
		print_help
		exit;
	} elseif {$item != ""} {
		if {[file exists $item] && [file isfile $item]}  {
			set didload 1
			load_profile . $item
		} else {
			set ok 0
			set dir [get_profiles_dir]
			set try "$dir/$item"
			foreach try [list $dir/$item $dir/$item.vnc] {
				if {[file exists $try] && [file isfile $try]} {
					load_profile . $try
					set ok 1
					break;
				}
			}
			if {! $ok && [regexp {:[0-9][0-9]*$} $item]} {
				global vncdisplay
				set vncdisplay $item
				set ok 1
			}
			
			if {! $ok} {
			    if {$ts_only || $ssh_only} {
				global vncdisplay
				set vncdisplay $item
				set ok 1
			    }
			}
			if {$ok} {
				update 
				set didload 1
				if [info exists env(SSVNC_PROFILE_LOADONLY)] {
					if {$env(SSVNC_PROFILE_LOADONLY) == "1"} {
						set ok 0
					}
				}
				if {$ok} {
					after 750
					launch
				}
			}
		}
	}
}
