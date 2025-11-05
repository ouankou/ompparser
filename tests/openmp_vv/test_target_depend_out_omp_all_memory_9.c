#pragma omp task shared(x, y, errors) depend(in: x, y)
