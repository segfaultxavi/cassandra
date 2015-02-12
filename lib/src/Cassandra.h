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
		virtual int num_transitions () const = 0;
		virtual State *get_transition (int i) const = 0;
		virtual Hash get_hash () const = 0;
		virtual bool has_won () const = 0;

		virtual Progress get_progress () const = 0;
		virtual void set_progress (Progress progress) = 0;
	};

	// Private struct
	struct StateNode;

	// Applications use this to obtain solutions.
	// Add an initial node with add_node() and call process() until it
	// returns true;
	class Solver {
	public:
		Solver (int num_hash_buckets);
		~Solver ();

		void add_node (State *state);
		bool process ();
		void print ();

	private:
		int num_hash_buckets;
		StateNode **node_hash;
		StateNode *incomplete_head;
		StateNode *incomplete_tail;

		StateNode *find (State *state);
		void reset_view_state ();
	};
}

#endif