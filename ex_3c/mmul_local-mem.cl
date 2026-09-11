/**
 * Matrix multiplication kernel using local memory.
 */

 __kernel void mmul(__global float* A,
                    __global float* B,
                    __global float* C,
                    const unsigned int N,
                    __local float* Bwrk) {

    if (get_global_id(0) >= N) return;
     
    int i, j;
    i = get_global_id(0);
    int iloc = get_local_id(0);     // Local index within the work-group
    int nloc = get_local_size(0);   // Number of work-items in the work-group

    float tmp;
    float Awrk[2048];

    // Load the row of A into local memory
    for (int k = 0; k < N; k++) {
        Awrk[k] = A[i * N + k];
    }

    for (int j = 0; j < N; j++) {

        // Load the column of B into local memory
        for(int k = iloc; k < N; k += nloc) {
            Bwrk[k] = B[k * N + j];
        }

        barrier(CLK_LOCAL_MEM_FENCE);   // Ensure all work-items have loaded B into local memory before proceeding

        // Compute the dot product of the row of A and the column of B stored in local memory
        tmp = 0.0f;
        for (int k = 0; k < N; k++) {
            tmp += Awrk[k] * Bwrk[k];
        }
        C[i * N + j] = tmp;

        barrier(CLK_LOCAL_MEM_FENCE);   // Ensure all work-items have finished using local memory before the next iteration
    }
 }