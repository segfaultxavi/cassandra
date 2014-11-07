#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include "SDL.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 800
#define MAP_WIDTH 20
#define MAP_HEIGHT 20
#define CELL_WIDTH (SCREEN_WIDTH / MAP_WIDTH)
#define CELL_HEIGHT (SCREEN_HEIGHT / MAP_HEIGHT)

SDL_Window *win;
SDL_Renderer *renderer;

struct Cell {
	virtual void render (int x, int y, float alpha) {
		SDL_Rect rect = { x * CELL_WIDTH, y * CELL_HEIGHT, CELL_WIDTH, CELL_HEIGHT };
		SDL_RenderFillRect (renderer, &rect);
	};
	virtual bool can_pass () = 0;
	virtual ~Cell () {}
};

char textMap[MAP_HEIGHT][MAP_WIDTH + 1] = {
	"####################",
	"#........#.........#",
	"###.#.#.##.........#",
	"#.....#..#.........#",
	"#.#.#.##...........#",
	"#......X.#.........#",
	"#.###.##.#.........#",
	"#.#.#.#..#.........#",
	"#...#C#.###.#.#.#.##",
	"###.######.........#",
	"#...#....#.#.#.#.#.#",
	"###.#.####.........#",
	"#.....#..##.#.#.#.##",
	"#####.##.#.........#",
	"#........#.#.#.#.#.#",
	"########.#.........#",
	"#........#.#.#.#.#.#",
	"#####.####.........#",
	"#..........#.#.#.#.#",
	"####################",
};

struct Cass {
	int x, y;
	static SDL_Texture *tex;

	void render (float alpha) {
		SDL_Rect rect = { x * CELL_WIDTH, y * CELL_HEIGHT, CELL_WIDTH, CELL_HEIGHT };
		if (alpha == 1.f) {
			SDL_SetTextureBlendMode (tex, SDL_BLENDMODE_NONE);
			SDL_SetTextureColorMod (tex, 255, 255, 0);
		} else {
			SDL_SetTextureBlendMode (tex, SDL_BLENDMODE_ADD);
			SDL_SetTextureAlphaMod (tex, (Uint8)(alpha * 255));
			SDL_SetTextureColorMod (tex, 255, 255, 255);
		}
		SDL_RenderCopy (renderer, tex, NULL, &rect);
	}
};

SDL_Texture *Cass::tex;

struct Undo {
	int cass_x, cass_y;
	int last_dir;

	Undo (int cass_x, int cass_y, int last_dir) {
		this->cass_x = cass_x;
		this->cass_y = cass_y;
		this->last_dir = last_dir;
	}
};

struct State {
	Cass cass;
	int last_dir;
	Cell *map[MAP_WIDTH][MAP_HEIGHT];

	~State () {
		int x, y;
		for (x = 0; x < MAP_WIDTH; x++) {
			for (y = 0; y < MAP_HEIGHT; y++) {
				delete map[x][y];
			}
		}
	}

	bool can_input (SDL_Keycode keycode) {
		switch (keycode) {
		case SDLK_ESCAPE:
			return true;
		case SDLK_UP:
			if (map[cass.x][cass.y - 1]->can_pass ()) return true;
			break;
		case SDLK_DOWN:
			if (map[cass.x][cass.y + 1]->can_pass ()) return true;
			break;
		case SDLK_LEFT:
			if (map[cass.x - 1][cass.y]->can_pass ()) return true;
			break;
		case SDLK_RIGHT:
			if (map[cass.x + 1][cass.y]->can_pass ()) return true;
			break;
		}
		return false;
	}

	Undo *input (SDL_Keycode keycode, bool *quit = NULL) {
		Undo *undo = NULL;
		switch (keycode) {
		case SDLK_ESCAPE:
			if (quit) *quit = true;
			break;
		case SDLK_UP:
			if (map[cass.x][cass.y - 1]->can_pass ()) {
				undo = new Undo (cass.x, cass.y, last_dir);
				cass.y--;
				last_dir = 0;
			}
			break;
		case SDLK_DOWN:
			if (map[cass.x][cass.y + 1]->can_pass ()) {
				undo = new Undo (cass.x, cass.y, last_dir);
				cass.y++;
				last_dir = 2;
			}
			break;
		case SDLK_LEFT:
			if (map[cass.x - 1][cass.y]->can_pass ()) {
				undo = new Undo (cass.x, cass.y, last_dir);
				cass.x--;
				last_dir = 3;
			}
			break;
		case SDLK_RIGHT:
			if (map[cass.x + 1][cass.y]->can_pass ()) {
				undo = new Undo (cass.x, cass.y, last_dir);
				cass.x++;
				last_dir = 1;
			}
			break;
		}
		return undo;
	}

	void render_background (float alpha) {
		for (int x = 0; x < MAP_WIDTH; x++) {
			for (int y = 0; y < MAP_HEIGHT; y++) {
				map[x][y]->render (x, y, alpha);
			}
		}
	}

	void render_cass (float alpha) {
		cass.render (alpha);
	}

	void undo (Undo *undo) {
		cass.x = undo->cass_x;
		cass.y = undo->cass_y;
		last_dir = undo->last_dir;
	}
};

struct EmptyCell : Cell {
	virtual void render (int x, int y, float alpha) {
		if (alpha < 1.f) return;
		SDL_SetRenderDrawColor (renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		Cell::render (x, y, alpha);
	};

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

	virtual bool can_pass () {
		return false;
	}
};

struct TrapCell : Cell {
	virtual void render (int x, int y, float alpha) {
		if (alpha < 1.f) return;
		SDL_SetRenderDrawColor (renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_Rect rect = { 0, 0, CELL_WIDTH / 7, CELL_HEIGHT / 7 };
		for (int sx = 0; sx < 3; sx++) {
			for (int sy = 0; sy < 3; sy++) {
				rect.x = (x + sx / 3.f + 1 / 7.f) * CELL_WIDTH;
				rect.y = (y + sy / 3.f + 1 / 7.f) * CELL_WIDTH;
				SDL_RenderFillRect (renderer, &rect);
			}
		}
	};

	virtual bool can_pass () {
		return true;
	}
};

void recurse (State *state, float alpha) {
	static const SDL_Keycode actions[4] = { SDLK_UP, SDLK_RIGHT, SDLK_DOWN, SDLK_LEFT };
	static const int weights[4] = { 3, 2, 1, 2 };
	int probs[4], i;
	float total_prob = 0.f;

	if (alpha < 0.001f) return;

	for (i = 0; i < 4; i++) {
		if (state->can_input (actions[i])) {
			probs[i] = weights[(i + 4 - state->last_dir) % 4];
		} else {
			probs[i] = 0;
		}
		total_prob += probs[i];
	}

	for (i = 0; i < 4; i++) {
		if (probs[i] > 0) {
			Undo *undo;
			float p = alpha * probs[i] / total_prob;
			undo = state->input (actions[i]);
			float p2 = 1.f - p; 
			p2 = 1.f - SDL_pow (p2, 8.0);
			state->render_background (p2 * 0.8f);
			state->render_cass (p2 * 0.8f);
			recurse (state, p);
			state->undo (undo);
			delete undo;
		}
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
	SDL_FillRect (surf, &rect, 0xFFFFFFFF); // ARGB
	Cass::tex = SDL_CreateTextureFromSurface (renderer, surf);
	SDL_FreeSurface (surf);

	int x, y;
	State current_state;
	for (x = 0; x < MAP_WIDTH; x++) {
		for (y = 0; y < MAP_HEIGHT; y++) {
			switch (textMap[y][x]) {
			case '#': current_state.map[x][y] = new WallCell (); break;
			case 'C': current_state.cass.x = x; current_state.cass.y = y; // Deliverate fallthrough
			case '.': current_state.map[x][y] = new EmptyCell (); break;
			case 'X': current_state.map[x][y] = new TrapCell (); break;
			}
		}
	}
	current_state.last_dir = 0;

	SDL_Event e;
	bool quit = false;
	while (!quit) {
		if (SDL_WaitEvent (&e)) {
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
		current_state.render_background (1.f);

		recurse (&current_state, 1.f);

		current_state.render_cass (1.f);
		SDL_RenderPresent (renderer);
	}

	SDL_DestroyTexture (Cass::tex);

	return 0;
}