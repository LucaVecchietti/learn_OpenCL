/**
 * Test stencil3d (F10): stencil 3D a 7 punti.
 *   out = c0 * centro + c1 * (somma dei 6 vicini), c0 = 0.4, c1 = 0.1
 * Le celle sul bordo della griglia sono copiate invariate (Q-36).
 * Indice lineare (z * Y + y) * X + x: la dimensione 0 è contigua in memoria.
 */
__kernel void stencil3d(__global const float *in,
                        __global float *out,
                        const unsigned int X,
                        const unsigned int Y,
                        const unsigned int Z)
{
    uint x = get_global_id(0);
    uint y = get_global_id(1);
    uint z = get_global_id(2);
    if (x >= X || y >= Y || z >= Z) {
        return;
    }

    size_t sy = X;
    size_t sz = (size_t)X * Y;
    size_t i = z * sz + y * sy + x;

    if (x == 0 || y == 0 || z == 0 || x == X - 1 || y == Y - 1 || z == Z - 1) {
        out[i] = in[i];
        return;
    }

    out[i] = 0.4f * in[i] +
             0.1f * (in[i - 1] + in[i + 1] + in[i - sy] + in[i + sy] + in[i - sz] + in[i + sz]);
}
