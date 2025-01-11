#include "SDL_keycode.h"
#include <SDL.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fftw3.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define WAVEFORM_HEIGHT 200
#define WAVEFORM_DRAW_SCALE 1.0f
#define SAMPLE_SIZE 2048
#define FFT_HEIGHT 200
#define FFT_DRAW_SCALE 100.0f

typedef struct AudioData
{
	float waveformData[SAMPLE_SIZE];
	double fftData[SAMPLE_SIZE/2];  // Store FFT magnitude data
	SDL_mutex* mutex;
	fftw_plan fftPlan;
	double* fftIn;
	fftw_complex* fftOut;
} AudioData;

void AudioCallback(void* userdata, Uint8* stream, int len)
{
	AudioData* audio = (AudioData*)userdata;
	SDL_LockMutex(audio->mutex);

	Sint16* samples = (Sint16*)stream;
	int sampleCount = len / 2; // 16-bit samples

	for (int i = 0; i < SAMPLE_SIZE && i < sampleCount; i++)
	{
		// NOTE: Normalize to [-1, 1] range
		audio->waveformData[i] = samples[i] / 32768.0f;
		audio->fftIn[i] = audio->waveformData[i];  // Copy to FFT input buffer
	}

	// Perform FFT
	fftw_execute(audio->fftPlan);

	// Calculate magnitude of complex FFT results
	for (int i = 0; i < SAMPLE_SIZE/2; i++)
	{
		double real = audio->fftOut[i][0];
		double imag = audio->fftOut[i][1];
		audio->fftData[i] = sqrt(real*real + imag*imag) / SAMPLE_SIZE;
	}

	SDL_UnlockMutex(audio->mutex);
}

void DrawFFT(SDL_Renderer* renderer, AudioData* audio)
{
	SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);  // Red for FFT

	float xScale = (float)SCREEN_WIDTH / (SAMPLE_SIZE/4);	// NOTE: Only show first quarter for better visibility
	int baseY = SCREEN_HEIGHT - 50;							// NOTE: Position at bottom of screen

	for (int i = 0; i < SAMPLE_SIZE/4; i++)
	{
		int x = i * xScale;
		int height = (int)(audio->fftData[i] * FFT_HEIGHT * FFT_DRAW_SCALE);  // Scale factor for visibility
		height = (height > FFT_HEIGHT) ? FFT_HEIGHT : height;
		SDL_RenderDrawLine(renderer, x, baseY, x, baseY - height);
	}
}

int main(int argc, char* argv[])
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
	{
		printf("SDL_Init Error: %s\n", SDL_GetError());
		return -1;
	}

	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
	{
		printf("Mix_OpenAudio Error: %s\n", Mix_GetError());
		SDL_Quit();
		return -1;
	}

	SDL_Window* window = SDL_CreateWindow(
		"SDL2 Waveform Visualizer",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		SCREEN_WIDTH, SCREEN_HEIGHT,
		SDL_WINDOW_SHOWN
	);

	if (!window)
	{
		printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
		Mix_CloseAudio();
		SDL_Quit();
		return -1;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	// Initialize AudioData with FFT
	AudioData audioData;
	audioData.mutex = SDL_CreateMutex();

	// Initialize FFTW
	audioData.fftIn = (double*)fftw_malloc(sizeof(double) * SAMPLE_SIZE);
	audioData.fftOut = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (SAMPLE_SIZE/2 + 1));
	audioData.fftPlan = fftw_plan_dft_r2c_1d(SAMPLE_SIZE, audioData.fftIn, audioData.fftOut, FFTW_MEASURE);

	Mix_SetPostMix(AudioCallback, &audioData);

	SDL_Event event;
	int running = 1;
	Mix_Music* currentMusic = NULL;
	int isPaused = 0;

	while (running)
	{
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				running = 0;
			else if (event.type == SDL_KEYDOWN)
			{
				if (event.key.keysym.sym == SDLK_SPACE)
				{
					if (currentMusic)
					{
						if (isPaused)
						{
							Mix_ResumeMusic();
							isPaused = 0;
						}
						else
						{
							Mix_PauseMusic();
							isPaused = 1;
						}
					}
				}
				else if (event.key.keysym.sym == SDLK_q)
					exit(0);
			}
			else if (event.type == SDL_DROPFILE)
			{
				char* droppedFile = event.drop.file;
				printf("File dropped: %s\n", droppedFile);

				if (currentMusic)
				{
					Mix_HaltMusic();
					Mix_FreeMusic(currentMusic);
				}

				currentMusic = Mix_LoadMUS(droppedFile);
				if (!currentMusic)
					printf("Failed to load music file: %s\n", Mix_GetError());
				else
				{
					printf("Playing music file: %s\n", droppedFile);
					if (Mix_PlayMusic(currentMusic, -1) == -1)
						printf("Mix_PlayMusic Error: %s\n", Mix_GetError());
					isPaused = 0;
				}

				SDL_free(droppedFile);
			}
		}

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		SDL_LockMutex(audioData.mutex);

		// NOTE: Draw Waveform -- should be a function
		SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
		float xScale = (float)SCREEN_WIDTH / SAMPLE_SIZE;
		int centerY = SCREEN_HEIGHT / 2;

		for (int i = 0; i < SAMPLE_SIZE - 1; i++)
		{
			int x1 = i * xScale;
			int x2 = (i + 1) * xScale;
			int y1 = centerY + (audioData.waveformData[i] * WAVEFORM_HEIGHT * WAVEFORM_DRAW_SCALE);
			int y2 = centerY + (audioData.waveformData[i + 1] * WAVEFORM_HEIGHT * WAVEFORM_DRAW_SCALE);
			SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
		}

		// Draw FFT
		DrawFFT(renderer, &audioData);

		SDL_UnlockMutex(audioData.mutex);

		SDL_RenderPresent(renderer);
	}

	// Cleanup
	if (currentMusic)
	{
		Mix_HaltMusic();
		Mix_FreeMusic(currentMusic);
	}

	fftw_destroy_plan(audioData.fftPlan);
	fftw_free(audioData.fftIn);
	fftw_free(audioData.fftOut);
	SDL_DestroyMutex(audioData.mutex);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	Mix_CloseAudio();
	SDL_Quit();

	return 0;
}
