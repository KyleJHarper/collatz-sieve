#!/bin/bash

function log {
  echo "${@}" >&2
}

start=0
count=1000
total=100200
max_threads=$(nproc)
while [ ${start} -lt ${total} ] ; do
  # Calculate the end.
  end=$(( ${start} + ${count} - 1 ))
  if [ ${end} -gt ${total} ] ; then
    end=${total}
  fi
  # Run the program.
  log "Starting: ${start} to ${end}."
  ./high_water_mark.py --start ${start} --end ${end} > hwm_results__${start}_to_${end} &
  # Bump up start.
  let 'start+=count'
  # Wait if we're full on threads.
  while [ $(jobs -p | wc -l) -ge ${max_threads} ] ; do
    wait -n
  done
done

log "Waiting for final jobs to finish."
while [ $(jobs -p | wc -l) -ge 1 ] ; do
  log "There are $(jobs -p | wc -l) jobs running."
  wait -n
done

log "All jobs done."

