#define _CRT_SECURE_NO_WARNINGS
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <memory.h>
#include "Game1.h"

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG

namespace Game1 {

	static const int dirs[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
	
	class StateImplementation;
	struct Cell;
	struct EmptyCell;
	struct WallCell;
	struct TrapCell;
	struct DoorCell;
	struct TriggerCell;
	struct PushableBlockCell;
	struct GoalCell;

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
		virtual void pass (StateImplementation *state, int incoming_dir) {};
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

	class StateImplementation : public State {
		Player cass;
		Map *map;

	public:
		StateImplementation (const char *filename);
		StateImplementation (int map_sizex, int map_sizey) {
			map = new Map (map_sizex, map_sizey);
		}
		~StateImplementation ();

		void render (float alpha) {
			render (alpha, NULL);
		}

		bool can_input (Input input_code) const;
		void input (Input input_code);

		Player *get_cass () { return &cass; }
		Map *get_map () { return map; }

		int get_map_size_x () { return map->get_sizex (); }
		int get_map_size_y () { return map->get_sizey (); }

		Cass::Solver *get_solver () {
			return Cass::get_solver (get_map_size_x () * get_map_size_y (), NUM_INPUTS);
		}

	private:
		void render (float alpha, const StateImplementation *current = NULL) {
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

		//
		// Cassandra Interface
		//
		virtual bool equals (const Cass::State *virt_other) const {
			const StateImplementation *other = (const StateImplementation *)virt_other;
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

		StateImplementation *clone () const {
			StateImplementation *new_state = new StateImplementation (map->get_sizex (), map->get_sizey ());
			for (int x = 0; x < map->get_sizex (); x++) {
				for (int y = 0; y < map->get_sizey (); y++) {
					new_state->map->set_cell (x, y, map->get_cell (x, y)->clone ());
				}
			}
			new_state->cass = cass;

			return new_state;
		}

		virtual Cass::State *get_transition (int i) const {
			if (!can_input ((Input)i))
				return NULL;

			StateImplementation *new_state = clone ();
			new_state->input ((Input)i);
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
			render (progress == Cass::State::GOAL ? 1.0f : 0.25f, (StateImplementation *)current);
		}
	};

	struct EmptyCell : Cell {
		EmptyCell (int x, int y) : Cell (x, y) {}
		virtual void render (float alpha) const { g_renderer->renderEmptyCell (x, y, alpha); }
		Cell *clone () const { return new EmptyCell (x, y); }
		virtual bool equals (const Cell *cell) const { return cell->equals (this); }
		virtual bool equals (const EmptyCell *cell) const { return true; }

		virtual bool can_pass (const Map *map, int incoming_dir) const { return true; }
	};

	struct WallCell : Cell {
		WallCell (int x, int y) : Cell (x, y) {}
		virtual void render (float alpha) const { g_renderer->renderWallCell (x, y, alpha); }
		Cell *clone () const { return new WallCell (x, y); }
		virtual bool equals (const Cell *cell) const { return cell->equals (this); }
		virtual bool equals (const WallCell *cell) const { return true; }

		virtual bool can_pass (const Map *map, int incoming_dir) const { return false; }
	};

	struct TrapCell : Cell {
		TrapCell (int x, int y) : Cell (x, y) {}
		virtual void render (float alpha) const { g_renderer->renderTrapCell (x, y, alpha); }
		Cell *clone () const { return new TrapCell (x, y); }
		virtual bool equals (const Cell *cell) const { return cell->equals (this); }
		virtual bool equals (const TrapCell *cell) const { return true; }

		virtual bool can_pass (const Map *map, int incoming_dir) const { return true; }
		virtual void pass (StateImplementation *state, int incoming_dir){
			state->get_cass()->dead = true;
		}

		virtual bool is_hole () const { return true; }
	};

	struct PushableBlockCell : Cell {
		Cell *block_below;

		PushableBlockCell (int x, int y, Cell *block_below) : Cell (x, y) {
			this->block_below = block_below;
		}

		~PushableBlockCell () {
			if (block_below)
				delete block_below;
		}

		virtual void render (float alpha) const { g_renderer->renderPushableBlockCell (x, y, alpha); }

		Cell *clone () const {
			return new PushableBlockCell (x, y, block_below->clone ());
		}

		virtual void toggle () {
			block_below->toggle ();
		}

		virtual bool equals (const Cell *cell) const { return cell->equals (this); }
		virtual bool equals (const PushableBlockCell *cell) const { return block_below->equals (cell->block_below); }

		virtual bool can_pass (const Map *map, int incoming_dir) const {
			int newx = x + dirs[incoming_dir][0];
			int newy = y + dirs[incoming_dir][1];
			return (map->get_cell (newx, newy)->can_pass (map, incoming_dir));
		}

		virtual void pass (StateImplementation *state, int incoming_dir) {
			int newx = x + dirs[incoming_dir][0];
			int newy = y + dirs[incoming_dir][1];

			if (state->get_map ()->get_cell (newx, newy)->is_hole ()) {
				delete state->get_map ()->get_cell (newx, newy);
				state->get_map ()->set_cell (x, y, new EmptyCell (x, y));
				state->get_map ()->set_cell (newx, newy, new EmptyCell (newx, newy));
				delete this;
				return;
			}

			Cell *new_below = state->get_map ()->get_cell (newx, newy);
			state->get_map ()->set_cell (newx, newy, this);
			state->get_map ()->set_cell (x, y, block_below);
			block_below = new_below;
			x = newx;
			y = newy;
		}
	};

	struct DoorCell : Cell {
		bool open;

		DoorCell (int x, int y, bool open) : Cell (x, y), open (open) {}
		virtual void render (float alpha) const { g_renderer->renderDoorCell (x, y, open, alpha); }
		Cell *clone () const { return new DoorCell (x, y, open); }
		virtual bool equals (const Cell *cell) const { return cell->equals (this); }
		virtual bool equals (const DoorCell *cell) const { return open == cell->open; }

		virtual bool can_pass (const Map *map, int incoming_dir) const { return open; }

		virtual void toggle () {
			open = !open;
		}
	};

	struct TriggerCell : Cell {
		int door_x, door_y;

		TriggerCell (int x, int y, int door_x, int door_y) : Cell (x, y), door_x (door_x), door_y (door_y) {}
		virtual void render (float alpha) const { g_renderer->renderTriggerCell (x, y, alpha); }
		Cell *clone () const { return new TriggerCell (x, y, door_x, door_y); }
		virtual bool equals (const Cell *cell) const { return cell->equals (this); }
		virtual bool equals (const TriggerCell *cell) const { return true; }

		virtual bool can_pass (const Map *map, int incoming_dir) const { return true; }
		virtual void pass (StateImplementation *state, int incoming_dir) {
			state->get_map ()->get_cell (door_x, door_y)->toggle ();
		}
	};

	struct GoalCell : Cell {
		GoalCell (int x, int y) : Cell (x, y) {}
		virtual void render (float alpha) const { g_renderer->renderGoalCell (x, y, alpha); }
		Cell *clone () const { return new GoalCell (x, y); }
		virtual bool equals (const Cell *cell) const { return cell->equals (this); }
		virtual bool equals (const GoalCell *cell) const { return true; }

		virtual bool can_pass (const Map *map, int incoming_dir) const { return true; }
		virtual void pass (StateImplementation *state, int incoming_dir) {
			state->get_cass ()->won = true;
		}
	};

	Map::~Map () {
		for (int x = 0; x < sizex; x++) {
			for (int y = 0; y < sizey; y++) {
				delete get_cell (x, y);
			}
		}
		delete cells;
	}

	StateImplementation::StateImplementation (const char *filename) {
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

	StateImplementation::~StateImplementation () {
		delete map;
	}

	bool StateImplementation::can_input (Input input_code) const {
		int dir = -1;
		switch (input_code) {
		case UP:
			dir = 0;
			break;
		case RIGHT:
			dir = 1;
			break;
		case DOWN:
			dir = 2;
			break;
		case LEFT:
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

	void StateImplementation::input (Input input_code) {
		int dir = -1;
		switch (input_code) {
		case UP:
			dir = 0;
			break;
		case RIGHT:
			dir = 1;
			break;
		case DOWN:
			dir = 2;
			break;
		case LEFT:
			dir = 3;
			break;
		}
		if (!cass.dead && !cass.won && dir > -1) {
			int new_x = cass.x + dirs[dir][0];
			int new_y = cass.y + dirs[dir][1];
			if (map->get_cell (new_x, new_y)->can_pass (map, dir)) {
				cass.x = new_x;
				cass.y = new_y;
				map->get_cell (new_x, new_y)->pass (this, dir);
			}
		}
	}

	State *load_state (const char *filename) {
		StateImplementation *state = new StateImplementation (filename);
		return state;
	}
}