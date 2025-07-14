#!/bin/bash

function log {
  echo "${@}" >&2
}

function show_help {
  log "Usage:  ${0} [-b BATCH_SIZE] [-h] [-s START] [-t MAX_THREADS] <MAX>"
  log "   Ex:  ${0} -b 5000 -t 3 100000  ==> Calculate up to 100,000 with 3 threads in batches of 5,000."
  log ""
  log "Options:"
  log "  -b  #  Number of values to put in each batch.  Default is ${BATCH_SIZE_DEFAULT}."
  log "  -h     Show this help."
  log "  -s  #  Start at this number.  Default is 0."
  log "  -t  #  Number of threads to run in parallel.  Defaults to OS CPU count."
  log ""
  exit 1
}

BATCH_SIZE_DEFAULT=10000
batch_size=${BATCH_SIZE_DEFAULT}
max_threads=$(nproc)
start=0
while getopts ':b:hs:t:' opt ; do
  case "${opt}" in
    'b') batch_size=${OPTARG}
         ;;
    'h') show_help
         ;;
    's') start=${OPTARG}
         ;;
    't') max_threads=${OPTARG}
         ;;
    *  ) log "Unknown option: -${OPTARG}  (aborting)"
         exit 1
         ;;
  esac
done
shift $((OPTIND - 1))
max=${1}
min=${start}
total=$((max - min))

if [ -z "${max}" ] ; then
  log "You must specify a total number of exponents to build as arg1."
  exit 1
fi
if [ ${max} -lt 1 ] ; then
  log "You must specify a positive, whole number of exponents to build as arg1."
  exit 1
fi


last_seconds=${SECONDS}
completed=0
while [ ${start} -lt ${max} ] ; do
  # Wait if we're full on threads.
  while [ $(jobs -p | wc -l) -ge ${max_threads} ] ; do
    wait -n
  done
  # Calculate the end.
  end=$(( ${start} + ${batch_size} - 1 ))
  if [ ${end} -gt ${max} ] ; then
    end=${max}
  fi
  # Run the program.
  wait_time=$((SECONDS - last_seconds))
  completed=$(( ${end} - ${min} - (${max_threads} * ${batch_size}) ))
  if [ ${completed} -lt 1 ] ; then
    completed=0
  fi
  rate=$((completed / (SECONDS + 1)))
  log "Starting: ${start} to ${end}.  Waited ${wait_time}s for a worker.  Rate: ~${rate}/s."
  filename="hwm_results__$(printf "%0${#max}d" ${start})_to_$(printf "%0${#max}d" ${end})"
  ../bin/high_water_mark -s ${start} -e ${end} > ${filename} &
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

