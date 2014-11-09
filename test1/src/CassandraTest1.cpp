#define _CRTDBG_MAP_ALLOC
#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG
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

static const int dirs[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };

struct Cass {
	int x, y;
	int dir;
	static SDL_Texture *tex;
	bool dead;

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
		if (dead) {
			SDL_SetTextureColorMod (tex, 255, 0, 0);
		}
		SDL_RenderCopy (renderer, tex, NULL, &rect);
	}
};

SDL_Texture *Cass::tex;

struct Cell;
struct State;
struct Undo;

struct Undo {
	Undo *next;

	Undo () : next (NULL) {}

	virtual ~Undo () {
		if (next) {
			delete next;
		}
	}

	virtual void apply (State *state) = 0;

	void add (Undo *new_undo) {
		Undo *last = this, *next = last->next;
		while (next) {
			last = next;
			next = next->next;
		}
		last->next = new_undo;
	}
};

struct CassUndo : Undo {
	int cass_x, cass_y;
	int cass_dir;
	bool dead;

	CassUndo (int cass_x, int cass_y, int cass_dir, bool dead) {
		this->cass_x = cass_x;
		this->cass_y = cass_y;
		this->cass_dir = cass_dir;
		this->dead = dead;
	}

	void apply (State *state);
};

struct PushUndo : Undo {
	int new_x, new_y;
	int old_x, old_y;
	Cell *old_cell;

	PushUndo (int new_x, int new_y, int old_x, int old_y, Cell *old_cell) {
		this->new_x = new_x;
		this->new_y = new_y;
		this->old_x = old_x;
		this->old_y = old_y;
		this->old_cell = old_cell;
	}

	void apply (State *state);
};

struct Map {
	Cell *cells[MAP_WIDTH][MAP_HEIGHT];
};

struct Cell {
	Map *map;
	int x, y;

	Cell (Map *map, int x, int y) : map (map), x (x), y (y) {}
	virtual ~Cell () {}

	virtual void render (int x, int y, float alpha) {
		SDL_Rect rect = { x * CELL_WIDTH, y * CELL_HEIGHT, CELL_WIDTH, CELL_HEIGHT };
		SDL_RenderFillRect (renderer, &rect);
	};
	virtual bool can_pass (int incoming_dir) = 0;
	virtual void pass (Cass *cass, Undo **undo = NULL) {};
};

struct EmptyCell : Cell {
	EmptyCell (Map *map, int x, int y) : Cell (map, x, y) {}

	virtual void render (int x, int y, float alpha) {
		if (alpha < 1.f) return;
		SDL_SetRenderDrawColor (renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		Cell::render (x, y, alpha);
	};

	virtual bool can_pass (int incoming_dir) { return true; }
};

struct WallCell : Cell {
	WallCell (Map *map, int x, int y) : Cell (map, x, y) {}

	virtual void render (int x, int y, float alpha) {
		if (alpha < 1.f) return;
		SDL_SetRenderDrawColor (renderer, 0, 0, 255, SDL_ALPHA_OPAQUE);
		Cell::render (x, y, alpha);
	};

	virtual bool can_pass (int incoming_dir) { return false; }
};

struct TrapCell : Cell {
	TrapCell (Map *map, int x, int y) : Cell (map, x, y) {}

	virtual void render (int x, int y, float alpha) {
		return;
		if (alpha < 1.f) return;
		SDL_SetRenderDrawColor (renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_Rect rect = { 0, 0, CELL_WIDTH / 7, CELL_HEIGHT / 7 };
		for (int sx = 0; sx < 3; sx++) {
			for (int sy = 0; sy < 3; sy++) {
				rect.x = (int)((x + sx / 3.f + 1 / 7.f) * CELL_WIDTH);
				rect.y = (int)((y + sy / 3.f + 1 / 7.f) * CELL_WIDTH);
				SDL_RenderFillRect (renderer, &rect);
			}
		}
	};

	virtual bool can_pass (int incoming_dir) { return true; }
	virtual void pass (Cass *cass, Undo **undo = NULL) {
		cass->dead = true;
	}
};

struct PushableBlockCell : Cell {
	PushableBlockCell (Map *map, int x, int y) : Cell (map, x, y) {}

	virtual void render (int x, int y, float alpha) {
		// FIXME Use alpha for pushables too!
		if (alpha < 1.f) return;
		SDL_SetRenderDrawColor (renderer, 0, 255, 255, SDL_ALPHA_OPAQUE);
		SDL_Rect rect = { x * CELL_WIDTH + 3, y * CELL_HEIGHT + 3, CELL_WIDTH - 6, CELL_HEIGHT - 6 };
		SDL_RenderFillRect (renderer, &rect);
	};

	virtual bool can_pass (int incoming_dir) {
		int newx = x + dirs[incoming_dir][0];
		int newy = y + dirs[incoming_dir][1];
		return (map->cells[newx][newy]->can_pass (incoming_dir));
	}

	virtual void pass (Cass *cass, Undo **undo = NULL) {
		int newx = x + dirs[cass->dir][0];
		int newy = y + dirs[cass->dir][1];
		if (undo) {
			*undo = new PushUndo (newx, newy, x, y, map->cells[newx][newy]);
		} else {
			delete map->cells[newx][newy];
		}
		map->cells[newx][newy] = this;
		// FIXME Detect pits and make both pit+pushable disappear
		map->cells[x][y] = new EmptyCell (map, x, y);
		x = newx;
		y = newy;
	}
};

struct State {
	bool finished;
	Cass cass;
	Map map;

	State (const char text_map[MAP_HEIGHT][MAP_WIDTH + 1]) {
		int x, y;
		for (x = 0; x < MAP_WIDTH; x++) {
			for (y = 0; y < MAP_HEIGHT; y++) {
				switch (text_map[y][x]) {
				case '#': map.cells[x][y] = new WallCell (&map, x, y); break;
				case 'C': cass.x = x; cass.y = y; // Deliverate fallthrough
				case '.': map.cells[x][y] = new EmptyCell (&map, x, y); break;
				case 'X': map.cells[x][y] = new TrapCell (&map, x, y); break;
				case 'p': map.cells[x][y] = new PushableBlockCell (&map, x, y); break;
				}
			}
		}
		cass.dir = 0;
		cass.dead = false;
		finished = false;
	}

	~State () {
		int x, y;
		for (x = 0; x < MAP_WIDTH; x++) {
			for (y = 0; y < MAP_HEIGHT; y++) {
				delete map.cells[x][y];
			}
		}
	}

	bool can_input (SDL_Keycode keycode) {
		int dir = -1;
		switch (keycode) {
		case SDLK_ESCAPE:
			return true;
		case SDLK_UP:
			dir = 0;
			break;
		case SDLK_RIGHT:
			dir = 1;
			break;
		case SDLK_DOWN:
			dir = 2;
			break;
		case SDLK_LEFT:
			dir = 3;
			break;
		}
		if (!cass.dead && dir > -1) {
			if (map.cells[cass.x + dirs[dir][0]][cass.y + dirs[dir][1]]->can_pass (dir)) return true;
		}
		return false;
	}

	void input (SDL_Keycode keycode, Undo **undo = NULL) {
		int dir = -1;
		switch (keycode) {
		case SDLK_ESCAPE:
			finished = true;
			break;
		case SDLK_UP:
			dir = 0;
			break;
		case SDLK_RIGHT:
			dir = 1;
			break;
		case SDLK_DOWN:
			dir = 2;
			break;
		case SDLK_LEFT:
			dir = 3;
			break;
		}
		if (!cass.dead && dir > -1) {
			if (map.cells[cass.x + dirs[dir][0]][cass.y + dirs[dir][1]]->can_pass (dir)) {
				Undo *subundo = NULL;
				if (undo)
					*undo = new CassUndo (cass.x, cass.y, cass.dir, false);
				cass.x += dirs[dir][0];
				cass.y += dirs[dir][1];
				cass.dir = dir;

				map.cells[cass.x][cass.y]->pass (&cass, undo ? &subundo : NULL);
				if (subundo) {
					(*undo)->add (subundo);
				}
			}
		}
	}

	void render_background (float alpha) {
		for (int x = 0; x < MAP_WIDTH; x++) {
			for (int y = 0; y < MAP_HEIGHT; y++) {
				map.cells[x][y]->render (x, y, alpha);
			}
		}
	}

	void render_cass (float alpha) {
		cass.render (alpha);
	}
};

void CassUndo::apply (State *state) {
	if (next)
		next->apply (state);
	state->cass.x = cass_x;
	state->cass.y = cass_y;
	state->cass.dir = cass_dir;
	state->cass.dead = dead;
}

void PushUndo::apply (State *state) {
	if (next)
		next->apply (state);
	delete state->map.cells[old_x][old_y];
	state->map.cells[old_x][old_y] = state->map.cells[new_x][new_y];
	state->map.cells[old_x][old_y]->x = old_x;
	state->map.cells[old_x][old_y]->y = old_y;
	state->map.cells[new_x][new_y] = old_cell;
}

void recurse (State *state, float alpha) {
	static const SDL_Keycode actions[4] = { SDLK_UP, SDLK_RIGHT, SDLK_DOWN, SDLK_LEFT };
	static const int weights[4] = { 3, 2, 1, 2 };
	int probs[4], i;
	float total_prob = 0.f;

	if (alpha < 0.001f) return;

	for (i = 0; i < 4; i++) {
		if (state->can_input (actions[i])) {
			probs[i] = weights[(i + 4 - state->cass.dir) % 4];
		} else {
			probs[i] = 0;
		}
		total_prob += probs[i];
	}

	for (i = 0; i < 4; i++) {
		if (probs[i] > 0) {
			Undo *undo;
			float p = alpha * probs[i] / total_prob;
			state->input (actions[i], &undo);
			float p2 = 1.f - p; 
			p2 = 1.f - (float)SDL_pow (p2, 8.0);
			state->render_background (p2 * 0.8f);
			state->render_cass (p2 * 0.8f);
			recurse (state, p);
			undo->apply (state);
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
	SDL_Rect rect = { 7, 7, CELL_WIDTH - 14, CELL_HEIGHT - 14 };
	SDL_FillRect (surf, &rect, 0xFFFFFFFF); // ARGB
	Cass::tex = SDL_CreateTextureFromSurface (renderer, surf);
	SDL_FreeSurface (surf);

	static const char textMap[MAP_HEIGHT][MAP_WIDTH + 1] = {
		"####################",
		"#........#.........#",
		"###.#.#.##.........#",
		"#.....#..#.........#",
		"#.#.#.##...........#",
		"#.p....X.#......X..#",
		"#.###.##.#.........#",
		"#.#.#.#..#.........#",
		"#...#C#.###.#.#.#.##",
		"###.######.........#",
		"#...#....#.#.#.#X#.#",
		"###.#.####.........#",
		"#.....#..##.##..#..#",
		"#####.##.#...#..#..#",
		"#......X.#.#.#..#..#",
		"#.######.#...#X.####",
		"#........#.#.#..#..#",
		"#####.####...#..#..#",
		"#.X........#....p..#",
		"####################",
	};
	State current_state (textMap);

	SDL_Event e;
	bool quit = false;
	while (!quit) {
		if (SDL_WaitEvent (&e)) {
			switch (e.type) {
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_KEYDOWN:
				current_state.input (e.key.keysym.sym);
				if (current_state.finished)
					quit = true;
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