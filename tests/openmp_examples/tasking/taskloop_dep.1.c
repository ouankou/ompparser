#pragma omp taskloop nogroup
#pragma omp task_iteration depend(inout: A[i]) depend(in: A[i-1])
#pragma omp taskloop grainsize(strict: 4) nogroup
#pragma omp task_iteration depend(inout: A[i]) depend(in: A[i-4]) if ((i % 4) == 0 || i == n-1)
#pragma omp task depend(in: A[n-1])
