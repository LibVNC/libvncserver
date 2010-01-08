  #!/bin/sh -- # A comment mentioning perl
eval 'exec perl -S $0 ${1+"$@"}'
        if 0;
#
# Here is the remote x11vnc command.
# Modify to your needs, required to have %DISP item that expands to X display
# and the -bg option to go into the background.
#
$x11vnc_cmd = "x11vnc -localhost -nap -q -bg -display %DISP";

#
# We will redir local ports to these remote ports hoping the remote
# x11vnc selects one of them:
#
@tunnel_ports = qw(5900 5901 5902 5903 5904);

#
# We need to specify the encoding preferences since vncviewer will
# mistakeningly prefer "raw" encoding for local connection.  required to
# have %VNC_ITEM to expand to localhost:<port>

# One really needs an -encodings option otherwise the vncviewer will
# prefer 'raw' which is very slow.
#
$viewer_cmd = "vncviewer -encodings 'copyrect tight zrle hextile zlib corre rre' %VNC_DISP";
$sleep_time = 15;

if ($ENV{USER} eq 'runge') {
	# my personal kludges:
	$viewer_cmd =~ s/vncviewer/vncviewerz/;	# for tight
	$x11vnc_cmd .= ' -rfbauth .vnc/passwd';	# I always want rfbauth
}

chop($Program = `basename $0`);

$Usage = <<"END";

$Program: wrapper to tunnel vncviewer <-> x11vnc VNC traffic through a ssh
	encrypted tunnel port redirection.

Usage: $Program <options> <remote-Xdisplay>

Options:
	-l <user>			ssh login as remote user <user>

	-rfbauth <remote-auth-file>	this option is passed to the remote
					x11vnc command for passwd file.

Notes:

Example: $Program snoopy:0

END

LOOP:	
while (@ARGV) {
    $_ = shift;
    CASE: {
	/^-display$/ && ($remote_xdisplay = shift, last CASE);
	/^-rfbauth$/ && ($x11vnc_cmd .= ' -rfbauth ' . shift, last CASE);
	/^-l$/ && ($remote_user = ' -l ' . shift, last CASE);
	/^--$/ && (last LOOP);	# -- means end of switches
	/^-(-.*)$/ && (unshift(@ARGV, $1), last CASE);
	/^(-h|-help)$/ && ((print STDOUT $Usage), exit 0, last CASE);
	if ( /^-(..+)$/ ) {	# split bundled switches:
		local($y, $x) = ($1, '');
		(unshift(@ARGV, $y), last CASE) if $y =~ /^-/;
		foreach $x (reverse(split(//, $y))) { unshift(@ARGV,"-$x") };
		last CASE;
	}
	/^-/ && ((print STDERR "Invalid arg: $_\n$Usage"), exit 1, last CASE);
	unshift(@ARGV,$_);
	last LOOP;
    }
}

select(STDERR); $| = 1;
select(STDOUT); $| = 1;

# Determine the remote X display to connect to:
$remote_xdisplay = shift if $remote_xdisplay eq '';
if ($remote_xdisplay !~ /:/) {
	$remote_xdisplay .= ':0';	# assume they mean :0 over there.
}
if ($remote_xdisplay =~ /:/) {
	$host = $`;
	$disp = ':' . $';
} else {
	die "bad X display: $remote_xdisplay, must be <host>:<display>\n";
}

#
# Get list of local ports in use so we can avoid them: 
# (tested on Linux and Solaris)
#
open(NETSTAT, "netstat -an|") || die "netstat -an: $!";
while (<NETSTAT>) {
	chomp ($line = $_);
	next unless $line =~ /(ESTABLISHED|LISTEN|WAIT2?)\s*$/;
	$line =~ s/^\s*//;
	$line =~ s/^tcp[\s\d]*//;
	$line =~ s/\s.*$//;
	$line =~ s/^.*\D//;
	if ($line !~ /^\d+$/) {
		die "bad netstat line: $line from $_"; 
	}
	$used_port{$line} = 1;
}
close(NETSTAT);

#
# Now match up free local ports with the desired remote ports
# (note that the remote ones could be in use but that won't stop
# the ssh with port redirs from succeeding)
#
$lport = 5900;
$cnt = 0;
foreach $rport (@tunnel_ports) {
	while ($used_port{$lport}) {
		$lport++;
		$cnt++;
		die "too hard to find local ports 5900-$lport" if $cnt > 200;
	}
	$port_map{$rport} = $lport;
	$lport++;
}

$redir = '';
foreach $rport (@tunnel_ports) {
	$redir .= " -L $port_map{$rport}:localhost:$rport";
}

#
# Have ssh put the command in the bg, then we look for PORT= in the
# tmp file.  The sleep at the end is to give us enough time to connect
# thru the port redir, otherwise ssh will exit before we can connect.
#

# This is the x11vnc cmd for the remote side:
$cmd = $x11vnc_cmd;
$cmd =~ s/%DISP/$disp/;

# This is the ssh cmd for the local side (this machine):
$ssh_cmd = "ssh -t -f $remote_user $redir $host '$cmd; echo END; sleep $sleep_time'";
$ssh_cmd =~ s/  / /g;
print STDERR "running ssh command:\n\n$ssh_cmd\n\n";

#
# Run ssh and redir into a tmp file (assumes ssh will use /dev/tty
# for password/passphrase dialog)
#
$tmp = "/tmp/rx.$$";
system("$ssh_cmd > $tmp");

# Now watch for the PORT=XXXX message:
$sleep = 0;
$rport = '';
print STDERR "\nWaiting for x11vnc to indicate its port ..";
while ($sleep < $sleep_time + 10) {
	print STDERR ".";
	sleep(1);
	$sleep++;
	if (`cat $tmp` =~ /PORT=(\d+)/) {
		$rport = $1;
		# wait 1 more second for output:
		sleep(1);
		if (`cat $tmp` =~ /PORT=(\d+)/) {
			$rport = $1;
		}
		last;
	}
}
print STDERR "\n";

if (! $rport) {
	print STDERR `cat $tmp`;
	unlink($tmp);
	die "could not determine remote port.\n";
}
unlink($tmp);

# Find the remote to local mapping:
$lport = $port_map{$rport};
print STDERR "remote port is: $rport (corresponds to port $lport here)\n";
if (! $lport) {
	die "could not determine local port redir.\n";
}

# Apply the special casing vncviewer does for 5900 <= port < 6000
if ($lport < 6000 && $lport >= 5900) {
	$lport = $lport - 5900;
}

# Finally, run the viewer.
$cmd = $viewer_cmd;
$cmd =~ s/%VNC_DISP/localhost:$lport/;

print STDERR "running vncviewer command:\n\n$cmd\n\n";
system($cmd);
