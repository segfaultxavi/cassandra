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

	struct Player;
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

	struct Player {
		int x, y;
		bool dead;
		bool won;

		bool equals (const Player *other) {
			return x == other->x && y == other->y && dead == other->dead && won == other->won;
		}
	};

	struct Map {
		Map (int sizex, int sizey) : sizex (sizex), sizey (sizey) {
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

	struct Cell {
		int x, y;

		Cell (int x, int y) : x (x), y (y) {}

		virtual ~Cell () {}

		virtual void render (float alpha) const = 0;
		virtual Cell *clone () const = 0;

		virtual bool can_pass (const Map *map, int incoming_dir) const = 0;
		virtual Action *pass (Map *map, int incoming_dir) { return NULL; };
		virtual bool is_hole () const { return false; }
		virtual void toggle () {}

		virtual bool equals (const Cell *cell) const = 0;
		virtual bool equals (const EmptyCell *cell) const { return false; }
		virtual bool equals (const WallCell *cell) const { return false; }
		virtual bool equals (const TrapCell *cell) const { return false; }
		virtual bool equals (const PushableBlockCell *cell) const { return false; }
		virtual bool equals (const DoorCell *cell) const { return false; }
		virtual bool equals (const TriggerCell *cell) const { return false; }
		virtual bool equals (const GoalCell *cell) const { return false; }
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

		void render (float alpha, const State *current = NULL) {
			for (int x = 0; x < map->get_sizex (); x++) {
				for (int y = 0; y < map->get_sizey (); y++) {
					if (current && map->get_cell (x, y)->equals (current->map->get_cell (x, y)))
						continue;
					map->get_cell (x, y)->render (alpha);
				}
			}

			if (current && cass.equals (&current->cass))
				return;
			g_renderer->renderPlayer (cass.x, cass.y, cass.dead, cass.won, alpha);
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

		bool can_input (Input input_code) const;
		Action *input (Input input_code) const;

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
					if (!cell->equals (other_cell)) return false;
				}
			}
			return true;
		}

		virtual State *get_transition (int i) const {
			Action *action = input ((Input)i);
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

		virtual void render_ghosts (Cass::State::Progress progress, const Cass::State *current) {
			if (progress == Cass::State::DEAD_END)
				return;
			render (progress == Cass::State::GOAL ? 1.0f : 0.25f, (State *)current);
		}
	};


}

#endif