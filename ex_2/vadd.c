/*
 * vadd.c
 *
 * Host program per l'Exercise 2 ("Running the Vadd kernel") delle slide
 * Hands On OpenCL (Bristol/KITE).
 *
 * Segue i 5 step classici descritti nella Lecture 4:
 *   1. Define the platform (platform + device + context + queue)
 *   2. Create and Build the program (carica e compila il kernel .cl)
 *   3. Setup memory objects (buffer per A, B, C)
 *   4. Define the kernel (crea il kernel, associa gli argomenti)
 *   5. Enqueue commands (scrive i dati, esegue il kernel, legge il risultato)
 */

#define CL_TARGET_OPENCL_VERSION 200  /* la tua Radeon supporta OpenCL 2.0 */

#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VECTOR_SIZE 1024

/* Legge l'intero contenuto di un file di testo in un buffer allocato
 * dinamicamente. Il chiamante deve fare free() sul risultato.
 * Necessario perché clCreateProgramWithSource vuole il sorgente del
 * kernel come stringa in memoria, non un path a un file. */
static char *load_kernel_source(const char *filename, size_t *out_size)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Impossibile aprire il file kernel: %s\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = (char *)malloc(size + 1);
    if (!buffer) {
        fprintf(stderr, "Allocazione memoria fallita per il kernel source.\n");
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

    /* ============================================================
     * STEP 1: Define the platform
     * ============================================================ */
    cl_platform_id platform;
    err = clGetPlatformIDs(1, &platform, NULL);
    check_error(err, "clGetPlatformIDs");

    cl_device_id device;
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    check_error(err, "clGetDeviceIDs");

    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    check_error(err, "clCreateContext");

    cl_command_queue queue = clCreateCommandQueue(context, device, 0, &err);
    check_error(err, "clCreateCommandQueue");

    /* ============================================================
     * STEP 2: Create and Build the program
     * ============================================================ */
    size_t source_size;
    char *source = load_kernel_source("ex_2/vadd.cl", &source_size);
    if (!source) {
        return EXIT_FAILURE;
    }

    cl_program program = clCreateProgramWithSource(
        context, 1, (const char **)&source, &source_size, &err);
    check_error(err, "clCreateProgramWithSource");
    free(source);

    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        print_build_log(program, device);
        return EXIT_FAILURE;
    }

    /* ============================================================
     * STEP 3: Setup memory objects
     * ============================================================ */
    float h_a[VECTOR_SIZE], h_b[VECTOR_SIZE], h_c[VECTOR_SIZE];

    for (int i = 0; i < VECTOR_SIZE; i++) {
        h_a[i] = (float)i;
        h_b[i] = (float)(VECTOR_SIZE - i);
    }

    cl_mem d_a = clCreateBuffer(context, CL_MEM_READ_ONLY,
                                 sizeof(float) * VECTOR_SIZE, NULL, &err);
    check_error(err, "clCreateBuffer d_a");

    cl_mem d_b = clCreateBuffer(context, CL_MEM_READ_ONLY,
                                 sizeof(float) * VECTOR_SIZE, NULL, &err);
    check_error(err, "clCreateBuffer d_b");

    cl_mem d_c = clCreateBuffer(context, CL_MEM_WRITE_ONLY,
                                 sizeof(float) * VECTOR_SIZE, NULL, &err);
    check_error(err, "clCreateBuffer d_c");

    /* ============================================================
     * STEP 4: Define the kernel
     * ============================================================ */
    cl_kernel kernel = clCreateKernel(program, "vadd", &err);
    check_error(err, "clCreateKernel");

    unsigned int count = VECTOR_SIZE;
    err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_a);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_b);
    err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_c);
    err |= clSetKernelArg(kernel, 3, sizeof(unsigned int), &count);
    check_error(err, "clSetKernelArg");

    /* ============================================================
     * STEP 5: Enqueue commands
     * ============================================================ */

    /* Scrittura dei buffer host -> device (non bloccanti: la vera
     * sincronizzazione avviene alla lettura finale, CL_TRUE) */
    err = clEnqueueWriteBuffer(queue, d_a, CL_FALSE, 0,
                                sizeof(float) * VECTOR_SIZE, h_a, 0, NULL, NULL);
    check_error(err, "clEnqueueWriteBuffer d_a");

    err = clEnqueueWriteBuffer(queue, d_b, CL_FALSE, 0,
                                sizeof(float) * VECTOR_SIZE, h_b, 0, NULL, NULL);
    check_error(err, "clEnqueueWriteBuffer d_b");

    /* Esecuzione del kernel: global_size = VECTOR_SIZE, local_size
     * lasciato a NULL per ora (decide il runtime). Lo controlleremo
     * esplicitamente più avanti quando parleremo di work-group sizing. */
    size_t global_size = VECTOR_SIZE;
    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL,
                                  &global_size, NULL, 0, NULL, NULL);
    check_error(err, "clEnqueueNDRangeKernel");

    /* Lettura bloccante del risultato: garantisce che tutto il lavoro
     * precedente sia completato prima di procedere. */
    err = clEnqueueReadBuffer(queue, d_c, CL_TRUE, 0,
                               sizeof(float) * VECTOR_SIZE, h_c, 0, NULL, NULL);
    check_error(err, "clEnqueueReadBuffer");

    /* ============================================================
     * Verifica del risultato
     * ============================================================ */
    int errors = 0;
    for (int i = 0; i < VECTOR_SIZE; i++) {
        float expected = h_a[i] + h_b[i];
        if (h_c[i] != expected) {
            if (errors < 10) {
                fprintf(stderr, "Mismatch a i=%d: atteso %f, ottenuto %f\n",
                        i, expected, h_c[i]);
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

    /* ============================================================
     * Pulizia risorse
     * ============================================================ */
    clReleaseMemObject(d_a);
    clReleaseMemObject(d_b);
    clReleaseMemObject(d_c);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    return (errors == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}