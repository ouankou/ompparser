!$omp    declare target (init)
!$omp    target data map(v1,v2)
!$omp    task shared(v1,v2) depend(out: N)
!$omp       target device(idev)
!$omp       end target
!$omp    end task
!$omp    task shared(v1,v2,p) depend(in: N)
!$omp       target device(idev) map(from: p)
!$omp          parallel do
!$omp       end target
!$omp    end task
!$omp    taskwait
!$omp    end target data
