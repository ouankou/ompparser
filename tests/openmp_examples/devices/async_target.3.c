#pragma omp parallel
#pragma omp masked
#pragma omp target teams distribute parallel for nowait map(to: v1[0:n/2]) map(to: v2[0:n/2]) map(from: vxv[0:n/2])
#pragma omp for schedule(dynamic,chunk)
