!$omp     target map(tofrom:compute_array(:, p_val)) firstprivate(p_val)
