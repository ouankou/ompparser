#pragma omp parallel for shared(x, y, index, n)
#pragma omp atomic update
