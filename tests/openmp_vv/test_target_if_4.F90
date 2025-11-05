!$omp              target if(s > 512      ) map(to: s, a(1:s), b(1:s)) map(tofrom: c(1:s), isHost)
