#pragma once

//const char* kernel_source =
//"inline int palette(double pos, double rateOfChange, int colour) {\n" //pos between 1 and 0
//"	if (colour == 0) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + 1) + 127.5); }\n"
//"	if (colour == 1) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + (2.0 / 3.0) * M_PI + 1) + 127.5); }\n"
//"	if (colour == 2) { return (int)round(127.5 * sin(rateOfChange * 2 * M_PI * pos + (4.0 / 3.0) * M_PI + 1) + 127.5); }\n"
//"	return 0;"
//"}\n"
//"__kernel void mandelbrot_kernel(__global int* pixelArr, int screenWidth, int screenHeight, double zoom, double position_x, double position_y, int maxIterations, __global const int* workQueue, __global int* globalIndex) {\n"
//"   int i = get_global_id(0);\n"
//"   int totalPixels = screenWidth*screenHeight;\n"
//"   int idx = atomic_inc(globalIndex);\n"
//"   double temp;\n"
//"   double past[2] = { 0,0 };\n"
//"   double temp2;\n"
//"   double temp3;\n"
//
//"   while (idx < totalPixels) {\n"
//"   int pixelIndex = workQueue[idx];\n"
//"   int x = pixelIndex % screenWidth;\n"
//"   int y = pixelIndex / screenHeight;\n"
//"   int iteration = 0;\n"
//"   double z[2] = { 0,0 };\n"
//"   double aspectRatio = (double)screenWidth/screenHeight;\n"
//"   double c[2] = { ((double)x / screenWidth - 0.5) * zoom * aspectRatio + position_x, ((double)y / screenWidth - 0.5) * zoom + position_y};\n"
////stops when abs(z)>=8 (that is, when pixel coordinates are not on mandlebrot set)
//"   while (z[0] * z[0] + z[1] * z[1] < 64 && ++iteration < maxIterations >> 1) {\n"
////z = (z^2 + c)^2 + c where z and c are complex numbers to estimate z = z^2 + c (because we want to remove loops in low level code)
//"       past[0] = z[0];\n"
//"       past[1] = z[1];\n"
//"       temp2 = z[0] * z[0] - z[1] * z[1] + c[0];\n"
//"       temp3 = 2 * z[0] * z[1] + c[1];\n"
//"       temp = 2 * temp2 * temp3 + c[1];\n"
//"       z[0] = temp2 * temp2 - temp3 * temp3 + c[0];\n"
//"       z[1] = temp;\n"
//"   }\n"
//"   z[0] = past[0];\n"
//"   z[1] = past[1];\n"
//"   iteration = iteration * 2;\n"
//"   while (z[0] * z[0] + z[1] * z[1] < 64 && ++iteration < maxIterations) {\n"
////        //z = z^2 + c
//"       temp = 2 * z[0] * z[1] + c[1];\n"
//"       z[0] = z[0] * z[0] - z[1] * z[1] + c[0];\n"
//"       z[1] = temp;\n"
//"   }\n"
//"   double rationalIteration = iteration + 2 - log(log(z[0] * z[0] + z[1] * z[1])) / log((double)2);\n"
//"   pixelArr[pixelIndex] = palette(rationalIteration, 0.01, 0);\n" //red
//"   pixelArr[totalPixels + pixelIndex] = palette(rationalIteration, 0.01, 1);\n" //green
//"   pixelArr[totalPixels * 2 + pixelIndex] = palette(rationalIteration, 0.01, 2);\n" //blue
//"   idx = atomic_inc(globalIndex);\n"
//"   }\n"
//"}\n";