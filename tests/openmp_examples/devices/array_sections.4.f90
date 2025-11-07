!$omp    target data map( A(1:10) )
!$omp      target map( p(4:10) )
!$omp      end target
!$omp    end target data
