#ifndef __TIMESTAMPS_H__
#define __TIMESTAMPS_H__

void store(GSList** list, struct timeval start, struct timeval stop);

void print_latencies(GSList* list, char* driver, char* op);

#endif /* __XOR_H__ */
