/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <guacamole/config.h>

#include <guacamole/pool.h>

#include <stdlib.h>

guac_pool* guac_pool_alloc(int size) {

#ifdef HAVE_LIBPTHREAD
    pthread_mutexattr_t lock_attributes;
#endif
    guac_pool* pool = static_cast<guac_pool*>(malloc(sizeof(guac_pool)));

    /* If unable to allocate, just return NULL. */
    if (pool == NULL)
        return NULL;

    /* Initialize empty pool */
    pool->min_size = size;
    pool->active = 0;
    pool->__next_value = 0;
    pool->__head = NULL;
    pool->__tail = NULL;

#ifdef HAVE_LIBPTHREAD
    /* Init lock */
    pthread_mutexattr_init(&lock_attributes);
    pthread_mutexattr_setpshared(&lock_attributes, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&(pool->__lock), &lock_attributes);
#endif
    return pool;

}

void guac_pool_free(guac_pool* pool) {

    /* Free all ints in pool */
    guac_pool_int* current = pool->__head;
    while (current != NULL) {

        guac_pool_int* old = current;
        current = current->__next;

        free(old);
    }

#ifdef HAVE_LIBPTHREAD
    /* Destroy lock */
    pthread_mutex_destroy(&(pool->__lock));
#endif

    /* Free pool */
    free(pool);

}

int guac_pool_next_int(guac_pool* pool) {

    int value;

#ifdef HAVE_BOOST
	pool->__lock.lock();
#elif defined HAVE_LIBPTHREAD
	/* Acquire exclusive access */
	pthread_mutex_lock(&(pool->__lock));
#endif
    

    pool->active++;

    /* If more integers are needed, return a new one. */
    if (pool->__head == NULL || pool->__next_value < pool->min_size) {
        value = pool->__next_value++;
#ifdef HAVE_BOOST
		pool->__lock.unlock();
#elif defined HAVE_LIBPTHREAD
        pthread_mutex_unlock(&(pool->__lock));
#endif
        return value;
    }

    /* Otherwise, remove first integer. */
    value = pool->__head->value;

    /* If only one element exists, reset pool to empty. */
    if (pool->__tail == pool->__head) {
        free(pool->__head);
        pool->__head = NULL;
        pool->__tail = NULL;
    }

    /* Otherwise, advance head. */
    else {
        guac_pool_int* old_head = pool->__head;
        pool->__head = old_head->__next;
        free(old_head);
    }

    /* Return retrieved value. */
#ifdef HAVE_BOOST
	pool->__lock.unlock();
#elif defined HAVE_LIBPTHREAD
	pthread_mutex_unlock(&(pool->__lock));
#endif
    return value;
}

void guac_pool_free_int(guac_pool* pool, int value) {

    /* Allocate and initialize new returned value */
    guac_pool_int* pool_int = static_cast<guac_pool_int*>(malloc(sizeof(guac_pool_int)));
    pool_int->value = value;
    pool_int->__next = NULL;

#ifdef HAVE_BOOST
	pool->__lock.lock();
#elif defined HAVE_LIBPTHREAD
	/* Acquire exclusive access */
	pthread_mutex_lock(&(pool->__lock));
#endif

    pool->active--;

    /* If pool empty, store as sole entry. */
    if (pool->__tail == NULL)
        pool->__head = pool->__tail = pool_int;

    /* Otherwise, append to end of pool. */
    else {
        pool->__tail->__next = pool_int;
        pool->__tail = pool_int;
    }

    /* Value has been freed */
#ifdef HAVE_BOOST
	pool->__lock.unlock();
#elif defined HAVE_LIBPTHREAD
	pthread_mutex_unlock(&(pool->__lock));
#endif
}

