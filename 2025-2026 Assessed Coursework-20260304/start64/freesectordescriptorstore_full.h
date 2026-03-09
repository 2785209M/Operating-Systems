#ifndef _FREE_SECTOR_DESCRIPTOR_STORE_FULL_H_
#define _FREE_SECTOR_DESCRIPTOR_STORE_FULL_H_

/*
 * extension to header file for FreeSectorDescriptorStore ADT to
 * define constructor and destructor
 */

#include "freesectordescriptorstore.h"

FreeSectorDescriptorStore *create_fsds(void);
void destroy_fsds(FreeSectorDescriptorStore *fsds);

#endif /* _FREE_SECTOR_DESCRIPTOR_STORE_FULL_H_ */
