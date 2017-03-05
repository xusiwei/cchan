#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>
#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>

#if (defined __STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <stdatomic.h>
#else // __STDC_VERSION__
#define atomic_int int
#define atomic_fetch_add __sync_fetch_and_add
#endif // __STDC_VERSION__

#include "cchan.h"

int nbuffers = 10;
int nproducts = 20;
int nproducers = 1;
int nconsumers = 1;
int produce_cost = 0;

long gettid() { return syscall(SYS_gettid); }

#define INFO(fmt, ...)                                                         \
	do {                                                                   \
		struct timespec ts;                                            \
		char buffer[128];                                              \
		clock_gettime(CLOCK_REALTIME, &ts);                            \
		strftime(buffer, sizeof(buffer), "%F %T",                      \
			 localtime(&ts.tv_sec));                               \
		printf("[%s.%ld] %ld " fmt "\n", buffer,                       \
		       ts.tv_nsec / (1000 * 1000), gettid(), ##__VA_ARGS__);   \
	} while (0)

#define msleep(ms) usleep((ms)*1000)

void* produce(void* args)
{
	int i;
	char line[128];
	chan_t* c = (chan_t*)args;

	static atomic_int count = 0;
	for (i = 0; i < nproducts; i++) {
		int id = atomic_fetch_add(&count, 1);
		sprintf(line, "item-%d", id);
		msleep(produce_cost);
		INFO("produced  %s", line);
		chan_put(c, strdup(line));
	}
	return NULL;
}

static atomic_int rest = 0;

void* consume(void* args)
{
	char* item;
	chan_t* c = (chan_t*)args;

	while (atomic_fetch_add(&rest, -1) >= 1) {
		item = (char*)chan_take(c);
		sleep(1);
		INFO("consumed %s ...", item);
		free(item);
	}
	return NULL;
}

int main(int argc, char* argv[])
{
	int i;
	chan_t* chan = NULL;
	pthread_t* consumers = NULL;
	pthread_t* producers = NULL;

	int c = 0;
	if (argc > ++c) produce_cost = atoi(argv[c]);
	if (argc > ++c) nproducers = atoi(argv[c]);
	if (argc > ++c) nconsumers = atoi(argv[c]);
	if (argc > ++c) nproducts = atoi(argv[c]);
	if (argc > ++c) nbuffers = atoi(argv[c]);

#ifdef __STDC_VERSION__
	printf("__STDC_VERSION__: %ld\n", __STDC_VERSION__);
#endif
	atomic_fetch_add(&rest, nproducers * nproducts);

	chan = chan_new(nbuffers);
	assert(chan);

	// create producers
	consumers = (pthread_t*)malloc(nconsumers * sizeof(pthread_t));
	assert(consumers);

	// create producers
	producers = (pthread_t*)malloc(nproducers * sizeof(pthread_t));
	assert(producers);

	// start producers
	for (i = 0; i < nproducers; i++) {
		pthread_create(&producers[i], NULL, produce, (void*)chan);
	}

	// start consumers
	for (i = 0; i < nconsumers; i++) {
		pthread_create(&consumers[i], NULL, consume, (void*)chan);
	}

	// wait consumers done
	for (i = 0; i < nconsumers; i++) {
		pthread_join(consumers[i], NULL);
	}

	// wait producers done
	for (i = 0; i < nproducers; i++) {
		pthread_join(producers[i], NULL);
	}

	free(consumers);
	free(producers);
	chan_delete(chan);

	return 0;
}
