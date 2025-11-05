!$omp             target depend(inout: dep_1) depend(inout: dep_2) map(tofrom: dep_1(1:1024), dep_2(1:1024))
