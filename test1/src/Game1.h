#ifndef __GAME1_H__
#define __GAME1_H__

#include "Cassandra.h"

namespace Game1 {
	// This game consists of a 2D array of cells and a player which moves around
	// and interacts with the cells. Some are floors, some are walls, and some are
	// traps, buttons, doors and pushable blocks.

	// Possible inputs
	enum Input {
		UP,
		DOWN,
		LEFT,
		RIGHT,
		NUM_INPUTS,
		NONE
	};

	// Applications must set the global pointer below to an instance of this interface in order
	// to have render.
	class Renderer {
	public:
		virtual void renderPlayer (int x, int y, bool dead, bool won, float alpha) = 0;
		virtual void renderEmptyCell (int x, int y, float alpha) = 0;
		virtual void renderWallCell (int x, int y, float alpha) = 0;
		virtual void renderTrapCell (int x, int y, float alpha) = 0;
		virtual void renderDoorCell (int x, int y, bool open, float alpha) = 0;
		virtual void renderTriggerCell (int x, int y, float alpha) = 0;
		virtual void renderPushableBlockCell (int x, int y, float alpha) = 0;
		virtual void renderGoalCell (int x, int y, float alpha) = 0;
	};

	extern Renderer *g_renderer;

	// This represents the game state. Applications use this interface to interact with the game.
	class State : public Cass::State {
	public:
		// Build from a text file
		State (const char *filename) {}
		State () {}
		virtual ~State () {}

		// Map dimensions
		virtual int get_map_size_x () const = 0;
		virtual int get_map_size_y () const = 0;

		// Render this state with some transparency
		virtual void render (float alpha) = 0;

		// Can this input be used in this state?
		virtual bool can_input (Input input_code) const = 0;
		// Use this input (The state is changed)
		virtual void input (Input input_code) = 0;

		// Get a solver
		virtual Cass::Solver *get_solver () = 0;
	};

	State *load_state (const char *filename);
}

#endif