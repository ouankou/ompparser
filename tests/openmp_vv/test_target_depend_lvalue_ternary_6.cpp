#pragma omp task depend(out: (a > b) ? b : c) shared(a,b)
