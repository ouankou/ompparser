!$omp               target data map(from: my2DArray(10:50-10, i)) map(from: my2DArray2(10:, i), my2DArray3(:50-10, i)) map(tofrom: myTmpArray)
