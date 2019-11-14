#!/bin/bash

cd "$( dirname "${BASH_SOURCE[0]}" )"

rm -rf trial
mkdir -p trial

#screen -dm pintos -c `
echo 'start'
max=2
for i in $(seq 1 $max)
do
make clean > "trial/compile"
make -j 8 check > "trial/trial_$i.txt"
if grep "FAIL" build/results
then
  grep "FAIL" build/results > "trial/result"
  echo "$i th trial failed" > "trial/result"
  break
fi
echo "$i th trial succeeded" > "trial/result"
done
#`