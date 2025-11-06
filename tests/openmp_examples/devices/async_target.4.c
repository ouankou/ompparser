#pragma omp parallel num_threads(2)
#pragma omp single
#pragma omp task depend(out:v1)
#pragma omp task depend(out:v2)
#pragma omp target nowait depend(in:v1,v2) depend(out:p) map(to:v1,v2) map( from: p)
#pragma omp parallel for private(i)
#pragma omp task depend(in:p)
