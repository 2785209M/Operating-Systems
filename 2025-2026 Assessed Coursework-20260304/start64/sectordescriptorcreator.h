#ifndef _SECTOR_DESCRIPTOR_CREATOR_HDR_
#define _SECTOR_DESCRIPTOR_CREATOR_HDR_

/*
 * define the function prototype for create_free_sector_descriptors()
 */

#include "freesectordescriptorstore.h"

void create_free_sector_descriptors( FreeSectorDescriptorStore *fsds,
		                     void *mem_start, unsigned long mem_length);

#endif /* _SECTOR_DESCRIPTOR_CREATOR_HDR_ */
