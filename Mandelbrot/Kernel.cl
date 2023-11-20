inline int palette(double pos, double rateOfChange, int colour) { //pos between 1 and 0
    if (colour == 0) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + 1) + 127.5); }
    if (colour == 1) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + (2.0 / 3.0) * M_PI + 1) + 127.5); }
    if (colour == 2) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + (4.0 / 3.0) * M_PI + 1) + 127.5); }
    return 0;
}
__kernel void mandelbrot_kernel(__global int* pixelArr, int screenWidth, int screenHeight, double zoom, 
double position_x, double position_y, int maxIterations, __global const int* workQueue, __global int* globalIndex) {
    int i = get_global_id(0);
    int totalPixels = screenWidth*screenHeight;
    int idx = atomic_inc(globalIndex);
    double temp;
    double past[2] = { 0,0 };
    double temp2;
    double temp3;
    double xold = 0;
    double yold = 0;
    int period = 0;

    while (idx < totalPixels) {
        int pixelIndex = workQueue[idx];
        int x = pixelIndex % screenWidth;
        int y = pixelIndex / screenHeight;
        int iteration = 0;
        double z[2] = { 0,0 };
        double aspectRatio = (double)screenWidth/screenHeight;
        double c[2] = { ((double)x / screenWidth - 0.5) * zoom * aspectRatio + position_x, ((double)y / screenWidth - 0.5) * zoom + position_y};

        //stops when abs(z)>=8 (that is, when pixel coordinates are not on the mandlebrot set)
        while (z[0] * z[0] + z[1] * z[1] < 64 && ++iteration < maxIterations) {
            //z = z^2 + c
            temp = 2 * z[0] * z[1] + c[1];
            z[0] = z[0] * z[0] - z[1] * z[1] + c[0];
            z[1] = temp;

            if(z[0] == xold && z[1] == yold){
                iteration = maxIterations;
                break;
            }

            period++;
            if(period > 50){
                period = 0;
                xold = z[0];
                yold = z[1];
            }
        }

        double rationalIteration = iteration + 2 - log(log(z[0] * z[0] + z[1] * z[1])) / log((double)2);
        if(iteration == maxIterations){
            pixelArr[pixelIndex] = 0; //red
            pixelArr[totalPixels + pixelIndex] = 0; //green
            pixelArr[totalPixels * 2 + pixelIndex] = 0; //blue
        }
        else{
            pixelArr[pixelIndex] = palette(rationalIteration, 0.01, 0); //red
            pixelArr[totalPixels + pixelIndex] = palette(rationalIteration, 0.01, 1); //green
            pixelArr[totalPixels * 2 + pixelIndex] = palette(rationalIteration, 0.01, 2); //blue
        }
        idx = atomic_inc(globalIndex);
    }
}