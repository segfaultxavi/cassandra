#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include "SDL.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 800
#define MAP_WIDTH 10
#define MAP_HEIGHT 10
#define CELL_WIDTH (SCREEN_WIDTH / MAP_WIDTH)
#define CELL_HEIGHT (SCREEN_HEIGHT / MAP_HEIGHT)

SDL_Window *win;
SDL_Renderer *renderer;

struct Cell {
	virtual void render (int x, int y, float alpha) {
		SDL_Rect rect = { x * CELL_WIDTH, y * CELL_HEIGHT, CELL_WIDTH, CELL_HEIGHT };
		SDL_RenderFillRect (renderer, &rect);
	};
	virtual Cell *clone () = 0;
	virtual bool can_pass () = 0;
	virtual ~Cell () {}
};

char textMap[10][11] = {
	"##########",
	"#........#",
	"###.#.#.##",
	"#.....#..#",
	"#.#.#.##.#",
	"#........#",
	"#.###.##.#",
	"#.#.#.#..#",
	"#...#C#.##",
	"##########"
};

struct Cass {
	int x, y;
	static SDL_Texture *tex;

	void render (float alpha) {
		SDL_Rect rect = { x * CELL_WIDTH, y * CELL_HEIGHT, CELL_WIDTH, CELL_HEIGHT };
		if (alpha == 1.f) {
			SDL_SetTextureBlendMode (tex, SDL_BLENDMODE_NONE);
		}
		else {
			SDL_SetTextureBlendMode (tex, SDL_BLENDMODE_ADD);
			SDL_SetTextureAlphaMod (tex, (Uint8)(alpha * 255));
		}
		SDL_RenderCopy (renderer, tex, NULL, &rect);
	}
};

SDL_Texture *Cass::tex;

struct State {
	Cass cass;
	Cell *map[MAP_WIDTH][MAP_HEIGHT];

	State *clone () {
		State *newstate = new State ();
		int x, y;
		for (x = 0; x < MAP_WIDTH; x++) {
			for (y = 0; y < MAP_HEIGHT; y++) {
				newstate->map[x][y] = map[x][y]->clone ();
			}
		}
		newstate->cass.x = cass.x;
		newstate->cass.y = cass.y;

		return newstate;
	}

	~State () {
		int x, y;
		for (x = 0; x < MAP_WIDTH; x++) {
			for (y = 0; y < MAP_HEIGHT; y++) {
				delete map[x][y];
			}
		}
	}

	bool input (SDL_Keycode keycode, bool *quit = NULL) {
		switch (keycode) {
		case SDLK_ESCAPE:
			if (quit) *quit = true;
			break;
		case SDLK_UP:
			if (map[cass.x][cass.y - 1]->can_pass ()) cass.y--;
			else return false;
			break;
		case SDLK_DOWN:
			if (map[cass.x][cass.y + 1]->can_pass ()) cass.y++;
			else return false;
			break;
		case SDLK_LEFT:
			if (map[cass.x - 1][cass.y]->can_pass ()) cass.x--;
			else return false;
			break;
		case SDLK_RIGHT:
			if (map[cass.x + 1][cass.y]->can_pass ()) cass.x++;
			else return false;
			break;
		}
		return true;
	}

	void render (float alpha) {
		for (int x = 0; x < MAP_WIDTH; x++) {
			for (int y = 0; y < MAP_HEIGHT; y++) {
				map[x][y]->render (x, y, alpha);
			}
		}
		cass.render (alpha);
	}
};

State current_state;

struct EmptyCell : Cell {
	virtual void render (int x, int y, float alpha) {
		if (alpha < 1.f) return;
		SDL_SetRenderDrawColor (renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		Cell::render (x, y, alpha);
	};

	virtual Cell *clone () {
		return new EmptyCell (*this);
	}

	virtual bool can_pass () {
		return true;
	}
};

struct WallCell : Cell {
	virtual void render (int x, int y, float alpha) {
		if (alpha < 1.f) return;
		SDL_SetRenderDrawColor (renderer, 0, 0, 255, SDL_ALPHA_OPAQUE);
		Cell::render (x, y, alpha);
	};

	virtual Cell *clone () {
		return new WallCell (*this);
	}

	virtual bool can_pass () {
		return false;
	}
};

void recurse (State *state, float alpha) {
	static const SDL_Keycode actions[4] = { SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT };
	if (alpha < 0.25f) return;

	for (int i = 0; i < 4; i++) {
		State *s = state->clone ();
		if (s->input (actions[i])) {
			s->render (alpha / 4.f);
			recurse (s, alpha / 2.f);
		}
		delete (s);
	}
}

int main (int argc, char *argv[]) {
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	if (SDL_Init (SDL_INIT_EVERYTHING) != 0) {
		printf ("SDL Init error: %s\n", SDL_GetError ());
		return 1;
	}

	win = SDL_CreateWindow ("Cassandra Test 1", 50, 50, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	if (win == NULL){
		printf ("SDL_CreateWindow Error: %s\n", SDL_GetError ());
		SDL_Quit ();
		return 1;
	}

	renderer = SDL_CreateRenderer (win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == NULL){
		printf ("SDL_CreateRenderer Error: %s\n", SDL_GetError ());
		SDL_DestroyWindow (win);
		SDL_Quit ();
		return 1;
	}

	SDL_Surface *surf = SDL_CreateRGBSurface (SDL_SWSURFACE, CELL_WIDTH, CELL_HEIGHT, 32, 0, 0, 0, 0);
	SDL_FillRect (surf, NULL, 0x00000000);
	SDL_Rect rect = { 5, 5, CELL_WIDTH - 10, CELL_HEIGHT - 10 };
	SDL_FillRect (surf, &rect, 0xFFFFFF00); // ARGB
	Cass::tex = SDL_CreateTextureFromSurface (renderer, surf);
	SDL_FreeSurface (surf);

	int x, y;
	for (x = 0; x < MAP_WIDTH; x++) {
		for (y = 0; y < MAP_HEIGHT; y++) {
			switch (textMap[y][x]) {
			case '#': current_state.map[x][y] = new WallCell (); break;
			case 'C': current_state.cass.x = x; current_state.cass.y = y; // Deliverate fallthrough
			case '.': current_state.map[x][y] = new EmptyCell (); break;
			}
		}
	}

	SDL_Event e;
	bool quit = false;
	while (!quit) {
		while (SDL_PollEvent (&e)) {
			switch (e.type) {
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_KEYDOWN:
				current_state.input (e.key.keysym.sym, &quit);
				break;
			}
		}

		SDL_SetRenderDrawColor (renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear (renderer);
		current_state.render (1.f);

		recurse (&current_state, 1.f);

		SDL_RenderPresent (renderer);
	}

	SDL_DestroyTexture (Cass::tex);

	return 0;
}