#!/usr/bin/perl

# This is a test injection script for Linux uinput.
# It can be handy working out / troubleshooting Linux uinput injection on a new device.

#
# Copyright (c) 2010 by Karl J. Runge <runge@karlrunge.com>
#
# uinput.pl is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
# 
# uinput.pl is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with uinput.pl; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA
# or see <http://www.gnu.org/licenses/>.
# 

set_constants();

# options for what injection to handle:
$rel = 1;
$abs = 1;
$touch = 1;
$allkeys = 1;

# these can be set via env:

$WIDTH  = $ENV{WIDTH};
$WIDTH  = 480 unless $WIDTH;
$HEIGHT = $ENV{HEIGHT};
$HEIGHT = 640 unless $HEIGHT;
$DEV = $ENV{DEV};
$DEV = "/dev/input/uinput" unless $DEV;

# this fills in name and input type part of uinput_user_dev struct:

$udev = "uinput.pl";
$n = 80 - length($udev);
$udev .= "\0" x $n;

$udev .= "\0" x 2;	# bus
$udev .= "\0" x 2;	# vendor
$udev .= "\0" x 2;	# product
$udev .= "\0" x 2;	# version

$udev .= "\0" x 4;	# ff_effects_max

# this fills in the abs arrays:
#
foreach $type (qw(absmax absmin absfuzz absflat))  {
	$n = $ABS_MAX + 1;
	for ($j = 0; $j < $n; $j++) {
		if ($abs && $type eq 'absmax' && $j == $ABS_X) {
			$udev .= pack("i", $WIDTH-1);
		} elsif ($abs && $type eq 'absmax' && $j == $ABS_Y) {
			$udev .= pack("i", $HEIGHT-1);
		} else {
			$udev .= "\0" x 4;
		}
	}
}

print "udev: ", length($udev) . " '$udev'\n";

$modes = $O_RDWR;
$modes = $O_WRONLY | $O_NDELAY;
printf("open modes: 0x%x\n", $modes);

sysopen(FD, $DEV, $modes) || die "$DEV: $!";

if ($rel) {
	io_ctl($UI_SET_EVBIT,  $EV_REL);
	io_ctl($UI_SET_RELBIT, $REL_X);
	io_ctl($UI_SET_RELBIT, $REL_Y);
}

io_ctl($UI_SET_EVBIT, $EV_KEY);

io_ctl($UI_SET_EVBIT, $EV_SYN);

for ($i=0; $i < 256; $i++) {
	last unless $allkeys;
	io_ctl($UI_SET_KEYBIT, $i);
}

io_ctl($UI_SET_KEYBIT, $BTN_MOUSE);
io_ctl($UI_SET_KEYBIT, $BTN_LEFT);
io_ctl($UI_SET_KEYBIT, $BTN_MIDDLE);
io_ctl($UI_SET_KEYBIT, $BTN_RIGHT);
io_ctl($UI_SET_KEYBIT, $BTN_FORWARD);
io_ctl($UI_SET_KEYBIT, $BTN_BACK);

if ($abs) {
	io_ctl($UI_SET_KEYBIT, $BTN_TOUCH) if $touch;
	io_ctl($UI_SET_EVBIT,  $EV_ABS);
	io_ctl($UI_SET_ABSBIT, $ABS_X);
	io_ctl($UI_SET_ABSBIT, $ABS_Y);
}

$ret = syswrite(FD, $udev, length($udev));
print "syswrite: $ret\n";

io_ctl($UI_DEV_CREATE);
fsleep(0.25);

# this should show our new virtual device:
#
system("cat /proc/bus/input/devices 1>&2");
print STDERR "\n";

#################################################
# put in your various test injection events here:

#do_key($KEY_A, 1, 0.1);
#do_key($KEY_A, 0, 0.1);

#do_key($KEY_POWER, 1, 0.1);
#do_key($KEY_POWER, 0, 0.1);

do_abs(118, 452, 0, 0.1);

do_abs(110, 572, 1, 0.1);

do_btn($BTN_TOUCH, 1, 0.1);
do_btn($BTN_TOUCH, 0, 0.1);

do_btn($BTN_MOUSE, 1, 0.1);
do_btn($BTN_MOUSE, 0, 0.1);
#################################################

fsleep(0.25);
io_ctl($UI_DEV_DESTROY);

close(FD);

exit(0);

sub io_ctl {
	my ($cmd, $val) = @_;
	if (defined $val) {
		my $ret = syscall($linux_ioctl_syscall, fileno(FD), $cmd, $val);
		my $err = $!; $err = '' if $ret == 0;
		print STDERR "io_ctl(FD, $cmd, $val) = $ret  $err\n";
	} else {
		my $ret = syscall($linux_ioctl_syscall, fileno(FD), $cmd);
		my $err = $!; $err = '' if $ret == 0;
		print STDERR "io_ctl(FD, $cmd) = $ret  $err\n";
	}
}

sub do_syn {
	my $ev = gtod();
	$ev .= pack("S", $EV_SYN);
	$ev .= pack("S", $SYN_REPORT);
	$ev .= pack("i", 0);
	print STDERR "do_syn EV_SYN\n";
	my $ret = syswrite(FD, $ev, length($ev));
	if (!defined $ret) {
		print STDERR "do_syn: $!\n";
	}
}

sub do_key {
	my ($key, $down, $sleep) = @_;
	my $ev = gtod();
	$ev .= pack("S", $EV_KEY);
	$ev .= pack("S", $key);
	$ev .= pack("i", $down);
	print STDERR "do_key $key $down\n";
	my $ret = syswrite(FD, $ev, length($ev));
	if (!defined $ret) {
		print STDERR "do_key: $!\n";
	}
	do_syn();
	fsleep($sleep);
	print STDERR "\n";
}

sub do_btn {
	my ($button, $down, $sleep) = @_;
	my $ev = gtod();
	$ev .= pack("S", $EV_KEY);
	$ev .= pack("S", $button);
	$ev .= pack("i", $down);
	print STDERR "do_btn $button $down\n";
	my $ret = syswrite(FD, $ev, length($ev));
	if (!defined $ret) {
		print STDERR "do_btn: $!\n";
	}
	do_syn();
	fsleep($sleep);
	print STDERR "\n";
}

sub do_abs {
	my ($x, $y, $p, $sleep) = @_;
	my $ev = gtod();
	$ev .= pack("S", $EV_ABS);
	$ev .= pack("S", $ABS_Y);
	$ev .= pack("i", $y);
	print STDERR "do_abs y=$y\n";
	my $ret = syswrite(FD, $ev, length($ev));
	if (!defined $ret) {
		print STDERR "do_abs: $!\n";
	}
	$ev = gtod();
	$ev .= pack("S", $EV_ABS);
	$ev .= pack("S", $ABS_X);
	$ev .= pack("i", $x);
	print STDERR "do_abs x=$x\n";
	$ret = syswrite(FD, $ev, length($ev));
	if (!defined $ret) {
		print STDERR "do_abs: $!\n";
	}
	$ev = gtod();
	$ev .= pack("S", $EV_ABS);
	$ev .= pack("S", $ABS_PRESSURE);
	$ev .= pack("i", $p);
	print STDERR "do_abs p=$p\n";
	$ret = syswrite(FD, $ev, length($ev));
	if (!defined $ret) {
		print STDERR "do_abs: $!\n";
	}
	do_syn();
	fsleep($sleep);
	print STDERR "\n";
}

sub do_rel {
	my ($dx, $dy, $sleep) = @_;
	my $ev = gtod();
	$ev .= pack("S", $EV_REL);
	$ev .= pack("S", $REL_Y);
	$ev .= pack("i", $dy);
	print STDERR "do_rel dy=$dy\n";
	my $ret = syswrite(FD, $ev, length($ev));
	if (!defined $ret) {
		print STDERR "do_rel: $!\n";
	}
	$ev = gtod();
	$ev .= pack("S", $EV_REL);
	$ev .= pack("S", $REL_X);
	$ev .= pack("i", $dx);
	print STDERR "do_rel dx=$dx\n";
	$ret = syswrite(FD, $ev, length($ev));
	if (!defined $ret) {
		print STDERR "do_rel: $!\n";
	}
	do_syn();
	fsleep($sleep);
	print STDERR "\n";
}

sub gtod {
	$tv = ("\0" x 4) x 2;   # assumes long is 4 bytes.  FIXME: use pack.
	$tz = ("\0" x 4) x 2;
	syscall($linux_gettimeofday_syscall, $tv, $tz);
	return $tv;
}

sub fsleep {
        my ($time) = @_; 
        select(undef, undef, undef, $time) if $time;
}

sub set_constants {

# from /usr/include/linux/uinput.h /usr/include/linux/input.h and x11vnc. 

# #define ABS_MAX                 0x3f = 63
# 
# #define UINPUT_MAX_NAME_SIZE    80
# 
# struct input_id {
#         __u16 bustype;
#         __u16 vendor;
#         __u16 product;
#         __u16 version;
# };
# 
# struct uinput_user_dev {
#         char name[UINPUT_MAX_NAME_SIZE];
#         struct input_id id;
#         int ff_effects_max;
#         int absmax[ABS_MAX + 1];
#         int absmin[ABS_MAX + 1];
#         int absfuzz[ABS_MAX + 1];
#         int absflat[ABS_MAX + 1];
# };
# #endif  /* __UINPUT_H_ */

$EV_SYN                  = 0x00;
$EV_KEY                  = 0x01;
$EV_REL                  = 0x02;
$EV_ABS                  = 0x03;
$EV_MSC                  = 0x04;
$EV_SW                   = 0x05;
$EV_LED                  = 0x11;
$EV_SND                  = 0x12;
$EV_REP                  = 0x14;
$EV_FF                   = 0x15;
$EV_PWR                  = 0x16;
$EV_FF_STATUS            = 0x17;
$EV_MAX                  = 0x1f;

$ID_BUS                  = 0;
$ID_VENDOR               = 1;
$ID_PRODUCT              = 2;
$ID_VERSION              = 3;

$BUS_PCI                 = 0x01;
$BUS_ISAPNP              = 0x02;
$BUS_USB                 = 0x03;
$BUS_HIL                 = 0x04;
$BUS_BLUETOOTH           = 0x05;
$BUS_VIRTUAL             = 0x06;

$BUS_ISA                 = 0x10;
$BUS_I8042               = 0x11;
$BUS_XTKBD               = 0x12;
$BUS_RS232               = 0x13;
$BUS_GAMEPORT            = 0x14;
$BUS_PARPORT             = 0x15;
$BUS_AMIGA               = 0x16;
$BUS_ADB                 = 0x17;
$BUS_I2C                 = 0x18;
$BUS_HOST                = 0x19;
$BUS_GSC                 = 0x1A;
$BUS_ATARI               = 0x1B;

$REL_X                   = 0x00;
$REL_Y                   = 0x01;
$REL_Z                   = 0x02;
$REL_RX                  = 0x03;
$REL_RY                  = 0x04;
$REL_RZ                  = 0x05;
$REL_HWHEEL              = 0x06;
$REL_DIAL                = 0x07;
$REL_WHEEL               = 0x08;
$REL_MISC                = 0x09;

$ABS_X                   = 0x00;
$ABS_Y                   = 0x01;
$ABS_Z                   = 0x02;
$ABS_RX                  = 0x03;
$ABS_RY                  = 0x04;
$ABS_RZ                  = 0x05;
$ABS_THROTTLE            = 0x06;
$ABS_RUDDER              = 0x07;
$ABS_WHEEL               = 0x08;
$ABS_GAS                 = 0x09;
$ABS_BRAKE               = 0x0a;
$ABS_HAT0X               = 0x10;
$ABS_HAT0Y               = 0x11;
$ABS_HAT1X               = 0x12;
$ABS_HAT1Y               = 0x13;
$ABS_HAT2X               = 0x14;
$ABS_HAT2Y               = 0x15;
$ABS_HAT3X               = 0x16;
$ABS_HAT3Y               = 0x17;
$ABS_PRESSURE            = 0x18;
$ABS_DISTANCE            = 0x19;
$ABS_TILT_X              = 0x1a;
$ABS_TILT_Y              = 0x1b;
$ABS_TOOL_WIDTH          = 0x1c;
$ABS_VOLUME              = 0x20;
$ABS_MISC                = 0x28;
$ABS_MT_TOUCH_MAJOR      = 0x30;
$ABS_MT_TOUCH_MINOR      = 0x31;
$ABS_MT_WIDTH_MAJOR      = 0x32;
$ABS_MT_WIDTH_MINOR      = 0x33;
$ABS_MT_ORIENTATION      = 0x34;
$ABS_MT_POSITION_X       = 0x35;
$ABS_MT_POSITION_Y       = 0x36;
$ABS_MT_TOOL_TYPE        = 0x37;
$ABS_MT_BLOB_ID          = 0x38;
$ABS_MT_TRACKING_ID      = 0x39;
#$ABS_MAX = 0x3f;


$BTN_MISC                = 0x100;
$BTN_0                   = 0x100;
$BTN_1                   = 0x101;
$BTN_2                   = 0x102;
$BTN_3                   = 0x103;
$BTN_4                   = 0x104;
$BTN_5                   = 0x105;
$BTN_6                   = 0x106;
$BTN_7                   = 0x107;
$BTN_8                   = 0x108;
$BTN_9                   = 0x109;

$BTN_MOUSE               = 0x110;
$BTN_LEFT                = 0x110;
$BTN_RIGHT               = 0x111;
$BTN_MIDDLE              = 0x112;
$BTN_SIDE                = 0x113;
$BTN_EXTRA               = 0x114;
$BTN_FORWARD             = 0x115;
$BTN_BACK                = 0x116;
$BTN_TASK                = 0x117;

$BTN_JOYSTICK            = 0x120;
$BTN_TRIGGER             = 0x120;
$BTN_THUMB               = 0x121;
$BTN_THUMB2              = 0x122;
$BTN_TOP                 = 0x123;
$BTN_TOP2                = 0x124;
$BTN_PINKIE              = 0x125;
$BTN_BASE                = 0x126;
$BTN_BASE2               = 0x127;
$BTN_BASE3               = 0x128;
$BTN_BASE4               = 0x129;
$BTN_BASE5               = 0x12a;
$BTN_BASE6               = 0x12b;
$BTN_DEAD                = 0x12f;

$BTN_GAMEPAD             = 0x130;
$BTN_A                   = 0x130;
$BTN_B                   = 0x131;
$BTN_C                   = 0x132;
$BTN_X                   = 0x133;
$BTN_Y                   = 0x134;
$BTN_Z                   = 0x135;
$BTN_TL                  = 0x136;
$BTN_TR                  = 0x137;
$BTN_TL2                 = 0x138;
$BTN_TR2                 = 0x139;
$BTN_SELECT              = 0x13a;
$BTN_START               = 0x13b;
$BTN_MODE                = 0x13c;
$BTN_THUMBL              = 0x13d;
$BTN_THUMBR              = 0x13e;

$BTN_DIGI                = 0x140;
$BTN_TOOL_PEN            = 0x140;
$BTN_TOOL_RUBBER         = 0x141;
$BTN_TOOL_BRUSH          = 0x142;
$BTN_TOOL_PENCIL         = 0x143;
$BTN_TOOL_AIRBRUSH       = 0x144;
$BTN_TOOL_FINGER         = 0x145;
$BTN_TOOL_MOUSE          = 0x146;
$BTN_TOOL_LENS           = 0x147;
$BTN_TOUCH               = 0x14a;
$BTN_STYLUS              = 0x14b;
$BTN_STYLUS2             = 0x14c;
$BTN_TOOL_DOUBLETAP      = 0x14d;
$BTN_TOOL_TRIPLETAP      = 0x14e;

$BTN_WHEEL               = 0x150;
$BTN_GEAR_DOWN           = 0x150;
$BTN_GEAR_UP             = 0x151;

$SYN_REPORT              = 0;
$SYN_CONFIG              = 1;
$SYN_MT_REPORT           = 2;

$KEY_RESERVED = 0;
$KEY_ESC =      1;
$KEY_1 =        2;
$KEY_2 =        3;
$KEY_3 =        4;
$KEY_4 =        5;
$KEY_5 =        6;
$KEY_6 =        7;
$KEY_7 =        8;
$KEY_8 =        9;
$KEY_9 =        10;
$KEY_0 =        11;
$KEY_MINUS =    12;
$KEY_EQUAL =    13;
$KEY_BACKSPACE =        14;
$KEY_TAB =      15;
$KEY_Q =        16;
$KEY_W =        17;
$KEY_E =        18;
$KEY_R =        19;
$KEY_T =        20;
$KEY_Y =        21;
$KEY_U =        22;
$KEY_I =        23;
$KEY_O =        24;
$KEY_P =        25;
$KEY_LEFTBRACE =        26;
$KEY_RIGHTBRACE =       27;
$KEY_ENTER =    28;
$KEY_LEFTCTRL = 29;
$KEY_A =        30;
$KEY_S =        31;
$KEY_D =        32;
$KEY_F =        33;
$KEY_G =        34;
$KEY_H =        35;
$KEY_J =        36;
$KEY_K =        37;
$KEY_L =        38;
$KEY_SEMICOLON =        39;
$KEY_APOSTROPHE =       40;
$KEY_GRAVE =    41;
$KEY_LEFTSHIFT =        42;
$KEY_BACKSLASH =        43;
$KEY_Z =        44;
$KEY_X =        45;
$KEY_C =        46;
$KEY_V =        47;
$KEY_B =        48;
$KEY_N =        49;
$KEY_M =        50;
$KEY_COMMA =    51;
$KEY_DOT =      52;
$KEY_SLASH =    53;
$KEY_RIGHTSHIFT =       54;
$KEY_KPASTERISK =       55;
$KEY_LEFTALT =  56;
$KEY_SPACE =    57;
$KEY_CAPSLOCK = 58;
$KEY_F1 =       59;
$KEY_F2 =       60;
$KEY_F3 =       61;
$KEY_F4 =       62;
$KEY_F5 =       63;
$KEY_F6 =       64;
$KEY_F7 =       65;
$KEY_F8 =       66;
$KEY_F9 =       67;
$KEY_F10 =      68;
$KEY_NUMLOCK =  69;
$KEY_SCROLLLOCK =       70;
$KEY_KP7 =      71;
$KEY_KP8 =      72;
$KEY_KP9 =      73;
$KEY_KPMINUS =  74;
$KEY_KP4 =      75;
$KEY_KP5 =      76;
$KEY_KP6 =      77;
$KEY_KPPLUS =   78;
$KEY_KP1 =      79;
$KEY_KP2 =      80;
$KEY_KP3 =      81;
$KEY_KP0 =      82;
$KEY_KPDOT =    83;
$KEY_103RD =    84;
$KEY_F13 =      85;
$KEY_102ND =    86;
$KEY_F11 =      87;
$KEY_F12 =      88;
$KEY_F14 =      89;
$KEY_F15 =      90;
$KEY_F16 =      91;
$KEY_F17 =      92;
$KEY_F18 =      93;
$KEY_F19 =      94;
$KEY_F20 =      95;
$KEY_KPENTER =  96;
$KEY_RIGHTCTRL =        97;
$KEY_KPSLASH =  98;
$KEY_SYSRQ =    99;
$KEY_RIGHTALT = 100;
$KEY_LINEFEED = 101;
$KEY_HOME =     102;
$KEY_UP =       103;
$KEY_PAGEUP =   104;
$KEY_LEFT =     105;
$KEY_RIGHT =    106;
$KEY_END =      107;
$KEY_DOWN =     108;
$KEY_PAGEDOWN = 109;
$KEY_INSERT =   110;
$KEY_DELETE =   111;
$KEY_MACRO =    112;
$KEY_MUTE =     113;
$KEY_VOLUMEDOWN =       114;
$KEY_VOLUMEUP = 115;
$KEY_POWER =    116;
$KEY_KPEQUAL =  117;
$KEY_KPPLUSMINUS =      118;
$KEY_PAUSE =    119;
$KEY_F21 =      120;
$KEY_F22 =      121;
$KEY_F23 =      122;
$KEY_F24 =      123;
$KEY_KPCOMMA =  124;
$KEY_LEFTMETA = 125;
$KEY_RIGHTMETA =        126;
$KEY_COMPOSE =  127;
$KEY_STOP =     128;
$KEY_AGAIN =    129;
$KEY_PROPS =    130;
$KEY_UNDO =     131;
$KEY_FRONT =    132;
$KEY_COPY =     133;
$KEY_OPEN =     134;
$KEY_PASTE =    135;
$KEY_FIND =     136;
$KEY_CUT =      137;
$KEY_HELP =     138;
$KEY_MENU =     139;
$KEY_CALC =     140;
$KEY_SETUP =    141;
$KEY_SLEEP =    142;
$KEY_WAKEUP =   143;
$KEY_FILE =     144;
$KEY_SENDFILE = 145;
$KEY_DELETEFILE =       146;
$KEY_XFER =     147;
$KEY_PROG1 =    148;
$KEY_PROG2 =    149;
$KEY_WWW =      150;
$KEY_MSDOS =    151;
$KEY_COFFEE =   152;
$KEY_DIRECTION =        153;
$KEY_CYCLEWINDOWS =     154;
$KEY_MAIL =     155;
$KEY_BOOKMARKS =        156;
$KEY_COMPUTER = 157;
$KEY_BACK =     158;
$KEY_FORWARD =  159;
$KEY_CLOSECD =  160;
$KEY_EJECTCD =  161;
$KEY_EJECTCLOSECD =     162;
$KEY_NEXTSONG = 163;
$KEY_PLAYPAUSE =        164;
$KEY_PREVIOUSSONG =     165;
$KEY_STOPCD =   166;
$KEY_RECORD =   167;
$KEY_REWIND =   168;
$KEY_PHONE =    169;
$KEY_ISO =      170;
$KEY_CONFIG =   171;
$KEY_HOMEPAGE = 172;
$KEY_REFRESH =  173;
$KEY_EXIT =     174;
$KEY_MOVE =     175;
$KEY_EDIT =     176;
$KEY_SCROLLUP = 177;
$KEY_SCROLLDOWN =       178;
$KEY_KPLEFTPAREN =      179;
$KEY_KPRIGHTPAREN =     180;
$KEY_INTL1 =    181;
$KEY_INTL2 =    182;
$KEY_INTL3 =    183;
$KEY_INTL4 =    184;
$KEY_INTL5 =    185;
$KEY_INTL6 =    186;
$KEY_INTL7 =    187;
$KEY_INTL8 =    188;
$KEY_INTL9 =    189;
$KEY_LANG1 =    190;
$KEY_LANG2 =    191;
$KEY_LANG3 =    192;
$KEY_LANG4 =    193;
$KEY_LANG5 =    194;
$KEY_LANG6 =    195;
$KEY_LANG7 =    196;
$KEY_LANG8 =    197;
$KEY_LANG9 =    198;
$KEY_PLAYCD =   200;
$KEY_PAUSECD =  201;
$KEY_PROG3 =    202;
$KEY_PROG4 =    203;
$KEY_SUSPEND =  205;
$KEY_CLOSE =    206;
$KEY_PLAY =     207;
$KEY_FASTFORWARD =      208;
$KEY_BASSBOOST =        209;
$KEY_PRINT =    210;
$KEY_HP =       211;
$KEY_CAMERA =   212;
$KEY_SOUND =    213;
$KEY_QUESTION = 214;
$KEY_EMAIL =    215;
$KEY_CHAT =     216;
$KEY_SEARCH =   217;
$KEY_CONNECT =  218;
$KEY_FINANCE =  219;
$KEY_SPORT =    220;
$KEY_SHOP =     221;
$KEY_ALTERASE = 222;
$KEY_CANCEL =   223;
$KEY_BRIGHTNESSDOWN =   224;
$KEY_BRIGHTNESSUP =     225;
$KEY_MEDIA =    226;
$KEY_UNKNOWN =  240;
$KEY_OK =       0x160;
$KEY_SELECT =   0x161;
$KEY_GOTO =     0x162;
$KEY_CLEAR =    0x163;
$KEY_POWER2 =   0x164;
$KEY_OPTION =   0x165;
$KEY_INFO =     0x166;
$KEY_TIME =     0x167;
$KEY_VENDOR =   0x168;
$KEY_ARCHIVE =  0x169;
$KEY_PROGRAM =  0x16a;
$KEY_CHANNEL =  0x16b;
$KEY_FAVORITES =        0x16c;
$KEY_EPG =      0x16d;
$KEY_PVR =      0x16e;
$KEY_MHP =      0x16f;
$KEY_LANGUAGE = 0x170;
$KEY_TITLE =    0x171;
$KEY_SUBTITLE = 0x172;
$KEY_ANGLE =    0x173;
$KEY_ZOOM =     0x174;
$KEY_MODE =     0x175;
$KEY_KEYBOARD = 0x176;
$KEY_SCREEN =   0x177;
$KEY_PC =       0x178;
$KEY_TV =       0x179;
$KEY_TV2 =      0x17a;
$KEY_VCR =      0x17b;
$KEY_VCR2 =     0x17c;
$KEY_SAT =      0x17d;
$KEY_SAT2 =     0x17e;
$KEY_CD =       0x17f;
$KEY_TAPE =     0x180;
$KEY_RADIO =    0x181;
$KEY_TUNER =    0x182;
$KEY_PLAYER =   0x183;
$KEY_TEXT =     0x184;
$KEY_DVD =      0x185;
$KEY_AUX =      0x186;
$KEY_MP3 =      0x187;
$KEY_AUDIO =    0x188;
$KEY_VIDEO =    0x189;
$KEY_DIRECTORY =        0x18a;
$KEY_LIST =     0x18b;
$KEY_MEMO =     0x18c;
$KEY_CALENDAR = 0x18d;
$KEY_RED =      0x18e;
$KEY_GREEN =    0x18f;
$KEY_YELLOW =   0x190;
$KEY_BLUE =     0x191;
$KEY_CHANNELUP =        0x192;
$KEY_CHANNELDOWN =      0x193;
$KEY_FIRST =    0x194;
$KEY_LAST =     0x195;
$KEY_AB =       0x196;
$KEY_NEXT =     0x197;
$KEY_RESTART =  0x198;
$KEY_SLOW =     0x199;
$KEY_SHUFFLE =  0x19a;
$KEY_BREAK =    0x19b;
$KEY_PREVIOUS = 0x19c;
$KEY_DIGITS =   0x19d;
$KEY_TEEN =     0x19e;
$KEY_TWEN =     0x19f;
$KEY_DEL_EOL =  0x1c0;
$KEY_DEL_EOS =  0x1c1;
$KEY_INS_LINE = 0x1c2;
$KEY_DEL_LINE = 0x1c3;
$KEY_MAX =      0x1ff;


        $key_lookup{XK_Escape}  = $KEY_ESC;
        $key_lookup{XK_1}               = $KEY_1;
        $key_lookup{XK_2}               = $KEY_2;
        $key_lookup{XK_3}               = $KEY_3;
        $key_lookup{XK_4}               = $KEY_4;
        $key_lookup{XK_5}               = $KEY_5;
        $key_lookup{XK_6}               = $KEY_6;
        $key_lookup{XK_7}               = $KEY_7;
        $key_lookup{XK_8}               = $KEY_8;
        $key_lookup{XK_9}               = $KEY_9;
        $key_lookup{XK_0}               = $KEY_0;
        $key_lookup{XK_exclam}  = $KEY_1;
        $key_lookup{XK_at}              = $KEY_2;
        $key_lookup{XK_numbersign}      = $KEY_3;
        $key_lookup{XK_dollar}  = $KEY_4;
        $key_lookup{XK_percent} = $KEY_5;
        $key_lookup{XK_asciicircum}     = $KEY_6;
        $key_lookup{XK_ampersand}       = $KEY_7;
        $key_lookup{XK_asterisk}        = $KEY_8;
        $key_lookup{XK_parenleft}       = $KEY_9;
        $key_lookup{XK_parenright}      = $KEY_0;
        $key_lookup{XK_minus}   = $KEY_MINUS;
        $key_lookup{XK_underscore}      = $KEY_MINUS;
        $key_lookup{XK_equal}   = $KEY_EQUAL;
        $key_lookup{XK_plus}    = $KEY_EQUAL;
        $key_lookup{XK_BackSpace}       = $KEY_BACKSPACE;
        $key_lookup{XK_Tab}             = $KEY_TAB;
        $key_lookup{XK_q}               = $KEY_Q;
        $key_lookup{XK_Q}               = $KEY_Q;
        $key_lookup{XK_w}               = $KEY_W;
        $key_lookup{XK_W}               = $KEY_W;
        $key_lookup{XK_e}               = $KEY_E;
        $key_lookup{XK_E}               = $KEY_E;
        $key_lookup{XK_r}               = $KEY_R;
        $key_lookup{XK_R}               = $KEY_R;
        $key_lookup{XK_t}               = $KEY_T;
        $key_lookup{XK_T}               = $KEY_T;
        $key_lookup{XK_y}               = $KEY_Y;
        $key_lookup{XK_Y}               = $KEY_Y;
        $key_lookup{XK_u}               = $KEY_U;
        $key_lookup{XK_U}               = $KEY_U;
        $key_lookup{XK_i}               = $KEY_I;
        $key_lookup{XK_I}               = $KEY_I;
        $key_lookup{XK_o}               = $KEY_O;
        $key_lookup{XK_O}               = $KEY_O;
        $key_lookup{XK_p}               = $KEY_P;
        $key_lookup{XK_P}               = $KEY_P;
        $key_lookup{XK_braceleft}       = $KEY_LEFTBRACE;
        $key_lookup{XK_braceright}      = $KEY_RIGHTBRACE;
        $key_lookup{XK_bracketleft}     = $KEY_LEFTBRACE;
        $key_lookup{XK_bracketright}    = $KEY_RIGHTBRACE;
        $key_lookup{XK_Return}  = $KEY_ENTER;
        $key_lookup{XK_Control_L}       = $KEY_LEFTCTRL;
        $key_lookup{XK_a}               = $KEY_A;
        $key_lookup{XK_A}               = $KEY_A;
        $key_lookup{XK_s}               = $KEY_S;
        $key_lookup{XK_S}               = $KEY_S;
        $key_lookup{XK_d}               = $KEY_D;
        $key_lookup{XK_D}               = $KEY_D;
        $key_lookup{XK_f}               = $KEY_F;
        $key_lookup{XK_F}               = $KEY_F;
        $key_lookup{XK_g}               = $KEY_G;
        $key_lookup{XK_G}               = $KEY_G;
        $key_lookup{XK_h}               = $KEY_H;
        $key_lookup{XK_H}               = $KEY_H;
        $key_lookup{XK_j}               = $KEY_J;
        $key_lookup{XK_J}               = $KEY_J;
        $key_lookup{XK_k}               = $KEY_K;
        $key_lookup{XK_K}               = $KEY_K;
        $key_lookup{XK_l}               = $KEY_L;
        $key_lookup{XK_L}               = $KEY_L;
        $key_lookup{XK_semicolon}       = $KEY_SEMICOLON;
        $key_lookup{XK_colon}   = $KEY_SEMICOLON;
        $key_lookup{XK_apostrophe}      = $KEY_APOSTROPHE;
        $key_lookup{XK_quotedbl}        = $KEY_APOSTROPHE;
        $key_lookup{XK_grave}   = $KEY_GRAVE;
        $key_lookup{XK_asciitilde}      = $KEY_GRAVE;
        $key_lookup{XK_Shift_L} = $KEY_LEFTSHIFT;
        $key_lookup{XK_backslash}       = $KEY_BACKSLASH;
        $key_lookup{XK_bar}             = $KEY_BACKSLASH;
        $key_lookup{XK_z}               = $KEY_Z;
        $key_lookup{XK_Z}               = $KEY_Z;
        $key_lookup{XK_x}               = $KEY_X;
        $key_lookup{XK_X}               = $KEY_X;
        $key_lookup{XK_c}               = $KEY_C;
        $key_lookup{XK_C}               = $KEY_C;
        $key_lookup{XK_v}               = $KEY_V;
        $key_lookup{XK_V}               = $KEY_V;
        $key_lookup{XK_b}               = $KEY_B;
        $key_lookup{XK_B}               = $KEY_B;
        $key_lookup{XK_n}               = $KEY_N;
        $key_lookup{XK_N}               = $KEY_N;
        $key_lookup{XK_m}               = $KEY_M;
        $key_lookup{XK_M}               = $KEY_M;
        $key_lookup{XK_comma}   = $KEY_COMMA;
        $key_lookup{XK_less}    = $KEY_COMMA;
        $key_lookup{XK_period}  = $KEY_DOT;
        $key_lookup{XK_greater} = $KEY_DOT;
        $key_lookup{XK_slash}   = $KEY_SLASH;
        $key_lookup{XK_question}        = $KEY_SLASH;
        $key_lookup{XK_Shift_R} = $KEY_RIGHTSHIFT;
        $key_lookup{XK_KP_Multiply}     = $KEY_KPASTERISK;
        $key_lookup{XK_Alt_L}   = $KEY_LEFTALT;
        $key_lookup{XK_space}   = $KEY_SPACE;
        $key_lookup{XK_Caps_Lock}       = $KEY_CAPSLOCK;
        $key_lookup{XK_F1}              = $KEY_F1;
        $key_lookup{XK_F2}              = $KEY_F2;
        $key_lookup{XK_F3}              = $KEY_F3;
        $key_lookup{XK_F4}              = $KEY_F4;
        $key_lookup{XK_F5}              = $KEY_F5;
        $key_lookup{XK_F6}              = $KEY_F6;
        $key_lookup{XK_F7}              = $KEY_F7;
        $key_lookup{XK_F8}              = $KEY_F8;
        $key_lookup{XK_F9}              = $KEY_F9;
        $key_lookup{XK_F10}             = $KEY_F10;
        $key_lookup{XK_Num_Lock}        = $KEY_NUMLOCK;
        $key_lookup{XK_Scroll_Lock}     = $KEY_SCROLLLOCK;
        $key_lookup{XK_KP_7}            = $KEY_KP7;
        $key_lookup{XK_KP_8}            = $KEY_KP8;
        $key_lookup{XK_KP_9}            = $KEY_KP9;
        $key_lookup{XK_KP_Subtract}     = $KEY_KPMINUS;
        $key_lookup{XK_KP_4}            = $KEY_KP4;
        $key_lookup{XK_KP_5}            = $KEY_KP5;
        $key_lookup{XK_KP_6}            = $KEY_KP6;
        $key_lookup{XK_KP_Add}  = $KEY_KPPLUS;
        $key_lookup{XK_KP_1}            = $KEY_KP1;
        $key_lookup{XK_KP_2}            = $KEY_KP2;
        $key_lookup{XK_KP_3}            = $KEY_KP3;
        $key_lookup{XK_KP_0}            = $KEY_KP0;
        $key_lookup{XK_KP_Decimal}      = $KEY_KPDOT;
        $key_lookup{XK_F13}             = $KEY_F13;
        $key_lookup{XK_F11}             = $KEY_F11;
        $key_lookup{XK_F12}             = $KEY_F12;
        $key_lookup{XK_F14}             = $KEY_F14;
        $key_lookup{XK_F15}             = $KEY_F15;
        $key_lookup{XK_F16}             = $KEY_F16;
        $key_lookup{XK_F17}             = $KEY_F17;
        $key_lookup{XK_F18}             = $KEY_F18;
        $key_lookup{XK_F19}             = $KEY_F19;
        $key_lookup{XK_F20}             = $KEY_F20;
        $key_lookup{XK_KP_Enter}        = $KEY_KPENTER;
        $key_lookup{XK_Control_R}       = $KEY_RIGHTCTRL;
        $key_lookup{XK_KP_Divide}       = $KEY_KPSLASH;
        $key_lookup{XK_Sys_Req} = $KEY_SYSRQ;
        $key_lookup{XK_Alt_R}   = $KEY_RIGHTALT;
        $key_lookup{XK_Linefeed}        = $KEY_LINEFEED;
        $key_lookup{XK_Home}            = $KEY_HOME;
        $key_lookup{XK_Up}              = $KEY_UP;
        $key_lookup{XK_Page_Up} = $KEY_PAGEUP;
        $key_lookup{XK_Left}            = $KEY_LEFT;
        $key_lookup{XK_Right}   = $KEY_RIGHT;
        $key_lookup{XK_End}             = $KEY_END;
        $key_lookup{XK_Down}            = $KEY_DOWN;
        $key_lookup{XK_Page_Down}       = $KEY_PAGEDOWN;
        $key_lookup{XK_Insert}  = $KEY_INSERT;
        $key_lookup{XK_Delete}  = $KEY_DELETE;
        $key_lookup{XK_KP_Equal}        = $KEY_KPEQUAL;
        $key_lookup{XK_Pause}   = $KEY_PAUSE;
        $key_lookup{XK_F21}             = $KEY_F21;
        $key_lookup{XK_F22}             = $KEY_F22;
        $key_lookup{XK_F23}             = $KEY_F23;
        $key_lookup{XK_F24}             = $KEY_F24;
        $key_lookup{XK_KP_Separator}    = $KEY_KPCOMMA;
        $key_lookup{XK_Meta_L}  = $KEY_LEFTMETA;
        $key_lookup{XK_Meta_R}  = $KEY_RIGHTMETA;
        $key_lookup{XK_Multi_key}       = $KEY_COMPOSE;

$ABS_MAX = 63;

$UI_DEV_CREATE  = 0x5501;
$UI_DEV_DESTROY = 0x5502;
$UI_SET_EVBIT   = 0x40045564;
$UI_SET_KEYBIT  = 0x40045565;
$UI_SET_RELBIT  = 0x40045566;
$UI_SET_ABSBIT  = 0x40045567;

# FIXME: time hires, etc.
$linux_gettimeofday_syscall = 78;

$O_RDONLY = 00;
$O_WRONLY = 01;
$O_RDWR   = 02;
$O_NDELAY = 04000;

}
