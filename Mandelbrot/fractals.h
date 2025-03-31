#pragma once

#include <random>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <tuple>
#include <memory>
#include <cstring>

using namespace std;

cl_int err;

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

    string type = "fractal";
    SDL_Surface* surface = NULL;
    SDL_Texture* texture = NULL;
    uint32_t* writePixelArr;
    uint32_t* readPixelArr;
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
    cl_mem d_points;

    fractal(int newWidth, int newHeight, SDL_Renderer* renderer, SDL_Window* window, cl_context context, cl_device_id device)
        : width(newWidth), height(newHeight) {

        writePixelArr = new uint32_t[width * height];
        readPixelArr = new uint32_t[width * height];
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
        d_readPixelArr = clCreateBuffer(context, CL_MEM_READ_ONLY, width * height * sizeof(uint32_t), NULL, &err);
        d_writePixelArr = clCreateBuffer(context, CL_MEM_WRITE_ONLY, width * height * sizeof(uint32_t), NULL, &err);

        err = clEnqueueWriteBuffer(queue, d_readPixelArr, CL_TRUE, 0, width * height * sizeof(uint32_t), readPixelArr, 0, NULL, NULL);
        err = clEnqueueWriteBuffer(queue, d_writePixelArr, CL_TRUE, 0, width * height * sizeof(uint32_t), writePixelArr, 0, NULL, NULL);

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

    /*void mapRGBReadPixelArr(uint32_t* pixelsToSet)
    {
        // A 3x3 Gaussian-like kernel
        // Typically, you want to sum to 16 (1+2+1 + 2+4+2 + 1+2+1)
        // You can tweak these values or use a true Gaussian distribution
        static const int kernel[3][3] = {
            { 1, 2, 1 },
            { 2, 8, 2 },
            { 1, 2, 1 }
        };
        static const int kernelWeightSum = 16; // sum of all the above values

#pragma omp parallel for
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                int sumR = 0;
                int sumG = 0;
                int sumB = 0;
                int totalWeight = 0;

                // Visit the 3x3 neighborhood
                for (int dy = -1; dy <= 1; dy++)
                {
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        int ny = y + dy;
                        int nx = x + dx;

                        // Check boundaries
                        if (ny >= 0 && ny < height && nx >= 0 && nx < width)
                        {
                            // Get the kernel weight for the (dy, dx) position
                            int kw = kernel[dy + 1][dx + 1];

                            // Accumulate weighted R, G, B
                            sumR += readPixelArr[ny * width + nx] * kw;
                            sumG += readPixelArr[width * height + ny * width + nx] * kw;
                            sumB += readPixelArr[2 * width * height + ny * width + nx] * kw;

                            // Accumulate total weight
                            totalWeight += kw;
                        }
                    }
                }

                // If for some reason totalWeight stays 0 (e.g. on an empty image),
                // we can fallback to the pixel itself or clamp at 1.
                if (totalWeight == 0) {
                    totalWeight = 1;
                }

                // Compute averages
                Uint8 avgR = static_cast<Uint8>(sumR / totalWeight);
                Uint8 avgG = static_cast<Uint8>(sumG / totalWeight);
                Uint8 avgB = static_cast<Uint8>(sumB / totalWeight);

                // Set pixel in output
                pixelsToSet[y * width + x] = SDL_MapRGB(surface->format, avgR, avgG, avgB);
            }
        }
    }*/

    void writeBuffers() {

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


        width = newWidth;
        height = newHeight;
        writePixelArr = new uint32_t[width * height];
        readPixelArr = new uint32_t[width * height];
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
        d_readPixelArr = clCreateBuffer(context, CL_MEM_READ_ONLY, width * height * sizeof(uint32_t), NULL, &err);
        d_writePixelArr = clCreateBuffer(context, CL_MEM_WRITE_ONLY, width * height * sizeof(uint32_t), NULL, &err);

        err = clEnqueueWriteBuffer(queue, d_readPixelArr, CL_TRUE, 0, width * height * sizeof(uint32_t), readPixelArr, 0, NULL, NULL);
        err = clEnqueueWriteBuffer(queue, d_writePixelArr, CL_TRUE, 0, width * height * sizeof(uint32_t), writePixelArr, 0, NULL, NULL);

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
    void setKernelArgs(cl_kernel& kernel) {

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