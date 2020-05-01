#include <stdio.h>
#include <stdlib.h>

#include "SDL_surface.h"
#include "airspeed-indicator.h"
#include "airspeed-page-descriptor.h"
#include "base-gauge.h"
#include "buffered-gauge.h"
#include "sdl-colors.h"
#include "sdl-pcf/SDL_pcf.h"
#include "text-gauge.h"
#include "view.h"

static void airspeed_indicator_render(AirspeedIndicator *self, Uint32 dt);
static BufferedGaugeOps airspeed_indicator_ops = {
    .render = (BufferRenderFunc)airspeed_indicator_render
};


AirspeedIndicator *airspeed_indicator_new(speed_t v_so, speed_t v_s1, speed_t v_fe, speed_t v_no, speed_t v_ne)
{
    AirspeedIndicator *self;

    self = calloc(1, sizeof(AirspeedIndicator));
    if(self){
        if(!airspeed_indicator_init(self, v_so, v_s1, v_fe, v_no, v_ne)){
            free(self);
            return NULL;
        }
    }
    return self;
}


AirspeedIndicator *airspeed_indicator_init(AirspeedIndicator *self, speed_t v_so, speed_t v_s1, speed_t v_fe, speed_t v_no, speed_t v_ne)
{
    AirspeedPageDescriptor *descriptor;
    DigitBarrel *db;
    PCF_Font *tmp;

    buffered_gauge_init(BUFFERED_GAUGE(self), &airspeed_indicator_ops, 68, 240+20);

    descriptor = airspeed_page_descriptor_new( v_so,  v_s1,  v_fe,  v_no,  v_ne);
    self->ladder = ladder_gauge_new((LadderPageDescriptor *)descriptor, -1);
    buffered_gauge_set_buffer(
        BUFFERED_GAUGE(self->ladder),
        buffered_gauge_get_view(BUFFERED_GAUGE(self)),
        0,
        0
    );

    db = digit_barrel_new(18, 0, 9.999, 1);
    self->odo = odo_gauge_new_multiple(-1, 3,
            -1, db,
            -2, db,
            -2, db
    );
    buffered_gauge_set_buffer(
        BUFFERED_GAUGE(self->odo),
        buffered_gauge_get_view(BUFFERED_GAUGE(self->ladder)),
        25,
        (BASE_GAUGE(self->ladder)->h-1)/2.0 - BASE_GAUGE(self->odo)->h/2.0 +1
    );

    buffered_gauge_clear(BUFFERED_GAUGE(self),NULL);

    tmp = PCF_OpenFont("ter-x12n.pcf.gz");
    if(!tmp) return NULL; //TODO: Free all above allocated resources+find a pattern for that case

    self->txt = text_gauge_new(NULL, true, 68, 21);
    text_gauge_build_static_font(self->txt, tmp, &SDL_WHITE, 2, "TASKT-", PCF_DIGITS);
    buffered_gauge_set_buffer(BUFFERED_GAUGE(self->txt),
        buffered_gauge_get_view(BUFFERED_GAUGE(self)),
        0,
        BASE_GAUGE(self)->h - 20 - 1
    );
    text_gauge_set_color(self->txt, SDL_BLACK, BACKGROUND_COLOR);
    PCF_CloseFont(tmp);

    airspeed_indicator_set_value(self, 0.0);
    return self;
}

void airspeed_indicator_dispose(AirspeedIndicator *self)
{
    ladder_gauge_free(self->ladder);
    odo_gauge_free(self->odo);
    text_gauge_free(self->txt);
    buffered_gauge_dispose(BUFFERED_GAUGE(self));
}

void airspeed_indicator_free(AirspeedIndicator *self)
{
    airspeed_indicator_dispose(self);
    free(self);
}

bool airspeed_indicator_set_value(AirspeedIndicator *self, float value)
{
    float cad; /*Current air density, must be in kg/m3 (same unit as RHO_0)*/
    char number[10]; //TAS XXXKT plus \0

    cad = RHO_0; /*Placeholder, should be reported by a sensor*/

    self->tas = round(value * sqrt(RHO_0/cad));
    snprintf(number, 10, "TAS %03dKT", self->tas);
    text_gauge_set_value(self->txt, number);

    animated_gauge_set_value(ANIMATED_GAUGE(self->ladder), value);
    BUFFERED_GAUGE(self)->damaged = true;
    return odo_gauge_set_value(self->odo, value);
}

static void airspeed_indicator_render(AirspeedIndicator *self, Uint32 dt)
{
    buffered_gauge_clear(BUFFERED_GAUGE(self), NULL);
    buffered_gauge_draw_outline(BUFFERED_GAUGE(self), &SDL_WHITE, NULL); /*Not really needed*/

    buffered_gauge_paint_buffer(BUFFERED_GAUGE(self->ladder), dt);
    buffered_gauge_paint_buffer(BUFFERED_GAUGE(self->odo), dt);
    buffered_gauge_paint_buffer(BUFFERED_GAUGE(self->txt), dt);
    if(animated_gauge_moving(ANIMATED_GAUGE(self->ladder)) || animated_gauge_moving(ANIMATED_GAUGE(self->odo)))
        BUFFERED_GAUGE(self)->damaged = true;
}
