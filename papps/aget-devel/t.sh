no=$1 
awk ' { print $2 }' log$no.interrupts_begin  > log$no.i0-b 
awk ' { print $3 }' log$no.interrupts_begin  > log$no.i1-b 
awk ' { print $4 }' log$no.interrupts_begin  > log$no.i2-b 
awk ' { print $5 }' log$no.interrupts_begin  > log$no.i3-b 

awk ' { print $2 }' log$no.interrupts_end> log$no.i0-e 
awk ' { print $3 }' log$no.interrupts_end> log$no.i1-e
awk ' { print $4 }' log$no.interrupts_end> log$no.i2-e
awk ' { print $5 }' log$no.interrupts_end> log$no.i3-e
