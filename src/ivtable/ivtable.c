#include <stdio.h>
#include <string.h>
#include "ivtable.h"
#include "../logdef.h"


//TODO: missing a delete operation

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
    //in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

char* get_unique_ident(int offset, const char* path){
    char str[15];
    sprintf(str, "%d", offset);
    char* ident = concat(str, path);

    return ident;
}

int hash_contains_key(ivdb *st, char* key){
  int val = (int) g_hash_table_contains (st->hash, key);

  return val;
}

int init_hash(ivdb *st){
	st->hash = g_hash_table_new(g_str_hash, g_str_equal);
  return 0;
}


int hash_put(ivdb *st, char* key, value_db* value){
 	g_hash_table_insert(st->hash, key, value);

  return 0;
}


int hash_get(ivdb *st, char *key, value_db** value){
	*value = g_hash_table_lookup(st->hash, key);

  return 0;
}

int clean_hash(struct db_struct *st,char *path){
 	g_hash_table_destroy(st->hash);

  //free keys and values
	return 0;

}

void print_keys(struct db_struct *st){
  GList* current = g_hash_table_get_keys (st->hash);
  while(current != NULL){
    DEBUG_MSG("Keys in map are %s\n", current->data);
    current = current->next; 
  }
}

void move_key(ivdb* st, char* from, char* to){
  //DEBUG_MSG("Going to move keys\n");
  value_db* get_val = g_hash_table_lookup(st->hash, from);
  if(get_val != NULL ){
    //DEBUG_MSG("Got value %d from key\n", get_val->file_size);
    //value_db* new_val = malloc(sizeof(value_db));
    //new_val->file_size = get_val->file_size;
    //g_hash_table_insert(st->hash, to, new_val);
    g_hash_table_insert(st->hash, to, get_val);

    //DEBUG_MSG("Going to remove key\n");
    g_hash_table_remove (st->hash, from);
  }

}

void remove_keys(ivdb* st, char* key){
  g_hash_table_remove (st->hash, key);
}