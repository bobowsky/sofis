#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL_ttf.h>

#include "ladder-page.h"
#include "sdl-colors.h"


LadderPageDescriptor *ladder_page_descriptor_init(LadderPageDescriptor *self, ScrollType direction, float page_size, float vstep, float vsubstep, LPInitFunc func)
{
    self->direction = direction;
    self->page_size = page_size;
    self->vstep = vstep;
    self->vsubstep = vsubstep;
    self->offset = NAN;
    self->init_page = func;

    return self;
}

/*TODO: Check if ppv could be intergrated into the descriptor*/
void ladder_page_descriptor_compute_offset(LadderPageDescriptor *self, float ppv)
{
    float half_split;
    int leading, trailing;
    float pixel_increment;

    pixel_increment = ppv * self->vstep; /*How many pixels between two ruler marks*/

    half_split = (pixel_increment-1)/2.0;

    if(self->direction == TOP_DOWN){ /*0 is at the top of the image, increments downwards*/
        leading = ceil(half_split);
        self->offset = -1.0 * leading*ppv;
    }else{ /*0 is at the bottom of the image, increments upwards*/
        trailing = floor(half_split);
        self->offset = -1.0 * trailing/ppv;
    }
}

LadderPage *ladder_page_new(float start, LadderPageDescriptor *descriptor)
{
    LadderPage *self;

    self = calloc(1, sizeof(LadderPage));
    if(self){
        VERTICAL_STRIP(self)->start = start;
        VERTICAL_STRIP(self)->end = NAN;
        self->descriptor = descriptor;
    }
    return self;
}

void ladder_page_free(LadderPage *self)
{
    vertical_strip_dispose(VERTICAL_STRIP(self));
    free(self);
}

int ladder_page_get_index(LadderPage *self)
{

    return VERTICAL_STRIP(self)->start/(self->descriptor->page_size+self->descriptor->offset);
}

/*TODO: inline*/
float ladder_page_resolve_value(LadderPage *self, float value)
{
    VerticalStrip *strip;
    bool reverse;

    strip = VERTICAL_STRIP(self);
    reverse = (self->descriptor->direction == BOTTUM_UP);

    if(fmod(value, self->descriptor->vstep) == 0){ /*Value is a big graduation*/
        float y;
        value = fmod(value, fabs(round(strip->end - strip->start)) + 1);
        int ngrads = value/self->descriptor->vstep;
        if(!reverse)
            y = self->descriptor->fei + ngrads * strip->ppv * self->descriptor->vstep;
        else
            y = self->descriptor->fei - ngrads * strip->ppv * self->descriptor->vstep;
        return y;
    }else if(self->descriptor->vsubstep != 0 && fmod(value, self->descriptor->vsubstep) == 0){ /*Value is a small graduation*/
        float y;
        value = fmod(value, fabs(round(strip->end - strip->start)) + 1);
        int ngrads = value/self->descriptor->vsubstep;
        if(!reverse)
            y = self->descriptor->fei + ngrads * strip->ppv * self->descriptor->vsubstep;
        else
            y = self->descriptor->fei - ngrads * strip->ppv * self->descriptor->vsubstep;
        return y;
    }else{
        return vertical_strip_resolve_value(strip, value, reverse);
    }
}

/*Put markings*/
void ladder_page_etch_markings(LadderPage *self, int font_size)
{
    SDL_Surface *text;
    TTF_Font *font;
    SDL_Rect dst;
    char number[6]; //5 digits plus \0
    float y;

    VerticalStrip *strip;
    int page_index;

    strip = VERTICAL_STRIP(self);

    page_index = ladder_page_get_index(self);
    printf("Page %d real range is [%f, %f]\n",page_index, strip->start, strip->end);


    font = TTF_OpenFont("TerminusTTF-4.47.0.ttf", font_size);
    printf("Writing indexes on %d starting at %d to %f\n",page_index, page_index*self->descriptor->page_size, strip->end);
    for(int i = page_index*self->descriptor->page_size; i <= strip->end; i += self->descriptor->vstep){
        snprintf(number, 6, "%d", i);
        text = TTF_RenderText_Solid(font, number, SDL_WHITE);

        y = ladder_page_resolve_value(self, i);
        dst.y = y - text->h/2.0; /*verticaly center text*/
        dst.x = (strip->ruler->w-1) - 10 - 5 - text->w;

        SDL_BlitSurface(text, NULL, strip->ruler, &dst);
        SDL_FreeSurface(text);
    }
    TTF_CloseFont(font);
}