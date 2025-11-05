#pragma omp target exit data if(size > 512) map(delete: a[0:size], b[0:size])
