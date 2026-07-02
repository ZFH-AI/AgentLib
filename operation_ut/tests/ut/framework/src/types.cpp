#include <assert.h>
#include "types.h"
#include <map>

const std::string & DataTypeToString(aclDataType data_type) {
    static std::map<aclDataType, std::string> dtype = {
        {ACL_DT_UNDEFINED, "undefined"},
        {ACL_FLOAT, "float32"},
        {ACL_FLOAT16, "float16"},
        {ACL_INT8, "int8"},
        {ACL_INT32, "int32"},
        {ACL_UINT8, "uint8"},
        {ACL_INT16, "int16"},
        {ACL_UINT16, "uint16"},
        {ACL_UINT32, "uint32"},
        {ACL_INT64, "int64"},
        {ACL_UINT64, "uint64"},
        {ACL_DOUBLE, "double"},
        {ACL_BOOL, "bool"},
        {ACL_STRING, "string"},
        {ACL_COMPLEX64, "complex64"},
        {ACL_COMPLEX128, "complex128"},
        {ACL_BF16, "bfloat16"}
    };
    auto iter = dtype.find(data_type);
    assert(iter != dtype.end());
    return iter->second;
}

const std::string getFormatName(aclFormat format) {
    static const std::unordered_map<aclFormat, std::string> formatMap = {
        {ACL_FORMAT_UNDEFINED, "UNDEFINED"},
        {ACL_FORMAT_NCHW, "NCHW"},
        {ACL_FORMAT_NHWC, "NHWC"},
        {ACL_FORMAT_ND, "ND"},
        {ACL_FORMAT_NC1HWC0, "NC1HWC0"},
        {ACL_FORMAT_FRACTAL_Z, "FRACTAL_Z"},
        {ACL_FORMAT_NC1HWC0_C04, "NC1HWC0_C04"},
        {ACL_FORMAT_HWCN, "HWCN"},
        {ACL_FORMAT_NDHWC, "NDHWC"},
        {ACL_FORMAT_FRACTAL_NZ, "FRACTAL_NZ"},
        {ACL_FORMAT_NCDHW, "NCDHW"},
        {ACL_FORMAT_NDC1HWC0, "NDC1HWC0"},
        {ACL_FRACTAL_Z_3D, "FRACTAL_Z_3D"},
        {ACL_FORMAT_NC, "NC"},
        {ACL_FORMAT_NCL, "NCL"}
    };

    auto it = formatMap.find(format);
    if (it != formatMap.end()) {
        return it->second;
    }
    return "UNKNOWN_FORMAT";
}
