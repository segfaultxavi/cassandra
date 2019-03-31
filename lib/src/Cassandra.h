#ifndef __CASSANDRA_H__
#define __CASSANDRA_H__

namespace Cass {
	// Applications must implement this interface for their states.
	class State {
	public:
		// Calculation progress
		enum Progress {
			DEAD_END,    // This state leads nowhere
			IN_PROCESS,  // This state might lead somewhere (calculation ongoing)
			GOAL         // This state leads to the goal
		};

		// States must implement this hash, for faster access
		typedef int Hash;

		virtual ~State () {}
		// Check if two states are equivalent
		virtual bool equals (const State *other) const = 0;
		// Create a deep copy of a state
		virtual State *clone () const = 0;
		// Get the new state after taking transition i. This always creates a new state.
		virtual State *get_transition (int i) const = 0;
		// Get the hash for this state
		virtual Hash get_hash () const = 0;
		// Is this a goal state?
		virtual bool has_won () const = 0;
		// Render this state given its progress and the current (present) state
		virtual void render_ghosts (Progress progress, const State *current) = 0;
	};

	// Applications use this to obtain solutions.
	// Add an initial node with add_node() and call process() until it
	// returns true;
	class Solver {
	public:
		virtual ~Solver () {};

		// Add the starting point
		virtual void add_start_point (State *state) = 0;
		// Process one more state. Call again if it returns false.
		virtual bool process () = 0;
		// Call this to know if calculation has finished
		virtual bool done () = 0;
		// Change the current state
		virtual void update (int input) = 0;
		// Calculate the progress of all currently known nodes
		virtual void calc_view_state () = 0;
		// Render all nodes at the given distance
		virtual void render (int distance) = 0;
	};

	Solver *get_full_solver (int num_hash_buckets, int num_inputs);
}

#endif