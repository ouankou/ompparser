!$omp    target data map(from: p)
!$omp       target map(to: v1, v2 )
!$omp          parallel do
!$omp       end target
!$omp       target map(to: v1, v2 )
!$omp          parallel do
!$omp       end target
!$omp    end target data
