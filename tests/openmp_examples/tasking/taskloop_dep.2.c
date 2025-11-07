#pragma omp taskloop collapse(2) nogroup
#pragma omp task_iteration depend(inout: B[i][j]) depend(in: B[i-1][j], B[i][j-1])
#pragma omp task depend(in: B[n-1][n-1])
