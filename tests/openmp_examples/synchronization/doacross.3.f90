!$omp parallel do ordered(2) private(i,j,k,tmp1,tmp2,tmp3)
!$omp     ordered doacross(sink: i-1,j) doacross(sink: i+1,j) doacross(sink: i,j-1) doacross(sink: i,j+1)
