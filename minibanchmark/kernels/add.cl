/**
 * OpenCL kernel for element-wise addition of two arrays.
 *
 * @param A The first input array.
 * @param B The second input array.
 * @param C The output array where the result will be stored.
 * @param buffer_size The size of the input and output arrays.
 */
__kernel void add(__global const float *A,
                  __global const float *B,
                  __global float *C,
                  __global const unsigned int *buffer_size)
{
    size_t i = get_global_id(0);
    if (i < *buffer_size) {
        C[i] = A[i] + B[i];
    }
}