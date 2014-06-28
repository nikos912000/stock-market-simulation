/**********************************************************************/
/*    StockMarket project 2013                                        */
/*    Skeleton by Nikos Pitsianis                                     */
/*    Implemented by Nikos Katirtzis (nikos912000)                    */
/*																	  */
/*    Based upon the pc.c producer-consumer demo by Andrae Muys       */
/**********************************************************************/

// Includes-defines
#include "StockMarket.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define QUEUESIZE 5000

// Queues to be used
queue *bm_q;		// buy-market queue
queue *sm_q;		// sell-market queue
queue *bl_q;		// buy-limit queue
queue *sl_q;		// sell-limit queue
queue *cancel_q;	// cancel queue

queue *queueInit (void);


/******************** Functions ********************/

// For queues-heaps
void queueAdd(queue *q, order ord);
void queueDel(queue *q, order *ord);
void queueDelete(queue *q);
void heapInsert(queue *q, order ord);
void heapDel(queue *q, order *out);

// For transactions
void MMtrans(queue *q1, queue *q2);
void MLtrans(queue *q1, queue *q2);
void LMtrans(queue *q1, queue *q2);
void LLtrans(queue *q1, queue *q2);

// For cancel
int  queueSearch(queue *q, long id);
int  heapSearch(queue *q, long id);
void queueExtract(queue *q, long i);
void heapExtract(queue *q, long i);

// Thread functions 
void* Prod(void* q);
void* Cons(void* q);
void* BMTry();
void* SMTry();
void* BLTry();
void* SLTry();
void* CancelTry();

// General functions
long getTimestamp();
order makeOrder();
void dispOrder (order ord);
void trace(long timestamp, int price, order ord1, order ord2, int volume);

pthread_mutex_t lock_transaction  = PTHREAD_MUTEX_INITIALIZER;	//mutex used for locking a transaction
int currentPriceX10 = 1000;	//current share price *10

// Log files
FILE *trace_file;
FILE *sharePrice;
//FILE *times;

/******************** Main function ********************/
int main()
{
	// number generator seed for different random sequence per run
    srand(time(NULL));
    //srand(0); // or to get the same sequence
    
    // start the time for timestamps
    gettimeofday (&startwtime, NULL);
    
	/********************************/
	/* prod_t: producer             */
	/* cons_t: consumer             */
	/* bmTry_t: buy market trier    */ 
	/* blTry_t: buy limit trier     */
	/* smTry_t: sell market trier   */
	/* slTry_t: sell market trier   */
	/* cancelTry_t: cancel trier    */
	/********************************/
	
    pthread_t prod_t,cons_t,bmTry_t,smTry_t,blTry_t,slTry_t,cancelTry_t;
	
	// open log files
    trace_file = fopen("trace.txt","wt");
    sharePrice = fopen("sharePrice.txt","wt");
	//times = fopen("times.txt","wt");
    
    queue* q;
	
    // initialize queues
    q = queueInit();
    bm_q = queueInit();
    sm_q = queueInit();
    bl_q = queueInit();
    sl_q = queueInit();
    cancel_q = queueInit();
    
    /********** Create threads **********/
    pthread_create(&prod_t,NULL,Prod,q);
    pthread_create(&cons_t,NULL,Cons,q);
	
	// transaction threads
    pthread_create(&bmTry_t,NULL,BMTry,NULL);
    pthread_create(&smTry_t,NULL,SMTry,NULL);
    pthread_create(&blTry_t,NULL,BLTry,NULL);
    pthread_create(&slTry_t,NULL,SLTry,NULL);
    pthread_create(&cancelTry_t,NULL,CancelTry,NULL);
    
	// Join threads
	// I actually do not expect them to ever terminate
    pthread_join(prod_t,NULL);
    pthread_join(cons_t,NULL);
    pthread_join(bmTry_t,NULL);
    pthread_join(smTry_t,NULL);
    pthread_join(blTry_t,NULL);
    pthread_join(slTry_t,NULL);
    pthread_join(cancelTry_t,NULL);
    
    pthread_exit(NULL);
}

/******************** Producer function ********************/
void *Prod (void *arg)
{
	queue *q = (queue *) arg;
    int magnitude=10;
    while(1)
	{
        // wait for a random amount of time in useconds
        //int waitmsec = ((double)rand() / (double)RAND_MAX * magnitude);
        //usleep(waitmsec*1000);
        
        pthread_mutex_lock (q->mut);
        while (q->full)
		{
			// This is bad and should not happen!
            printf ("*** Incoming Order Queue is FULL.\n"); fflush(stdout);
            pthread_cond_wait (q->notFull, q->mut);
        }
        queueAdd (q, makeOrder());
        pthread_mutex_unlock (q->mut);
        pthread_cond_signal (q->notEmpty);
    }
    return;
}

/******************** Consumer function ********************/
void* Cons (void* arg)
{
    queue *q = (queue *) arg;
    order ord;
    
    while(1)
	{
        // Select an order 
        pthread_mutex_lock (q->mut);
        while (q->empty) 
		{
            printf ("*** Incoming Order Queue is EMPTY.\n"); fflush(stdout);
            pthread_cond_wait(q->notEmpty, q->mut);
        }
        queueDel(q, &ord);
        pthread_mutex_unlock(q->mut);
        pthread_cond_signal(q->notFull);
        
        // Move order from arrival queue to one of our queues
		// and signal appropriate handler to deal with it
		
		switch (ord.type)
		{
			case 'M':
			{
				switch(ord.action)
				{
					case 'B':
					{
						pthread_mutex_lock(bm_q->mut);
						while (bm_q->full) 
						{
							printf ("*** Buy Market Queue is FULL.\n"); fflush(stdout);
							pthread_cond_wait(bm_q->notFull, bm_q->mut);
						}
						
						queueAdd(bm_q, ord);
						pthread_mutex_unlock(bm_q->mut);
						pthread_cond_signal(bm_q->notEmpty);
						break;
					}
					case 'S':
					{
						pthread_mutex_lock(sm_q->mut);
						while (sm_q->full) 
						{
							printf ("*** Sell MarketFIFO is FULL.\n"); fflush(stdout);
							pthread_cond_wait(sm_q->notFull, sm_q->mut);
						}
                
						queueAdd(sm_q, ord);
						pthread_mutex_unlock(sm_q->mut);
						pthread_cond_signal(sm_q->notEmpty);
						break;
					}
					default : break;
				}
				break;
			}
			case 'L':
			{
				switch(ord.action)
				{
					case 'B':
					{
						pthread_mutex_lock(bl_q->mut);
						while (bl_q->full)
						{
							printf ("*** Buy Limit Queue is FULL.\n"); fflush(stdout);
							pthread_cond_wait(bl_q->notFull, bl_q->mut);
						}
                
						heapInsert(bl_q, ord);
						pthread_mutex_unlock(bl_q->mut);
						pthread_cond_signal(bl_q->notEmpty);
						break;
					}
					case 'S':
					{
						pthread_mutex_lock(sl_q->mut);
						while (sl_q->full) 
						{
							printf ("*** Sell Limit Queue is FULL.\n"); fflush(stdout);
							pthread_cond_wait(sl_q->notFull, sl_q->mut);
						}
                
						heapInsert(sl_q, ord);
						pthread_mutex_unlock(sl_q->mut);
						pthread_cond_signal(sl_q->notEmpty);
						//break;
					}
					default : break;
				}
				break;
			}
			case 'C':
			{
				pthread_mutex_lock(cancel_q->mut);
                while (cancel_q->full)
				{
                    printf ("*** Cancel Queue is FULL.\n"); fflush(stdout);
                    pthread_cond_wait(cancel_q->notFull, cancel_q->mut);
                }
            
                queueAdd(cancel_q, ord);
                pthread_mutex_unlock(cancel_q->mut);
                pthread_cond_signal(cancel_q->notEmpty);
				break;
			}
			default : break;
		}

    }
	// Display message when the order is executed
	//printf ("Processing at time %8ld : ", getTimestamp());
	//dispOrder(ord); fflush(stdout);
    return;
}

/******************** Threads-triers ********************/

/********** Try a Buy Market transaction**********/
void* BMTry()
{
    int done = 0;
    
    while(1)
	{
        // Wait for a buy market order 
        pthread_mutex_lock(bm_q->mut);
        while (bm_q->empty) 
		{
            //printf ("*** Buy Market Queue is EMPTY.\n"); fflush(stdout);
            pthread_cond_wait(bm_q->notEmpty, bm_q->mut);
        }
        
        // Try a Buy Market- Sell Limit transaction
        if (pthread_mutex_trylock(sl_q->mut) == 0) 
		{
            if ((sl_q->empty == 0) && (sl_q->item[1].price < currentPriceX10))
			{
                pthread_mutex_lock(&lock_transaction);
                MLtrans (bm_q, sl_q);
                pthread_mutex_unlock(&lock_transaction);
                done = 1;
            }
            pthread_mutex_unlock(sl_q->mut);
        }
        
        // If no Buy Market - Sell Limit transaction was achieved, try a Market - Market transaction
        if (done == 0) 
		{
            if (pthread_mutex_trylock(sm_q->mut) == 0) 
			{
                if (sm_q->empty == 0) 
				{
                    pthread_mutex_lock(&lock_transaction);
                    MMtrans (bm_q, sm_q);
                    pthread_mutex_unlock(&lock_transaction);
                }
                pthread_mutex_unlock(sm_q->mut);
            }
        }
        pthread_mutex_unlock(bm_q->mut);
        done = 0;
    }
    return;
}

/********** Try a Sell Market transaction**********/
void* SMTry()
{
    int done = 0;
    
    while(1)
	{
        
        // Wait for a sell market order 
        pthread_mutex_lock(sm_q->mut);
        while (sm_q->empty) 
		{
            // printf ("*** Sell Market Queue is EMPTY.\n"); fflush(stdout);
            pthread_cond_wait(sm_q->notEmpty, sm_q->mut);
        }
        
        // Try a Sell Market - Buy Limit transaction
        if (pthread_mutex_trylock(bl_q->mut) == 0)
		{
            if ((bl_q->empty == 0) && (bl_q->item[1].price > currentPriceX10))
			{
                pthread_mutex_lock(&lock_transaction);
                MLtrans (sm_q, bl_q);
                pthread_mutex_unlock(&lock_transaction);
                done = 1;
            }
            pthread_mutex_unlock(bl_q->mut);
        }
        
        // If no Sell Market - Buy Limit transaction was achieved, try a Market - Market transaction
        if (done == 0) 
		{
            if (pthread_mutex_trylock(bm_q->mut) == 0)
			{
                if (bm_q->empty == 0)
				{
                    pthread_mutex_lock(&lock_transaction);
                    MMtrans (sm_q, bm_q);
                    pthread_mutex_unlock(&lock_transaction);
                }
                pthread_mutex_unlock(bm_q->mut);
            }
        }
        pthread_mutex_unlock(sm_q->mut);
        done = 0;
    }
    return;
}

/********** Try a Buy Limit transaction**********/
void* BLTry()
{
    int done = 0;
    
    while(1) 
	{
        // Wait for a buy limit order 
        pthread_mutex_lock(bl_q->mut);
        while (bl_q->empty) 
		{
            // printf ("*** Buy Limit Queue is EMPTY.\n"); fflush(stdout);
            pthread_cond_wait(bl_q->notEmpty, bl_q->mut);
        }
        
        // Try a Buy Limit - Sell Market transaction
        if (pthread_mutex_trylock(sm_q->mut) == 0)
		{
            if (sm_q->empty == 0) 
			{
                pthread_mutex_lock(&lock_transaction);
                LMtrans (bl_q, sm_q);
                pthread_mutex_unlock(&lock_transaction);
                done = 1;
            }
            pthread_mutex_unlock(sm_q->mut);
        }
        
        // If no Buy Limit - Sell Market was achieved, try Limit - Limit transaction
        if (done== 0) 
		{
            if (pthread_mutex_trylock(sl_q->mut) == 0)
			{
                if ((sl_q->empty == 0)&&(bl_q->item[1].price >= sl_q->item[1].price))
				{
                    pthread_mutex_lock(&lock_transaction);
                    LLtrans (bl_q, sl_q);
                    pthread_mutex_unlock(&lock_transaction);
                }
                pthread_mutex_unlock(sl_q->mut);
            }
        }
        pthread_mutex_unlock(bl_q->mut);
        done= 0;
    }
    return;
}

/********** Try a Sell Limit transaction**********/
void* SLTry()
{
    int done = 0;
    
    while(1)
	{
        
        // Wait for a sell limit order **/
        pthread_mutex_lock(sl_q->mut);
        while (sl_q->empty)
		{
            // printf ("*** Buy Limit Queue is EMPTY.\n"); fflush(stdout);
            pthread_cond_wait(sl_q->notEmpty, sl_q->mut);
        }
        
        // Try a Sell Limit - Buy Market transaction
        if (pthread_mutex_trylock(bm_q->mut) == 0)
		{
            if (bm_q->empty == 0) 
			{
                pthread_mutex_lock(&lock_transaction);
                LMtrans (sl_q, bm_q);
                pthread_mutex_unlock(&lock_transaction);
                done = 1;
            }
            pthread_mutex_unlock(bm_q->mut);
        }
        
        // If no Sell Limit - Buy Market was achieved, try Limit - Limit transaction
        if (done == 0)
		{
            if (pthread_mutex_trylock(bl_q->mut)== 0)
			{
                if ((bl_q->empty == 0)&&(bl_q->item[1].price >= sl_q->item[1].price))
				{
                    pthread_mutex_lock(&lock_transaction);
                    LLtrans (sl_q, bl_q);
                    pthread_mutex_unlock(&lock_transaction);
                }
                pthread_mutex_unlock(bl_q->mut);
            }
        }
        pthread_mutex_unlock(sl_q->mut);
        done = 0;
    }
    return;
}


/******************** Transaction functions ********************/

/********** Buy Market - Sell Market transaction**********/
void MMtrans (queue *q1, queue *q2)
 {
    int volume = 0;
    order ord1,ord2,trash;
    
    ord1 = q1->item[q1->head];
    ord2 = q2->item[q2->head];
    
    if (ord1.vol > ord2.vol)
	{
        ord1.vol = ord1.vol - ord2.vol;
        volume = ord2.vol;
        queueDel(q2, &trash);
        pthread_cond_signal(q2->notFull);
    }
    else if(ord1.vol < ord2.vol)
	{
        ord2.vol = ord2.vol-ord1.vol;
        volume = ord1.vol;
        queueDel(q1, &trash);
        pthread_cond_signal(q1->notFull);
    }
    else
	{
        volume = ord1.vol;
        queueDel(q1, &trash);
        pthread_cond_signal(q1->notFull);
        queueDel(q2, &trash);
        pthread_cond_signal(q2->notFull);
    }
    trace(getTimestamp(), currentPriceX10, ord1, ord2, volume);
	dispOrder (trash); fflush(stdout);
}

/********** Buy Market - Sell Limit transaction**********/
void MLtrans (queue *q1, queue *q2) 
{
    int volume = 0;
    order trash, ord1,ord2;
    
    ord1 = q1->item[q1->head];
    ord2 = q2->item[1];
    currentPriceX10 = ord2.price;
    
    if (ord1.vol > ord2.vol)
	{
        ord1.vol = ord1.vol - ord2.vol;
        volume = ord2.vol;
        heapDel(q2, &trash);
        pthread_cond_signal(q2->notFull);
    }
    else if(ord1.vol < ord2.vol)
	{
        ord2.vol = ord2.vol - ord1.vol;
        volume = ord1.vol;
        queueDel(q1, &trash);
        pthread_cond_signal(q1->notFull);
    }
    else 
	{
        volume = ord1.vol;
        heapDel(q2, &trash);
        pthread_cond_signal(q2->notFull);
        queueDel(q1, &trash);
        pthread_cond_signal(q1->notFull);
    }
    trace(getTimestamp(), currentPriceX10, ord1, ord2, volume);
	dispOrder (trash); fflush(stdout);
}

/********** Buy Limit - Sell Market transaction**********/
void LMtrans (queue *q1, queue *q2)
 {
    int volume = 0;
    order trash, ord1,ord2;
    
    ord1 = q1->item[1];
    ord2 = q2->item[q2->head];
    currentPriceX10 = ord1.price;
    
    if (ord1.vol > ord2.vol)
	{
        ord1.vol = ord1.vol - ord2.vol;
        volume = ord2.vol;
        queueDel(q2, &trash);
        pthread_cond_signal(q2->notFull);
    }
    else if(ord1.vol < ord2.vol)
	{
        ord2.vol = ord2.vol - ord1.vol;
        volume = ord1.vol;
        heapDel(q1, &trash);
        pthread_cond_signal(q1->notFull);
    }
    else 
	{
        volume = ord1.vol;
        queueDel(q2, &trash);
        pthread_cond_signal(q2->notFull);
        heapDel(q1, &trash);
        pthread_cond_signal(q1->notFull);
    }
    trace(getTimestamp(), currentPriceX10, ord1, ord2, volume);
	dispOrder (trash); fflush(stdout);
}

/********** Buy Limit - Sell Limit transaction**********/
void LLtrans (queue *q1, queue *q2) 
{
    int volume = 0;
    order trash,ord1,ord2;
    
    ord1 = q1->item[1];
    ord2 = q2->item[1];
    currentPriceX10 = (ord1.price + ord2.price)/2;
    
    if (ord1.vol > ord2.vol)
	{
        ord1.vol = ord1.vol - ord2.vol;
        volume = ord2.vol;
        heapDel(q2, &trash);
        pthread_cond_signal(q2->notFull);
    }
    else if(ord1.vol < ord2.vol)
	{
        ord2.vol = ord2.vol - ord1.vol;
        volume = ord1.vol;
        heapDel(q1, &trash);
        pthread_cond_signal(q1->notFull);
    }
    else 
	{
        volume = ord1.vol;
        heapDel(q2, &trash);
        pthread_cond_signal(q2->notFull);
        heapDel(q1, &trash);
        pthread_cond_signal(q1->notFull);
    }
    trace(getTimestamp(), currentPriceX10, ord1, ord2, volume);
	dispOrder (trash); fflush(stdout);
}

/******************** Trace function ********************/
void trace(long timestamp, int price, order ord1, order ord2, int volume)
{
	// write current price  to appropriate file 
    fprintf(sharePrice, "%5.1f\n", (float) price/10.0); fflush(sharePrice);
    
	// write the desired values to trace file
	fprintf(trace_file,"%08ld  %5.1f  %4d  %08ld  %c  %08ld  %c\n", timestamp, (float) price/10.0, volume, ord1.id, ord1.type, ord2.id, ord2.type); fflush(trace_file);
	//fprintf(times,"%08ld\n", timestamp-ord1.timestamp); fflush(times);
}

/******************** Order generator ********************/
order makeOrder()
{
    int magnitude = 10;
    static int count = 0;
    order ord;
    
    int waitmsec = ((double)rand() / (double)RAND_MAX * magnitude);
    usleep(waitmsec*1000);
    
    ord.id = count++;
    ord.timestamp = getTimestamp();
    
    // Buy or Sell
    ord.action = ((double)rand()/(double)RAND_MAX <= 0.5) ? 'B' : 'S';
    
    // Order type
    double u2 = ((double)rand()/(double)RAND_MAX);
    if (u2 < 0.4)
	{
        ord.type = 'M';                 // Market order
        ord.vol = (1 + rand()%50)*100;
        
    }
	else if (0.4 <= u2 && u2 < 0.9)
	{
        ord.type = 'L';                 // Limit order
        ord.vol = (1 + rand()%50)*100;
        
        pthread_mutex_lock(&lock_transaction);
        ord.price = currentPriceX10 + 10*(0.5 -((double)rand()/(double)RAND_MAX));
        pthread_mutex_unlock(&lock_transaction);
    }
    else if (0.9 <= u2)
	{
        ord.type = 'C';                 // Cancel order
        ord.oldid = ((double)rand()/(double)RAND_MAX)*count;
    }
    //dispOrder(ord);
    return (ord);
}

/******************** Get time function ********************/
long getTimestamp()
{
    gettimeofday(&endwtime, NULL);
    
    return((double)((endwtime.tv_usec - startwtime.tv_usec)/1.0e6
                    + endwtime.tv_sec - startwtime.tv_sec)*1000);
}

/******************** Display order function ********************/
void dispOrder(order ord)
{
    printf("%08ld ", ord.id);
    printf("%08ld ", ord.timestamp);
    switch( ord.type )
	{
        case 'M':
            printf("%c ", ord.action);
            printf("Market (%4d)        ", ord.vol); 
			break;
            
        case 'L':
            printf("%c ", ord.action);
            printf("Limit  (%4d,%5.1f) ", ord.vol, (float) ord.price/10.0); 
			break;
            
        case 'C':
            printf("* Cancel  %6ld        ", ord.oldid); 
			break;
        default : break;
    }
    printf("\n");
}

/******************** Queue initilization function ********************/
queue *queueInit (void)
{
    queue *q;
    
    q = (queue *)malloc (sizeof (queue));
    if (q == NULL) return (NULL);
    
	q->size = 0;	//used for heaps(priority queues)
    q->empty = 1;
    q->full = 0;
    q->head = 0;
    q->tail = 0;
    q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
    pthread_mutex_init (q->mut, NULL);
    q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
    pthread_cond_init (q->notFull, NULL);
    q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
    pthread_cond_init (q->notEmpty, NULL);
	
    return (q);
}

/******************** Delete queue function ********************/
void queueDelete (queue *q)
{
    pthread_mutex_destroy (q->mut);
    free (q->mut);
    pthread_cond_destroy (q->notFull);
    free (q->notFull);
    pthread_cond_destroy (q->notEmpty);
    free (q->notEmpty);
    free (q);
}

/******************** Add order to queue function ********************/
void queueAdd (queue *q, order in)
{
    q->item[q->tail] = in;
    q->tail++;
    if (q->tail == QUEUESIZE)
        q->tail = 0;
    if (q->tail == q->head)
        q->full = 1;
    q->empty = 0;
    
    return;
}

/*************** Delete order from queue function***************/
void queueDel (queue *q, order *out)
{
    *out = q->item[q->head];
    
    q->head++;
    if (q->head == QUEUESIZE)
        q->head = 0;
    if (q->head == q->tail)
        q->empty = 1;
    q->full = 0;
    
    return;
}

/*************** Insert order to priority queue (heap) function ( O(logN) time )***************/
void heapInsert(queue *q, order ord)
{
	/*************************************************************************/
	/* Insert new element into the heap, at the next available slot("hole")  */
	/* 	   - maintaining a complete binary tree			                     */
	/* Then, "percolate" the element up the heap, while heap-order property  */
	/* doesn't meet criteria.                                                */
	/*************************************************************************/
	
    int hole;
    order temp;
    
	//percolate up
    hole = ++q->size;
    
	//insert to min heap
	if (ord.action == 'B')
	{
		for (;hole>1 && ord.price>q->item[hole/2].price;hole/=2)
		{
			q->item[hole]=q->item[hole/2];
        }
    }
	
	//insert to max heap
    else
	{
		for (;hole>1 && ord.price<q->item[hole/2].price;hole/=2)
		{
			q->item[hole]=q->item[hole/2];
        }
    }
	q->item[hole] = ord;
	
    if (q->size == QUEUESIZE)
        q->full = 1;
    q->empty = 0;
}

/*************** Delete order from heap function***************/
void heapDel (queue *q, order *out)
{
    /***********************************************************************/
	/* Minimum element is always at the root                               */
	/* Heap decreases by one in size                                       */
	/* Move last element into hole at the root                             */
	/* Percolate down while heap-order property not satisfied              */
	/***********************************************************************/
	
    *out = q->item[1];
	//q->item[1] is the minimum/maximum element
    q->item[1] = q->item[q->size--];
    
    int child,hole;
    order temp;
	hole = 1;
	temp=q->item[hole];
    if (out->action == 'B') 
	{
		// max heap
		// percolate down
        for(;hole*2<=q->size;hole=child) 
		{
			// left child=hole*2, right child=hole*2+1
            child = hole*2;
            
            if(child!=q->size && q->item[child].price < q->item[child+1].price)
			{
                 child++;
            }
			// pick child to swap with
            if(q->item[child].price<temp.price) 
			{
                q->item[hole]=q->item[child];
            }
            else
                break;
        }
        
    } 
	else
	{
        // min heap 
		for(;hole*2<=q->size;hole=child)
		{
			// left child=hole*2, right child=hole*2+1
            child = hole*2;
            
            if(child!=q->size && q->item[child].price > q->item[child+1].price)
			{
                 child++;
            }
			// pick child to swap with
            if(q->item[child].price>temp.price) 
			{
                q->item[hole]=q->item[child];
            }
            else
                break;
        }
    }
	q->item[hole]=temp;
    if (q->size == 0)
        q->empty = 1;
    q->full = 0;
}

/*************** Try a cancel thread ***************/
void *CancelTry()
{
    order ord;
    long id;
    
    while(1) 
	{
        pthread_mutex_lock(cancel_q->mut);
        while(cancel_q->empty)
		{
            //  printf("*** Cancel Order Queue is Empty.\n");
            pthread_cond_wait(cancel_q->notEmpty, cancel_q->mut);
        }
        queueDel(cancel_q, &ord);
        pthread_mutex_unlock(cancel_q->mut);
        pthread_cond_signal (cancel_q->notFull);
        
        id = ord.oldid;
            
        // Search everywhere for the id to be cancelled
        if( queueSearch(bm_q,id) )
		{
            printf("Canceled\n"); 
			fflush(stdout);
		}	
        else if( queueSearch(sm_q,id) )
        {
 	 		printf("Canceled\n");
 			fflush(stdout);
		}
        else if( heapSearch(bl_q,id) )
        {
    		printf("Canceled\n");
	 		fflush(stdout);
		}        
		else if( heapSearch(sl_q,id) )
        {
   			printf("Canceled\n");
 			fflush(stdout);
        }
		else
        {    
			printf("Not Found\n");
			fflush(stdout);
		}
    }
    return;
}

/******************** Search id in a queue function ********************/
int queueSearch(queue *q,long id)
{
    int i;
    
    for (i = q->head; i != q->tail ; i = (i+1)%QUEUESIZE)
	{
        if (q->item[i].id == id)
		{
            pthread_mutex_lock( q->mut );
            queueExtract(q,i);
            pthread_mutex_unlock(q->mut);
            pthread_cond_signal(q->notFull);
            return (1);
        }
    }
    return (0);
}

/******************** Search id in a heap function ********************/
int heapSearch( queue *q,long id)
{
    int i;
    
    for (i = 1; i <=q->size ; i++)
	{
        if (q->item[i].id == id)
		{
            pthread_mutex_lock(q->mut);
            heapExtract(q,i);
            pthread_mutex_unlock(q->mut);
            pthread_cond_signal(q->notFull);
            return (1);
        }
    }
    return (0);
}

/******************** Extract order in 'index' position from a queue function ********************/
void queueExtract( queue *q, long index)
{
    int j;
    
    if(q->tail > q->head)
	{
        for(j = index; j < q->tail-1; j++)
        {
			q->item[j] = q->item[j+1];
        }
		q->tail--;
        
        if (q->tail == q->head)
            q->empty= 1;
        q->full = 0;
        
    }
	else if (q->head > q->tail)
	{
        if (index >= q->head)
		{
            for(j = index-1; j >= q->head; j--)
            {
				q->item[j+1] = q->item[j];
            }
			q->head++;
            
            if (q->head == QUEUESIZE)
                q->head = 0;
            if (q->head == q->tail)
                q->empty = 1;
            q->full = 0;
        }
		else
		{
            for(j = index; j < q->tail-1; j++)
            {
				q->item[j] = q->item[j+1];
            }
			q->tail--;
        }
    }
}


/******************** Extract order in 'index' position from a heap function ********************/
void heapExtract(queue *q, long index)
{
    order out;
    
    out = q->item[index];
	//q->item[i] is the extracted element
    q->item[index] = q->item[q->size--];
    
    int child,hole;
    order temp;
	hole = index;
	temp=q->item[hole];
    if (out.action == 'B')
	{
		// max heap
		// percolate down
        for(;hole*2<=q->size;hole=child)
		{
			// left child=hole*2, right child=hole*2+1
            child = hole*2;
            
            if(child!=q->size && q->item[child].price < q->item[child+1].price)
			{
                 child++;
            }
			// pick child to swap with
            if(q->item[child].price<temp.price)
			{
                q->item[hole]=q->item[child];
            }
            else
                break;
        }
        
    } 
	else 
	{
        // min heap 
		for(;hole*2<=q->size;hole=child) 
		{
			// left child=hole*2, right child=hole*2+1
            child = hole*2;
            
            if(child!=q->size && q->item[child].price > q->item[child+1].price)
			{
                child++;
            }
			// pick child to swap with
            if(q->item[child].price>temp.price) 
			{
                q->item[hole]=q->item[child];
            }
            else
                break;
        }
    }
	q->item[hole]=temp;
    if (q->size == 0)
        q->empty = 1;
    q->full = 0;
}