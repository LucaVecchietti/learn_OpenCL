/**
 * Mini Benchmark for OpenCL
 * 
 * Read the device and the platform information. Select automaticaly the device to be used for the benchmark.
 * The benchmark will then run a series of tests to measure the performance of the selected device.
 * The results will be displayed on the console.
 * 
 * The benchmark says which is the best performing device and the best configuration for it and for the kernel.
 */

#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>

/*
Functions to GET and print device and platform information based on type.
*/

static void print_device_info_uint(cl_device_id device, cl_device_info param);
static void print_device_info_string(cl_device_id device, cl_device_info param);
static void print_device_info_bool(cl_device_id device, cl_device_info param);
static void print_device_info_cl_exec_capabilities(cl_device_id device, cl_device_info param);
static void print_device_info_ulong(cl_device_id device, cl_device_info param);
static void print_device_info_size_t(cl_device_id device, cl_device_info param);
static void print_device_info_uint(cl_device_id device, cl_device_info param);

static void print_platform_info_string(cl_platform_id platform, cl_platform_info param);
static void print_platform_info_ulong(cl_platform_id platform, cl_platform_info param);

int main(void)
{
    return 0;
}