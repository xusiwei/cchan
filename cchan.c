#include "cchan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CC_FATAL
#define CC_FATAL(msg)                                                          \
	do {                                                                   \
		fprintf(stderr, "CC_FATAL: %s", msg);                          \
		abort();                                                       \
	} while (0)
#endif

#ifndef CC_MALLOC
#define CC_MALLOC malloc
#endif

#ifndef CC_FREE
#define CC_FREE free
#endif

chan_t* chan_new(uint32_t size)
{
	chan_t* c = (chan_t*)CC_MALLOC(sizeof(chan_t));
	if (!c) {
		CC_FATAL("out of memory!");
	}

	size_t nbytes = (size + 1) * sizeof(void*);
	c->data = (void**)CC_MALLOC(nbytes);
	if (!c->data) {
		CC_FATAL("out of memory!");
	}

	memset(c->data, 0, nbytes);
	c->nslots = size + 1; // one slot for guard
	c->begin = 0;
	c->end = 0;

	pthread_mutex_init(&c->mutex, NULL);
	pthread_cond_init(&c->item_cond, NULL);
	pthread_cond_init(&c->slot_cond, NULL);

	return c;
}

void chan_delete(chan_t* c)
{
	pthread_mutex_destroy(&c->mutex);
	pthread_cond_destroy(&c->item_cond);
	pthread_cond_destroy(&c->slot_cond);

	CC_FREE(c->data);
	CC_FREE(c);
}

static uint32_t chan_items_locked(chan_t* c)
{
	return (c->end + c->nslots - c->begin) % c->nslots;
}

static bool chan_full_locked(chan_t* c)
{
	return chan_items_locked(c) + 1 == c->nslots;
}

static void chan_push_item_locked(chan_t* c, void* data)
{
	c->data[c->end] = data;
	if (++c->end == c->nslots) {
		c->end = 0;
	}
}

static void* chan_pop_item_locked(chan_t* c)
{
	void* data = c->data[c->begin];
	if (++c->begin == c->nslots) {
		c->begin = 0;
	}
	return data;
}

uint32_t chan_items(chan_t* c)
{
	uint32_t size = 0;
	pthread_mutex_lock(&c->mutex);
	size = chan_items_locked(c);
	pthread_mutex_unlock(&c->mutex);
	return size;
}

bool chan_empty(chan_t* chan) { return chan_items(chan) == 0; }

bool chan_full(chan_t* c)
{
	bool full = false;
	pthread_mutex_lock(&c->mutex);
	full = chan_full_locked(c);
	pthread_mutex_unlock(&c->mutex);
	return full;
}

void chan_put(chan_t* c, void* data)
{
	pthread_mutex_lock(&c->mutex);

	// wait for slot avaiable
	while (chan_full_locked(c)) {
		pthread_cond_wait(&c->slot_cond, &c->mutex);
	}

	// put down item
	chan_push_item_locked(c, data);

	// notify new item
	pthread_cond_signal(&c->item_cond);

	pthread_mutex_unlock(&c->mutex);
}

void* chan_take(chan_t* c)
{
	void* data = NULL;

	pthread_mutex_lock(&c->mutex);

	// wait for item arrived
	while (chan_items_locked(c) == 0) {
		pthread_cond_wait(&c->item_cond, &c->mutex);
	}

	// pick up item
	data = chan_pop_item_locked(c);

	// notify new slot avaiable
	pthread_cond_signal(&c->slot_cond);

	pthread_mutex_unlock(&c->mutex);

	return data;
}

#ifdef USE_PICK
bool chan_pick(chan_t* c, void** out)
{
	void* data = NULL;

	pthread_mutex_lock(&c->mutex);

	// without blocking
	if (chan_items_locked(c) == 0) {
		return false;
	}

	// pick up item
	data = chan_pop_item_locked(c);

	// notify new slot avaiable
	pthread_cond_signal(&c->slot_cond);

	pthread_mutex_unlock(&c->mutex);

	*out = data;
	return true;
}

chan_t* chan_pickall(chan_t* c)
{
	void* d = NULL;
	chan_t* out = NULL;

	pthread_mutex_lock(&c->mutex);

	out = chan_new(chan_items_locked(c));

	// picks them up
	while (chan_items_locked(c) > 0) {
		d = chan_pop_item_locked(c);
		chan_push_item_locked(out, d);
	}

	// notify new slot avaiable
	pthread_cond_broadcast(&c->slot_cond);

	pthread_mutex_unlock(&c->mutex);
}
#endif // USE_PICK
