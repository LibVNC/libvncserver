#!/usr/bin/perl
#
##########################################################################
# desktop.cgi:
#
# This is an example CGI script to provide multi-user web access to
# x11vnc desktops.  The user desktop sessions run in 'Xvfb' displays
# that are created automatically.
#
# This script should/must be served by an HTTPS (i.e. SSL) webserver,
# otherwise the unix and vnc passwords would be sent over the network
# unencrypted (see below to disable if you really want to.)
#
# The Java VNC Viewer applet connections are encrypted by SSL as well.
#
# You can use this script to provide unix users desktops available on
# demand via any Java enabled web browser.  One could also use this for
# a special-purpose 'single application' service running in a minimal
# window manager.
#
# One example of a special-purpose application would be a scientific
# data visualization tool running on a server where the data is housed.
# To do this set $x11vnc_extra_opts = '-env FD_PROG=/path/to/app/launcher'
# where the program launches your special purpose application.  A very
# simple example:  '-env FD_PROG=/usr/bin/xclock'
# 
#
# Depending on where you place this script, the user accesses the service
# with the URL:
#
#     https://your.webserver.net/cgi-bin/desktop.cgi
#
# Then they login with their unix username and password to get their
# own desktop session.
#
# If the user has an existing desktop it is connected to directly,
# otherwise a new session is created inside an Xvfb display and then
# connected to by VNC.
#
# It is possible to do port redirection to other machines running SSL
# enabled VNC servers (see below.)  This script does not start the VNC
# servers on the other machines, although with some extra rigging you
# should be able to do that as well.
#
# You can customize the login procedure to whatever you want by modifying
# this script, or by using ideas in this script write your own PHP,
# (for example), script.
#
##########################################################################
# Overriding default settings:
#
# If you want to override any settings in this script and do not
# want to edit this script create the assignments in a file named
# 'desktop.cgi.conf' in the same directory as desktop.cgi.  It will be
# sourced after the defaults are set.  The format of desktop.cgi.conf
# is simply perl statements that make the assignments.
#
# For example, if you put something like this in desktop.cgi.conf:
#
#	$x11vnc = '/usr/local/bin/x11vnc';
#
# that will set the path to the x11vnc binary to that location.  Look at
# the settings below for the other variables that you can modify, for
# example one could set $allowed_users_file.
#
##########################################################################
# x11vnc:
#
# You need to install x11vnc or otherwise have it available.  It is
# REQUIRED that you use x11vnc 0.9.10 or later.  It won't work with
# earlier versions.  See below the $x11vnc parameter that you can set
# to the full path to x11vnc.
#
##########################################################################
# Xvfb:
#
# Note that the x11vnc -create virtual desktop service used below requires
# that you install the 'Xvfb' program.  On debian this is currently done
# via 'apt-get install xvfb'.
#
# If you are having trouble getting 'x11vnc -create' to work with this
# script (it can be tricky), try it manually and/or see the x11vnc FAQ
# links below.
#
##########################################################################
# Apache httpd:
#
# You should put this script in, say, a cgi-bin directory.  Enable cgi
# scripts in your apache (or other httpd) config.  For example, we have
# these lines (not commented):
#
# In httpd.conf:
#
#	ScriptAlias /cgi-bin/ "/dist/apache/2.0/cgi-bin/"
#
#	<Directory "/dist/apache/2.0/cgi-bin">
#	    AllowOverride None
#	    Options None     
#	    Order allow,deny
#	    Allow from all 
#	</Directory>
#
# and in ssl.conf:
#
#	<Directory "/dist/apache/2.0/cgi-bin">
#	    SSLOptions +StdEnvVars
#	</Directory>
#
# Do not be confused by the non-standard /dist/apache/2.0 apache
# installation location that we happen to use.  Yours will be different.
#
# You can test that you have CGI scripts working properly with the 
# 'test-cgi' and 'printenv' scripts apache provides.
#
# Copy this file (desktop.cgi) to /dist/apache/2.0/cgi-bin and then run
# 'chmod 755 ...' on it to make it executable.
#
##########################################################################
# Applet Jar files served by apache:
#
# You will *also* need to copy the x11vnc classes/ssl/UltraViewerSSL.jar
# file to the httpd DocumentRoot to be accessible by: /UltraViewerSSL.jar
# in a URL (or change $applet_jar below or the html in $applet_html if
# you want to use a different location.)
#
# This location is relative to the apache DocumentRoot 'htdocs' directory.
# For our (non-standard location installation) that meant we copied the
# file to:
#
#    /dist/apache/2.0/htdocs/UltraViewerSSL.jar
#
# (your DocumentRoot directory will be different.) 
#
# The VncViewer.jar (tightvnc) will also work, but you need to change
# the $applet_jar below.  You can get these jar files from the x11vnc
# tarball from:
#
#    http://www.karlrunge.com/x11vnc/#downloading
#
# This script requires x11vnc 0.9.10 or later.
#
# Note that the usage mode for this script is a different from regular
# 'x11vnc -http ...' usage where x11vnc acts as a mini web server and
# serves its own applet jars.  We don't use that mode for this script.
# Apache (httpd) serves the jars.
#
#
##########################################################################
# Notes and Information:
#
# Each x11vnc server created for a user login will listen on its own port
# (see below for port selection schemes.)  Your firewall must let in *ALL*
# of these ports (e.g. a port range, see below for the syntax.)
#
# It is also possible, although not as reliable, to do all of this through
# a single port, see the fixed port scheme $find_free_port = 'fixed:5910'
# below.  This single port mode must be different from apache's port
# (usually 443 for https) and must also be allowed in by your firewall.
#
# Note: The fixed port scheme is DISABLED by default.
#
# It is also possible to have this script act as a vnc redirector to SSL
# enabled VNC servers running on *other* machines inside your firewall
# (presumably the users' desktops) See the $enable_port_redirection
# setting below.  The user provides 'username@host:port' instead of just
# 'username' when she logs in.  This script doesn't start VNC servers
# on those other machines, the servers must be running there already.
# (If you want this script to start them you will need to add it
# yourself.)  It is possible to provide a host:port allow list to limit
# which internal machines and ports can be redirected to.  This is the
# $port_redirection_allowed_hosts parameter.
#
# Note: The vnc redirector scheme is DISABLED by default.
#
# Note there are *two* SSL certificates involved that the user may be
# asked to inspect: apache's SSL cert and x11vnc's SSL cert.  This may
# confuse naive users.  You may want to use the same cert for both.
#
# This script provides one example on how to provide the service.  You can
# customize it to meet your needs, e.g. switch to php, newer cgi modules,
# different authentication, SQL database for user authentication, etc,
# etc.  If you plan to use it in production, please examine all security
# aspects of it carefully; read the comments in the script for more info.
#
# More information and background and troubleshooting:
#
#      http://www.karlrunge.com/x11vnc/faq.html#faq-xvfb
#      http://www.karlrunge.com/x11vnc/faq.html#faq-ssl-tunnel-viewers
#      http://www.karlrunge.com/x11vnc/faq.html#faq-ssl-java-viewer-proxy
#      http://www.karlrunge.com/x11vnc/faq.html#faq-ssl-portal
#      http://www.karlrunge.com/x11vnc/faq.html#faq-unix-passwords
#      http://www.karlrunge.com/x11vnc/faq.html#faq-userlogin
#
#
# Please also read the comments below for changing specific settings.
# You can modify them in this script or by override file 'desktop.cgi.conf'


#-------------------------------------------------------------------------
# Copyright (c) 2010 by Karl J. Runge <runge@karlrunge.com>
#
# desktop.cgi is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
# 
# desktop.cgi is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with desktop.cgi; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA
# or see <http://www.gnu.org/licenses/>.
#-------------------------------------------------------------------------

use strict;
use IO::Socket::INET;

# Test for INET6 support:
#
my $have_inet6 = 0;
eval "use IO::Socket::INET6;";
$have_inet6 = 1 if $@ eq "";

##########################################################################
# Path to the x11vnc program:
#
my $x11vnc = '/usr/bin/x11vnc';


##########################################################################
# You can set some extra x11vnc cmdline options here:
#
my $x11vnc_extra_opts = '';


##########################################################################
# Override the default x11vnc viewer connection timeout of 75 seconds:
#
my $x11vnc_timeout = '';


##########################################################################
# TCP Ports:
#
# Set find_free_port to 1 (or the other modes described below) to
# autoselect a free port to use.  The default is to use a port based on
# the userid number (7000 + uid).
#
my $find_free_port = 0;

# Or specify a port range:
#
#$find_free_port = '7000-8000';
#
# Or indicate to use a kludge to try to do everything through a SINGLE
# port.  To try to avoid contention on the port, simultaneous instances
# of this script attempt to 'take turns' using it the single port.
#
#$find_free_port = 'fixed:5910';

# This is the starting port for 7000 + uid and also $find_free_port = 1
# autoselection:
#
my $starting_port = 7000;

# Listen on AF_INET6 if IO::Socket::INET6 is available.
#
my $listen_on_ipv6 = 0;


##########################################################################
# Port redirection mode:
#
# This is to enable port redirection mode: username@host:port. If
# username is valid, there will be a port redirection to internal machine
# host:port.  Presumably there is already an SSL enabled and password
# protected VNC server running there.  We don't start that VNC server.
# (You might be able to figure out a way to do this yourself.)
#
# See the next setting for an allowed hosts file.  The default for port
# redirection is off.
#
my $enable_port_redirection = 0;

# A file with allowed port redirections.  The empty string '' (the
# default) means all host:port redirections would be allowed.
#
# Format of the file: A list of 'user@host:port' or 'host:port'
# entries, one per line.  Port ranges, e.g. host:n-m are also accepted.
#
# Leading and trailing whitespace is trimmed off each line.  Blank lines
# and comment lines starting with '#' are skipped.  A line consisting of
# 'ALL' matches everything.  If no match can be found or the file cannot
# be opened the connection is dropped.
#
my $port_redirection_allowed_hosts = '';


##########################################################################
# Allowed users:
#
# To limit which users can use this service, set the following to a file
# that contains the allowed user names one per line.  Lines starting with
# the '#' character are skipped.
#
my $allowed_users_file = '';


##########################################################################
# Denied users:
#
# As with $allowed_users_file, but to deny certain users.  Applied after
# any $allowed_users_file check and overrides the result.
#
my $denied_users_file = '';


##########################################################################
# trustUrlVncCert applet parameter:
#
# Set to 0 to have the java applet html set the parameter
# trustUrlVncCert=no, i.e. the applet will not automatically accept
# an SSL cert already accepted by an HTTPS URL.  See $applet_html and
# print_applet_html() below for more info.
#
my $trustUrlVncCert = 1;


##########################################################################
# One-time VNC password fifo:
#
# For extra security against local untrusted users a fifo is used
# to copy the one-time VNC password to the user's VNC password file
# ~user/x11vnc.pw.  If that fifo transfer technique causes problems,
# you can set this value to 1 to disable the security feature:
#
my $disable_vnc_passwd_fifo_safety = 0;


##########################################################################
# Comment this out if you don't want PATH modified:
#
$ENV{PATH} = "/usr/bin:/bin:$ENV{PATH}";


##########################################################################
# For the next two settings, note that most users will be confused that
# geometry and session are ignored when they are returning to their
# existing desktop session (x11vnc FINDDISPLAY action.)


##########################################################################
# Used below if user did not specify preferred geometry and color depth:
#
my $default_geometry = '1024x768x24';


# Set this to the list of x11vnc -create sessions types to show a session
# dropdown for the user to select from.
#
my $session_types = '';
#
# example:
#$session_types = 'gnome kde xfce lxde wmaker enlightenment mwm twm failsafe'; 


##########################################################################
# Set this to 1 to enable user setting a unique tag for each one
# of his desktops and so can have multiple ones simultaneously and
# select which one he wants.  For now we just hack this onto geometry
# 1024x768x24:my_2nd_desktop but ultimately there should be a form entry
# for it.  Search for enable_unique_tags for more info:
#
my $enable_unique_tags = 0;
my $unique_tag = '';


##########################################################################
# String of HTML for the login form:
#
# Feel free to customize to your taste, _USERNAME_ and _GEOMETRY_ are
# expanded to that of the request.
#
my $login_str = <<"END";
<title>x11vnc web access</title>
<h3>x11vnc web access</h3>
<form action="$ENV{REQUEST_URI}" method="post">
 <table border="0">
  <tr><td colspan=2><h2>Login</h2></td></tr>
  <tr><td>Username:</td><td>
  <input type="text" name="username" maxlength="40" value="_USERNAME_">
  </td></tr>
  <tr><td>Password:</td><td>
  <input type="password" name="password" maxlength="50">
  </td></tr>
  <tr><td>Geometry:</td><td>
  <input type="text" name="geometry" maxlength="40" value="_GEOMETRY_">
  </td></tr>
  <!-- session -->
  <tr><td colspan="2" align="right">
  <input type="submit" name="submit" value="Login">
  </td></tr>
 </table>
</form>
END


##########################################################################
# String of HTML returned to web browser to launch applet:
#
# Feel free to customize to your taste, _UID_, _VNC_PORT_, _WIDTH_,
# _HEIGHT_, _PASS_, _TRUST_UVC_, _APPLET_JAR_, and _APPLET_CLASS_ are
# expanded to the appropriate values before sending out to the browser.
#
my $applet_html = <<"END";
<html>
<TITLE>
x11vnc desktop (_UID_/_VNC_PORT_)
</TITLE>
<APPLET CODE=_APPLET_CLASS_ ARCHIVE=_APPLET_JAR_ WIDTH=_WIDTH_ HEIGHT=_HEIGHT_>
<param name=PORT value=_VNC_PORT_>
<param name=VNCSERVERPORT value=_VNC_PORT_>
<param name=PASSWORD value=_PASS_>
<param name=trustUrlVncCert value=_TRUST_UVC_>
<param name="Open New Window" value=yes>
<param name="Offer Relogin" value=no>
<param name="ignoreMSLogonCheck" value=yes>
<param name="delayAuthPanel" value=yes>
<!-- extra -->
</APPLET>
<br>
<a href="$ENV{REQUEST_URI}">Login page</a><br>
<a href=http://www.karlrunge.com/x11vnc>x11vnc website</a>
</html>
END


##########################################################################
# These java applet strings are expanded into the above $applet_html.
# Note that $applet_jar is relative to your apache DocumentRoot (htdocs)
# not the filesystem root. 
#
my $applet_jar = '/UltraViewerSSL.jar';
my $applet_class = 'VncViewer.class';

# These make the applet panel smaller because we use 'Open New Window'
# anyway (set to 'W' or 'H' to use actual session geometry values):
#
my $applet_width  = '400';
my $applet_height = '300';

# To customize ALL of the HTML printed out you may need to redefine
# the bye() subtroutine in your desktop.cgi.conf file. 


##########################################################################
# Override any of the above settings by setting them in a file named
# 'desktop.cgi.conf'.  It is sourced here.
#
# You can override any variable set above by supplying perl code
# in $0.conf that sets it to the desired value.
#
# Some examples you could put in $0.conf:
#
#   $x11vnc = '/usr/local/bin/x11vnc';
#   $x11vnc_extra_opts = '-env FD_PROG=/usr/bin/xclock';
#   $x11vnc_extra_opts = '-ssl /usr/local/etc/dtcgi.pem';
#   $find_free_port = 'fixed:5999';
#   $enable_port_redirection = 1;
#   $allowed_users_file = '/usr/local/etc/dtcgi.allowed';
#
if (-f "$0.conf") {
	eval `cat "$0.conf"`;
}


##########################################################################
# END OF MAIN USER SETTINGS.
# Only power users should change anything below.
##########################################################################

# Print http header reply:
#
print STDOUT "Content-Type: text/html\r\n\r\n";


# Require HTTPS so that unix and vnc passwords are not sent in clear text
# (perhaps it is too late...)  Disable HTTPS here at your own risk.
#
if ($ENV{HTTPS} !~ /^on$/i) {
	bye("HTTPS must be used (to encrypt passwords)");
}


# Read URL request:
#
my $request;
if ($ENV{'REQUEST_METHOD'} eq "POST") {
	read(STDIN, $request, $ENV{'CONTENT_LENGTH'});
} elsif ($ENV{'REQUEST_METHOD'} eq "GET" ) {
	$request = $ENV{'QUERY_STRING'};
} else {
	$request = $ARGV[0];
}

my %request = url_decode(split(/[&=]/, $request));


# Experiment for FD_TAG x11vnc feature for multiple desktops for a
# single user:
#
# we hide it in geometry:tag for now:
#
if ($enable_unique_tags && $request{geometry} =~ /^(.*):(\w+)$/) {
	$request{geometry} = $1;
	$unique_tag = $2;
}

# Check/set geometry and session:
#
if (!exists $request{geometry} || $request{geometry} !~ /^[x\d]+$/) {
	# default geometry and depth:
	$request{geometry} = $default_geometry;
}
if (!exists $request{session} || $request{session} =~ /^\s*$/) {
	$request{session} = '';
}


# Expand _USERNAME_ and _GEOMETRY_ in the login string HTML:
#
$login_str =~ s/_USERNAME_/$request{username}/g;
$login_str =~ s/_GEOMETRY_/$request{geometry}/g;


# Check x11vnc version for installers of this script who do not know
# how to read and follow instructions:
#
my $version = (split(' ', `$x11vnc -version`))[1];
$version =~ s/\D*$//;

my ($major, $minor, $micro) = split(/\./, $version);
if ($major !~ /^\d+$/ || $minor !~ /^\d+$/) {
	bye("The x11vnc program is not installed correctly.");
}
$micro = 0 unless $micro;
my $level  = $major * 100 * 100 + $minor * 100 + $micro;
my $needed =      0 * 100 * 100 +      9 * 100 +     10;
if ($level < $needed) {
	bye("x11vnc version 0.9.10 or later is required. (Found version $version)");
}


# Set up user selected desktop session list, if enabled:
#
my %sessions;

if ($session_types ne '') {
	my $str = "<tr><td>Session:</td><td>\n<select name=session>";
	$str .= "<option value=none>select</option>";

	foreach my $sess (split(' ', $session_types)) {
		next if $sess =~ /^\s*$/;
		next if $sess !~ /^\w+$/; # alphanumeric
		$sessions{$sess} = 1;
		$str .= "<option value=$sess>$sess</option>";
	}
	$str .= "</select>\n</td></tr>";

	# This forces $request{session} to be a valid one:
	#
	if (! exists $sessions{$request{session}}) {
		$request{session} = 'none';
	}

	# Insert into login_str:
	#
	my $r = $request{session};
	$str =~ s/option value=\Q$r\E/option selected value=$r/;
	$login_str =~ s/<!-- session -->/$str/;
}


# If no username or password, show login form:
#
if (!$request{username} && !$request{password}) {
	bye($login_str);
} elsif (!$request{username}) {
	bye("No Username.<p>$login_str");
} elsif (!$request{password}) {
	bye("No Password.<p>$login_str");
}


# Some shorthand names:
#
my $username = $request{username};
my $password = $request{password};
my $geometry = $request{geometry};
my $session  = $request{session};


# If port redirection is enabled, split username@host:port
#
my $redirect_host = '';
my $current_fh1 = '';
my $current_fh2 = '';

if ($enable_port_redirection) {
	($username, $redirect_host) = split(/@/, $username, 2);
	if ($redirect_host ne '') {
		# will exit if the redirection is not allowed:
		check_redirect_host();
	}
}

# If there is an $allowed_users_file, check username against it:
#
if ($allowed_users_file ne '') {
	if (! open(USERS, "<$allowed_users_file")) {
		bye("Internal Error #0");
	}
	my $ok = 0;
	while (<USERS>) {
		chomp;
		$_ =~ s/^\s*//;
		$_ =~ s/\s*$//;
		next if /^#/;
		if ($username eq $_) {
			$ok = 1;
		}
	}
	close USERS;
	if (! $ok) {
		bye("Denied Username.<p>$login_str");
	}
}

# If there is a $denied_users_file, check username against it:
#
if ($denied_users_file ne '') {
	if (! open(USERS, "<$denied_users_file")) {
		bye("Internal Error #0");
	}
	my $ok = 1;
	while (<USERS>) {
		chomp;
		$_ =~ s/^\s*//;
		$_ =~ s/\s*$//;
		next if /^#/;
		if ($username eq $_) {
			$ok = 0;
		}
	}
	close USERS;
	if (! $ok) {
		bye("Denied Username.<p>$login_str");
	}
}

# Require username to be alphanumeric + '-' + '_':
# (one may want to add '.' as well)
#
if ($username !~ /^\w[-\w]*$/) {
	bye("Invalid Username.<p>$login_str");
}


# Get the userid number, we may use it as his VNC display port; this
# also checks if the username exists:
#
my $uid = `/usr/bin/id -u '$username'`;
chomp $uid;
if ($? != 0 || $uid !~ /^\d+$/) {
	bye("Invalid Username.<p>$login_str");
}


# Use x11vnc trick to check if the unix password is valid:
# (requires x11vnc 0.9.10 or later.)
#
if (!open(X11VNC, "| $x11vnc -unixpw \%stdin > /dev/null")) {
	bye("Internal Error #1");
}
print X11VNC "$username:$password\n";

if (!close X11VNC) {
	# x11vnc returns non-zero for invalid username+password:
	bye("Invalid Password.<p>$login_str");
}


# Initialize random number generator for use below:
#
initialize_random();


# Set vnc port:
#
my $vnc_port = 0;
my $fixed_port = 0;

if (! $find_free_port) {
	# Fixed port based on userid (we assume it is free):
	#
	$vnc_port = $starting_port + $uid;

} elsif ($find_free_port =~ /^fixed:(\d+)$/) {
	#
	# Enable the -loopbg method that tries to share a single port:
	#
	$vnc_port = $1;
	$fixed_port = 1;
} else {
	# Autoselect a port, either default range (7000-8000) or a user
	# supplied range.  (note that $find_free_port will now contain
	# a socket listening on the found port so that it is held.)
	#
	$vnc_port = auto_select_port();
}

# Check for crazy port value:
#
if ($vnc_port > 64000 || $vnc_port < 1) {
	bye("Internal Error #2 $vnc_port");
}


# If port redirection is enabled and the user selected it via
# username@host:port, we do that right now and then exit.
#
if ($enable_port_redirection && $redirect_host ne '') {
	port_redir();
	exit 0;
}


# Make a random, onetime vnc password:
#
my $pass = '';
my $chars = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
my @abc = split(//, $chars);

for (my $i = 0; $i < 8; $i++) {
	$pass .= $abc[ rand(scalar(@abc)) ];
}

# Use x11vnc trick to switch to user and store vnc pass in the passwdfile.
# Result is $pass is placed in user's $HOME/x11vnc.pw
#
# (This is actually difficult to do without untrusted LOCAL users being
# able to see the pass as well, see copy_password_to_user() for details
# on how we try to avoid this.)
#
copy_password_to_user($pass);


# Make a tmp file for x11vnc launcher script:
#
my $tmpfile = `/bin/mktemp /tmp/desktop.cgi.XXXXXX`;
chomp $tmpfile;

# Check if the tmpfile is valid:
#
if (! -e $tmpfile || ! -o $tmpfile || -l $tmpfile) {
	unlink $tmpfile;
	bye("Internal Error #3");
}
if (!chmod 0644, $tmpfile) {
	unlink $tmpfile;
	bye("Internal Error #4");
}
if (!open(TMP, ">$tmpfile")) {
	unlink $tmpfile;
	bye("Internal Error #5");
}


# The x11vnc command.  You adjust it to suit your needs.
#
# some ideas:  -env FD_PROG=/usr/bin/gnome-session 
#              -env FD_SESS=kde
#              -env FD_TAG=my_2nd_desktop
#              -ultrafilexfer
#
# Note that -timeout will cause it to exit if client does not connect
# and -sslonly disables VeNCrypt SSL connections.

# Some settings:
# (change these if you encounter timing problems, etc.)
#
my $timeout = 75;
my $extra = '';
if ($fixed_port) {
	# settings for fixed port case:
	$timeout = 45;
	$extra .= " -loopbg100,1";
}
$timeout = $x11vnc_timeout if $x11vnc_timeout ne '';

if ($session_types ne '') {
	# settings for session selection case:
	if (exists $sessions{$session}) {
		$extra .= " -env FD_SESS='$session'";
	}
}
if ($enable_unique_tags && $unique_tag ne '' && $unique_tag =~ /^\w+$/) {
	$extra .= " -env FD_TAG='$unique_tag'";
}

# This md5sum check of the vnc passwd is for extra safety (see
# copy_password_to_user for details.)
#
my $md5sum = '';
system("type md5sum > /dev/null");
if ($? == 0) {
	my $md5 = `/bin/mktemp /tmp/desktop.cgi.XXXXXX`;
	chomp $md5;
	# compute md5sum of password:
	if (-o $md5 && open(MD5, "| md5sum > $md5")) {
		print MD5 "$pass\n";
		close MD5;
		if (open(MD5, "<$md5")) {
			# read it:
			my $line = <MD5>;
			close MD5;
			my ($s, $t) = split(' ', $line);
			if (length($s) >= 32 && $s =~ /^\w+$/) {
				# shell code for user to check he has correct passwd:
				$md5sum = "if md5sum \$HOME/x11vnc.pw | grep '$s' > /dev/null; then true; else exit 1; fi";
			}
		}
	}
	unlink $md5;
}

# Write x11vnc command to the tmp file:
#
print TMP <<"END";
#!/bin/sh
export PATH=/usr/bin:/bin:\$PATH
$md5sum
$x11vnc -sigpipe ignore:HUP -nopw -rfbport $vnc_port \\
    -passwdfile \$HOME/x11vnc.pw -oa \$HOME/x11vnc.log \\
    -create -ssl SAVE -sslonly -env FD_GEOM=$geometry \\
    -timeout $timeout $extra $x11vnc_extra_opts \\
        >/dev/null 2>/dev/null </dev/null &
sleep 2
exit 0
END

close TMP;

# Now launch x11vnc to switch to user and run the wrapper script:
# (this requires x11vnc 0.9.10 or later.)
#
$ENV{UNIXPW_CMD} = "/bin/sh $tmpfile";

# For the fixed port scheme we try to cooperate via lock file:
# (disabled by default.)
#
my $rmlock = '';
#
if ($fixed_port) {
	# try to grab the fixed port for the next 90 secs removing stale
	# locks older than 60 secs:
	#
	$rmlock = lock_fixed_port(90, 60);
}

# Start the x11vnc cmd:
#
if (!open(X11VNC, "| $x11vnc -unixpw \%stdin > /dev/null")) {
	unlink $tmpfile;
	unlink $rmlock if $rmlock;
	bye("Internal Error #6");
}

select(X11VNC); $| = 1; select(STDOUT);

# Close any port we held.  There is still a gap of time between now
# and when when x11vnc in $tmpfile reopens the port after the password
# authentication.  So another instance of this script could accidentally
# think it is free...
# 
sleep 1;
close $find_free_port if $find_free_port;

print X11VNC "$username:$password\n";
close X11VNC;	# note we ignore return value.
unlink $tmpfile;

if ($rmlock) {
	# let our x11vnc proceed a bit before removing lock.
	sleep 2;
	unlink $rmlock;
}

# Return html for the java applet to connect to x11vnc.
#
print_applet_html();

exit 0;

#################################################################
# Subroutines:

# print the message to client and exit with success.
#
sub bye {
	my $msg = shift;
	print STDOUT "<html>$msg</html>\n";
	exit 0;
}

# decode %xx to character:
#
sub url_decode {
	foreach (@_) {
		tr/+/ /;
		s/%(..)/pack("c",hex($1))/ge;
	}
	@_;
}

# seed random
#
sub initialize_random {
	my $rbytes = '';
	if (open(RAN, "</dev/urandom")) {
		read(RAN, $rbytes, 8);
	} elsif (open(RAN, "</dev/random")) {
		read(RAN, $rbytes, 8);
	} else {
		$rbytes = sprintf("%08d", $$);
	}
	close RAN;

	# set seed:
	#
	my $seed = join('', unpack("C8", $rbytes));
	$seed = substr($seed, -9);
	srand($seed);

	for (my $i = 0; $i < ($$ % 4096); $i++) {
		# Mix it up even a little bit more.  There should be
		# over 1 billion possible vnc passwords now.
		rand();
	}
}

# Autoselect a port for vnc.  Note that a socket for the found port
# is kept open (and stored in $find_free_port) until we call x11vnc at
# the end.
#
sub auto_select_port {
	my $pmin = $starting_port;	# default range 7000-8000.
	my $pmax = $starting_port + 1000;

	if ($find_free_port =~ /^(\d+)-(\d+)$/) {
		# user supplied a range:
		$pmin = $1;
		$pmax = $2;
		if ($pmin > $pmax) {
			($pmin, $pmax) = ($pmax, $pmin);
		}
	} elsif ($find_free_port > 1024) {
		# user supplied a starting port:
		$pmin = $find_free_port;
		$pmax = $pmin + 1000;
	}

	# Try to add a bit of randomness to the starting port so
	# simultaneous instances of this script won't be fooled by the gap
	# of time before x11vnc reopens the port (see near the bottom.)
	#
	my $dp = int(rand(1.0) * 0.25 * ($pmax - $pmin));
	if ($pmin + $dp < $pmax - 20) {
		$pmin = $pmin + $dp;
	}

	my $port = 0;

	# Now try to find a free one:
	#
	for (my $p = $pmin; $p <= $pmax; $p++) {
		my $sock = '';
		if ($have_inet6 && $listen_on_ipv6) {
			eval {$sock = IO::Socket::INET6->new(
				Listen    => 1,
				LocalPort => $p,
				ReuseAddr => 1,
				Domain    => AF_INET6,
				LocalAddr => "::",
				Proto     => "tcp"
			);};
		} else {
			$sock = IO::Socket::INET->new(
				Listen	  => 1,
				LocalPort => $p,
				ReuseAddr => 1,
				Proto     => "tcp"
			);
		}
		if ($sock) {
			# we will keep this open until we call x11vnc:
			$find_free_port = $sock;
			$port = $p;
			last;
		}
	}
	return $port;
}

# Since apache typically runs as user 'apache', 'nobody', etc, and not
# as root it is tricky for us to copy the pass string to a file owned by
# the user without some other untrusted local user being able to learn
# the password (e.g. via reading a file or watching ps.)  Note that with
# the x11vnc -unixpw trick we unfortunately can't use a pipe because
# the user command is run in its own tty.
#
# The best way would be a sudo action or a special setuid program for
# copying.  So consider doing that and thereby simplify this function.
#
# Short of a special program doing this, we use a fifo so ONLY ONE
# process can read the password.  If the untrusted local user reads it,
# then the logging-in user's x11vnc won't get it.  The login and x11vnc
# will fail, but the untrusted user won't gain access to the logging-in
# user's desktop.
#
# So here we start long, tedious work carefully managing the fifo.
#
sub copy_password_to_user {

	my $pass = shift;
	
	my $use_fifo = '';

	# Find a command to make a fifo:
	#
	system("type mkfifo > /dev/null");
	if ($? == 0) {
		$use_fifo = 'mkfifo %s';
	} else {
		system("type mknod > /dev/null");
		if ($? == 0) {
			$use_fifo = 'mknod %s p';
		}
	}

	# Create the filename for our fifo:
	#
	my $fifo = `/bin/mktemp /tmp/desktop.cgi.XXXXXX`;
	chomp $fifo;

	if (! -e $fifo || ! -o $fifo || -l $fifo) {
		unlink $fifo;
		bye("Internal Error #7");
	}

	# disable fifo safety if requested:
	#
	if ($disable_vnc_passwd_fifo_safety) {
		$use_fifo = '';
	}

	# Make the fifo:
	#
	if ($use_fifo) {
		$use_fifo = sprintf($use_fifo, $fifo);

		# there is a small race here:
		system("umask 077; rm -f $fifo; $use_fifo; chmod 600 $fifo");

		if (!chmod 0600, $fifo) {
			# we chmod once more..
			unlink $fifo;
			bye("Internal Error #8");
		}

		if (! -o $fifo || ! -p $fifo || -l $fifo)  {
			# but we get out if not owned by us anymore:
			unlink $fifo;
			bye("Internal Error #9");
		}
	}

	# Build cmd for user to read our fifo:
	#
	my $upw = '$HOME/x11vnc.pw';
	$ENV{UNIXPW_CMD} = "touch $upw; chmod 600 $upw; cat $fifo > $upw";

	# Start it:
	#
	if (!open(X11VNC, "| $x11vnc -unixpw \%stdin > /dev/null")) {
		unlink $fifo;
		bye("Internal Error #10");
	}
	select(X11VNC); $| = 1; select(STDOUT);

	if (! $use_fifo) {
		# regular file, we need to write it now.
		if (!open(FIFO, ">$fifo")) {
			close X11VNC;
			unlink $fifo;
			bye("Internal Error #11");
		}
		print FIFO "$pass\n";
		close FIFO;
	}

	# open fifo up for reading.
	# (this means the bad guy can read it too.)
	#
	if (!chmod 0644, $fifo) {
		unlink $fifo;
		bye("Internal Error #12");
	}

	# send the user's passwd now:
	#
	print X11VNC "$username:$password\n";

	if ($use_fifo) {
		# wait a bit for the cat $fifo to start, reader will block.
		sleep 1;
		if (!open(FIFO, ">$fifo")) {
			close X11VNC;
			unlink $fifo;
			bye("Internal Error #13");
		}
		# here it goes:
		print FIFO "$pass\n";
		close FIFO;
	}
	close X11VNC;	# note we ignore return value.
	fsleep(0.5);
	unlink $fifo;

	# Done!
}

# For fixed, single port mode.  Try to open and lock the port before
# proceeding.
#
sub lock_fixed_port {
	my ($t_max, $t_age) = @_;

	# lock file name:
	#
	my $lock = '/tmp/desktop.cgi.lock';
	my $remove = '';

	my $t = 0;
	my $sock = '';

	while ($t < $t_max) {
		if (-e $lock) {
			# clean out stale locks if possible:
			if (! -l $lock) {
				unlink $lock;
			} else {
				my ($pid, $time) = split(/:/, readlink($lock));
				if (! -d "/proc/$pid") {
					unlink $lock;
				}
				if (time() > $time + $t_age) {
					unlink $lock;
				}
			}
		}

		my $reason = '';

		if (-l $lock) {
			# someone has locked it.
			$reason = 'locked';
		} else {
			# unlocked, try to listen on port:
			if ($have_inet6 && $listen_on_ipv6) {
				eval {$sock = IO::Socket::INET6->new(
					Listen    => 1,
					LocalPort => $vnc_port,
					ReuseAddr => 1,
					Domain    => AF_INET6,
					LocalAddr => "::",
					Proto     => "tcp"
				);};
			} else {
				$sock = IO::Socket::INET->new(
					Listen	  => 1,
					LocalPort => $vnc_port,
					ReuseAddr => 1,
					Proto     => "tcp"
				);
			}
			if ($sock) {
				# we got it, now try to lock:
				my $str = "$$:" . time();
				if (symlink($str, $lock)) {
					$remove = $lock;
					$find_free_port = $sock;
					last;
				}
				# wow, we didn't lock it...
				$reason = "symlink failed: $!";
				close $sock;
			} else {
				$reason = "listen failed: $!";
			}
		}
		# sleep a bit and then try again:
		#
		print STDERR "$$ failed to get fixed port $vnc_port for $username at $t ($reason)\n";
		$sock = '';
		$t += 5;
		sleep 5;
	}
	if (! $sock) {
		bye("Failed to lock fixed TCP port. Try again a bit later.<p>$login_str");
	}
	print STDERR "$$ got fixed port $vnc_port for $username at $t\n";

	# Return the file to remove, if any:
	#
	return $remove;
}


# Return html for the java applet to connect to x11vnc.
#
# N.B. Please examine the applet params, e.g. trustUrlVncCert=yes to
# see if you agree with them.  See x11vnc classes/ssl/README for all
# parameters.
#
# Note how we do not take extreme care to authenticate the server to
# the client applet (but note that trustUrlVncCert=yes is better than
# trustAllVncCerts=yes)  One can tighten all of this up at the expense
# of extra certificate dialogs (assuming the user bothers to check...)
#
# This assumes /UltraViewerSSL.jar is at document root; you need to put
# it there.
#
sub print_applet_html {
	my ($W, $H, $D) = split(/x/, $geometry);

	# make it smaller since we 'Open New Window' below anyway.
	if ($applet_width ne 'W') {
		$W = $applet_width;
	}
	if ($applet_height ne 'H') {
		$H = $applet_height;
	}

	my $tUVC = ($trustUrlVncCert ? 'yes' : 'no');

	# see $applet_html set in defaults section for more info:
	#
	my $str = $applet_html;

	$str =~ s/_UID_/$uid/g;
	$str =~ s/_VNC_PORT_/$vnc_port/g;
	$str =~ s/_WIDTH_/$W/g;
	$str =~ s/_HEIGHT_/$H/g;
	$str =~ s/_PASS_/$pass/g;
	$str =~ s/_APPLET_JAR_/$applet_jar/g;
	$str =~ s/_APPLET_CLASS_/$applet_class/g;
	$str =~ s/_TRUST_UVC_/$tUVC/g;

	if ($enable_port_redirection && $redirect_host ne '') {
		$str =~ s/name=PASSWORD value=.*>/name=NOT_USED value=yes>/i;
		#$str =~ s/<!-- extra -->/<!-- extra -->\n<param name="ignoreProxy" value=yes>/;
	}

	print $str;
}

##########################################################################
# The following subroutines are for port redirection only, which is
# disabled by default ($enable_port_redirection == 0)
#
sub port_redir {
	# To aid in avoiding zombies:
	#
	setpgrp(0, 0);

	# For the fixed port scheme we try to cooperate via lock file:
	#
	my $rmlock = '';
	#
	if ($fixed_port) {
		# try to grab the fixed port for the next 90 secs removing
		# stale locks older than 60 secs:
		#
		$rmlock = lock_fixed_port(90, 60);

	} elsif ($find_free_port eq '0') {
		if ($have_inet6 && $listen_on_ipv6) {
			eval {$find_free_port = IO::Socket::INET6->new(
				Listen    => 1,
				LocalPort => $vnc_port,
				ReuseAddr => 1,
				Domain    => AF_INET6,
				LocalAddr => "::",
				Proto     => "tcp"
			);};
		} else {
			$find_free_port = IO::Socket::INET->new(
				Listen	  => 1,
				LocalPort => $vnc_port,
				ReuseAddr => 1,
				Proto     => "tcp"
			);
		}
	}
	# In all cases, at this point $find_free_port is the listening
	# socket.

	# fork a helper process to do the port redir:
	#
	# Actually we need to spawn 4(!) of them in case the proxy check
	# /check.https.proxy.connection (it is by default) and the other
	# test connections.  Spawn one for each expected connection, for
	# whatever applet parameter usage mode you set up.
	#
	for (my $n = 1; $n <= 4; $n++) {
		my $pid = fork();
		if (! defined $pid) {
			bye("Internal Error #14");
		} elsif ($pid) {
			wait;
		} else {
			if (fork) {
				exit 0;
			}
			setpgrp(0, 0);
			handle_conn();
			exit 0;
		}
	}

	# We now close the listening socket:
	#
	close $find_free_port;
	
	if ($rmlock) {
		# let our process proceed a bit before removing lock.
		sleep 1;
		unlink $rmlock;
	}

	# Now send html to the browser so it can connect:
	#
	print_applet_html();

	exit 0;
}

# This checks the validity of a username@host:port for the port
# redirection mode.  Finishes and exits if it is invalid.
#
sub check_redirect_host {
	# First check that the host:port string is valid:
	#
	if ($redirect_host !~ /^\w[-\w\.]*:\d+$/) {
		bye("Invalid Redirect Host:Port.<p>$login_str");
	}
	# Second, check if the allowed host file permits it:
	#
	if ($port_redirection_allowed_hosts ne '') {
		if (! open(ALLOWED, "<$port_redirection_allowed_hosts")) {
			bye("Internal Error #15");
		}
		my $ok = 0;
		while (my $line = <ALLOWED>) {
			chomp $line;
			# skip blank lines and '#' comments:
			next if $line =~ /^\s*$/;
			next if $line =~ /^\s*#/;

			# trim spaces from ends:
			$line =~ s/^\s*//;
			$line =~ s/\s*$//;

			# collect host:ports in case port range given:
			my @items;
			if ($line =~ /^(.*):(\d+)-(\d+)$/) {
				# port range:
				my $host = $1;
				my $pmin = $2;
				my $pmax = $3;
				for (my $p = $pmin; $p <= $pmax; $p++) {
					push @items, "$host:$p";
				}
			} else {
				push @items, $line;
			}

			# now check each item for a match:
			foreach my $item (@items) {
				if ($item eq 'ALL') {
					$ok = 1;
					last;
				} elsif ($item =~ /@/) {
					if ("$username\@$redirect_host" eq $item) {
						$ok = 1;
						last;
					}
				} elsif ($redirect_host eq $item) {
					$ok = 1;
					last;
				}
			}
			# got a match:
			last if $ok;
		}
		close ALLOWED;

		if (! $ok) {
			bye("Disallowed Redirect Host:Port.<p>$login_str");
		}
	}
}

# Much of this code is borrowed from 'connect_switch':
#
# (it only applies to the vnc redirector $enable_port_redirection mode
# which is off by default.)
#
sub handle_conn {
	close STDIN;
	close STDOUT;
	close STDERR;

	$SIG{ALRM} = sub {close $find_free_port; exit 0};

	# We only wait 30 secs for the redir case, esp. since
	# we need to spawn so many helpers...
	#
	alarm(30);

	my ($client, $ip) = $find_free_port->accept();

	alarm(0);

	close $find_free_port;

	if (!$client) {
		exit 1;
	}

	my $host = '';
	my $port = '';
	if ($redirect_host =~ /^(.*):(\d+)$/) {
		($host, $port) = ($1, $2);
	}

	my $sock = IO::Socket::INET->new(
		PeerAddr => $host,
		PeerPort => $port,
		Proto => "tcp"
	);
	if (! $sock && $have_inet6) {
		eval {$sock = IO::Socket::INET6->new(
			PeerAddr => $host,
			PeerPort => $port,
			Proto => "tcp"
		);};
	}

	if (! $sock) {
		close $client;
		exit 1;
	}

	$current_fh1 = $client;
	$current_fh2 = $sock;

	$SIG{TERM} = sub {close $current_fh1; close $current_fh2; exit 0};

	my $killpid = 1;

	my $parent = $$;
	if (my $child = fork()) {
		xfer($sock, $client, 'S->C');
		if ($killpid) {
			fsleep(0.5);
			kill 'TERM', $child;
		}
	} else {
		xfer($client, $sock, 'C->S');
		if ($killpid) {
			fsleep(0.75);
			kill 'TERM', $parent;
		}
	}
	exit 0;
}

# This does socket data transfer in one direction.
#
sub xfer {
	my($in, $out, $lab) = @_;
	my ($RIN, $WIN, $EIN, $ROUT);
	$RIN = $WIN = $EIN = "";
	$ROUT = "";
	vec($RIN, fileno($in), 1) = 1;
	vec($WIN, fileno($in), 1) = 1;
	$EIN = $RIN | $WIN;
	my $buf;

	while (1) {
		my $nf = 0;
		while (! $nf) {
			$nf = select($ROUT=$RIN, undef, undef, undef);
		}
		my $len = sysread($in, $buf, 8192);
		if (! defined($len)) {
			next if $! =~ /^Interrupted/;
			last;
		} elsif ($len == 0) {
			last;
		}

		my $offset = 0;
		my $quit = 0;
		while ($len) {
			my $written = syswrite($out, $buf, $len, $offset);
			if (! defined $written) {
				$quit = 1;
				last;
			}
			$len -= $written;
			$offset += $written;
		}
		last if $quit;
	}
	close($in);
	close($out);
}

# Sleep a small amount of time (float)
#
sub fsleep {
	my ($time) = @_;
	select(undef, undef, undef, $time) if $time;
}
