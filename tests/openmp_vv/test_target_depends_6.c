#pragma omp task depend(inout: dep_1) depend(inout: dep_2) shared(dep_1, dep_2)
