#pragma omp target enter data map(alloc:v[0:len])
#pragma omp target exit data map(delete:v[0:len])
