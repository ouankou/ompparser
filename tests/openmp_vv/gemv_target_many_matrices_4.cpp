#pragma omp target teams distribute map(to:A[:n*n], V[:n]) map(from:Vout[:n])
