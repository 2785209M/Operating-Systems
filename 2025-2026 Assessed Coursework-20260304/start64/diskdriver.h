#ifndef _DISKDRIVER_H_
#define _DISKDRIVER_H_

/*
 * Define the interface to the DiskDriver module that mediates access to
 * a disk device
 */

#include "sectordescriptor.h"
#include "block.h"
#include "pid.h"
#include "freesectordescriptorstore.h"
#include "diskdevice.h"
#include "voucher.h"

/*
 * the following calls must be implemented by the students
 */

/*
 * called before any other methods to allow you to initialize data
 * structures and to start any internal threads.
 *
 * Arguments:
 *   dd: the DiskDevice that you must drive
 *   mem_start, mem_length: some memory for SectorDescriptors
 *   fsds_ptr: you hand back a FreeSectorDescriptorStore constructed
 *             from the memory provided in the two previous arguments
 */
void init_disk_driver(DiskDevice *dd, void *mem_start, unsigned long mem_length,
		      FreeSectorDescriptorStore **fsds);

/*
 * the following calls are used to write a sector to the disk
 * the nonblocking call must return promptly, returning 1 if successful at
 * queueing up the write, 0 if not (in case internal buffers are full)
 * the blocking call will usually return promptly, but there may be
 * a delay while it waits for space in your buffers.
 * neither call should delay until the sector is actually written to the disk
 * for a successful nonblocking call and for the blocking call, a voucher is
 * returned that is required to determine the success/failure of the write
 */
void blocking_write_sector(SectorDescriptor *sd, Voucher **v);
int nonblocking_write_sector(SectorDescriptor *sd, Voucher **v);

/*
 * the following calls are used to initiate the read of a sector from the disk
 * the nonblocking call must return promptly, returning 1 if successful at
 * queueing up the read, 0 if not (in case internal buffers are full)
 * the blocking callwill usually return promptly, but there may be
 * a delay while it waits for space in your buffers.
 * neither call should delay until the sector is actually read from the disk
 * for successful nonblocking call and for the blocking call, a voucher is
 * returned that is required to collect the sector after the read completes.
 */
void blocking_read_sector(SectorDescriptor *sd, Voucher **v);
int nonblocking_read_sector(SectorDescriptor *sd, Voucher **v);

/*
 * the following call is used to retrieve the status of the read or write
 * the return value is 1 if successful, 0 if not
 * the calling application is blocked until the read/write has completed
 * regardless of the status of a read, the associated SectorDescriptor is
 * returned in *sd
 */
int redeem_voucher(Voucher *v, SectorDescriptor **sd);

#endif /* _DISKDRIVER_H_ */
