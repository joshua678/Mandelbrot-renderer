inline int palette(double pos, double rateOfChange, int colour) { //pos between 1 and 0
    pos = log(pos);
    if (colour == 0) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + 1) + 127.5); }
    if (colour == 1) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + (2.0 / 3.0) * M_PI + 1) + 127.5); }
    if (colour == 2) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + (4.0 / 3.0) * M_PI + 1) + 127.5); }
    return 0;
}
__kernel void mandelbrotKernel(__global int* pixelArr, int screenWidth, int screenHeight, double zoom,
    double position_x, double position_y, int maxIterations, __global const int* workQueue, __global int* globalIndex, int colouringScheme) {
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
            double z[2] = { 0,0 };
            double c[2] = { ((double)x / screenWidth - 0.5) * zoom * aspectRatio + position_x, ((double)y / screenWidth - 0.5) * zoom + position_y };
            double xold = 0;
            double yold = 0;
            int period = 0;

            //stops when abs(z)>=8 (that is, when pixel coordinates are not on the mandlebrot set)
            while (z[0] * z[0] + z[1] * z[1] < 64 && ++iteration < maxIterations) {
                //z = z^2 + c
                temp = 2 * z[0] * z[1] + c[1];
                z[0] = z[0] * z[0] - z[1] * z[1] + c[0];
                z[1] = temp;

                if (z[0] == xold && z[1] == yold) {
                    iteration = maxIterations;
                    break;
                }

                if (period++ > 50) {
                    period = 0;
                    xold = z[0];
                    yold = z[1];
                }
            }

            double rationalIteration = iteration + 2 - log(log(z[0] * z[0] + z[1] * z[1])) / log((double)2);
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
        double v[2] = { cos(radians), sin(radians) };
        double R = 100;

        while (idx < totalPixels) {
            int pixelIndex = workQueue[idx];
            int x = pixelIndex % screenWidth;
            int y = pixelIndex / screenHeight;
            double aspectRatio = (double)screenWidth / screenHeight;
            int iteration = 0;
            double z[2] = { 0,0 };
            double c[2] = { ((double)x / screenWidth - 0.5) * zoom * aspectRatio + position_x, ((double)y / screenWidth - 0.5) * zoom + position_y };
            double xold = 0;
            double yold = 0;
            int period = 0;

            double dc[2] = { 1,0 };
            double der[2] = { 1,0 };

            //stops when abs(z)>=8 (that is, when pixel coordinates are not on the mandlebrot set)
            while (z[0] * z[0] + z[1] * z[1] < 64 && ++iteration < maxIterations) {
                //der = der*2*z + dc
                temp = (der[0] * z[1] + der[1] * z[0]) * 2 + dc[1];
                der[0] = (der[0] * z[0] - der[1] * z[1]) * 2 + dc[0];
                der[1] = temp;

                //z = z^2 + c
                temp = 2 * z[0] * z[1] + c[1];
                z[0] = z[0] * z[0] - z[1] * z[1] + c[0];
                z[1] = temp;

                if (z[0] == xold && z[1] == yold) {
                    iteration = maxIterations;
                    break;
                }

                if (period++ > 50) {
                    period = 0;
                    xold = z[0];
                    yold = z[1];
                }
            }

            if (iteration == maxIterations) {
                pixelArr[pixelIndex] = 0; //red
                pixelArr[totalPixels + pixelIndex] = 0; //green
                pixelArr[totalPixels * 2 + pixelIndex] = 0; //blue
            }
            else {
                //u = z/der
                double denom = der[0] * der[0] + der[1] * der[1];
                double u[2] = {
                    (z[0] * der[0] + z[1] * der[1]) / denom,
                    (z[1] * der[0] - z[0] * der[1]) / denom
                };

                //temp = abs(u)
                temp = sqrt(u[0] * u[0] + u[1] * u[1]);

                //u = u/abs(u)
                u[0] = u[0] / temp;
                u[1] = u[1] / temp;

                double t = u[0] * v[0] - u[1] * v[1] + h2;

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