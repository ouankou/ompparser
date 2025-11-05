!$omp                 target data map(to: my3DArray(10:50-10, i, j)) map(to: my3DArray2(10:, i, j), my3DArray3(:50-10, i, j)) map(tofrom:myTmpArray)
