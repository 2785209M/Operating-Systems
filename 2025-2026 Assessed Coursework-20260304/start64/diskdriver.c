#include <stdlib.h>
#include <pthread.h>

#include "diskdriver.h"
#include "BoundedBuffer.h"
#include "freesectordescriptorstore_full.h"
#include "sectordescriptorcreator.h"

#define REQUEST_QUEUE_SIZE 200
#define MAX_VOUCHERS 500

// A request packages the sector, its type and its return voucher
typedef struct {
    int is_read;
    SectorDescriptor *sd;
    Voucher *v;
} Request;

// The voucher uses a pre-allocated pool to avoid mutex creation/destruction races
struct voucher {
    pthread_mutex_t lock;
    pthread_cond_t completed_cond;
    int completed;
    int status;
    SectorDescriptor *sd;
    int in_use;
    int is_read;
};

static DiskDevice *driver_disk_device;
static BoundedBuffer *read_request_queue;
static BoundedBuffer *write_request_queue;
static pthread_t read_worker_thread;
static pthread_t write_worker_thread;
static FreeSectorDescriptorStore *global_fsds;

// Static pool of vouchers to avoid dynamic allocation races
static struct voucher voucher_pool[MAX_VOUCHERS];
static pthread_mutex_t pool_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pool_available_cond = PTHREAD_COND_INITIALIZER;

static void init_voucher_pool(void) {
    for (int i = 0; i < MAX_VOUCHERS; i++) {
        pthread_mutex_init(&voucher_pool[i].lock, NULL);
        pthread_cond_init(&voucher_pool[i].completed_cond, NULL);
        voucher_pool[i].in_use = 0;
    }
}

static Voucher *allocate_voucher(void) {
    pthread_mutex_lock(&pool_lock);
    while (1) {
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
        // No vouchers available, wait for one to be freed
        pthread_cond_wait(&pool_available_cond, &pool_lock);
    }
}

static void free_voucher(Voucher *v) {
    if (v == NULL) return;
    pthread_mutex_lock(&pool_lock);
    v->in_use = 0;
    pthread_cond_signal(&pool_available_cond);
    pthread_mutex_unlock(&pool_lock);
}

// Worker thread function to process write requests
static void *write_worker(void *arg) {
    (void)arg;
    while (1) {
        Voucher *v = (Voucher *)blockingReadBB(write_request_queue);
        if (v == NULL) break; // Sentinel for graceful shutdown

        int status = write_sector(driver_disk_device, v->sd);
        
        // Return the descriptor to the store after the write completes, 
        // regardless of success or failure.
        if (global_fsds != NULL) {
            blocking_put_sd(global_fsds, v->sd);
            v->sd = NULL;
        }

        pthread_mutex_lock(&v->lock);
        v->status = status;
        v->completed = 1;
        pthread_cond_signal(&v->completed_cond);
        pthread_mutex_unlock(&v->lock);
    }
    return NULL;
}

// Worker thread function to process read requests
static void *read_worker(void *arg) {
    (void)arg;
    while (1) {
        Voucher *v = (Voucher *)blockingReadBB(read_request_queue);
        if (v == NULL) break; // Sentinel for graceful shutdown

        int status = read_sector(driver_disk_device, v->sd);

        // We DO NOT return the SD here. The application handles it for reads!

        pthread_mutex_lock(&v->lock);
        v->status = status;
        v->completed = 1;
        pthread_cond_signal(&v->completed_cond);
        pthread_mutex_unlock(&v->lock);
    }
    return NULL;
}

void init_disk_driver(DiskDevice *dd, void *mem_start, unsigned long mem_length,
                      FreeSectorDescriptorStore **fsds) {
    driver_disk_device = dd;

    // Initialize the voucher pool
    init_voucher_pool();

    if (fsds != NULL) {
        *fsds = create_fsds();
        if (*fsds != NULL) {
            create_free_sector_descriptors(*fsds, mem_start, mem_length);
            global_fsds = *fsds;
        }
    }

    write_request_queue = createBB(REQUEST_QUEUE_SIZE);
    read_request_queue = createBB(REQUEST_QUEUE_SIZE);

    pthread_create(&write_worker_thread, NULL, write_worker, NULL);
    pthread_create(&read_worker_thread, NULL, read_worker, NULL);
}

static int queue_request(int is_read, SectorDescriptor *sd, Voucher **v, int is_blocking) {
    if (driver_disk_device == NULL || sd == NULL || v == NULL) {
        if (v != NULL) *v = NULL;
        return 0;
    }

    *v = allocate_voucher();
    // allocate_voucher() blocks until one is available, never returns NULL
    (*v)->sd = sd;
    (*v)->is_read = is_read;

    BoundedBuffer *queue = is_read ? read_request_queue : write_request_queue;

    if (is_blocking) {
        blockingWriteBB(queue, *v); // Pass the voucher directly
        return 1;
    } else {
        if (nonblockingWriteBB(queue, *v)) {
            return 1;
        } else {
            free_voucher(*v);
            *v = NULL;
            return 0;
        }
    }
}

void blocking_write_sector(SectorDescriptor *sd, Voucher **v) {
    queue_request(0, sd, v, 1);
}

int nonblocking_write_sector(SectorDescriptor *sd, Voucher **v) {
    return queue_request(0, sd, v, 0);
}

void blocking_read_sector(SectorDescriptor *sd, Voucher **v) {
    queue_request(1, sd, v, 1);
}

int nonblocking_read_sector(SectorDescriptor *sd, Voucher **v) {
    return queue_request(1, sd, v, 0);
}

int redeem_voucher(Voucher *v, SectorDescriptor **sd) {
    if (v == NULL) {
        if (sd != NULL) *sd = NULL;
        return 0; // The return value is 1 if successful, 0 if not[cite: 112].
    }

    pthread_mutex_lock(&v->lock);
    while (!v->completed) {
        pthread_cond_wait(&v->completed_cond, &v->lock);
    }
    pthread_mutex_unlock(&v->lock);

    // Handout specifies: "if a successful read, the associated Sector Descriptor 
    // is returned in *sd".
    // Only return the descriptor if it was a Successful read
    if (sd != NULL && v->is_read) {
        *sd = v->sd; 
    }

    int status = v->status;
    free_voucher(v);
    return status;
}