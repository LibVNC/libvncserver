  #!/bin/sh -- # A comment mentioning perl
eval 'exec perl -S $0 ${1+"$@"}'
        if 0;
#
# slide.pl: amusing example slideshow program for use with x11vnc -rawfb mode.
#
# E.g. x11vnc -rawfb map:/tmp/foo@640x480x32:ff/ff00/ff0000 -pipeinput slide.pl
#
# requires: jpegtopnm(1), (maybe LSB too).
#

@jpegs = qw(
	dr_fun_new.jpg	canon.jpg	go_microsoft.jpg	jonathan2.jpg
	michelle1.jpg	novm.jpg	photo-008.jpg		presrange.jpg
);

# Or:
#	@jpegs = @ARGV;
#	@jpegs = <*.jpg>;

# this is x11vnc's -rawfb value:
if ($ENV{X11VNC_RAWFB_STR} =~ m,:(.*)@(\d+)x(\d+)x(\d+),) {
	$fb = $1;	# filename
	$W = $2;	# width
	$H = $3;	# height
} else {
	die "No usable X11VNC_RAWFB_STR\n";
}

open(FB, ">$fb") || die "$!";

# make a solid background:
$ones = "\377" x ($W * 4);
$grey = "\340" x ($W * 4);
for ($y = 0; $y < $H; $y++) {
	print FB $grey;
}

# this is rather slow with many jpegs... oh well.
foreach $pic (@jpegs) {
	print STDERR "loading '$pic'	please wait ...\n"; 
	open(JPEG, "jpegtopnm '$pic' 2>/dev/null|") || die "$!";
	while (<JPEG>) {
		next if /^P\d/;
		if (/^(\d+)\s+(\d+)\s*$/) {
			$Jpeg{$pic}{w} = $1;
			$Jpeg{$pic}{h} = $2;
		}
		last if /^255$/;
	}
	$data = '';
	while (<JPEG>) {
		$data .= $_;
	}
	close(JPEG);

	# need to put in a 4th 0 byte after RGB for 32bpp. 24bpp doesn't work.
	# (MSB might be other way around).

	$new = '';
	for ($l = 0; $l < int(length($data)/3); $l++) {
		$new .= substr($data, $l * 3, 3) . "\0";
	}
	$Jpeg{$pic}{data} = $new;
	$data = ''; $new = '';

	if ($pic eq $jpegs[0]) {
		showpic(0);
	}
}

$N = scalar(@jpegs);
print STDERR "\nFinished loading $N images. Click Button or Spacebar for next.\n";
$I = 0;

while (<>) {
	# read the next user input event, watch for button press or spacebar:
	###last if /^Keysym.* [qQ] /;
	next unless /^(Pointer.*ButtonPress|Keysym.*space.*KeyPress)/;
	$I = ($I + 1) % $N;
	showpic($I);
}

sub showpic {
	my($i) = @_;

	my $pic = $jpegs[$i];
	my $h = $Jpeg{$pic}{h};
	my $w = $Jpeg{$pic}{w};

	my $dy = int(($H - $h)/2);
	my $dx = int(($W - $w)/2);

	print STDERR "showing pic $i: $pic\t$w x $h +$dy+$dx\n";

	# clear screen:
	seek(FB, 0, 0);
	for ($y = 0; $y < $H; $y++) {
		print FB $ones;
	}

	# insert new picture:
	for ($y = 0; $y < $h; $y++) {
		seek(FB, (($y + $dy) * $W + $dx) * 4, 0);
		$line = substr($Jpeg{$pic}{data}, $y * $w * 4, $w * 4);
		print FB $line;
	}
}

close(FB);
###unlink($fb);	# this (probably) won't kill x11vnc
print STDERR "$0 done.\n";
