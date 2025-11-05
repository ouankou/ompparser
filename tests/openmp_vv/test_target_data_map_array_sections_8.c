#pragma omp target data map(from: a3d[1:1000 - 2][0:2][0:2]) map(from: a3d2[0:1000][0:2][0:2])
