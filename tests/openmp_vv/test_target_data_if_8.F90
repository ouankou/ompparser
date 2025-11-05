!$omp              target data if(s > 512      ) map(to: a(1:s), b(1:s))map(tofrom: c(1:s))
