#!/usr/bin/perl

use Getopt::Long;
use nacro;

$output="my_script";
$server="localhost";
$port=5900;
$listen_port=5923;
$timing=0;
$symbolic=0;
$compact=0;
$compact_dragging=0;

if(!GetOptions(
	"script:s" => \$output,
	"listen:i" => \$listen_port,
	"timing" => \$timing,
	"symbolic" => \$symbolic,
	"compact" => \$compact,
	"compact-dragging" => \$compact_dragging,
) || $#ARGV!=0) {
	print STDERR "Usage: $ARGV0 [--script output_name] [--listen listen_port] [--timing]\n\t[--symbolic] [--compact] [--compact-dragging] server[:port]\n";
	exit 2;
}

$output=~s/\.pl$//;

if ($timing) {
	eval 'use Time::HiRes';
	$timing=0 if $@;
	$starttime=-1;
}

if ($symbolic) {
	eval 'use X11::Keysyms qw(%Keysyms)';
	$symbolic=0 if $@;
	%sym_name = reverse %Keysyms;
}

$server=$ARGV[0];

if($server=~/^(.*):(\d+)$/) {
	$server=$1;
	$port=$2;
	if($2<100) {
		$port+=5900;
	}
}

if($listen_port<100) {
	$listen_port+=5900;
}

# do not overwrite script

if(stat("$output.pl")) {
	print STDERR "Will not overwrite $output.pl\n";
	exit 2;
}

# start connection
$vnc=nacro::initvnc($server,$port,$listen_port);

if($vnc<0) {
	print STDERR "Could not initialize $server:$port\n";
	exit 1;
}

open OUT, ">$output.pl";
print OUT "#!/usr/bin/perl\n";
print OUT "\n";
if ($symbolic) {
	print OUT "use X11::Keysyms qw(\%sym);\n";
}
print OUT "use nacro;\n";
print OUT "\n";
print OUT "\$x_origin=0; \$y_origin=0;\n";
print OUT "\$vnc=nacro::initvnc(\"$server\",$port,$listen_port);\n";

$mode="passthru";
$image_counter=1;
$magickey=0;
$x_origin=0; $y_origin=0;

sub writetiming () {
	if ($timing) {
		$now=Time::HiRes::time();
		if ($starttime>0) {
			print OUT "nacro::process(\$vnc," . ($now - $starttime) . ");\n";
		}
		$starttime=$now;
	}
}

$last_button = -1;

sub handle_mouse {
	my $x = shift;
	my $y = shift;
	my $buttons = shift;
	if(nacro::sendmouse($vnc,$x,$y,$buttons)) {
		$x-=$x_origin; $y-=$y_origin;
		writetiming();
		print OUT "nacro::sendmouse(\$vnc,\$x_origin"
			. ($x>=0?"+":"")."$x,\$y_origin"
			. ($y>=0?"+":"")."$y,$buttons);\n";
	}
}

while(1) {
	$result=nacro::waitforinput($vnc,999999);
	if($result==0) {
		# server went away
		close OUT;
		exit 0;
	}

	if($mode eq "passthru") {
		if($result&$nacro::RESULT_KEY) {
			$keysym=nacro::getkeysym($vnc);
			$keydown=nacro::getkeydown($vnc);
			if(nacro::sendkey($vnc,$keysym,$keydown)) {
				writetiming();
				if ($symbolic and exists $sym_name{$keysym}) {
					print OUT 'nacro::sendkey($vnc,$sym{'.$sym_name{$keysym}."},$keydown);\n";
				} else {
					print OUT "nacro::sendkey(\$vnc,$keysym,$keydown);\n";
				}
			}
			if($keysym==0xffe3 || $keysym==0xffe4) {
				if (!$keydown) {
					# Control pressed
					$magickey++;
					if ($magickey > 1) {
						$magickey = 0;
						$mode = "menu";
						nacro::alert($vnc,"VisualNaCro: press 'q' to quit,\n'd' to display current reference image,\nor mark reference rectangle by dragging",10);
					}
				}
			} else {
				$magickey=0;
			}
		}
		if($result&$nacro::RESULT_MOUSE) {
			$x=nacro::getx($vnc);
			$y=nacro::gety($vnc);
			$buttons=nacro::getbuttons($vnc);
			if ($buttons != $last_buttons) {
				if (!$buttons && $compact_dragging) {
					handle_mouse($x, $y, $last_buttons);
				}
				$last_buttons = $buttons;
			} else {
				if (($buttons && $compact_dragging) ||
						(!$buttons && $compact)) {
					next;
				}
			}
			handle_mouse($x, $y, $buttons);
		}
		if ($result & $nacro::RESULT_TEXT_CLIENT) {
			my $text = nacro::gettext_client($vnc);
			if (nacro::sendtext($vnc,$text)) {
				writetiming();
				print OUT "nacro::sendtext(\$vnc, q(\Q$text\E));\n";
				print "got text from client: $text\n";
			}
		}
		if ($result & $nacro::RESULT_TEXT_SERVER) {
			my $text = nacro::gettext_server($vnc);
			if (nacro::sendtext_to_server($vnc,$text)) {
				writetiming();
				print OUT "nacro::sendtext_to_server(\$vnc, q(\Q$text\E));\n";
				print "got text from server: $text\n";
			}
		}
	} else {
		if($result&$nacro::RESULT_KEY) {
			$keysym=nacro::getkeysym($vnc);
			$keydown=nacro::getkeydown($vnc);
			if($keysym==ord('q')) {
				# shutdown
				close OUT;
				nacro::closevnc($vnc);
				exit 0;
			} elsif ($keysym == ord('d')) {
				$pnm=$output.($image_counter - 1).".pnm";
				$res = nacro::displaypnm($vnc, $pnm,
						$x_origin, $y_origin, 1, 10);
						#0, 0, 1, 10);
				if ($res == 0) {
					nacro::alert($vnc, "Error displaying "
							. $pnm, 10);
				}
			} else {
				nacro::alert($vnc,"Unknown key",10);
			}
			$mode="passthru";
		}
		if($result&$nacro::RESULT_MOUSE) {
			$x=nacro::getx($vnc);
			$y=nacro::gety($vnc);
			$buttons=nacro::getbuttons($vnc);
			if(($buttons&1)==1) {
				print STDERR "start draggin: $x $y\n";
				$start_x=$x;
				$start_y=$y;
				nacro::rubberband($vnc, $x, $y);
				$x=nacro::getx($vnc);
				$y=nacro::gety($vnc);
				if($start_x==$x && $start_y==$y) {
					# reset
					print OUT "\$x_origin=0; \$y_origin=0;\n";
				} else {
					if($start_x>$x) {
						$dummy=$x; $x=$start_x; $start_x=$dummy;
					}
					if($start_y>$y) {
						$dummy=$y; $y=$start_y; $start_y=$dummy;
					}
					$pnm=$output.$image_counter.".pnm";
					$image_counter++;
					if(!nacro::savepnm($vnc,$pnm,$start_x,$start_y,$x,$y)) {
						nacro::alert($vnc,"Saving $pnm failed!",10);
					} else {
						$x_origin=$start_x;
						$y_origin=$start_y;
						nacro::alert($vnc,"Got new origin: $x_origin $y_origin",10);
						print OUT "if(nacro::visualgrep(\$vnc,\"$pnm\",999999)) {\n"
							. "\t\$x_origin=nacro::getxorigin(\$vnc);\n"
							. "\t\$y_origin=nacro::getyorigin(\$vnc);\n}\n";
					}
				}
				$mode="passthru";
			}
		}
	}
}

