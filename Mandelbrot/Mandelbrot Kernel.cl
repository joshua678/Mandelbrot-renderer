inline int palette(double pos, double rateOfChange, int colour) { //pos between 1 and 0
    pos = log(pos);
    if (colour == 0) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + 1) + 127.5); }
    if (colour == 1) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + (2.0 / 3.0) * M_PI + 1) + 127.5); }
    if (colour == 2) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + (4.0 / 3.0) * M_PI + 1) + 127.5); }
    return 0;
}

__constant uint colorGradient[8] = {
    0xFF000000u, // black
    0xFF0000FFu, // blue
    0xFF00FFFFu, // cyan
    0xFF00FF00u, // green
    0xFFFFFF00u, // yellow
    0xFFFF0000u, // red
    0xFFFF00FFu, // magenta
    0xFFFFFFFFu  // white
};
#define GRADIENT_SIZE 8

inline uint interpolateColor(uint c1, uint c2, float fraction)
{
    uint A1 = (c1 >> 24) & 0xFF;
    uint R1 = (c1 >> 16) & 0xFF;
    uint G1 = (c1 >> 8) & 0xFF;
    uint B1 = (c1) & 0xFF;

    uint A2 = (c2 >> 24) & 0xFF;
    uint R2 = (c2 >> 16) & 0xFF;
    uint G2 = (c2 >> 8) & 0xFF;
    uint B2 = (c2) & 0xFF;

    uint A = (uint)((1.0f - fraction) * A1 + fraction * A2);
    uint R = (uint)((1.0f - fraction) * R1 + fraction * R2);
    uint G = (uint)((1.0f - fraction) * G1 + fraction * G2);
    uint B = (uint)((1.0f - fraction) * B1 + fraction * B2);

    return (A << 24) | (R << 16) | (G << 8) | B;
}

typedef struct {
    double real;
    double imag;
} complexDouble;

__kernel void mandelbrotKernel(__global uint* pixelArr, int screenWidth, int screenHeight, double zoom,
    double positionX, double positionY, int maxIterations, __global const int* workQueue, __global int* globalIndex, int colouringScheme) {
    if (colouringScheme == 0) {
        int i = get_global_id(0);
        int totalPixels = screenWidth * screenHeight;
        int idx = atomic_inc(globalIndex);
        double temp;

        while (idx < totalPixels) {
            int pixelIndex = workQueue[idx];
            int x = pixelIndex % screenWidth;
            int y = pixelIndex / screenHeight;
            double aspectRatio = (double)screenWidth / screenHeight;
            int iteration = 0;
            complexDouble z = { 0,0 };
            complexDouble zOld = { 0,0 };
            const complexDouble complexPoint = { ((double)x / screenWidth - 0.5) * zoom * aspectRatio + positionX, ((double)y / screenWidth - 0.5) * zoom + positionY };
            int boundedThreshold = 8*8;
            int period = 0;

            // stops when abs(z) >= sqrt(boundedThreshold), at which point we estimate that z is unbounded at complexPoint, so that complexPoint is not in the mandelbrot set
            while (z.real * z.real + z.imag * z.imag < boundedThreshold && ++iteration < maxIterations) {
                // z = z^2 + c
                temp = 2 * z.real * z.imag + complexPoint.imag;
                z.real = z.real * z.real - z.imag * z.imag + complexPoint.real;
                z.imag = temp;

                if (z.real == zOld.real && z.imag == zOld.imag) {
                    iteration = maxIterations;
                    break;
                }

                if (period++ > 50) {
                    period = 0;
                    zOld.real = z.real;
                    zOld.imag = z.imag;
                }
            }

            double rationalIteration = iteration + 2 - log(log(z.real * z.real + z.imag * z.imag)) / log((double)2);
            if (iteration == maxIterations) {
                pixelArr[pixelIndex] = ((uint)(255) << 24); // black
            }
            else {
                pixelArr[pixelIndex] = ((uint)(255) << 24)
                    | ((uint)(palette(rationalIteration, 1, 0)) << 16) // red
                    | ((uint)(palette(rationalIteration, 1, 1)) << 8) // green
                    | ((uint)(palette(rationalIteration, 1, 2)) << 0); // blue
            }
            idx = atomic_inc(globalIndex);
        }
    }
    if (colouringScheme == 1) {
        int i = get_global_id(0);
        int totalPixels = screenWidth * screenHeight;
        int idx = atomic_inc(globalIndex);
        double temp;

        double pi = 3.14159265358979323846;
        double h2 = 1.5;
        double angle = 45;
        double radians = angle * (pi / 180);
        complexDouble v = { cos(radians), sin(radians) };
        double R = 100;

        while (idx < totalPixels) {
            int pixelIndex = workQueue[idx];
            int x = pixelIndex % screenWidth;
            int y = pixelIndex / screenHeight;
            double aspectRatio = (double)screenWidth / screenHeight;
            int iteration = 0;
            complexDouble z = { 0,0 };
            complexDouble zOld = { 0,0 };
            const complexDouble complexPoint = { ((double)x / screenWidth - 0.5) * zoom * aspectRatio + positionX, ((double)y / screenWidth - 0.5) * zoom + positionY };
            int boundedThreshold = 8 * 8;
            int period = 0;

            complexDouble dc = { 1,0 };
            complexDouble der = { 1,0 };

            // stops when abs(z) >= sqrt(boundedThreshold), at which point we estimate that z is unbounded at complexPoint, so that complexPoint is not in the mandelbrot set
            while (z.real * z.real + z.imag * z.imag < boundedThreshold && ++iteration < maxIterations) {
                // der = der*2*z + dc
                temp = (der.real * z.imag + der.imag * z.real) * 2 + dc.imag;
                der.real = (der.real * z.real - der.imag * z.imag) * 2 + dc.real;
                der.imag = temp;

                // z = z^2 + c
                temp = 2 * z.real * z.imag + complexPoint.imag;
                z.real = z.real * z.real - z.imag * z.imag + complexPoint.real;
                z.imag = temp;

                if (z.real == zOld.real && z.imag == zOld.imag) {
                    iteration = maxIterations;
                    break;
                }

                if (period++ > 50) {
                    period = 0;
                    zOld.real = z.real;
                    zOld.imag = z.imag;
                }
            }

            if (iteration == maxIterations) {
                pixelArr[pixelIndex] = ((uint)(255) << 24); // black
            }
            else {
                // u = z/der
                double denom = der.real * der.real + der.imag * der.imag;
                complexDouble u = {
                    (z.real * der.real + z.imag * der.imag) / denom,
                    (z.imag * der.real - z.real * der.imag) / denom
                };

                // temp = abs(u)
                temp = sqrt(u.real * u.real + u.imag * u.imag);

                // u = u/abs(u)
                u.real = u.real / temp;
                u.imag = u.imag / temp;

                double t = u.real * v.real - u.imag * v.imag + h2;

                t /= 1 + h2;

                if (t < 0) { t = 0; }

                pixelArr[pixelIndex] = ((uint)(255) << 24)
                    | ((uint)(t * 255) << 16) // red
                    | ((uint)(t * 255) << 8) // green
                    | ((uint)(t * 255) << 0); // blue
            }
            idx = atomic_inc(globalIndex);
        }
    }
}