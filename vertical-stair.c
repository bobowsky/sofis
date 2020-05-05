#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL_image.h>

#include "SDL_pixels.h"
#include "SDL_surface.h"
#include "animated-gauge.h"
#include "buffered-gauge.h"
#include "misc.h"
#include "resource-manager.h"
#include "sdl-pcf/SDL_pcf.h"
#include "vertical-stair.h"
#include "sdl-colors.h"


static void vertical_stair_render_value(VerticalStair *self, float value);
static AnimatedGaugeOps vertical_stair_ops = {
   .render_value = (ValueRenderFunc)vertical_stair_render_value
};

VerticalStair *vertical_stair_new(const char *bg_img, const char *cursor_img, PCF_Font *font)
{
    VerticalStair *self;

    self = calloc(1, sizeof(VerticalStair));
    if(self){
        if(!vertical_stair_init(self, bg_img, cursor_img, font)){
            free(self);
            return NULL;
        }
    }
    return self;
}


VerticalStair *vertical_stair_init(VerticalStair *self, const char *bg_img, const char *cursor_img, PCF_Font *font)
{

    self->scale.ruler = IMG_Load(bg_img);
    self->scale.ppv = 43.0/1000.0; /*0.0426*/
    self->scale.start = -2279;
    self->scale.end = 2279;

    self->cursor = IMG_Load(cursor_img);
    self->font = font;
    if(!self->cursor || !self->font)
        return NULL; //TODO: Will leak self->scale.ruler

    self->font->xfont.refcnt++;

    animated_gauge_init(ANIMATED_GAUGE(self), ANIMATED_GAUGE_OPS(&vertical_stair_ops), self->cursor->w, self->scale.ruler->h);

    return self;
}

void vertical_stair_dispose(VerticalStair *self)
{
    animated_gauge_dispose(ANIMATED_GAUGE(self));
    vertical_strip_dispose(&self->scale);
    if(self->cursor)
        SDL_FreeSurface(self->cursor);
    if(self->font)
        PCF_CloseFont(self->font);
}

void vertical_stair_free(VerticalStair *self)
{
    vertical_stair_dispose(self);
    free(self);
}

static void vertical_stair_render_value(VerticalStair *self, float value)
{
    float y;
    int ivalue;
    char number[6]; //5 digits plus \0
    SDL_Rect cloc; /*Cursor location*/
    SDL_Rect dst;

    ivalue = round(value);
    snprintf(number, 6, "% -d", ivalue);
    vertical_strip_clip_value(&self->scale, &value);

    buffered_gauge_clear(BUFFERED_GAUGE(self), NULL);
    buffered_gauge_blit(BUFFERED_GAUGE(self), self->scale.ruler, NULL, NULL);

    y = vertical_strip_resolve_value(&self->scale, value, true);
    y = round(round(y) - self->cursor->h/2.0);

    cloc = (SDL_Rect){1, y,self->cursor->w,self->cursor->h};
    buffered_gauge_blit(BUFFERED_GAUGE(self), self->cursor, NULL, &cloc);

    PCF_FontGetSizeRequestRect(self->font, number, &dst);
    SDLExt_RectAlign(&dst, &cloc, HALIGN_LEFT | VALIGN_MIDDLE);
    dst.x += self->font->xfont.fontPrivate->metrics->metrics.characterWidth;

    buffered_gauge_font_draw_text(BUFFERED_GAUGE(self),
        &dst, 0,
        number,
        self->font,
        SDL_UWHITE(BUFFERED_GAUGE(self)->view),
        SDL_UCKEY(BUFFERED_GAUGE(self)->view)
    );
}
