#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <memory.h>
#include <GL/glew.h>
#include "SDL.h"

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 640
#define MAP_WIDTH 20
#define MAP_HEIGHT 20
#define CELL_WIDTH (SCREEN_WIDTH / MAP_WIDTH)
#define CELL_HEIGHT (SCREEN_HEIGHT / MAP_HEIGHT)

extern unsigned char tiles_data[];
int tiles_width, tiles_height;

struct Action;
struct Cell;
struct DoorCell;
struct PushableBlockCell;
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
				glVertex2i ((x + sx) * CELL_WIDTH, (y + sy) * CELL_HEIGHT);
			}
		}
		glEnd ();
	}
};

struct Cass : Renderable {
	int x, y;
	int dir;
	bool dead;
	int steps;

	Cass () : Renderable (4, 0) {}

	void render (float alpha) {
		if (dead) {
			tilex = 8; tiley = 3;
		} else {
			tilex = 4; tiley = 0;
		}
		Renderable::render (x, y, alpha);
	}
};

struct Map {
	Cell *cells[MAP_WIDTH][MAP_HEIGHT];
};

struct Cell : Renderable {
	int x, y;
	bool inmutable;

	Cell (int x, int y, int tilex, int tiley, bool inmutable) :
		Renderable (tilex, tiley), x (x), y (y), inmutable (inmutable) {}

	virtual ~Cell () {}

	virtual Cell *clone () = 0;

	virtual void render (float alpha) {
		if (inmutable && alpha <= 1.0)
			return;
		Renderable::render (x, y, alpha);
	};

	virtual bool can_pass (const Map *map, int incoming_dir) = 0;
	virtual Action *pass (Map *map, int incoming_dir) { return NULL; };
	virtual bool is_hole () { return false; }
};

struct EmptyCell : Cell {
	EmptyCell (int x, int y) : Cell (x, y, 2, 6, true) {}
	Cell *clone () { return new EmptyCell (*this); }

	virtual bool can_pass (const Map *map, int incoming_dir) { return true; }
};

struct WallCell : Cell {
	WallCell (int x, int y) : Cell (x, y, 0, 5, true) {}
	Cell *clone () { return new WallCell (*this); }

	virtual bool can_pass (const Map *map, int incoming_dir) { return false; }
};

struct TrapCell : Cell {
	TrapCell (int x, int y) : Cell (x, y, 2, 6, true) {}
	Cell *clone () { return new TrapCell (*this); }

	virtual bool can_pass (const Map *map, int incoming_dir) { return true; }
	virtual Action *pass (Map *map, int incoming_dir);

	virtual bool is_hole () { return true; }
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

	Cell *clone () {
		PushableBlockCell *new_pushable = new PushableBlockCell (*this);
		new_pushable->block_below = block_below->clone ();
		return new_pushable;
	}

	virtual bool can_pass (const Map *map, int incoming_dir) {
		int newx = x + dirs[incoming_dir][0];
		int newy = y + dirs[incoming_dir][1];
		return (map->cells[newx][newy]->can_pass (map, incoming_dir));
	}

	virtual Action *pass (Map *map, int incoming_dir);
};

struct DoorCell : Cell {
	bool open;

	DoorCell (int x, int y, bool open) : Cell (x, y, 3, 5, false), open (open) {}
	Cell *clone () { return new DoorCell (*this); }

	virtual void render (float alpha) {
		if (open) {
			tilex = 4; tiley = 5;
		} else {
			tilex = 3; tiley = 5;
		}
		Cell::render (alpha);
	};

	virtual bool can_pass (const Map *map, int incoming_dir) { return open; }

	void toggle () {
		open = !open;
	}
};

struct TriggerCell : Cell {
	int door_x, door_y;

	TriggerCell (int x, int y, int door_x, int door_y) : Cell (x, y, 2, 2, true), door_x (door_x), door_y (door_y) {}
	Cell *clone () { return new TriggerCell (*this); }

	virtual bool can_pass (const Map *map, int incoming_dir) { return true; }
	virtual Action *pass (Map *map, int incoming_dir);
};

struct State {
	Cass cass;
	Map map;

	State (const char text_map[MAP_HEIGHT][MAP_WIDTH + 1]);
	~State ();

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

	State *clone () {
		State *new_state = new State (*this);
		for (int x = 0; x < MAP_WIDTH; x++) {
			for (int y = 0; y < MAP_HEIGHT; y++) {
				new_state->map.cells[x][y] = map.cells[x][y]->clone ();
			}
		}

		return new_state;
	}
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

struct CassAction : Action {
	int inc_x, inc_y;
	bool toggle_dead;

	CassAction (int inc_x, int inc_y, bool toggle_dead) : inc_x (inc_x), inc_y (inc_y), toggle_dead (toggle_dead) {}

	void apply (State *state) {
		state->cass.x += inc_x;
		state->cass.y += inc_y;
		state->cass.steps++;
		state->cass.dead ^= toggle_dead;
		if (next)
			next->apply (state);
	}

	void undo (State *state) {
		if (next)
			next->undo (state);
		state->cass.x -= inc_x;
		state->cass.y -= inc_y;
		state->cass.steps--;
		state->cass.dead ^= toggle_dead;
	}
};

struct RemovePushableBlockAction : Action {
	int x, y;

	RemovePushableBlockAction (int x, int y) : x (x), y (y) {}

	void apply (State *state) {
		/* Delete pushable and block below */
		delete state->map.cells[x][y];
		state->map.cells[x][y] = new EmptyCell (x, y);
		if (next)
			next->apply (state);
	}

	void undo (State *state) {
		if (next)
			next->undo (state);
		/* Delete empty cell */
		delete state->map.cells[x][y];
		state->map.cells[x][y] = new PushableBlockCell (x, y, new TrapCell (x, y));
	}
};

struct PushAction : Action {
	int pushable_x, pushable_y;
	int inc_x, inc_y;
	bool remove_block;

	PushAction (const Map *map, int pushable_x, int pushable_y, int inc_x, int inc_y) : pushable_x (pushable_x), pushable_y (pushable_y), inc_x (inc_x), inc_y (inc_y) {
		if (map->cells[pushable_x + inc_x][pushable_y + inc_y]->is_hole ()) {
			add (new RemovePushableBlockAction (pushable_x + inc_x, pushable_y + inc_y));
		}
	}

	void move (State *state, int pushable_x, int pushable_y, int new_x, int new_y) {
		PushableBlockCell *pushable = (PushableBlockCell *)state->map.cells[pushable_x][pushable_y];

		Cell *new_below = state->map.cells[new_x][new_y];
		state->map.cells[new_x][new_y] = pushable;
		state->map.cells[pushable_x][pushable_y] = pushable->block_below;
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
		DoorCell *door = (DoorCell *)state->map.cells[door_x][door_y];
		door->toggle ();
		if (next)
			next->apply (state);
	}

	void undo (State *state) {
		DoorCell *door = (DoorCell *)state->map.cells[door_x][door_y];
		if (next)
			next->undo (state);
		door->toggle ();
	}
};

Action *TrapCell::pass (Map *map, int incoming_dir) {
	return new CassAction (0, 0, true);
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

State::State (const char text_map[MAP_HEIGHT][MAP_WIDTH + 1]) {
	int x, y;
	for (x = 0; x < MAP_WIDTH; x++) {
		for (y = 0; y < MAP_HEIGHT; y++) {
			switch (text_map[y][x]) {
			case '#': map.cells[x][y] = new WallCell (x, y); break;
			case '@': cass.x = x; cass.y = y; cass.steps = 0;// Deliverate fallthrough
			case '.': map.cells[x][y] = new EmptyCell (x, y); break;
			case '^': map.cells[x][y] = new TrapCell (x, y); break;
			case '%': map.cells[x][y] = new PushableBlockCell (x, y, new EmptyCell (x, y)); break;
			}
			if (text_map[y][x] >= 'a' && text_map[y][x] <= 'z') {
				int id = text_map[y][x] - 'a';
				for (int sx = 0; sx < MAP_WIDTH; sx++) {
					for (int sy = 0; sy < MAP_HEIGHT; sy++) {
						if (text_map[sy][sx] == 'A' + id) {
							DoorCell *door = new DoorCell (sx, sy, false);
							map.cells[sx][sy] = door;
							map.cells[x][y] = new TriggerCell (x, y, sx, sy);
						}
					}
				}
			}
		}
	}
	cass.dir = 0;
	cass.dead = false;
}

State::~State () {
	int x, y;
	for (x = 0; x < MAP_WIDTH; x++) {
		for (y = 0; y < MAP_HEIGHT; y++) {
			delete map.cells[x][y];
		}
	}
}

struct Game {
	bool finished;
	int max_depth;
	bool show_ghosts;
	State state;

	Game (const char text_map[MAP_HEIGHT][MAP_WIDTH + 1]) : finished (false), show_ghosts (false), max_depth (3), state (text_map) {}

	bool can_input (SDL_Keycode keycode) {
		int dir = -1;
		switch (keycode) {
		case SDLK_ESCAPE:
		case SDLK_SPACE:
		case SDLK_KP_PLUS:
		case SDLK_KP_MINUS:
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
		if (!state.cass.dead && dir > -1) {
			int new_x = state.cass.x + dirs[dir][0];
			int new_y = state.cass.y + dirs[dir][1];
			if (state.map.cells[new_x][new_y]->can_pass (&state.map, dir)) return true;
		}
		return false;
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
		if (!state.cass.dead && dir > -1) {
			int new_x = state.cass.x + dirs[dir][0];
			int new_y = state.cass.y + dirs[dir][1];
			if (state.map.cells[new_x][new_y]->can_pass (&state.map, dir)) {
				action = new CassAction (dirs[dir][0], dirs[dir][1], false);
				action->add (state.map.cells[new_x][new_y]->pass (&state.map, dir));
			}
		}

		return action;
	}
};

void recurse (Game *game, float alpha, int depth) {
	static const SDL_Keycode actions[4] = { SDLK_UP, SDLK_RIGHT, SDLK_DOWN, SDLK_LEFT };
	static const int weights[4] = { 3, 2, 1, 2 };
	int probs[4], i;
	float total_prob = 0.f;

	if (depth >= game->max_depth) return;

	if (alpha < 0.001f) return;

	for (i = 0; i < 4; i++) {
		if (game->can_input (actions[i])) {
			probs[i] = weights[(i + 4 - game->state.cass.dir) % 4];
		} else {
			probs[i] = 0;
		}
		total_prob += probs[i];
	}

	for (i = 0; i < 4; i++) {
		if (probs[i] > 0) {
			Action *action;
			float p = alpha * probs[i] / total_prob;
			action = game->input (actions[i]);
			if (action) {
				action->apply (&game->state);
				game->state.render_background (p);
				game->state.render_cass (p);
				recurse (game, p, depth + 1);
				action->undo (&game->state);
			}
			delete action;
		}
	}
}

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

	/* Create ghostplane texture */
	GLuint ghostplane_tex, ghostplane_fbo;
	glGenFramebuffers (1, &ghostplane_fbo);
	glBindFramebuffer (GL_FRAMEBUFFER, ghostplane_fbo);
	glGenTextures (1, &ghostplane_tex);
	glBindTexture (GL_TEXTURE_2D, ghostplane_tex);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA32F, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ghostplane_tex, 0);
	result = glCheckFramebufferStatus (GL_FRAMEBUFFER);
	if (result != GL_FRAMEBUFFER_COMPLETE) {
		printf ("FBO incomplete\n");
	}

	/* Ghostplane rendering shader */
	char *ghostplane_vertex_shader_source =
		"void main() {"
		"  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;"
		"  gl_TexCoord[0] = gl_MultiTexCoord0;"
		"}";

	char *ghostplane_fragment_shader_source =
		"uniform sampler2D tex;"
		"void main() {"
		"  vec4 color = texture2D(tex, gl_TexCoord[0].st);"
		"  gl_FragColor.r = color.r / color.a;"
		"  gl_FragColor.g = color.g / color.a;"
		"  gl_FragColor.b = color.b / color.a;"
		"  gl_FragColor.a = 1.0;"
		"}";

	GLuint ghostplane_program = glCreateProgram ();
	GLuint ghostplane_vertex_shader = glCreateShader (GL_VERTEX_SHADER);
	GLuint ghostplane_fragment_shader = glCreateShader (GL_FRAGMENT_SHADER);

	glShaderSource (ghostplane_vertex_shader, 1, &ghostplane_vertex_shader_source, NULL);
	glShaderSource (ghostplane_fragment_shader, 1, &ghostplane_fragment_shader_source, NULL);

	glCompileShaderARB (ghostplane_vertex_shader);
	glCompileShaderARB (ghostplane_fragment_shader);

	glAttachShader (ghostplane_program, ghostplane_vertex_shader);
	glAttachShader (ghostplane_program, ghostplane_fragment_shader);

	GLint status;
	char log[1024];
	GLsizei logsize;
	glLinkProgram (ghostplane_program);
	glGetProgramiv (ghostplane_program, GL_LINK_STATUS, &status);
	glGetProgramInfoLog (ghostplane_program, 1024, &logsize, log);
	if (status != 1 || logsize != 0) {
		printf ("Program failed (link status %d):\n%s", status, log);
	}
	GLint tex_uniform = glGetUniformLocation (ghostplane_program, "tex");
	glProgramUniform1i (ghostplane_program, tex_uniform, 0);

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
	Game game (textMap);

	SDL_Event e;
	bool quit = false;
	Action *action;
	while (!quit) {
		if (SDL_WaitEvent (&e)) {
			switch (e.type) {
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_KEYDOWN:
				action = game.input (e.key.keysym.sym);
				if (action) {
					action->apply (&game.state);
					delete action;
				}
				if (game.finished)
					quit = true;
				break;
			}
		}

		// Render solid world
		glBindTexture (GL_TEXTURE_2D, tiles_tex);
		glBindFramebuffer (GL_FRAMEBUFFER, 0);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glUseProgram (0);
		glClear (GL_COLOR_BUFFER_BIT);
		game.state.render_background (2.f);
		game.state.render_cass (2.f);

		if (game.show_ghosts) {
			// Render ghosts on ghostplane
			glBindTexture (GL_TEXTURE_2D, tiles_tex);
			glBindFramebuffer (GL_FRAMEBUFFER, ghostplane_fbo);
			glBlendFunc (GL_SRC_ALPHA, GL_ONE);
			glClear (GL_COLOR_BUFFER_BIT);
			recurse (&game, 1.f, 0);

			// Render ghostplane
			glBindFramebuffer (GL_FRAMEBUFFER, 0);
			glBlendFunc (GL_ONE, GL_ONE);
			glBindTexture (GL_TEXTURE_2D, ghostplane_tex);
			glUseProgram (ghostplane_program);
			glColor3f (1.f, 1.f, 1.f);
			glBegin (GL_TRIANGLE_STRIP);
			glTexCoord2f (0.f, 1.f); glVertex2i (0, 0);
			glTexCoord2f (1.f, 1.f); glVertex2i (SCREEN_WIDTH, 0);
			glTexCoord2f (0.f, 0.f); glVertex2i (0, SCREEN_HEIGHT);
			glTexCoord2f (1.f, 0.f); glVertex2i (SCREEN_WIDTH, SCREEN_HEIGHT);
			glEnd ();
		}

		SDL_GL_SwapWindow (win);
	}

	SDL_GL_DeleteContext (gl_context);
	return 0;
}