#pragma omp target enter data map(alloc:mat->A[:n])
#pragma omp target exit data map(delete:mat->A[:mat->N])
