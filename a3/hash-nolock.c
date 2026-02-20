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
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "hash.h"
typedef struct entry{
	int key;
	int value;
	struct entry *next;
}entry_t;

struct _hash_table_t {
	int size;// should be a prime number
	entry_t **buckets;
};

static int hash_fn(hash_table_t *t, int key){
	int h = key % t->size;
	return h < 0 ? h + t->size: h;
}

static bool is_prime(int n)
{
	assert(n > 0);
	for (int i = 2; i <= sqrt(n); i++) {
		if (n % i == 0) return false;
	}
	return true;
}

// Get the smallest prime number that is not less than n (for hash table size computation)
int next_prime(int n)
{
	for (int i = n; ; i++) {
		if (is_prime(i)) return i;
	}
	assert(false);
	return 0;
}


// Create a hash table with 'size' buckets; the storage is allocated dynamically using malloc(); returns NULL on error
hash_table_t *hash_create(int size)
{
	assert(size > 0);
	hash_table_t *t = malloc(sizeof(*t));
	if(!t) return NULL; 
	t -> size = next_prime(size);
	t->buckets = calloc(t->size, sizeof(entry_t*));
	if(!t->buckets){
		free(t);
		return NULL;
	}
	return t;
}

// Release all memory used by the hash table, its buckets and entries
void hash_destroy(hash_table_t *table)
{
	assert(table != NULL);
	
	for(int i = 0; i < table->size;i++){
		entry_t *e = table->buckets[i];
		while(e){
			entry_t *next = e->next;
			free(e);
			e = next;
		}
	}
	free(table->buckets);
	free(table);
}


// Returns -1 if key is not found
int hash_get(hash_table_t *table, int key)
{
	assert(table != NULL);
	
	int index = hash_fn(table,key);
	for(entry_t *e = table->buckets[index]; e; e = e->next){
		if (e->key == key) return e->value;
	}

	return -1;
}

// Returns 0 on success, -1 on failure
int hash_put(hash_table_t *table, int key, int value)
{
	assert(table != NULL);
	int index = hash_fn(table,key);
	for (entry_t *e = table->buckets[index]; e; e = e->next){
		if (e->key == key){
			e->value = value;
			return 0;
		}

	}
	entry_t *e = malloc(sizeof(*e));
	if(!e) return -1;
	e->key = key;
	e->value = value;
	e->next = table->buckets[index];
	table->buckets[index] = e;
	return 0;
}
