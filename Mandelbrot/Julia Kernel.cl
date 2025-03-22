inline int palette(double pos, double rateOfChange, int colour) { //pos between 1 and 0
    pos = log(pos);
    if (colour == 0) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + 1) + 127.5); }
    if (colour == 1) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + (2.0 / 3.0) * M_PI + 1) + 127.5); }
    if (colour == 2) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + (4.0 / 3.0) * M_PI + 1) + 127.5); }
    return 0;
}

typedef struct {
    double real;
    double imag;
} complexDouble;

__kernel void juliaKernel(__global int* pixelArr, int screenWidth, int screenHeight, double zoom,
    double positionX, double positionY, int maxIterations, __global const int* workQueue, __global int* globalIndex, int colouringScheme, double cX, double cY) {
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
            complexDouble z = { ((double)x / screenWidth - 0.5) * zoom * aspectRatio + positionX, ((double)y / screenWidth - 0.5) * zoom + positionY };
            complexDouble zOld = { 0,0 };
            const complexDouble complexPoint = { cX, cY };
            int boundedThreshold = 8 * 8;
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
                pixelArr[pixelIndex] = 0; //red
                pixelArr[totalPixels + pixelIndex] = 0; //green
                pixelArr[totalPixels * 2 + pixelIndex] = 0; //blue
            }
            else {
                pixelArr[pixelIndex] = palette(rationalIteration, 1, 0); //red
                pixelArr[totalPixels + pixelIndex] = palette(rationalIteration, 1, 1); //green
                pixelArr[totalPixels * 2 + pixelIndex] = palette(rationalIteration, 1, 2); //blue
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
            complexDouble z = { ((double)x / screenWidth - 0.5) * zoom * aspectRatio + positionX, ((double)y / screenWidth - 0.5) * zoom + positionY };
            complexDouble zOld = { 0,0 };
            const complexDouble complexPoint = { cX, cY };
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
                pixelArr[pixelIndex] = 0; // red
                pixelArr[totalPixels + pixelIndex] = 0; // green
                pixelArr[totalPixels * 2 + pixelIndex] = 0; // blue
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


                pixelArr[pixelIndex] = 255 * t; //red
                pixelArr[totalPixels + pixelIndex] = 255 * t; //green
                pixelArr[totalPixels * 2 + pixelIndex] = 255 * t; //blue
            }
            idx = atomic_inc(globalIndex);
        }
    }
}