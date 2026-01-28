#!/bin/bash

function get_latest {
    tail -n 1 "${file}" | awk '{print $3}'
}

function now {
    echo "$(date '%s.%N')"
}


file="${1}"
if [ ! -f "${file}" ] ; then
  echo "Cannot find file: ${file}" >&2
  exit 1
fi

initial_value=0
global_delta=0
while true ; do
    sleep 2
    entries=$(tail -n 2 "${file}")
    entry_1="$(cut -d $'\n' -f 1 <<< "${entries}" | sed -r -e "s/\[|\]//g")"
    entry_2="$(cut -d $'\n' -f 2 <<< "${entries}" | sed -r -e "s/\[|\]//g")"
    t1=$(date -d "$(awk '{print $1 " " $2}' <<< "${entry_1}")" '+%s')
    t2=$(date -d "$(awk '{print $1 " " $2}' <<< "${entry_2}")" '+%s')
    v1="$(awk '{print $3}' <<< "${entry_1}")"
    v2="$(awk '{print $3}' <<< "${entry_2}")"
    [ ${initial_value} = 0 ] && initial_value=${v1}
    global_delta=$((v2 - initial_value))
    delta=$((v2 - v1))
    rate=$((delta / (t2 - t1)))
    printf "Current: %'d   Rate: %'d/s   Total Processed: %'d\n" ${v2} ${rate} ${global_delta}
done

