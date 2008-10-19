#!/bin/sh

# copy "vncviewer.sh" back over to "vncviewer" in case you delete or overwrite it
# via build.unix. etc

dir=`dirname "$0"`

if [ "X$SSVNC_DYLD_LIBRARY_PATH" != "X" ]; then
	if [ "X$DYLD_LIBRARY_PATH" = "X" ] ; then
		DYLD_LIBRARY_PATH=$SSVNC_DYLD_LIBRARY_PATH
	else
		DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:$SSVNC_DYLD_LIBRARY_PATH
	fi
	export DYLD_LIBRARY_PATH
fi

if [ "X$DISPLAY" != "X" -a "X$DARWIN_COTVNC" != "X1" ]; then
	"$dir/vncviewer.x11" "$@"
else
	args=""
	for a in "$@"
	do
		if echo "$a" | grep '^-' > /dev/null; then
			args="$args $a"
		elif echo "$a" | grep ':' > /dev/null; then
			h=`echo "$a" | awk -F: '{print $1}'`
			p=`echo "$a" | awk -F: '{print $2}'`
			if [ "X$p" != "X" ]; then
				if [ $p -lt 5900 ]; then
					p=`expr $p + 5900`
				fi
			fi
			args="$args $h:$p"
		else
			args="$args $a"
		fi
	done
	"$dir/../../MacOSX/Chicken of the VNC.app/Contents/MacOS/Chicken of the VNC" $args
fi
