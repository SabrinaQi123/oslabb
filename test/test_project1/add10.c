#include "pipe.h"

int main() {
    int a = loadint();
    saveint(a + 10);
    return 0;
}
