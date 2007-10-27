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
	after $delay
	catch {flush $server_fh}
	after $delay
	catch {close $server_fh}
	after $delay
	destroy .
	exit
}

proc check_closed {} {
	global got_connection debug
	global client_fh server_fh

	if {! $got_connection} {
		return
	}
	if {$client_fh != "" && [eof $client_fh]} {
		if {$debug} {
			puts stderr "client_fh EOF"
		}
		getout
	}
	if {$server_fh != "" && [eof $server_fh]} {
		if {$debug} {
			puts stderr "server_fh EOF"
		}
		getout
	}
}

proc xfer_in_to_out {} {
	global client_fh server_fh debug
	if {$client_fh != "" && ![eof $client_fh]} {
		set str [read $client_fh 4096]
		if {$debug} {
			puts stderr "xfer_in_to_out: $str"
		}
		if {$server_fh != "" && $str != ""} {
			puts -nonewline $server_fh $str
			flush $server_fh
		}
	}
	check_closed
}

proc xfer_out_to_in {} {
	global client_fh server_fh debug
	if {$server_fh != "" && ![eof $server_fh]} {
		set str [read $server_fh 4096]
		if {$debug} {
			puts stderr "xfer_out_to_in: $str"
		}
		if {$client_fh != "" && $str != ""} {
			puts -nonewline $client_fh $str
			flush $client_fh
		}
	}
	check_closed
}

proc do_connect_http {sock hostport which} {
	global debug cur_proxy
	set con ""
	append con "CONNECT $hostport HTTP/1.1\r\n"
	append con "Host: $hostport\r\n"
	append con "Connection: close\r\n\r\n"

	puts stderr "pxy=$which CONNECT $hostport HTTP/1.1 via $cur_proxy"

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

	set s [split $hostport ":"]
	set host [lindex $s 0]
	set port [lindex $s 1]

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

	set s [split $hostport ":"]
	set host [lindex $s 0]
	set port [lindex $s 1]

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

proc do_connect {sock type hostport which} {
	if {$type == "http"} 	{
		do_connect_http $sock $hostport $which
	} elseif {$type == "socks"} {
		do_connect_socks4 $sock $hostport $which
	} elseif {$type == "socks5"} {
		do_connect_socks5 $sock $hostport $which
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

	fileevent $fh   readable xfer_in_to_out
	fileevent $sock readable xfer_out_to_in

	set cur_proxy $proxy1
	if {$proxy2 != ""} {
		do_connect $sock $proxy1_type $proxy2 1

		set cur_proxy $proxy2
		if {$proxy3 != ""} {
			do_connect $sock $proxy2_type $proxy3 2

			set cur_proxy $proxy3
			do_connect $sock $proxy3_type $dest 3

		} else {
			do_connect $sock $proxy2_type $dest 2
		}
	} else {
		do_connect $sock $proxy1_type $dest 1
	}
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
	} else {
		return "http"
	}
}

global env

set proxy1 ""
set proxy2 ""
set proxy3 ""
set client_fh ""
set server_fh ""

set debug 0
if {$debug} {
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
	if {! [info exists env(SSVNC_LISTEN)]} {
		destroy .; exit;
	}
}

set dest $env(SSVNC_DEST)

if [regexp {,} $env(SSVNC_PROXY)] {
	set s [split $env(SSVNC_PROXY) ","]
	set proxy1 [lindex $s 0]
	set proxy2 [lindex $s 1]
	set proxy3 [lindex $s 2]
} else {
	set proxy1 $env(SSVNC_PROXY)
}

set proxy1_type [proxy_type $proxy1]
regsub {^[A-z0-9][A-z0-9]*://} $proxy1 "" proxy1

set s [split $proxy1 ":"]
set proxy1_host [lindex $s 0]
set proxy1_port [lindex $s 1]

set proxy2_type ""
set proxy2_host ""
set proxy2_port ""

set proxy3_type ""
set proxy3_host ""
set proxy3_port ""

if {$proxy2 != ""} {
	set proxy2_type [proxy_type $proxy2]
	regsub {^[A-z0-9][A-z0-9]*://} $proxy2 "" proxy2
	set s [split $proxy2 ":"]
	set proxy2_host [lindex $s 0]
	set proxy2_port [lindex $s 1]
}

if {$proxy3 != ""} {
	set proxy3_type [proxy_type $proxy3]
	regsub {^[A-z0-9][A-z0-9]*://} $proxy3 "" proxy3
	set s [split $proxy3 ":"]
	set proxy3_host [lindex $s 0]
	set proxy3_port [lindex $s 1]
}

set lport $env(SSVNC_LISTEN)

set got_connection 0
set rc [catch {set lsock [socket -myaddr 127.0.0.1 -server handle_connection $lport]}]
if {$rc != 0} {
	puts stderr "error listening"	
	destroy .
	exit
}

if {1} {
	wm withdraw .
}
button .b -text "CONNECT_BR" -command {destroy .}
pack .b
after 1000 check_callback 
