#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stdbool.h>
#include "shared.h"

int get_shared_memory(key_t key, unsigned int size);
void* attach_shared_memory(int shmemid, unsigned int readonly);
void cleanup_shared_memory(int shmemid, void* p);
void detach_from_shared_memory(void* p);
void deallocate_shared_memory(int shmemid);

#endif
