#!/usr/bin/wish

proc check_callback {} {
	global debug
	if {$debug} {
		puts stderr "."
	}
	check_closed
	after 1000 check_callback 
}

proc getout {} {
	global client_fh server_fh

	set delay 50
	catch {flush $client_fh}
	after $delay
	catch {close $client_fh}
	set client_fh ""
	after $delay
	catch {flush $server_fh}
	after $delay
	catch {close $server_fh}
	set server_fh ""
	after $delay

	global bmesg_cnt
	if [info exists bmesg_cnt] {
		catch {tkwait window .bmesg$bmesg_cnt}
	}
	destroy .
	exit
}

proc check_closed {} {
	global got_connection debug
	global client_fh server_fh

	if {! $got_connection} {
		return
	}
	if {$client_fh != ""} {
		set ef ""
		catch {set ef [eof $client_fh]}
		if {$ef == 1} {
			if {$debug} {
				puts stderr "client_fh EOF"
			}
			getout
		}
	}
	if {$server_fh != ""} {
		set ef ""
		catch {set ef [eof $server_fh]}
		if {$ef == 1} {
			if {$debug} {
				puts stderr "server_fh EOF"
			}
			getout
		}
	}
}

proc xfer_in_to_out {} {
	global client_fh server_fh debug do_bridge
	if {$client_fh != "" && ![eof $client_fh]} {
		set ef ""
		catch {set ef [eof $client_fh]}
		if {$ef == 0} {
			set str ""
			catch {set str [read $client_fh 4096]}
			if {$debug} {
				#puts stderr "xfer_in_to_out: $str"
				puts stderr "xfer_in_to_out: [string length $str]"
			}
			if {$server_fh != "" && $str != ""} {
				catch {puts -nonewline $server_fh $str}
				catch {flush $server_fh}
			}
		}
	}
	check_closed
}

proc xfer_out_to_in {} {
	global client_fh server_fh debug do_bridge
	if {$server_fh != ""} {
		set ef ""
		catch {set ef [eof $server_fh]}
		if {$ef == 0} {
			set str ""
			catch {set str [read $server_fh 4096]}
			if {$debug} {
				#puts stderr "xfer_out_to_in: $str"
				puts stderr "xfer_out_to_in: [string length $str]"
			}
			if {$client_fh != "" && $str != ""} {
				catch {puts -nonewline $client_fh $str}
				catch {flush $client_fh}
			}
		}
	}
	check_closed
}

proc bmesg {msg} {
	global env
	if {! [info exists env(BMESG)]} {
		return
	}
	if {$env(BMESG) == 0} {
		return
	}

	global bmesg_cnt
	if {! [info exists bmesg_cnt]} {
		set bmesg_cnt 0
	}
	incr bmesg_cnt
	set w .bmesg$bmesg_cnt
	catch {destroy $w}
	toplevel $w
	label $w.l -width 70 -text "$msg"
	pack $w.l
	update
	if {$env(BMESG) > 1} {
		for {set i 0} {$i < $env(BMESG)} {incr i} {
			after 1000
			update
		}
	}
}

proc do_connect_http {sock hostport which} {
	global debug cur_proxy
	set con ""
	append con "CONNECT $hostport HTTP/1.1\r\n"
	append con "Host: $hostport\r\n"
	append con "Connection: close\r\n\r\n"

	puts stderr "pxy=$which CONNECT $hostport HTTP/1.1 via $cur_proxy"
	bmesg "H: $which CONNECT $hostport HTTP/1.1 $cur_proxy";

	puts -nonewline $sock $con
	flush $sock

	set r ""
	set cnt 0
	while {1} {
		incr cnt
		set c [read $sock 1]
		if {$c == ""} {
			check_closed
			after 20
		}
		append r $c
		if {[regexp "\r\n\r\n" $r] || [regexp "a--no--\n\n" $r]} {
			break
		}
		if {$cnt > 30000} {
			break
		}
	}
	if {! [regexp {HTTP/.* 200} $r]} {
		puts stderr "did not find HTTP 200 #1"
		destroy .
		exit 1
	}
}

proc do_connect_socks4 {sock hostport which} {
	global debug cur_proxy

	set host ""
	set port ""
	if [regexp {^(.*):([0-9][0-9]*)$} $hostport mvar host port] {
		;
	} else {
		puts stderr "could not parse host:port $hostport"
		destroy .
		exit 1
	}

	set i1 ""
	set i2 ""
	set i3 ""
	set i4 ""

	set socks4a 0

	if {$host == "localhost" || $host == "127.0.0.1"} {
		set i1 127
		set i2 0
		set i3 0
		set i4 1
		
	} elseif [regexp {^[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$} $host] {
		set n [split $host "."]
		set i1 [lindex $n 0]
		set i2 [lindex $n 1]
		set i3 [lindex $n 2]
		set i4 [lindex $n 3]
	} else {
		set i1 0
		set i2 0
		set i3 0
		set i4 3
		
		set socks4a 1
	}

	if {$socks4a} {
		puts stderr "pxy=$which socks4a connection to $host:$port via $cur_proxy"
	} else {
		puts stderr "pxy=$which socks4  connection to $host:$port via $cur_proxy"
	}

	set p1 [binary format ccScccc 4 1 $port $i1 $i2 $i3 $i4]
	set p2 "nobody"
	set p3 [binary format c 0]

	puts -nonewline $sock $p1
	puts -nonewline $sock $p2
	puts -nonewline $sock $p3
	if {$socks4a} {
		puts -nonewline $sock $host
		puts -nonewline $sock $p3
	}
	flush $sock

	set r ""; set s ""; set i 0; set cnt 0
	set ok 1
	while {$cnt < 30000 && $i < 8} {
		incr cnt
		set c [read $sock 1]
		if {$c == ""} {
			check_closed
			after 20
			continue
		}
		
		binary scan $c c s
		if {$i == 0 && $s != 0} {
			puts stderr "socks4: $i - $s"
			set ok 0
		}
		if {$i == 1 && $s != 90} {
			puts stderr "socks4: $i - $s"
			set ok 0
		}
		set r "$r,$s"
		incr i
	}
	if {! $ok} {
		puts stderr "socks4 failure: $r"
		destroy .
		exit 1
	}
}

proc do_connect_socks5 {sock hostport which} {
	global debug cur_proxy

	set host ""
	set port ""
	if [regexp {^(.*):([0-9][0-9]*)$} $hostport mvar host port] {
		;
	} else {
		puts stderr "could not parse host:port $hostport"
		destroy .
		exit 1
	}

	set p1 [binary format ccc 5 1 0]
	puts -nonewline $sock $p1
	flush $sock

	set r ""; set s ""; set i 0; set cnt 0
	set ok 1
	while {$cnt < 30000 && $i < 2} {
		incr cnt
		set c [read $sock 1]
		if {$c == ""} {
			check_closed
			after 20
			continue
		}
		
		binary scan $c c s
		if {$i == 0 && $s != 5} {
			puts stderr "$i - $s"
			set ok 0
		}
		if {$i == 1 && $s != 0} {
			puts stderr "$i - $s"
			set ok 0
		}
		set r "$r,$s"
		incr i
	}
	if {! $ok} {
		puts stderr "socks5 failure: $r"
		destroy .
		exit 1
	}

	set len [string length $host]
	set p1 [binary format ccccc 5 1 0 3 $len]
	set p2 $host

	set n1 [expr int($port/256)]
	set n2 [expr "$port - $n1 * 256"]
	set p3 [binary format cc $n1 $n2]

	puts stderr "pxy=$which socks5  connection to $host:$port via $cur_proxy"

	puts -nonewline $sock $p1
	puts -nonewline $sock $p2
	puts -nonewline $sock $p3
	flush $sock

	set i1 ""; set i2 ""; set i3 ""; set i4 ""
	set r ""; set s ""; set i 0; set cnt 0
	set ok 1
	while {$cnt < 30000 && $i < 4} {
		incr cnt
		set c [read $sock 1]
		if {$c == ""} {
			check_closed
			after 20
			continue
		}
		
		binary scan $c c s
		if {$i == 0} {
			set i1 $s
		} elseif {$i == 1} {
			set i2 $s
		} elseif {$i == 2} {
			set i3 $s
		} elseif {$i == 3} {
			set i4 $s
		}
		incr i
	}
	set r "i1=$i1,i2=$i2,i3=$i3,i4=$i4"

	if {$i4 == 1} {
		set n 6
	} elseif {$i4 == 3} {
		set c ""
		for {set i 0} {$i < 1000} {incr i} {
			set c [read $sock 1]
			if {$c == ""} {
				check_closed
				after 20
				continue
			}
			break;
		}
		if {$c == ""} {
			puts stderr "socks5 failure c: $r"
			destroy .
			exit 1
		}
		binary scan $c c s
		set n [expr $s + 2] 
	} elseif {$i4 == 4} {
		set n 18 
	} else {
		puts stderr "socks5 failure x: $r"
		destroy .
		exit 1
	}
	#puts "n=$n --- $r"

	set i 0; set cnt 0
	while {$cnt < 30000 && $i < $n} {
		incr cnt
		set c [read $sock 1]
		if {$c == ""} {
			check_closed
			after 20
			continue
		}
		incr i
	}
	if {$i1 != 5 || $i2 != 0 || $i3 != 0} {
		puts stderr "socks failure $r"
		destroy .
		exit 1
	}
}

proc do_connect_repeater {sock hostport which repeater} {
	global debug cur_proxy

	# 250 is UltraVNC buffer size.
	set con [binary format a250 $repeater]

	puts stderr "pxy=$which REPEATER $repeater via $cur_proxy"
	bmesg "R: $which CONNECT $hostport | $repeater $cur_proxy";

	puts -nonewline $sock $con
	flush $sock

	set r ""
	set cnt 0
	while {1} {
		incr cnt
		set c [read $sock 1]
		if {$c == ""} {
			check_closed
			after 20
		}
		append r $c
		if {[string length $r] >= 12} {
			puts stderr "do_connect_repeater: $r"
			break
		}
		if {$cnt > 30000} {
			break
		}
	}
}

proc vread {n sock} {
	set str ""
	set max 3000
	set dt 10
	set i 0
	set cnt 0
	while {$cnt < $max && $i < $n} {
		incr cnt
		set c [read $sock 1]
		if {$c == ""} {
			check_closed
			after $dt
			continue
		}
		incr i
		append str $c
	}
	if {$i != $n} {
		puts stderr "vread failure $n  $i"
		destroy .; exit 1
	}
	return $str
}

proc append_handshake {str} {
	global env
	if [info exists env(SSVNC_PREDIGESTED_HANDSHAKE)] {
		set file $env(SSVNC_PREDIGESTED_HANDSHAKE)
		set fh ""
		catch {set fh [open $file a]}
		if {$fh != ""} {
			puts $fh $str
			catch {close $fh}
		}
	}
}

proc vencrypt_bridge_connection {fh host port} {
	puts stderr "vencrypt_bridge_connection: got connection $fh $host $port"
	bmesg       "vencrypt_bridge_connection: got connection $fh $host $port"
	global viewer_sock
	set viewer_sock $fh
}

proc center_win {w} {
	update
	set W [winfo screenwidth  $w]
	set W [expr $W + 1]
	wm geometry $w +$W+0
	update
	set x [expr [winfo screenwidth  $w]/2 - [winfo width  $w]/2]
	set y [expr [winfo screenheight $w]/2 - [winfo height $w]/2]

	wm geometry $w +$x+$y
	wm deiconify $w
	update
}


proc get_user_pass {} {
	global env
	set up ""
	if [info exists env(SSVNC_UNIXPW)] {
		set rm 0
		set up $env(SSVNC_UNIXPW)
		if [regexp {^rm:} $up]  {
			set rm 1
			regsub {^rm:} $up "" up
		}
		if [file exists $up] {
			set fh ""
			set f $up
			catch {set fh [open $up r]}
			if {$fh != ""} {
				gets $fh u	
				gets $fh p	
				catch {close $fh}
				set up "$u@$p"
			}
			if {$rm} {
				catch {file delete $f}
			}
		}
	} elseif [info exists env(SSVNC_VENCRYPT_USERPASS)] {
		set up $env(SSVNC_VENCRYPT_USERPASS)
	}
	if {$up != ""} {
		return $up
	}

	toplevel .t
	wm title .t {VeNCrypt Viewer Bridge User/Pass}

	global user pass
	set user ""
	set pass ""
	label .t.l -text {SSVNC VeNCrypt Viewer Bridge}

	frame .t.f0
	frame .t.f0.fL
	label .t.f0.fL.la -text {Username: }
	label .t.f0.fL.lb -text {Password: }

	pack .t.f0.fL.la .t.f0.fL.lb -side top

	frame .t.f0.fR
	entry .t.f0.fR.ea -width 24 -textvariable user
	entry .t.f0.fR.eb -width 24 -textvariable pass -show *

	pack .t.f0.fR.ea .t.f0.fR.eb -side top -fill x

	pack .t.f0.fL -side left
	pack .t.f0.fR -side right -expand 1 -fill x

	button .t.no -text Cancel -command {set user ""; set pass ""; destroy .t}
	button .t.ok -text Done   -command {destroy .t}

	center_win .t
	pack .t.l .t.f0 .t.no .t.ok -side top -fill x
	update
	wm deiconify .t

	bind .t.f0.fR.ea <Return> {focus .t.f0.fR.eb}
	bind .t.f0.fR.eb <Return> {destroy .t}
	focus .t.f0.fR.ea

	wm resizable .t 1 0
	wm minsize .t [winfo reqwidth .t] [winfo reqheight .t]

	tkwait window .t
	if {$user == "" || $pass == ""} {
		return ""
	} else {
		return "$user@$pass"
	}
}

proc do_vencrypt_viewer_bridge {listen connect} {
	global env

	#set env(BMESG) 1

	vencrypt_constants

	set backwards 0

	if {! [info exists env(SSVNC_PREDIGESTED_HANDSHAKE)]} {
		puts stderr "no SSVNC_PREDIGESTED_HANDSHAKE filename in environment."	
		destroy .; exit 1
	}
	set handshake $env(SSVNC_PREDIGESTED_HANDSHAKE)
	bmesg $handshake

	if {$listen < 0} {
		set backwards 1
		set listen [expr -$listen]
	}

	# listen on $listen	
	global viewer_sock
	set viewer_sock ""
	set lsock ""
	set rc [catch {set lsock [socket -myaddr 127.0.0.1 -server vencrypt_bridge_connection $listen]}]
	if {$rc != 0} {
		puts stderr "error listening on 127.0.0.1:$listen"	
		destroy .; exit 1
	}
	bmesg "listen on $listen OK"

	# accept
	vwait viewer_sock
	catch {close $lsock}
	fconfigure $viewer_sock -translation binary -blocking 0

	global got_connection
	set got_connection 1

	# connect to $connect
	set server_sock ""
	set rc [catch {set server_sock [socket 127.0.0.1 $connect]}]
	if {$rc != 0} {
		puts stderr "error connecting to 127.0.0.1:$connect"	
		destroy .; exit 1
	}
	bmesg "made connection to $connect"
	fconfigure $server_sock -translation binary -blocking 0

	if {$backwards} {
		puts stderr "reversing roles of viewer and server"
		set t $viewer_sock
		set viewer_sock $server_sock
		set server_sock $t
	}

	# wait for SSVNC_PREDIGESTED_HANDSHAKE "done", put in hash.
	set dt 200
	set slept 0
	set maxwait 20000
	set hs(mode) init 
	while {$slept < $maxwait} {
		after $dt
		set slept [expr $slept + $dt]
		set done 0
		set fh ""
		catch {set fh [open $handshake r]}
		set str ""
		if {$fh != ""} {
			array unset hs 
			while {[gets $fh line] > -1} {
				set line [string trim $line]
				set str "$str$line\n";
				if {$line == "done"} {
					set done 1
				} elseif [regexp {=} $line] {
					set s [split $line "="]
					set key [lindex $s 0]
					set val [lindex $s 1]
					set hs($key) $val
				}
			}
			catch {close $fh}
		}
		if {$done} {
			puts stderr $str
			bmesg "$str"
			break
		}
	}

	catch [file delete $handshake]

	if {! [info exists hs(sectype)]} {
		puts stderr "no hs(sectype) found"	
		destroy .; exit 1
	}

	# read viewer RFB
	if {! [info exists hs(server)]} {
		set hs(server) "RFB 003.008"
	}
	puts -nonewline $viewer_sock "$hs(server)\n"
	flush $viewer_sock
	puts stderr "sent $hs(server) to viewer sock."

	set viewer_rfb [vread 12 $viewer_sock]
	puts stderr "read viewer_rfb $viewer_rfb"

	set viewer_major 3 
	set viewer_minor 8 
	if [regexp {^RFB 003\.0*([0-9][0-9]*)} $viewer_rfb m v] {
		set viewer_minor $v
	}

	if {$hs(sectype) == $rfbSecTypeAnonTls} {
		puts stderr "handling rfbSecTypeAnonTls"
		if {$viewer_major > 3 || $viewer_minor >= 7} {
			puts stderr "viewer >= 3.7, nothing to set up."
		} else {
			puts stderr "viewer <= 3.3, faking things up."
			set t [vread 1 $server_sock]
			binary scan $t c nsectypes
			puts stderr "nsectypes=$nsectypes"
			for {set i 0} {$i < $nsectypes} {incr i} {
				set t [vread 1 $server_sock]
				binary scan $t c st
				puts stderr "   $i: $st"
				set types($st) $i
			}
			set use 1
			if [info exists types(1)] {
				set use 1
			} elseif [info exists types(2)] {
				set use 2
			} else {
				puts stderr "no valid sectypes"	
				destroy .; exit 1
			}
			# this should be MSB:
			vsend_uchar $viewer_sock 0
			vsend_uchar $viewer_sock 0
			vsend_uchar $viewer_sock 0
			vsend_uchar $viewer_sock $use

			vsend_uchar $server_sock $use
			if {$use == 1} {
				set t [vread 4 $server_sock]
			}
		}
	} elseif {$hs(sectype) == $rfbSecTypeVencrypt} {
		puts stderr "handling rfbSecTypeVencrypt"
		if {! [info exists hs(subtype)]} {
			puts stderr "no subtype"	
			destroy .; exit 1
		}
		set fake_type "None"
		set plain 0

		set sub_type $hs(subtype)


		if {$sub_type == $rfbVencryptTlsNone} {
			set fake_type "None"
		} elseif {$sub_type == $rfbVencryptTlsVnc} {
			set fake_type "VncAuth"
		} elseif {$sub_type == $rfbVencryptTlsPlain} {
			set fake_type "None"
			set plain 1
		} elseif {$sub_type == $rfbVencryptX509None} {
			set fake_type "None"
		} elseif {$sub_type == $rfbVencryptX509Vnc} {
			set fake_type "VncAuth"
		} elseif {$sub_type == $rfbVencryptX509Plain} {
			set fake_type "None"
			set plain 1
		}

		if {$plain} {
			set up [get_user_pass]
			if [regexp {@} $up] {
				set user $up
				set pass $up
				regsub {@.*$}  $user "" user
				regsub {^[^@]*@} $pass "" pass
				vsend_uchar $server_sock 0
				vsend_uchar $server_sock 0
				vsend_uchar $server_sock 0
				vsend_uchar $server_sock [string length $user]
				vsend_uchar $server_sock 0
				vsend_uchar $server_sock 0
				vsend_uchar $server_sock 0
				vsend_uchar $server_sock [string length $pass]
				puts stderr "sending VencryptPlain user and pass."
				puts -nonewline $server_sock $user
				puts -nonewline $server_sock $pass
				flush $server_sock
			}
		}
		set ft 0
		if {$fake_type == "None"} {
			set ft 1
		} elseif {$fake_type == "VncAuth"} {
			set ft 2
		} else {
			puts stderr "no valid fake_type"	
			destroy .; exit 1
		}

		if {$viewer_major > 3 || $viewer_minor >= 7} {
			vsend_uchar $viewer_sock 1
			vsend_uchar $viewer_sock $ft
			set t [vread 1 $viewer_sock]
			binary scan $t c cr
			if {$cr != $ft} {
				puts stderr "client selected wront type $cr $ft"	
				destroy .; exit 1
			}
		} else {
			puts stderr "viewer <= 3.3, faking things up."
			# this should be MSB:
			vsend_uchar $viewer_sock 0
			vsend_uchar $viewer_sock 0
			vsend_uchar $viewer_sock 0
			vsend_uchar $viewer_sock $ft

			if {$ft == 1} {
				set t [vread 4 $server_sock]
			}
		}
	}

	global client_fh server_fh
	set client_fh $viewer_sock
	set server_fh $server_sock

	fileevent $client_fh readable xfer_in_to_out
	fileevent $server_fh readable xfer_out_to_in
}

proc vsend_uchar {sock n} {
	set s [binary format c $n]
	puts -nonewline $sock $s
	flush $sock
}

proc vencrypt_constants {} {
	uplevel {
		set rfbSecTypeAnonTls  18
		set rfbSecTypeVencrypt 19

		set rfbVencryptPlain        256
		set rfbVencryptTlsNone      257
		set rfbVencryptTlsVnc       258
		set rfbVencryptTlsPlain     259
		set rfbVencryptX509None     260
		set rfbVencryptX509Vnc      261
		set rfbVencryptX509Plain    262
	}
}

proc do_vencrypt {sock which} {

	vencrypt_constants

	set t [vread 1 $sock]
	binary scan $t c vs_major
	set t [vread 1 $sock]
	binary scan $t c vs_minor

	if {$vs_minor == "" || $vs_major == "" || $vs_major != 0 || $vs_minor < 2} {
		puts stderr "vencrypt failure bad vs version major=$major minor=$minor"
		destroy .; exit 1
	}
	puts stderr "server vencrypt version $vs_major.$vs_minor"
	bmesg "server vencrypt version $vs_major.$vs_minor"

	append_handshake "subversion=0.2"
	vsend_uchar $sock 0
	vsend_uchar $sock 2

	set t [vread 1 $sock]
	binary scan $t c result
	if {$result != 0} {
		puts stderr "vencrypt failed result: $result"
		bmesg "vencrypt failed result: $result"
		destroy .; exit 1
	}

	set t [vread 1 $sock]
	binary scan $t c nsubtypes
	puts stderr "nsubtypes: $nsubtypes"
	bmesg "nsubtypes: $nsubtypes"

	for {set i 0} {$i < $nsubtypes} {incr i} {
		set t [vread 4 $sock]
		binary scan $t I stype
		puts stderr "subtypes: $i: $stype"
		append_handshake "sst$i=$stype"
		set subtypes($stype) $i
	}

	set subtype 0
	if [info exists subtypes($rfbVencryptX509None)] {
		set subtype $rfbVencryptX509None
		puts stderr "selected rfbVencryptX509None"
	} elseif [info exists subtypes($rfbVencryptX509Vnc)] {
		set subtype $rfbVencryptX509Vnc
		puts stderr "selected rfbVencryptX509Vnc"
	} elseif [info exists subtypes($rfbVencryptX509Plain)] {
		set subtype $rfbVencryptX509Plain
		puts stderr "selected rfbVencryptX509Plain"
	} elseif [info exists subtypes($rfbVencryptTlsNone)] {
		set subtype $rfbVencryptTlsNone
		puts stderr "selected rfbVencryptTlsNone"
	} elseif [info exists subtypes($rfbVencryptTlsVnc)] {
		set subtype $rfbVencryptTlsVnc
		puts stderr "selected rfbVencryptTlsVnc"
	} elseif [info exists subtypes($rfbVencryptTlsPlain)] {
		set subtype $rfbVencryptTlsPlain
		puts stderr "selected rfbVencryptTlsPlain"
	}
	append_handshake "subtype=$subtype"
	set st [binary format I $subtype]
	puts -nonewline $sock $st
	flush $sock
	
	if {$subtype == 0} {
		puts stderr "vencrypt could not find an acceptable subtype: $subtype"
		destroy .; exit 1
	}

	set t [vread 1 $sock]
	binary scan $t c result
	puts stderr "result=$result"

	append_handshake "done"

	if {$result == 0} {
		puts stderr "vencrypt failure result: $result"
		destroy .; exit 1
	}

}

proc do_connect_vencrypt {sock hostport which} {
	global debug cur_proxy

	vencrypt_constants

	puts stderr "pxy=$which vencrypt $hostport via $cur_proxy"
	bmesg "V: $which vencrypt $hostport via $cur_proxy"

	append_handshake "mode=connect"

	set srfb [vread 12 $sock]
	puts stderr "srfb: $srfb"
	bmesg "srfb: $srfb"
	set srfb [string trim $srfb]
	append_handshake "server=$srfb"

	set minor ""
	if [regexp {^RFB 00[456]\.} $srfb] {
		set minor 8
	} elseif [regexp {^RFB 003\.0*([0-9][0-9]*)} $srfb mvar minor] {
		;
	}
	if {$minor == "" || $minor < 7} {
		puts stderr "vencrypt failure bad minor=$minor"
		destroy .; exit 1
	}

	set vrfb "RFB 003.008\n"
	if {$minor == 7} {
		set vrfb "RFB 003.007\n"
	}
	puts -nonewline $sock $vrfb
	flush $sock

	set vrfb [string trim $vrfb] 
	append_handshake "viewer=$vrfb"
	append_handshake "latency=0.10"

	set str [vread 1 $sock]
	binary scan $str c nsec
	puts stderr "nsec: $nsec"
	bmesg "nsec: $nsec"
	for {set i 0} {$i < $nsec} {incr i} {
		set str [vread 1 $sock]
		binary scan $str c sec
		puts stderr "sec: $sec"
		bmesg "sec: $sec"
		set sectypes($i) $sec
	}
	for {set i 0} {$i < $nsec} {incr i} {
		if {$sectypes($i) == $rfbSecTypeVencrypt} {
			append_handshake "sectype=$rfbSecTypeVencrypt"
			vsend_uchar $sock $rfbSecTypeVencrypt
			after 500
			bmesg "do_vencrypt $sock $which"
			do_vencrypt $sock $which
			return
		}
	}
	for {set i 0} {$i < $nsec} {incr i} {
		if {$sectypes($i) == $rfbSecTypeAnonTls} {
			append_handshake "sectype=$rfbSecTypeAnonTls"
			vsend_uchar $sock $rfbSecTypeAnonTls
			bmesg "rfbSecTypeAnonTls"
			after 500
			append_handshake "done"
			return
		}
	}
}

proc do_connect {sock type hostport which} {
	if {$type == "http"} 	{
		do_connect_http $sock $hostport $which
	} elseif {$type == "socks"} {
		do_connect_socks4 $sock $hostport $which
	} elseif {$type == "socks5"} {
		do_connect_socks5 $sock $hostport $which
	} elseif [regexp -nocase {^repeater:} $type] {
		regsub -nocase {^repeater:} $type "" repeater
		do_connect_repeater $sock $hostport $which $repeater
	} elseif {$type == "vencrypt"} {
		do_connect_vencrypt $sock $hostport $which
	}
}

proc handle_connection {fh host port} {
	global proxy1_host proxy1_port proxy1_type
	global proxy2_host proxy2_port proxy2_type
	global proxy3_host proxy3_port proxy3_type
	global proxy1 proxy2 proxy3 dest
	global debug cur_proxy
	global got_connection

	if {$got_connection} {
		catch {close $fh}
		return
	}
	set got_connection 1

	if {$debug} {
		puts stderr "connection from: $host $port"	
		puts stderr "socket $proxy1_host $proxy1_port"
	}

	set rc [catch {set sock [socket $proxy1_host $proxy1_port]}]
	if {$rc != 0} {
		puts stderr "error connecting"	
		catch {close $sock}
		destroy .
		exit
	}

	if {$debug} {
		puts stderr "got sock: $sock"	
	}
	
	global client_fh server_fh
	set client_fh $fh
	set server_fh $sock

	fconfigure $fh   -translation binary -blocking 0
	fconfigure $sock -translation binary -blocking 0

	set cur_proxy $proxy1
	if {$proxy2 != ""} {
		do_connect $sock $proxy1_type "$proxy2_host:$proxy2_port" 1

		set cur_proxy $proxy2
		if {$proxy3 != ""} {
			do_connect $sock $proxy2_type "$proxy3_host:$proxy3_port" 2

			set cur_proxy $proxy3
			do_connect $sock $proxy3_type $dest 3

		} else {
			do_connect $sock $proxy2_type $dest 2
		}
	} else {
		do_connect $sock $proxy1_type $dest 1
	}

	fileevent $fh   readable xfer_in_to_out
	fileevent $sock readable xfer_out_to_in
}

proc proxy_type {proxy} {
	if [regexp -nocase {^socks://} $proxy] {
		return "socks"
	} elseif [regexp -nocase {^socks4://} $proxy] {
		return "socks"
	} elseif [regexp -nocase {^socks4a://} $proxy] {
		return "socks"
	} elseif [regexp -nocase {^socks5://} $proxy] {
		return "socks5"
	} elseif [regexp -nocase {^http://} $proxy] {
		return "http"
	} elseif [regexp -nocase {^https://} $proxy] {
		return "http"
	} elseif [regexp -nocase {^repeater://.*\+(.*)$} $proxy mat idstr] {
		return "repeater:$idstr"
	} elseif [regexp -nocase {^vencrypt://} $proxy] {
		return "vencrypt"
	} else {
		return "http"
	}
}

proc proxy_hostport {proxy} {
	regsub -nocase {^[a-z][a-z0-9]*://} $proxy "" hp
	regsub {\+.*$} $hp "" hp
	if {! [regexp {:[0-9]} $hp] && [regexp {^repeater:} $proxy]} {
		set hp "$hp:5900"
	}
	return $hp
}

proc setb {} {
	wm withdraw .
	catch {destroy .b}
	button .b -text "CONNECT_BR" -command {destroy .}
	pack .b
	after 1000 check_callback 
}

proc connect_br_sleep {} {
	global env
	if [info exists env(CONNECT_BR_SLEEP)] {
		if [regexp {^[0-9][0-9]*$} $env(CONNECT_BR_SLEEP)] {
			setb
			for {set i 0} {$i < $env(CONNECT_BR_SLEEP)} {incr i} {
				bmesg "$i sleep"	
				after 1000
			}
		}
	}
}

global env

set got_connection 0
set proxy1 ""
set proxy2 ""
set proxy3 ""
set client_fh ""
set server_fh ""
set do_bridge 0
set debug 0

if [info exists env(CONNECT_BR_DEBUG)] {
	set debug 1
}

if [info exists env(SSVNC_VENCRYPT_VIEWER_BRIDGE)] {
	set s [split $env(SSVNC_VENCRYPT_VIEWER_BRIDGE) ","]
	set listen  [lindex $s 0]
	set connect [lindex $s 1]

	setb

	do_vencrypt_viewer_bridge $listen $connect
	set do_bridge 1
}

if {$do_bridge} {
	;
} else {
	if {$debug && 0} {
		if {! [info exists env(SSVNC_DEST)]} {
			set env(SSVNC_DEST) "haystack:2037"
		}
		if {! [info exists env(SSVNC_PROXY)]} {
			set env(SSVNC_PROXY) "haystack:2037"
		}
		if {! [info exists env(SSVNC_LISTEN)]} {
			set env(SSVNC_LISTEN) "6789"
		}
	} else {
		if {! [info exists env(SSVNC_DEST)]} {
			destroy .; exit;
		}
		if {! [info exists env(SSVNC_PROXY)]} {
			destroy .; exit;
		}
		if {! [info exists env(SSVNC_LISTEN)] && ! [info exists env(SSVNC_REVERSE)]} {
			destroy .; exit;
		}
	}

	#set env(BMESG) 1

	set dest $env(SSVNC_DEST)

	if [regexp {,} $env(SSVNC_PROXY)] {
		set s [split $env(SSVNC_PROXY) ","]
		set proxy1 [lindex $s 0]
		set proxy2 [lindex $s 1]
		set proxy3 [lindex $s 2]
	} else {
		set proxy1 $env(SSVNC_PROXY)
	}

	set proxy1_type [proxy_type     $proxy1]
	set proxy1_hp   [proxy_hostport $proxy1]

	set proxy1_host ""
	set proxy1_port ""
	if [regexp {^(.*):([0-9][0-9]*)$} $proxy1_hp mvar proxy1_host proxy1_port] {
		;
	} else {
		puts stderr "could not parse hp1 host:port $proxy1_hp"
		destroy .
		exit 1
	}

	set proxy2_type ""
	set proxy2_host ""
	set proxy2_port ""

	if {$proxy2 != ""} {
		set proxy2_type [proxy_type     $proxy2]
		set proxy2_hp   [proxy_hostport $proxy2]

		set proxy2_host ""
		set proxy2_port ""
		if [regexp {^(.*):([0-9][0-9]*)$} $proxy2_hp mvar proxy2_host proxy2_port] {
			;
		} else {
			puts stderr "could not parse hp2 host:port $proxy2_hp"
			destroy .
			exit 1
		}
	}

	set proxy3_type ""
	set proxy3_host ""
	set proxy3_port ""

	if {$proxy3 != ""} {
		set proxy3_type [proxy_type     $proxy3]
		set proxy3_hp   [proxy_hostport $proxy3]

		set proxy3_host ""
		set proxy3_port ""
		if [regexp {^(.*):([0-9][0-9]*)$} $proxy3_hp mvar proxy3_host proxy3_port] {
			;
		} else {
			puts stderr "could not parse hp3 host:port $proxy3_hp"
			destroy .
			exit 1
		}
	}

	bmesg "1: '$proxy1_host' '$proxy1_port' '$proxy1_type'";
	bmesg "2: '$proxy2_host' '$proxy2_port' '$proxy2_type'";
	bmesg "3: '$proxy3_host' '$proxy3_port' '$proxy3_type'";

	if [info exists env(SSVNC_REVERSE)] {
		set rhost ""
		set rport ""
		if [regexp {^(.*):([0-9][0-9]*)$} $env(SSVNC_REVERSE) mvar rhost rport] {
			;
		} else {
			puts stderr "could not parse SSVNC_REVERSE host:port $env(SSVNC_REVERSE)"
			destroy .
			exit 1
		}
		setb
		set rc [catch {set lsock [socket $rhost $rport]}]
		if {$rc != 0} {
			puts stderr "error reversing"	
			bmesg "1 error reversing"	
			after 2000
			set rc [catch {set lsock [socket $rhost $rport]}]
		}
		if {$rc != 0} {
			puts stderr "error reversing"	
			bmesg "2 error reversing"	
			after 2000
			set rc [catch {set lsock [socket $rhost $rport]}]
		}
		if {$rc != 0} {
			puts stderr "error reversing"	
			bmesg "3 error reversing"	
			destroy .; exit 1
		}
		puts stderr "SSVNC_REVERSE to $rhost $rport OK";
		bmesg "SSVNC_REVERSE to $rhost $rport OK";
		connect_br_sleep
		handle_connection $lsock $rhost $rport
	} else {
		set lport $env(SSVNC_LISTEN)
		connect_br_sleep
		set rc [catch {set lsock [socket -myaddr 127.0.0.1 -server handle_connection $lport]}]
		if {$rc != 0} {
			puts stderr "error listening"	
			destroy .; exit 1
		}
		puts stderr "SSVNC_LISTEN on $lport OK";
		setb
	}
}
