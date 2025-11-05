#pragma omp target map(from: singleCopy) map(from: arrayCopy[0:5]) map(tofrom: isHost) map(alloc: single, array[0:5])
