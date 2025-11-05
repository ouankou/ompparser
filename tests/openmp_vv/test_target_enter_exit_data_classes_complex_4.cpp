#pragma omp target enter data map(to: helper_ptr[0:1]) if(not_mapped)
