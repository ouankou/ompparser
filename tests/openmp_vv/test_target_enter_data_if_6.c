#pragma omp target enter data if(size > 512) map(to: size) map (to: a[0:size], b[0:size])
