/*
  SafeFS
  (c) 2016 2016 INESC TEC. Written by J. Paulo and R. Pontes

*/


#ifndef __TIMESTAMPS_H__
#define __TIMESTAMPS_H__

void store(GSList** list, struct timeval start, struct timeval stop);

void print_latencies(GSList* list, char* driver, char* op);

#endif /* __XOR_H__ */
