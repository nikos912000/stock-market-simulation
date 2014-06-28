#include <pthread.h>

#define QUEUESIZE 5000

/******************** Structs ********************/

// Order struct
typedef struct
 {
    long id;             // identification number
    long oldid;          // old identification number for Cancel
    long timestamp;      // time of order placement
    int  vol;            // number of shares
    int  price;          // price limit for Limit orders
    char action;         // 'B' for buy | 'S' for sell
    char type;           // 'M' for market | 'L' for limit | 'C' for cancel
} order;

// The struct timeval structure represents an elapsed time
struct timeval startwtime, endwtime;

// Queue struct
typedef struct
{
    order item[QUEUESIZE];
    long head, tail;
    int full, empty;
    int size;
    pthread_mutex_t *mut;
    pthread_cond_t *notFull, *notEmpty;
} queue;