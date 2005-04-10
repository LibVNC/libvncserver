  #!/bin/sh -- # A comment mentioning perl
eval 'exec perl -S $0 ${1+"$@"}'
        if 0;
#
# vcinject.pl: simple hack to inject keystrokes into Linux VC tty.
# See LinuxVNC.c for a more careful treatment using C and public API.
#
# Usage:  vcinject.pl <N>   (or /dev/ttyN)
#
# This is an example x11vnc -pipeinput program  E.g.: 
#
#   x11vnc -rawfb map:/dev/fb0@1024x768x16 -pipeinput "vcinject.pl /dev/tty3" 
#
# (see fbset(8) for obtaining fb info).
#
# It reads lines like this from STDIN:
#
# Keysym <id> <down> <n> <Keysym> ...
#
# <id> is ignored, it uses the rest to deduce the keystrokes to send
# to the console.
#

$tty = shift;
$tty = "/dev/tty$tty" if $tty =~ /^\d+$/;

warn "strange tty device: $tty\n" if $tty !~ m,^/dev/tty\d+$,;

open(TTY, ">$tty") || die "open $tty: $!\n";
$fd = fileno(TTY);

$linux_ioctl_syscall = 54;	# common knowledge, eh? :-)
$TIOCSTI = 0x5412;

%Map = qw(
	Escape		27
	Tab 		 9
	Return		13
	BackSpace	 8
	Home		 1
	End		 5
	Up		16
	Down		14
	Right		 6
	Left		 2
	Next		 6
	Prior		 2
);
# the latter few above seem to be vi specials. (since they are normally
# escape sequences, e.g. ESC [ 5 ~)

sub lookup {
	my($down, $key, $name) = @_;

	my $n = -1; 
	$name =~ s/^KP_//;

	# algorithm borrowed from LinuxVNC.c:
	if (! $down) {
		if ($name =~ /^Control/) {
			$control--;
		}
		return $n;
	} 

	if ($name =~ /^Control/) {
		$control++;
	} else {
		if (exists($Map{$name})) {
			$n = $Map{$name};
		}
		if ($control && $name =~ /^[A-z]$/) {
			$n = ord($name);
			# shift down to the Control zone:
			if ($name =~ /[a-z]/) {
				$n -= (ord("a") - 1);
			} else {
				$n -= (ord("A") - 1);
			}
		}
		if ($n < 0 && $key < 256) {
			$n = $key;
		}
	}
	return $n;
}

$control = 0;
$debug = 0;

while (<>) {
	chomp;
	if (/^\w+$/) {
		# for debugging, you type the keysym in manually.
		$_ = "Keysym 1 0 999 $_ None";
	}
	next unless /^Keysym/;

	my ($j, $id, $down, $k, $keysym, $rest) = split(' ', $_);

	$n = lookup($down, $k, $keysym);
	if ($n < 0 || $n > 255) {
		print STDERR "skip: '$keysym' -> $n\n" if $down && $debug;
		next;
	}

	$n_p = pack("c", $n);
	$ret = syscall($linux_ioctl_syscall, $fd, $TIOCSTI, $n_p);

	print STDERR "ctrl=$control $keysym/$k syscall(" .
		"$linux_ioctl_syscall, $fd, $TIOCSTI, $n) = $ret\n" if $debug;

}
