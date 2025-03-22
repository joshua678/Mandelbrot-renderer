#pragma once
#include <CL/cl.h>
#include <SDL/SDL_ttf.h>
#include <unordered_map>
#include "fractals.h"
#include "globals.h"

bool lockColourScheme = false;
unordered_map<SDL_Keycode, bool> activatedKeyCodesMap;
int pressedKeys = 0;
bool leftMouseButtonHeld = false;
array<int, 2> mousePos = { 0, 0 };

bool handleInput(fractal& activeFractal, mandelbrotSet& mandelbrot, juliaSet& julia) { //returns true if program quit requested
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            bool keyState = (event.type == SDL_KEYDOWN);
            SDL_Keycode keyCode = event.key.keysym.sym;
            if (activatedKeyCodesMap[keyCode] != keyState) { keyState == 1 ? pressedKeys++ : pressedKeys--; }
            activatedKeyCodesMap[keyCode] = keyState;
        }
        else if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
            && event.button.button == SDL_BUTTON_LEFT) {
            leftMouseButtonHeld = (event.type == SDL_MOUSEBUTTONDOWN);
        }
        else if (event.type == SDL_QUIT) { return true; }
    }
    if (activatedKeyCodesMap[SDLK_w]) { activeFractal.position[1] -= activeFractal.moveSpeed * activeFractal.zoom * deltaTime; }
    if (activatedKeyCodesMap[SDLK_a]) { activeFractal.position[0] -= activeFractal.moveSpeed * activeFractal.zoom * deltaTime; }
    if (activatedKeyCodesMap[SDLK_s]) { activeFractal.position[1] += activeFractal.moveSpeed * activeFractal.zoom * deltaTime; }
    if (activatedKeyCodesMap[SDLK_d]) { activeFractal.position[0] += activeFractal.moveSpeed * activeFractal.zoom * deltaTime; }
    if (activatedKeyCodesMap[SDLK_e]) { activeFractal.zoom /= (1 + activeFractal.zoomSpeed * deltaTime); }
    if (activatedKeyCodesMap[SDLK_q]) { activeFractal.zoom *= (1 + activeFractal.zoomSpeed * deltaTime); }
    if (activatedKeyCodesMap[SDLK_DOWN] && activeFractal.maxIterations > activeFractal.maxIterationsFloor) { activeFractal.maxIterations /= (1 + 0.5 * deltaTime); }
    if (activatedKeyCodesMap[SDLK_UP]) { activeFractal.maxIterations = activeFractal.maxIterations * (1 + 0.5 * deltaTime) + 1; }
    if (activatedKeyCodesMap[SDLK_LSHIFT]) {
        activeFractal.zoomSpeed = 1.2;
        activeFractal.moveSpeed = 0.45;
    }
    else {
        activeFractal.zoomSpeed = 0.4;
        activeFractal.moveSpeed = 0.15;
    }
    if (activatedKeyCodesMap[SDLK_SPACE] && !lockColourScheme) {
        activeFractal.colouringScheme = (activeFractal.colouringScheme + 1) % 2;
        julia.framesToUpdate = 4;
        mandelbrot.framesToUpdate = 4;
        lockColourScheme = true;
    }
    else { lockColourScheme = false; }

    if (pressedKeys > 0) { activeFractal.framesToUpdate = 4; }

    if (leftMouseButtonHeld && activeFractal.type == "mandelbrotSet") {
        julia.framesToUpdate = 4;
        array<int, 2> newMouseState = { 0,0 };
        SDL_GetMouseState(&newMouseState[0], &newMouseState[1]);
        if (newMouseState[0] <= mandelbrot.width + mandelbrotGap
            && newMouseState[1] <= mandelbrot.height + mandelbrotGap
            && newMouseState[0] >= mandelbrotGap
            && newMouseState[1] >= mandelbrotGap) {
            mousePos = newMouseState;
        }

        julia.index[0] = ((double)(mousePos[0] - mandelbrotGap) / mandelbrot.width - 0.5) * mandelbrot.zoom * ((double)mandelbrot.width / mandelbrot.height) + mandelbrot.position[0];
        julia.index[1] = ((double)(mousePos[1] - mandelbrotGap) / mandelbrot.height - 0.5) * mandelbrot.zoom + mandelbrot.position[1];

        julia.position[0] = 0;
        julia.position[1] = 0;
        julia.zoom = 3;
    }
    return false;
}