#!/bin/bash

function get_latest {
  tail -n 1 "${file}" | awk '{print $3}'
}


file="${1}"
if [ ! -f "${file}" ] ; then
  echo "Cannot find file: ${file}" >&2
  exit 1
fi

start=$(get_latest)
current=0
delta=0
rate=0
while true ; do
  sleep 5
  current=$(get_latest)
  delta=$((current - start))
  rate=$((delta / SECONDS))
  printf "Current: %'d   Rate: %'d/s   Total Processed: %'d\n" ${current} ${rate} ${delta}
done

