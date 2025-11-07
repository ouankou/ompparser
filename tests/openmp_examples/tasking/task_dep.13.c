#pragma omp parallel masked num_threads(5)
#pragma omp task
#pragma omp task depend(out: a)
#pragma omp task depend(out: d)
#pragma omp task depend(inout: omp_all_memory)
#pragma omp task
#pragma omp task depend(in: a,d)
