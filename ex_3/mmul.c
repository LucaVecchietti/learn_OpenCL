/**
 * Matrix Multiplication using OpenCL
 * 
 * Computes the product of two matrices using OpenCL kernels.
 */

#define CL_TARGET_OPENCL_VERSION 200

#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MATRIX_SIZE 4

/**
 * Load the OpenCL kernel source code from a file.
 * 
 * @param filename The path to the kernel source file.
 * @param out_size Pointer to a variable to store the size of the loaded source.
 * @return A pointer to the loaded kernel source code. The caller is responsible for freeing it.
 */
static char *load_kernel_source(const char *filename, size_t *out_size)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open kernel source file: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    char *buffer = (char *)malloc(size +1);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory for kernel source\n");
        exit(EXIT_FAILURE);
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    if (out_size) {
        *out_size = size;
    }

    return buffer;
}

/**
 * Print the build log for an OpenCL program.
 *
 * @param program The OpenCL program.
 * @param device The OpenCL device associated with the program.
 */
static void print_build_log(cl_program program, cl_device_id device)
{
    size_t log_size;
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
    char *log = (char *)malloc(log_size);
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
    fprintf(stderr, "Build log:\n%s\n", log);
    free(log);
}

/**
 * Check OpenCL errors and print a message if an error occurred.
 *
 * @param err The OpenCL error code.
 * @param msg The message to print if an error occurred.
 */
static void check_error(cl_int err, const char *process)
{
    if (err != CL_SUCCESS) {
        fprintf(stderr, "OpenCL error during %s: %d\n", process, err);
        exit(EXIT_FAILURE);
    }
}

/**
 * Main function for matrix multiplication using OpenCL.
 */
int main(void) 
{
    cl_int err;     // Variable to store OpenCL error codes

    /* Initialize OpenCL platform, device, context, and command queue */

    //Get the first available OpenCL platform
    cl_platform_id platform;
    err = clGetPlatformIDs(1, &platform, NULL);
    check_error(err, "clGetPlatformIDs");

    //Get the first available GPU device for the platform
    cl_device_id device;
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    check_error(err, "clGetDeviceIDs");

    //Create an OpenCL context for the selected device
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    check_error(err, "clCreateContext");

    // Create a command queue for the selected context and device
    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, 0, &err);
    check_error(err, "clCreateCommandQueue");

    /* Load and build the OpenCL kernel */

    const char *kernel_source_file = "ex_3/mmul.cl";
    size_t source_size;
    char *source = load_kernel_source(kernel_source_file, &source_size);
    if (!source) {
        fprintf(stderr, "Failed to load source file %s\n", kernel_source_file);
        exit(EXIT_FAILURE);
    }

    cl_program program = clCreateProgramWithSource(context, 1, (const char **)&source, &source_size, &err);
    check_error(err, "clCreateProgramWithSource");

    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        print_build_log(program, device);
        check_error(err, "clBuildProgram");
    }

    free(source);

    /* Define memory buffers for the matrices */

    float *h_A = (float *)malloc(sizeof(float) * MATRIX_SIZE * MATRIX_SIZE);
    float *h_B = (float *)malloc(sizeof(float) * MATRIX_SIZE * MATRIX_SIZE);
    float *h_C = (float *)malloc(sizeof(float) * MATRIX_SIZE * MATRIX_SIZE);

    for (unsigned int i = 0; i < MATRIX_SIZE * MATRIX_SIZE; i++) {
        h_A[i] = (float)(i + 1);
        h_B[i] = (float)(i + 2);
        h_C[i] = 0.0f;
    }

    cl_mem d_A = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float) * MATRIX_SIZE * MATRIX_SIZE, h_A, &err);
    check_error(err, "clCreateBuffer d_A");

    cl_mem d_B = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float) * MATRIX_SIZE * MATRIX_SIZE, h_B, &err);
    check_error(err, "clCreateBuffer d_B");

    cl_mem d_C = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float) * MATRIX_SIZE * MATRIX_SIZE, NULL, &err);
    check_error(err, "clCreateBuffer d_C");

    /* Build the OpenCL kernel */

    cl_kernel kernel = clCreateKernel(program, "mmul", &err);
    check_error(err, "clCreateKernel");

    /* Set kernel arguments */

    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_A);
    check_error(err, "clSetKernelArg 0");
    err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_B);
    check_error(err, "clSetKernelArg 1");
    err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_C);
    check_error(err, "clSetKernelArg 2");

    const unsigned int count = MATRIX_SIZE;
    err = clSetKernelArg(kernel, 3, sizeof(unsigned int), &count);
    check_error(err, "clSetKernelArg 3");

    /* Enqueue the kernel for execution */

    // No clEnqueuedWriteBuffer needed couse we used the CL_MEM_COPY_HOST_PTR flag when creating the input buffers

    size_t global_work_size[2] = {MATRIX_SIZE, MATRIX_SIZE};    // Define the global work size for the 2D NDRange kernel
    err = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global_work_size, NULL, 0, NULL, NULL);
    check_error(err, "clEnqueueNDRangeKernel");

    err = clEnqueueReadBuffer(queue, d_C, CL_TRUE, 0, sizeof(float) * MATRIX_SIZE * MATRIX_SIZE, h_C, 0, NULL, NULL);    // Read the result matrix C from device to host
    check_error(err, "clEnqueueReadBuffer d_C");

    /* Verification of the result */

    float *sum = (float *)malloc(sizeof(float) * MATRIX_SIZE * MATRIX_SIZE);
    memset(sum, 0, sizeof(float) * MATRIX_SIZE * MATRIX_SIZE);

    for (unsigned int i=0; i < MATRIX_SIZE; i++) {
        for (unsigned int j=0; j < MATRIX_SIZE; j++) {
            for (unsigned int k = 0; k < MATRIX_SIZE; k++) {
                sum[i * MATRIX_SIZE + j] += h_A[i * MATRIX_SIZE + k] * h_B[k * MATRIX_SIZE + j];
            }
        }
    }
    for (unsigned int i = 0; i < MATRIX_SIZE; i++) {
        for (unsigned int j = 0; j < MATRIX_SIZE; j++) {
            if (h_C[i * MATRIX_SIZE + j] != sum[i * MATRIX_SIZE + j]) {
                fprintf(stderr, "Verification failed at index (%u, %u): %f != %f\n", i, j, h_C[i * MATRIX_SIZE + j], sum[i * MATRIX_SIZE + j]);
                free(sum);
                exit(EXIT_FAILURE);
            }
        }
    }
    printf("Verification passed!\n");
    free(sum);

    /* Free allocated resources */
    free(h_A);
    free(h_B);
    free(h_C);
    clReleaseMemObject(d_A);
    clReleaseMemObject(d_B);
    clReleaseMemObject(d_C);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
}

    

