#! /bin/bash

make all


./os os_1_mlq_paging_small_1K | tee gantt_output/os_1_mlq_paging_small_1K.txt
./os os_0_mlq_paging | tee gantt_output/os_0_mlq_paging.txt

# ./os sched | tee gantt_output/sched.txt