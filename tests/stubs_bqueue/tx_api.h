#ifndef TEST_BQUEUE_TX_API_H
#define TEST_BQUEUE_TX_API_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t ULONG;
typedef unsigned int UINT;
typedef struct { ULONG count; } TX_SEMAPHORE;

#define TX_SUCCESS (0U)
#define TX_NO_INSTANCE (1U)
#define TX_NO_WAIT (0U)
#define TX_WAIT_FOREVER UINT32_MAX

UINT tx_semaphore_create(TX_SEMAPHORE *semaphore, const char *name, ULONG count);
UINT tx_semaphore_get(TX_SEMAPHORE *semaphore, ULONG wait_ticks);
UINT tx_semaphore_put(TX_SEMAPHORE *semaphore);

#endif
