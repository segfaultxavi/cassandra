#include <stdio.h>
#include "SDL.h"

int main(int argc, char *argv[]) {
	if (SDL_Init (SDL_INIT_EVERYTHING) != 0) {
		printf ("SDL Init error: %s\n", SDL_GetError());
		return 1;
	}

	SDL_Window *win = SDL_CreateWindow ("Cassandra Test 1", 0, 0, 1024, 768, SDL_WINDOW_SHOWN);
	if (win == NULL){
		printf ("SDL_CreateWindow Error: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	SDL_Renderer *renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == NULL){
		printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
		SDL_DestroyWindow(win);
		SDL_Quit();
		return 1;
	}

	SDL_RenderClear(renderer);
	SDL_SetRenderDrawColor(renderer, 255, 50, 50, SDL_ALPHA_OPAQUE);
	SDL_RenderFillRect(renderer, NULL);
	SDL_RenderPresent(renderer);

	getchar();

	return 0;
}