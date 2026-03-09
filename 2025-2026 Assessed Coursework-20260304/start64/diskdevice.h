#ifndef _DISKDEVICE_H_
#define _DISKDEVICE_H_

/*
 * header file for DiskDevice ADT
 */

#include "sectordescriptor.h"
#include "block.h"
#include "pid.h"

typedef struct disk_device DiskDevice;

/*
 * write sector to disk, returns 1 if successful, 0 if unsuccessful
 * this may take a substantial amount of time to return
 * if unsuccessful, you should return an indication of this lack of
 * success to the application
 */
int write_sector(DiskDevice *dd, SectorDescriptor *sd);

/*
 * read sector from disk, returns 1 if successful, 0 if unsuccessful
 * if successful, the sector read is contained in the SectorDescriptor
 * this may take a substantial amount of time to return
 * if unsuccessful, you should return an indication of this lack of
 * success to the application
 */
int read_sector(DiskDevice *dd, SectorDescriptor *sd);

#endif /* _DISKDEVICE_H_ */
