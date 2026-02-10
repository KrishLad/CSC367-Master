#!/bin/bash
#SBATCH --nodes=1
#SBATCH --cpus-per-task=8
#SBATCH --time=00:20:00
#SBATCH --job-name a2
#SBATCH --output=a2_%j.out

module load scipy-stack/2025a
python3 perfs_student.py
