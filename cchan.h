#ifndef _CCHAN_H_
#define _CCHAN_H_
#include <pthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif // __cplusplus

typedef struct _cchan_chan_st {
	void** data;
	uint32_t nslots;
	uint32_t begin;
	uint32_t end;
	pthread_mutex_t mutex;
	pthread_cond_t item_cond;
	pthread_cond_t slot_cond;
} chan_t;

chan_t* chan_new(uint32_t size);

void chan_delete(chan_t* chan);

void chan_put(chan_t* chan, void* data);

void* chan_take(chan_t* chan);

#ifdef USE_PICK
bool chan_pick(chan_t* chan, void** out);

chan_t* chan_pickall(chan_t* chan);
#endif // USE_PICK

uint32_t chan_items(chan_t* chan);

bool chan_full(chan_t* chan);

bool chan_empty(chan_t* chan);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _CCHAN_H_
