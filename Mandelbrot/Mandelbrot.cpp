#include <iostream>
#include <SDL/SDL.h>
#include <chrono>
#include <thread>
#include <Windows.h>
#include <ppl.h>
#include <vector>
#include <sstream>
#include <cmath>
#include <CL/cl.h>
#include "Kernel.h"
#include <array>

#define MAX_SOURCE_SIZE (0x100000)

using namespace std;
using namespace concurrency;

int maxIterations = 1024;
int maxIterationsFloor = 512;
long double position[2] = { -0.29,0 }; //view centre in the plane
double zoom = 3;
long double moveSpeed = 0.15;
long double zoomSpeed = 0.4;
bool showPalette = false; //set to true to show the palette instead of the mandlebrot render
bool showFrameTime = 1;

const int screenWidth = 1920;
const int screenHeight = 1080;

int pixelArr[screenWidth * screenHeight * 3];

int totalPixels = screenWidth * screenHeight;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Surface* surface = NULL;
SDL_Texture* texture = NULL;

#define DBOUT( s )            \
{                             \
   std::wostringstream os_;    \
   os_ << s;                   \
   OutputDebugStringW( os_.str().c_str() );  \
}

unsigned long RGBtoHex(int r, int g, int b)
{
    return ((r & 0xff) << 16) + ((g & 0xff) << 8) + (b & 0xff);
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

int* palette(double pos, double size) { //pos is between 0 and 1
    double data[][4] = { //{position, red, green, blue}
        {size * 0, 32, 107, 203},
        {size * (double)1 / 15, 237, 255, 255},
        {size * (double)2 / 15, 255, 170, 0},
        {size * (double)3 / 15, 0, 2, 0},
        {size * (double)4 / 15, 255, 0, 0},
        {size * (double)5 / 15, 0, 255, 0},
        {size * (double)6 / 15, 0, 0, 255},
        {size * (double)7 / 15, 255, 255, 0},
        {size * (double)8 / 15, 255, 0, 255},
        {size * (double)9 / 15, 0, 255, 255},
        {size * (double)10 / 15, 255, 128, 0},
        {size * (double)11 / 15, 128, 255, 0},
        {size * (double)12 / 15, 0, 255, 128},
        {size * (double)13 / 15, 128, 0, 255},
        {size * (double)14 / 15, 255, 0, 128},
        {size * 1, 75, 0, 130}
    };
    int dataSize = sizeof(data) / sizeof(data[0]);
    int result[] = { data[0][1], data[0][2], data[0][3] }; //rgb values


    for (int i = 0; i < dataSize - 1; i++) {
        if (pos > data[i][0] && pos <= data[i + 1][0]) {
            result[0] = data[i][1] + (pos - data[i][0]) * (data[i + 1][1] - data[i][1]) * (1 / (data[i + 1][0] - data[i][0]));
            result[1] = data[i][2] + (pos - data[i][0]) * (data[i + 1][2] - data[i][2]) * (1 / (data[i + 1][0] - data[i][0]));
            result[2] = data[i][3] + (pos - data[i][0]) * (data[i + 1][3] - data[i][3]) * (1 / (data[i + 1][0] - data[i][0]));
        }
    }

    return result;
}

int* palette2(double pos, double size) { //pos is between 0 and 1
    double data[][4] = { //{position, red, green, blue}
        {size * 0, 66, 30, 15},
        {size * (double)1 / 15, 25, 7, 26},
        {size * (double)2 / 15, 9, 1, 47},
        {size * (double)3 / 15, 4, 4, 73},
        {size * (double)4 / 15, 0, 7, 100},
        {size * (double)5 / 15, 12, 44, 138},
        {size * (double)6 / 15, 24, 82, 177},
        {size * (double)7 / 15, 57, 125, 209},
        {size * (double)8 / 15, 134, 181, 229},
        {size * (double)9 / 15, 211, 236, 248},
        {size * (double)10 / 15, 241, 233, 191},
        {size * (double)11 / 15, 248, 201, 95},
        {size * (double)12 / 15, 255, 170, 0},
        {size * (double)13 / 15, 204, 128, 0},
        {size * (double)14 / 15, 153, 87, 0},
        {size * 1, 106, 52, 3}
    };
    int dataSize = sizeof(data) / sizeof(data[0]);
    int result[] = { data[0][1], data[0][2], data[0][3] }; //rgb values

    for (int i = 0; i < dataSize - 1; i++) {
        if (pos > data[i][0] && pos <= data[i + 1][0]) {
            result[0] = (int)(data[i][1] + (pos - data[i][0]) * (data[i + 1][1] - data[i][1]) * (1 / (data[i + 1][0] - data[i][0])));
            result[1] = (int)(data[i][2] + (pos - data[i][0]) * (data[i + 1][2] - data[i][2]) * (1 / (data[i + 1][0] - data[i][0])));
            result[2] = (int)(data[i][3] + (pos - data[i][0]) * (data[i + 1][3] - data[i][3]) * (1 / (data[i + 1][0] - data[i][0])));
        }
    }

    return result;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    std::array<std::chrono::time_point<std::chrono::high_resolution_clock>, 6> profilingPoints;
    std::array<long double, 5> profilingTimeMeans;

    long double frameTime = 0;


    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();

        return 0;
    }

    window = SDL_CreateWindow("Mandlebrot :)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight, SDL_WINDOW_SHOWN);
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

    surface = SDL_CreateRGBSurface(0, screenWidth, screenHeight, 32, 0, 0, 0, 0);
    if (surface == NULL)
    {
        printf("Surface could not be created! SDL_Error: %s\n", SDL_GetError());

        SDL_FreeSurface(surface);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();

        return 0;
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

        return 0;
    }

    bool wKey = false, aKey = false, sKey = false, dKey = false,
        eKey = false, qKey = false, downKey = false, upKey = false;

    // OpenCL setup
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
    cl_queue_properties queue_properties = 0;
    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, &queue_properties, &err);

    cl_mem d_pixelArr = clCreateBuffer(context, CL_MEM_WRITE_ONLY, totalPixels * sizeof(int) * 3, NULL, &err);

    err = clEnqueueWriteBuffer(queue, d_pixelArr, CL_TRUE, 0, totalPixels * sizeof(int) * 3, pixelArr, 0, NULL, NULL);

    cl_program program = clCreateProgramWithSource(context, 1, &kernel_source, NULL, &err);
    clBuildProgram(program, 1, &device, NULL, NULL, NULL);

    char buildLog[16384];
    size_t buildLogSize;
    err = clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(buildLog), buildLog, &buildLogSize);
    if (err != CL_SUCCESS) {
        std::cerr << "\n\nError: Failed to retrieve build log!\n\n" << std::endl;
        exit(1);
    }
    DBOUT("\n\nBuild log:\n" << buildLog << "\n\n")

        cl_kernel kernel = clCreateKernel(program, "mandelbrot_kernel", &err);

    int frameCounter = 0;

    int* workQueue = new int[totalPixels];
    for (int i = 0; i < totalPixels; ++i) {
        workQueue[i] = i;
    }

    // Main render loop
    bool quit = false;
    while (!quit)
    {
        profilingPoints[0] = std::chrono::high_resolution_clock::now();

        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_w)
                {
                    // "w" key is pressed
                    wKey = true;
                }
                if (event.key.keysym.sym == SDLK_a)
                {
                    // "a" key is pressed
                    aKey = true;
                }
                if (event.key.keysym.sym == SDLK_s)
                {
                    // "s" key is pressed
                    sKey = true;
                }
                if (event.key.keysym.sym == SDLK_d)
                {
                    // "d" key is pressed
                    dKey = true;
                }
                if (event.key.keysym.sym == SDLK_e)
                {
                    // "e" key is pressed
                    eKey = true;
                }
                if (event.key.keysym.sym == SDLK_q)
                {
                    // "q" key is pressed
                    qKey = true;
                }
                if (event.key.keysym.sym == SDLK_DOWN)
                {
                    // "q" key is pressed
                    downKey = true;
                }
                if (event.key.keysym.sym == SDLK_UP)
                {
                    // "q" key is pressed
                    upKey = true;
                }
            }
            else if (event.type == SDL_KEYUP)
            {
                if (event.key.keysym.sym == SDLK_w)
                {
                    // "w" key is released
                    wKey = false;
                }
                if (event.key.keysym.sym == SDLK_a)
                {
                    // "a" key is released
                    aKey = false;
                }
                if (event.key.keysym.sym == SDLK_s)
                {
                    // "s" key is released
                    sKey = false;
                }
                if (event.key.keysym.sym == SDLK_d)
                {
                    // "d" key is released
                    dKey = false;
                }
                if (event.key.keysym.sym == SDLK_e)
                {
                    // "e" key is released
                    eKey = false;
                }
                if (event.key.keysym.sym == SDLK_q)
                {
                    // "q" key is released
                    qKey = false;
                }
                if (event.key.keysym.sym == SDLK_DOWN)
                {
                    // "q" key is pressed
                    downKey = false;
                }
                if (event.key.keysym.sym == SDLK_UP)
                {
                    // "q" key is pressed
                    upKey = false;
                }
            }
        }
        if (wKey == true) {
            position[1] -= moveSpeed * zoom * frameTime;
            //maxIterations = baseMaxIterations;
        }
        if (aKey == true) {
            position[0] -= moveSpeed * zoom * frameTime;
            //maxIterations = baseMaxIterations;
        }
        if (sKey == true) {
            position[1] += moveSpeed * zoom * frameTime;
            //maxIterations = baseMaxIterations;
        }
        if (dKey == true) {
            position[0] += moveSpeed * zoom * frameTime;
            //maxIterations = baseMaxIterations;
        }
        if (eKey == true) {
            zoom /= (1 + zoomSpeed * frameTime);
            //maxIterations = baseMaxIterations;
        }
        if (qKey == true) {
            zoom *= (1 + zoomSpeed * frameTime);
            //maxIterations = baseMaxIterations;
        }
        if (downKey == true) {
            if (maxIterations > maxIterationsFloor) {
                maxIterations /= (1 + 0.5 * frameTime);
            }
            DBOUT(maxIterations << "\n");
            //maxIterations = baseMaxIterations;
        }
        if (upKey == true) {
            maxIterations *= (1 + 0.5 * frameTime);
            DBOUT(maxIterations << "\n");
            //maxIterations = baseMaxIterations;
        }

        cl_mem d_workQueue = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(int) * totalPixels, workQueue, &err);

        int globalIndex = 0;
        cl_mem d_globalIndex = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(int), &globalIndex, &err);

        err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_pixelArr);
        err = clSetKernelArg(kernel, 1, sizeof(int), &screenWidth);
        err = clSetKernelArg(kernel, 2, sizeof(int), &screenHeight);
        err = clSetKernelArg(kernel, 3, sizeof(double), &zoom);
        err = clSetKernelArg(kernel, 4, sizeof(double), &position[0]);
        err = clSetKernelArg(kernel, 5, sizeof(double), &position[1]);
        err = clSetKernelArg(kernel, 6, sizeof(int), &maxIterations);
        err = clSetKernelArg(kernel, 7, sizeof(cl_mem), &d_workQueue);
        err = clSetKernelArg(kernel, 8, sizeof(cl_mem), &d_globalIndex);

        size_t global_work_size = 6400;
        size_t local_work_size = NULL;

        profilingPoints[1] = std::chrono::high_resolution_clock::now();

        // Execute the kernel
        clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);
        clFinish(queue);

        profilingPoints[2] = std::chrono::high_resolution_clock::now();

        // Transfer data from device to host
        err = clEnqueueReadBuffer(queue, d_pixelArr, CL_TRUE, 0, totalPixels * sizeof(int) * 3, pixelArr, 0, NULL, NULL);

        profilingPoints[3] = std::chrono::high_resolution_clock::now();

        uint32_t* pixels = (uint32_t*)surface->pixels;
        parallel_for(0, screenHeight, [&](int l) {
            for (int i = 0; i < screenWidth; i++) {
                pixels[l * screenWidth + i] = SDL_MapRGB(surface->format, pixelArr[l * screenWidth + i], pixelArr[totalPixels + l * screenWidth + i], pixelArr[totalPixels * 2 + l * screenWidth + i]);
            }
            });

        profilingPoints[4] = std::chrono::high_resolution_clock::now();

        if (showPalette) {
            for (int l = 0; l < screenHeight; l++) {
                for (int i = 0; i < screenWidth; i++) {
                    pixels[l * screenWidth + i] = SDL_MapRGB(surface->format, palette((double)l / screenWidth, 1)[0], palette((double)l / screenWidth, 1)[1], palette((double)l / screenWidth, 1)[2]);
                }
            }
        }

        SDL_RenderClear(renderer);

        SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);

        SDL_RenderCopy(renderer, texture, NULL, NULL);

        SDL_RenderPresent(renderer);

        profilingPoints[5] = std::chrono::high_resolution_clock::now();

        frameCounter++;

        int frameCountMean = 100;
        double meanFrameTime;
        frameTime = (long double)(chrono::duration_cast<chrono::microseconds>(profilingPoints[5] - profilingPoints[0]).count()) / 1000000;
        for (int i = 0; i < profilingTimeMeans.size(); i++) {
            profilingTimeMeans[i] += (long double)(chrono::duration_cast<chrono::microseconds>(profilingPoints[i + 1] - profilingPoints[i]).count()) / 1000000 / 100;
        }
        if (showFrameTime && frameCounter % 100 == 0) {
            meanFrameTime = profilingTimeMeans[0] + profilingTimeMeans[1] + profilingTimeMeans[2] + profilingTimeMeans[3] + profilingTimeMeans[4];
            DBOUT(endl << "-total frame time: " << meanFrameTime << endl);
            DBOUT("time spent sending data to GPU & using GPU: " << profilingTimeMeans[1] << "(" << round((profilingTimeMeans[1] / meanFrameTime) * 100) << "%)" << endl);
            DBOUT("time spent collecting data from GPU: " << profilingTimeMeans[2] << "(" << round((profilingTimeMeans[2] / meanFrameTime) * 100) << "%)" << endl);
            DBOUT("time spent getting image ready: " << profilingTimeMeans[3] << "(" << round((profilingTimeMeans[3] / meanFrameTime) * 100) << "%)" << endl);
            DBOUT("time spent updating screen: " << profilingTimeMeans[4] << "(" << round((profilingTimeMeans[4] / meanFrameTime) * 100) << "%)" << endl);
            DBOUT("time spent on other: " << profilingTimeMeans[0] << "(" << round((profilingTimeMeans[0] / meanFrameTime) * 100) << "%)" << endl);
            for (int i = 0; i < profilingTimeMeans.size(); i++) {
                profilingTimeMeans[i] = 0;
            }
        }
        clReleaseMemObject(d_globalIndex);
        clReleaseMemObject(d_workQueue);
    }

    delete[] devices;
    delete[] pixelArr;
    delete[] workQueue;

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    clReleaseMemObject(d_pixelArr);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    return 0;
}