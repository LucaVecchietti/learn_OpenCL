/**
 * Mini Benchmark for OpenCL
 * 
 * Read the device and the platform information. Select automaticaly the device to be used for the benchmark.
 * The benchmark will then run a series of tests to measure the performance of the selected device.
 * The results will be displayed on the console.
 * 
 * The benchmark says which is the best performing device and the best configuration for it and for the kernel.
 */

#define CL_TARGET_OPENCL_VERSION 200

#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
kernels to tests. Add the kernel source code or paths to the kernel files here.
*/

#define KERNEL_SOURCE_PATH "minibanchmark/kernels/";

#define KERNEL_SOURCE_ADD "minibanchmark/kernels/add.cl"
#define KERNEL_SOURCE_SUB "minibanchmark/kernels/sub.cl"
#define KERNEL_SOURCE_MUL "minibanchmark/kernels/mul.cl"
#define KERNEL_SOURCE_DIV "minibanchmark/kernels/div.cl"
#define KERNEL_SOURCE_MAX "minibanchmark/kernels/max.cl"

/*
Functions to GET and print device and platform information based on type.
*/

static void print_device_info_uint(cl_device_id device, cl_device_info param, const char *param_desc);
static void print_device_info_string(cl_device_id device, cl_device_info param, const char *param_desc);
static void print_device_info_bool(cl_device_id device, cl_device_info param, const char *param_desc);
static void print_device_info_cl_exec_capabilities(cl_device_id device, cl_device_info param, const char *param_desc);
static void print_device_info_ulong(cl_device_id device, cl_device_info param, const char *param_desc);
static void print_device_info_size_t(cl_device_id device, cl_device_info param, const char *param_desc);
static void print_device_info_uint(cl_device_id device, cl_device_info param, const char *param_desc);

static void print_platform_info_string(cl_platform_id platform, cl_platform_info param, const char *param_desc);
static void print_platform_info_ulong(cl_platform_id platform, cl_platform_info param, const char *param_desc);

/**
 * Get the number of available platforms.
 * 
 * @return The number of available platforms.
 */
static unsigned int get_platform_num(void);

/**
 * Get the platforms available for OpenCL.
 * 
 * @param platforms Pointer to an array where the platform IDs will be stored.
 * @param num_platforms Pointer to a variable where the number of available platforms will be stored.
 * 
 * @note Call the function get_platform_num() to determine the number of available platforms before calling this function.
 * 
 * @return void
 */
static void get_platforms(cl_platform_id *platforms, cl_uint *num_platforms);

/**
 * Get the number of device available for OpenCL by platform.
 *
 * @param platform The platform for which to query the number of devices.
 *
 * @return The number of available OpenCL devices.
 */
static unsigned int get_device_num(cl_platform_id platform);

/**
 * Selects the devices that can run CL kernels.
 * 
 * @param platform The platform for which to select devices.
 * @param devices Pointer to an array where the selected device IDs will be stored.
 * @param num_devices Pointer to a variable where the number of selected devices will be stored.
 * 
 * @note Call the function get_device_num() to determine the number of available devices before calling this function.
 * 
 * @return void
 */
static void get_devices(cl_platform_id platform, cl_device_id *devices, cl_uint *num_devices);

/**
 * Create an OpenCL context for a given platform and device.
 *
 * @param platform The platform for which to create the context.
 * @param device The device for which to create the context.
 *
 * @return The created OpenCL context, or NULL if the context creation failed.
 */
static cl_context create_context(cl_platform_id platform, cl_device_id device);

/**
 * Create a Command Queue for a given OpenCL context and device.
 *
 * @param context The OpenCL context for which to create the command queue.
 * @param device The device for which to create the command queue.
 *
 * @return The created OpenCL command queue, or NULL if the command queue creation failed.
 */
static cl_command_queue create_command_queue(cl_context context, cl_device_id device);

/**
 * Create the program from the given kernel source path.
 *
 * @param context The OpenCL context for which to create the program.
 * @param kernel_source_path The path to the kernel source file.
 *
 * @return The created OpenCL program, or NULL if the program creation failed.
 */
static cl_program create_program(cl_context *context, const char *kernel_source_path);

/**
 * Adds a kernel to the benchmark suite.
 *
 * @param program The OpenCL program containing the kernel.
 * @param kernel_name The name of the kernel to add.
 *
 * @return 0 if the kernel was successfully added, -1 otherwise.
 */
static int _banchmark_add(cl_device_id *device, cl_program *program, char **kernels_names, int num_kernels, cl_context *context, cl_command_queue *command_queue, cl_ulong max_work_group_size);

/**
 * Perform benchmarks for a given OpenCL context and device.
 *
 * @param platform The platform for which to perform benchmarks.
 * @param device The device for which to perform benchmarks.
 *
 * @return void
 */
static void perform_benchmarks(cl_platform_id *platform, cl_device_id *device);


int main(void)
{
    // 1. We need to retrive the pletforms aviable.
    cl_uint num_platforms = get_platform_num();
    cl_platform_id *platforms = (cl_platform_id *)malloc(sizeof(cl_platform_id) * num_platforms);
    get_platforms(platforms, &num_platforms);

    // Print the platforms. Debug
    for (cl_uint i = 0; i < num_platforms; i++) {
        print_platform_info_string(platforms[i], CL_PLATFORM_NAME, "Platform Name");
        print_platform_info_string(platforms[i], CL_PLATFORM_VENDOR, "Platform Vendor");
        print_platform_info_string(platforms[i], CL_PLATFORM_VERSION, "Platform Version");

        fprintf(stdout, "\n");
    }

    // 2. We need to retrieve the devices available for each platform.
    for (cl_uint i = 0; i < num_platforms; i++) {
        cl_uint num_devices = get_device_num(platforms[i]);
        cl_device_id *devices = (cl_device_id *)malloc(sizeof(cl_device_id) * num_devices);
        get_devices(platforms[i], devices, &num_devices);

        // Print the devices for the current platform. Debug
        for (cl_uint j = 0; j < num_devices; j++) {
            print_device_info_string(devices[j], CL_DEVICE_NAME, "Device Name");
            print_device_info_string(devices[j], CL_DEVICE_VENDOR, "Device Vendor");
            print_device_info_string(devices[j], CL_DEVICE_VERSION, "Device Version");

            print_device_info_ulong(devices[j], CL_DEVICE_ADDRESS_BITS, "Device Address Bits");
            print_device_info_ulong(devices[j], CL_DEVICE_GLOBAL_MEM_SIZE, "Device Global Memory Size");
            print_device_info_ulong(devices[j], CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, "Device Global Memory Cache Size");
            print_device_info_ulong(devices[j], CL_DEVICE_LOCAL_MEM_SIZE, "Device Local Memory Size");
            print_device_info_ulong(devices[j], CL_DEVICE_MAX_MEM_ALLOC_SIZE, "Device Max Memory Allocation Size");
            print_device_info_ulong(devices[j], CL_DEVICE_MAX_WORK_GROUP_SIZE, "Device Max Work Group Size");

            fprintf(stdout, "\n");
        }

        // 3. For every device perform different benchmarks or computations as needed.

        fprintf(stdout, "Performing benchmarks for platform %p and its devices.\n\n", (void *)platforms[i]);

        for (cl_uint j = 0; j < num_devices; j++) {
            // Perform benchmarks or computations for each device here.

            fprintf(stdout, "Performing benchmarks for device %p.\n\n", (void *)devices[j]);

            perform_benchmarks(&platforms[i], &devices[j]);
        }

        // Free the allocated memory for devices.
        free(devices);
    }

    // Free the allocated memory for platforms.
    free(platforms);


    return 0;
}


/*
Functions Implementations
*/

static unsigned int get_platform_num(void)
{
    cl_uint num_platforms;
    cl_int ret = clGetPlatformIDs(0, NULL, &num_platforms);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to get the number of OpenCL platforms.\n\n");
        return 0;
    }

    fprintf(stdout, "Number of OpenCL platforms: %u\n\n", (unsigned int)num_platforms);

    return (unsigned int)num_platforms;
}

static void get_platforms(cl_platform_id *platforms, cl_uint *num_platforms)
{
    cl_int ret = clGetPlatformIDs(*num_platforms, platforms, NULL);
    if (ret != CL_SUCCESS) {
        *num_platforms = 0;
        fprintf(stderr, "Failed to get OpenCL platforms.\n\n");
        return;
    }

    fprintf(stdout, "Successfully retrieved OpenCL platforms.\n\n");
}

static unsigned int get_device_num(cl_platform_id platform)
{
    cl_int ret;
    cl_uint num_devices;
    ret = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, NULL, &num_devices);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to get the number of OpenCL devices.\n\n");
        return 0;
    }

    fprintf(stdout, "Number of OpenCL devices for the platform %p: %u\n\n", (void *)platform, (unsigned int)num_devices);

    return (unsigned int)num_devices;
}

static void get_devices(cl_platform_id platform, cl_device_id *devices, cl_uint *num_devices)
{
    cl_int ret = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, *num_devices, devices, NULL);
    if (ret != CL_SUCCESS) {
        *num_devices = 0;
        fprintf(stderr, "Failed to get OpenCL devices.\n\n");
        return;
    }

    fprintf(stdout, "Successfully retrieved OpenCL devices for the platform %p.\n\n", (void *)platform);
}

/*
Information about the OpenCL platforms and devices.
*/

static void print_platform_info_string(cl_platform_id platform, cl_platform_info param_name, const char *param_desc)
{
    size_t param_size;
    clGetPlatformInfo(platform, param_name, 0, NULL, &param_size);
    char *param_value = (char *)malloc(param_size);
    clGetPlatformInfo(platform, param_name, param_size, param_value, NULL);
    fprintf(stdout, "%s: %s\n", param_desc, param_value);
    free(param_value);
}

static void print_device_info_string(cl_device_id device, cl_device_info param, const char *param_desc)
{
    size_t param_size;
    clGetDeviceInfo(device, param, 0, NULL, &param_size);
    char *param_value = (char *)malloc(param_size);
    clGetDeviceInfo(device, param, param_size, param_value, NULL);
    fprintf(stdout, "%s: %s\n", param_desc, param_value);
    free(param_value);
}

static void print_device_info_ulong(cl_device_id device, cl_device_info param, const char *param_desc)
{
    cl_ulong param_value;
    clGetDeviceInfo(device, param, sizeof(cl_ulong), &param_value, NULL);
    fprintf(stdout, "%s: %llu\n", param_desc, (unsigned long long)param_value);
}

/*
Context and benchmarking functions for OpenCL.
*/

cl_context create_context(cl_platform_id platform, cl_device_id device)
{
    cl_int ret;
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &ret);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to create OpenCL context.\n\n");
        return NULL;
    }

    return context;
}

cl_command_queue create_command_queue(cl_context context, cl_device_id device)
{
    cl_int ret;
    cl_command_queue command_queue = clCreateCommandQueueWithProperties(context, device, 0, &ret);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to create OpenCL command queue.\n\n");
        return NULL;
    }

    return command_queue;
}

cl_program create_program(cl_context *context, const char *kernel_source_path)
{
    cl_int ret;

    FILE *f = fopen(kernel_source_path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to load kernel source file: %s \n\n", kernel_source_path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t source_size = ftell(f);
    rewind(f);

    char* source_buffer = (char*)malloc(source_size + 1);
    fread(source_buffer, 1, source_size, f);
    source_buffer[source_size] = '\0';
    fclose(f);

    cl_program program = clCreateProgramWithSource(*context, 1, (const char **)&source_buffer, &source_size, &ret);
    free(source_buffer);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to create OpenCL program from source file: %s \n\n", kernel_source_path);
        return NULL;
    }

    ret = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to build OpenCL program from source file: %s \n\n", kernel_source_path);
        clReleaseProgram(program);
        return NULL;
    }

    return program;
}

static int _banchmark_add(cl_device_id *device, cl_program *program, char **kernels_names, int num_kernels, cl_context *context, cl_command_queue *command_queue, cl_ulong max_work_group_size)
{
    cl_int ret;

    clock_t time_req = clock(); 

    fprintf(stdout, "Starting benchmark for %d kernels.\n", num_kernels);
    for (int i = 0; i < num_kernels; i++) {
        fprintf(stdout, "Kernel to benchmark: %s\n", kernels_names[i]);
    }
    fprintf(stdout, "\n");

    for (int i = 0; i < num_kernels; i++) {

        char *kernel_name = kernels_names[i];
        cl_kernel kernel = clCreateKernel(*program, kernel_name, &ret);
        if (ret != CL_SUCCESS) {
            fprintf(stderr, "Failed to create OpenCL kernel: %s \n\n", kernel_name);
            return -1;
        }

        clGetKernelWorkGroupInfo(kernel, *device, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &max_work_group_size, NULL);

        fprintf(stdout, "Creating and setting up kernel: %s\n", kernel_name);

        // Create params and memory Buffers for the kernels

        size_t buffer_size = 1048576; // Example buffer size
        float *h_A = (float*)malloc(buffer_size * sizeof(float));
        float *h_b = (float*)malloc(buffer_size * sizeof(float));
        float *h_C = (float*)malloc(buffer_size * sizeof(float));

        fprintf(stdout, "Total memory allocated for host buffers: %zu bytes\n", buffer_size * sizeof(float));

        fprintf(stdout, "Initializing host buffers for kernel: %s\n", kernel_name);
        // Initzializing the host buffers
        for (size_t j = 0; j < buffer_size; j++) {
            h_A[j] = (float)j;
            h_b[j] = (float)j;
            h_C[j] = 0.0f;
        }

        fprintf(stdout, "Creating device buffers for kernel: %s\n", kernel_name);
        // Create device buffers
        cl_mem d_A = clCreateBuffer(*context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, buffer_size * sizeof(float), h_A, &ret);
        cl_mem d_b = clCreateBuffer(*context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, buffer_size * sizeof(float), h_b, &ret);
        cl_mem d_C = clCreateBuffer(*context, CL_MEM_WRITE_ONLY, buffer_size * sizeof(float), NULL, &ret);
        cl_mem d_buffer_size = clCreateBuffer(*context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(size_t), &buffer_size, &ret);

        fprintf(stdout, "Setting kernel arguments for kernel: %s\n", kernel_name);
        // Set kernel arguments
        clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&d_A);
        clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&d_b);
        clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&d_C);
        clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *)&d_buffer_size);

        fprintf(stdout, "Enqueuing kernel for execution: %s\n", kernel_name);

        // Enqueue the kernel for execution (example, assuming 1D range)
        size_t min_work_group_size;

        clGetKernelWorkGroupInfo(kernel, *device, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE , sizeof(size_t), &min_work_group_size, NULL);

        printf("Minimum work group size for kernel %s: %zu\n", kernel_name, min_work_group_size);

        size_t global_work_size = buffer_size;
        size_t local_work_size = min_work_group_size; // Initial local work size.  

        fprintf(stdout, "Initial global work size: %zu and local work size: %zu for kernel: %s\n", global_work_size, local_work_size, kernel_name);
        
        while (local_work_size <= max_work_group_size) {
            fprintf(stdout, "Adjusting local work size for kernel: %s\n", kernel_name);

            clock_t time_req_inner = clock(); 

            // Execute the queue
            fprintf(stdout, "Executing kernel: %s with global work size: %zu and local work size: %zu\n", kernel_name, global_work_size, local_work_size);
            clEnqueueNDRangeKernel(*command_queue, kernel, 1, NULL, &global_work_size, &local_work_size, 0, NULL, NULL);

            clFinish(*command_queue);

            time_req_inner = clock() - time_req_inner;
            double time_taken_inner = ((double)time_req_inner) / CLOCKS_PER_SEC;
            printf("Time taken for this kernel execution: %f seconds\n", time_taken_inner);

            local_work_size *= 2;
        }
        // Example local work size

        // Release device buffers after use
        clReleaseMemObject(d_A);
        clReleaseMemObject(d_b);
        clReleaseMemObject(d_C);
        clReleaseMemObject(d_buffer_size);
        
        // Free the host buffers
        free(h_A);
        free(h_b);
        free(h_C);

        //Free the kernel
        free(kernel_name);
        clReleaseKernel(kernel);
        
    }

    // Define the sum of the time taken to create all kernels
    time_req = clock() - time_req;
    double time_taken = ((double)time_req) / CLOCKS_PER_SEC;
    printf("Time taken to create %d kernels: %f seconds\n", num_kernels, time_taken);

    return 0;
}

static void perform_benchmarks(cl_platform_id *platform, cl_device_id *device)
{
    cl_context context = create_context(*platform, *device);

    fprintf(stdout, "Created context for platform %p and device %p\n", (void *)*platform, (void *)*device);

    cl_command_queue command_queue = create_command_queue(context, *device);

    fprintf(stdout, "Created command queue for platform %p and device %p\n", (void *)*platform, (void *)*device);

    cl_program program = create_program(&context, KERNEL_SOURCE_ADD);

    fprintf(stdout, "Created program form the source: %s for platform %p and device %p\n\n", KERNEL_SOURCE_ADD, (void *)*platform, (void *)*device);

    // Get maximum global memory size for the device
    cl_uint bites;
    clGetDeviceInfo(*device, CL_DEVICE_ADDRESS_BITS, sizeof(cl_uint), &bites, NULL);

    cl_ulong max_global_mem_size;
    clGetDeviceInfo(*device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &max_global_mem_size, NULL);

    if (bites < 64) {
        fprintf(stderr, "Warning: Device has less than 64-bit address space.\n");

        if (max_global_mem_size > (cl_ulong)4 * 1024 * 1024 * 1024) {
            fprintf(stderr, "Warning: Device has more than 4GB of global memory but less than 64-bit address space.\n");

            max_global_mem_size = (cl_ulong)4 * 1024 * 1024 * 1024;
        }
    } else {
        fprintf(stdout, "Device has 64-bit address space.\n");

        clGetDeviceInfo(*device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &max_global_mem_size, NULL);

        fprintf(stdout, "Maximum memory allocation size for the device: %llu bytes\n", (unsigned long long)max_global_mem_size);
    }

    // Get maximum work group size for the device
    cl_ulong max_work_group_size;
    clGetDeviceInfo(*device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(cl_ulong), &max_work_group_size, NULL);

    char **kernels_names = (char **)malloc(1 * sizeof(char *));

    kernels_names[0] = "add";

    fprintf(stdout, "The kernels to execute are: %s\n", kernels_names[0]);

    _banchmark_add(device, &program, kernels_names, 1, &context, &command_queue, max_work_group_size);

    fprintf(stdout, "Benchmarking completed for platform %p and device %p\n", (void *)*platform, (void *)*device);

    free(kernels_names);

}