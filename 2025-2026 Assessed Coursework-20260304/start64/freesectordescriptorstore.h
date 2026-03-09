#ifndef _FREE_SECTOR_DESCRIPTOR_STORE_H_
#define _FREE_SECTOR_DESCRIPTOR_STORE_H_

/*
 * header file for FreeSectorDescriptorStore ADT
 */

#include "sectordescriptor.h"

typedef struct freesectordescriptorstore FreeSectorDescriptorStore;

/*
 * as usual, the blocking versions only return when they succeed
 * the nonblocking versions return 1 if successful, 0 otherwise
 * the _get_ functions set their final argument if they succeed
 */

void blocking_get_sd(FreeSectorDescriptorStore *fsds, SectorDescriptor **sd);
int nonblocking_get_sd(FreeSectorDescriptorStore *fsds, SectorDescriptor **sd);

void blocking_put_sd(FreeSectorDescriptorStore *fsds, SectorDescriptor *sd);
int nonblocking_put_sd(FreeSectorDescriptorStore *fsds, SectorDescriptor *sd);

#endif /* _FREE_SECTOR_DESCRIPTOR_STORE_H_ */
