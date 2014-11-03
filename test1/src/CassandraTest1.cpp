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
	virtual void render (int x, int y) {
		SDL_Rect rect = {x * CELL_WIDTH, y * CELL_HEIGHT, CELL_WIDTH, CELL_HEIGHT};
		SDL_RenderFillRect (renderer, &rect);
	};
	virtual Cell *clone () = 0;
	virtual bool can_pass () = 0;
};

char textMap[10][11] = {
	"##########",
	"#........#",
	"###.#.#.##",
	"#...#.#..#",
	"#.###.##.#",
	"#........#",
	"#.###.##.#",
	"#.#.#.#..#",
	"#...#C#.##",
	"##########"
};

struct State {
	int cass_x, cass_y;
	Cell *map[MAP_WIDTH][MAP_HEIGHT];

	State *clone () {
		State *newstate = new State ();
		int x, y;
		for (x = 0; x < MAP_WIDTH; x++) {
			for (y = 0; y < MAP_HEIGHT; y++) {
				newstate->map[x][y] = map[x][y]->clone ();
			}
		}
		newstate->cass_x = cass_x;
		newstate->cass_y = cass_y;
	}
};

State current_state;

struct EmptyCell : Cell {
	virtual void render (int x, int y) {
		SDL_SetRenderDrawColor (renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		Cell::render (x, y);
	};

	virtual Cell *clone () {
		return new EmptyCell (*this);
	}

	virtual bool can_pass () {
		return true;
	}
};

struct WallCell : Cell {
	virtual void render (int x, int y) {
		SDL_SetRenderDrawColor (renderer, 0, 0, 255, SDL_ALPHA_OPAQUE);
		Cell::render (x, y);
	};

	virtual Cell *clone () {
		return new WallCell (*this);
	}

	virtual bool can_pass () {
		return false;
	}
};

void render_cass (int x, int y) {
	SDL_Rect rect = { x * CELL_WIDTH + 5, y * CELL_HEIGHT + 5, CELL_WIDTH - 10, CELL_HEIGHT - 10};
	SDL_RenderFillRect (renderer, &rect);
}

int main(int argc, char *argv[]) {
	if (SDL_Init (SDL_INIT_EVERYTHING) != 0) {
		printf ("SDL Init error: %s\n", SDL_GetError());
		return 1;
	}

	win = SDL_CreateWindow ("Cassandra Test 1", 50, 50, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	if (win == NULL){
		printf ("SDL_CreateWindow Error: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == NULL){
		printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
		SDL_DestroyWindow(win);
		SDL_Quit();
		return 1;
	}

	int x, y;
	for (x = 0; x < MAP_WIDTH; x++) {
		for (y = 0; y < MAP_HEIGHT; y++) {
			switch (textMap[y][x]) {
			case '#': current_state.map[x][y] = new WallCell (); break;
			case 'C': current_state.cass_x = x; current_state.cass_y = y; // Deliverate fallthrough
			case '.': current_state.map[x][y] = new EmptyCell (); break;
			}
		}
	}

	SDL_Event e;
	bool quit = false;
	while (!quit) {
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT){
				quit = true;
			}
			if (e.type == SDL_KEYDOWN) {
				switch (e.key.keysym.sym) {
				case SDLK_ESCAPE:
					quit = true;
					break;
				case SDLK_UP:
					if (current_state.map[current_state.cass_x][current_state.cass_y - 1]->can_pass ()) current_state.cass_y--;
					break;
				case SDLK_DOWN:
					if (current_state.map[current_state.cass_x][current_state.cass_y + 1]->can_pass ()) current_state.cass_y++;
					break;
				case SDLK_LEFT:
					if (current_state.map[current_state.cass_x - 1][current_state.cass_y]->can_pass ()) current_state.cass_x--;
					break;
				case SDLK_RIGHT:
					if (current_state.map[current_state.cass_x + 1][current_state.cass_y]->can_pass ()) current_state.cass_x++;
					break;
				}
			}
			if (e.type == SDL_MOUSEBUTTONDOWN) {
			}
		}

		SDL_SetRenderDrawColor (renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);
		for (x = 0; x < MAP_WIDTH; x++) {
			for (y = 0; y < MAP_HEIGHT; y++) {
				current_state.map[x][y]->render (x, y);
			}
		}
		SDL_SetRenderDrawColor (renderer, 0, 255, 0, SDL_ALPHA_OPAQUE);
		render_cass (current_state.cass_x, current_state.cass_y);
		SDL_RenderPresent(renderer);
	}

	for (x = 0; x < MAP_WIDTH; x++) {
		for (y = 0; y < MAP_HEIGHT; y++) {
			delete current_state.map[x][y];
		}
	}

	return 0;
}