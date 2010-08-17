#!/bin/sh 
./WATER-NSQUARED < input.p2 2> log1
./WATER-NSQUARED < input.p2 2> log2
./det_compare.sh log1
./det_compare.sh log2
