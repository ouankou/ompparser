!$omp    target data map(to: v1, v2) map(from: p)
!$omp       target
!$omp          parallel do
!$omp       end target
!$omp       target update if(changed) to(v1(:N))
!$omp       target update if(changed) to(v2(:N))
!$omp       target
!$omp          parallel do
!$omp       end target
!$omp    end target data
