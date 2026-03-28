#!/bin/bash
# SPDX-License-Identifier: GPL-2.0+
#
# Alternate sleeping and spinning on randomly selected CPUs.  The purpose
# of this script is to inflict random OS jitter on a concurrently running
# test.
#
# Usage: jitter.sh me jittering-path duration [ sleepmax [ spinmax ] ]
#
# me: Random-number-generator seed salt.
# duration: Time to run in seconds.
# jittering-path: Path to file whose removal will stop this script.
# sleepmax: Maximum microseconds to sleep, defaults to one second.
# spinmax: Maximum microseconds to spin, defaults to one millisecond.
#
# Copyright (C) IBM Corporation, 2016
#
# Authors: Paul E. McKenney <paulmck@linux.ibm.com>

me=$(($1 * 1000))
jittering=$2
duration=$3
sleepmax=${4-1000000}
spinmax=${5-1000}

n=1

starttime=`awk 'BEGIN { print systime(); }' < /dev/null`

while :
do
	# Check for done.
	t=`awk -v s=$starttime 'BEGIN { print systime() - s; }' < /dev/null`
	if test "$t" -gt "$duration"
	then
		exit 0;
	fi

	# Check for stop request.
	if ! test -f "$jittering"
	then
		exit 1;
	fi

	# Set affinity to randomly selected online CPU
	cpus=`grep 1 /sys/devices/system/cpu/*/online |
		sed -e 's,/[^/]*$,,' -e 's/^[^0-9]*//'`

	# Do not leave out poor old cpu0 which may not be hot-pluggable
	if [ ! -f "/sys/devices/system/cpu/cpu0/online" ]; then
		cpus="0 $cpus"
	fi

	cpumask=`awk -v cpus="$cpus" -v me=$me -v n=$n 'BEGIN {
		srand(n + me + systime());
		ncpus = split(cpus, ca);
		print ca[int(rand() * ncpus + 1)];
	}' < /dev/null`
	n=$(($n+1))
	if ! taskset -c -p $cpumask $$ > /dev/null 2>&1
	then
		echo taskset failure: '"taskset -c -p ' $cpumask $$ '"'
		exit 1
	fi

	# Sleep a random duration
	sleeptime=`awk -v me=$me -v n=$n -v sleepmax=$sleepmax 'BEGIN {
		srand(n + me + systime());
		printf("%06d", int(rand() * sleepmax));
	}' < /dev/null`
	n=$(($n+1))
	sleep .$sleeptime

	# Spin a random duration
	limit=`awk -v me=$me -v n=$n -v spinmax=$spinmax 'BEGIN {
		srand(n + me + systime());
		printf("%06d", int(rand() * spinmax));
	}' < /dev/null`
	n=$(($n+1))
	for i in {1..$limit}
	do
		echo > /dev/null
	done
done

exit 1
