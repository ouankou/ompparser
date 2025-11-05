#pragma omp target map(alloc: a3d[1:1000 - 2][0:2][0:2] ,a3d2[0:1000][0:2][0:2])
