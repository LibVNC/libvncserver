#!/usr/bin/perl

use nacro;

$vnc=nacro::initvnc("localhost",5900,5923);

print $vnc;

# give it a chance to get a first screen update

print nacro::waitforupdate($vnc,.4);

print STDERR "Now\n";

print nacro::sendmouse($vnc,90,250,0);

print nacro::sendkey($vnc,ord('a'),-1);
print nacro::sendkey($vnc,ord('a'),0);

print nacro::sendmouse($vnc,100,10,0);

print nacro::savepnm($vnc,"hallo.pnm",50,50,300,200);

nacro::process($vnc,3);

print"\n";

