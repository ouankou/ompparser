#pragma omp target if(size > 512) map(to: a[0:size], b[0:size]) map(to: c[0:size])
