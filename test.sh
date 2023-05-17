#! /bin/bash

make all

./os os_1_singleCPU_mlq | tee os_output/os_1_singleCPU_mlq.txt
./os os_1_singleCPU_mlq_paging | tee os_output/os_1_singleCPU_mlq_paging.txt
./os os_1_mlq_paging_small_1K | tee os_output/os_1_mlq_paging_small_1K.txt
./os os_1_mlq_paging_small_4K | tee os_output/os_1_mlq_paging_small_4K.txt
./os os_1_mlq_paging | tee os_output/os_1_mlq_paging.txt
./os os_0_mlq_paging | tee os_output/os_0_mlq_paging.txt

# ./os sched | tee os_output/sched.txt
# ./os sched_0 | tee os_output/sched_0.txt
# ./os sched_1 | tee os_output/sched_1.txt

# exit
