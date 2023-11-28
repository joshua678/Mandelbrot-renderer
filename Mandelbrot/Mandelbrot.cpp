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

const bool showPalette = 0;
const bool debugFrameTime = 1;
const bool fullscreen = 1;
bool FPSCounter = 1;
bool shouldRenderJuliaSet = 1;
const int screenWidth = 1920;
const int screenHeight = 1080;
int mandelbrotGap = 30; //in pixels

struct fractal {
    int maxIterations = 1024;
    int maxIterationsFloor = 10;
    long double position[2] = { 0,0 }; //view centre in the plane
    double zoom = 3;
    long double moveSpeed = 0.15;
    long double zoomSpeed = 0.4;
    int colouringScheme = 1;

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

    fractal(int newWidth, int newHeight, SDL_Renderer* renderer, SDL_Window* window, cl_context context, cl_device_id device) {
        cl_int err;
        width = newWidth;
        height = newHeight;
        writePixelArr = new int[width * height * 3];
        readPixelArr = new int[width * height * 3];
        workQueue = new int[width * height];

        surface = SDL_CreateRGBSurface(0, width, height, 32, 0, 0, 0, 0);
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
};

//struct mandelbrotSet : fractal {
//
//};
//
//struct JuliaSet : fractal {
//
//};

#define DBOUT( s )            \
{                             \
   std::wostringstream os_;    \
   os_ << s;                   \
   OutputDebugStringW( os_.str().c_str() );  \
}

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

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;

array<double, 2> juliaIndex = { 0, 0 };

bool wKey = false, aKey = false, sKey = false, dKey = false,
eKey = false, qKey = false, downKey = false, upKey = false,
rightMouseButtonHeld = false;
bool shouldUpdateJulia = false;
array<int, 2> mousePos = { 0, 0 };

long double frameTime = 0;
std::array<std::chrono::time_point<std::chrono::high_resolution_clock>, 4> profilingPoints;
std::array<long double, 3> profilingTimeMeans;
std::chrono::time_point<std::chrono::high_resolution_clock> frameStart;
std::chrono::time_point<std::chrono::high_resolution_clock> frameEnd;
double deltaTime = 1;
int frameCounter = 0;
int fps = 0;
double timer = 0;
double timerPoint = 0;
int frameCounterPoint = 0;

bool handleInput(fractal &mandelbrot) { //returns true if program quit requested
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
                mandelbrot.zoomSpeed = 1.2;
                mandelbrot.moveSpeed = 0.45;
            }
            if (event.key.keysym.sym == SDLK_SPACE)
            {
                mandelbrot.colouringScheme = (mandelbrot.colouringScheme + 1) % 2;
                shouldUpdateJulia = true;
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
                mandelbrot.zoomSpeed = 0.4;
                mandelbrot.moveSpeed = 0.15;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_RIGHT) {
                rightMouseButtonHeld = true;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == SDL_BUTTON_RIGHT) {
                rightMouseButtonHeld = false;
            }
        }
    }
    if (wKey == true) {
        mandelbrot.position[1] -= mandelbrot.moveSpeed * mandelbrot.zoom * frameTime;
    }
    if (aKey == true) {
        mandelbrot.position[0] -= mandelbrot.moveSpeed * mandelbrot.zoom * frameTime;
    }
    if (sKey == true) {
        mandelbrot.position[1] += mandelbrot.moveSpeed * mandelbrot.zoom * frameTime;
    }
    if (dKey == true) {
        mandelbrot.position[0] += mandelbrot.moveSpeed * mandelbrot.zoom * frameTime;
    }
    if (eKey == true) {
        mandelbrot.zoom /= (1 + mandelbrot.zoomSpeed * frameTime);
    }
    if (qKey == true) {
        mandelbrot.zoom *= (1 + mandelbrot.zoomSpeed * frameTime);
    }
    if (downKey == true) {
        if (mandelbrot.maxIterations > mandelbrot.maxIterationsFloor) {
            mandelbrot.maxIterations /= (1 + 0.5 * frameTime);
        }
        DBOUT(mandelbrot.maxIterations << "\n");
    }
    if (upKey == true) {
        mandelbrot.maxIterations = mandelbrot.maxIterations*(1 + 0.5 * frameTime) + 1;
        DBOUT(mandelbrot.maxIterations << "\n");
    }
    if (rightMouseButtonHeld) {
        shouldUpdateJulia = true;
        array<int, 2> newMouseState = { 0,0 };
        SDL_GetMouseState(&newMouseState[0], &newMouseState[1]);
        if (newMouseState[0] <= mandelbrot.width && newMouseState[1] <= mandelbrot.height) {
            mousePos = newMouseState;
        }

        juliaIndex[0] = ((double)(mousePos[0] - mandelbrotGap) / mandelbrot.width - 0.5) * mandelbrot.zoom * ((double)mandelbrot.width / mandelbrot.height) + mandelbrot.position[0];
        juliaIndex[1] = ((double)(mousePos[1] - mandelbrotGap) / mandelbrot.height - 0.5) * mandelbrot.zoom + mandelbrot.position[1];
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    std::string kernelSource = loadKernelSource("Mandelbrot Kernel.cl");
    std::string kernelSource2 = loadKernelSource("Julia Kernel.cl");
    const char* sourceStr = kernelSource.c_str();
    const char* sourceStr2 = kernelSource2.c_str();
    //size_t sourceSize = kernelSource.size();

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

    TTF_Font* font = TTF_OpenFont("../Resources/Arial.ttf", 14);
    SDL_Color textColor = { 255, 150, 0, 255 };

    fractal mandelbrot(1400, 1000, renderer, window, context, device);
    mandelbrot.position[0] = -0.7;
    fractal julia(screenWidth - mandelbrot.width - mandelbrotGap*3, screenWidth - mandelbrot.width - mandelbrotGap * 3, renderer, window, context, device);

    cl_program program = clCreateProgramWithSource(context, 1, &sourceStr, NULL, &err);
    clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    cl_program program2 = clCreateProgramWithSource(context, 1, &sourceStr2, NULL, &err);
    clBuildProgram(program2, 1, &device, NULL, NULL, NULL);


    char* buildLog = new char[16384];
    size_t buildLogSize;
    err = clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(buildLog), buildLog, &buildLogSize);
    if (err != CL_SUCCESS) {
        std::cerr << "\n\nError: Failed to retrieve build log!\n\n" << std::endl;
        exit(1);
    }
    DBOUT("\n\nmandelbrot build log:\n" << buildLog << "\n\n")

    err = clGetProgramBuildInfo(program2, device, CL_PROGRAM_BUILD_LOG, sizeof(buildLog), buildLog, &buildLogSize);
    if (err != CL_SUCCESS) {
        std::cerr << "\n\nError: Failed to retrieve build log!\n\n" << std::endl;
        exit(1);
    }
    DBOUT("\n\njulia build log:\n" << buildLog << "\n\n")

    cl_kernel kernel = clCreateKernel(program, "mandelbrotKernel", &err);
    cl_kernel kernel2 = clCreateKernel(program2, "juliaKernel", &err);

    mandelbrot.rect = { mandelbrotGap, mandelbrotGap, mandelbrot.surface->w, mandelbrot.surface->h };
    julia.rect = { screenWidth - julia.width - mandelbrotGap, mandelbrotGap, julia.surface->w, julia.surface->h };

    SDL_Texture* backgroundTexture = SDL_CreateTextureFromSurface(renderer, SDL_LoadBMP("../Resources/background.bmp"));

    // Main render loop
    bool quit = false;
    while (!quit)
    {
        if (timer - timerPoint > 1) {
            fps = (int)((double)(frameCounter - frameCounterPoint) / (timer - timerPoint));
            timerPoint = timer;
            frameCounterPoint = frameCounter;
        }
        std::string fpsText = "FPS: " + std::to_string(fps);
        SDL_Surface* fpsSurface = TTF_RenderText_Solid(font, fpsText.c_str(), textColor);
        SDL_Texture* fpsTexture = SDL_CreateTextureFromSurface(renderer, fpsSurface);
        SDL_Rect textRect = { 10, 10, fpsSurface->w, fpsSurface->h };

        frameStart = std::chrono::high_resolution_clock::now();
        quit = handleInput(mandelbrot);

        std::swap(mandelbrot.readPixelArr, mandelbrot.writePixelArr);
        swapClMemObjects(mandelbrot.d_readPixelArr, mandelbrot.d_writePixelArr);

        std::swap(julia.readPixelArr, julia.writePixelArr);
        swapClMemObjects(julia.d_readPixelArr, julia.d_writePixelArr);

        err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &mandelbrot.d_writePixelArr);
        err = clSetKernelArg(kernel, 1, sizeof(int), &(mandelbrot.width));
        err = clSetKernelArg(kernel, 2, sizeof(int), &(mandelbrot.height));
        err = clSetKernelArg(kernel, 3, sizeof(double), &mandelbrot.zoom);
        err = clSetKernelArg(kernel, 4, sizeof(double), &mandelbrot.position[0]);
        err = clSetKernelArg(kernel, 5, sizeof(double), &mandelbrot.position[1]);
        err = clSetKernelArg(kernel, 6, sizeof(int), &mandelbrot.maxIterations);
        err = clSetKernelArg(kernel, 7, sizeof(cl_mem), &mandelbrot.d_workQueue);
        err = clSetKernelArg(kernel, 8, sizeof(cl_mem), &mandelbrot.d_globalIndex);
        err = clSetKernelArg(kernel, 9, sizeof(int), &mandelbrot.colouringScheme);

        julia.colouringScheme = mandelbrot.colouringScheme;
        err = clSetKernelArg(kernel2, 0, sizeof(cl_mem), &julia.d_writePixelArr);
        err = clSetKernelArg(kernel2, 1, sizeof(int), &(julia.width));
        err = clSetKernelArg(kernel2, 2, sizeof(int), &(julia.height));
        err = clSetKernelArg(kernel2, 3, sizeof(double), &julia.zoom);
        err = clSetKernelArg(kernel2, 4, sizeof(double), &julia.position[0]);
        err = clSetKernelArg(kernel2, 5, sizeof(double), &julia.position[1]);
        err = clSetKernelArg(kernel2, 6, sizeof(int), &julia.maxIterations);
        err = clSetKernelArg(kernel2, 7, sizeof(cl_mem), &julia.d_workQueue);
        err = clSetKernelArg(kernel2, 8, sizeof(cl_mem), &julia.d_globalIndex);
        err = clSetKernelArg(kernel2, 9, sizeof(int), &julia.colouringScheme);
        err = clSetKernelArg(kernel2, 10, sizeof(double), &juliaIndex[0]);
        err = clSetKernelArg(kernel2, 11, sizeof(double), &juliaIndex[1]);

        profilingPoints[0] = std::chrono::high_resolution_clock::now();

        err = clEnqueueWriteBuffer(mandelbrot.queue, mandelbrot.d_workQueue, CL_FALSE, 0, sizeof(int) * mandelbrot.width*mandelbrot.height, mandelbrot.workQueue, 0, NULL, NULL);
        if (err != CL_SUCCESS) {
            std::cerr << "\n\nError: Failed to write buffer!\n\n" << std::endl;
            exit(1);
        }
        mandelbrot.globalIndex = 0;
        err = clEnqueueWriteBuffer(mandelbrot.queue, mandelbrot.d_globalIndex, CL_FALSE, 0, sizeof(int), &mandelbrot.globalIndex, 0, NULL, NULL);
        if (err != CL_SUCCESS) {
            std::cerr << "\n\nError: Failed to write buffer!\n\n" << std::endl;
            exit(1);
        }

        // Transfer data from device to host
        err = clEnqueueReadBuffer(mandelbrot.queue, mandelbrot.d_readPixelArr, CL_FALSE, 0, mandelbrot.width * mandelbrot.height * sizeof(int) * 3, mandelbrot.readPixelArr, 0, NULL, NULL);

        uint32_t* pixels = (uint32_t*)mandelbrot.surface->pixels;

        // Execute the mandelbrot kernel

        clEnqueueNDRangeKernel(mandelbrot.queue, kernel, 1, NULL, &mandelbrot.globalWorkSize, NULL, 0, NULL, NULL);

#pragma omp parallel for
        for (int l = 0; l < mandelbrot.height; l++) {
            for (int i = 0; i < mandelbrot.width; i++) {
                pixels[l * mandelbrot.width + i] = SDL_MapRGB(mandelbrot.surface->format, 
                    mandelbrot.readPixelArr[l * mandelbrot.width + i], 
                    mandelbrot.readPixelArr[mandelbrot.width*mandelbrot.height + l * mandelbrot.width + i], 
                    mandelbrot.readPixelArr[mandelbrot.width*mandelbrot.height * 2 + l * mandelbrot.width + i]);
            }
        }

        clFinish(mandelbrot.queue);

        //execute the julia kernel

        if (shouldUpdateJulia) {
            err = clEnqueueWriteBuffer(julia.queue, julia.d_workQueue, CL_FALSE, 0, sizeof(int) * julia.width * julia.height, julia.workQueue, 0, NULL, NULL);
            if (err != CL_SUCCESS) {
                std::cerr << "\n\nError: Failed to write buffer!\n\n" << std::endl;
                exit(1);
            }
            julia.globalIndex = 0;
            err = clEnqueueWriteBuffer(julia.queue, julia.d_globalIndex, CL_FALSE, 0, sizeof(int), &julia.globalIndex, 0, NULL, NULL);
            if (err != CL_SUCCESS) {
                std::cerr << "\n\nError: Failed to write buffer!\n\n" << std::endl;
                exit(1);
            }

            err = clEnqueueReadBuffer(julia.queue, julia.d_readPixelArr, CL_FALSE, 0, julia.width * julia.height * sizeof(int) * 3, julia.readPixelArr, 0, NULL, NULL);

            uint32_t* pixels2 = (uint32_t*)julia.surface->pixels;

            clEnqueueNDRangeKernel(julia.queue, kernel2, 1, NULL, &julia.globalWorkSize, NULL, 0, NULL, NULL);

#pragma omp parallel for
            for (int l = 0; l < julia.width; l++) {
                for (int i = 0; i < julia.width; i++) {
                    pixels2[l * julia.width + i] = SDL_MapRGB(julia.surface->format,
                        julia.readPixelArr[l * julia.width + i],
                        julia.readPixelArr[julia.width * julia.height + l * julia.width + i],
                        julia.readPixelArr[julia.width * julia.height * 2 + l * julia.width + i]);
                }
            }

            clFinish(julia.queue);
            shouldUpdateJulia = false;
        }


        if (showPalette) {
            for (int l = 0; l < mandelbrot.height; l++) {
                for (int i = 0; i < mandelbrot.width; i++) {
                    pixels[l * mandelbrot.width + i] = SDL_MapRGB(mandelbrot.surface->format, 
                        palette((double)l / mandelbrot.width, 1)[0], 
                        palette((double)l / mandelbrot.width, 1)[1], 
                        palette((double)l / mandelbrot.width, 1)[2]);
                }
            }
        }

        //update screen

        SDL_RenderClear(renderer);

        SDL_UpdateTexture(mandelbrot.texture, NULL, mandelbrot.surface->pixels, mandelbrot.surface->pitch);
        SDL_UpdateTexture(julia.texture, NULL, julia.surface->pixels, julia.surface->pitch);

        SDL_RenderCopy(renderer, backgroundTexture, NULL, NULL);
        SDL_RenderCopy(renderer, mandelbrot.texture, NULL, &mandelbrot.rect);
        SDL_RenderCopy(renderer, julia.texture, NULL, &julia.rect);
        SDL_RenderCopy(renderer, fpsTexture, nullptr, &textRect);

        SDL_RenderPresent(renderer);

        profilingPoints[3] = std::chrono::high_resolution_clock::now();

        SDL_FreeSurface(fpsSurface);
        SDL_DestroyTexture(fpsTexture);

        frameCounter++;

        int frameCountMean = 100;
        double meanFrameTime;
        frameTime = (long double)(chrono::duration_cast<chrono::microseconds>(profilingPoints[3] - profilingPoints[0]).count()) / 1000000;
        for (int i = 0; i < profilingTimeMeans.size(); i++) {
            profilingTimeMeans[i] += (long double)(chrono::duration_cast<chrono::microseconds>(profilingPoints[i + 1] - profilingPoints[i]).count()) / 1000000 / 100;
        }
        if (debugFrameTime && frameCounter % 100 == 0) {
            meanFrameTime = profilingTimeMeans[0] + profilingTimeMeans[1] + profilingTimeMeans[2];
            DBOUT(endl << "-total frame time: " << meanFrameTime << endl);
            DBOUT("time spent iterating pixels on GPU while writing/reading buffers and getting image ready in parallel: " << profilingTimeMeans[0] << "(" << round((profilingTimeMeans[0] / meanFrameTime) * 100) << "%)" << endl);
            //DBOUT("time spent collecting data from GPU: " << profilingTimeMeans[2] << "(" << round((profilingTimeMeans[2] / meanFrameTime) * 100) << "%)" << endl);
            DBOUT("time spent in GPU alone: " << profilingTimeMeans[1] << "(" << round((profilingTimeMeans[1] / meanFrameTime) * 100) << "%)" << endl);
            DBOUT("time spent displaying image: " << profilingTimeMeans[2] << "(" << round((profilingTimeMeans[2] / meanFrameTime) * 100) << "%)" << endl);
            for (int i = 0; i < profilingTimeMeans.size(); i++) {
                profilingTimeMeans[i] = 0;
            }
        }
        frameEnd = std::chrono::high_resolution_clock::now();
        deltaTime = (long double)(chrono::duration_cast<chrono::microseconds>(frameEnd - frameStart).count()) / 1000000;
        timer += deltaTime;
    }

    delete[] devices;
    delete[] platforms;
    delete[] buildLog;

    clReleaseKernel(kernel);
    clReleaseKernel(kernel2);
    clReleaseProgram(program);
    clReleaseProgram(program2);

    clReleaseContext(context);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    TTF_CloseFont(font);

    return 0;
}