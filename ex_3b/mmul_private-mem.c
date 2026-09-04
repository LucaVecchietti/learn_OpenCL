/**
 * Matrix multiplication using local memory in OpenCL.
 * 
 * This program demonstrates how to perform matrix multiplication
 * efficiently by utilizing local memory to reduce global memory
 * accesses.
 */

 #define CL_TARGET_OPENCL_VERSION 200

 #include <CL/cl.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>

 #define N 2048

 /**
  * Import kernel source file as a string
  * 
  * @param filename The path to the kernel source file.
  * @param out_size The size of the output buffer.
  * @return The contents of the kernel source file as a string.
  */
static char *load_kernel_source(const char *filename, size_t *out_size) {

    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open kernel source file: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    char *source = (char *)malloc(size + 1);
    if (!source) {
        fprintf(stderr, "Failed to allocate memory for kernel source\n");
        exit(EXIT_FAILURE);
    }

    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);

    if (out_size) {
        *out_size = size;
    }

    return source;
}

/**
 * Printe the build log of an OpenCL program.
 *
 * @param program The OpenCL program.
 * @param device The OpenCL device.
 */
static void print_build_log(cl_program program, cl_device_id device) {
    size_t log_size;
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
    char *log = (char *)malloc(log_size);
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
    fprintf(stderr, "%s\n", log);
    free(log);
}

/**
 * Print the error message and exit the program.
 *
 * @param err The OpenCL error code.
 * @param message The error message to print.
 */
static void check_error(cl_int err, const char *message) {
    if (err != CL_SUCCESS) {
        fprintf(stderr, "%s (Error code: %d)\n", message, err);
        exit(EXIT_FAILURE);
    }
}

/**
 * Main function.
 *
 * @return Exit status of the program.
 */
int main(void) {
    cl_int err;

    /* Initialize OpenCL context, command queue, and program */

    cl_platform_id platform;
    err = clGetPlatformIDs(1, &platform, NULL);
    check_error(err, "Failed to get OpenCL platform");

    cl_device_id device;
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    check_error(err, "Failed to get OpenCL device");

    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    check_error(err, "Failed to create OpenCL context");

    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, NULL, &err);
    check_error(err, "Failed to create OpenCL command queue");

    /* Load snd build the OpenCL program */

    size_t source_size;
    char *source = load_kernel_source("ex_3b/mmul_private-mem.cl", &source_size);

    cl_program program = clCreateProgramWithSource(context, 1, (const char **)&source, &source_size, &err);
    check_error(err, "Failed to create OpenCL program");

    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        print_build_log(program, device);
        check_error(err, "Failed to build OpenCL program");
    }

    free(source);

    /* Declare mamory buffers for matrices */

    float *h_A = (float *)malloc(sizeof(float) * N * N);
    float *h_B = (float *)malloc(sizeof(float) * N * N);
    float *h_C = (float *)malloc(sizeof(float) * N * N);

    for (int i = 0; i < N * N; i++) {
        h_A[i] = (float)i + 1;
        h_B[i] = (float)i + 1;
        h_C[i] = 0.0f;
    }

    cl_mem d_A = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float) * N * N, h_A, &err);
    check_error(err, "Failed to create buffer for matrix A");

    cl_mem d_B = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float) * N * N, h_B, &err);
    check_error(err, "Failed to create buffer for matrix B");

    cl_mem d_C = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float) * N * N, NULL, &err);
    check_error(err, "Failed to create buffer for matrix C");

    /* Build the kernel and set the kernel arguments */

    unsigned int count = N;

    cl_kernel kernel = clCreateKernel(program, "matrix_multiply_private", &err);
    check_error(err, "Failed to create kernel");

    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_A);
    check_error(err, "Failed to set kernel argument 0");
    err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_B);
    check_error(err, "Failed to set kernel argument 1");
    err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_C);
    check_error(err, "Failed to set kernel argument 2");
    err = clSetKernelArg(kernel, 3, sizeof(unsigned int), &count);
    check_error(err, "Failed to set kernel argument 3");

    /* Enqueue the kernel for execution */
    size_t global_work_size = N * N; // This time the dimensionality is 1D
    size_t local_work_size = 64; // Example local work size

    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, &local_work_size, 0, NULL, NULL);
    check_error(err, "Failed to enqueue NDRange kernel");

    /* Read the result back to host memory */
    err = clEnqueueReadBuffer(queue, d_C, CL_TRUE, 0, sizeof(float) * N * N, h_C, 0, NULL, NULL);
    check_error(err, "Failed to read buffer for matrix C");

    /* Debug verification: print the resulting matrix C */
    // for (int i = 0; i < N; i++) {
    //     for (int j = 0; j < N; j++) {
    //         printf("%f ", h_C[i * N + j]);
    //     }
    //     printf("\n");
    // }

    /* Clean up */
    clReleaseMemObject(d_A);
    clReleaseMemObject(d_B);
    clReleaseMemObject(d_C);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    free(h_A);
    free(h_B);
    free(h_C);

    return 0;
}