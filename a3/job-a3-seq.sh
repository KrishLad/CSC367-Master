#!/bin/bash
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --time=0:10:00
#SBATCH --job-name a3_seq
#SBATCH --output=a3_seq_%j.out

declare -a filearray=(
"../data/dataset0"
"../data/dataset1"
"../data/dataset2"
"../data/dataset3"
"../data/dataset4"
"../data/dataset5"
)


#-n:use Nested loop join, -m: use Sort-Merge join, -h use Hash join
join=-h


executable="./join-seq" 

echo "join = $join"
echo "option = $option"


for filename in "${filearray[@]}"
do
        echo "filename = $filename"
        $executable $join $filename	

done

