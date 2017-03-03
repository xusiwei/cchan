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
int produce_cost = 0;

long gettid() { return syscall(SYS_gettid); }

#define INFO(fmt, ...) printf("[%ld] " fmt "\n", gettid(), ##__VA_ARGS__)

#define msleep(ms) usleep((ms)*1000)

void* produce(void* args)
{
	int i;
	char line[128];
	chan_t* c = (chan_t*)args;

	for (i = 0; i < nproducts; i++) {
		sprintf(line, "item-%d", i);
		msleep(produce_cost);
		INFO("produced  %s", line);
		chan_put(c, strdup(line));
	}
	return NULL;
}

void* consume(void* args)
{
	int i, total;
	char* item;
	chan_t* c = (chan_t*)args;

	total = nproducers * nproducts;
	for (i = 0; i < total; i++) {
		item = (char*)chan_take(c);
		sleep(1);
		INFO("consumed %s ...", item);
	}
	return NULL;
}

int main(int argc, char* argv[])
{
	int i;
	chan_t* chan = NULL;
	pthread_t consumer;
	pthread_t* producers = NULL;

	if (argc > 1) produce_cost = atoi(argv[1]);
	if (argc > 2) nproducers = atoi(argv[2]);
	if (argc > 3) nproducts = atoi(argv[3]);
	if (argc > 4) nbuffers = atoi(argv[4]);

	chan = chan_new(nbuffers);
	assert(chan);

	// create producers
	producers = (pthread_t*)malloc(nproducers * sizeof(pthread_t));
	assert(producers);

	// start producers
	for (i = 0; i < nproducers; i++) {
		pthread_create(&producers[i], NULL, produce, (void*)chan);
	}

	// start consumer
	pthread_create(&consumer, NULL, consume, (void*)chan);

	// wait consumer done
	pthread_join(consumer, NULL);

	// wait producers done
	for (i = 0; i < nproducers; i++) {
		pthread_join(producers[i], NULL);
	}

	free(producers);
	chan_delete(chan);

	return 0;
}
