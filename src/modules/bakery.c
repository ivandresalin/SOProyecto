#include "bakery.h"

void bakery_lock_init(BakeryLock *l) {
    atomic_init(&l->thread_count, 0);
    for (int i = 0; i < MAX_THREADS; i++) {
        atomic_init(&l->choosing[i], false);
        atomic_init(&l->number[i], 0);
    }
}

int bakery_lock_register_thread(BakeryLock *l) {
    int id = atomic_fetch_add(&l->thread_count, 1);
    if (id >= MAX_THREADS) {
        atomic_fetch_sub(&l->thread_count, 1);
        return -1;
    }
    return id;
}

void bakery_lock_acquire(BakeryLock *l, int my_id) {
    l->choosing[my_id] = true;

    int n = atomic_load(&l->thread_count);
    uint32_t max = 0;
    for (int i = 0; i < n; i++) {
        uint32_t num = atomic_load(&l->number[i]);
        if (num > max) max = num;
    }
    l->number[my_id] = max + 1;
    l->choosing[my_id] = false;

    for (int i = 0; i < n; i++) {
        if (i == my_id) continue;
        while (atomic_load(&l->choosing[i])) { }  // i todavia esta eligiendo numero
        while (atomic_load(&l->number[i]) != 0 &&
               (atomic_load(&l->number[i]) < atomic_load(&l->number[my_id]) ||
                (atomic_load(&l->number[i]) == atomic_load(&l->number[my_id]) && i < my_id))) { }
    }
}

void bakery_lock_release(BakeryLock *l, int my_id) {
    l->number[my_id] = 0;
}