
#include <stdint.h>
#include "fast_math.h"

uint16_t fm_sqrt(uint32_t n) {
    uint32_t root = 0;
    uint32_t bit = (uint32_t)1 << 30; // Il bit più alto possibile per un uint32

    // "bit" deve essere la più grande potenza di 4 minore o uguale a n
    while (bit > n) {
        bit >>= 2;
    }

    while (bit != 0) {
        if (n >= root + bit) {
            n -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }

    return (uint16_t)root;
}