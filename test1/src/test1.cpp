#include "Game1.h"
#include <stdio.h>

Game1::Renderer *Game1::g_renderer;

class Renderer : public Game1::Renderer {
public:
	void renderPlayer (int x, int y, bool dead, bool won, float alpha) { printf ("\E[%d;%dH@", y+1, x+1); }
	void renderEmptyCell (int x, int y, float alpha) { printf ("\E[%d;%dH ", y+1, x+1); }
	void renderWallCell (int x, int y, float alpha)  { printf ("\E[%d;%dH#", y+1, x+1); }
	void renderTrapCell (int x, int y, float alpha)  { printf ("\E[%d;%dH^", y+1, x+1); }
	void renderDoorCell (int x, int y, bool open, float alpha)  { printf ("\E[%d;%dH+", y+1, x+1); }
	void renderTriggerCell (int x, int y, float alpha)  { printf ("\E[%d;%dH.", y+1, x+1); }
	void renderPushableBlockCell (int x, int y, float alpha)  { printf ("\E[%d;%dH%%", y+1, x+1); }
	void renderGoalCell (int x, int y, float alpha)  { printf ("\E[%d;%dH0", y+1, x+1); }
};

int main (int argc, char *argv[]) {
    Renderer renderer;
	Game1::g_renderer = &renderer;
	
	Game1::State *current_state = Game1::load_state ("test1-map.txt");
	printf ("\E[2J");
	Cass::Solver *solver = current_state->get_solver ();
	solver->add_start_point (current_state);
	int i = 0;
	while (!solver->done ()) {
		solver->process ();
		i++;
	}
	current_state->render (1.f);
	printf ("\E[%d;%dHMap size is %dx%d\n", current_state->get_map_size_y () + 2, 1, current_state->get_map_size_x (), current_state->get_map_size_y ());
	printf ("Processed %d nodes\n", i);
	delete current_state;
	delete solver;
}