#!/usr/bin/perl

use Getopt::Long;
use nacro;

# TODO: take options

$output="my_script";
$server="localhost";
$port=5900;
$listen_port=5923;

if(!GetOptions(
	"script:s" => \$output,
	"listen:i" => \$listen_port
) || $#ARGV!=0) {
	print STDERR "Usage: $ARGV0 [--script output_name] [--listen listen_port] server[:port]\n";
	exit 2;
}

$output=~s/\.pl$//;

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

# TODO: timing

open OUT, ">$output.pl";
print OUT "#!/usr/bin/perl\n";
print OUT "\n";
print OUT "use nacro;\n";
print OUT "\n";
print OUT "\$x_origin=0; \$y_origin=0;\n";
print OUT "\$vnc=nacro::initvnc(\"$server\",$port,$listen_port);\n";

$mode="passthru";
$image_counter=1;
$magickey=0;
$x_origin=0; $y_origin=0;

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
				print OUT "nacro::sendkey(\$vnc,$keysym,$keydown);\n";
			}
			if($keysym==0xffe3 || $keysym==0xffe4) {
				# Control pressed
				$magickey++;
				if($magickey>3 && !$keydown) {
					$magickey=0;
					$mode="menu";
					$dragging=0;
					nacro::alert($vnc,"VisualNaCro: press 'q' to quit\nor mark reference rectangle by dragging",10);
				}
			} else {
				$magickey=0;
			}
		}
		if($result&$nacro::RESULT_MOUSE) {
			$x=nacro::getx($vnc);
			$y=nacro::gety($vnc);
			$buttons=nacro::getbuttons($vnc);
			if(nacro::sendmouse($vnc,$x,$y,$buttons)) {
				$x-=$x_origin; $y-=$y_origin;
				print OUT "nacro::sendmouse(\$vnc,\$x_origin"
					. ($x>=0?"+":"")."$x,\$y_origin"
					. ($y>=0?"+":"")."$y,$buttons);\n";
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
			}
			nacro::alert($vnc,"Unknown key",10);
			$mode="passthru";
		}
		if($result&$nacro::RESULT_MOUSE) {
			$x=nacro::getx($vnc);
			$y=nacro::gety($vnc);
			$buttons=nacro::getbuttons($vnc);
			if(!$dragging && (($buttons&1)==1)) {
			print STDERR "start draggin: $x $y\n";
				$start_x=$x;
				$start_y=$y;
				$dragging=1;
			} elsif($dragging && (($buttons&1)==0)) {
			print STDERR "stop draggin: $x $y\n";
				if($start_x==$x && $start_y==$y) {
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

