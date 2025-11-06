!$omp   do collapse(2) ordered(2)
!$omp     ordered doacross(source:)
!$omp     ordered doacross(sink: j,i-1) doacross(sink: j-1,i)
