// ------------
// This code is provided solely for the personal and private use of
// students taking the CSC367 course at the University of Toronto.
// Copying for purposes other than this use is expressly prohibited.
// All forms of distribution of this code, whether as given or with
// any changes, are expressly prohibited.
//
// Authors: Bogdan Simion, Maryam Dehnavi, Alexey Khrabrov
//
// All of the files in this directory and all subdirectories are:
// Copyright (c) 2019 Bogdan Simion and Maryam Dehnavi
// -------------

#include <assert.h>
#include <stddef.h>

#include "join.h"
#define GPA_THRESHOLD 3.0

int join_nested(const student_record *students, int students_count, const ta_record *tas, int tas_count)
{
	assert(students != NULL);
	assert(tas != NULL);

	int sum = 0; 
	for (int i = 0; i < students_count; i++){
		for(int j =0; j < tas_count; j++){
			if (students[i].sid == tas[j].sid && students[i].gpa > GPA_THRESHOLD){ 
				sum++; 
			}
		}
	}

	return sum;
	
}

// Assumes that records in both tables are already sorted by sid
int join_merge(const student_record *students, int students_count, const ta_record *tas, int tas_count)
{
	assert(students != NULL);
	assert(tas != NULL);

	int current_student = 0;
	int current_ta = 0;
	int sum = 0;
	while(current_student < students_count && current_ta < tas_count){
		if(students[current_student].sid > tas[current_ta].sid){
			current_ta++;
		}
		else if(students[current_student].sid < tas[current_ta].sid){
			current_student++;
		}
		else{ //found a match
			
			//once we find a match we can just zoom through the rest of it

			int sid = students[current_student].sid;

			int s_start = current_student;
			int t_start = current_ta;

			while (current_student < students_count &&
				students[current_student].sid == sid) {
				current_student++;
			} 

			while (current_ta < tas_count &&
				tas[current_ta].sid == sid) {
				current_ta++;
			}

			for(int s = s_start; s < current_student; s++){ 
				if(students[current_student].gpa > GPA_THRESHOLD){
					sum += current_ta - t_start; //saves loop iteration by counting in chunks
				}
			}
		}
	}

	return sum; 
}

int join_hash(const student_record *students, int students_count, const ta_record *tas, int tas_count)
{
	assert(students != NULL);
	assert(tas != NULL);

	//TODO

	return -1;
}

