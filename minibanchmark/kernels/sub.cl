/**
 * Test 1D elemento per elemento: C[i] = A[i] - B[i] (differenza).
 *
 * @param A Primo array di input.
 * @param B Secondo array di input.
 * @param C Array di output.
 * @param n Numero di elementi (dimensione effettiva).
 */
__kernel void sub(__global const float *A,
                  __global const float *B,
                  __global float *C,
                  const unsigned int n)
{
    size_t i = get_global_id(0);
    if (i < n) {
        C[i] = A[i] - B[i];
    }
}
