#define _CRT_SECURE_NO_WARNINGS
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <memory.h>
#include "Cassandra.h"

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG

namespace Cass {

	struct StateNode {
		static const int MAX_STEPS = 1000000;
		static int NUM_TRANSITIONS;

		StateNode **transitions;
		State *state;
		StateNode *next_in_hash_bucket;
		StateNode *next_in_incomplete_list;
		int steps;
		State::Progress progress;

		// StateNode takes ownership of the state
		StateNode (State *state) : state (state), transitions (NULL),
			next_in_hash_bucket (NULL), next_in_incomplete_list (NULL), steps (0) {
			progress = State::IN_PROCESS;
		}

		~StateNode () {
			delete state;
			if (transitions)
				delete[] transitions;
		}

		State::Progress calc_view_state (int new_steps) {
			// This node has already been processed
			if (steps < new_steps)
				return State::DEAD_END;

			steps = new_steps;

			if (!transitions)
				return State::IN_PROCESS;

			progress = State::DEAD_END;
			for (int i = 0; i < NUM_TRANSITIONS; i++) {
				StateNode *target = transitions[i];
				if (target) {
					State::Progress prog = target->calc_view_state (new_steps + 1);
					if (prog != State::DEAD_END) {
						progress = State::IN_PROCESS;
					}
				}
			}

			// Regardless of anything else
			if (state->has_won ())
				return State::IN_PROCESS;

			return progress;
		}

		int find_minimum_goal_distance (int new_steps) const {
			// This node has already been processed
			if (steps < new_steps || !transitions)
				return MAX_STEPS;

			if (state->has_won ()) {
				return new_steps;
			}

			int min_dist = MAX_STEPS;
			for (int i = 0; i < NUM_TRANSITIONS; i++) {
				StateNode *target = transitions[i];
				if (target) {
					int dist = target->find_minimum_goal_distance (new_steps + 1);
					if (dist < min_dist) {
						min_dist = dist;
					}
				}
			}
			return min_dist;;
		}

		bool calc_goal_path (int new_steps, int min_steps) {
			// This node has already been processed
			if (steps < new_steps || !transitions)
				return false;

			if (state->has_won () && steps == min_steps) {
				// Back track to mark GOAL nodes
				progress = State::GOAL;
				return true;
			}

			for (int i = 0; i < NUM_TRANSITIONS; i++) {
				StateNode *target = transitions[i];
				if (target) {
					if (target->calc_goal_path (new_steps + 1, min_steps)) {
						progress = State::GOAL;
						return true;
					}
				}
			}
			return false;
		}

		void render_ghosts (int steps, int max_steps, const StateNode *current) {
			if (steps > this->steps)
				return;

			if (steps == max_steps) {
				state->render_ghosts (progress, current->state);
				return;
			}

			if (!transitions)
				return;

			for (int i = 0; i < NUM_TRANSITIONS; i++) {
				StateNode *target = transitions[i];
				if (target) {
					target->render_ghosts (steps + 1, max_steps, current);
				}
			}
		}
	};

	int StateNode::NUM_TRANSITIONS;

	class SolverImplementation : public Solver {
	private:
		int num_hash_buckets;
		int num_transitions;
		StateNode **node_hash;
		StateNode *incomplete_head;
		StateNode *incomplete_tail;

		StateNode *current_node;

		StateNode *find (State *state) {
			State::Hash hash = state->get_hash ();
			StateNode *tmp = node_hash[hash];
			while (tmp) {
				if (state->equals (tmp->state)) return tmp;
				tmp = tmp->next_in_hash_bucket;
			}
			return NULL;
		}

		void reset_view_state () {
			for (int i = 0; i < num_hash_buckets; i++) {
				StateNode *node = node_hash[i];
				while (node) {
					node->steps = StateNode::MAX_STEPS;
					node->progress = State::DEAD_END;
					node = node->next_in_hash_bucket;
				}
			}
		}

		// Create a StateNode wrapping the State, and add it both to the hash and the list
		// of incomplete nodes.
		StateNode *add_node (State *state) {
			StateNode *node = new StateNode (state);
			State::Hash hash = state->get_hash ();
			StateNode *tmp = node_hash[hash], *prv = NULL;
			while (tmp) {
				prv = tmp;
				tmp = tmp->next_in_hash_bucket;
			}
			if (!prv)
				node_hash[hash] = node;
			else
				prv->next_in_hash_bucket = node;

			if (!incomplete_head) {
				incomplete_head = incomplete_tail = node;
			} else {
				incomplete_tail->next_in_incomplete_list = node;
				incomplete_tail = node;
			}

			return node;
		}

	public:
		SolverImplementation (int num_hash_buckets, int num_transitions) :
				num_hash_buckets (num_hash_buckets), num_transitions (num_transitions),
				incomplete_head (NULL), incomplete_tail (NULL) {
			node_hash = new StateNode*[num_hash_buckets];
			memset (node_hash, 0, num_hash_buckets * sizeof (StateNode *));
		}

		~SolverImplementation () {
			for (int i = 0; i < num_hash_buckets; i++) {
				StateNode *node = node_hash[i];
				while (node) {
					StateNode *tmp = node;
					node = node->next_in_hash_bucket;
					delete tmp;
				}
			}
			delete node_hash;
		}

		void add_start_point (State *state) {
			current_node = add_node (state->clone ());
		}

		// Process the node at the head of the incomplete nodes list, and add
		// More nodes to the tail if necessary.
		bool process () {
			StateNode *node = incomplete_head;
			node->transitions = new StateNode*[num_transitions];
			memset (node->transitions, 0, num_transitions * sizeof (StateNode*));
			for (int i = 0; i < num_transitions; i++) {
				State *target_state = node->state->get_transition (i);
				if (!target_state)
					continue;

				StateNode *other_target = find (target_state);
				if (other_target) {
					delete target_state;
					node->transitions[i] = other_target;
				} else {
					node->transitions[i] = add_node (target_state);
				}
			}

			incomplete_head = incomplete_head->next_in_incomplete_list;
			node->next_in_incomplete_list = NULL;

			return done ();
		}

		bool done () {
			return incomplete_head == NULL;
		}

		void update (int input) {
			// FIXME When this transition has not been calculated yet
			if (current_node->transitions) {
				current_node = current_node->transitions[input];
			} else {
				printf ("Unprocessed transition!\n");
			}
		}

		void calc_view_state () {
			reset_view_state ();
			current_node->calc_view_state (0);
			int dist = current_node->find_minimum_goal_distance (0);
			current_node->calc_goal_path (0, dist);
		}

		void render (int distance) {
			current_node->render_ghosts (0, distance, current_node);
		}
	};


	Solver *get_solver (int num_hash_buckets, int num_transitions) {
		StateNode::NUM_TRANSITIONS = num_transitions;
		return new SolverImplementation (num_hash_buckets, num_transitions);
	}

} // namespace Cass