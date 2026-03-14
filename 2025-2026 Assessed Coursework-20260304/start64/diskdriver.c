#include <stdlib.h>
#include <pthread.h>

#include "diskdriver.h"
#include "BoundedBuffer.h"
#include "freesectordescriptorstore_full.h"
#include "sectordescriptorcreator.h"

#define REQUEST_QUEUE_SIZE 50

// A request packages the sector, its type and its return voucher
typedef struct {
    int is_write;
    SectorDescriptor *sd;
    Voucher *v;
} Request;

// The voucher needs to be thread-safe for redemption
struct voucher {
    pthread_mutex_t lock;
    pthread_cond_t completed_cond;
    int completed;
    int status;
    SectorDescriptor *sd;
};

static DiskDevice *driver_disk_device;
static BoundedBuffer *write_request_queue;
static BoundedBuffer *read_request_queue;
static pthread_t write_worker_thread;
static pthread_t read_worker_thread;

static Voucher *create_voucher(SectorDescriptor *sd) {
    Voucher *v = (Voucher *)malloc(sizeof(Voucher));
    if (v == NULL) return NULL;

    pthread_mutex_init(&v->lock, NULL);
    pthread_cond_init(&v->completed_cond, NULL);
    v->completed = 0;
    v->status = 0;
    v->sd = sd;
    return v;
}

static void destroy_voucher(Voucher *v) {
    pthread_mutex_destroy(&v->lock);
    pthread_cond_destroy(&v->completed_cond);
    free(v);
}

// Worker thread function to process write requests
static void *write_worker(void *arg) {
    while (1) {
        Request *req = (Request *)blockingReadBB(write_request_queue);
        if (req == NULL) break; // Sentinel for shutdown

        int status = write_sector(driver_disk_device, req->sd);

        pthread_mutex_lock(&req->v->lock);
        req->v->status = status;
        req->v->completed = 1;
        pthread_cond_signal(&req->v->completed_cond);
        pthread_mutex_unlock(&req->v->lock);

        free(req);
    }
    return NULL;
}

// Worker thread function to process read requests
static void *read_worker(void *arg) {
    while (1) {
        Request *req = (Request *)blockingReadBB(read_request_queue);
        if (req == NULL) break; // Sentinel for shutdown

        int status = read_sector(driver_disk_device, req->sd);

        pthread_mutex_lock(&req->v->lock);
        req->v->status = status;
        req->v->completed = 1;
        pthread_cond_signal(&req->v->completed_cond);
        pthread_mutex_unlock(&req->v->lock);

        free(req);
    }
    return NULL;
}

void init_disk_driver(DiskDevice *dd, void *mem_start, unsigned long mem_length,
                      FreeSectorDescriptorStore **fsds) {
    driver_disk_device = dd;

    if (fsds != NULL) {
        *fsds = create_fsds();
        if (*fsds != NULL) {
            create_free_sector_descriptors(*fsds, mem_start, mem_length);
        }
    }

    write_request_queue = createBB(REQUEST_QUEUE_SIZE);
    read_request_queue = createBB(REQUEST_QUEUE_SIZE);

    // In a real scenario, we'd handle creation failures
    pthread_create(&write_worker_thread, NULL, write_worker, NULL);
    pthread_create(&read_worker_thread, NULL, read_worker, NULL);
}

static int queue_request(int is_write, SectorDescriptor *sd, Voucher **v, int is_blocking) {
    if (driver_disk_device == NULL || sd == NULL || v == NULL) {
        if (v != NULL) *v = NULL;
        return 0;
    }

    Request *req = (Request *)malloc(sizeof(Request));
    if (req == NULL) {
        *v = NULL;
        return 0;
    }

    *v = create_voucher(sd);
    if (*v == NULL) {
        free(req);
        return 0;
    }

    req->is_write = is_write;
    req->sd = sd;
    req->v = *v;

    BoundedBuffer *queue = is_write ? write_request_queue : read_request_queue;

    if (is_blocking) {
        blockingWriteBB(queue, req);
        return 1;
    } else {
        if (nonblockingWriteBB(queue, req)) {
            return 1;
        } else {
            destroy_voucher(*v);
            *v = NULL;
            free(req);
            return 0;
        }
    }
}

void blocking_write_sector(SectorDescriptor *sd, Voucher **v) {
    queue_request(1, sd, v, 1);
}

int nonblocking_write_sector(SectorDescriptor *sd, Voucher **v) {
    return queue_request(1, sd, v, 0);
}

void blocking_read_sector(SectorDescriptor *sd, Voucher **v) {
    queue_request(0, sd, v, 1);
}

int nonblocking_read_sector(SectorDescriptor *sd, Voucher **v) {
    return queue_request(0, sd, v, 0);
}

int redeem_voucher(Voucher *v, SectorDescriptor **sd) {
    if (v == NULL) {
        if (sd != NULL) *sd = NULL;
        return 0;
    }

    pthread_mutex_lock(&v->lock);
    while (!v->completed) {
        pthread_cond_wait(&v->completed_cond, &v->lock);
    }
    pthread_mutex_unlock(&v->lock);

    if (sd != NULL) {
        *sd = v->sd;
    }

    int status = v->status;
    destroy_voucher(v);
    return status;
}