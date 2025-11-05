!$omp     target map(tofrom:compute_array(:,fp_val)) map(to:fp_val) private(p_val)
