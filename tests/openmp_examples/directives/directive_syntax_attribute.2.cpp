#pragma omp threadprivate(u, v)
#pragma omp declare_target(a)
#pragma omp declare_target link(b)
#pragma omp declare_target(f)
#pragma omp begin declare_target
#pragma omp end declare_target
