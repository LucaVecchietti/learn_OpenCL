/**
 * Test mmul_tiled (F9): C = A * B a tile TS x TS in local memory.
 * TS è il lato del work-group (forma solo quadrata, vincolo EQUAL).
 * Richiede M, N e K multipli di TS: M e N lo sono per costruzione (dimensione
 * effettiva), K è controllato dal setup del test.
 *
 * As e Bs: TS * TS float ciascuno, passati dal test con clSetKernelArg(..., NULL).
 */
__kernel void mmul_tiled(__global const float *A,
                         __global const float *B,
                         __global float *C,
                         const unsigned int M,
                         const unsigned int N,
                         const unsigned int K,
                         __local float *As,
                         __local float *Bs)
{
    uint ts = get_local_size(0);
    uint lx = get_local_id(0);
    uint ly = get_local_id(1);
    uint col = get_global_id(0);
    uint row = get_global_id(1);

    float sum = 0.0f;
    for (uint t = 0; t < K; t += ts) {
        /* Ogni work-item carica un elemento della tile di A e uno di B. */
        As[ly * ts + lx] = A[row * K + t + lx];
        Bs[ly * ts + lx] = B[(t + ly) * N + col];
        barrier(CLK_LOCAL_MEM_FENCE);

        for (uint k = 0; k < ts; k++) {
            sum += As[ly * ts + k] * Bs[k * ts + lx];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if (row < M && col < N) {
        C[row * N + col] = sum;
    }
}
