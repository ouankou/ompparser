!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp             target depend(out: dep_1) map(tofrom: dep_1(1:1024))
!$omp             end target
!$omp             target depend(out: dep_2) map(tofrom: dep_2(1:1024))
!$omp             end target
!$omp             task depend(inout: dep_1) depend(inout: dep_2) shared(dep_1, dep_2)
!$omp             end task
!$omp             target depend(inout: dep_1) depend(inout: dep_2) map(tofrom: dep_1(1:1024), dep_2(1:1024))
!$omp             end target
!$omp             target depend(in: dep_1) depend(in: dep_2) map(tofrom:dep_1(1:1024)) map(tofrom: dep_2(1:1024))
!$omp             end target
!$omp             taskwait
