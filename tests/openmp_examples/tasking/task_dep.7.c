#pragma omp task depend(inout: x) shared(x)
#pragma omp task depend(in: x) depend(inout: y) shared(x, y)
#pragma omp taskwait depend(in: x)
#pragma omp taskwait
#pragma omp parallel
#pragma omp single
