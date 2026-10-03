/**
 * Test max (F8): riduzione, massimo di ogni work-group in partial[group].
 * La chiusura (massimo dei parziali) si fa sull'host, fuori dalla misura.
 *
 * Il dimezzamento arrotonda per eccesso, quindi funziona con qualsiasi dimensione
 * del work-group, non solo con le potenze di 2 (RB-2 prova anche 96, 160...).
 * Il ciclo dipende solo da get_local_size: è uniforme nel gruppo, il barrier è legale.
 *
 * Il kernel si chiama max_reduce perché max è una funzione predefinita di OpenCL C.
 */
__kernel void max_reduce(__global const float *A,
                         __global float *partial,
                         __local float *scratch,
                         const unsigned int n)
{
    size_t gid = get_global_id(0);
    uint lid = get_local_id(0);
    uint s = get_local_size(0);

    scratch[lid] = (gid < n) ? A[gid] : -INFINITY;
    barrier(CLK_LOCAL_MEM_FENCE);

    while (s > 1) {
        uint h = (s + 1) / 2;
        if (lid < s / 2) {
            scratch[lid] = fmax(scratch[lid], scratch[lid + h]);
        }
        barrier(CLK_LOCAL_MEM_FENCE);
        s = h;
    }

    if (lid == 0) {
        partial[get_group_id(0)] = scratch[0];
    }
}
