/**
 * vadc.c 
 * 
 * Host del preogramma per l'esercizio di somma di 3 vettori 
 * ("Hosting del Kernel di vadc.cl") 
 */

#define CL_TARGET_OPENCL_VERSION 200 // Target version of open CL 

#include<CL/cl.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>

#define VECTOR_SIZE 1024 // deve essere un multiplo di 64

/**
 * Load the kernel surce as a string from the .cl file
 * 
 * @param filename
 * @param out_size
 * 
 * @return surce file as a string
 */
static char *load_kernel_source(char *filename, size_t *out_size)
{

    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        fprintf(stderr, "Impossibile aprire il file kernel: %s\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    char *buffer = (char *)malloc(size + 1);
    if (!buffer)
    {
        fprintf(stderr, "Errore dirante la collocazione della mamoria.");
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, size, f);
    buffer[read_bytes] = '\0';
    fclose(f);

    if (out_size) *out_size = read_bytes;
    return buffer;
}

/* Stampa il build log in caso di errore di compilazione del kernel.
 * Corrisponde esattamente allo snippet "Error messages" della Lecture 4. */
static void print_build_log(cl_program program, cl_device_id device)
{
    char buffer[4096];
    size_t len;
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                           sizeof(buffer), buffer, &len);
    fprintf(stderr, "Build log:\n%s\n", buffer);
}

/* Piccola utility per controllare gli error code OpenCL senza ripetere
 * lo stesso if/fprintf/exit ovunque. */
static void check_error(cl_int err, const char *operation)
{
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Errore durante '%s': codice %d\n", operation, err);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    cl_int err;

    /**
     * Conffigurazione piattaforma
     */

    cl_platform_id platform;
    clGetPlatformIDs(1, &platform, NULL);
    check_error(err, "clGetPlatformIDs");

    cl_device_id device;
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    check_error(err, "clGetDeviceIDs");

    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    check_error(err, "clCreateContext");

    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, 0, &err);
    check_error(err, "clCreateCommandQueueWithProperties");

    /**
     * Creazione e costruzione del programma
     */

     size_t source_size;
     char *source = load_kernel_source("ex_2b/vadc.cl", &source_size);
     if (!source) {
        return EXIT_FAILURE;
     }

     cl_program program = clCreateProgramWithSource(context, 1, (const char **)&source, &source_size, &err);
     check_error(err, "clCreateProgramWithSource");
     free(source);

     err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
     if(err != CL_SUCCESS) {
         print_build_log(program, device);
         check_error(err, "clBuildProgram");
     }

    /**
     * Setup memory buffers and kernel arguments
     */

    float h_a[VECTOR_SIZE], h_b[VECTOR_SIZE], h_c[VECTOR_SIZE], h_d[VECTOR_SIZE];

    for (int i = 0; i < VECTOR_SIZE; i++) {
        h_a[i] = (float)i;
        h_b[i] = (float)(i * 2);
        h_c[i] = 1.2f;
        h_d[i] = 0.0f;
    }

    cl_mem d_a = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(h_a), h_a, &err);
    check_error(err, "clCreateBuffer d_a");

    cl_mem d_b = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(h_b), h_b, &err);
    check_error(err, "clCreateBuffer d_b");

    cl_mem d_c = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(h_c), h_c, &err);
    check_error(err, "clCreateBuffer d_c");

    cl_mem d_d = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(h_d), NULL, &err);
    check_error(err, "clCreateBuffer d_d");

    /**
     * Setup kernel arguments
     */

    cl_kernel kernel = clCreateKernel(program, "vadc", &err);
    check_error(err, "clCreateKernel");

    unsigned int vector_size = VECTOR_SIZE;

    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_a);
    check_error(err, "clSetKernelArg 0");
    err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_b);
    check_error(err, "clSetKernelArg 1");
    err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_c);
    check_error(err, "clSetKernelArg 2");
    err = clSetKernelArg(kernel, 3, sizeof(cl_mem), &d_d);
    check_error(err, "clSetKernelArg 3");
    err = clSetKernelArg(kernel, 4, sizeof(unsigned int), &vector_size);
    check_error(err, "clSetKernelArg 4");

    /**
     * Enqueue kernel for execution
     */

     err = clEnqueueWriteBuffer(queue, d_a, CL_FALSE, 0, sizeof(h_a), h_a, 0, NULL, NULL);
     check_error(err, "clEnqueueWriteBuffer d_a");

     err = clEnqueueWriteBuffer(queue, d_b, CL_FALSE, 0, sizeof(h_b), h_b, 0, NULL, NULL);
     check_error(err, "clEnqueueWriteBuffer d_b");

     err = clEnqueueWriteBuffer(queue, d_c, CL_FALSE, 0, sizeof(h_c), h_c, 0, NULL, NULL);
     check_error(err, "clEnqueueWriteBuffer d_c");

    size_t global_work_size = VECTOR_SIZE;
    size_t local_work_size = 64; // Example local work size, adjust as needed

    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, &local_work_size, 0, NULL, NULL);
    check_error(err, "clEnqueueNDRangeKernel");

    // Read the result back to host
    err = clEnqueueReadBuffer(queue, d_d, CL_TRUE, 0, sizeof(h_d), h_d, 0, NULL, NULL);
    check_error(err, "clEnqueueReadBuffer d_d");

    /**
     * Verify the result
     */

    int errors = 0;
    for (int i = 0; i < VECTOR_SIZE; i++) {
        float expected = h_a[i] + h_b[i] + h_c[i];
        if (h_d[i] != expected) {
            if (errors < 10) {
                fprintf(stderr, "Mismatch a i=%d: atteso %f, ottenuto %f\n",
                        i, expected, h_d[i]);
            }
            errors++;
        }
    }

    if (errors == 0) {
        printf("Vector addition completata correttamente! (%d elementi)\n",
               VECTOR_SIZE);
    } else {
        printf("Vector addition FALLITA: %d errori su %d elementi.\n",
               errors, VECTOR_SIZE);
    }

    /**
     * Cleanup
     */
    clReleaseMemObject(d_a);
    clReleaseMemObject(d_b);
    clReleaseMemObject(d_c);
    clReleaseMemObject(d_d);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    return 0;
}