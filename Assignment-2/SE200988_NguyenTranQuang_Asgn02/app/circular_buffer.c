/* circular_buffer.c - Buffer vong an toan da luong (shared_buffer)
 * Author : Nguyen Tran Quang - SE200988
 * Date   : 2026-10-16 */
#include <stdlib.h>
#include "sms.h"

int cb_init(CircularBuffer *cb)
{
	cb->data = malloc(BUFFER_SIZE * sizeof(SensorData));
	if (cb->data == NULL)
		return -1;
	cb->head = cb->tail = cb->count = 0;
	cb->stopped = 0;
	cb->produced = cb->consumed = cb->overflow = 0;
	pthread_mutex_init(&cb->lock, NULL);
	pthread_cond_init(&cb->not_empty, NULL);
	return 0;
}

void cb_destroy(CircularBuffer *cb)
{
	pthread_mutex_destroy(&cb->lock);
	pthread_cond_destroy(&cb->not_empty);
	free(cb->data);
	cb->data = NULL;
}

int cb_push(CircularBuffer *cb, const SensorData *d)
{
	int ret = 0;

	pthread_mutex_lock(&cb->lock);
	if (cb->count == BUFFER_SIZE) {
		cb->overflow++;
		ret = -1;
	} else {
		cb->data[cb->head] = *d;
		cb->head = (cb->head + 1) % BUFFER_SIZE;
		cb->count++;
		cb->produced++;
		pthread_cond_signal(&cb->not_empty);
	}
	pthread_mutex_unlock(&cb->lock);
	return ret;
}

int cb_pop(CircularBuffer *cb, SensorData *d)
{
	pthread_mutex_lock(&cb->lock);
	while (cb->count == 0 && !cb->stopped)
		pthread_cond_wait(&cb->not_empty, &cb->lock);

	if (cb->count == 0) {
		pthread_mutex_unlock(&cb->lock);
		return -1;
	}
	*d = cb->data[cb->tail];
	cb->tail = (cb->tail + 1) % BUFFER_SIZE;
	cb->count--;
	cb->consumed++;
	pthread_mutex_unlock(&cb->lock);
	return 0;
}

void cb_stop(CircularBuffer *cb)
{
	pthread_mutex_lock(&cb->lock);
	cb->stopped = 1;
	pthread_cond_broadcast(&cb->not_empty);
	pthread_mutex_unlock(&cb->lock);
}

void cb_get_stats(CircularBuffer *cb, int *size, unsigned long *produced,
		  unsigned long *consumed, unsigned long *overflow)
{
	pthread_mutex_lock(&cb->lock);
	*size = cb->count;
	*produced = cb->produced;
	*consumed = cb->consumed;
	*overflow = cb->overflow;
	pthread_mutex_unlock(&cb->lock);
}
