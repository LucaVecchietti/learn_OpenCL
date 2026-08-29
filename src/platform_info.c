/*
 * platform_info.c
 *
 * Equivalente dell'Exercise 1 delle slide "Hands On OpenCL" (Bristol/KITE):
 * elenca tutte le OpenCL Platform disponibili e, per ciascuna, tutti i
 * Device visibili, stampandone le informazioni principali.
 */

#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>

static void print_device_string(cl_device_id device, cl_device_info param,
                                 const char *label)
{
    char buffer[1024];
    cl_int err = clGetDeviceInfo(device, param, sizeof(buffer), buffer, NULL);
    if (err == CL_SUCCESS) {
        printf("    %-28s: %s\n", label, buffer);
    } else {
        printf("    %-28s: <errore query, codice %d>\n", label, err);
    }
}

static void print_device_uint(cl_device_id device, cl_device_info param,
                               const char *label)
{
    cl_uint value;
    cl_int err = clGetDeviceInfo(device, param, sizeof(value), &value, NULL);
    if (err == CL_SUCCESS) {
        printf("    %-28s: %u\n", label, value);
    } else {
        printf("    %-28s: <errore query, codice %d>\n", label, err);
    }
}

static void print_device_size(cl_device_id device, cl_device_info param,
                               const char *label)
{
    size_t value;
    cl_int err = clGetDeviceInfo(device, param, sizeof(value), &value, NULL);
    if (err == CL_SUCCESS) {
        printf("    %-28s: %zu\n", label, value);
    } else {
        printf("    %-28s: <errore query, codice %d>\n", label, err);
    }
}

static void print_device_ulong_mb(cl_device_id device, cl_device_info param,
                                   const char *label)
{
    cl_ulong value;
    cl_int err = clGetDeviceInfo(device, param, sizeof(value), &value, NULL);
    if (err == CL_SUCCESS) {
        printf("    %-28s: %llu MB\n", label,
               (unsigned long long)(value / (1024 * 1024)));
    } else {
        printf("    %-28s: <errore query, codice %d>\n", label, err);
    }
}

static void print_device_ulong_kb(cl_device_id device, cl_device_info param,
                                   const char *label)
{
    cl_ulong value;
    cl_int err = clGetDeviceInfo(device, param, sizeof(value), &value, NULL);
    if (err == CL_SUCCESS) {
        printf("    %-28s: %llu KB\n", label,
               (unsigned long long)(value / 1024));
    } else {
        printf("    %-28s: <errore query, codice %d>\n", label, err);
    }
}

static const char *device_type_to_string(cl_device_type type)
{
    if (type & CL_DEVICE_TYPE_GPU) return "GPU";
    if (type & CL_DEVICE_TYPE_CPU) return "CPU";
    if (type & CL_DEVICE_TYPE_ACCELERATOR) return "Accelerator";
    return "Unknown/Other";
}

int main(void)
{
    cl_int err;
    cl_uint num_platforms = 0;

    err = clGetPlatformIDs(0, NULL, &num_platforms);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Errore in clGetPlatformIDs: %d\n", err);
        return EXIT_FAILURE;
    }

    if (num_platforms == 0) {
        printf("Nessuna piattaforma OpenCL trovata sul sistema.\n");
        printf("Verifica che il driver AMD sia installato correttamente.\n");
        return EXIT_FAILURE;
    }

    printf("Trovate %u piattaforma/e OpenCL.\n\n", num_platforms);

    cl_platform_id *platforms =
        (cl_platform_id *)malloc(num_platforms * sizeof(cl_platform_id));
    if (platforms == NULL) {
        fprintf(stderr, "Allocazione memoria fallita.\n");
        return EXIT_FAILURE;
    }

    err = clGetPlatformIDs(num_platforms, platforms, NULL);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Errore in clGetPlatformIDs (fetch): %d\n", err);
        free(platforms);
        return EXIT_FAILURE;
    }

    for (cl_uint p = 0; p < num_platforms; p++) {
        char platform_name[256];
        char platform_vendor[256];
        char platform_version[256];

        clGetPlatformInfo(platforms[p], CL_PLATFORM_NAME,
                           sizeof(platform_name), platform_name, NULL);
        clGetPlatformInfo(platforms[p], CL_PLATFORM_VENDOR,
                           sizeof(platform_vendor), platform_vendor, NULL);
        clGetPlatformInfo(platforms[p], CL_PLATFORM_VERSION,
                           sizeof(platform_version), platform_version, NULL);

        printf("=======================================================\n");
        printf("Platform %u\n", p);
        printf("  Name    : %s\n", platform_name);
        printf("  Vendor  : %s\n", platform_vendor);
        printf("  Version : %s\n", platform_version);
        printf("=======================================================\n");

        cl_uint num_devices = 0;
        err = clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, 0, NULL,
                              &num_devices);
        if (err != CL_SUCCESS || num_devices == 0) {
            printf("  (Nessun device trovato per questa platform)\n\n");
            continue;
        }

        cl_device_id *devices =
            (cl_device_id *)malloc(num_devices * sizeof(cl_device_id));
        if (devices == NULL) {
            fprintf(stderr, "Allocazione memoria fallita.\n");
            continue;
        }

        clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, num_devices,
                       devices, NULL);

        for (cl_uint d = 0; d < num_devices; d++) {
            cl_device_type dtype;
            clGetDeviceInfo(devices[d], CL_DEVICE_TYPE, sizeof(dtype),
                             &dtype, NULL);

            printf("\n  --- Device %u [%s] ---\n", d,
                   device_type_to_string(dtype));

            print_device_string(devices[d], CL_DEVICE_NAME, "Name");
            print_device_string(devices[d], CL_DEVICE_VENDOR, "Vendor");
            print_device_string(devices[d], CL_DRIVER_VERSION,
                                 "Driver version");
            print_device_string(devices[d], CL_DEVICE_VERSION,
                                 "Device (OpenCL) version");
            print_device_string(devices[d], CL_DEVICE_OPENCL_C_VERSION,
                                 "OpenCL C version");

            print_device_uint(devices[d], CL_DEVICE_MAX_COMPUTE_UNITS,
                               "Max compute units");
            print_device_uint(devices[d], CL_DEVICE_MAX_CLOCK_FREQUENCY,
                               "Max clock freq (MHz)");

            print_device_size(devices[d], CL_DEVICE_MAX_WORK_GROUP_SIZE,
                               "Max work-group size");

            print_device_ulong_mb(devices[d], CL_DEVICE_GLOBAL_MEM_SIZE,
                                   "Global memory");
            print_device_ulong_kb(devices[d], CL_DEVICE_LOCAL_MEM_SIZE,
                                   "Local memory");
            print_device_ulong_mb(devices[d], CL_DEVICE_MAX_MEM_ALLOC_SIZE,
                                   "Max mem alloc size");
        }

        printf("\n");
        free(devices);
    }

    free(platforms);
    return EXIT_SUCCESS;
}