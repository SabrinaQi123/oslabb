#ifndef PIPE_H
#define PIPE_H

#include <kernel.h>

#define SAVE_LOCATION 0x5fffff00

int saveint(int x) {
    *(int*)SAVE_LOCATION = x;
    return SAVE_LOCATION;
}

int loadint() { return *(int*)SAVE_LOCATION; }

#endif
