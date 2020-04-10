#ifndef ODO_GAUGE_H
#define ODO_GAUGE_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "animated-gauge.h"
#include "basic-animation.h"
#include "digit-barrel.h"

typedef struct{
    AnimatedGauge parent;

    DigitBarrel **barrels;
    int *heights;
    uintf8_t nbarrels;

    int rubis;
    float max_value;
}OdoGauge;

OdoGauge *odo_gauge_new(DigitBarrel *barrel, int height, int rubis);
OdoGauge *odo_gauge_new_multiple(int rubis, int nbarrels, ...);
OdoGauge *odo_gauge_init(OdoGauge *self, int rubis, int nbarrels, ...);
OdoGauge *odo_gauge_vainit(OdoGauge *self, int rubis, int nbarrels, va_list ap);
void odo_gauge_free(OdoGauge *self);

bool odo_gauge_set_value(OdoGauge *self, float value);
#endif /* ODO_GAUGE_H */