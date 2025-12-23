typedef struct spinlock {
    int ticket; 
    int turn;   
} spinlock_t;

void init_spinlock(spinlock_t *lock) {
    lock->ticket = 0;
    lock->turn = 0;
}

void acquire_spinlock1(spinlock_t *lock) 
 
    int my_turn = Fetch_and_Increment(&lock->ticket);

  
  

void release_spinlock1(spinlock_t *lock) {

    lock->turn++; 
}





typedef struct spinlock {
    int status;
} spinlock_t;

void acquire_spinlock2(spinlock * lock) {
    int not_success = 1;
    while (not_success) {
        Compare_and_Swap(&lock->status, 0, &not_success);
    }
}

void release_spinlock2(spinlock * lock) {
    lock->status = 0;
}

void do_something() {
    spinlock_t lock;
    acquire_spinlock2(&lock);
    // critical_section
    release_spinlock2(&lock);
}