!$omp         declare mapper(custom: newvec :: v) map(to: v, v%data(1:v%len))
