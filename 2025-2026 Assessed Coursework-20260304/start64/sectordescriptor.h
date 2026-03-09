#ifndef _SECTOR_DESCRIPTOR_HDR_
#define _SECTOR_DESCRIPTOR_HDR_

/*
 * header file for SectorDescriptor ADT
 */

#include "block.h"
#include "pid.h"

typedef struct sectordescriptor SectorDescriptor;

/* a sector descriptor is a pointer to 64 bytes of memory */
#define SIZEOF_SectorDescriptor 64

/* resets the sector descriptor to empty - used by fake applications */
void init_sector_descriptor(SectorDescriptor *sd);

/*
 * the next few functions are used to set/get the PID and Block Number
 */
void sector_descriptor_set_pid(SectorDescriptor *sd, Pid pid);
void sector_descriptor_set_block(SectorDescriptor *sd, Block block);
Pid sector_descriptor_get_pid(SectorDescriptor *sd);
Block sector_descriptor_get_block(SectorDescriptor *sd);

/*
 * routines for actually manipulating the data written to/read from the
 * disk are omitted, as we don't actually bother with that data in this
 * test harness
 */

#endif /* _SECTOR_DESCRIPTOR_HDR_ */
