/*************************************************************************
 * Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All rights reserved.
 *
 * See LICENSE.txt for license information
 ************************************************************************/
#include "test_CombinedCallsMultiProcess.hpp"

namespace CorrectnessTests
{
    TEST_P(CombinedCallsMultiProcessCorrectnessTest, Correctness)
    {
        // Important: Make sure the order of ncclFunc_t's here match the order of ncclFunc_ts
        // as they appear in TestCombinedCalls()
        std::vector<ncclFunc_t> ncclFuncs;
        ncclFuncs.push_back(ncclCollAllGather);
        ncclFuncs.push_back(ncclCollAllReduce);
        ncclFuncs.push_back(ncclCollBroadcast);
        ncclFuncs.push_back(ncclCollReduce);
        ncclFuncs.push_back(ncclCollReduceScatter);

        // Create multiple datasets for combined operation
        std::vector<Dataset*> datasets(ncclFuncs.size());
        for (int i = 0; i < datasets.size(); i++)
        {
            datasets[i] = (Dataset*)mmap(NULL, sizeof(Dataset), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
            datasets[i]->InitializeRootProcess(numDevices, numElements, dataType, inPlace, ncclFuncs[i]);
        }
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
                TestCombinedCalls(0, datasets, ncclFuncs);
                if (pid3 > 0)
                {
                    waitpid(pid3, NULL, 0);
                }
            }
            else if ((pid2 == 0 && pid3 == 0 && numDevices == 2) || (pid2 == 0 && pid3 > 0 && numDevices > 2))
            {
                // Process 1
                TestCombinedCalls(1, datasets, ncclFuncs);
                if (numDevices > 2)
                {
                    waitpid(pid3, NULL, 0);
                }
                exit(0);
            }
            else if (pid2 > 0 && pid3 == 0 && numDevices > 2)
            {
                // Process 2 (available when numDevices > 2)
                TestCombinedCalls(2, datasets, ncclFuncs);
                exit(0);
            }
            else if (pid2 == 0 && pid3 == 0 && numDevices == 4)
            {
                // Process 3 (available when numDevices == 4)
                TestCombinedCalls(3, datasets, ncclFuncs);
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
        for (int i = 0; i < datasets.size(); i++)
        {
            munmap(datasets[i], sizeof(Dataset));
        }
    }

    INSTANTIATE_TEST_SUITE_P(CombinedCallsMultiProcessCorrectnessSweep,
                            CombinedCallsMultiProcessCorrectnessTest,
                            testing::Combine(
                                // Reduction operator (not used)
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
                                testing::Values(3072, 3145728),
                                // Number of devices
                                testing::Values(2,3,4),
                                // In-place or not
                                testing::Values(false, true),
                                testing::Values("")),
                            CorrectnessTest::PrintToStringParamName());
} // namespace
