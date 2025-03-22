#include <iostream>
#include <SDL/SDL.h>
#include <chrono>
#include <Windows.h>
#include <ppl.h>
#include <vector>
#include <sstream>
#include <cmath>
#include <CL/cl.h>
#include <array>
#include <omp.h>
#include <fstream>
#include <SDL/SDL_ttf.h>
#include "fractals.h"
#include <unordered_map>
#include "globals.h"
#include "input.h"
#include "handle errors.h"

#define MAX_SOURCE_SIZE (0x100000)

using namespace std;
using namespace concurrency;

const bool debugFrameTime = 1;
const bool fullscreen = 1;
bool FPSCounter = 1;
bool shouldRenderJuliaSet = 1;
int mandelbrotGap = 15; //in pixels
int frameRateCap = 0; //set to 0 for native refresh rate
//set to 0 to get native resolution
int screenWidth = 0;
int screenHeight = 0;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;

std::chrono::time_point<std::chrono::high_resolution_clock> frameStart;
std::chrono::time_point<std::chrono::high_resolution_clock> frameEnd;
std::chrono::time_point<std::chrono::high_resolution_clock> p1;
std::chrono::time_point<std::chrono::high_resolution_clock> p2;
int frameCounter = 0;
int fps = 0;
double timer = 0;
double timerPoint = 0;
int frameCounterPoint = 0;
double frameStall = 0;

struct text {
    TTF_Font* font;
    SDL_Color colour = { 0, 0, 0, 255 };
    string textStr;
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_Rect rect;
    int x, y;

    text(string newText, int newX, int newY, int newSize)
    : textStr(newText), x(newX), y(newY) {
        font = TTF_OpenFont("Resources/Arial.ttf", newSize);
        surface = TTF_RenderText_Solid(font, textStr.c_str(), colour);
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        rect = { x, y, surface->w, surface->h };
    }

    ~text() {
        if (texture) {
            SDL_DestroyTexture(texture);
        }
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (font) {
            TTF_CloseFont(font);
        }
    }

    void setText(string newText) {
        textStr = newText;
        if (texture) {
            SDL_DestroyTexture(texture);
        }
        if (surface) {
            SDL_FreeSurface(surface);
        }
        surface = TTF_RenderText_Solid(font, textStr.c_str(), colour);
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        rect = { x, y, surface->w, surface->h };
    }
};

array<int, 3> palette(double pos, double size) {
    return { (int)round(127.5 * sin(2 * M_PI * pos) + 127.5), 
        (int)round(127.5 * sin(2 * M_PI * pos + (2.0/3.0)*M_PI) + 127.5), 
        (int)round(127.5 * sin(2 * M_PI * pos + (4.0 / 3.0) * M_PI) + 127.5) };
}

void swapClMemObjects(cl_mem& mem1, cl_mem& mem2) {
    cl_mem temp = mem1;
    mem1 = mem2;
    mem2 = temp;
}

std::string loadKernelSource(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void swapFractalSizes(juliaSet &julia, mandelbrotSet &mandelbrot, cl_context context, cl_device_id device) {
    array<int, 2> temp = { mandelbrot.width, mandelbrot.height };
    mandelbrot.resize(julia.width, julia.height, renderer, window, context, device);
    mandelbrot.rect = { mandelbrotGap, mandelbrotGap, mandelbrot.surface->w, mandelbrot.surface->h };
    julia.resize(temp[0], temp[1], renderer, window, context, device);
    julia.rect = { screenWidth - julia.width - mandelbrotGap, mandelbrotGap, julia.surface->w, julia.surface->h };
    julia.framesToUpdate = 4;
    mandelbrot.framesToUpdate = 4;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    std::string mandelbrotKernelSource = loadKernelSource("Mandelbrot Kernel.cl");
    std::string juliaKernelSource = loadKernelSource("Julia Kernel.cl");
    const char* mandelbrotSourceStr = mandelbrotKernelSource.c_str();
    const char* juliaSourceStr = juliaKernelSource.c_str();

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();

        return 0;
    }
    if (TTF_Init() == -1) {
        DBOUT("SDL_ttf could not initialize! TTF_Error: " << TTF_GetError() << std::endl);
        return 0;
    }
    SDL_DisplayMode DM;
    if (SDL_GetDesktopDisplayMode(0, &DM) != 0) {
        // Handle error
        SDL_Log("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
        return 1;
    }
    if (screenWidth == 0) {
        screenWidth = DM.w;
        DBOUT(screenWidth);
    }
    if (screenHeight == 0) {
        screenHeight = DM.h;
    }
    if (frameRateCap == 0) {
        frameRateCap = DM.refresh_rate;
    }
    if (fullscreen) {
        window = SDL_CreateWindow("Mandelbrot :)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    else {
        window = SDL_CreateWindow("Mandelbrot :)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight, SDL_WINDOW_SHOWN);
    }
    if (window == NULL)
    {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();

        return 0;
    }
    DBOUT(endl << endl << DM.format << endl << endl);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL)
    {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();

        return 0;
    }




    // OpenCL initialization with platform and device selection
    cl_int err;
    cl_uint num_platforms = 0;
    err = clGetPlatformIDs(0, NULL, &num_platforms);
    if (err != CL_SUCCESS || num_platforms == 0) {
        std::cerr << "Error: Failed to find any OpenCL platforms!" << std::endl;
        exit(1);
    }

    cl_platform_id* platforms = new cl_platform_id[num_platforms];
    err = clGetPlatformIDs(num_platforms, platforms, NULL);
    if (err != CL_SUCCESS) {
        std::cerr << "Error: Failed to get OpenCL platform IDs!" << std::endl;
        exit(1);
    }

    // Select the first available platform (AMD, NVIDIA, etc.)
    cl_platform_id selectedPlatform = NULL;
    char platformName[128];
    for (cl_uint i = 0; i < num_platforms; ++i) {
        err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, sizeof(platformName), platformName, NULL);
        if (err != CL_SUCCESS) {
            std::cerr << "Error: Failed to get platform name!" << std::endl;
            exit(1);
        }
        selectedPlatform = platforms[i]; // Pick the first available platform
        break;
    }

    if (selectedPlatform == NULL) {
        std::cerr << "Error: No suitable OpenCL platform found!" << std::endl;
        exit(1);
    }

    // Get devices from the selected platform
    cl_uint num_devices = 0;
    err = clGetDeviceIDs(selectedPlatform, CL_DEVICE_TYPE_GPU, 0, NULL, &num_devices);
    if (err != CL_SUCCESS || num_devices == 0) {
        std::cerr << "Error: Failed to find GPU devices on the selected platform!" << std::endl;
        exit(1);
    }

    cl_device_id* devices = new cl_device_id[num_devices];
    err = clGetDeviceIDs(selectedPlatform, CL_DEVICE_TYPE_GPU, num_devices, devices, NULL);
    if (err != CL_SUCCESS) {
        std::cerr << "Error: Failed to get device IDs from the selected platform!" << std::endl;
        exit(1);
    }

    cl_device_id device = devices[0];

    char deviceName[128];
    err = clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(deviceName), deviceName, NULL);
    if (err != CL_SUCCESS) {
        std::cerr << "Error: Failed to get device name!" << std::endl;
        exit(1);
    }
    std::cout << "Using device: " << deviceName << std::endl;





    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);

    mandelbrotSet mandelbrot(screenWidth * 0.7, screenHeight - mandelbrotGap * 2, renderer, window, context, device);
    mandelbrot.position[0] = -0.7;
    juliaSet julia(screenWidth - mandelbrot.width - mandelbrotGap * 3, screenWidth - mandelbrot.width - mandelbrotGap * 3, renderer, window, context, device);

    cl_program mandelbrotProgram = clCreateProgramWithSource(context, 1, &mandelbrotSourceStr, NULL, &err);

    if (err != CL_SUCCESS) {
        DBOUT("Error: Failed to create Mandelbrot OpenCL program! " << getErrorString(err) << std::endl)
        exit(1);
    }

    err = clBuildProgram(mandelbrotProgram, 1, &device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        // Get the build log
        size_t logSize;
        clGetProgramBuildInfo(mandelbrotProgram, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
        char* buildLog = new char[logSize + 1];
        clGetProgramBuildInfo(mandelbrotProgram, device, CL_PROGRAM_BUILD_LOG, logSize, buildLog, NULL);
        buildLog[logSize] = '\0';
        DBOUT("Error in Mandelbrot kernel:\n" << buildLog << std::endl)
        delete[] buildLog;
        exit(1);
    }

    clBuildProgram(mandelbrotProgram, 1, &device, NULL, NULL, NULL);

    cl_program juliaProgram = clCreateProgramWithSource(context, 1, &juliaSourceStr, NULL, &err);
    if (err != CL_SUCCESS) {
        DBOUT("Error: Failed to create Julia OpenCL program! " << getErrorString(err) << std::endl)
        exit(1);
    }

    err = clBuildProgram(juliaProgram, 1, &device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        // Get the build log
        size_t logSize;
        clGetProgramBuildInfo(juliaProgram, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
        char* buildLog = new char[logSize + 1];
        clGetProgramBuildInfo(juliaProgram, device, CL_PROGRAM_BUILD_LOG, logSize, buildLog, NULL);
        buildLog[logSize] = '\0';
        DBOUT("Error in Julia kernel:\n" << buildLog << std::endl)
        delete[] buildLog;
        exit(1);
    }

    clBuildProgram(juliaProgram, 1, &device, NULL, NULL, NULL);

    char* buildLog = new char[16384];
    size_t buildLogSize;
    err = clGetProgramBuildInfo(mandelbrotProgram, device, CL_PROGRAM_BUILD_LOG, sizeof(buildLog), buildLog, &buildLogSize);
    if (err != CL_SUCCESS) {
        std::cerr << "\n\nError: Failed to retrieve build log!\n\n" << std::endl;
        exit(1);
    }
    DBOUT("\n\nmandelbrot build log:\n" << buildLog << "\n\n")

    err = clGetProgramBuildInfo(juliaProgram, device, CL_PROGRAM_BUILD_LOG, sizeof(buildLog), buildLog, &buildLogSize);
    if (err != CL_SUCCESS) {
        std::cerr << "\n\nError: Failed to retrieve build log!\n\n" << std::endl;
        exit(1);
    }
    DBOUT("\n\njulia build log:\n" << buildLog << "\n\n")

    cl_kernel mandelbrotKernel = clCreateKernel(mandelbrotProgram, "mandelbrotKernel", &err);
    cl_kernel juliaKernel = clCreateKernel(juliaProgram, "juliaKernel", &err);

    SDL_Texture* backgroundTexture = SDL_CreateTextureFromSurface(renderer, SDL_LoadBMP("Resources/background.bmp"));

    mandelbrot.rect = { mandelbrotGap, mandelbrotGap, mandelbrot.surface->w, mandelbrot.surface->h };
    julia.rect = { screenWidth - julia.width - mandelbrotGap, mandelbrotGap, julia.surface->w, julia.surface->h };

    array<int, 2> mousePos = { 0, 0 };
    string activeFractal = "mandelbrot";

    text fpsText("FPS: ", 5, 0, 14);
    fpsText.colour = { 255, 255, 255, 255 };
    text mandelbrotIterationText("Mandelbrot iterations: ", 100, 0, 14);
    mandelbrotIterationText.colour = { 255, 255, 255, 255 };
    text juliaIterationText("Julia iterations: ", 300, 0, 14);
    juliaIterationText.colour = { 255, 255, 255, 255 };



    // Main render loop
    bool quit = false;
    while (!quit)
    {
        frameStart = std::chrono::high_resolution_clock::now();
        SDL_GetMouseState(&mousePos[0], &mousePos[1]);

        if (mousePos[0] >= mandelbrot.rect.x && mousePos[0] <= mandelbrot.width + mandelbrot.rect.x && mousePos[1] >= mandelbrot.rect.y && mousePos[1] <= mandelbrot.height + mandelbrot.rect.y && mandelbrot.width < julia.width) {
            activeFractal = "mandelbrot";
            swapFractalSizes(julia, mandelbrot, context, device);
        }
        if (mousePos[0] >= julia.rect.x && mousePos[0] <= julia.width + julia.rect.x && mousePos[1] >= julia.rect.y && mousePos[1] <= julia.height + julia.rect.y && julia.width < mandelbrot.width) {
            activeFractal = "julia";
            swapFractalSizes(julia, mandelbrot, context, device);
        }

        if (timer - timerPoint > 1) {
            fps = (int)((double)(frameCounter - frameCounterPoint) / (timer - timerPoint));
            timerPoint = timer;
            frameCounterPoint = frameCounter;
        }
        fpsText.setText("FPS: " + to_string(fps));
        mandelbrotIterationText.setText("Mandelbrot iterations: " + to_string(mandelbrot.maxIterations));
        juliaIterationText.setText("Julia iterations: " + to_string(julia.maxIterations));

        if (activeFractal == "mandelbrot") {
            quit = handleInput(mandelbrot, mandelbrot, julia);
        }
        else {
            quit = handleInput(julia, mandelbrot, julia);
        }

        if (mandelbrot.framesToUpdate > 0) {
            std::swap(mandelbrot.readPixelArr, mandelbrot.writePixelArr);
            swapClMemObjects(mandelbrot.d_readPixelArr, mandelbrot.d_writePixelArr);

            //vector<array<double, 2>> points = pointBatches1m[0].samples;
            mandelbrot.setKernelArgs(mandelbrotKernel);
            mandelbrot.writeBuffers();

            // Transfer data from device to host
            err = clEnqueueReadBuffer(mandelbrot.queue, mandelbrot.d_readPixelArr, CL_FALSE, 0, mandelbrot.width * mandelbrot.height * sizeof(uint32_t) * 3, mandelbrot.readPixelArr, 0, NULL, NULL);

            // Execute the mandelbrot kernel
            clEnqueueNDRangeKernel(mandelbrot.queue, mandelbrotKernel, 1, NULL, &mandelbrot.globalWorkSize, NULL, 0, NULL, NULL);

            clFinish(mandelbrot.queue);

            //createPoints(1, mandelbrot, pointBatches1m[0]);

            mandelbrot.mapRGBReadPixelArr((uint32_t*)mandelbrot.surface->pixels);
            
            mandelbrot.framesToUpdate--;
        }

        if (julia.framesToUpdate > 0) {
            swap(julia.readPixelArr, julia.writePixelArr);
            swapClMemObjects(julia.d_readPixelArr, julia.d_writePixelArr);

            julia.colouringScheme = mandelbrot.colouringScheme;
            julia.setKernelArgs(juliaKernel);
            julia.writeBuffers();

            err = clEnqueueReadBuffer(julia.queue, julia.d_readPixelArr, CL_FALSE, 0, julia.width * julia.height * sizeof(uint32_t) * 3, julia.readPixelArr, 0, NULL, NULL);

            clEnqueueNDRangeKernel(julia.queue, juliaKernel, 1, NULL, &julia.globalWorkSize, NULL, 0, NULL, NULL);

            julia.mapRGBReadPixelArr((uint32_t*)julia.surface->pixels);

            clFinish(julia.queue);

            julia.framesToUpdate--;
        }


        frameStall += (1.0 / frameRateCap) - deltaTime;
        if (frameStall > 0) {
            SDL_Delay(frameStall * 1000);
        }

        //update screen

        SDL_RenderClear(renderer);

        SDL_UpdateTexture(mandelbrot.texture, NULL, mandelbrot.surface->pixels, mandelbrot.surface->pitch);
        SDL_UpdateTexture(julia.texture, NULL, julia.surface->pixels, julia.surface->pitch);

        SDL_RenderCopy(renderer, backgroundTexture, NULL, NULL);
        SDL_RenderCopy(renderer, mandelbrot.texture, NULL, &mandelbrot.rect);
        SDL_RenderCopy(renderer, julia.texture, NULL, &julia.rect);
        SDL_RenderCopy(renderer, fpsText.texture, nullptr, &fpsText.rect);
        SDL_RenderCopy(renderer, mandelbrotIterationText.texture, nullptr, &mandelbrotIterationText.rect);
        SDL_RenderCopy(renderer, juliaIterationText.texture, nullptr, &juliaIterationText.rect);

        SDL_RenderPresent(renderer);

        frameCounter++;
        frameEnd = std::chrono::high_resolution_clock::now();
        deltaTime = (long double)(chrono::duration_cast<chrono::microseconds>(frameEnd - frameStart).count()) / 1000000;
        timer += deltaTime;
    }

    delete[] devices;
    delete[] platforms;
    delete[] buildLog;

    clReleaseKernel(mandelbrotKernel);
    clReleaseKernel(juliaKernel);
    clReleaseProgram(mandelbrotProgram);
    clReleaseProgram(juliaProgram);

    clReleaseContext(context);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    return 0;
}