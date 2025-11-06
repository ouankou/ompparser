#pragma omp for ordered(2)
#pragma omp ordered doacross(sink: i-1,j) doacross(sink: i,j-1)
#pragma omp ordered doacross(source:)
