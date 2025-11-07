#pragma omp parallel
#pragma omp for collapse(2) lastprivate(jlast, klast)
#pragma omp single
