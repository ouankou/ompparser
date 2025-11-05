!$omp     begin metadirective when(device={arch("nvptx")}: teams distribute parallel do) default(parallel do)
