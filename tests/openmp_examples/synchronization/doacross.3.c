#pragma omp parallel for ordered(2) private(i,j,k)
#pragma omp ordered doacross(sink: i-1,j) doacross(sink: i+1,j) doacross(sink: i,j-1) doacross(sink: i,j+1)
