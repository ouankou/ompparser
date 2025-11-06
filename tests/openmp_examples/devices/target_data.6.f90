!$omp    target data if(N>THRESHOLD) map(from: p)
!$omp       target if(N>THRESHOLD) map(to: v1, v2)
!$omp          parallel do
!$omp       end target
!$omp       target if(N>THRESHOLD) map(to: v1, v2)
!$omp          parallel do
!$omp       end target
!$omp    end target data
