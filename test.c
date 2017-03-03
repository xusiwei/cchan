#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>

#include "cchan.h"

int nbuffers = 10;
int nproducts = 20;
int nproducers = 1;
int nconsumers = 1;

long gettid() { return syscall(SYS_gettid); }

#define INFO(fmt, ...) printf("[%ld] " fmt "\n", gettid(), ##__VA_ARGS__)

void* produce(void* args)
{
	int i;
	char line[128];
	chan_t* c = (chan_t*)args;

	for (i = 0; i < nproducts; i++) {
		sprintf(line, "item-%d", i);
		INFO("produced  %s", line);
		chan_put(c, strdup(line));
	}
	return NULL;
}

void* consume(void* args)
{
	int i;
	char* item;
	chan_t* c = (chan_t*)args;

	for (i = 0; i < nproducts; i++) {
		item = (char*)chan_take(c);
		INFO("consuming %s ...", item);
		sleep(1);
	}
	return NULL;
}

int main(int argc, char* argv[])
{
	int i;
	chan_t* chan = NULL;
	pthread_t* producers = NULL;
	pthread_t* consumers = NULL;

	if (argc > 1) nconsumers = atoi(argv[1]);
	if (argc > 2) nproducers = atoi(argv[2]);
	if (argc > 3) nproducts = atoi(argv[3]);
	if (argc > 4) nbuffers = atoi(argv[34]);

	chan = chan_new(nbuffers);
	assert(chan);

	producers = (pthread_t*)malloc(nproducers * sizeof(pthread_t));
	assert(producers);
	for (i = 0; i < nproducers; i++) {
		pthread_create(&producers[i], NULL, produce, (void*)chan);
	}

	consumers = (pthread_t*)malloc(nconsumers * sizeof(pthread_t));
	assert(consumers);
	for (i = 0; i < nconsumers; i++) {
		pthread_create(&consumers[i], NULL, consume, (void*)chan);
	}

	for (i = 0; i < nconsumers; i++) {
		pthread_join(consumers[i], NULL);
	}

	for (i = 0; i < nproducers; i++) {
		pthread_join(producers[i], NULL);
	}

	free(consumers);
	free(producers);
	chan_delete(chan);

	return 0;
}
