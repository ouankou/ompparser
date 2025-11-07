!$omp    parallel num_threads(2)
!$omp       single
!$omp          task depend(out:v1)
!$omp          end task
!$omp          task depend(out:v2)
!$omp          end task
!$omp          target nowait depend(in:v1,v2) depend(out:p) map(to:v1,v2) map(from: p)
!$omp          parallel do
!$omp          end target
!$omp          task depend(in:p)
!$omp          end task
!$omp      end single
!$omp    end parallel
