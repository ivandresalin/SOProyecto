#ifndef BAKERY_H
#define BAKERY_H

#include "store_p2p.h"

void bakery_lock_init(BakeryLock *lock);
int bakery_lock_register_thread(BakeryLock *lock);
void bakery_lock_acquire(BakeryLock *lock, int my_id);
void bakery_lock_release(BakeryLock *lock, int my_id);

#endif