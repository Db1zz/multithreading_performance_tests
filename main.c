#include "timer.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>

/*
	With copying
	Rendered for [m:0, s:46, ms:594]

	With pointers
	Rendered for [m:0, s:41, ms:456]

====================================

	Heavy struct with copying
	Rendered for [m:8, s:58, ms:599]

	Heavy struct with pointers
	Rendered for [m:3, s:13, ms:201]
*/

/*
	/ =================================================================================================================== /
	/ =================================================================================================================== /
	/ ================================================== THREADS TESTS ================================================== /
	/ =================================================================================================================== /
	/ =================================================================================================================== /
*/

#include <pthread.h>
#include <semaphore.h>

#define __MUTEXES_SIZE_LIMIT 1024

typedef struct s_thread_sync
{
	size_t threads_finished;
	bool destroy_threads;
}	t_threads_sync;

typedef struct s_mutexes
{
	pthread_mutex_t global_mutex;
	pthread_mutex_t threads_mutexes[__MUTEXES_SIZE_LIMIT];
	size_t threads_amount;
}	t_mutexes;

typedef struct s_thread_ctx
{
	unsigned int *arr;
	t_mutexes *mutexes;
	t_threads_sync *sync;
	size_t height;
	size_t width;
	size_t tid;
}	t_thread_ctx;

typedef	struct s_process_data
{
	t_mutexes *mutexes;
	pthread_t *threads;
	t_thread_ctx *ctxs;
	t_threads_sync *sync;
	size_t threads_amount;
}	t_process_data;

t_thread_ctx	*init_thread_ctxs(size_t img_width, size_t img_height, size_t threads_amount, t_mutexes *mutexes, t_threads_sync *sync)
{
	t_thread_ctx *ctxs = calloc(threads_amount, sizeof(t_thread_ctx));
	size_t perv_end = 0;

	for (size_t i = 0; i < threads_amount; ++i) {
		ctxs[i].height = (perv_end + (img_height / threads_amount + ((img_height - perv_end) % (threads_amount - i)))) - perv_end;
		ctxs[i].width = img_width;
		ctxs[i].arr = calloc(ctxs[i].height * ctxs[i].width, sizeof(unsigned int));
		ctxs[i].tid = i;
		ctxs[i].mutexes = mutexes;
		ctxs[i].sync = sync;
	}
	return ctxs;
}

t_mutexes *init_mutexes(size_t threads_amount)
{
	t_mutexes *mutexes = calloc(1, sizeof(t_mutexes));
	mutexes->threads_amount = threads_amount;
	pthread_mutex_init(&mutexes->global_mutex, NULL);

	for (size_t i = 0; i < threads_amount; ++i) {
		pthread_mutex_init(&mutexes->threads_mutexes[i], NULL);
		pthread_mutex_lock(&mutexes->threads_mutexes[i]);
	}

	return mutexes;
}

t_threads_sync *init_sync()
{
	t_threads_sync *sync = calloc(1, sizeof(t_threads_sync));

	return sync;
}

unsigned int some_expensive_computation(size_t i, size_t j)
{
	unsigned int value = 0;

	while (value < i * j) {
		++value;
	}
	return value;
}

void thread_compute(t_thread_ctx *ctx)
{
	for (size_t i = 0; i < ctx->height; ++i)
	{
		for (size_t j = 0; j < ctx->width; ++j)
			ctx->arr[i * j] = some_expensive_computation(i, j);
	}
}

bool is_exit_needed(t_thread_ctx *ctx)
{
	bool result = false;

	pthread_mutex_lock(&ctx->mutexes->global_mutex);
	result = ctx->sync->destroy_threads;
	pthread_mutex_unlock(&ctx->mutexes->global_mutex);
	return result;
}

void *thread_routine(void *data)
{
	t_thread_ctx *ctx = (t_thread_ctx *)data;

	while (true)
	{
		printf("thread[%zu]: waiting\n", ctx->tid);
		while (pthread_mutex_trylock(&ctx->mutexes->threads_mutexes[ctx->tid])) 
		{
			if (is_exit_needed(ctx))
				return NULL;
			usleep(5000);
		}
		
		printf("thread[%zu]: processing\n", ctx->tid);
		thread_compute(ctx);
		pthread_mutex_lock(&ctx->mutexes->global_mutex);
		printf("thread[%zu]: finished\n", ctx->tid);
		ctx->sync->threads_finished += 1;
		pthread_mutex_unlock(&ctx->mutexes->global_mutex);
	}
	printf("thread[%zu]: exiting\n", ctx->tid);
	return NULL;
}

pthread_t *create_threads(t_thread_ctx *ctxs, size_t threads_amount)
{
	pthread_t *threads = calloc(threads_amount, sizeof(pthread_t));

	for (size_t i = 0; i < threads_amount; ++i) {
		pthread_create(&threads[i], NULL, thread_routine, &ctxs[i]);
	}
	return threads;
}

t_process_data *init_process(size_t img_width, size_t img_height, size_t threads_amount)
{
	t_process_data *process_data = calloc(1, sizeof(t_process_data));
	t_mutexes *mutexes = init_mutexes(threads_amount);
	t_threads_sync *sync = init_sync();
	t_thread_ctx *ctxs = init_thread_ctxs(img_width, img_height, threads_amount, mutexes, sync);

	process_data->mutexes = mutexes;
	process_data->ctxs = ctxs;
	process_data->threads = create_threads(ctxs, threads_amount);
	process_data->sync = sync;
	process_data->threads_amount = threads_amount;

	return process_data;
}

void unlock_all_mutexes(pthread_mutex_t *mutexes, size_t amount_to_unlock)
{
	for (size_t i = 0; i < amount_to_unlock; ++i) {
		pthread_mutex_unlock(&mutexes[i]);
	}
}

void wait_threads_to_finish(t_process_data *process_data)
{
	for (size_t i = 0; i < process_data->threads_amount; ++i)
	{
		pthread_join(process_data->threads[i], NULL);
	}
}

void main_thread_routine(t_process_data *process_data, size_t amount_of_cycles)
{
	for (size_t i = 0; i < amount_of_cycles; ++i)
	{
		pthread_mutex_lock(&process_data->mutexes->global_mutex);
		process_data->ctxs->sync->threads_finished = 0;
		pthread_mutex_unlock(&process_data->mutexes->global_mutex);
		unlock_all_mutexes(process_data->mutexes->threads_mutexes, process_data->mutexes->threads_amount);
		while (true)
		{
			pthread_mutex_lock(&process_data->mutexes->global_mutex);
			if (process_data->sync->threads_finished == process_data->threads_amount)
			{
				pthread_mutex_unlock(&process_data->mutexes->global_mutex);	
				break;
			}
			pthread_mutex_unlock(&process_data->mutexes->global_mutex);
			usleep(1000);
		}
		printf("Cycle\n");
	}
	printf("Killing threads...\n");
	pthread_mutex_lock(&process_data->mutexes->global_mutex);
	process_data->ctxs->sync->destroy_threads = true;
	pthread_mutex_unlock(&process_data->mutexes->global_mutex);
	printf("Waiting threads to finish...\n");
	wait_threads_to_finish(process_data);
}

int main(void)
{
	const size_t amount_of_cycles = 2;
	const size_t img_width = 1920;
	const size_t img_height = 1080;
	const size_t cpu_amount = sysconf(_SC_NPROCESSORS_CONF);
	
	unsigned int *img = calloc(img_width * img_height, sizeof(unsigned int));
	struct timeval start_time = getTime();
	
	t_process_data *process_data = init_process(img_width, img_height, cpu_amount);
	main_thread_routine(process_data, amount_of_cycles);

	struct timeval end_time = getTime();
	printf("Rendered for [m:%ld, s:%ld, ms:%ld]\n",
		getMinutesDiff(&start_time, &end_time),
		getSecondsDiff(&start_time, &end_time),
		getMilisecondsDiff(&start_time, &end_time)); 
	return 0;
}