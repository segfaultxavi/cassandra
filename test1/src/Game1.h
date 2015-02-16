#ifndef __GAME1_H__
#define __GAME1_H__

#include "Cassandra.h"

namespace Game1 {

	enum Input {
		UP,
		DOWN,
		LEFT,
		RIGHT,
		NUM_INPUTS,
		NONE
	};

	// Applications must point the global pointer below to an instance of this interface in order
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
		State (const char *filename);
		State () {}
		virtual ~State () {}

		virtual int get_map_size_x () = 0;
		virtual int get_map_size_y () = 0;

		virtual void render (float alpha) = 0;

		virtual bool can_input (Input input_code) const = 0;
		virtual void input (Input input_code) = 0;

		virtual Cass::Solver *get_solver () = 0;
	};

	State *load_state (const char *filename);
}

#endif