/**
 * Test mmul (F9): C = A * B in memoria globale, un elemento di C per work-item.
 * A è M x K, B è K x N, C è M x N (righe contigue).
 *
 * col sulla dimensione 0: work-item vicini leggono B e scrivono C in posizioni
 * contigue (diverso da ex_3, dove la dimensione 0 era la riga).
 */
__kernel void mmul(__global const float *A,
                   __global const float *B,
                   __global float *C,
                   const unsigned int M,
                   const unsigned int N,
                   const unsigned int K)
{
    uint col = get_global_id(0);
    uint row = get_global_id(1);

    if (row < M && col < N) {
        float sum = 0.0f;
        for (uint k = 0; k < K; k++) {
            sum += A[row * K + k] * B[k * N + col];
        }
        C[row * N + col] = sum;
    }
}
