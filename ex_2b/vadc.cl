/**
 * Vector addition kernel
 * Each work-item computes one element of the output vector
 */

__kernel void vadc(__global const float *a,
                   __global const float *b,
                   __global const float *c,
                   __global float *d,
                   const unsigned int vector_size) {
    int gid = get_global_id(0);
    if (gid < vector_size) {
        d[gid] = a[gid] + b[gid] + c[gid];
    }
}