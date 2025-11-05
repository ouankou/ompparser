!$omp        target teams distribute map(alloc: work_storage(1:1024, x),ticket(1:1)) private(my_ticket) nowait
