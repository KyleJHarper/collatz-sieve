#!/bin/bash

first=7
for step in $(seq 16 16 128) ; do
  last=$(( ${first} + (${step}/16) ))
  end=$(( ${step} * 8 ))
  for start in $(seq ${first} ${step} ${last} ) ; do
    ./integer_table.py --oe-split 11 --start ${start} --step ${step} --end ${end} --oe-width 32 --sequence-width 3
    echo ''
  done
  echo ''
  echo ''
  echo ''
done

