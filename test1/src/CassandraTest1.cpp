#define _CRT_SECURE_NO_WARNINGS
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <memory.h>
#include <GL/glew.h>
#include "SDL.h"
#include "Cassandra.h"

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 640

#define MAX_STEPS 100000

extern unsigned char tiles_data[];
int tiles_width, tiles_height;
int cell_width, cell_height;

enum Inputs {
	UP,
	DOWN,
	LEFT,
	RIGHT,
	NUM_INPUTS
};

int InputKeys[] = {
	SDLK_UP,
	SDLK_DOWN,
	SDLK_LEFT,
	SDLK_RIGHT
};

struct Action;
struct Cell;
struct EmptyCell;
struct WallCell;
struct TrapCell;
struct DoorCell;
struct TriggerCell;
struct PushableBlockCell;
struct GoalCell;
struct State;

static const int dirs[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };

struct Renderable {
	int tilex, tiley;

	Renderable (int tilex, int tiley) : tilex (tilex), tiley (tiley) {}

	void render (int x, int y, float alpha) {
		if (alpha >= 1.f) {
			glColor4f (1.f, 1.f, 1.f, 1.f);
		} else {
			glColor4f (1.f, 1.f, 1.f, alpha);
		}
		glBegin (GL_TRIANGLE_STRIP);
		for (int sx = 0; sx < 2; sx++) {
			for (int sy = 0; sy < 2; sy++) {
				glTexCoord2f ((tilex + sx) * 32 / (GLfloat)tiles_width, (tiley + sy) * 32 / (GLfloat)tiles_height);
				glVertex2i ((x + sx) * cell_width, (y + sy) * cell_height);
			}
		}
		glEnd ();
	}
};

struct Player : Renderable {
	int x, y;
	bool dead;
	bool won;

	Player () : Renderable (4, 0) {}

	void render (float alpha) {
		if (dead) {
			tilex = 8; tiley = 3;
		} else {
			if (won) {
				tilex = 9; tiley = 0;
			} else {
				tilex = 4; tiley = 0;
			}
		}
		Renderable::render (x, y, alpha);
	}
};

struct Map {
	Map (int sizex, int sizey) : sizex (sizex), sizey (sizey) {
		cell_width = SCREEN_WIDTH / sizex;
		cell_height = SCREEN_HEIGHT / sizey;
		cells = new Cell*[sizex * sizey];
	}

	~Map ();

	int get_sizex () const { return sizex; }
	int get_sizey () const { return sizey; }
	Cell *get_cell (int x, int y) { return cells[x * sizey + y]; }
	const Cell *get_cell (int x, int y) const { return cells[x * sizey + y]; }
	void set_cell (int x, int y, Cell *c) { cells[x * sizey + y] = c; }

private:
	int sizex;
	int sizey;
	Cell **cells;
};

struct Cell : Renderable {
	int x, y;
	bool inmutable;

	Cell (int x, int y, int tilex, int tiley, bool inmutable) :
		Renderable (tilex, tiley), x (x), y (y), inmutable (inmutable) {}

	virtual ~Cell () {}

	virtual Cell *clone () const = 0;

	virtual void render (float alpha) {
		Renderable::render (x, y, alpha);
	};

	virtual bool can_pass (const Map *map, int incoming_dir) const = 0;
	virtual Action *pass (Map *map, int incoming_dir) { return NULL; };
	virtual bool is_hole () const { return false; }
	virtual void toggle () {}

	virtual bool is_same (const Cell *cell) const = 0;
	virtual bool is_same (const EmptyCell *cell) const { return false; }
	virtual bool is_same (const WallCell *cell) const { return false; }
	virtual bool is_same (const TrapCell *cell) const { return false; }
	virtual bool is_same (const PushableBlockCell *cell) const { return false; }
	virtual bool is_same (const DoorCell *cell) const { return false; }
	virtual bool is_same (const TriggerCell *cell) const { return false; }
	virtual bool is_same (const GoalCell *cell) const { return false; }
};

struct EmptyCell : Cell {
	EmptyCell (int x, int y) : Cell (x, y, 2, 6, true) {}
	Cell *clone () const { return new EmptyCell (x, y); }
	virtual bool is_same (const Cell *cell) const { return cell->is_same (this); }
	virtual bool is_same (const EmptyCell *cell) const { return true; }

	virtual bool can_pass (const Map *map, int incoming_dir) const { return true; }
};

struct WallCell : Cell {
	WallCell (int x, int y) : Cell (x, y, 0, 5, true) {}
	Cell *clone () const { return new WallCell (x, y); }
	virtual bool is_same (const Cell *cell) const { return cell->is_same (this); }
	virtual bool is_same (const WallCell *cell) const { return true; }

	virtual bool can_pass (const Map *map, int incoming_dir) const { return false; }
};

struct TrapCell : Cell {
	TrapCell (int x, int y) : Cell (x, y, 3, 2, true) {}
	Cell *clone () const { return new TrapCell (x, y); }
	virtual bool is_same (const Cell *cell) const { return cell->is_same (this); }
	virtual bool is_same (const TrapCell *cell) const { return true; }

	virtual bool can_pass (const Map *map, int incoming_dir) const { return true; }
	virtual Action *pass (Map *map, int incoming_dir);

	virtual bool is_hole () const { return true; }
};

struct PushableBlockCell : Cell {
	Cell *block_below;

	PushableBlockCell (int x, int y, Cell *block_below) : Cell (x, y, 2, 9, false) {
		this->block_below = block_below;
	}

	~PushableBlockCell () {
		if (block_below)
			delete block_below;
	}

	Cell *clone () const {
		return new PushableBlockCell (x, y, block_below->clone ());
	}

	virtual void toggle () {
		block_below->toggle ();
	}

	virtual bool is_same (const Cell *cell) const { return cell->is_same (this); }
	virtual bool is_same (const PushableBlockCell *cell) const { return block_below->is_same (cell->block_below); }

	virtual bool can_pass (const Map *map, int incoming_dir) const {
		int newx = x + dirs[incoming_dir][0];
		int newy = y + dirs[incoming_dir][1];
		return (map->get_cell (newx, newy)->can_pass (map, incoming_dir));
	}

	virtual Action *pass (Map *map, int incoming_dir);
};

struct DoorCell : Cell {
	bool open;

	DoorCell (int x, int y, bool open) : Cell (x, y, 3, 5, false), open (open) {}
	Cell *clone () const { return new DoorCell (x, y, open); }
	virtual bool is_same (const Cell *cell) const { return cell->is_same (this); }
	virtual bool is_same (const DoorCell *cell) const { return open == cell->open; }

	virtual void render (float alpha) {
		if (open) {
			tilex = 4; tiley = 5;
		} else {
			tilex = 3; tiley = 5;
		}
		Cell::render (alpha);
	};

	virtual bool can_pass (const Map *map, int incoming_dir) const { return open; }

	virtual void toggle () {
		open = !open;
	}
};

struct TriggerCell : Cell {
	int door_x, door_y;

	TriggerCell (int x, int y, int door_x, int door_y) : Cell (x, y, 2, 2, true), door_x (door_x), door_y (door_y) {}
	Cell *clone () const { return new TriggerCell (x, y, door_x, door_y); }
	virtual bool is_same (const Cell *cell) const { return cell->is_same (this); }
	virtual bool is_same (const TriggerCell *cell) const { return true; }

	virtual bool can_pass (const Map *map, int incoming_dir) const { return true; }
	virtual Action *pass (Map *map, int incoming_dir);
};

struct GoalCell : Cell {
	GoalCell (int x, int y) : Cell (x, y, 0, 2, true) {}
	Cell *clone () const { return new GoalCell (x, y); }
	virtual bool is_same (const Cell *cell) const { return cell->is_same (this); }
	virtual bool is_same (const GoalCell *cell) const { return true; }

	virtual bool can_pass (const Map *map, int incoming_dir) const { return true; }
	virtual Action *pass (Map *map, int incoming_dir);
};

struct Action {
	Action *next;

	Action () : next (NULL) {}

	virtual ~Action () {
		if (next) {
			delete next;
		}
	}

	virtual void apply (State *state) = 0;
	virtual void undo (State *state) = 0;

	void add (Action *new_action) {
		if (!next)
			next = new_action;
		else
			next->add (new_action);
	}
};

struct State : Cass::State {
	Player cass;
	Map *map;

	State (const char *filename);
	State (int map_sizex, int map_sizey) {
		map = new Map (map_sizex, map_sizey);
	}
	~State ();

	void render_background (float alpha) {
		for (int x = 0; x < map->get_sizex (); x++) {
			for (int y = 0; y < map->get_sizey (); y++) {
				map->get_cell (x, y)->render (alpha);
			}
		}
	}

	void render_cass (float alpha) {
		cass.render (alpha);
	}

	State *clone () const {
		State *new_state = new State (map->get_sizex (), map->get_sizey ());
		for (int x = 0; x < map->get_sizex (); x++) {
			for (int y = 0; y < map->get_sizey (); y++) {
				new_state->map->set_cell (x, y, map->get_cell (x, y)->clone ());
			}
		}
		new_state->cass = cass;

		return new_state;
	}

	bool can_input (SDL_Keycode keycode) const;
	Action *input (SDL_Keycode keycode) const;

	//
	// Cassandra Interface
	//
	virtual bool equals (const Cass::State *virt_other) const {
		const State *other = (const State *)virt_other;
		int x, y;
		for (x = 0; x < map->get_sizex (); x++) {
			for (y = 0; y < map->get_sizey (); y++) {
				const Cell *cell = map->get_cell (x, y);
				const Cell *other_cell = other->map->get_cell (x, y);
				if (!cell->is_same (other_cell)) return false;
			}
		}
		return true;
	}

	virtual State *get_transition (int i) const {
		Action *action = input (InputKeys[i]);
		if (!action)
			return NULL;

		State *new_state = clone ();
		action->apply (new_state);
		delete action;
		return new_state;
	}

	virtual Hash get_hash () const {
		return cass.x + map->get_sizex () * cass.y;
	}

	virtual bool has_won () const {
		return cass.won;
	}

	virtual void render (Cass::State::Progress progress) {
		if (progress == Cass::State::DEAD_END)
			return;
		render_background (0.5f);
		render_cass (0.5);
	}
};

struct CassAction : Action {
	int inc_x, inc_y;
	bool toggle_dead;
	bool toggle_won;

	CassAction (int inc_x, int inc_y, bool toggle_dead, bool toggle_won) :
		inc_x (inc_x), inc_y (inc_y), toggle_dead (toggle_dead), toggle_won (toggle_won) {}

	void apply (State *state) {
		state->cass.x += inc_x;
		state->cass.y += inc_y;
		state->cass.dead ^= toggle_dead;
		state->cass.won ^= toggle_won;
		if (next)
			next->apply (state);
	}

	void undo (State *state) {
		if (next)
			next->undo (state);
		state->cass.x -= inc_x;
		state->cass.y -= inc_y;
		state->cass.dead ^= toggle_dead;
		state->cass.won ^= toggle_won;
	}
};

struct RemovePushableBlockAction : Action {
	int x, y;

	RemovePushableBlockAction (int x, int y) : x (x), y (y) {}

	void apply (State *state) {
		/* Delete pushable and block below */
		delete state->map->get_cell (x, y);
		state->map->set_cell (x, y, new EmptyCell (x, y));
		if (next)
			next->apply (state);
	}

	void undo (State *state) {
		if (next)
			next->undo (state);
		/* Delete empty cell */
		delete state->map->get_cell (x, y);
		state->map->set_cell (x, y, new PushableBlockCell (x, y, new TrapCell (x, y)));
	}
};

struct PushAction : Action {
	int pushable_x, pushable_y;
	int inc_x, inc_y;
	bool remove_block;

	PushAction (const Map *map, int pushable_x, int pushable_y, int inc_x, int inc_y) : pushable_x (pushable_x), pushable_y (pushable_y), inc_x (inc_x), inc_y (inc_y) {
		if (map->get_cell (pushable_x + inc_x, pushable_y + inc_y)->is_hole ()) {
			add (new RemovePushableBlockAction (pushable_x + inc_x, pushable_y + inc_y));
		}
	}

	void move (State *state, int pushable_x, int pushable_y, int new_x, int new_y) {
		PushableBlockCell *pushable = (PushableBlockCell *)state->map->get_cell (pushable_x, pushable_y);

		Cell *new_below = state->map->get_cell (new_x, new_y);
		state->map->set_cell (new_x, new_y, pushable);
		state->map->set_cell (pushable_x, pushable_y, pushable->block_below);
		pushable->block_below = new_below;
		pushable->x = new_x;
		pushable->y = new_y;
	}

	void apply (State *state) {
		/* FIXME: A PushableBlock should not go _over_ another block */
		move (state, pushable_x, pushable_y, pushable_x + inc_x, pushable_y + inc_y);
		if (next)
			next->apply (state);
	}

	void undo (State *state) {
		if (next)
			next->undo (state);
		move (state, pushable_x + inc_x, pushable_y + inc_y, pushable_x, pushable_y);
	}
};

struct DoorToggleAction : Action {
	int door_x, door_y;

	DoorToggleAction (int door_x, int door_y) : door_x (door_x), door_y (door_y) {}

	void apply (State *state) {
		state->map->get_cell (door_x, door_y)->toggle ();
		if (next)
			next->apply (state);
	}

	void undo (State *state) {
		DoorCell *door = (DoorCell *)state->map->get_cell (door_x, door_y);
		if (next)
			next->undo (state);
		door->toggle ();
	}
};

Action *TrapCell::pass (Map *map, int incoming_dir) {
	return new CassAction (0, 0, true, false);
}

Action *PushableBlockCell::pass (Map *map, int incoming_dir) {
	Action *action;

	action = new PushAction (map, x, y, dirs[incoming_dir][0], dirs[incoming_dir][1]);
	action->add (block_below->pass (map, incoming_dir));

	return action;
}

Action *TriggerCell::pass (Map *map, int incoming_dir) {
	return new DoorToggleAction (door_x, door_y);
}

Action *GoalCell::pass (Map *map, int incoming_dir) {
	return new CassAction (0, 0, false, true);
}

Map::~Map () {
	for (int x = 0; x < sizex; x++) {
		for (int y = 0; y < sizey; y++) {
			delete get_cell (x, y);
		}
	}
	delete cells;
}

State::State (const char *filename) {
	FILE *f;
	int width, height;
	char *textmap;

	f = fopen (filename, "rt");
	if (!f) {
		printf ("Could not open %s", filename);
		return;
	}
	if (fscanf (f, "%d,%d\n", &width, &height) != 2) {
		printf ("Could not read map width & height from file");
		return;
	}
	map = new Map (width, height);

	textmap = new char[width * height];
	for (int y = 0; y < map->get_sizey (); y++) {
		for (int x = 0; x < map->get_sizex (); x++) {
			fscanf (f, "%c", textmap + x * height + y);
		}
		fscanf (f, "\n");
	}
	fclose (f);

	for (int y = 0; y < map->get_sizey (); y++) {
		for (int x = 0; x < map->get_sizex (); x++) {
			char c = textmap[x * height + y];
			switch (c) {
			case '#': map->set_cell (x, y, new WallCell (x, y)); break;
			case '@': cass.x = x; cass.y = y; // Deliverate fallthrough
			case '.': map->set_cell (x, y, new EmptyCell (x, y)); break;
			case '^': map->set_cell (x, y, new TrapCell (x, y)); break;
			case '%': map->set_cell (x, y, new PushableBlockCell (x, y, new EmptyCell (x, y))); break;
			case '*': map->set_cell (x, y, new GoalCell (x, y)); break;
			default:
				if (c >= 'a' && c <= 'z') {
					int id = c - 'a';
					for (int sx = 0; sx < map->get_sizex (); sx++) {
						for (int sy = 0; sy < map->get_sizey (); sy++) {
							char sc = textmap[sx * height + sy];
							if (sc == 'A' + id) {
								DoorCell *door = new DoorCell (sx, sy, false);
								map->set_cell (sx, sy, door);
								map->set_cell (x, y, new TriggerCell (x, y, sx, sy));
							}
						}
					}
				} else
					if (c < 'A' && c > 'Z') {
						printf ("Unknown char '%c' at %d, %d", c, x, y);
						return;
					}
				break;
			}
		}
	}
	delete textmap;

	cass.dead = false;
	cass.won = false;
}

State::~State () {
	delete map;
}
bool State::can_input (SDL_Keycode keycode) const {
	int dir = -1;
	switch (keycode) {
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
	if (!cass.dead && !cass.won && dir > -1) {
		int new_x = cass.x + dirs[dir][0];
		int new_y = cass.y + dirs[dir][1];
		if (map->get_cell (new_x, new_y)->can_pass (map, dir)) return true;
	}
	return false;
}

Action *State::input (SDL_Keycode keycode) const {
	int dir = -1;
	Action *action = NULL;
	switch (keycode) {
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
	if (!cass.dead && !cass.won && dir > -1) {
		int new_x = cass.x + dirs[dir][0];
		int new_y = cass.y + dirs[dir][1];
		if (map->get_cell (new_x, new_y)->can_pass (map, dir)) {
			action = new CassAction (dirs[dir][0], dirs[dir][1], false, false);
			action->add (map->get_cell (new_x, new_y)->pass (map, dir));
		}
	}

	return action;
}

struct Game {
	bool finished;
	int max_depth;
	bool show_ghosts;
	State *state;

	Game (const char *filename) : finished (false), show_ghosts (true), max_depth (3) {
		state = new State (filename);
	}

	~Game () {
		delete state;
	}

	bool can_input (SDL_Keycode keycode) {
		int dir = -1;
		switch (keycode) {
		case SDLK_ESCAPE:
		case SDLK_SPACE:
		case SDLK_KP_PLUS:
		case SDLK_KP_MINUS:
			return true;
		default:
			return state->can_input (keycode);
		}
	}

	Action *input (SDL_Keycode keycode) {
		int dir = -1;
		Action *action = NULL;
		switch (keycode) {
		case SDLK_ESCAPE:
			finished = true;
			break;
		case SDLK_SPACE:
			show_ghosts = !show_ghosts;
			break;
		case SDLK_KP_PLUS:
			max_depth++;
			break;
		case SDLK_KP_MINUS:
			max_depth = max_depth > 1 ? max_depth - 1 : max_depth;
			break;
		default:
			action = state->input (keycode);
			break;
		}

		return action;
	}
};

#if 0
	void render () {
		glDisable (GL_TEXTURE_2D);
		for (int x = 0; x < sizex; x++) {
			for (int y = 0; y < sizey; y++) {
				StateNode *node = get_at (x, y), *best = node;
				if (!node)
					continue;
				int total_nodes = 0, in_process_nodes = 0;
				while (node) {
					total_nodes++;
					if (node->view_state == IN_PROCESS)
						in_process_nodes++;
					if (node->view_state > best->view_state)
						best = node;
					node = node->next_in_cell;
				}
				float col = in_process_nodes / (float)total_nodes;
				switch (best->view_state) {
				case DEAD_END: glColor4ub (0, 0, 0, 0); break;
				case IN_PROCESS: glColor4f (col, col, col, 0.5f); break;
				case GOAL: glColor4ub (255, 255, 0, 64); break;
				}
				glBegin (GL_TRIANGLE_STRIP);
				for (int sx = 0; sx < 2; sx++) {
					for (int sy = 0; sy < 2; sy++) {
						glVertex2i ((x + sx) * cell_width, (y + sy) * cell_height);
					}
				}
				glEnd ();
			}
		}
		glEnable (GL_TEXTURE_2D);
	}

	void print () {
		static const char state_names[4][3] = { "DE", "PR", "GO" };
		printf ("+");
		for (int x = 0; x < sizex; x++) {
			printf ("-----------------+");
		}
		printf ("\n");
		for (int y = 0; y < sizey; y++) {
			int max_height = 0;
			for (int x = 0; x < sizex; x++) {
				int height = 0;
				StateNode *node = get_at (x, y);
				while (node) {
					node = node->next_in_cell;
					height++;
				}
				if (height > max_height) max_height = height;
			}
			for (int n = 0; n < max_height; n++) {
				for (int i = 0; i < NUM_INPUTS + (n < max_height - 1 ? 1 : 0); i++) {
					printf ("|");
					for (int x = 0; x < sizex; x++) {
						StateNode *node = get_at (x, y);
						for (int nn = 0; nn < n; nn++) {
							if (node) node = node->next_in_cell;
						}
						if (!node) {
							printf ("                 ");
						} else {
							switch (i) {
							case 0:
								printf ("%08x>", ((int)node) & 0xFFFFFFFF);
								break;
							case 1:
								printf ("steps%3d ", node->steps);
								break;
							case 2:
								printf ("state %s ", state_names[node->view_state]);
								break;
							case 3:
								printf ("%4s%4s ", node->cache->cass.dead ? "Dead" : "", node->cache->cass.won ? "Won" : "");
								break;
							default:
								printf ("         ");
								break;
							}
							if (i >= NUM_INPUTS || !node->transition[i]) {
								printf ("        ");
							} else
							if (node->transition[i]->target) {
								printf ("%08x", ((int)node->transition[i]->target) & 0xFFFFFFFF);
							} else {
								printf ("........");
							}
						}
						printf ("|");
					}
					printf ("\n");
				}
			}
			printf ("+");
			for (int x = 0; x < sizex; x++) {
				printf ("-----------------+");
			}
			printf ("\n");
		}
	}
#endif

int main (int argc, char *argv[]) {
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	if (SDL_Init (SDL_INIT_EVERYTHING) != 0) {
		printf ("SDL Init error: %s\n", SDL_GetError ());
		return 1;
	}


	SDL_Window *win = SDL_CreateWindow ("Cassandra Test 1", 50, 50, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
	if (win == NULL){
		printf ("SDL_CreateWindow Error: %s\n", SDL_GetError ());
		SDL_Quit ();
		return 1;
	}

	SDL_GLContext gl_context = SDL_GL_CreateContext (win);
	if (!gl_context) {
		printf ("SDL_GL_CreateContext Error: %s\n", SDL_GetError ());
	}

	if (SDL_GL_MakeCurrent (win, gl_context) != 0) {
		printf ("SDL_GL_MakeCurrent Error: %s\n", SDL_GetError ());
	}

	GLenum result = glewInit ();
	if (result != GLEW_OK) {
		printf ("glewInit Error: %s\n", glewGetErrorString (result));
	}

	printf ("GLRenderer: %s\n", glGetString (GL_RENDERER));
	printf ("GLVendor: %s\n", glGetString (GL_VENDOR));
	printf ("GLVersion: %s\n", glGetString (GL_VERSION));
	printf ("GLSLVersion: %s\n", glGetString (GL_SHADING_LANGUAGE_VERSION));

	glViewport (0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	glMatrixMode (GL_PROJECTION);
	glOrtho (0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, -1, 1);

	/* Load tiles and add ALPHA channel */
	SDL_Surface *tiles = SDL_LoadBMP ("..\\tiles.bmp");
	if (!tiles) {
		printf ("SDL_LoadBMP Error: %s\n", SDL_GetError ());
	}
	GLuint tiles_tex;
	GLubyte *raw = (GLubyte *)malloc (tiles->w * tiles->h * 4);
	for (int y = 0; y < tiles->h; y++) {
		GLubyte *srcrow = (GLubyte*)tiles->pixels + y * tiles->pitch;
		GLubyte *dstrow = raw + y * tiles->w * 4;
		for (int x = 0; x < tiles->w; x++) {
			GLubyte *srcpixel = srcrow + x * 3;
			GLubyte *dstpixel = dstrow + x * 4;
			dstpixel[0] = srcpixel[2];
			dstpixel[1] = srcpixel[1];
			dstpixel[2] = srcpixel[0];
			dstpixel[3] = srcpixel[0] == 0xFF && srcpixel[1] == 0xFF && srcpixel[2] == 0xFF ? 0 : 0xFF;
		}
	}
	glGenTextures (1, &tiles_tex);
	glBindTexture (GL_TEXTURE_2D, tiles_tex);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, tiles->w, tiles->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, raw);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	tiles_width = tiles->w;
	tiles_height = tiles->h;
	SDL_FreeSurface (tiles);
	free (raw);

	glEnable (GL_TEXTURE_2D);
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	Game game ("..\\map1.txt");
	Cass::Solver *solver = Cass::get_solver (game.state->map->get_sizex () * game.state->map->get_sizey (), NUM_INPUTS);
	solver->add_start_point (game.state->clone ());

	SDL_Event e;
	bool quit = false;
	Action *action;
	while (!quit) {
		int pending = 0;

		if (!solver->done ()) {
			Uint32 last_time = SDL_GetTicks ();
			while (!solver->done () && SDL_GetTicks () - last_time < 33) {
				solver->process ();
			}
			solver->calc_view_state ();
			pending = SDL_PollEvent (&e);
		} else {
			pending = SDL_WaitEventTimeout (&e, 33);
		}

		if (pending) {
			switch (e.type) {
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_KEYDOWN:
#if 0
				if (e.key.keysym.sym == SDLK_p)
					node_hash.print ();
#endif
				action = game.input (e.key.keysym.sym);
				if (action) {
					action->apply (game.state);
					delete action;

					for (int i = 0; i < NUM_INPUTS; i++) {
						// FIXME When this transition has not been calculated yet
						if (InputKeys[i] == e.key.keysym.sym) {
							solver->update (i);
							solver->calc_view_state ();
						}
					}
				}
				if (game.finished)
					quit = true;
				break;
			}
		}

		// Render solid world
		glClear (GL_COLOR_BUFFER_BIT);
		game.state->render_background (1.f);
		game.state->render_cass (1.f);

		if (game.show_ghosts) {
			// Render ghosts
			solver->render (game.max_depth);
		}

		SDL_GL_SwapWindow (win);
	}

	SDL_GL_DeleteContext (gl_context);

	delete solver;

	return 0;
}