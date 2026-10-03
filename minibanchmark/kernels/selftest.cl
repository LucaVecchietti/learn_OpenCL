/**
 * Kernel dei test di prova (attivi solo compilando con -DBENCH_SELFTEST, F11).
 */

/* Ciclo lunghissimo: deve superare il tempo massimo (CA-13). Il risultato finisce
 * in output così il compilatore non può eliminare il ciclo. */
__kernel void selftest_timeout(__global float *out, const unsigned int iterations)
{
    float x = (float)get_global_id(0);
    for (unsigned int i = 0; i < iterations; i++) {
        x = x * 1.0000001f + 0.5f;
    }
    out[get_global_id(0)] = x;
}

/* Usa local memory proporzionale al work-group: le configurazioni grandi
 * superano la local memory del device e vanno scartate (CA-3). */
__kernel void selftest_localmem(__global float *out, __local float *scratch, const unsigned int n)
{
    size_t g = get_global_id(0);
    size_t l = get_local_id(0);
    scratch[l] = (float)g;
    barrier(CLK_LOCAL_MEM_FENCE);
    if (g < n) {
        out[g] = scratch[l];
    }
}
