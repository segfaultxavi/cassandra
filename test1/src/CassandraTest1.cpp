#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include "SDL.h"

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 800
#define MAP_WIDTH 20
#define MAP_HEIGHT 20
#define CELL_WIDTH (SCREEN_WIDTH / MAP_WIDTH)
#define CELL_HEIGHT (SCREEN_HEIGHT / MAP_HEIGHT)

SDL_Window *win;
SDL_Renderer *renderer;
SDL_Texture *ghostplane;

struct Cell;
struct DoorCell;
struct PushableBlockCell;
struct State;

static const int dirs[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };

struct Cass {
	int x, y;
	int dir;
	static SDL_Texture *tex;
	bool dead;

	Cass () {
		if (tex == NULL) {
			SDL_Surface *surf = SDL_CreateRGBSurface (SDL_SWSURFACE, CELL_WIDTH, CELL_HEIGHT, 32, 0, 0, 0, 0);
			SDL_FillRect (surf, NULL, 0x00000000);
			SDL_Rect rect = { 7, 7, CELL_WIDTH - 14, CELL_HEIGHT - 14 };
			SDL_FillRect (surf, &rect, 0xFFFFFFFF); // ARGB
			tex = SDL_CreateTextureFromSurface (renderer, surf);
			SDL_FreeSurface (surf);
		}
	}

	~Cass () {
		SDL_DestroyTexture (tex);
	}

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

SDL_Texture *Cass::tex = NULL;

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
	int old_x, old_y;
	PushableBlockCell *pushable;

	PushUndo (int old_x, int old_y, PushableBlockCell *pushable) {
		this->old_x = old_x;
		this->old_y = old_y;
		this->pushable = pushable;
	}

	void apply (State *state);
};

struct DoorToggleUndo : Undo {
	DoorCell *door;

	DoorToggleUndo (DoorCell *door) : door (door) {}

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

	virtual void render (float alpha) = 0;
	virtual bool can_pass (int incoming_dir) = 0;
	virtual void pass (Cass *cass, Undo **undo = NULL) {};
	virtual bool is_hole () { return false; }

protected:
	void  private_render (SDL_Texture *tex) {
		SDL_Rect rect = { x * CELL_WIDTH, y * CELL_HEIGHT, CELL_WIDTH, CELL_HEIGHT };
		SDL_RenderCopy (renderer, tex, NULL, &rect);
	}

	SDL_Texture *create_texture (int inset) {
		SDL_Texture *tex;

		SDL_Surface *surf = SDL_CreateRGBSurface (SDL_SWSURFACE, CELL_WIDTH, CELL_HEIGHT, 32, 0, 0, 0, 0);
		SDL_FillRect (surf, NULL, 0x00000000);
		SDL_Rect rect = { inset, inset, CELL_WIDTH - inset * 2, CELL_HEIGHT - inset * 2 };
		SDL_FillRect (surf, &rect, 0xFFFFFFFF); // ARGB
		tex = SDL_CreateTextureFromSurface (renderer, surf);
		SDL_FreeSurface (surf);

		return tex;
	}
};

struct EmptyCell : Cell {
	EmptyCell (Map *map, int x, int y) : Cell (map, x, y) {}
	static SDL_Texture *tex;

	virtual void render (float alpha) {
		if (!tex)
			tex = Cell::create_texture (0);
		if (alpha < 1.f) return;
		SDL_SetTextureBlendMode (tex, SDL_BLENDMODE_NONE);
		SDL_SetTextureColorMod (tex, 0, 0, 0);
		Cell::private_render (tex);
	};

	virtual bool can_pass (int incoming_dir) { return true; }
};

SDL_Texture *EmptyCell::tex = NULL;

struct WallCell : Cell {
	WallCell (Map *map, int x, int y) : Cell (map, x, y) {}
	static SDL_Texture *tex;

	virtual void render (float alpha) {
		if (!tex)
			tex = Cell::create_texture (0);
		if (alpha < 1.f) return;
		SDL_SetTextureBlendMode (tex, SDL_BLENDMODE_NONE);
		SDL_SetTextureColorMod (tex, 0, 0, 255);
		Cell::private_render (tex);
	};

	virtual bool can_pass (int incoming_dir) { return false; }
};

SDL_Texture *WallCell::tex = NULL;

struct TrapCell : Cell {
	TrapCell (Map *map, int x, int y) : Cell (map, x, y) {}

	virtual void render (float alpha) {
		/* Invisible! */
		return;
	};

	virtual bool can_pass (int incoming_dir) { return true; }
	virtual void pass (Cass *cass, Undo **undo = NULL) {
		cass->dead = true;
	}

	virtual bool is_hole () { return true; }
};

struct PushableBlockCell : Cell {
	static SDL_Texture *tex;
	Cell *block_below;

	PushableBlockCell (Map *map, int x, int y) : Cell (map, x, y) {
		block_below = new EmptyCell (map, x, y);
	}

	~PushableBlockCell () {
		if (block_below)
			delete block_below;
	}

	virtual void render (float alpha) {
		if (!tex)
			tex = Cell::create_texture (2);
		if (alpha < 1.f) {
			SDL_SetTextureBlendMode (tex, SDL_BLENDMODE_ADD);
			SDL_SetTextureAlphaMod (tex, (Uint8)(alpha * 255));
			SDL_SetTextureColorMod (tex, 0, 255, 255);
		} else {
			SDL_SetTextureBlendMode (tex, SDL_BLENDMODE_NONE);
			SDL_SetTextureColorMod (tex, 0, 255, 255);
		}
		Cell::private_render (tex);
	};

	virtual bool can_pass (int incoming_dir) {
		int newx = x + dirs[incoming_dir][0];
		int newy = y + dirs[incoming_dir][1];
		return (map->cells[newx][newy]->can_pass (incoming_dir));
	}

	virtual void pass (Cass *cass, Undo **undo = NULL) {
		int oldx = x;
		int oldy = y;
		int newx = x + dirs[cass->dir][0];
		int newy = y + dirs[cass->dir][1];

		map->cells[oldx][oldy] = block_below;
		block_below = map->cells[newx][newy];
		map->cells[newx][newy] = this;
		x = newx;
		y = newy;

		if (undo) {
			*undo = new PushUndo (oldx, oldy, this);
		}
		Undo *subundo = NULL;
		map->cells[oldx][oldy]->pass (cass, undo ? &subundo : NULL);
		if (subundo) {
			(*undo)->add (subundo);
		}

		if (block_below->is_hole ()) {
			map->cells[newx][newy] = new EmptyCell (map, newx, newy);
			if (!undo) {
				delete this; /* And block_below */
			}
		}
	}
};

SDL_Texture *PushableBlockCell::tex = NULL;

struct DoorCell : Cell {
	int id;
	bool open;

	DoorCell (Map *map, int x, int y, int id) : Cell (map, x, y), id (id), open (false) {}
	static SDL_Texture *tex;

	virtual void render (float alpha) {
		if (!tex)
			tex = Cell::create_texture (0);
		if (alpha < 1.f) {
			SDL_SetTextureBlendMode (tex, SDL_BLENDMODE_ADD);
			SDL_SetTextureAlphaMod (tex, (Uint8)(alpha * 255));
		} else {
			SDL_SetTextureBlendMode (tex, SDL_BLENDMODE_NONE);
		}
		if (open)
			SDL_SetTextureColorMod (tex, 0, 0, 0);
		else
			SDL_SetTextureColorMod (tex, 128, 0, 128);
		Cell::private_render (tex);
	};

	virtual bool can_pass (int incoming_dir) { return open; }

	void toggle () {
		open = !open;
	}
};

SDL_Texture *DoorCell::tex = NULL;

struct TriggerCell : Cell {
	DoorCell *door;
	int id;

	TriggerCell (Map *map, int x, int y, DoorCell *door, int id) : Cell (map, x, y), door (door), id (id) {}
	static SDL_Texture *tex;

	virtual void render (float alpha) {
		if (!tex)
			tex = Cell::create_texture (12);
		if (alpha < 1.f) return;
		SDL_SetTextureBlendMode (tex, SDL_BLENDMODE_NONE);
		SDL_SetTextureColorMod (tex, 128, 0, 128);
		Cell::private_render (tex);
	};

	virtual bool can_pass (int incoming_dir) { return true; }

	virtual void pass (Cass *cass, Undo **undo = NULL) {
		if (undo) {
			*undo = new DoorToggleUndo (door);
		}
		door->toggle ();
	}
};

SDL_Texture *TriggerCell::tex = NULL;

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
				case '@': cass.x = x; cass.y = y; // Deliverate fallthrough
				case '.': map.cells[x][y] = new EmptyCell (&map, x, y); break;
				case '^': map.cells[x][y] = new TrapCell (&map, x, y); break;
				case '%': map.cells[x][y] = new PushableBlockCell (&map, x, y); break;
				}
				if (text_map[y][x] >= 'a' && text_map[y][x] <= 'z') {
					int id = text_map[y][x] - 'a';
					for (int sx = 0; sx < MAP_WIDTH; sx++) {
						for (int sy = 0; sy < MAP_HEIGHT; sy++) {
							if (text_map[sy][sx] == 'A' + id) {
								DoorCell *door = new DoorCell (&map, sx, sy, id);
								map.cells[sx][sy] = door;
								map.cells[x][y] = new TriggerCell (&map, x, y, door, id);
							}
						}
					}
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
				map.cells[x][y]->render (alpha);
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
	int new_x = pushable->x;
	int new_y = pushable->y;
	if (pushable->block_below->is_hole ()) {
		delete state->map.cells[new_x][new_y];
	}
	state->map.cells[new_x][new_y] = pushable->block_below;
	pushable->block_below = state->map.cells[old_x][old_y];
	state->map.cells[old_x][old_y] = pushable;
	pushable->x = old_x;
	pushable->y = old_y;
}

void DoorToggleUndo::apply (State *state) {
	if (next)
		next->apply (state);
	door->toggle ();
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

	ghostplane = SDL_CreateTexture (renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_TARGET, SCREEN_WIDTH, SCREEN_HEIGHT);

	static const char textMap[MAP_HEIGHT][MAP_WIDTH + 1] = {
		"####################",
		"#c.......C.........#",
		"###.#.#.##.........#",
		"#.....#..#.........#",
		"#.#.#.##.A..%......#",
		"#.%....^.#..b...^..#",
		"#.###.##.#.........#",
		"#.#.#.#.a#.........#",
		"#...#@..###.#.#.#.##",
		"###.#####.%........#",
		"#...#.....#.#.#.#.##",
		"###.#.#####........#",
		"#.....#..##.##..#.^#",
		"#####.##.#...#^.#..#",
		"#......^.#.#.#..#..#",
		"#.######.#...#.###B#",
		"#........#.#.#.#...#",
		"#####.####...#.#...#",
		"#.^........#...%...#",
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

		// Render ghosts on ghostplane
		SDL_SetRenderTarget (renderer, ghostplane);
		SDL_SetRenderDrawColor (renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear (renderer);
		recurse (&current_state, 1.f);

		// Render solid world
		SDL_SetRenderTarget (renderer, NULL);
		current_state.render_background (1.f);
		current_state.render_cass (1.f);

		// Render ghostplane
		SDL_SetTextureBlendMode (ghostplane, SDL_BLENDMODE_BLEND);
		SDL_SetTextureAlphaMod (ghostplane, 128);
		SDL_RenderCopy (renderer, ghostplane, NULL, NULL);
		SDL_RenderPresent (renderer);
	}

	SDL_DestroyTexture (ghostplane);

	return 0;
}