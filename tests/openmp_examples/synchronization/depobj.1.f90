!$omp     depobj(obj) depend(inout: a)
!$omp     depobj(obj) update(in)
!$omp     depobj(obj) destroy(obj)
!$omp    parallel num_threads(2)
!$omp      single
!$omp        task depend(depobj: obj)
!$omp        end task
!$omp        task depend(in: a)
!$omp        end task
!$omp      end single
!$omp    end parallel
