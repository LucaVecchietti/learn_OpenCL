/**
 * Matrix multiplication kernel
 * Each work-item computes one element of the output matrix C
 */

 __kernel void mmul(
    __global const float *A,
    __global const float *B,
    __global float *C,
    const unsigned int count
 )
 {
    int row = get_global_id(0);
    int col = get_global_id(1);

    if (row < count && col < count) {
        float sum = 0.0f;
        for (int k = 0; k < count; k++) {
            sum += A[row * count + k] * B[k * count + col];
        }
        C[row * count + col] = sum;
    }
 }