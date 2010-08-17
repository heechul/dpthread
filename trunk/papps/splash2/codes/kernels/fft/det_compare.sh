#!/bin/sh 

log=$1

grep "1]" $log > $log.p1 
grep "2]" $log > $log.p2 
