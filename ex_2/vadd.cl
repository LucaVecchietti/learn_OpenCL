/*
 * vadd.cl
 *
 * Kernel OpenCL per la somma di due vettori: C[i] = A[i] + B[i]
 */

__kernel void vadd(
    __global const float *a,
    __global const float *b,
    __global float *c,
    const unsigned int count)
{
    int gid = get_global_id(0);

    if (gid < count) {
        c[gid] = a[gid] + b[gid];
    }
}