#!/bin/bash

nice ./work 1024 R 50000 & nice ./work 1024 R 10000 &
wait
./monitor > profile1.data

nice ./work 1024 R 50000 & nice ./work 1024 L 10000 &
wait
./monitor > profile2.data
