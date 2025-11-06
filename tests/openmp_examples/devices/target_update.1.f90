!$omp    target data map(to: v1, v2) map(from: p)
!$omp       target
!$omp       parallel do
!$omp       end target
!$omp       target update to(v1, v2)
!$omp       target
!$omp       parallel do
!$omp       end target
!$omp    end target data
