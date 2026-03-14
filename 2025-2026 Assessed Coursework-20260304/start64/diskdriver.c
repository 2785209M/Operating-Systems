#include <stdlib.h>
#include <pthread.h>

#include "diskdriver.h"
#include "freesectordescriptorstore_full.h" 
#include "sectordescriptorcreator.h"

// 1. Bounded Data Structures

#define MAX_VOUCHERS 100
#define QUEUE_CAPACITY 50

struct voucher {
    int in_use;       // Flag to manage our static pool
    int completed;
    int status;
    SectorDescriptor *sd;
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

// Static voucher pool to avoid malloc()
static struct voucher voucher_pool[MAX_VOUCHERS];
static pthread_mutex_t pool_lock = PTHREAD_MUTEX_INITIALIZER;

// A request packages the sector and its return voucher
typedef struct {
    SectorDescriptor *sd;
    Voucher *v;
} Request;

// Bounded queue structure
typedef struct {
    Request buffer[QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} BoundedQueue;

static BoundedQueue write_queue;
static BoundedQueue read_queue;

static DiskDevice *driver_disk_device;
static FreeSectorDescriptorStore *driver_fsds;

// 2. Internal Helpers

// Initialize a bounded queue
static void init_queue(BoundedQueue *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

// Push to queue (blocking if full)
static void push_queue_blocking(BoundedQueue *q, SectorDescriptor *sd, Voucher *v) {
    pthread_mutex_lock(&q->lock);
    while (q->count == QUEUE_CAPACITY) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    q->buffer[q->tail].sd = sd;
    q->buffer[q->tail].v = v;
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

// Push to queue (returns 0 if full, 1 on success)
static int push_queue_nonblocking(BoundedQueue *q, SectorDescriptor *sd, Voucher *v) {
    pthread_mutex_lock(&q->lock);
    if (q->count == QUEUE_CAPACITY) {
        pthread_mutex_unlock(&q->lock);
        return 0; // Failed to queue
    }
    q->buffer[q->tail].sd = sd;
    q->buffer[q->tail].v = v;
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 1;
}

// Pop from queue (blocks if empty)
static Request pop_queue(BoundedQueue *q) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    Request req = q->buffer[q->head];
    q->head = (q->head + 1) % QUEUE_CAPACITY;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return req;
}

// Retrieve a free voucher from the static pool
static Voucher *get_free_voucher() {
    pthread_mutex_lock(&pool_lock);
    for (int i = 0; i < MAX_VOUCHERS; i++) {
        if (!voucher_pool[i].in_use) {
            voucher_pool[i].in_use = 1;
            voucher_pool[i].completed = 0;
            voucher_pool[i].status = 0;
            voucher_pool[i].sd = NULL;
            pthread_mutex_unlock(&pool_lock);
            return &voucher_pool[i];
        }
    }
    pthread_mutex_unlock(&pool_lock);
    return NULL; // Pool is empty
}

// 3. Worker Threads

// Thread for dispatching writes to the device
static void *write_worker(void *arg) {
    while (1) {
        Request req = pop_queue(&write_queue);
        
        // Write to physical disk (this is slow and blocks the thread)
        int status = write_sector(driver_disk_device, req.sd);

        // Signal application via voucher
        pthread_mutex_lock(&req.v->lock);
        req.v->status = status;
        req.v->completed = 1;
        pthread_cond_signal(&req.v->cond);
        pthread_mutex_unlock(&req.v->lock);

        // The driver must return the SD to the store after writing
        blocking_put_sd(driver_fsds, req.sd);
    }
    return NULL;
}

// Thread for dispatching reads to the device
static void *read_worker(void *arg) {
    while (1) {
        Request req = pop_queue(&read_queue);
        
        // Read from physical disk (this is slow and blocks the thread)
        int status = read_sector(driver_disk_device, req.sd);

        // Signal application via voucher
        pthread_mutex_lock(&req.v->lock);
        req.v->status = status;
        req.v->sd = req.sd; // Pass the read sector back to the application
        req.v->completed = 1;
        pthread_cond_signal(&req.v->cond);
        pthread_mutex_unlock(&req.v->lock);
        
        // Note: For reads, applications return the SD to the store
    }
    return NULL;
}

// 4. API Implementation

void init_disk_driver(DiskDevice *dd, void *mem_start, unsigned long mem_length,
                      FreeSectorDescriptorStore **fsds)
{
    driver_disk_device = dd;

    if (fsds == NULL) return;

    // Initialize the FreeSectorDescriptorStore
    *fsds = FreeSectorDescriptorStore_create(mem_start, mem_length); // Assuming this signature exists
    driver_fsds = *fsds; 
    
    // Initialize Vouchers
    for (int i = 0; i < MAX_VOUCHERS; i++) {
        voucher_pool[i].in_use = 0;
        pthread_mutex_init(&voucher_pool[i].lock, NULL);
        pthread_cond_init(&voucher_pool[i].cond, NULL);
    }

    // Initialize Queues
    init_queue(&write_queue);
    init_queue(&read_queue);

    // Spawn dedicated threads
    pthread_t w_thread, r_thread;
    pthread_create(&w_thread, NULL, write_worker, NULL);
    pthread_create(&r_thread, NULL, read_worker, NULL);
}

void blocking_write_sector(SectorDescriptor *sd, Voucher **v)
{
    Voucher *new_v = get_free_voucher();
    if (v != NULL) *v = new_v;
    
    // Queue up the write, blocking if our internal buffer is full
    push_queue_blocking(&write_queue, sd, new_v);
}

int nonblocking_write_sector(SectorDescriptor *sd, Voucher **v)
{
    Voucher *new_v = get_free_voucher();
    
    // Attempt to queue up the write, returning 0 immediately if full
    if (push_queue_nonblocking(&write_queue, sd, new_v)) {
        if (v != NULL) *v = new_v;
        return 1;
    } else {
        // Free voucher since we failed to queue
        new_v->in_use = 0; 
        return 0;
    }
}

void blocking_read_sector(SectorDescriptor *sd, Voucher **v)
{
    Voucher *new_v = get_free_voucher();
    if (v != NULL) *v = new_v;
    
    // Queue up the read, blocking if our internal buffer is full
    push_queue_blocking(&read_queue, sd, new_v);
}

int nonblocking_read_sector(SectorDescriptor *sd, Voucher **v)
{
    Voucher *new_v = get_free_voucher();
    
    // Attempt to queue up the read, returning 0 immediately if full
    if (push_queue_nonblocking(&read_queue, sd, new_v)) {
        if (v != NULL) *v = new_v;
        return 1;
    } else {
        new_v->in_use = 0;
        return 0;
    }
}

int redeem_voucher(Voucher *v, SectorDescriptor **sd)
{
    if (v == NULL) return 0;

    pthread_mutex_lock(&v->lock);
    // Block the application thread until the driver thread sets completed to 1
    while (v->completed == 0) {
        pthread_cond_wait(&v->cond, &v->lock);
    }
    
    int status = v->status;
    if (sd != NULL) {
        *sd = v->sd; // Will be NULL for writes, populated for reads
    }
    
    // Free the voucher back into the static pool
    v->in_use = 0; 
    pthread_mutex_unlock(&v->lock);

    return status;
}