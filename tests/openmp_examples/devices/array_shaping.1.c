#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target data map(a[0:nx*(ny+2)])
#pragma omp target
#pragma omp target update from( (([nx][ny+2])a)[0:nx][1], (([nx][ny+2])a)[0:nx][ny] )
#pragma omp target update to( (([nx][ny+2])a)[0:nx][0], (([nx][ny+2])a)[0:nx][ny+1] )
#pragma omp target
