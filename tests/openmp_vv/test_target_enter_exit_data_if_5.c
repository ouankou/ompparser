#pragma omp target enter data if(size > 512) map(to: size) map(to: c[0:size])
