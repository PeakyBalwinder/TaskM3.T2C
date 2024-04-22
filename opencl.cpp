#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <CL/cl.hpp>

using namespace std;

// Define the Point structure to represent data points
struct Point {
    float x, y;
};

// Function to generate random points for testing
void generateRandomPoints(vector<Point>& points, int numPoints) {
    points.clear();
    srand(time(nullptr));
    for (int i = 0; i < numPoints; ++i) {
        float x = static_cast<float>(rand()) / RAND_MAX * 100.0f; // Random x-coordinate between 0 and 100
        float y = static_cast<float>(rand()) / RAND_MAX * 100.0f; // Random y-coordinate between 0 and 100
        points.push_back({x, y});
    }
}

// Kernel source code for K-Means clustering
const char* kMeansKernelSource = R"(
_kernel void kMeansClustering(_global float2* points, __global float2* centroids, int numPoints, int numClusters) {
    int gid = get_global_id(0);
    float minDistance = INFINITY;
    int minIndex = 0;

    // Calculate distance to each centroid and find the closest one
    for (int i = 0; i < numClusters; ++i) {
        float dx = points[gid].x - centroids[i].x;
        float dy = points[gid].y - centroids[i].y;
        float distance = sqrt(dx * dx + dy * dy);
        if (distance < minDistance) {
            minDistance = distance;
            minIndex = i;
        }
    }

    // Assign the point to the closest centroid
    points[gid] = centroids[minIndex];
}
)";

// Function to check OpenCL errors
void checkOpenCLError(cl_int error, const char* operation) {
    if (error != CL_SUCCESS) {
        cerr << "Error during " << operation << ": " << error << endl;
        exit(EXIT_FAILURE);
    }
}

// K-Means clustering algorithm using OpenCL
void kMeansClusteringOpenCL(vector<Point>& points, vector<Point>& centroids, int numClusters) {
    // Initialize OpenCL
    vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    checkOpenCLError(!platforms.empty() ? CL_SUCCESS : -1, "getting platforms");
    cl::Platform platform = platforms.front();

    vector<cl::Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_CPU, &devices);
    checkOpenCLError(!devices.empty() ? CL_SUCCESS : -1, "getting devices");
    cl::Device device = devices.front();

    cl::Context context(device);
    cl::CommandQueue queue(context, device);

    // Load kernel source code
    cl::Program::Sources sources;
    sources.push_back({kMeansKernelSource, strlen(kMeansKernelSource)});
    cl::Program program(context, sources);
    checkOpenCLError(program.build("-cl-std=CL1.2"), "building kernel");

    // Create buffers for points and centroids
    cl::Buffer pointsBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(Point) * points.size(), points.data());
    cl::Buffer centroidsBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(Point) * centroids.size(), centroids.data());

    // Create kernel and set arguments
    cl::Kernel kernel(program, "kMeansClustering");
    checkOpenCLError(kernel.setArg(0, pointsBuffer), "setting kernel argument 0");
    checkOpenCLError(kernel.setArg(1, centroidsBuffer), "setting kernel argument 1");
    checkOpenCLError(kernel.setArg(2, static_cast<int>(points.size())), "setting kernel argument 2");
    checkOpenCLError(kernel.setArg(3, numClusters), "setting kernel argument 3");

    // Execute the kernel
    checkOpenCLError(queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(points.size()), cl::NullRange), "enqueueing kernel");

    // Read back the updated points buffer
    checkOpenCLError(queue.enqueueReadBuffer(pointsBuffer, CL_TRUE, 0, sizeof(Point) * points.size(), points.data()), "reading buffer");
}

int main() {
    int numPoints = 1000;
    int numClusters = 5;

    // Generate random points and initial centroids
    vector<Point> points, centroids(numClusters);
    generateRandomPoints(points, numPoints);
    generateRandomPoints(centroids, numClusters); // Initialize centroids randomly

    // Perform K-Means clustering using OpenCL
    auto start = chrono::high_resolution_clock::now();
    kMeansClusteringOpenCL(points, centroids, numClusters);
    auto end = chrono::high_resolution_clock::now();

    // Calculate execution time
    chrono::duration<double, milli> duration_ms = end - start;
    cout << "Execution time: " << duration_ms.count() << " milliseconds" << endl;

    return 0;
}
