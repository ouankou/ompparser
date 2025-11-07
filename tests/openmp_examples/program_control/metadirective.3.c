#pragma omp begin declare target
#pragma omp metadirective when( construct={target}: distribute parallel for ) otherwise( parallel for simd )
#pragma omp end declare target
#pragma omp target teams map(tofrom: d[0:1000])
