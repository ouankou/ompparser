!$omp     target defaultmap( firstprivate: scalar) defaultmap(tofrom: aggregate) defaultmap(tofrom: allocatable) defaultmap(default: pointer)
!$omp     end target
!$omp     target defaultmap(none) map(tofrom: s, A, D)
!$omp     end target
!$omp     target defaultmap(tofrom: scalar) firstprivate(s1,s2)
!$omp     end target
!$omp     target defaultmap(firstprivate: aggregate ) defaultmap(firstprivate: allocatable) map(from: s1, s2)
!$omp     end target
!$omp     target defaultmap(to: all) map(from: s3)
!$omp     end target
