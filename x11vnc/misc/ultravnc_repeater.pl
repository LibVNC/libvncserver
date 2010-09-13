#!/usr/bin/env perl
#
# Copyright (c) 2009-2010 by Karl J. Runge <runge@karlrunge.com>
#
# ultravnc_repeater.pl is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
# 
# ultravnc_repeater.pl is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with ultravnc_repeater.pl; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA
# or see <http://www.gnu.org/licenses/>.
# 

my $usage = '
ultravnc_repeater.pl:
          perl script implementing the ultravnc repeater
          proxy protocol.

protocol: Listen on one port for vnc clients (default 5900.)
          Listen on one port for vnc servers (default 5500.)
          Read 250 bytes from connecting vnc client or server.
          Accept ID:<string> from clients and servers, connect them
          together once both are present.

          The string "RFB 000.000\n" is sent to the client (the client
          must understand this means send ID:... or host:port.)
          Also accept <host>:<port> from clients and make the
          connection to the vnc server immediately. 

          Note there is no authentication or security WRT ID names or
          identities; it is up to the client and server to completely
          manage that aspect and whether to encrypt the session, etc.

usage:  ultravnc_repeater.pl [-r] [client_port [server_port]]

Use -r to refuse new server/client connections when there is an existing
server/client ID.  The default is to close the previous one.

To write to a log file set the env. var ULTRAVNC_REPEATER_LOGFILE.

To run in a loop restarting the server if it exits set the env. var.
ULTRAVNC_REPEATER_LOOP=1 or ULTRAVNC_REPEATER_LOOP=BG, the latter
forks into the background.  Set ULTRAVNC_REPEATER_PIDFILE to a file
to store the master pid in.

Set ULTRAVNC_REPEATER_NO_RFB=1 to disable sending "RFB 000.000" to
the client.  Then this program acts as general TCP rendezvous tool.

Examples:

	ultravnc_repeater.pl
	ultravnc_repeater.pl -r
	ultravnc_repeater.pl 5901
	ultravnc_repeater.pl 5901 5501

	env ULTRAVNC_REPEATER_LOOP=BG ULTRAVNC_REPEATER_LOGFILE=/tmp/u.log ultravnc_repeater.pl ...

';

use strict;

# Set up logging:
#
if (exists $ENV{ULTRAVNC_REPEATER_LOGFILE}) {
	close STDOUT;
	if (!open(STDOUT, ">>$ENV{ULTRAVNC_REPEATER_LOGFILE}")) {
	        die "ultravnc_repeater.pl: $ENV{ULTRAVNC_REPEATER_LOGFILE} $!\n";
	}
	close STDERR;
	open(STDERR, ">&STDOUT");
}
select(STDERR); $| = 1;
select(STDOUT); $| = 1;

# interrupt handler:
#
my $looppid = '';
my $pidfile = '';
#
sub get_out {
	lprint("$_[0]:\t$$ looppid=$looppid");
	if ($looppid) {
		kill 'TERM', $looppid;
		fsleep(0.2);
	}
	unlink $pidfile if $pidfile;
	cleanup();
	exit 0;
}

sub lprint {
	print STDERR scalar(localtime), ": ", @_, "\n";
}

# These are overridden in actual server thread:
#
$SIG{INT}  = \&get_out;
$SIG{TERM} = \&get_out;

# pidfile:
#
sub open_pidfile {
	if (exists $ENV{ULTRAVNC_REPEATER_PIDFILE}) {
		my $pf = $ENV{ULTRAVNC_REPEATER_PIDFILE};
		if (open(PID, ">$pf")) {
			print PID "$$\n";
			close PID;
			$pidfile = $pf;
		} else {
			lprint("could not open pidfile: $pf - $! - continuing...");
		}
		delete $ENV{ULTRAVNC_REPEATER_PIDFILE};
	}
}

####################################################################
# Set ULTRAVNC_REPEATER_LOOP=1 to have this script create an outer loop
# restarting itself if it ever exits.  Set ULTRAVNC_REPEATER_LOOP=BG to
# do this in the background as a daemon.

if (exists $ENV{ULTRAVNC_REPEATER_LOOP}) {
	my $csl = $ENV{ULTRAVNC_REPEATER_LOOP};
	if ($csl ne 'BG' && $csl ne '1') {
		die "ultravnc_repeater.pl: invalid ULTRAVNC_REPEATER_LOOP.\n";
	}
	if ($csl eq 'BG') {
		# go into bg as "daemon":
		setpgrp(0, 0);
		my $pid = fork();
		if (! defined $pid) {
			die "ultravnc_repeater.pl: $!\n";
		} elsif ($pid) {
			wait;
			exit 0;
		}
		if (fork) {
			exit 0;
		}
		setpgrp(0, 0);
		close STDIN;
		if (! $ENV{ULTRAVNC_REPEATER_LOGFILE}) {
			close STDOUT;
			close STDERR;
		}
	}
	delete $ENV{ULTRAVNC_REPEATER_LOOP};

	if (exists $ENV{ULTRAVNC_REPEATER_PIDFILE}) {
		open_pidfile();
	}

	lprint("ultravnc_repeater.pl: starting service. master-pid=$$");
	while (1) {
		$looppid = fork;
		if (! defined $looppid) {
			sleep 10;
		} elsif ($looppid) {
			wait;
		} else {
			exec $0, @ARGV;	
			exit 1;
		}
		lprint("ultravnc_repeater.pl: re-starting service.  master-pid=$$");
		sleep 1;
	}
	exit 0;
}
if (exists $ENV{ULTRAVNC_REPEATER_PIDFILE}) {
	open_pidfile();
}

# End of background/daemon stuff.
####################################################################

use warnings;
use IO::Socket::INET;
use IO::Select;

# Test for INET6 support:
#
my $have_inet6 = 0;
eval "use IO::Socket::INET6;";
$have_inet6 = 1 if $@ eq "";
print "perl module IO::Socket::INET6 not available: no IPv6 support.\n" if ! $have_inet6;

my $prog = 'ultravnc_repeater';
my %ID;

my $refuse = 0;
my $init_timeout = 5;

if (@ARGV && $ARGV[0] =~ /-h/) {
	print $usage;
	exit 0;
}
if (@ARGV && $ARGV[0] eq '-r') {
	$refuse = 1;
	lprint("enabling refuse mode (-r).");
	shift;
}

my $client_port = shift;
my $server_port = shift;

$client_port = 5900 unless $client_port;
$server_port = 5500 unless $server_port;

my $uname = `uname`;

my $repeater_bufsize = 250;
$repeater_bufsize = $ENV{BUFSIZE} if exists $ENV{BUFSIZE};

my ($RIN, $WIN, $EIN, $ROUT);

my $client_listen = IO::Socket::INET->new(
	Listen    => 10,
	LocalPort => $client_port, 
	ReuseAddr => 1,
	Proto => "tcp"
);
my $err1 = $!;
my $err2 = '';
$client_listen = '' if ! $client_listen;

my $client_listen6 = '';
if ($have_inet6) {
	eval {$client_listen6 = IO::Socket::INET6->new(
		Listen    => 10,
		LocalPort => $client_port,
		ReuseAddr => 1,
		Domain    => AF_INET6,
		LocalAddr => "::",
		Proto     => "tcp"
	);};
	$err2 = $!;
}
if (! $client_listen && ! $client_listen6) {
	cleanup();
	die "$prog: error: client listen on port $client_port: $err1 - $err2\n";
}

my $server_listen = IO::Socket::INET->new(
	Listen    => 10,
	LocalPort => $server_port, 
	ReuseAddr => 1,
	Proto => "tcp"
);
$err1 = $!;
$err2 = '';
$server_listen = '' if ! $server_listen;

my $server_listen6 = '';
if ($have_inet6) {
	eval {$server_listen6 = IO::Socket::INET6->new(
		Listen    => 10,
		LocalPort => $server_port,
		ReuseAddr => 1,
		Domain    => AF_INET6,
		LocalAddr => "::",
		Proto     => "tcp"
	);};
	$err2 = $!;
}
if (! $server_listen && ! $server_listen6) {
	cleanup();
	die "$prog: error: server listen on port $server_port: $err1 - $err2\n";
}

my $select = new IO::Select();
if (! $select) {
	cleanup();
	die "$prog: select $!\n";
}

$select->add($client_listen)  if $client_listen;
$select->add($client_listen6) if $client_listen6;
$select->add($server_listen)  if $server_listen;
$select->add($server_listen6) if $server_listen6;

$SIG{INT}  = sub {cleanup(); exit;};
$SIG{TERM} = sub {cleanup(); exit;};

my $SOCK1 = '';
my $SOCK2 = '';
my $CURR = '';

lprint("$prog: starting up.  pid: $$");
lprint("watching for IPv4 connections on $client_port/client.") if $client_listen;
lprint("watching for IPv4 connections on $server_port/server.") if $server_listen;
lprint("watching for IPv6 connections on $client_port/client.") if $client_listen6;
lprint("watching for IPv6 connections on $server_port/server.") if $server_listen6;

my $alarm_sock = '';
my $got_alarm = 0;
sub alarm_handler {
	lprint("$prog: got sig alarm.");
	if ($alarm_sock ne '') {
		close $alarm_sock;
	}
	$alarm_sock = '';
	$got_alarm = 1;
}

while (my @ready = $select->can_read()) {
	foreach my $fh (@ready) {
		if (($client_listen && $fh == $client_listen) || ($client_listen6 && $fh == $client_listen6)) {
			lprint("new vnc client connecting."); 
		} elsif (($server_listen && $fh == $server_listen) || ($server_listen6 && $fh == $server_listen6)) {
			lprint("new vnc server connecting."); 
		}
		my $sock = $fh->accept();
		if (! $sock) {
			lprint("$prog: accept $!");
			next;
		}

		if (($client_listen && $fh == $client_listen) || ($client_listen6 && $fh == $client_listen6)) {
			if (exists $ENV{ULTRAVNC_REPEATER_NO_RFB} && $ENV{ULTRAVNC_REPEATER_NO_RFB}) {
				lprint("ULTRAVNC_REPEATER_NO_RFB: not sending RFB 000.000"); 
			} else {
				my $str = "RFB 000.000\n";
				my $len = length $str;
				my $n = syswrite($sock, $str, $len, 0);
				if ($n != $len) {
					lprint("$prog: bad $str write: $n != $len $!");
					close $sock;
				}
			}
		}

		my $buf = '';
		my $size = $repeater_bufsize;
		$size = 1024 unless $size;

		$SIG{ALRM} = "alarm_handler";
		$alarm_sock = $sock;
		$got_alarm = 0;
		alarm($init_timeout);
		my $n = sysread($sock, $buf, $size);
		alarm(0);

		if ($got_alarm) {
			lprint("$prog: read timed out: $!");
		} elsif (! defined $n) {
			lprint("$prog: read error: $!");
		} elsif ($repeater_bufsize > 0 && $n != $size) {
			lprint("$prog: short read $n != $size $!");
			close $sock;
		} elsif (($client_listen && $fh == $client_listen) || ($client_listen6 && $fh == $client_listen6)) {
			do_new_client($sock, $buf);
		} elsif (($server_listen && $fh == $server_listen) || ($server_listen6 && $fh == $server_listen6)) {
			do_new_server($sock, $buf);
		}
	}
}

sub do_new_client {
	my ($sock, $buf) = @_;

	if ($buf =~ /^ID:(\w+)/) {
		my $id = $1;
		if (exists $ID{$id} && exists $ID{$id}{client} && $ID{$id}{client} eq "0") {
			if (!established($ID{$id}{sock})) {
				lprint("server socket for ID:$id is no longer established, closing it.");
				close $ID{$id}{sock};
				delete $ID{$id};
			} else {
				lprint("server socket for ID:$id is still established.");
			}
		}
		if (exists $ID{$id}) {
			if ($ID{$id}{client}) {
				my $ref = $refuse;
				if ($ref && !established($ID{$id}{sock})) {
					lprint("socket for ID:$id is no longer established, closing it.");
					$ref = 0;
				}
				if ($ref) {
					lprint("refusing extra vnc client for ID:$id.");
					close $sock;
					return;
				} else {
					lprint("closing and deleting previous vnc client with ID:$id.");
					close $ID{$id}{sock};

					lprint("storing new vnc client with ID:$id.");
					$ID{$id}{client} = 1;
					$ID{$id}{sock} = $sock;
				}
			} else {
				lprint("hooking up new vnc client with existing vnc server for ID:$id.");
				my $sock2 = $ID{$id}{sock};
				delete $ID{$id};
				hookup($sock, $sock2, "ID:$id"); 
			}
		} else {
			lprint("storing new vnc client with ID:$id.");
			$ID{$id}{client} = 1;
			$ID{$id}{sock} = $sock;
		}
	} else {
		my $str = sprintf("%s", $buf);
		$str =~ s/\s*$//g;
		$str =~ s/\0*$//g;
		my $host = '';
		my $port = '';
		if ($str =~ /^(.+):(\d+)$/) {
			$host = $1;
			$port = $2;
		} else {
			$host = $str;
			$port = 5900;
		}
		if ($port < 0) {
			my $pnew = -$port;
			lprint("resetting port from $port to $pnew.");
			$port = $pnew;
		} elsif ($port < 200) {
			my $pnew = $port + 5900;
			lprint("resetting port from $port to $pnew.");
			$port = $pnew;
		}
		lprint("making vnc client connection directly to vnc server host='$host' port='$port'.");
		my $sock2 =  IO::Socket::INET->new(
			PeerAddr => $host,
			PeerPort => $port,
			Proto => "tcp"
		);
		if (! $sock2 && $have_inet6) {
			lprint("IPv4 connect error: $!, trying IPv6 ...");
			eval{$sock2 = IO::Socket::INET6->new(
				PeerAddr => $host,
				PeerPort => $port,
				Proto => "tcp"
			);};
			lprint("IPv6 connect error: $!") if !$sock2;
		} else {
			lprint("IPv4 connect error: $!") if !$sock2;
		}
		if (!$sock2) {
			lprint("failed to connect to $host:$port.");
			close $sock;
			return;
		}
		hookup($sock, $sock2, "$host:$port"); 
	}
}

sub do_new_server {
	my ($sock, $buf) = @_;

	if ($buf =~ /^ID:(\w+)/) {
		my $id = $1;
		my $store = 1;
		if (exists $ID{$id} && exists $ID{$id}{client} && $ID{$id}{client} eq "1") {
			if (!established($ID{$id}{sock})) {
				lprint("client socket for ID:$id is no longer established, closing it.");
				close $ID{$id}{sock};
				delete $ID{$id};
			} else {
				lprint("client socket for ID:$id is still established.");
			}
		}
		if (exists $ID{$id}) {
			if (! $ID{$id}{client}) {
				my $ref = $refuse;
				if ($ref && !established($ID{$id}{sock})) {
					lprint("socket for ID:$id is no longer established, closing it.");
					$ref = 0;
				}
				if ($ref) {
					lprint("refusing extra vnc server for ID:$id.");
					close $sock;
					return;
				} else {
					lprint("closing and deleting previous vnc server with ID:$id.");
					close $ID{$id}{sock};

					lprint("storing new vnc server with ID:$id.");
					$ID{$id}{client} = 0;
					$ID{$id}{sock} = $sock;
				}
			} else {
				lprint("hooking up new vnc server with existing vnc client for ID:$id.");
				my $sock2 = $ID{$id}{sock};
				delete $ID{$id};
				hookup($sock, $sock2, "ID:$id"); 
			}
		} else {
			lprint("storing new vnc server with ID:$id.");
			$ID{$id}{client} = 0;
			$ID{$id}{sock} = $sock;
		}
	} else {
		lprint("invalid ID:NNNNN string for vnc server: $buf");
		close $sock;
		return;
	}
}

sub established {
	my $fh = shift;

	return established_linux_proc($fh);

	# not working:
	my $est = 1;
	my $str = "Z";
	my $res;
	#$res = recv($fh, $str, 1, MSG_PEEK | MSG_DONTWAIT);
	if (defined($res)) {
		lprint("established OK:  $! '$str'.");
		$est = 1;
	} else {
		# would check for EAGAIN here to decide ...
		lprint("established err: $! '$str'.");
		$est = 1;
	}
	return $est;
}


sub established_linux_proc {
	# hack for Linux to see if remote side has gone away:
	my $fh = shift;

	# if we can't figure things out, we must return true.
	if ($uname !~ /Linux/) {
		return 1;
	}

	my @proc_net_tcp = ();
	if (-e "/proc/net/tcp") {
		push @proc_net_tcp, "/proc/net/tcp";
	}
	if (-e "/proc/net/tcp6") {
		push @proc_net_tcp, "/proc/net/tcp6";
	}
	if (! @proc_net_tcp) {
		return 1;
	}

	my $n = fileno($fh);
	if (!defined($n)) {
		return 1;
	}

	my $proc_fd = "/proc/$$/fd/$n";
	if (! -e $proc_fd) {
		return 1;
	}

	my $val = readlink($proc_fd);
	if (! defined $val || $val !~ /socket:\[(\d+)\]/) {
		return 1;
	}
	my $num = $1;

	my $st = '';

	foreach my $tcp (@proc_net_tcp) {
		if (! open(TCP, "<$tcp")) {
			next;
		}
		while (<TCP>) {
			next if /^\s*[A-z]/;
			chomp;
			#  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode                                                     
			# 170: 0102000A:170C FE02000A:87FA 01 00000000:00000000 00:00000000 00000000  1001        0 423294766 1 f6fa4100 21 4 4 2 -1
			# 172: 0102000A:170C FE02000A:87FA 08 00000000:00000001 00:00000000 00000000  1001        0 423294766 1 f6fa4100 21 4 4 2 -1
			my @items = split(' ', $_);
			my $state = $items[3];
			my $inode = $items[9];
			if (!defined $state || $state !~ /^\d+$/) {
				next;
			}
			if (!defined $inode || $inode !~ /^\d+$/) {
				next;
			}
			if ($inode == $num) {
				$st = $state;
				last;
			}
		}
		close TCP;
		last if $st ne '';
	}

	if ($st ne '' && $st != 1) {
		return 0;
	}
	return 1;
}

sub handler {
	lprint("\[$$/$CURR] got SIGTERM.");
	close $SOCK1 if $SOCK1;
	close $SOCK2 if $SOCK2;
	exit;
}

sub hookup {
	my ($sock1, $sock2, $tag) = @_;

	my $worker = fork();

	if (! defined $worker) {
		lprint("failed to fork worker: $!");
		close $sock1;
		close $sock2;
		return;
	} elsif ($worker) {
		close $sock1;
		close $sock2;
		wait;
	} else {
		cleanup();
		if (fork) {
			exit 0;
		}
		setpgrp(0, 0);
		$SOCK1 = $sock1;
		$SOCK2 = $sock2;
		$CURR  = $tag;
		$SIG{TERM} = "handler";
		$SIG{INT}  = "handler";
		xfer_both($sock1, $sock2);
		exit 0;
	}
}

sub xfer {
	my ($in, $out) = @_;

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
			lprint("\[$$/$CURR] $!");
			last;
		} elsif ($len == 0) {
			lprint("\[$$/$CURR] Input is EOF.");
			last;
		}
		my $offset = 0;
		my $quit = 0;
		while ($len) {
			my $written = syswrite($out, $buf, $len, $offset);
			if (! defined $written) {
				lprint("\[$$/$CURR] Output is EOF. $!");
				$quit = 1;
				last;
			}
			$len -= $written;
			$offset += $written;
		}
		last if $quit;
	}
	close($out);
	close($in);
	lprint("\[$$/$CURR] finished xfer.");
}

sub xfer_both {
	my ($sock1, $sock2) = @_;

	my $parent = $$;

	my $child = fork();

	if (! defined $child) {
		lprint("$prog\[$$/$CURR] failed to fork: $!");
		return;
	}

	$SIG{TERM} = "handler";
	$SIG{INT}  = "handler";

	if ($child) {
		lprint("[$$/$CURR] parent 1 -> 2.");
		xfer($sock1, $sock2);
		select(undef, undef, undef, 0.25);
		if (kill 0, $child) {
			select(undef, undef, undef, 0.9);
			if (kill 0, $child) {
				lprint("\[$$/$CURR] kill TERM child $child");
				kill "TERM", $child;
			} else {
				lprint("\[$$/$CURR] child  $child gone.");
			}
		}
	} else {
		select(undef, undef, undef, 0.05);
		lprint("[$$/$CURR] child  2 -> 1.");
		xfer($sock2, $sock1);
		select(undef, undef, undef, 0.25);
		if (kill 0, $parent) {
			select(undef, undef, undef, 0.8);
			if (kill 0, $parent) {
				lprint("\[$$/$CURR] kill TERM parent $parent.");
				kill "TERM", $parent;
			} else {
				lprint("\[$$/$CURR] parent $parent gone.");
			}
		}
	}
}

sub fsleep {
	my ($time) = @_;
	select(undef, undef, undef, $time) if $time;
}

sub cleanup {
	close $client_listen  if $client_listen;
	close $client_listen6 if $client_listen6;
	close $server_listen  if $server_listen;
	close $server_listen6 if $server_listen6;
	foreach my $id (keys %ID) {
		close $ID{$id}{sock};
	}
}
