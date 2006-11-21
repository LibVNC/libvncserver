#!/usr/bin/wish

global env

set proxy1 ""
set proxy2 ""
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
}

set dest $env(SSVNC_DEST)

if [regexp {,} $env(SSVNC_PROXY)] {
	set s [split $env(SSVNC_PROXY) ","]
	set proxy1 [lindex $s 0]
	set proxy2 [lindex $s 1]
} else {
	set proxy1 $env(SSVNC_PROXY)
}

set s [split $proxy1 ":"]
set proxy1_host [lindex $s 0]
set proxy1_port [lindex $s 1]

if {$proxy2 != ""} {
	set s [split $proxy2 ":"]
	set proxy2_host [lindex $s 0]
	set proxy2_port [lindex $s 1]
}

set lport $env(SSVNC_LISTEN)

set got_connection 0
set lsock [socket -myaddr 127.0.0.1 -server handle_connection $lport]

if {1} {
	wm withdraw .
}
button .b -text "CONNECT_BR" -command {destroy .}
pack .b
after 1000 check_callback 

proc check_callback {} {
	global debug
	if {$debug} {
		puts stderr "."
	}
	check_closed
	after 1000 check_callback 
}

proc check_closed {} {
	global client_fh server_fh debug
	global got_connection

	if {! $got_connection} {
		return
	}
	set delay 100
	if {$client_fh != "" && [eof $client_fh]} {
		if {$debug} {
			puts stderr "client_fh EOF"
		}
		catch {flush $client_fh}
		after $delay
		catch {close $client_fh}
		after $delay
		catch {flush $server_fh}
		after $delay
		catch {close $server_fh}
		destroy .
		exit
	}
	if {$server_fh != "" && [eof $server_fh]} {
		if {$debug} {
			puts stderr "server_fh EOF"
		}
		catch {flush $server_fh}
		after $delay
		catch {close $server_fh}
		after $delay
		catch {flush $client_fh}
		after $delay
		catch {close $client_fh}
		destroy .
		exit
	}
}

proc xfer_in_to_out {} {
	global client_fh server_fh debug
	if {$client_fh != "" && ![eof $client_fh]} {
		set str [read $client_fh 4096]
		if {$debug} {
			puts stderr "xfer_in_to_out: $str"
		}
		if {$server_fh != ""} {
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
		if {$client_fh != ""} {
			puts -nonewline $client_fh $str
			flush $client_fh
		}
	}
	check_closed
}

proc handle_connection {fh host port} {
	global proxy1_host proxy1_port
	global proxy2_host proxy2_port
	global proxy1 proxy2
	global dest
	global debug
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

	set sock [socket $proxy1_host $proxy1_port]

	global client_fh server_fh
	set client_fh $fh
	set server_fh $sock

	fconfigure $fh   -translation binary -blocking 0
	fconfigure $sock -translation binary -blocking 0

	set con ""
	if {$proxy2 != ""} {
		append con "CONNECT $proxy2 HTTP/1.1\r\n"
		append con "Host: $proxy2\r\n\r\n"
	} else {
		append con "CONNECT $dest HTTP/1.1\r\n"
		append con "Host: $dest\r\n\r\n"
	}

	puts -nonewline $sock $con
	flush $sock

	set r ""
	set cnt 0
	while {1} {
		set c [read $sock 1]
		if {$c == ""} {
			check_closed
			after 20
		}
		incr cnt
		if {$debug} {
			.b configure -text "A $cnt -- $c"
			update
		}
		append r $c
		if {[regexp "\r\n\r\n" $r] || [regexp "a--no--\n\n" $r]} {
			break
		}
		if {$cnt > 3000} {
			break
		}
	}
	if {! [regexp {HTTP/.* 200} $r]} {
		puts stderr "did not find HTTP 200 #1"
		if {1} {
			destroy .
			exit 1
		}
	}

	if {$proxy2 != ""} {
		set con ""
		append con "CONNECT $dest HTTP/1.1\r\n"
		append con "Host: $dest\r\n\r\n"

		puts -nonewline $sock $con
		flush $sock

		set r ""
		set cnt 0
		while {1} {
			set c [read $sock 1]
			if {$c == ""} {
				check_closed
				after 20
			}
			incr cnt
			if {$debug} {
				.b configure -text "B $cnt -- $c"
				update
			}
			append r $c
			if {[regexp "\r\n\r\n" $r] || [regexp "a--no--\n\n" $r]} {
				break
			}
			if {$cnt > 3000} {
				break
			}
		}
		if {! [regexp {HTTP/.* 200} $r]} {
			puts stderr "did not find HTTP 200 #2"
			destroy .
			exit 1
		}
	}

	fileevent $fh   readable xfer_in_to_out
	fileevent $sock readable xfer_out_to_in
}
