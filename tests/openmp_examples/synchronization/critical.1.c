#pragma omp parallel shared(x, y) private(ix_next, iy_next)
#pragma omp critical (xaxis)
#pragma omp critical (yaxis)
