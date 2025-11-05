#pragma omp target teams loop map(tofrom : x [0:n]) map(to : y [0:n])
