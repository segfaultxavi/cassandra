#ifndef __CASSANDRA_H__
#define __CASSANDRA_H__

namespace Cass {
	// Applications must implement this interface for their states.
	class State {
	public:
		enum Progress {
			DEAD_END,
			IN_PROCESS,
			GOAL
		};

		typedef int Hash;

		virtual ~State () {}
		virtual bool equals (const State *other) const = 0;
		virtual State *get_transition (int i) const = 0;
		virtual Hash get_hash () const = 0;
		virtual bool has_won () const = 0;
		virtual void render (Progress progress) = 0;
	};

	// Applications use this to obtain solutions.
	// Add an initial node with add_node() and call process() until it
	// returns true;
	class Solver {
	public:
		virtual ~Solver () {};

		virtual void add_start_point (State *state) = 0;
		virtual bool process () = 0;
		virtual bool done () = 0;
		virtual void update (int input) = 0;
		virtual void calc_view_state () = 0;
		virtual void render (int distance) = 0;
	};

	Solver *get_solver (int num_hash_buckets, int num_inputs);
}

#endif