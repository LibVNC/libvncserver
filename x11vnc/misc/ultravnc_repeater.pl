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
          identities; it us up to the client and server to manage that
          and whether to encrypt the session, etc.

usage:  ultravnc_repeater.pl [-r] [client_port [server_port]]

Use -r to refuse new server/client connections with an existing
server/client ID.  The default is to close the previous one.

To write to a log file set the env. var ULTRAVNC_REPEATER_LOGFILE.

To run in a loop restarting the server if it exits set the env. var.
ULTRAVNC_REPEATER_LOOP=1 or ULTRAVNC_REPEATER_LOOP=BG, the latter
forks into the background.  Set ULTRAVNC_REPEATER_PIDFILE to a file
to store the master pid in.


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
	print STDERR "$_[0]:\t$$ looppid=$looppid\n";
	if ($looppid) {
		kill 'TERM', $looppid;
		fsleep(0.2);
	}
	unlink $pidfile if $pidfile;
	cleanup();
	exit 0;
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
			print STDERR "could not open pidfile: $pf - $! - continuing...\n";
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

	print STDERR "ultravnc_repeater.pl: starting service at ", scalar(localtime), " master-pid=$$\n";
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
		print STDERR "ultravnc_repeater.pl: re-starting service at ", scalar(localtime), " master-pid=$$\n";
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

my $prog = 'ultravnc_repeater.pl';
my %ID;

my $refuse = 0;
my $init_timeout = 3;

if (@ARGV && $ARGV[0] =~ /-h/) {
	print $usage;
	exit 0;
}
if (@ARGV && $ARGV[0] eq '-r') {
	$refuse = 1;
	shift;
}

my $client_port = shift;
my $server_port = shift;

$client_port = 5900 unless $client_port;
$server_port = 5500 unless $server_port;


my $repeater_bufsize = 250;
$repeater_bufsize = $ENV{BUFSIZE} if exists $ENV{BUFSIZE};

my ($RIN, $WIN, $EIN, $ROUT);

my $client_listen = IO::Socket::INET->new(
	Listen    => 10,
	LocalPort => $client_port, 
	ReuseAddr => 1,
	Proto => "tcp"
);
if (! $client_listen) {
	cleanup();
	die "$prog: error: client listen on port $client_port: $!\n";
}

my $server_listen = IO::Socket::INET->new(
	Listen    => 10,
	LocalPort => $server_port, 
	ReuseAddr => 1,
	Proto => "tcp"
);
if (! $server_listen) {
	cleanup();
	die "$prog: error: server listen on port $server_port: $!\n";
}

my $select = new IO::Select();
if (! $select) {
	cleanup();
	die "$prog: select $!\n";
}

$select->add($client_listen);
$select->add($server_listen);

$SIG{INT}  = sub {cleanup(); exit;};
$SIG{TERM} = sub {cleanup(); exit;};

my $SOCK1 = '';
my $SOCK2 = '';
my $CURR = '';

print "watching for connections on ports $server_port/server and $client_port/client\n";

my $alarm_sock = '';
my $got_alarm = 0;
sub alarm_handler {
	print "$prog: got sig alarm.\n";
	if ($alarm_sock ne '') {
		close $alarm_sock;
	}
	$alarm_sock = '';
	$got_alarm = 1;
}

while (my @ready = $select->can_read()) {
	foreach my $fh (@ready) {
		if ($fh == $client_listen) {
			print "new vnc client connecting at ", scalar(localtime), "\n"; 
		} elsif ($fh == $server_listen) {
			print "new vnc server connecting at ", scalar(localtime), "\n"; 
		}
		my $sock = $fh->accept();
		if (! $sock) {
			print "$prog: accept $!\n";
			next;
		}

		if ($fh == $client_listen) {
			my $str = "RFB 000.000\n";
			my $len = length $str;
			my $n = syswrite($sock, $str, $len, 0);
			if ($n != $len) {
				print "$prog: bad $str write: $n != $len $!\n";
				close $sock;
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
			print "$prog: read timed out: $!\n";
		} elsif (! defined $n) {
			print "$prog: read error: $!\n";
		} elsif ($repeater_bufsize > 0 && $n != $size) {
			print "$prog: short read $n != $size $!\n";
			close $sock;
		} elsif ($fh == $client_listen) {
			do_new_client($sock, $buf);
		} elsif ($fh == $server_listen) {
			do_new_server($sock, $buf);
		}
	}
}

sub do_new_client {
	my ($sock, $buf) = @_;

	if ($buf =~ /^ID:(\w+)/) {
		my $id = $1;
		if (exists $ID{$id}) {
			if ($ID{$id}{client}) {
				print "refusing extra vnc client for ID:$id\n";
				close $sock;
				return;
				if ($refuse) {
					print "refusing extra vnc client for ID:$id\n";
					close $sock;
					return;
				} else {
					print "closing and deleting previous vnc client with ID:$id\n";
					close $ID{$id}{sock};

					print "storing new vnc client with ID:$id\n";
					$ID{$id}{client} = 1;
					$ID{$id}{sock} = $sock;
				}
			} else {
				print "hooking up new vnc client with existing vnc server for ID:$id\n";
				my $sock2 = $ID{$id}{sock};
				delete $ID{$id};
				hookup($sock, $sock2, "ID:$id"); 
			}
		} else {
			print "storing new vnc client with ID:$id\n";
			$ID{$id}{client} = 1;
			$ID{$id}{sock} = $sock;
		}
	} else {
		my $str = sprintf("%s", $buf);
		my $host = '';
		my $port = '';
		if ($str =~ /^(.+):(\d+)/) {
			$host = $1;
			$port = $2;
		} else {
			$host = $str;
			$port = 5900;
		}
		if ($port < 0) {
			my $pnew = -$port;
			print "resetting port from $port to $pnew\n";
			$port = $pnew;
		} elsif ($port < 200) {
			my $pnew = $port + 5900;
			print "resetting port from $port to $pnew\n";
			$port = $pnew;
		}
		print "making vnc client connection directly to vnc server $host:$port\n";
		my $sock2 =  IO::Socket::INET->new(
			PeerAddr => $host,
			PeerPort => $port,
			Proto => "tcp"
		);
		if (!$sock2) {
			print "failed to connect to $host:$port\n";
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
		if (exists $ID{$id}) {
			if (! $ID{$id}{client}) {
				if ($refuse) {
					print "refusing extra vnc server for ID:$id\n";
					close $sock;
					return;
				} else {
					print "closing and deleting previous vnc server with ID:$id\n";
					close $ID{$id}{sock};

					print "storing new vnc server with ID:$id\n";
					$ID{$id}{client} = 0;
					$ID{$id}{sock} = $sock;
				}
			} else {
				print "hooking up new vnc server with existing vnc client for ID:$id\n";
				my $sock2 = $ID{$id}{sock};
				delete $ID{$id};
				hookup($sock, $sock2, "ID:$id"); 
			}
		} else {
			print "storing new vnc server with ID:$id\n";
			$ID{$id}{client} = 0;
			$ID{$id}{sock} = $sock;
		}
	} else {
		print "invalid ID:NNNNN string for vnc server: $buf\n";
		close $sock;
		return;
	}
}

sub handler {
	print STDERR "$prog\[$$/$CURR]: got SIGTERM.\n";
	close $SOCK1 if $SOCK1;
	close $SOCK2 if $SOCK2;
	exit;
}

sub hookup {
	my ($sock1, $sock2, $tag) = @_;

	my $worker = fork();

	if (! defined $worker) {
		print "failed to fork worker: $!\n";
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
			print STDERR "$prog\[$$/$CURR]: $!\n";
			last;
		} elsif ($len == 0) {
			print STDERR "$prog\[$$/$CURR]: Input is EOF.\n";
			last;
		}
		my $offset = 0;
		my $quit = 0;
		while ($len) {
			my $written = syswrite($out, $buf, $len, $offset);
			if (! defined $written) {
				print STDERR "$prog\[$$/$CURR]: Output is EOF. $!\n";
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
	print STDERR "$prog\[$$/$CURR]: finished xfer.\n";
}

sub xfer_both {
	my ($sock1, $sock2) = @_;

	my $parent = $$;

	my $child = fork();

	if (! defined $child) {
		print STDERR "$prog\[$$/$CURR] failed to fork: $!\n";
		return;
	}

	$SIG{TERM} = "handler";
	$SIG{INT}  = "handler";

	if ($child) {
		print STDERR "$prog parent[$$/$CURR]  1 -> 2\n";
		xfer($sock1, $sock2);
		select(undef, undef, undef, 0.25);
		if (kill 0, $child) {
			select(undef, undef, undef, 0.9);
			if (kill 0, $child) {
				print STDERR "$prog\[$$/$CURR]: kill TERM child $child\n";
				kill "TERM", $child;
			} else {
				print STDERR "$prog\[$$/$CURR]: child  $child gone.\n";
			}
		}
	} else {
		select(undef, undef, undef, 0.05);
		print STDERR "$prog child [$$/$CURR]  2 -> 1\n";
		xfer($sock2, $sock1);
		select(undef, undef, undef, 0.25);
		if (kill 0, $parent) {
			select(undef, undef, undef, 0.8);
			if (kill 0, $parent) {
				print STDERR "$prog\[$$/$CURR]: kill TERM parent $parent\n";
				kill "TERM", $parent;
			} else {
				print STDERR "$prog\[$$/$CURR]: parent $parent gone.\n";
			}
		}
	}
}

sub cleanup {
	close $client_listen if defined $client_listen;
	close $server_listen if defined $server_listen;
	foreach my $id (keys %ID) {
		close $ID{$id}{sock};
	}
}
