/**
 * Chaining Vector Addition using OpenCL
 */

#define CL_TARGET_OPENCL_VERSION 220

#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VECTOR_SIZE 1024

/**
 * load kernel source from file
 */
static char *load_kernel_source(const char *fileneame, size_t *out_size)
{
    FILE *f = fopen(fileneame, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open kernel source file: %s\n", fileneame);
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    char *buffer = (char *)malloc(size + 1);
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

static void print_build_log(cl_program program, cl_device_id device)
{
    char buffer[4096];
    size_t len;
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
    fprintf(stderr, "Build log:\n%s\n", buffer);
}

static void check_error(cl_int err, const char *operation)
{
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error during operation '%s': %d\n", operation, err);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    cl_int err; // error code for OpenCL functions

    /**
     * Initialize OpenCL context, command queue, and program
     */

     //Get platform
     cl_platform_id platform;
     err = clGetPlatformIDs(1, &platform, NULL);
     check_error(err, "Getting platform");

     //Get device
     cl_device_id device;
     err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
     check_error(err, "Getting device");

     //Create context
     cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
     check_error(err, "Creating context");

     //Create command queue
     cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, 0, &err);
     check_error(err, "Creating command queue");

     /**
      * Load and build OpenCL program
      */

      size_t source_size;
      char *source = load_kernel_source("ex_2/vadd.cl", &source_size);
      
      cl_program program = clCreateProgramWithSource(context, 1, (const char **)&source, &source_size, &err);
      check_error(err, "Creating program with source");

      err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
      if (err != CL_SUCCESS) {
          print_build_log(program, device);
          check_error(err, "Building program");
      }

      free(source);

      /**
       * Create memory buffers and kernel
       */

       float h_a[VECTOR_SIZE], h_b[VECTOR_SIZE], h_c[VECTOR_SIZE], h_d[VECTOR_SIZE];

       for (int i = 0; i < VECTOR_SIZE; i++) {
           h_a[i] = i;
           h_b[i] = i*2;
           h_c[i] = i*0.5;
           h_d[i] = 0;
       }

       cl_mem d_a = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(h_a), h_a, &err);
       check_error(err, "Creating buffer d_a");

       cl_mem d_b = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(h_b), h_b, &err);
       check_error(err, "Creating buffer d_b");

       cl_mem d_temp = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(h_c), NULL, &err);
       check_error(err, "Creating buffer d_temp");

       cl_mem d_c = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(h_c), h_c, &err);
       check_error(err, "Creating buffer d_c");

       cl_mem d_d = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(h_d), NULL, &err);
       check_error(err, "Creating buffer d_d");

       cl_kernel kernel = clCreateKernel(program, "vadd", &err);
       check_error(err, "Creating kernel");

      /**
       * setup kernel arguments for first vadd
       */

       unsigned int count = VECTOR_SIZE;

       err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_a);
       check_error(err, "Setting kernel argument 0");

       err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_b);
       check_error(err, "Setting kernel argument 1");

       err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_temp);
       check_error(err, "Setting kernel argument 2");

       err = clSetKernelArg(kernel, 3, sizeof(unsigned int), &count);
       check_error(err, "Setting kernel argument 3");

       /**
        * Enqueue the commands for first execution of the kernel
        */

        err = clEnqueueWriteBuffer(queue, d_a, CL_FALSE, 0, sizeof(h_a), h_a, 0, NULL, NULL);
        check_error(err, "Writing buffer d_a");

        err = clEnqueueWriteBuffer(queue, d_b, CL_FALSE, 0, sizeof(h_b), h_b, 0, NULL, NULL);
        check_error(err, "Writing buffer d_b");

        size_t global_work_size = VECTOR_SIZE;
        size_t local_work_size = 64; // Example local work size

        err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, &local_work_size, 0, NULL, NULL);
        check_error(err, "Enqueueing kernel");

        /**
         * Set kernel arguments for second vadd
         */

        err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_temp);
        check_error(err, "Setting kernel argument 0");

        err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_c);
        check_error(err, "Setting kernel argument 1");

        err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_d);
        check_error(err, "Setting kernel argument 2");

        err = clSetKernelArg(kernel, 3, sizeof(unsigned int), &count);
        check_error(err, "Setting kernel argument 3");

        /**
         * Enqueue the commands for second execution of the kernel
         */

        err = clEnqueueWriteBuffer(queue, d_c, CL_FALSE, 0, sizeof(h_c), h_c, 0, NULL, NULL);
        check_error(err, "Writing buffer d_c");

        err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, &local_work_size, 0, NULL, NULL);
        check_error(err, "Enqueueing kernel");

        /**
         * Read the result from the device buffer d_d back to the host buffer h_d
         */

        err = clEnqueueReadBuffer(queue, d_d, CL_TRUE, 0, sizeof(h_d), h_d, 0, NULL, NULL);
        check_error(err, "Reading buffer d_d");
        
        /**
         * verify the result by comparing h_d with the expected result
         */

         for (int i = 0; i < VECTOR_SIZE; i++) {
             float expected = h_a[i] + h_b[i] + h_c[i];
             if (h_d[i] != expected) {
                 fprintf(stderr, "Verification failed at index %d: expected %f, got %f\n", i, expected, h_d[i]);
                 exit(EXIT_FAILURE);
             }
         }
         printf("Verification passed!\n");

         /**
          * Cleanup OpenCL resources
          */

        clReleaseMemObject(d_a);
        clReleaseMemObject(d_b);
        clReleaseMemObject(d_c);
        clReleaseMemObject(d_d);
        clReleaseMemObject(d_temp);
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);

        return 0;

}