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

bool wKey = false, aKey = false, sKey = false, dKey = false,
eKey = false, qKey = false, downKey = false, upKey = false,
leftMouseButtonHeld = false;
array<int, 2> mousePos = { 0, 0 };

std::chrono::time_point<std::chrono::high_resolution_clock> frameStart;
std::chrono::time_point<std::chrono::high_resolution_clock> frameEnd;
double deltaTime = 1;
int frameCounter = 0;
int fps = 0;
double timer = 0;
double timerPoint = 0;
int frameCounterPoint = 0;
double frameStall = 0;

#define DBOUT( s )            \
{                             \
   std::wostringstream os_;    \
   os_ << s;                   \
   OutputDebugStringW( os_.str().c_str() );  \
}

struct fractal {
private:
    Uint32 rmask, gmask, bmask, amask;
public:
    int maxIterations = 1024;
    int maxIterationsFloor = 10;
    long double position[2] = { 0,0 }; //view centre in the plane
    double zoom = 3;
    long double moveSpeed = 0.15;
    long double zoomSpeed = 0.4;
    int colouringScheme = 0;

    string type;
    SDL_Surface* surface = NULL;
    SDL_Texture* texture = NULL;
    int* writePixelArr;
    int* readPixelArr;
    int width;
    int height;
    SDL_Rect rect;
    cl_queue_properties queueProperties = 0;
    cl_command_queue queue;
    cl_mem d_readPixelArr;
    cl_mem d_writePixelArr;
    size_t globalWorkSize = 6400;
    size_t localWorkSize = NULL;
    int* workQueue;
    cl_mem d_workQueue;
    int globalIndex = 0;
    cl_mem d_globalIndex;
    int framesToUpdate = 0;

    fractal(int newWidth, int newHeight, SDL_Renderer* renderer, SDL_Window* window, cl_context context, cl_device_id device) {
        type = "fractal";
        cl_int err;
        width = newWidth;
        height = newHeight;
        writePixelArr = new int[width * height * 3];
        readPixelArr = new int[width * height * 3];
        workQueue = new int[width * height];

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        rmask = 0xff000000;
        gmask = 0x00ff0000;
        bmask = 0x0000ff000;
        amask = 0x000000ff;
#else // SDL_BYTEORDER == SDL_LIL_ENDIAN
        rmask = 0x000000ff;
        gmask = 0x0000ff00;
        bmask = 0x00ff0000;
        amask = 0xff000000;
#endif

        surface = SDL_CreateRGBSurface(0, width, height, 32, rmask, gmask, bmask, amask);
        if (surface == NULL)
        {
            printf("Surface could not be created! SDL_Error: %s\n", SDL_GetError());

            SDL_FreeSurface(surface);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
        }
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture == NULL)
        {
            printf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());

            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
        }

        queueProperties = 0;
        queue = clCreateCommandQueueWithProperties(context, device, &queueProperties, &err);
        d_readPixelArr = clCreateBuffer(context, CL_MEM_READ_ONLY, width * height * sizeof(int) * 3, NULL, &err);
        d_writePixelArr = clCreateBuffer(context, CL_MEM_WRITE_ONLY, width * height * sizeof(int) * 3, NULL, &err);

        err = clEnqueueWriteBuffer(queue, d_readPixelArr, CL_TRUE, 0, width * height * sizeof(int) * 3, readPixelArr, 0, NULL, NULL);
        err = clEnqueueWriteBuffer(queue, d_writePixelArr, CL_TRUE, 0, width * height * sizeof(int) * 3, writePixelArr, 0, NULL, NULL);

        for (int i = 0; i < width * height; ++i) {
            workQueue[i] = i;
        }

        globalWorkSize = 6400;
        localWorkSize = NULL;
        d_workQueue = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(int) * width * height, workQueue, &err);
        globalIndex = 0;
        d_globalIndex = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(int), &globalIndex, &err);
    }

    ~fractal() {
        // Release OpenCL resources
        if (queue) {
            clReleaseCommandQueue(queue);
        }
        if (d_readPixelArr) {
            clReleaseMemObject(d_readPixelArr);
        }
        if (d_writePixelArr) {
            clReleaseMemObject(d_writePixelArr);
        }
        if (d_workQueue) {
            clReleaseMemObject(d_workQueue);
        }
        if (d_globalIndex) {
            clReleaseMemObject(d_globalIndex);
        }

        // Release dynamically allocated arrays
        delete[] writePixelArr;
        delete[] readPixelArr;
        delete[] workQueue;

        // Release SDL resources
        if (texture) {
            SDL_DestroyTexture(texture);
        }
        if (surface) {
            SDL_FreeSurface(surface);
        }
    }

    void mapRGBReadPixelArr(uint32_t* pixelsToSet) {
#pragma omp parallel for
        for (int l = 0; l < height; l++) {
            for (int i = 0; i < width; i++) {
                pixelsToSet[l * width + i] = SDL_MapRGB(surface->format,
                    readPixelArr[l * width + i],
                    readPixelArr[width * height + l * width + i],
                    readPixelArr[width * height * 2 + l * width + i]);
            }
        }
    }

    void writeBuffers() {
        cl_int err;
        err = clEnqueueWriteBuffer(queue, d_workQueue, CL_FALSE, 0, sizeof(int) * width * height, workQueue, 0, NULL, NULL);
        if (err != CL_SUCCESS) {
            std::cerr << "\n\nError: Failed to write buffer!\n\n" << std::endl;
            exit(1);
        }
        globalIndex = 0;
        err = clEnqueueWriteBuffer(queue, d_globalIndex, CL_FALSE, 0, sizeof(int), &globalIndex, 0, NULL, NULL);
        if (err != CL_SUCCESS) {
            std::cerr << "\n\nError: Failed to write buffer!\n\n" << std::endl;
            exit(1);
        }
    }

    void resize(int newWidth, int newHeight, SDL_Renderer* renderer, SDL_Window* window, cl_context context, cl_device_id device) {
        // Release OpenCL resources
        if (queue) {
            clReleaseCommandQueue(queue);
        }
        if (d_readPixelArr) {
            clReleaseMemObject(d_readPixelArr);
        }
        if (d_writePixelArr) {
            clReleaseMemObject(d_writePixelArr);
        }
        if (d_workQueue) {
            clReleaseMemObject(d_workQueue);
        }
        if (d_globalIndex) {
            clReleaseMemObject(d_globalIndex);
        }

        // Release dynamically allocated arrays
        delete[] writePixelArr;
        delete[] readPixelArr;
        delete[] workQueue;

        // Release SDL resources
        if (texture) {
            SDL_DestroyTexture(texture);
        }
        if (surface) {
            SDL_FreeSurface(surface);
        }

        cl_int err;
        width = newWidth;
        height = newHeight;
        writePixelArr = new int[width * height * 3];
        readPixelArr = new int[width * height * 3];
        workQueue = new int[width * height];

        surface = SDL_CreateRGBSurface(0, width, height, 32, rmask, gmask, bmask, amask);
        if (surface == NULL)
        {
            printf("Surface could not be created! SDL_Error: %s\n", SDL_GetError());

            SDL_FreeSurface(surface);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
        }
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture == NULL)
        {
            printf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());

            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
        }

        queueProperties = 0;
        queue = clCreateCommandQueueWithProperties(context, device, &queueProperties, &err);
        d_readPixelArr = clCreateBuffer(context, CL_MEM_READ_ONLY, width * height * sizeof(int) * 3, NULL, &err);
        d_writePixelArr = clCreateBuffer(context, CL_MEM_WRITE_ONLY, width * height * sizeof(int) * 3, NULL, &err);

        err = clEnqueueWriteBuffer(queue, d_readPixelArr, CL_TRUE, 0, width * height * sizeof(int) * 3, readPixelArr, 0, NULL, NULL);
        err = clEnqueueWriteBuffer(queue, d_writePixelArr, CL_TRUE, 0, width * height * sizeof(int) * 3, writePixelArr, 0, NULL, NULL);

        for (int i = 0; i < width * height; ++i) {
            workQueue[i] = i;
        }

        globalWorkSize = 6400;
        localWorkSize = NULL;
        d_workQueue = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(int) * width * height, workQueue, &err);
        globalIndex = 0;
        d_globalIndex = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(int), &globalIndex, &err);
    }
};

struct mandelbrotSet : fractal {
    mandelbrotSet(int newWidth, int newHeight, SDL_Renderer* renderer, SDL_Window* window, cl_context context, cl_device_id device)
        : fractal(newWidth, newHeight, renderer, window, context, device) {
        type = "mandelbrotSet";
        framesToUpdate = 4;
    }
    void setKernelArgs(cl_kernel& kernel) {
        cl_int err;
        err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_writePixelArr);
        err = clSetKernelArg(kernel, 1, sizeof(int), &width);
        err = clSetKernelArg(kernel, 2, sizeof(int), &height);
        err = clSetKernelArg(kernel, 3, sizeof(double), &zoom);
        err = clSetKernelArg(kernel, 4, sizeof(double), &position[0]);
        err = clSetKernelArg(kernel, 5, sizeof(double), &position[1]);
        err = clSetKernelArg(kernel, 6, sizeof(int), &maxIterations);
        err = clSetKernelArg(kernel, 7, sizeof(cl_mem), &d_workQueue);
        err = clSetKernelArg(kernel, 8, sizeof(cl_mem), &d_globalIndex);
        err = clSetKernelArg(kernel, 9, sizeof(int), &colouringScheme);
    }
};

struct juliaSet : fractal {
    array<double, 2> index = { 0, 0 };
    juliaSet(int newWidth, int newHeight, SDL_Renderer* renderer, SDL_Window* window, cl_context context, cl_device_id device)
        : fractal(newWidth, newHeight, renderer, window, context, device) {
        type = "juliaSet";
    }
    void setKernelArgs(cl_kernel &kernel) {
        cl_int err;
        err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_writePixelArr);
        err = clSetKernelArg(kernel, 1, sizeof(int), &(width));
        err = clSetKernelArg(kernel, 2, sizeof(int), &(height));
        err = clSetKernelArg(kernel, 3, sizeof(double), &zoom);
        err = clSetKernelArg(kernel, 4, sizeof(double), &position[0]);
        err = clSetKernelArg(kernel, 5, sizeof(double), &position[1]);
        err = clSetKernelArg(kernel, 6, sizeof(int), &maxIterations);
        err = clSetKernelArg(kernel, 7, sizeof(cl_mem), &d_workQueue);
        err = clSetKernelArg(kernel, 8, sizeof(cl_mem), &d_globalIndex);
        err = clSetKernelArg(kernel, 9, sizeof(int), &colouringScheme);
        err = clSetKernelArg(kernel, 10, sizeof(double), &index[0]);
        err = clSetKernelArg(kernel, 11, sizeof(double), &index[1]);
    }
};

struct text {
    TTF_Font* font;
    SDL_Color colour = { 0, 0, 0, 255 };
    string textStr;
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_Rect rect;
    int x, y;

    text(string newText, int newX, int newY, int newSize) {
        textStr = newText;
        x = newX;
        y = newY;
        font = TTF_OpenFont("../Resources/Arial.ttf", newSize);
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

const char* getErrorString(cl_int error)
{
    switch (error) {
        // run-time and JIT compiler errors
    case 0: return "CL_SUCCESS";
    case -1: return "CL_DEVICE_NOT_FOUND";
    case -2: return "CL_DEVICE_NOT_AVAILABLE";
    case -3: return "CL_COMPILER_NOT_AVAILABLE";
    case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case -5: return "CL_OUT_OF_RESOURCES";
    case -6: return "CL_OUT_OF_HOST_MEMORY";
    case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
    case -8: return "CL_MEM_COPY_OVERLAP";
    case -9: return "CL_IMAGE_FORMAT_MISMATCH";
    case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case -11: return "CL_BUILD_PROGRAM_FAILURE";
    case -12: return "CL_MAP_FAILURE";
    case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
    case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
    case -15: return "CL_COMPILE_PROGRAM_FAILURE";
    case -16: return "CL_LINKER_NOT_AVAILABLE";
    case -17: return "CL_LINK_PROGRAM_FAILURE";
    case -18: return "CL_DEVICE_PARTITION_FAILED";
    case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

        // compile-time errors
    case -30: return "CL_INVALID_VALUE";
    case -31: return "CL_INVALID_DEVICE_TYPE";
    case -32: return "CL_INVALID_PLATFORM";
    case -33: return "CL_INVALID_DEVICE";
    case -34: return "CL_INVALID_CONTEXT";
    case -35: return "CL_INVALID_QUEUE_PROPERTIES";
    case -36: return "CL_INVALID_COMMAND_QUEUE";
    case -37: return "CL_INVALID_HOST_PTR";
    case -38: return "CL_INVALID_MEM_OBJECT";
    case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case -40: return "CL_INVALID_IMAGE_SIZE";
    case -41: return "CL_INVALID_SAMPLER";
    case -42: return "CL_INVALID_BINARY";
    case -43: return "CL_INVALID_BUILD_OPTIONS";
    case -44: return "CL_INVALID_PROGRAM";
    case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
    case -46: return "CL_INVALID_KERNEL_NAME";
    case -47: return "CL_INVALID_KERNEL_DEFINITION";
    case -48: return "CL_INVALID_KERNEL";
    case -49: return "CL_INVALID_ARG_INDEX";
    case -50: return "CL_INVALID_ARG_VALUE";
    case -51: return "CL_INVALID_ARG_SIZE";
    case -52: return "CL_INVALID_KERNEL_ARGS";
    case -53: return "CL_INVALID_WORK_DIMENSION";
    case -54: return "CL_INVALID_WORK_GROUP_SIZE";
    case -55: return "CL_INVALID_WORK_ITEM_SIZE";
    case -56: return "CL_INVALID_GLOBAL_OFFSET";
    case -57: return "CL_INVALID_EVENT_WAIT_LIST";
    case -58: return "CL_INVALID_EVENT";
    case -59: return "CL_INVALID_OPERATION";
    case -60: return "CL_INVALID_GL_OBJECT";
    case -61: return "CL_INVALID_BUFFER_SIZE";
    case -62: return "CL_INVALID_MIP_LEVEL";
    case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
    case -64: return "CL_INVALID_PROPERTY";
    case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
    case -66: return "CL_INVALID_COMPILER_OPTIONS";
    case -67: return "CL_INVALID_LINKER_OPTIONS";
    case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";

        // extension errors
    case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
    case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
    case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
    case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
    case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
    case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
    default: return "Unknown OpenCL error";
    }
}

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

bool handleInput(fractal &activeFractal, mandelbrotSet &mandelbrot, juliaSet &julia) { //returns true if program quit requested
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
        if (event.type == SDL_QUIT)
        {
            return true;
        }
        else if (event.type == SDL_KEYDOWN)
        {
            if (event.key.keysym.sym == SDLK_w)
            {
                wKey = true;
            }
            if (event.key.keysym.sym == SDLK_a)
            {
                aKey = true;
            }
            if (event.key.keysym.sym == SDLK_s)
            {
                sKey = true;
            }
            if (event.key.keysym.sym == SDLK_d)
            {
                dKey = true;
            }
            if (event.key.keysym.sym == SDLK_e)
            {
                eKey = true;
            }
            if (event.key.keysym.sym == SDLK_q)
            {
                qKey = true;
            }
            if (event.key.keysym.sym == SDLK_DOWN)
            {
                downKey = true;
            }
            if (event.key.keysym.sym == SDLK_UP)
            {
                upKey = true;
            }
            if (event.key.keysym.sym == SDLK_LSHIFT)
            {
                activeFractal.zoomSpeed = 1.2;
                activeFractal.moveSpeed = 0.45;
            }
            if (event.key.keysym.sym == SDLK_SPACE)
            {
                activeFractal.colouringScheme = (activeFractal.colouringScheme + 1) % 2;
                julia.framesToUpdate = 4;
                mandelbrot.framesToUpdate = 4;
            }
        }
        else if (event.type == SDL_KEYUP)
        {
            if (event.key.keysym.sym == SDLK_w)
            {
                wKey = false;
            }
            if (event.key.keysym.sym == SDLK_a)
            {
                aKey = false;
            }
            if (event.key.keysym.sym == SDLK_s)
            {
                sKey = false;
            }
            if (event.key.keysym.sym == SDLK_d)
            {
                dKey = false;
            }
            if (event.key.keysym.sym == SDLK_e)
            {
                eKey = false;
            }
            if (event.key.keysym.sym == SDLK_q)
            {
                qKey = false;
            }
            if (event.key.keysym.sym == SDLK_DOWN)
            {
                downKey = false;
            }
            if (event.key.keysym.sym == SDLK_UP)
            {
                upKey = false;
            }
            if (event.key.keysym.sym == SDLK_LSHIFT)
            {
                activeFractal.zoomSpeed = 0.4;
                activeFractal.moveSpeed = 0.15;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                leftMouseButtonHeld = true;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                leftMouseButtonHeld = false;
            }
        }
    }
    if (wKey == true) {
        activeFractal.position[1] -= activeFractal.moveSpeed * activeFractal.zoom * deltaTime;
        activeFractal.framesToUpdate = 4;
    }
    if (aKey == true) {
        activeFractal.position[0] -= activeFractal.moveSpeed * activeFractal.zoom * deltaTime;
        activeFractal.framesToUpdate = 4;
    }
    if (sKey == true) {
        activeFractal.position[1] += activeFractal.moveSpeed * activeFractal.zoom * deltaTime;
        activeFractal.framesToUpdate = 4;
    }
    if (dKey == true) {
        activeFractal.position[0] += activeFractal.moveSpeed * activeFractal.zoom * deltaTime;
        activeFractal.framesToUpdate = 4;
    }
    if (eKey == true) {
        activeFractal.zoom /= (1 + activeFractal.zoomSpeed * deltaTime);
        activeFractal.framesToUpdate = 4;
    }
    if (qKey == true) {
        activeFractal.zoom *= (1 + activeFractal.zoomSpeed * deltaTime);
        activeFractal.framesToUpdate = 4;
    }
    if (downKey == true) {
        if (activeFractal.maxIterations > activeFractal.maxIterationsFloor) {
            activeFractal.maxIterations /= (1 + 0.5 * deltaTime);
            activeFractal.framesToUpdate = 4;
        }
        DBOUT(mandelbrot.maxIterations << "\n");
    }
    if (upKey == true) {
        activeFractal.maxIterations = activeFractal.maxIterations*(1 + 0.5 * deltaTime) + 1;
        activeFractal.framesToUpdate = 4;
        DBOUT(activeFractal.maxIterations << "\n");
    }
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
    cl_device_id* devices = new cl_device_id[1];
    cl_uint num_devices;
    cl_platform_id* platforms;
    cl_uint num_platforms;
    clGetPlatformIDs(0, NULL, &num_platforms);
    platforms = new cl_platform_id[num_platforms];
    clGetPlatformIDs(num_platforms, platforms, NULL);
    cl_int err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 1, &devices[0], &num_devices);
    if (err != CL_SUCCESS) {
        std::cerr << "Error: Failed to retrieve device IDs!" << std::endl;
        exit(1);
    }
    cl_device_id device = devices[0];
    DBOUT("\n\ndevice: " << device << "\n\n");
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);

    mandelbrotSet mandelbrot(screenWidth * 0.7, screenHeight - mandelbrotGap * 2, renderer, window, context, device);
    mandelbrot.position[0] = -0.7;
    juliaSet julia(screenWidth - mandelbrot.width - mandelbrotGap*3, screenWidth - mandelbrot.width - mandelbrotGap * 3, renderer, window, context, device);

    cl_program mandelbrotProgram = clCreateProgramWithSource(context, 1, &mandelbrotSourceStr, NULL, &err);
    clBuildProgram(mandelbrotProgram, 1, &device, NULL, NULL, NULL);
    cl_program juliaProgram = clCreateProgramWithSource(context, 1, &juliaSourceStr, NULL, &err);
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

    SDL_Texture* backgroundTexture = SDL_CreateTextureFromSurface(renderer, SDL_LoadBMP("../Resources/background.bmp"));

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

            mandelbrot.setKernelArgs(mandelbrotKernel);
            mandelbrot.writeBuffers();
            // Transfer data from device to host
            err = clEnqueueReadBuffer(mandelbrot.queue, mandelbrot.d_readPixelArr, CL_FALSE, 0, mandelbrot.width * mandelbrot.height * sizeof(int) * 3, mandelbrot.readPixelArr, 0, NULL, NULL);

            // Execute the mandelbrot kernel
            clEnqueueNDRangeKernel(mandelbrot.queue, mandelbrotKernel, 1, NULL, &mandelbrot.globalWorkSize, NULL, 0, NULL, NULL);

            mandelbrot.mapRGBReadPixelArr((uint32_t*)mandelbrot.surface->pixels);

            clFinish(mandelbrot.queue);
            
            mandelbrot.framesToUpdate--;
        }

        if (julia.framesToUpdate > 0) {
            swap(julia.readPixelArr, julia.writePixelArr);
            swapClMemObjects(julia.d_readPixelArr, julia.d_writePixelArr);

            julia.colouringScheme = mandelbrot.colouringScheme;
            julia.setKernelArgs(juliaKernel);
            julia.writeBuffers();

            err = clEnqueueReadBuffer(julia.queue, julia.d_readPixelArr, CL_FALSE, 0, julia.width * julia.height * sizeof(int) * 3, julia.readPixelArr, 0, NULL, NULL);

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