#ifndef BUS_TEST_TX_API_H
#define BUS_TEST_TX_API_H
#include <stdint.h>
typedef uint32_t UINT;
typedef uint32_t ULONG;
typedef struct { uint32_t created; } TX_MUTEX;
typedef struct { uint32_t unused; } TX_THREAD;
extern uint32_t mock_irq_depth;
extern uint32_t mock_context;
#define TX_NULL ((void *)0)
#define TX_SUCCESS 0U
#define TX_NOT_AVAILABLE 1U
#define TX_INHERIT 1U
#define TX_TIMER_TICKS_PER_SECOND 1000U
#define TX_INTERRUPT_SAVE_AREA uint32_t saved_irq;
#define TX_DISABLE saved_irq = mock_irq_depth; mock_irq_depth++;
#define TX_RESTORE mock_irq_depth = saved_irq;
#define TX_THREAD_GET_SYSTEM_STATE() mock_context
TX_THREAD *tx_thread_identify(void);
UINT tx_mutex_create(TX_MUTEX *,char *,UINT);
UINT tx_mutex_get(TX_MUTEX *,ULONG);
UINT tx_mutex_put(TX_MUTEX *);
#endif
