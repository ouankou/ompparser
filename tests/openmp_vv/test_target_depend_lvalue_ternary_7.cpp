#pragma omp task depend(in: (a > b) ? b : c) shared(a, b)
