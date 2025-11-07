#pragma omp parallel for private(i) shared(x, y, n) reduction(+:a) reduction(^:b) reduction(min:c) reduction(max:d)
