#pragma omp target exit data map(delete: helper_ptr[0:1]) if(not_mapped)
