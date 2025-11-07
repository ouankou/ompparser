#pragma omp threadprivate(work,size,tol)
#pragma omp parallel copyin(tol,size)
