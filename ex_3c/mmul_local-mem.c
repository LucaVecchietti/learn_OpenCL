/**
 * Matrix multiplication using local memory in OpenCL.
 */

 #define CL_TARGET_OPENCL_VERSION 200

 #include <CL/cl.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>

 #define N 2048

 /**
 *  Load the OpenCL kernel source code from a file.
 */
 static char *load_kernel_source(const char *filename, size_t *out_size)
 {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open kernel source file: %s\n", filename);
        exit(EXIT_FAILURE);
    }
    
    fseek(f, 0, SEEK_END);
    *out_size = ftell(f);
    rewind(f);

    char *buffer = (char *)malloc(*out_size);
    if (!buffer) {
        fprintf(stderr, "failed to allocate memory for kernel source\n");
        exit(EXIT_FAILURE);
    }

    fread(buffer, 1, *out_size, f);
    fclose(f);
    return buffer;
 }

 /**
  * Log the build information for an OpenCL program.
  */
 static void log_build_info(cl_program program, cl_device_id device)
 {
     size_t log_size;
     clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
     char *log = (char *)malloc(log_size);
     clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
     fprintf(stderr, "%s\n", log);
     free(log);
 }

 /**
  *  Check for OpenCL errors and print an error message if any.
  */
 static void check_error(cl_int err, const char *message)
 {
     if (err != CL_SUCCESS) {
         fprintf(stderr, "OpenCL error: %s (%d)\n", message, err);
         exit(EXIT_FAILURE);
     }
 }

 /**
  *  Main function for matrix multiplication using local memory in OpenCL.
  */
  int main(void)
  {
    cl_int err; 

    /** Define the OpenCL context, command queue, and program */

    cl_platform_id platform;
    err = clGetPlatformIDs(1, &platform, NULL);
    check_error(err, "Failed to get OpenCL platform IDs");

    cl_device_id device; 
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    check_error(err, "Failed to get OpenCL device IDs");

    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    check_error(err, "Failed to create OpenCL context");

    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, 0, &err);
    check_error(err, "Failed to create OpenCL command queue");

    /** Load and build the OpenCL program */

    size_t source_size;
    char *source = load_kernel_source("ex_3c/mmul_local-mem.cl", &source_size);

    cl_program program = clCreateProgramWithSource(context, 1, (const char **)&source, &source_size, &err);
    check_error(err, "Failed to create OpenCL program with source");

    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        log_build_info(program, device);
        check_error(err, "Failed to build OpenCL program");
    }

    free(source);

    /* Create memory buffers for matrices A, B, and C */
    const unsigned int count = N;

    float *h_A = (float *)malloc(N * N * sizeof(float));
    float *h_B = (float *)malloc(N * N * sizeof(float));
    float *h_C = (float *)malloc(N * N * sizeof(float));

    for (int i = 0; i < N * N; i++) {
        h_A[i] = (float)i;
        h_B[i] = (float)i;
        h_C[i] = 0.0f;
    }

    cl_mem d_A = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, N * N * sizeof(float), h_A, &err);
    check_error(err, "Failed to create buffer for matrix A");

    cl_mem d_B = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, N * N * sizeof(float), h_B, &err);
    check_error(err, "Failed to create buffer for matrix B");

    cl_mem d_C = clCreateBuffer(context, CL_MEM_WRITE_ONLY, N * N * sizeof(float), NULL, &err);
    check_error(err, "Failed to create buffer for matrix C");

    /** Create kernel  */

    cl_kernel kernel = clCreateKernel(program, "mmul", &err);
    check_error(err, "Failed to create kernel");

    err = clSetKernelArg(kernel, 0, sizeof(h_A), &d_A);
    check_error(err, "Failed to set kernel argument 0");

    err = clSetKernelArg(kernel, 1, sizeof(h_B), &d_B);
    check_error(err, "Failed to set kernel argument 1");

    err = clSetKernelArg(kernel, 2, sizeof(h_C), &d_C);
    check_error(err, "Failed to set kernel argument 2");

    err = clSetKernelArg(kernel, 3, sizeof(count), &count);
    check_error(err, "Failed to set kernel argument 3");

    err = clSetKernelArg(kernel, 4, sizeof(float) * N, NULL);
    check_error(err, "Failed to set kernel argument 4");

    /** Enqueue kernel for execution */

    size_t global_work_size = N;
    size_t local_work_size = 256; 

    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, &local_work_size, 0, NULL, NULL);
    check_error(err, "Failed to enqueue NDRange kernel");

    err = clEnqueueReadBuffer(queue, d_C, CL_TRUE, 0, N * N * sizeof(float), h_C, 0, NULL, NULL);
    check_error(err, "Failed to read buffer for matrix C");

    /* Verify the result */
    // for (int i = 0; i < N; i++) {
    //     for (int j = 0; j < N; j++) {
    //         float expected = 0.0f;
    //         for (int k = 0; k < N; k++) {
    //             expected += h_A[i * N + k] * h_B[k * N + j];
    //         }
    //         if (h_C[i * N + j] != expected) {
    //             printf("Mismatch at (%d, %d): got %f, expected %f\n", i, j, h_C[i * N + j], expected);
    //         }
    //     }
    // }

    free(h_A);
    free(h_B);
    free(h_C);

    /* Release OpenCL resources */
    clReleaseMemObject(d_A);
    clReleaseMemObject(d_B);
    clReleaseMemObject(d_C);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
}