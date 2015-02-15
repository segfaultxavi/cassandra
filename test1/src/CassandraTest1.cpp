#define _CRT_SECURE_NO_WARNINGS
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <memory.h>
#include <GL/glew.h>
#include "SDL.h"
#include "Cassandra.h"
#include "Game1.h"

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 640

extern unsigned char tiles_data[];
int tiles_width, tiles_height;
int cell_width, cell_height;

void Game1::render_tile_func (int x, int y, Game1::TileType type, float alpha) {
	static const int tiles[][2] = {
		{ 4, 0 }, // PLAYER_ALIVE
		{ 8, 3 }, // PLAYER_DEAD
		{ 9, 0 }, // PLAYER_WON
		{ 2, 6 }, // EMPTY
		{ 0, 5 }, // WALL
		{ 3, 2 }, // TRAP
		{ 4, 5 }, // DOOR_OPEN
		{ 3, 5 }, // DOOR_CLOSED
		{ 2, 2 }, // TRIGGER
		{ 2, 9 }, // PUSHABLE_BLOCK
		{ 0, 2 }, // GOAL
	};

	glColor4f (1.f, 1.f, 1.f, alpha);
	glBegin (GL_TRIANGLE_STRIP);
	for (int sx = 0; sx < 2; sx++) {
		for (int sy = 0; sy < 2; sy++) {
			glTexCoord2f ((tiles[type][0] + sx) * 32 / (GLfloat)tiles_width, (tiles[type][1] + sy) * 32 / (GLfloat)tiles_height);
			glVertex2i ((x + sx) * cell_width, (y + sy) * cell_height);
		}
	}
	glEnd ();
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
	glBlendFunc (GL_SRC_ALPHA, GL_ONE);

	int max_depth = 6;
	bool show_ghosts = true;
	Game1::State *current_state = new Game1::State ("..\\map1.txt");
	Cass::Solver *solver = Cass::get_solver (current_state->map->get_sizex () * current_state->map->get_sizey (), Game1::NUM_INPUTS);
	solver->add_start_point (current_state->clone ());

	cell_width = SCREEN_WIDTH / current_state->map->get_sizex ();
	cell_height = SCREEN_HEIGHT / current_state->map->get_sizey ();

	SDL_Event e;
	bool quit = false;
	int anim_step = 0, anim_delay = 0;
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
			Game1::Input input = Game1::Input::NONE;
			switch (e.type) {
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_KEYDOWN:
				switch (e.key.keysym.sym) {
				case SDLK_ESCAPE:
					quit = true;
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
					input = Game1::Input::UP;
					break;
				case SDLK_DOWN:
					input = Game1::Input::DOWN;
					break;
				case SDLK_LEFT:
					input = Game1::Input::LEFT;
					break;
				case SDLK_RIGHT:
					input = Game1::Input::RIGHT;
					break;
				default:
					break;
				}
			default:
				break;
			}
			if (input != Game1::Input::NONE) {
				Game1::Action *action = current_state->input (input);
				if (action) {
					action->apply (current_state);
					delete action;

					solver->update (input);
					solver->calc_view_state ();
					anim_step = 0;
				}
			}
		}

		// Render solid world
		glClear (GL_COLOR_BUFFER_BIT);
		current_state->render (1.f);

		if (show_ghosts) {
			// Render ghosts
			solver->render (anim_step);
		}

		anim_delay++;
		if (anim_delay == 4) {
			anim_delay = 0;
			anim_step++;
			if (anim_step == max_depth)
				anim_step = 0;
		}

		SDL_GL_SwapWindow (win);
	}

	SDL_GL_DeleteContext (gl_context);

	delete current_state;
	delete solver;

	return 0;
}