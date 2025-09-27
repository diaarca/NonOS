#ifndef __MIGRATE_H__
#define __MIGRATE_H__

#include <sys/stat.h>
#include "network.h"
#include "bitmap.h"
#include "system.h"
#include "post.h"
#include "addrspace.h"


bool SendProcess(int farAddr);

int ListenProcess();


#endif 

