#pragma omp for collapse(2) ordered(2)
#pragma omp ordered doacross(source:)
#pragma omp ordered doacross(sink: i-1,j) doacross(sink: i,j-1)
