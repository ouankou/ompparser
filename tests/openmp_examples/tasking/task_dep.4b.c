#pragma omp parallel private(i,v) shared(R)
#pragma omp single
#pragma omp task depend(inout: R)
#pragma omp task depend(in: R)
#pragma omp taskwait
#pragma omp task depend(inoutset: R)
#pragma omp atomic
#pragma omp task depend(in: R)
