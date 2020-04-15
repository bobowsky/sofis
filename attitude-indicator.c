#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_rotozoom.h>

#include "SDL_render.h"
#include "SDL_surface.h"
#include "animated-gauge.h"
#include "attitude-indicator.h"
#include "misc.h"
#include "sdl-colors.h"

#define sign(x) (((x) > 0) - ((x) < 0))

static void attitude_indicator_render_value(AttitudeIndicator *self, float value);
static void attitude_indicator_create_scale(AttitudeIndicator *self, int size, int font_size);
static void attitude_indicator_draw_ball(AttitudeIndicator *self);

AttitudeIndicator *attitude_indicator_new(int width, int height)
{

    AttitudeIndicator *self;
    self = calloc(1, sizeof(AttitudeIndicator));
    if(self){
        if(!attitude_indicator_init(self, width, height)){
            free(self);
            return NULL;
        }
    }
    return self;
}


AttitudeIndicator *attitude_indicator_init(AttitudeIndicator *self, int width, int height)
{
    AnimatedGauge *parent;

    parent = ANIMATED_GAUGE(self);
    parent->view = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    parent->damaged = true;
    parent->renderer = (ValueRenderFunc)attitude_indicator_render_value;

	self->common_center.x = round((parent->view->w)/2.0);
	self->common_center.y = round(parent->view->h*0.4);
	self->size = 2; /*In tens of degrees, here 20deg (+/-)*/

	self->markers[MARKER_LEFT] = IMG_Load("left-marker.png");
	self->markers[MARKER_RIGHT] = IMG_Load("right-marker.png");
	self->markers[MARKER_CENTER] = IMG_Load("center-marker.png");

	self->rollslip = roll_slip_gauge_new();

	self->locations[MARKER_LEFT] = (SDL_Rect){
	/*The left marker has its arrow pointing right and the arrow X is at marker->w-1*/
		self->common_center.x - 78 - (self->markers[0]->w-1),
		round(parent->view->h*0.4) - round(self->markers[0]->h/2.0) +1,
		0,0
	};
	self->locations[MARKER_RIGHT] = (SDL_Rect){
		self->common_center.x + 78,
		self->locations[MARKER_LEFT].y,
		0,0
	};
	self->locations[MARKER_CENTER] = (SDL_Rect){
		self->common_center.x - round((self->markers[2]->w-1)/2.0),
		round(parent->view->h*0.4) +1,
		0,0
	};

	self->locations[ROLL_SLIP] = (SDL_Rect){
		self->common_center.x - round((self->rollslip->parent.view->w-1)/2.0),
		7,
		0,0
	};

	self->buffer = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
	self->renderer =  SDL_CreateSoftwareRenderer(self->buffer);

	self->tmp_hz = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);

	attitude_indicator_create_scale(self, self->size, 12);
	attitude_indicator_draw_ball(self);
    return self;
}

void attitude_indicator_dispose(AttitudeIndicator *self)
{
	animated_gauge_dispose(ANIMATED_GAUGE(self));

	roll_slip_gauge_free(self->rollslip);
	if(self->ball)
		SDL_FreeSurface(self->ball);
	if(self->ruler)
		SDL_FreeSurface(self->ruler);
	for(int i = 0; i < 3; i++){
		if(self->markers[i])
			SDL_FreeSurface(self->markers[i]);
	}
	if(self->buffer)
		SDL_FreeSurface(self->buffer);
	if(self->tmp_hz)
		SDL_FreeSurface(self->tmp_hz);
	if(self->ball_buffer)
		SDL_FreeSurface(self->ball_buffer);
	SDL_DestroyRenderer(self->renderer);
}

void attitude_indicator_free(AttitudeIndicator *self)
{
	attitude_indicator_dispose(self);
	free(self);
}

void attitude_indicator_set_roll(AttitudeIndicator *self, float value)
{
	animated_gauge_set_value(ANIMATED_GAUGE(self->rollslip), value);
	self->parent.damaged = true;
}

Uint32 interpolate_color_p(const SDL_PixelFormat *format, Uint32 from, Uint32 to, float progress)
{
    Uint8 fr,fg,fb;
    Uint8 tr,tg,tb;
    Uint8 rv_r, rv_g, rv_b;

    SDL_GetRGB(from, format, &fr, &fg, &fb);
    SDL_GetRGB(to, format, &tr, &tg, &tb);
//	printf("Interpolate between (%d,%d,%d) and (%d,%d,%d), progress=%0.2f percent\n",fr,fg,fb,tr,tg,tb, progress*100);

#if 1
    rv_r = fr + round((tr-fr)*progress);
    rv_g = fg + round((tg-fg)*progress);
    rv_b = fb + round((tb-fb)*progress);
#elif 0
    rv_r = round((1.0-progress) * fr + progress * tr + 0.5);
    rv_g = round((1.0-progress) * fg + progress * tg + 0.5);
    rv_b = round((1.0-progress) * fb + progress * tb + 0.5);
#endif

    return SDL_MapRGB(format, rv_r, rv_g, rv_b);
}

float range_progress(float value, float start, float end)
{
	float rv;
	float _v, _s, _e;
	_v = value - start;
	_e = end - start;

	rv = _v/_e;

//    printf("Progress=%0.2f value=%0.2f start=%0.2f, end=%0.2f\n",rv, value, start, end);
	return rv;
}

static void attitude_indicator_draw_ball(AttitudeIndicator *self)
{
    Uint32 *pixels;
    SDL_Surface *surface;
    int x, y, limit;
    Uint32 white;
    Uint32 sky,sky_down;
    Uint32 earth;

	if(!self->ball)
	    self->ball = SDL_CreateRGBSurfaceWithFormat(0, ANIMATED_GAUGE(self)->view->w, ANIMATED_GAUGE(self)->view->h, 32, SDL_PIXELFORMAT_RGBA32);

    surface = self->ball;
    SDL_LockSurface(surface);
    pixels = surface->pixels;

    white = SDL_UWHITE(surface);
//    earth = SDL_MapRGB(surface->format, 0x46, 0x2b, 0x0c);
//    earth = SDL_MapRGB(surface->format, 0x45, 0x2a, 0x06);
//    earth = SDL_MapRGB(surface->format, 0x44, 0x28, 0x07);
//    earth = SDL_MapRGB(surface->format, 0x3f, 0x28, 0x0a);
    earth = SDL_MapRGB(surface->format, 0x58, 0x34, 0x0a);
//    sky = SDL_MapRGB(surface->format, 0x13, 0x51, 0xd3);
    sky = SDL_MapRGB(surface->format, 0x00, 0x50, 0xff);
    sky_down = SDL_MapRGB(surface->format, 0x52, 0x6c, 0xd0);


    limit = round(surface->h*0.4); /*40/60 split between sky and earth*/
	self->ball_horizon = limit;
	int first_gradient = round(limit * 0.25); /*First gradiant from the centerline to 25% up*/
	int first_gradiant_stop = limit - first_gradient;
	Uint32 color;
    /*Draw the sky*/
    for(y = limit; y >= 0; y--){
		if(y  > first_gradiant_stop){
			float progress;
			progress = range_progress(y, limit, first_gradiant_stop);

			color = interpolate_color_p(surface->format, sky_down, sky, progress );
		}else{
			color = sky;
//			exit(0);
		}
        for(int x = 0; x < surface->w; x++){
	        pixels[y * surface->w + x] = color;
        }
    }
    /*Draw a white center line*/
	y = limit;
    for(x = 0; x < surface->w; x++){
        pixels[y * surface->w + x] = white;
    }

    /*Draw the earth*/
    int start = y +1;
    for(y = start; y < surface->h; y++){
        for(int x = 0; x < surface->w; x++){
//            pixels[y * surface->w + x] = interpolate_color(surface->format, white, earth, y, start, surface->h-1);
            pixels[y * surface->w + x] = earth;
        }
    }
    SDL_UnlockSurface(surface);
}

static void attitude_indicator_create_scale(AttitudeIndicator *self, int size, int font_size)
{
	Uint32 *pixels;
	int width, height;
	int x, y;
	int start_x, middle_x, end_x;
	int middle_y;

	int yoffset = 10;
	/* Width: 57px wide for the 10s graduations plus 2x20 to allow space for the font*/
	width = 57+(2*20);
	middle_x = (57-1)/2 + 19;
	 /* Height: 73px for +10 and -10 graduations by the size */
	height = 73*size + 20;
	printf("max height: %d\n", height-1); //166 values from 0 to 165
	middle_y = ((73*size)-1)/2.0 + yoffset;

	self->ruler_center.x = middle_x;
	self->ruler_center.y = middle_y;

	self->ruler = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
	SDL_SetColorKey(self->ruler, SDL_TRUE, SDL_UCKEY(self->ruler));
	SDL_FillRect(self->ruler, NULL, SDL_UCKEY(self->ruler));


    SDL_LockSurface(self->ruler);
    pixels = self->ruler->pixels;
    Uint32 color = SDL_UWHITE(self->ruler);
    Uint32 mcolor = SDL_URED(self->ruler);

	int grad_level;
	int grad_sizes[] = {57,11,25,11};

	/*Go upwards*/
	grad_level = 0;
	for(y = middle_y; y >= 0+yoffset; y--){
		if( (y-middle_y) % 9 == 0){
			start_x = middle_x - (grad_sizes[grad_level]-1)/2;
			end_x = middle_x + (grad_sizes[grad_level]-1)/2;

			for( x = start_x; x <= end_x; x++){
                pixels[y * self->ruler->w + x] = color;
			}
            pixels[y * self->ruler->w + middle_x] = mcolor;

			grad_level = (grad_level < 3) ? grad_level + 1 : 0;
		}
	}
	/*Go downwards*/
	grad_level = 0;
	for(y = middle_y; y < self->ruler->h-yoffset; y++){
		if( (y-middle_y) % 9 == 0){
			start_x = middle_x - (grad_sizes[grad_level]-1)/2;
			end_x = middle_x + (grad_sizes[grad_level]-1)/2;

			for( x = start_x; x <= end_x; x++){
                pixels[y * self->ruler->w + x] = color;
			}
            pixels[y * self->ruler->w + middle_x] = mcolor;

			grad_level = (grad_level < 3) ? grad_level + 1 : 0;
		}
	}
    SDL_UnlockSurface(self->ruler);

	TTF_Font *font;
    SDL_Surface *text;
	char number[4]; /*Three digits plus \0*/
	uintf8_t current_grad;
	SDL_Rect left, right;

	font  = TTF_OpenFont("TerminusTTF-4.47.0.ttf", font_size);
	start_x = middle_x - (57-1)/2;
	end_x = middle_x + (57-1)/2;

	/*Go upwards*/
	current_grad = 0;
	for(y = middle_y; y >= 0+yoffset; y--){
		if((y-middle_y) % 36 == 0){ // 10 graduation
			if(current_grad > 0){
				snprintf(number, 4, "%d", current_grad);
                text = TTF_RenderText_Solid(font, number, SDL_WHITE);

				left.x = middle_x - (57-1)/2 - (text->w + 4);
				right.x = middle_x + (57-1)/2 + (4);
				left.y = y - round(text->h/2.0);
				right.y = left.y;

				SDL_BlitSurface(text, NULL, self->ruler, &left);
				SDL_BlitSurface(text, NULL, self->ruler, &right);
				SDL_FreeSurface(text);
			}

			current_grad += 10;
		}
	}

	/*Go downwards*/
	current_grad = 0;
	for(y = middle_y; y < self->ruler->h-yoffset; y++){
		if((y-middle_y) % 36 == 0){ // 10 graduation
			if(current_grad > 0){
				snprintf(number, 4, "%d", current_grad);
                text = TTF_RenderText_Solid(font, number, SDL_WHITE);

				left.x = middle_x - (57-1)/2 - (text->w + 4);
				right.x = middle_x + (57-1)/2 + (4);
				left.y = y - round(text->h/2.0);
				right.y = left.y;

				SDL_BlitSurface(text, NULL, self->ruler, &left);
				SDL_BlitSurface(text, NULL, self->ruler, &right);
				SDL_FreeSurface(text);
			}
			current_grad += 10;
		}
	}
	TTF_CloseFont(font);
}


/*TODO: This might go in a Ruler class*/
int attitude_indicator_resolve_value(AttitudeIndicator *self, float value)
{
	float aval;
	float ngrads;

	aval = fabs(value);
	if(aval == 0){
		ngrads = 0;
	}else if(!fmod(aval, 2.5)){
		ngrads = aval/2.5;
	}else if(!fmod(aval, 5.0)){
		ngrads = aval/5;
	}else if(!fmod(aval, 10)){
		ngrads = aval/10;
	}else {
		ngrads = aval/2.5;
	}

	return self->common_center.y + sign(value) * round((ngrads * 9.0));
}


static SDL_Surface *attitude_indicator_get_ball_buffer(AttitudeIndicator *self)
{
	if(!self->ball_buffer){
		SDL_Rect ball_pos;
		SDL_Rect ruler_pos;

		self->ball_buffer = SDL_CreateRGBSurfaceWithFormat(0, self->parent.view->w, self->parent.view->h, 32, SDL_PIXELFORMAT_RGBA32);

		//int common_y = attitude_indicator_resolve_value(self, value);
		int common_y = attitude_indicator_resolve_value(self, 0);

		/* First place the ball and the scale, such has the middle of the the scale is on
		 * the same line as the "middle" of the ball. They both need to have the same y coordinate
		 * on screen.
		 * */
		ball_pos.x = 0;
		ball_pos.y = common_y - self->ball_horizon;
		SDL_BlitSurface(self->ball, NULL, self->ball_buffer, &ball_pos);

		ruler_pos.x = self->common_center.x - self->ruler_center.x;
		ruler_pos.y = common_y - self->ruler_center.y;
		SDL_BlitSurface(self->ruler, NULL, self->ball_buffer, &ruler_pos);
	}

	return self->ball_buffer;
}

static void attitude_indicator_render_value(AttitudeIndicator *self, float value)
{
	SDL_Surface *surface;
	SDL_Rect ball_pos;

//	printf("Attitude indicator rendering value %0.2f\n",value);

	value = (value > self->size*10) ? self->size*10 + 5 : value;
	value = (value < self->size*-10) ? self->size*-10 - 5: value;

	SDL_FillRect(self->parent.view, NULL, SDL_MapRGBA(self->parent.view->format, 0, 0, 0, SDL_ALPHA_TRANSPARENT));
	surface = self->tmp_hz;

	int common_y = attitude_indicator_resolve_value(self, value);

	ball_pos.x = 0;
	ball_pos.y = common_y - self->ball_horizon;
	SDL_BlitSurface(attitude_indicator_get_ball_buffer(self), NULL, surface, &ball_pos);

	/*Place the roll indicator*/
	SDL_BlitSurface(self->rollslip->parent.view, NULL, surface, &self->locations[ROLL_SLIP]);

	if(self->rollslip->parent.value != 0){
        SDL_Point center_point = {self->common_center.x, self->common_center.y};
        SDL_Texture *tex = SDL_CreateTextureFromSurface(self->renderer, surface);
        SDL_RenderCopyEx(self->renderer, tex, NULL, NULL, self->rollslip->parent.value, &center_point, SDL_FLIP_NONE);
		SDL_DestroyTexture(tex);
        surface = self->buffer;
	}
	SDL_BlitSurface(surface, NULL, self->parent.view, NULL);

	/*Then place markers in the middle of the *screen* markers don't move*/
	SDL_BlitSurface(self->markers[MARKER_LEFT], NULL, self->parent.view, &self->locations[MARKER_LEFT]);
	SDL_BlitSurface(self->markers[MARKER_RIGHT], NULL, self->parent.view, &self->locations[MARKER_RIGHT]);
	SDL_BlitSurface(self->markers[MARKER_CENTER], NULL, self->parent.view, &self->locations[MARKER_CENTER]);

}