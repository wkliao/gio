#!/bin/bash
#
# Copyright (C) 2026, Northwestern University and Argonne National Laboratory
# See COPYRIGHT notice in top-level directory.
#

# Exit immediately if a command exits with a non-zero status.
set -e

DRY_RUN=no
VERBOSE=no

run_cmd() {
   local lineno=${BASH_LINENO[$((${#BASH_LINENO[@]} - 2))]}
   if test "x$VERBOSE" = xyes || test "x$DRY_RUN" = xyes ; then
      echo "Line $lineno CMD: $MPIRUN $@"
   fi
   if test "x$DRY_RUN" = xno ; then
      $MPIRUN $@
   fi
}

MPIRUN=`echo ${TESTMPIRUN} | ${SED} -e "s/NP/$1/g"`
# echo "MPIRUN = ${MPIRUN}"
# echo "check_PROGRAMS=${check_PROGRAMS}"

for i in ${check_PROGRAMS} ; do
   # Capture start time in seconds and nanoseconds
   start_time=$(date +%s.%1N)

   exe_name=`basename $i`

   run_cmd ./$i ${TESTOUTDIR}/${exe_name}.dat

   # Calculate difference (requires bc for floating point math)
   end_time=$(date +%s.%1N)
   elapsed_time=$(echo "$end_time - $start_time" | bc)

   fixed_length=48
   printf "*** TESTING  %-${fixed_length}s   -- pass (%4ss)\n" "$i" "$elapsed_time"

done # check_PROGRAMS

