#!/bin/bash
for l in 1 5 11; do
	for i in `seq 1 $l`; do
		nice ./work 200 R 10000 &
	done
	wait
	./monitor > profile_c2_$l.data
done
