#pragma omp target depend(inout: dep_1) depend(inout: dep_2) map(tofrom: dep_1[0:1000]) map(tofrom: dep_2[0:1000])
