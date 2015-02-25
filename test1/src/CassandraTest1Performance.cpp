#include "Game1.h"
#include <stdio.h>
#include <time.h>

#ifdef _WIN32

# include <Windows.h>
# include <psapi.h>

#pragma warning( disable : 4129 ) // 'E' unrecognized character escape sequence

unsigned int used_memory () {
	PROCESS_MEMORY_COUNTERS counter;
	GetProcessMemoryInfo (GetCurrentProcess (), &counter, sizeof (counter));
	return (unsigned int)counter.PeakWorkingSetSize;
}

#else

# include <unistd.h>

unsigned int used_memory () {
	size_t size = 0;
    FILE *file = fopen("/proc/self/statm", "r");
    if (file) {
        unsigned long vm = 0;
        fscanf (file, "%ld", &vm);
        fclose (file);
       size = (size_t)vm * getpagesize();
    }
    return size;
}

#endif

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
	clock_t time;
    Renderer renderer;
	Game1::g_renderer = &renderer;
	
	Game1::State *current_state = Game1::load_state ("test1-map.txt");
	if (!current_state)
		return -1;
	Cass::Solver *solver = current_state->get_solver ();
	solver->add_start_point (current_state);
	time = clock ();
	int i = 0;
	while (!solver->done ()) {
		solver->process ();
		i++;
	}
	time = clock () - time;
#ifndef _WIN32
	printf ("\E[2J");
	current_state->render (1.f);
	printf ("\E[%d;%dH", current_state->get_map_size_y () + 2, 1);
#endif
	printf ("Map size is %dx%d\n", current_state->get_map_size_x (), current_state->get_map_size_y ());
	printf ("Processed %d nodes in %gms and used %gMB\n", i, 1000 * time / (float)CLOCKS_PER_SEC, used_memory () / (float)(1024 * 1024));

	time = clock ();
	solver->calc_view_state ();
	time = clock () - time;
	printf ("Solved view states in %gms\n", 1000 * time / (float)CLOCKS_PER_SEC);

	delete current_state;
	delete solver;

	if (argc > 1) {
		printf ("Press ENTER\n");
		getchar ();
	}

	return 0;
}