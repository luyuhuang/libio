#ifndef _BASIC_VALUE_H_
#define _BASIC_VALUE_H_

#include <stdint.h>

typedef union basic_value {
    int64_t l;
    uint64_t u;
    double d;
    const char *s;
    void *p;
} basic_value_t;

#define BASIC2L(_b) ((_b).l)
#define BASIC2U(_b) ((_b).u)
#define BASIC2D(_b) ((_b).d)
#define BASIC2S(_b) ((_b).s)
#define BASIC2P(_b, _t) ((_t)((_b).p))

#define L2BASIC(_l) ({basic_value_t _l2BASIC_value = {.l = _l}; _l2BASIC_value;})
#define U2BASIC(_u) ({basic_value_t _u2BASIC_value = {.u = _u}; _u2BASIC_value;})
#define D2BASIC(_d) ({basic_value_t _d2BASIC_value = {.d = _d}; _d2BASIC_value;})
#define S2BASIC(_s) ({basic_value_t _s2BASIC_value = {.s = _s}; _s2BASIC_value;})
#define P2BASIC(_p) ({basic_value_t _p2BASIC_value = {.p = (void*)_p}; _p2BASIC_value;})

static const basic_value_t _basic_value_null = {0};

#define BASIC_NULL _basic_value_null
#define BASIC_IS_NULL(b) ((b).p == NULL)

#endif //_BASIC_VALUE_H_
