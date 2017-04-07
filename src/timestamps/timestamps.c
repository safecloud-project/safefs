#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "timestamps.h"
#include "../logdef.h"

//structure used to store timestamps of layers.

 
void store(GSList** list, struct timeval start, struct timeval stop){

	/**
	 *adjust the second part to microseonds and the nanoseconds to microseconds.
	 *previous code did not take into account the seconds part as usec wrapped around 0.
	 * http://stackoverflow.com/questions/10192903/time-in-milliseconds
	 */
	long elapsed = (stop.tv_sec - start.tv_sec) * 1000000 + (stop.tv_usec - start.tv_usec);

	(*list) =  g_slist_prepend(*list,  GSIZE_TO_POINTER(elapsed));
}

void print_latencies(GSList* list, char* driver, char* op){
	char filename[200];
	//sprintf(filename, "/usr/local/src/sdsfuse/code/SDSFuse/%s_%s.txt", driver, op);
	sprintf(filename, "/opt/dump/%s_%s.txt", driver, op);
	FILE *f = fopen(filename, "w");

	GSList* current = list;
	while(current != NULL){
		//GPOINTER_TO_SIZE()
		//tsp* value = current->data;
		fprintf(f, "%ld\n", GPOINTER_TO_SIZE(current->data));
		//ERROR_MSG("%s|%s|%lf\n", op, driver, 1.0e-3*(value->val));
    	current = current->next; 
  	}
    fclose(f);
}