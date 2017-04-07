/*

  IV database driver
  (c) 2016 2016 INESC TEC.  by R. Pontes (ADD Other authors here)

*/
#ifndef __IV_TABLE_H__
#define __IV_TABLE_H__

#include "../logdef.h"
#include <stdlib.h>
#include <glib.h>
#include <stdint.h>
#include <stdio.h>


typedef struct db_struct{

	GHashTable* hash;

} ivdb;

typedef struct value{
	uint64_t file_size;
} value_db;

char* get_unique_ident(int offset, const char* path);

int init_hash(ivdb *db);

int hash_put(ivdb *db, char* key, value_db* value);

int hash_get(ivdb *db, char* key, value_db** value);

//Copy every key with key "from" to a new key with "to";
void hash_rename(ivdb *db, char* from, char* to);

int hash_contains_key(ivdb *db, char* key);

int clean_hash(ivdb* db, char* path);

void print_keys(struct db_struct *st);

void remove_keys(ivdb* st, char* key);

void move_key(ivdb* st, char* from, char* to);

#endif 
