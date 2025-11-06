#pragma omp parallel shared(a, b, c, d, x, y, n) private(a_p, b_p, c_p, d_p)
#pragma omp for private(i)
#pragma omp critical
