/**
 * Matrix multiplication using private memory in OpenCL.
 *
 * This kernel demonstrates how to perform matrix multiplication
 * efficiently by utilizing private memory to store intermediate
 * results for each work-item.
 */

 __kernel void matrix_multiply_private(__global const float *A,
                                       __global const float *B,
                                       __global float *C,
                                       const unsigned int count) {
    int gid = get_global_id(0);
    if (gid >= count * count ) return;

    int j, k;

    float tmp;
    float Awrk[2048]; // Private memory of the current work_item

    // Copy one time the row of matrix A into private memory
    for (k = 0; k < count; k++) {
        Awrk[k] = A[gid * count + k];
    }

    for (j = 0; j < count; j++) {
        tmp = 0.0f;
        for (k = 0; k < count; k++) {
            tmp += Awrk[k] * B[k * count + j];
        }
        C[gid * count + j] = tmp;
    }
 }