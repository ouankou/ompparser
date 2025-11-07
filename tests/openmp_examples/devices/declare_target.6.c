#pragma omp declare target link(sp,sv1,sv2) link(dp,dv1,dv2)
#pragma omp begin declare target
#pragma omp parallel for
#pragma omp parallel for
#pragma omp end declare target
#pragma omp target map(to:sv1,sv2) map(from:sp)
#pragma omp target map(to:dv1,dv2) map(from:dp)
