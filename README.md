# Real-Time Mandelbrot and Julia Set Renderer

This project is a real-time renderer for the Mandelbrot and Julia sets, implemented in C++ with GPU acceleration using OpenCL and graphics rendering using SDL2. The application allows interactive exploration of these fractals with smooth zooming and panning capabilities.

## Features

- **Real-Time Rendering**: Leverages OpenCL for parallel computation on the GPU, enabling high-performance rendering.
- **Interactive Exploration**: Navigate the fractals using keyboard controls for zooming and panning.
- **Dynamic Julia Set**: The Julia set updates in real-time by indexing the Mandelbrot set at the position of your cursor.
- **Dual Display**: Renders both the Mandelbrot set and its corresponding Julia set side by side.
- **Adjustable Parameters**: Modify iteration counts and coloring schemes in real time.
- **High Resolution Support**: Automatically adapts to native screen resolutions, with fullscreen mode available.

## Technologies Used

- **C++**: Core programming language for the application.
- **OpenCL**: GPU acceleration for fractal computations.
- **SDL2**: Handles window management, rendering, and input.
- **SDL_ttf**: Renders text for UI elements like FPS counters.
- **OpenMP**: Utilized for multi-threading in certain computations.

## Controls

- **Navigation**:
  - ```W``` / ```A``` / ```S``` / ```D```: Pan up, left, down, and right.
  - ```Q```: Zoom in.
  - ```E```: Zoom out.
  - ```Left Shift```: Increase movement and zoom speed while held.
- **Iteration Control**:
  - ```Up Arrow```: Increase maximum iterations.
  - ```Down Arrow```: Decrease maximum iterations.
- **Mouse Interaction**:
  - ```Left Click```: Update the Julia set by clicking on the Mandelbrot set (preferably while zoomed out on the mandelbrot set).
- **Color Scheme**:
  - ```Spacebar```: Toggle between different coloring schemes.

## Running the Application

### Using the Pre-Built Executable (Windows)

For users who prefer not to compile the source code, a pre-built executable is available.

1. **Download the Release**

   - Navigate to the ```Releases``` section of this repository.
   - Download the latest release ZIP file.

2. **Extract the Files**

   - Right-click on the downloaded ZIP file and select ```Extract All...```.
   - Choose a destination folder and extract the contents.

3. **Run the Application**

   - Open the extracted ```release``` folder.
   - Double-click on ```Mandelbrot.exe``` to run the application.

   **Note**: Ensure that all files in the release folder remain together, as the executable depends on accompanying resources like DLLs, fonts, and images.

### Building and Running from Source

If you prefer to compile and run the application from source, follow the instructions below.

#### Prerequisites

- **OpenCL SDK**: Ensure your GPU drivers support OpenCL and the SDK is installed.
- **C++ Compiler**: Compatible with C++11 or higher.

### Building on Windows (Using Visual Studio)

1. **Clone the Repository**:
   ```
   git clone https://github.com/Joshua678/mandelbrot-renderer.git
   ```
2. **Open the Project**: Load the `.sln` file in Visual Studio.
3. **Build the Solution**: Simply build the project. All necessary include directories and libraries are configured using relative paths.

## Screenshots

### Mandelbrot Set

![Mandelbrot Set Screenshot](Example%20images/Mandelbrot1.PNG)
![Mandelbrot Set Screenshot](Example%20images/Mandelbrot2.PNG)
![Mandelbrot Set Screenshot](Example%20images/Mandelbrot3.PNG)

### Julia Set

![Mandelbrot Set Screenshot](Example%20images/Mandelbrot4.PNG)
![Mandelbrot Set Screenshot](Example%20images/Mandelbrot5.PNG)

## Project Structure

- **```main.cpp```**: Contains the entry point and main loop of the application.
- **```fractals.h```**: Header file defining the ```fractal```, ```mandelbrotSet```, and ```juliaSet``` classes.
- **```Mandelbrot Kernel.cl```**: OpenCL kernel source for computing the Mandelbrot set.
- **```Julia Kernel.cl```**: OpenCL kernel source for computing the Julia set.
- **```Resources/```**: Contains assets like fonts and background images.

## Customization

- **Resolution and Fullscreen**: Modify ```screenWidth```, ```screenHeight```, and ```fullscreen``` variables at the top of ```main.cpp```.
- **Frame Rate Cap**: Adjust ```frameRateCap``` to limit the maximum FPS.

## Notes

- **Platform Compatibility**: While the code is geared towards Windows (```<Windows.h>``` is included), it can be adapted for Linux by removing Windows-specific headers and adjusting library links.
- **Error Handling**: OpenCL error messages are provided for easier debugging.
- **Performance**: The application uses double buffering and efficient memory management for smooth rendering.

## Future Improvements

- **Cross-Platform Support**: Improve compatibility with macOS and Linux.
- **User Interface**: Add a graphical menu for adjusting settings.
- **Zoom Enhancements**: Implement infinite zoom capabilities with arbitrary precision arithmetic on the GPU.
- **Optimization**: A potential optimization/improvement could be by creating a point cloud, allowing for constant fps through upscaling, with downscaling while remaining stationary to improve image quality.
- **Interpolation**: Interpolation could be used to improve image quality, especially in combination with a point cloud.

## License

This project is licensed under the Apache License 2.0—see the ```LICENSE``` file for details.

---

Feel free to contribute to this project by opening issues or submitting pull requests.
