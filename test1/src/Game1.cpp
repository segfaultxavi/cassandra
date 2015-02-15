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
	
	struct EmptyCell : Cell {
		EmptyCell (int x, int y) : Cell (x, y) {}
		TileType type () const { return TileType::EMPTY; }
		Cell *clone () const { return new EmptyCell (x, y); }
		virtual bool equals (const Cell *cell) const { return cell->equals (this); }
		virtual bool equals (const EmptyCell *cell) const { return true; }

		virtual bool can_pass (const Map *map, int incoming_dir) const { return true; }
	};

	struct WallCell : Cell {
		WallCell (int x, int y) : Cell (x, y) {}
		TileType type () const { return TileType::WALL; }
		Cell *clone () const { return new WallCell (x, y); }
		virtual bool equals (const Cell *cell) const { return cell->equals (this); }
		virtual bool equals (const WallCell *cell) const { return true; }

		virtual bool can_pass (const Map *map, int incoming_dir) const { return false; }
	};

	struct TrapCell : Cell {
		TrapCell (int x, int y) : Cell (x, y) {}
		TileType type () const { return TileType::TRAP; }
		Cell *clone () const { return new TrapCell (x, y); }
		virtual bool equals (const Cell *cell) const { return cell->equals (this); }
		virtual bool equals (const TrapCell *cell) const { return true; }

		virtual bool can_pass (const Map *map, int incoming_dir) const { return true; }
		virtual Action *pass (Map *map, int incoming_dir);

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

		TileType type () const { return TileType::PUSHABLE_BLOCK; }

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

		virtual Action *pass (Map *map, int incoming_dir);
	};

	struct DoorCell : Cell {
		bool open;

		DoorCell (int x, int y, bool open) : Cell (x, y), open (open) {}
		TileType type () const { return open ? TileType::DOOR_OPEN : TileType::DOOR_CLOSED; }
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
		TileType type () const { return TileType::TRIGGER; }
		Cell *clone () const { return new TriggerCell (x, y, door_x, door_y); }
		virtual bool equals (const Cell *cell) const { return cell->equals (this); }
		virtual bool equals (const TriggerCell *cell) const { return true; }

		virtual bool can_pass (const Map *map, int incoming_dir) const { return true; }
		virtual Action *pass (Map *map, int incoming_dir);
	};

	struct GoalCell : Cell {
		GoalCell (int x, int y) : Cell (x, y) {}
		TileType type () const { return TileType::GOAL; }
		Cell *clone () const { return new GoalCell (x, y); }
		virtual bool equals (const Cell *cell) const { return cell->equals (this); }
		virtual bool equals (const GoalCell *cell) const { return true; }

		virtual bool can_pass (const Map *map, int incoming_dir) const { return true; }
		virtual Action *pass (Map *map, int incoming_dir);
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
	bool State::can_input (Input input_code) const {
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

	Action *State::input (Input input_code) const {
		int dir = -1;
		Action *action = NULL;
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
				action = new CassAction (dirs[dir][0], dirs[dir][1], false, false);
				action->add (map->get_cell (new_x, new_y)->pass (map, dir));
			}
		}

		return action;
	}




}