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

		int num_transitions;
		StateNode **transitions;
		State *state;
		StateNode *next_in_hash_bucket;
		StateNode *next_in_incomplete_list;
		int steps;

		// StateNode takes ownership of the state
		StateNode (State *state) : num_transitions (0), transitions (NULL), state (state),
			next_in_hash_bucket (NULL), next_in_incomplete_list (NULL), steps (0) {
			state->set_progress (State::IN_PROCESS);
		}

		~StateNode () {
			delete state;
			for (int i = 0; i < num_transitions; i++) {
				delete transitions[i];
			}
			if (transitions)
				delete[]transitions;
		}

		State::Progress calc_view_state (int new_steps) {
			// This node has already been processed
			if (steps < new_steps)
				return State::DEAD_END;

			steps = new_steps;

			if (num_transitions == 0)
				return State::IN_PROCESS;

			State::Progress ret = State::DEAD_END;
			for (int i = 0; i < num_transitions; i++) {
				StateNode *target = transitions[i];
				if (target) {
					State::Progress prog = target->calc_view_state (new_steps + 1);
					if (prog != State::DEAD_END) {
						ret = State::IN_PROCESS;
					}
				}
			}

			state->set_progress (ret);
			return ret;
		}

		int find_minimum_goal_distance (int new_steps) const {
			// This node has already been processed
			if (steps < new_steps)
				return MAX_STEPS;

			if (state->has_won ()) {
				return new_steps;
			}

			int min_dist = MAX_STEPS;
			for (int i = 0; i < num_transitions; i++) {
				StateNode *target = transitions[i];
				int dist = target->find_minimum_goal_distance (new_steps + 1);
				if (dist < min_dist) {
					min_dist = dist;
				}
			}
			return min_dist;;
		}

		bool calc_goal_path (int new_steps, int min_steps) {
			// This node has already been processed
			if (steps < new_steps)
				return false;

			if (state->has_won () && steps == min_steps) {
				// Back track to mark GOAL nodes
				state->set_progress (State::GOAL);
				return true;
			}

			for (int i = 0; i < num_transitions; i++) {
				StateNode *target = transitions[i];
				if (target->calc_goal_path (new_steps + 1, min_steps)) {
					state->set_progress (State::GOAL);
					return true;
				}
			}
			return false;
		}
	};


	Solver::Solver (int num_hash_buckets) : num_hash_buckets (num_hash_buckets),
		incomplete_head (NULL), incomplete_tail (NULL) {
		node_hash = new StateNode*[num_hash_buckets];
		memset (node_hash, 0, num_hash_buckets * sizeof (StateNode *));
	}

	Solver::~Solver () {
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

	// Create a StateNode wrapping the State, and add it both to the hash and the list
	// of incomplete nodes.
	void Solver::add_node (State *state) {
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
	}

	StateNode *Solver::find (State *state) {
		State::Hash hash = state->get_hash ();
		StateNode *tmp = node_hash[hash];
		while (tmp) {
			if (state->equals (tmp->state)) return tmp;
			tmp = tmp->next_in_hash_bucket;
		}
		return NULL;
	}

	void Solver::print () {
#if 0
		static const char state_names[4][3] = { "DE", "PR", "GO" };
		printf ("+");
		for (int x = 0; x < sizex; x++) {
			printf ("-----------------+");
		}
		printf ("\n");
		for (int y = 0; y < sizey; y++) {
			int max_height = 0;
			for (int x = 0; x < sizex; x++) {
				int height = 0;
				StateNode *node = get_at (x, y);
				while (node) {
					node = node->next_in_cell;
					height++;
				}
				if (height > max_height) max_height = height;
			}
			for (int n = 0; n < max_height; n++) {
				for (int i = 0; i < NUM_INPUTS + (n < max_height - 1 ? 1 : 0); i++) {
					printf ("|");
					for (int x = 0; x < sizex; x++) {
						StateNode *node = get_at (x, y);
						for (int nn = 0; nn < n; nn++) {
							if (node) node = node->next_in_cell;
						}
						if (!node) {
							printf ("                 ");
						} else {
							switch (i) {
							case 0:
								printf ("%08x>", ((int)node) & 0xFFFFFFFF);
								break;
							case 1:
								printf ("steps%3d ", node->steps);
								break;
							case 2:
								printf ("state %s ", state_names[node->view_state]);
								break;
							case 3:
								printf ("%4s%4s ", node->cache->cass.dead ? "Dead" : "", node->cache->cass.won ? "Won" : "");
								break;
							default:
								printf ("         ");
								break;
							}
							if (i >= NUM_INPUTS || !node->transition[i]) {
								printf ("        ");
							} else
								if (node->transition[i]->target) {
									printf ("%08x", ((int)node->transition[i]->target) & 0xFFFFFFFF);
								} else {
									printf ("........");
								}
						}
						printf ("|");
					}
					printf ("\n");
				}
			}
			printf ("+");
			for (int x = 0; x < sizex; x++) {
				printf ("-----------------+");
			}
			printf ("\n");
		}
#endif
	}

	void Solver::reset_view_state () {

		for (int i = 0; i < num_hash_buckets; i++) {
			StateNode *node = node_hash[i];
			while (node) {
				node->steps = StateNode::MAX_STEPS;
				node->state->set_progress (State::DEAD_END);
				node = node->next_in_hash_bucket;
			}
		}
	}

	// Process the node at the head of the incomplete nodes list, and add
	// More nodes to the tail if necessary.
	bool Solver::process () {
		StateNode *node = incomplete_head;
		node->num_transitions = node->state->num_transitions ();
		node->transitions = new StateNode*[node->num_transitions];
		for (int i = 0; i < node->num_transitions; i++) {
			State *target_state = node->state->get_transition (i);
			StateNode *target_node = NULL;
			StateNode *other_target = find (target_state);
			if (other_target) {
				delete target_state;
				target_node = other_target;
			} else {
				add_node (target_state);
				target_node = incomplete_tail;
			}

			node->transitions[i] = target_node;
		}

		incomplete_head = node->next_in_incomplete_list;
		node->next_in_incomplete_list = NULL;

		return incomplete_head == NULL;
	}

} // namespace Cass