#include <assert.h>
#include <stdio.h>
#include "app_bqueue.h"

static ULONG last_wait;
static unsigned int created;

UINT tx_semaphore_create(TX_SEMAPHORE *semaphore, const char *name, ULONG count)
{
    (void)name;
    semaphore->count = count;
    created++;
    return TX_SUCCESS;
}

UINT tx_semaphore_get(TX_SEMAPHORE *semaphore, ULONG wait_ticks)
{
    last_wait = wait_ticks;
    if (semaphore->count == 0U) { return TX_NO_INSTANCE; }
    semaphore->count--;
    return TX_SUCCESS;
}

UINT tx_semaphore_put(TX_SEMAPHORE *semaphore)
{
    semaphore->count++;
    assert(semaphore->count <= BQUEUE_MAX_BUFFERS);
    return TX_SUCCESS;
}

int main(void)
{
    uint8_t first[4] = {0}, second[4] = {0};
    uint8_t *buffers[] = {first, second};
    app_bqueue_t queue;
    app_bqueue_init(&queue, 2U, buffers);
    assert(created == 2U && queue.free.count == 2U && queue.ready.count == 0U);
    assert(app_bqueue_get_ready_timeout(&queue, 100U) == NULL);
    assert(last_wait == 100U && queue.ready_idx == 0U);
    assert(app_bqueue_get_free(&queue, 0U) == first && last_wait == TX_NO_WAIT);
    assert(app_bqueue_get_free_timeout(&queue, 77U) == second && last_wait == 77U);
    assert(app_bqueue_get_free_timeout(&queue, 77U) == NULL);
    assert(queue.free_idx == 0U && queue.free.count == 0U);
    app_bqueue_put_ready(&queue);
    app_bqueue_put_ready(&queue);
    assert(app_bqueue_get_ready_timeout(&queue, 100U) == first);
    assert(app_bqueue_get_ready_timeout(&queue, 100U) == second);
    assert(app_bqueue_get_ready_timeout(&queue, 100U) == NULL);
    assert(queue.ready_idx == 0U && queue.ready.count == 0U);
    app_bqueue_put_free(&queue);
    app_bqueue_put_free(&queue);
    for (unsigned int i = 0U; i < 5000U; i++) {
        uint8_t *expected = i % 2U ? second : first;
        assert(app_bqueue_get_free_timeout(&queue, 3U) == expected);
        expected[0] = (uint8_t)i;
        app_bqueue_put_ready(&queue);
        uint8_t *actual = app_bqueue_get_ready_timeout(&queue, 5U);
        assert(actual == expected && actual[0] == (uint8_t)i);
        app_bqueue_put_free(&queue);
    }
    /* Stop/quiescence belongs to the supervisor contract. Here both workers
       are idle while pending producer slots and notifications are reclaimed. */
    assert(app_bqueue_get_free_timeout(&queue, 3U) == first);
    assert(app_bqueue_get_free_timeout(&queue, 3U) == second);
    app_bqueue_put_ready(&queue);
    app_bqueue_put_ready(&queue);
    app_bqueue_reset(&queue);
    assert(created == 2U && queue.free.count == 2U && queue.ready.count == 0U);
    assert(queue.free_idx == 0U && queue.ready_idx == 0U);
    assert(app_bqueue_get_ready_timeout(&queue, 10U) == NULL);
    assert(app_bqueue_get_free_timeout(&queue, 10U) == first);
    app_bqueue_put_free(&queue);
    app_bqueue_reset(&queue);
    app_bqueue_reset(&queue);
    assert(queue.free.count == 2U && queue.ready.count == 0U);
    assert(app_bqueue_get_free_timeout(&queue, 10U) == first);
    assert(app_bqueue_get_free_timeout(&queue, 10U) == second);
    assert(app_bqueue_get_free_timeout(&queue, 10U) == NULL);
    puts("PASS: real bqueue finite waits, timeout index stability, cycling and quiescent reset");
    return 0;
}
