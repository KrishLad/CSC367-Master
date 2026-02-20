#!/bin/bash 

#First load all related modules.  
#You can put the below two lines in a batch file. 
#But remember the modules might get unloaded so check you loaded modules frequently. 
module load StdEnv/2023
module load scipy-stack/2025a
module load StdEnv/2020
module load gcc/10.3.0
module load openmpi/4.1.1

#Compile your code
gcc -v
make clean
make


#Schedule your jobs with sbatch
sbatch job-a3-omp.sh


