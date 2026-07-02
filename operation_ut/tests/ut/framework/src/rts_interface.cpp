#include <cstdlib>
#include <iostream>
#include <map>
#include "rts_interface.h"
#include "types.h"
#include <acl/acl.h>

using namespace std;

int RtsInit() {
    aclInit(NULL);
    auto rtRet = rtSetDevice(DEVICE_ID);
    if (rtRet != RT_ERROR_NONE) {
        std::cout << "RtsInit rtSetDevice failed.."<< std::endl;
        return 1;
    }
    return 0;
}

void RtsUnInit() {
    rtError_t rtRet = rtDeviceReset(DEVICE_ID);
    if (rtRet != RT_ERROR_NONE) {
        std::cout << "RtsUnInit rtDeviceReset failed.."<< rtRet << std::endl;
    }
    aclFinalize();
}

void RtsCreateStream(rtStream_t *stream) {
    rtError_t ret = rtStreamCreate(stream, 0);
    if (ret != RT_ERROR_NONE) {
        cout << "RtsCreateStream rtStreamSynchronize failed, return.."<< endl;
    }
}

int SynchronizeStream(rtStream_t stream) {
    rtError_t ret = rtStreamSynchronize(stream);
    if (ret != RT_ERROR_NONE) {
        cout << "SynchronizeStream rtStreamSynchronize failed, return.."<< endl;
        return 1;
    }
    return 0;
}
