/*************************************************************************
 * Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All rights reserved.
 *
 * See LICENSE.txt for license information
 ************************************************************************/

#include "test_GatherMultiProcess.hpp"

namespace CorrectnessTests
{
    TEST_P(GatherMultiProcessCorrectnessTest, Correctness)
    {
        Dataset* dataset = (Dataset*)mmap(NULL, sizeof(Dataset), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
        dataset->InitializeRootProcess(numDevices, numElements, dataType, inPlace, ncclCollGather);
        Barrier::ClearShmFiles(std::atoi(getenv("NCCL_COMM_ID")));

        int pid1 = 0;
        int pid2 = 0;
        int pid3 = 0;
        pid1 = fork();

        // From this point on, ignore original process as we cannot have it create a HIP context
        if (pid1 == 0)
        {
            pid2 = fork();
            if (numDevices > 2)
            {
                pid3 = fork();
            }
            if ((pid2 > 0 && pid3 == 0 && numDevices == 2)  || (pid2 > 0 && pid3 > 0 && numDevices > 2))
            {
                // Process 0
                TestGather(0, *dataset);
                if (pid3 > 0)
                {
                    waitpid(pid3, NULL, 0);
                }
            }
            else if ((pid2 == 0 && pid3 == 0 && numDevices == 2) || (pid2 == 0 && pid3 > 0 && numDevices > 2))
            {
                // Process 1
                TestGather(1, *dataset);
                if (numDevices > 2)
                {
                    waitpid(pid3, NULL, 0);
                }
                exit(0);
            }
            else if (pid2 > 0 && pid3 == 0 && numDevices > 2)
            {
                // Process 2 (available when numDevices > 2)
                TestGather(2, *dataset);
                exit(0);
            }
            else if (pid2 == 0 && pid3 == 0 && numDevices == 4)
            {
                // Process 3 (available when numDevices == 4)
                TestGather(3, *dataset);
                exit(0);
            }
            else
            {
                exit(0);
            }
            waitpid(pid2, NULL, 0);
            exit(0);
        }
        waitpid(pid1, NULL, 0);
        munmap(dataset, sizeof(Dataset));
    }

    INSTANTIATE_TEST_SUITE_P(GatherMultiProcessCorrectnessSweep,
                            GatherMultiProcessCorrectnessTest,
                            testing::Combine(
                                // Reduction operator is not used
                                testing::Values(ncclSum),
                                // Data types
                                testing::Values(ncclInt8,
                                                ncclUint8,
                                                ncclInt32,
                                                ncclUint32,
                                                ncclInt64,
                                                ncclUint64,
                                                //ncclFloat16,
                                                ncclFloat32,
                                                ncclFloat64,
                                                ncclBfloat16),
                                // Number of elements
                                testing::Values(1024, 1048576),
                                // Number of devices
                                testing::Values(2,3,4),
                                // In-place or not
                                testing::Values(false),
                                testing::Values("RCCL_ALLTOALL_KERNEL_DISABLE=0", "RCCL_ALLTOALL_KERNEL_DISABLE=1")),
                            CorrectnessTest::PrintToStringParamName());
} // namespace
