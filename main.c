#include "timer.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>

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

typedef struct s_img
{
	unsigned int *arr;
	size_t height;
	size_t width;
}	t_img;

typedef struct s_thread_ctx
{
	t_img *img;
	t_mutexes *mutexes;
	t_threads_sync *sync;
	size_t tid;
}	t_thread_ctx;

typedef	struct s_process_data
{
	t_mutexes *mutexes;
	pthread_t *threads;
	t_thread_ctx *ctxs;
	t_threads_sync *sync;
	t_img *img;
	size_t threads_amount;
}	t_process_data;

t_img *init_img(size_t img_width, size_t img_height)
{
	t_img *img = malloc(sizeof(t_img));

	img->arr = malloc(img_width * img_height * sizeof(unsigned int));
	img->width = img_width;
	img->height = img_height;
	return img;
}

void destroy_img(t_img *img)
{
	free(img->arr);
	free(img);
}

t_thread_ctx	*init_thread_ctxs(size_t img_width, size_t img_height, size_t threads_amount, t_mutexes *mutexes, t_threads_sync *sync)
{
	t_thread_ctx *ctxs = calloc(threads_amount, sizeof(t_thread_ctx));
	size_t perv_end = 0;

	for (size_t i = 0; i < threads_amount; ++i) {
		size_t ctx_height = (perv_end + (img_height / threads_amount + ((img_height - perv_end) % (threads_amount - i)))) - perv_end;
		ctxs[i].img = init_img(img_width, ctx_height);
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
	for (size_t i = 0; i < ctx->img->height; ++i)
	{
		for (size_t j = 0; j < ctx->img->width; ++j)
			ctx->img->arr[i * j] = some_expensive_computation(i, j);
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
	process_data->img = init_img(img_width, img_height);

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

void copy_data_from_threads_to_array(t_process_data *process_data)
{
	size_t x = 0;
	size_t y = 0;

	for (size_t i = 0; i < process_data->threads_amount; ++i)
	{
		size_t x_end = process_data->ctxs[i].img->height;
		size_t y_end = process_data->ctxs[i].img->width;
		while (x < x_end)
		{
			while (y < y_end)
			{
				++y;
			}
			++x;
		}
	}
}

void main_thread_routine(t_process_data *process_data, size_t amount_of_cycles)
{
	struct timeval start_time;
	struct timeval end_time;

	for (size_t i = 0; i < amount_of_cycles; ++i)
	{
		pthread_mutex_lock(&process_data->mutexes->global_mutex);
		process_data->ctxs->sync->threads_finished = 0;
		pthread_mutex_unlock(&process_data->mutexes->global_mutex);
		start_time = getTime();
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
		copy_data_from_threads_to_array(process_data);
		end_time = getTime();
		printf("Rendered for [m:%ld, s:%ld, ms:%ld]\n",
				getMinutesDiff(&start_time, &end_time),
				getSecondsDiff(&start_time, &end_time),
				getMilisecondsDiff(&start_time, &end_time));
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
	
	t_process_data *process_data = init_process(img_width, img_height, cpu_amount);

	main_thread_routine(process_data, amount_of_cycles);

	return 0;
}