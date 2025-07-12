#!/bin/bash

function log {
  echo "${@}" >&2
}

function show_help {
  log "Usage:  ${0} [-b BATCH_SIZE] [-h] [-t MAX_THREADS] <TOTAL>"
  log "   Ex:  ${0} -b 5000 -t 3 100000  ==> Calculate up to 100,000 with 3 threads in batches of 5,000."
  log ""
  log "Options:"
  log "  -b  #  Number of values to put in each batch.  Default is ${BATCH_SIZE_DEFAULT}."
  log "  -h     Show this help."
  log "  -t  #  Number of threads to run in parallel.  Defaults to OS CPU count."
  log ""
  exit 1
}

BATCH_SIZE_DEFAULT=10000
batch_size=${BATCH_SIZE_DEFAULT}
max_threads=$(nproc)
while getopts ':b:ht:' opt ; do
  case "${opt}" in
    'b') batch_size=${OPTARG}
         ;;
    'h') show_help
         ;;
    't') max_threads=${OPTARG}
         ;;
    *  ) log "Unknown option: -${OPTARG}  (aborting)"
         exit 1
         ;;
  esac
done
shift $((OPTIND - 1))
total=${1}

if [ -z "${total}" ] ; then
  log "You must specify a total number of exponents to build."
  exit 1
fi
if [ ${total} -lt 1 ] ; then
  log "You must specify a positive, whole number of exponents to build."
  exit 1
fi


start=0
last_seconds=${SECONDS}
while [ ${start} -lt ${total} ] ; do
  # Wait if we're full on threads.
  while [ $(jobs -p | wc -l) -ge ${max_threads} ] ; do
    wait -n
  done
  # Calculate the end.
  end=$(( ${start} + ${batch_size} - 1 ))
  if [ ${end} -gt ${total} ] ; then
    end=${total}
  fi
  # Run the program.
  wait_time=$((SECONDS - last_seconds))
  rate=$((start / (SECONDS + 1)))
  log "Starting: ${start} to ${end}.  Waited ${wait_time}s for a worker.  Rate: ~${rate}/s."
  filename="hwm_results__$(printf "%0${#total}d" ${start})_to_$(printf "%0${#total}d" ${end})"
  ./high_water_mark.py --start ${start} --end ${end} > ${filename} &
  last_seconds=${SECONDS}
  # Bump up start.
  let 'start+=batch_size'
done

log "Waiting for final jobs to finish."
while [ $(jobs -p | wc -l) -ge 1 ] ; do
  log "There are $(jobs -p | wc -l) jobs running."
  wait -n
done

log "All jobs done.  Ran for ${SECONDS}s.  Roughly $((total / SECONDS))/s."

