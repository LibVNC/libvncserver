  #!/bin/sh -- # A comment mentioning perl
eval 'exec perl -S $0 ${1+"$@"}'
        if 0;

# ranfb.pl: example -rawfb setup program.
# E.g.  x11vnc -rawfb setup:./ranfb.pl

# can supply WxH or W H on cmd line:
if ($ARGV[0] =~ /^(\d+)x(\d+)$/) {
	$W = $1;
	$H = $2;
} else {
	$W = shift;
	$H = shift;
}
 
$W = 480 unless $W;
$H = 360 unless $H;

$fb = "/tmp/ranfb.$$";
open(FB, ">$fb") || die "$!";

$ones = "\377" x ($W * 4);
for ($y = 0; $y < $H; $y++) {
	print FB $ones;
}

if (fork) {
	print "map:$fb\@${W}x${H}x32\n";
	exit 0;
}

srand();
while (1) {
	showpic();
	if (! kill 0, $ENV{X11VNC_PID}) {
		print STDERR "PID $ENV{X11VNC_PID} gone\n";
		unlink($fb);
		exit;
	}
}

sub showpic {

	#  0 < x,y < 1;  R1, R2, ... B4 random & scaled so R,G,B < 255:
	# R(x,y) = R1 + R2 * x + R3 * y + R4 * x * y
	# G(x,y) = G1 + G2 * x + G3 * y + G4 * x * y
	# B(x,y) = B1 + B2 * x + B3 * y + B4 * x * y

	$minfac = 0.25;
	foreach $c ('R', 'G', 'B') {
		$a1 = rand() * $minfac;
		$a2 = rand();
		$a3 = rand();
		$a4 = rand();
		$at = $a1 + $a2 + $a3 + $a4;
		$a1 = 255 * ($a1/$at);
		$a2 = 255 * ($a2/$at);
		$a3 = 255 * ($a3/$at);
		$a4 = 255 * ($a4/$at);
		# invert axes randomly
		$ax = 0; $ax = 1 if rand() < 0.5;
		$ay = 0; $ay = 1 if rand() < 0.5;
		eval "\$${c}1 = \$a1";
		eval "\$${c}2 = \$a2";
		eval "\$${c}3 = \$a3";
		eval "\$${c}4 = \$a4";
		eval "\$${c}x = \$ax";
		eval "\$${c}y = \$ay";
	}

	for ($i = 0; $i < 256; $i++) {
		$p[$i] = pack("c", $i);
	}

	$Winv = 1.0/$W;
	$Hinv = 1.0/$H;

	$str = '';
	for ($y = 0; $y < $H; $y++) {
		$yr = $yg = $yb = $y;
		$yr = $H - $yr if $Ry;
		$yg = $H - $yg if $Gy;
		$yb = $H - $yb if $By;
		$yr = $yr * $Hinv;
		$yg = $yg * $Hinv;
		$yb = $yb * $Hinv;
		
		$Y[3*$y+0] = $yr;
		$Y[3*$y+1] = $yg;
		$Y[3*$y+2] = $yb;
	}

	for ($x = 0; $x < $W; $x++) {
		$xr = $xg = $xb = $x;
		$xr = $W - $xr if $Rx;
		$xg = $W - $xg if $Gx;
		$xb = $W - $xb if $Bx;
		$xr = $xr * $Winv;
		$xg = $xg * $Winv;
		$xb = $xb * $Winv;

		$X[3*$x+0] = $xr;
		$X[3*$x+1] = $xg;
		$X[3*$x+2] = $xb;
	}

	for ($y = 0; $y < $H; $y++) {
		#$yr = $yg = $yb = $y;
		#$yr = $H - $yr if $Ry;
		#$yg = $H - $yg if $Gy;
		#$yb = $H - $yb if $By;
		#$yr = $yr * $Hinv;
		#$yg = $yg * $Hinv;
		#$yb = $yb * $Hinv;

		$yr = $Y[3*$y+0];
		$yg = $Y[3*$y+1];
		$yb = $Y[3*$y+2];

		$RY1 = $R1 + $yr * $R3;
		$GY1 = $G1 + $yg * $G3;
		$BY1 = $B1 + $yb * $B3;

		$RY2 = $R2 + $yr * $R4;
		$GY2 = $G2 + $yg * $G4;
		$BY2 = $B2 + $yb * $B4;

		for ($x = 0; $x < $W; $x++) {
			#$xr = $xg = $xb = $x;
			#$xr = $W - $xr if $Rx;
			#$xg = $W - $xg if $Gx;
			#$xb = $W - $xb if $Bx;
			#$xr = $xr * $Winv;
			#$xg = $xg * $Winv;
			#$xb = $xb * $Winv;

			$n = 3 * $x;

			#$v = int($R1 + $xr*$R2 + $yr*$R3 + $xr*$yr*$R4);
			$v  = int($RY1 + $X[$n]*$RY2);
			$str .= $p[$v];

			#$v = int($G1 + $xg*$G2 + $yg*$G3 + $xg*$yg*$G4);
			$v  = int($GY1 + $X[$n+1]*$GY2);
			$str .= $p[$v];

			#$v = int($B1 + $xb*$B2 + $yb*$B3 + $xb*$yb*$B4);
			$v  = int($BY1 + $X[$n+2]*$BY2);
			$str .= $p[$v];

			$str .= "\0";
		}
	}
	seek(FB, 0, 0);
	print FB $str;
}
