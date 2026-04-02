/*
 * Airport baggage simulation: one producer loads the belt (2 s/item), one
 * consumer unloads to aircraft (4 s/item), one monitor prints status every 5 s.
 *
 * Synchronization: pthread_mutex_t protects the circular buffer, counters,
 * and completion flags. pthread_cond_t not_full / not_empty block the producer
 * when the belt is full and the consumer when it is empty. The producer signals
 * not_empty after each insert; the consumer signals not_full after each remove.
 *
 * The monitor only reads shared state under the same mutex so it never sees
 * torn updates and never mutates buffer indices.
 *
 * Shutdown: after TOTAL items are produced and consumed, we set done=1,
 * broadcast conditions so blocked threads can wake (not needed here since
 * loops are bounded), join all threads including the monitor.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define BELT_CAP 5
#define TOTAL 20

static int buffer[BELT_CAP];
static int in_idx;
static int out_idx;
static int count;

static int loaded_total;
static int dispatched_total;

static int done;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
static pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

static void *producer(void *arg) {
    (void)arg;
    for (int id = 1; id <= TOTAL; id++) {
        sleep(2);

        pthread_mutex_lock(&mtx);
        while (count == BELT_CAP)
            pthread_cond_wait(&not_full, &mtx);

        buffer[in_idx] = id;
        in_idx = (in_idx + 1) % BELT_CAP;
        count++;
        loaded_total++;

        printf("[Loader] Placed luggage %d on belt | belt size: %d\n", id, count);
        fflush(stdout);

        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

static void *consumer(void *arg) {
    (void)arg;
    for (int n = 0; n < TOTAL; n++) {
        sleep(4);

        pthread_mutex_lock(&mtx);
        while (count == 0)
            pthread_cond_wait(&not_empty, &mtx);

        int item = buffer[out_idx];
        out_idx = (out_idx + 1) % BELT_CAP;
        count--;
        dispatched_total++;

        printf("[Aircraft] Loaded luggage %d | belt size: %d\n", item, count);
        fflush(stdout);

        pthread_cond_signal(&not_full);

        if (dispatched_total >= TOTAL)
            done = 1;

        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

static void *monitor(void *arg) {
    (void)arg;
    for (;;) {
        sleep(5);

        pthread_mutex_lock(&mtx);
        printf("[Monitor] Loaded so far: %d | Dispatched: %d | Belt: %d\n",
               loaded_total, dispatched_total, count);
        fflush(stdout);

        if (done && dispatched_total >= TOTAL) {
            pthread_mutex_unlock(&mtx);
            break;
        }
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

int main(void) {
    pthread_t t_prod, t_cons, t_mon;

    pthread_create(&t_prod, NULL, producer, NULL);
    pthread_create(&t_cons, NULL, consumer, NULL);
    pthread_create(&t_mon, NULL, monitor, NULL);

    pthread_join(t_prod, NULL);
    pthread_join(t_cons, NULL);
    pthread_join(t_mon, NULL);

    printf("Simulation complete (%d items).\n", TOTAL);
    return 0;
}
