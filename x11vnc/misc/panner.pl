#!/usr/bin/perl
#
# panner.pl: start up x11vnc in '-clip' mode viewing a small (WxH)
#            rectangular region of the screen.  Allow the viewer user
#            to 'pan' around the display region by moving the mouse.
#
#            Remote interaction with applications, e.g. clicking a
#            button though the VNC viewer, will be very difficult.
#            This may be useful in a 'demo' mode where the user sitting
#            at the physical display is the only one moving the mouse.
#            Depending on your usage the following x11vnc options may
#            be useful: -nonap
#
# Usage:  panner.pl WxH      <x11vnc-args>  (e.g. -display ...)
# or      panner.pl WxH:0.05 <x11vnc-args>  (e.g. 0.05 is polling time in secs.)

use strict;

my $WxH = shift;
my $poll_time;

# split off poll time:
#
($WxH, $poll_time) = split(/:/, $WxH);
my ($W, $H) = split(/x/, $WxH);

$poll_time = 0.1 unless $poll_time ne '';

# set to x11vnc command (e.g. full PATH)
#
my $x11vnc = "x11vnc";

# check if display was given:
#
my $query_args = "";
for (my $i=0; $i < @ARGV; $i++) {
	if ($ARGV[$i] eq '-display') {
		$query_args = "-display $ARGV[$i+1]";
	}
}

# find the size of display and the current mouse position:
my %v;
vset("DIRECT:wdpy_x,wdpy_y,pointer_x,pointer_y,pointer_same");

# set a -clip argument based on the above:
#
my $clip = '';
clip_set();
$clip = "${W}x${H}+0+0" unless $v{pointer_same};

# launch x11vnc with -clip in the background:
#
my $cmd = "$x11vnc -clip $clip -bg " . join(" ", @ARGV);
print STDERR "running: $cmd\n";
system $cmd;

# user can hit Ctrl-C or kill this script to quit (and stop x11vnc)
#
sub quit {
	system("$x11vnc $query_args -R stop");
	exit 0;
}

$SIG{INT}  = \&quit;
$SIG{TERM} = \&quit;

# loop forever waiting for mouse position to change, then shift -clip:
#
my $clip_old = $clip;
while (1) {
	fsleep($poll_time);
	vset("pointer_x,pointer_y,pointer_same");
	next unless $v{pointer_same};
	clip_set();
	if ($clip ne $clip_old) {
		system("$x11vnc $query_args -R clip:$clip");
		$clip_old = $clip
	}
}

exit 0;

# short sleep:
#
sub fsleep {
	my ($time) = @_;
	select(undef, undef, undef, $time) if $time;
}

# set the -clip string, making sure view doesn't go off edges of display:
#
sub clip_set {
	my $x = int($v{pointer_x} - $W/2); 
	my $y = int($v{pointer_y} - $H/2); 
	$x = 0 if $x < 0;
	$y = 0 if $y < 0;
	$x = $v{wdpy_x} - $W if $x + $W > $v{wdpy_x};
	$y = $v{wdpy_y} - $H if $y + $H > $v{wdpy_y};
	$clip = "${W}x${H}+$x+$y";
}

# query x11vnc for values, put results in the %v hash:
#
sub vset {
	my $str = shift;
	my $out = `$x11vnc $query_args -Q $str 2>/dev/null`;
	chomp $out;
	foreach my $pair (split(/,/, $out)) {
		$pair =~ s/^a..=//;
		my ($k, $v) = split(/:/, $pair, 2);
		if ($k ne '' && $v ne '') {
			print STDERR "k=$k v=$v\n" if $ENV{DEBUG};
			$v{$k} = $v;
		}
	}
}
