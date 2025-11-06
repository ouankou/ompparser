#pragma omp parallel
#pragma omp single
#pragma omp task shared(x) depend(out: x)
#pragma omp task shared(x) depend(inout: x) if(0)
