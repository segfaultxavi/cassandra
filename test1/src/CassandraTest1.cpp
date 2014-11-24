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

struct Cell : Renderable {
	Map *map;
	int x, y;
	bool inmutable;

	Cell (Map *map, int x, int y, int tilex, int tiley, bool inmutable) :
		Renderable (tilex, tiley), map (map), x (x), y (y), inmutable (inmutable) {}

	virtual ~Cell () {}

	virtual void render (float alpha) {
		if (inmutable && alpha <= 1.0)
			return;
		Renderable::render (x, y, alpha);
	};

	virtual bool can_pass (int incoming_dir) = 0;
	virtual void pass (Cass *cass, Undo **undo = NULL) {};
	virtual bool is_hole () { return false; }
};

struct EmptyCell : Cell {
	EmptyCell (Map *map, int x, int y) : Cell (map, x, y, 2, 6, true) {}

	virtual bool can_pass (int incoming_dir) { return true; }
};

struct WallCell : Cell {
	WallCell (Map *map, int x, int y) : Cell (map, x, y, 0, 5, true) {}

	virtual bool can_pass (int incoming_dir) { return false; }
};

struct TrapCell : Cell {
	TrapCell (Map *map, int x, int y) : Cell (map, x, y, 2, 6, true) {}

	virtual bool can_pass (int incoming_dir) { return true; }
	virtual void pass (Cass *cass, Undo **undo = NULL) {
		cass->dead = true;
	}

	virtual bool is_hole () { return true; }
};

struct PushableBlockCell : Cell {
	Cell *block_below;

	PushableBlockCell (Map *map, int x, int y) : Cell (map, x, y, 2, 9, false) {
		block_below = new EmptyCell (map, x, y);
	}

	~PushableBlockCell () {
		if (block_below)
			delete block_below;
	}

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

		/* FIXME: A PushableBlock should not go _over_ another block */
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

struct DoorCell : Cell {
	int id;
	bool open;

	DoorCell (Map *map, int x, int y, int id) : Cell (map, x, y, 3, 5, false), id (id), open (false) {}

	virtual void render (float alpha) {
		if (open) {
			tilex = 4; tiley = 5;
		} else {
			tilex = 3; tiley = 5;
		}
		Cell::render (alpha);
	};

	virtual bool can_pass (int incoming_dir) { return open; }

	void toggle () {
		open = !open;
	}
};

struct TriggerCell : Cell {
	DoorCell *door;
	int id;

	TriggerCell (Map *map, int x, int y, DoorCell *door, int id) : Cell (map, x, y, 2, 2, true), door (door), id (id) {}

	virtual bool can_pass (int incoming_dir) { return true; }

	virtual void pass (Cass *cass, Undo **undo = NULL) {
		if (undo) {
			*undo = new DoorToggleUndo (door);
		}
		door->toggle ();
	}
};

struct State {
	bool finished;
	int max_depth;
	bool show_ghosts;
	Cass cass;
	Map map;

	State (const char text_map[MAP_HEIGHT][MAP_WIDTH + 1]): finished (false), show_ghosts (false), max_depth (3) {
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

void recurse (State *state, float alpha, int depth) {
	static const SDL_Keycode actions[4] = { SDLK_UP, SDLK_RIGHT, SDLK_DOWN, SDLK_LEFT };
	static const int weights[4] = { 3, 2, 1, 2 };
	int probs[4], i;
	float total_prob = 0.f;

	if (depth >= state->max_depth) return;

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
			state->render_background (p);
			state->render_cass (p);
			recurse (state, p, depth + 1);
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
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA16F, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
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

		// Render solid world
		glBindTexture (GL_TEXTURE_2D, tiles_tex);
		glBindFramebuffer (GL_FRAMEBUFFER, 0);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glUseProgram (0);
		glClear (GL_COLOR_BUFFER_BIT);
		current_state.render_background (2.f);
		current_state.render_cass (2.f);

		if (current_state.show_ghosts) {
			// Render ghosts on ghostplane
			glBindTexture (GL_TEXTURE_2D, tiles_tex);
			glBindFramebuffer (GL_FRAMEBUFFER, ghostplane_fbo);
			glBlendFunc (GL_SRC_ALPHA_SATURATE, GL_ONE);
			glClear (GL_COLOR_BUFFER_BIT);
			recurse (&current_state, 1.f, 0);

			// Render ghostplane
			glBindFramebuffer (GL_FRAMEBUFFER, 0);
			glBlendFunc (GL_ONE, GL_ONE);
			glBindTexture (GL_TEXTURE_2D, ghostplane_tex);
			GLint tex = glGetUniformLocation (ghostplane_program, "tex");
			glProgramUniform1i (ghostplane_program, tex, 0);
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