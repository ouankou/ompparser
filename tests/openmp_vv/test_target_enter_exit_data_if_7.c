#pragma omp target exit data if(size > 512) map(from: c[0:size])
