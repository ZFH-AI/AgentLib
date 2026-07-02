#include <sstream>
#include "hacl_rt.h"
#include "file_io.h"
#include "types.h"
#include "scalar_desc.h"
//#include "opdev/fp16_t.h"

using namespace std;

ScalarDesc::ScalarDesc(float val, aclDataType data_type, void *memoryAddress, bool set_bin_to_value_file) {
    assert(data_type == ACL_FLOAT || data_type == ACL_FLOAT16);
    data_type_ = data_type;
    if (data_type == ACL_FLOAT) {
        val_.f = val;
    } else {
        f16_ = val;
    }
    memoryAddress = memoryAddress;
    set_bin_to_value_file_ = set_bin_to_value_file;
}
// 拷贝构造函数
ScalarDesc::ScalarDesc(const ScalarDesc& desc) {
    val_ = desc.val_;
    f16_ = desc.f16_;
    data_type_ = desc.data_type_;
    memoryAddress = desc.memoryAddress;
    set_bin_to_value_file_ = desc.set_bin_to_value_file_;
    gen_data_from_onnx_ = desc.gen_data_from_onnx_;
    value_ = desc.value_;
    memoryAddressList = desc.memoryAddressList;
}
ScalarDesc::ScalarDesc(ScalarDesc&& desc) noexcept {
    val_ = desc.val_;
    f16_ = desc.f16_;
    data_type_ = desc.data_type_;
    memoryAddress = desc.memoryAddress;
    set_bin_to_value_file_ = desc.set_bin_to_value_file_;
    gen_data_from_onnx_ = desc.gen_data_from_onnx_;
    value_ = desc.value_;
    memoryAddressList = desc.memoryAddressList;
}
void ScalarDesc::freeMemoryAddress() {
    if (memoryAddress == nullptr) {
        return;
    }
    rtFree(memoryAddress);
}

// 指定Tensor取值来自某个二进制文件，必须以".bin"结尾
ScalarDesc& ScalarDesc::ValueFile(const string & binary_file) {
    static string suffix = ".bin";
    assert((binary_file.length() > suffix.length()) &&
           (binary_file.compare(binary_file.length() - suffix.length(), suffix.length(), suffix) == 0) &&
           "Binary File Must End With `.bin`");
    value_.clear();
    value_ = binary_file;
    set_bin_to_value_file_ = true;
    return *this;
}

ScalarDesc& ScalarDesc::InputNodeInfo(const string& node_name, const string& random_type) {
    value_.clear();
    value_ = node_name;

    random_type_.clear();
    random_type_ = random_type;

    gen_data_from_onnx_ = true;

    return *this;
}

void ScalarDesc::SetInt8Value(void * v) {
    val_.i8 = *reinterpret_cast<const uint8_t *>(v);
}

void ScalarDesc::SetInt16Value(void * v) {
    val_.i16 = *reinterpret_cast<const uint16_t *>(v);
}

void ScalarDesc::SetInt32Value(void * v) {
    val_.i32 = *reinterpret_cast<const uint32_t *>(v);
}

void ScalarDesc::SetInt64Value(void * v) {
    val_.i64 = *reinterpret_cast<const uint64_t *>(v);
}

void ScalarDesc::ToJson(json& root, bool is_input) const {
    (void)is_input;
    json x;
    stringstream ss;
    switch (data_type_) {
        case ACL_BOOL: ss << (val_.b ? "True" : "False"); break;
        case ACL_FLOAT: ss << val_.f; break;
        case ACL_FLOAT16: ss << float(f16_); break;
        case ACL_INT8: ss << (int32_t)(static_cast<int8_t>(val_.i8)); break;
        case ACL_UINT8: ss << static_cast<uint8_t>(val_.i8); break;
        case ACL_INT16: ss << (static_cast<int16_t>(val_.i16)); break;
        case ACL_UINT16: ss << val_.i16; break;
        case ACL_INT32: ss << (static_cast<int32_t>(val_.i32)); break;
        case ACL_UINT32: ss << val_.i32; break;
        case ACL_INT64: ss << (static_cast<int64_t>(val_.i64)); break;
        case ACL_UINT64: ss << val_.i64; break;
        case ACL_DOUBLE: ss << val_.d; break;
        default:
            break;
    }
    x["dtype"] = DataTypeToString(data_type_);
    x["value"] = ss.str();
    root.push_back(x);
}

int ScalarDesc::GetDateTypeSize(aclDataType data_type) const{
     size_t size = 1;
     switch (data_type) {
        case ACL_BOOL: size = sizeof(bool); break;
        case ACL_FLOAT: size = sizeof(float);  break;
        case ACL_FLOAT16: size = 2; break;
        case ACL_INT8: size = sizeof(int8_t); break;
        case ACL_UINT8: size = sizeof(uint8_t); break;
        case ACL_INT16: size = sizeof(int16_t);  break;
        case ACL_UINT16: size = sizeof(int16_t); break;
        case ACL_INT32: size = sizeof(int32_t); break;
        case ACL_UINT32: size = sizeof(uint32_t); break;
        case ACL_INT64: size = sizeof(int64_t); break;
        case ACL_UINT64: size = sizeof(uint64_t); break;
        case ACL_DOUBLE: size = sizeof(double); break;
        default: break;
    }
    return size;
}

// ==========================================
// 模板方法：Scalar和Tensor共有的模板函数特例化实现
// ==========================================
void DescToJson(json& root, const ScalarDesc& scalar_desc, bool is_input) {
    scalar_desc.ToJson(root, is_input);
}

ScalarDesc *InferAclType(ScalarDesc& scalar_desc) {
    return &scalar_desc;
}

ScalarDesc *DescToAclContainer(ScalarDesc& scalar_desc) {
    return new ScalarDesc(scalar_desc);
}

// ========================
// 对BIN文件读取操作
// ========================
/*<-- 读取数据 -->*/
static void* MallocAndMemcpyDeviceMemory(void* host_mem, size_t size) {
    // DEVICE侧分配内存
    void* dev_mem = nullptr;
    rtError_t error = rtMalloc(&dev_mem, size, RT_MEMORY_HBM);
    if (dev_mem == nullptr || error != RT_ERROR_NONE) {
        LogError("[scalar_desc][MallocAndMemcpyDeviceMemory] rtMalloc failed error code is " + to_string(error));
        return nullptr;
    }
    // HOST->DEVICE内存拷贝
    error = rtMemcpy(dev_mem, size, host_mem, size, RT_MEMCPY_HOST_TO_DEVICE);
    if (error != RT_ERROR_NONE) {
        if (dev_mem != nullptr) {
            rtFree(dev_mem);
            dev_mem = nullptr;
        }
    }
    return dev_mem;
}


int InitializeHostMemoryByValue(ScalarDesc *p, void* host_mem, size_t size)
{
    switch (p->data_type_) {
        case ACL_FLOAT:
            memset_s(host_mem, size, p->val_.f, size);
            break;
        case ACL_FLOAT16:
            memset_s(host_mem, size, p->f16_, size);
            break;
        case ACL_INT8:
        case ACL_UINT8:
            memset_s(host_mem, size, p->val_.i8, size);
            break;
        case ACL_INT16:
        case ACL_UINT16:
            memset_s(host_mem, size, p->val_.i16, size);
            break;
        case ACL_INT32:
        case ACL_UINT32:
            memset_s(host_mem, size, p->val_.i32, size);
            break;
        case ACL_INT64:
        case ACL_UINT64:
            memset_s(host_mem, size, p->val_.i64, size);
            break;
        case ACL_BOOL:
            memset_s(host_mem, size, p->val_.b, size);
            break;
        case ACL_DOUBLE:
            memset_s(host_mem, size, p->val_.d, size);
            break;
        default:
            return 1;
    }

    return 0;
}

static void AssignMemoryPointer(ScalarDesc* p, void* dev_mem, int32_t index) {
    LogDebug("[AssignMemoryPointer] index=[" + to_string(index) + "] single stream default value index = -1");
    if (index == -1) {
        p->memoryAddress = dev_mem;
        //printf("LoadBinaryFile p->memoryAddress = %p, dev_mem = %p\n", p->memoryAddress,dev_mem);
    } else {
        p->memoryAddressList[index] = dev_mem;
        //printf("LoadBinaryFile p->memoryAddressList[index] = %p, dev_mem = %p\n", p->memoryAddressList[index],dev_mem);
    }
}

static int ReloadDataFromBinaryFileBYSingleStream(ScalarDesc *p, size_t index, int EXEC_CNT)
{
    string prt_info;
    size_t size = 0;
    void *host_mem = nullptr;
    if (p->set_bin_to_value_file_)  {
        host_mem = ReadBinFile(p->value_, size);
        if (host_mem == nullptr) {
            return 1;
        }
        prt_info = string("Reload Customized Input BIN File ") + to_string(index) + string(" Size ") + to_string(size);
    } else if (p->gen_data_from_onnx_) {
        host_mem = ReadBinFile(p->value_, size); // 这里只包含节点名，应该还需要完整路径
        if (host_mem == nullptr) {
            return 1;
        }
        prt_info = string("Reload Customized Input BIN File Depends On Onnx") + to_string(index) + string(" Size ") + to_string(size);
    } else {
        size = p->GetDateTypeSize(p->data_type_);
        host_mem = static_cast<void *>(new(nothrow) char[size]);
        if (host_mem == nullptr) {
            return 1;
        }
        if (InitializeHostMemoryByValue(p, host_mem, size) == 1) {
            return 1;
        }
        prt_info = string("Reload Input for scalar. Index: ") + to_string(index) +  string(" Size ") + to_string(size);
    }

    LogDebug(prt_info);

    void * dev_mem = MallocAndMemcpyDeviceMemory(host_mem, size);
    if (dev_mem == nullptr) {
        return 1;
    }
    AssignMemoryPointer(p, dev_mem, EXEC_CNT);
    return 0;
}

static int ReloadDataFromBinaryFileBYSMultiStream(ScalarDesc *p, size_t index, int EXEC_CNT, bool MEM_CACHE_TEST)
{
    p->memoryAddressList = (void **)malloc(EXEC_CNT * sizeof(void *));
    if (p->memoryAddressList == nullptr) {
        LogError("[ReloadDataFromBinaryFileBYSMultiStream] Malloc memoryAddressList Failed ...");
        return 1;
    }
    int ret = ReloadDataFromBinaryFileBYSingleStream(p, index, 0);
    if (ret != 0) {
        return ret;
    }
    for (int i = 1 ; i < EXEC_CNT; i++) {
        if (MEM_CACHE_TEST) {
            ret = ReloadDataFromBinaryFileBYSingleStream(p, index, i);
            if (ret != 0) {
                return ret;
            }
        } else {
            p->memoryAddressList[i] = p->memoryAddressList[0];
        }
    }
    return 0;
}

int ReloadDataFromBinaryFile(ScalarDesc *p, size_t index, size_t count, const string& file_prefix,
                             int EXEC_CNT, bool MEM_CACHE_TEST) {
    if (p == nullptr) {
        return 0;
    }
    if (index >= count) {
        return 0;
    }

    (void)file_prefix;

    if (EXEC_CNT == -1) {
        return ReloadDataFromBinaryFileBYSingleStream(p, index, EXEC_CNT);
    }
    return ReloadDataFromBinaryFileBYSMultiStream(p, index, EXEC_CNT, MEM_CACHE_TEST);
}
