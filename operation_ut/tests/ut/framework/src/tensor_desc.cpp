#include "hacl_rt.h"
#include "c_shell.h"
#include "file_io.h"
#include "tensor_desc.h"
#include "types.h"

using namespace std;
using namespace nlohmann;

/*<------------------ TensorDesc 类相关    ------------------------>*/
TensorDesc::TensorDesc(const vector<int64_t>& view_dims,
                       aclDataType data_type,
                       aclFormat format,
                       const vector<int64_t>& stride,
                       int64_t offset,
                       const vector<int64_t>& storage_dims,
                       void *memoryAddress,
                       void **memoryAddressList,
                       bool set_bin_to_value_file)
{
    view_dims_ = view_dims;
    data_type_ = data_type;
    format_ = format;
    stride_ = stride;
    offset_ = offset;
    storage_dims_ = storage_dims;
    memoryAddress = memoryAddress;
    memoryAddressList = memoryAddressList;
    set_bin_to_value_file_ = set_bin_to_value_file;
    tensorSize = GetViewCount(view_dims_) * GetDateTypeSize(data_type_);
    precision_typeid = 0;
}

// 拷贝构造函数
TensorDesc::TensorDesc(const TensorDesc& desc) {
    view_dims_ = desc.view_dims_;
    data_type_ = desc.data_type_;
    format_ = desc.format_;
    stride_ = desc.stride_;
    offset_ = desc.offset_;
    storage_dims_ = desc.storage_dims_;
    precision_ = desc.precision_;
    value_range_ = desc.value_range_;
    value_ = desc.value_;
    valid_count_ = desc.valid_count_;
    memoryAddress = desc.memoryAddress;
    memoryAddressList = desc.memoryAddressList;
    set_bin_to_value_file_ = desc.set_bin_to_value_file_;
    gen_data_from_onnx_ = desc.gen_data_from_onnx_;
    tensorSize = desc.tensorSize;
    precision_typeid = desc.precision_typeid;
}

//TensorDesc::TensorDesc(const TensorDesc && desc) noexcept
//    : view_dims_(std::move(desc.view_dims_)),
//    data_type_(std::move(desc.data_type_)),
//    format_(std::move(desc.format_)),
//    stride_(std::move(desc.stride_)),
//    offset_(std::move(desc.offset_)),
//    storage_dims_(std::move(desc.storage_dims_)),
//    precision_(std::move(desc.precision_)),
//    value_range_(std::move(desc.value_range_)),
//    value_(std::move(desc.value_)),
//    valid_count_(std::move(desc.valid_count_)),
//    tensorSize(std::move(desc.tensorSize)),
//    memoryAddress(std::move(desc.memoryAddress)),
//    memoryAddressList(std::move(memoryAddressList)),
//    set_bin_to_value_file_(std::move(desc.set_bin_to_value_file_),
//    gen_data_from_onnx_(std::move(desc.gen_data_from_onnx_)),
//    precision_typeid(std::move(desc.precision_typeid))) { }

TensorDesc::~TensorDesc() { }

void TensorDesc::ToJson(json& root, bool is_input) const {
    json x;
    x["dtype"] = DataTypeToString(data_type_);
    x["format"] = getFormatName(format_);
    x["view_shape"] = view_dims_;
    x["storage_shape"] = storage_dims_.empty() ? view_dims_ : storage_dims_;
    x["offset"] = offset_;
    x["set_bin_to_value_file"] = set_bin_to_value_file_;
    x["gen_data_from_onnx"] = gen_data_from_onnx_;
    if (!stride_.empty()) {
        x["stride"] = stride_;
    }

    if (is_input) {
        x["value_range"] = value_range_;
        if (!value_.empty()) {
            x["value"] = value_;
        }
    }

    if (set_bin_to_value_file_) {
        x["value"] = value_;
    }

    if (gen_data_from_onnx_) {
        x["value"] = value_;
        x["random_type"] = random_type_;
    }

    if (!precision_.empty()) {
        x["precision"] = precision_;  // in case of inplace
        x["precision_typeid"] = precision_typeid;
    }

    if (valid_count_ >= 0) {
        x["valid_count"] = valid_count_; // count in output need to check.
    }
    root.push_back(x);
}

TensorDesc& TensorDesc::ViewDims(const vector<int64_t>& view_dims) {
    view_dims_ = view_dims;
    return *this;
}

TensorDesc& TensorDesc::Format(aclFormat format) {
    format_ = format;
    return *this;
}

/**
默认比数函数的参数解析：
Precision(A, B, C)
A: 相对误差(rtol): 表示两个数的相对差异在多大程度上才会被认为是相等
C: 绝对误差(atol): 表示两个数的绝对差异在多大程度上才会被认为是相等
    allowed_error= rtol× ∣golden∣ + atol
    举个栗子：
     若 golden = 10, rtol=0.1, atol=0.01
        allowed_error = 10 * 0.1 + 0.1 = 1.01
     则 output 在[10 - allowed_error, 10 + allowed_error]=[8.99,11.01]范围内均视为接近

B: output数据集和golden数据集中满足allowed_error的元素个数占golden数据集总元素的占比
       diffNum = 对output数据集和golden数据集中逐元素统计满足此条件 ( ∣output−golden∣ > allowed_error )的个数
       goldenNum = golden数据集中的元素个数
       IF diffNum / goldenNum > B THEN NO PASS ELSE PASS
**/

TensorDesc& TensorDesc::Precision(const std::vector<float>& precision_value, int precision_mode) {
    stringstream ss;
    // 默认的比数参数
    if (precision_mode == 0) {
        assert(precision_value[0] >= 0 && precision_value[1] >= 0 && precision_value[2] >= 0);
        ss << "(" << precision_value[0] << "," << precision_value[1] << "," << precision_value[2] << ")";
    } else {
    // 自定义比数参数
        size_t pre_num = precision_value.size();
        assert(pre_num > 0);
        ss << "(";
        for (size_t i = 0; i < pre_num; ++i) {
            ss << precision_value[i];
            if (i != pre_num -1) {
                ss << ",";
            }
        }
        ss << ")";
    }
    precision_.clear();
    ss >> precision_;
    precision_typeid = precision_mode;
    return *this;
}
// 指定Tensor取值来自某个二进制文件，必须以".bin"结尾
TensorDesc& TensorDesc::ValueFile(const string & binary_file) {
    const std::string suffix = ".bin";
    assert((binary_file.length() > suffix.length()) &&
           (binary_file.compare(binary_file.length() - suffix.length(), suffix.length(), suffix) == 0) &&
           "Binary File Must End With `.bin`");
    value_.clear();
    value_ = binary_file;
    set_bin_to_value_file_ = true;
    return *this;
}

TensorDesc& TensorDesc::InputNodeInfo(const string& node_name, const string& random_type) {
    value_.clear();
    value_ = node_name;

    random_type_.clear();
    random_type_ = random_type;

    gen_data_from_onnx_ = true;

    return *this;
}

TensorDesc& TensorDesc::ValidCount(int32_t cnt) {
    valid_count_ = cnt;
    return *this;
}

void TensorDesc::freeMemoryAddress() {
    if (memoryAddress == nullptr) {
        return;
    }
    rtFree(memoryAddress);
}

// 返回Tensor张量的元素总数，即所有维度的乘积
int64_t TensorDesc::GetViewCount(vector<int64_t> &view_dims) const {
    const auto & v = view_dims;
    return accumulate(v.cbegin(), v.cend(), 1, multiplies<int64_t>());
}

// 返回自定义有数据&有格式的 tensor张量的元素总数，即所有维度的乘积
int64_t TensorDesc::GetStorageCount() const {
    const auto & v = storage_dims_.empty() ? view_dims_ : storage_dims_;
    return accumulate(v.cbegin(), v.cend(), 1, multiplies<int64_t>());
}

int TensorDesc::GetDateTypeSize(aclDataType data_type) const{
    std::array<size_t, 34> sizeMap = {{
        sizeof(float),    // ACL_FLOAT = 0
        2,                // ACL_FLOAT16 = 1
        sizeof(int8_t),   // ACL_INT8 = 2
        sizeof(int32_t),  // ACL_INT32 = 3
        sizeof(uint8_t),  // ACL_UINT8 = 4
        0,                // 5 (未使用)
        sizeof(int16_t),  // ACL_INT16 = 6
        sizeof(uint16_t), // ACL_UINT16 = 7
        sizeof(uint32_t), // ACL_UINT32 = 8
        sizeof(int64_t),  // ACL_INT64 = 9
        sizeof(uint64_t), // ACL_UINT64 = 10
        sizeof(double),   // ACL_DOUBLE = 11
        sizeof(bool),     // ACL_BOOL = 12
        0,                // ACL_STRING = 13 (可变长度)
        0, 0,             // 14, 15 (未使用)
        8,                // ACL_COMPLEX64 = 16
        16,               // ACL_COMPLEX128 = 17
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 18-26 (未使用)
        2,                // ACL_BF16 = 27
        0,                // 28 (未使用)
        1,                // ACL_INT4 = 29
        1,                // ACL_UINT1 = 30
        0, 0,             // 31, 32 (未使用)
        4                 // ACL_COMPLEX32 = 33
    }};

    if (data_type == ACL_DT_UNDEFINED || data_type < -1 || data_type >= static_cast<aclDataType>(sizeMap.size())) {
        return 4;
    }
    if (data_type == ACL_STRING) {
        throw std::runtime_error("STRING type has variable size");
    }
    return sizeMap[data_type];
}

// ================================================
// 模板方法：Scalar和Tensor共有的模板函数特例化实现
// ================================================
void DescToJson(json& root, const TensorDesc& tensor_desc, bool is_input) {
    tensor_desc.ToJson(root, is_input);
}

TensorDesc *InferAclType(TensorDesc& tensor_desc) {
//  return &tensor_desc;
    (void)tensor_desc;
    return nullptr;
}

TensorDesc *DescToAclContainer(TensorDesc& tensor_desc) {
    return new TensorDesc(tensor_desc); // 调用拷贝构造函数
    // return new TensorDesc(std::move(tensor_desc));  // 调用移动构造函数
}

/*
 * ========================
 *  对BIN文件读取
 * ========================
*/
static void* MallocAndMemcpyDeviceMemory(void* host_mem, size_t size) {
    // DEVICE侧分配内存
    void* dev_mem = nullptr;
    rtError_t error = rtMalloc(&dev_mem, size, RT_MEMORY_HBM);
    if (dev_mem == nullptr || error != RT_ERROR_NONE) {
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

//  读取BIN文件函数
static void* ReadBinaryFile(const string& binary_file, size_t& size) {
    void* host_mem = ReadBinFile(binary_file, size);
    if (host_mem == nullptr || size <= 0) {
        LogError("[ReadBinaryFile] input load bin file failed because the size is illegal: " + to_string(size));
        return nullptr;
    }
    LogDebug("Input Reload " + binary_file + " Size " + to_string(size) + " success .." );
    return host_mem;
}

// BIN内容由 HOST -> DEVICE 内存拷贝
static void* CopyToDeviceMemory(void* host_mem, size_t size) {
    void* dev_mem = MallocAndMemcpyDeviceMemory(host_mem, size);
    if (dev_mem == nullptr) {
        LogError("[CopyToDeviceMemory] input malloc memory and copy bin failed: ");
        return nullptr;
    }
    LogDebug("Memory Copy Host -> Device Size " + to_string(size) + " success .." );
    return dev_mem;
}

// 指针赋值函数
static void AssignMemoryPointer(TensorDesc* p, void* dev_mem, int32_t index) {
    LogDebug("[AssignMemoryPointer] index=[" + to_string(index) + "] single stream default value index = -1");
    LogMemAddress(dev_mem, "AssignMemoryPointer");
    if (index == -1) {
        p->memoryAddress = dev_mem;
        //printf("LoadBinaryFile p->memoryAddress = %p, dev_mem = %p\n", p->memoryAddress,dev_mem);
    } else {
        p->memoryAddressList[index] = dev_mem;
        //printf("LoadBinaryFile p->memoryAddressList[index] = %p, dev_mem = %p\n", p->memoryAddressList[index],dev_mem);
    }
}

static int LoadBinaryFile(TensorDesc*p, const string& binary_file, int32_t index) {
    // 读取input的BIN数据文件
    size_t size = 0;
    void* host_mem = ReadBinaryFile(binary_file, size);
    if (host_mem == nullptr) {
        return 1;
    }
    // BIN内容由 HOST -> DEVICE 内存拷贝
    unique_ptr<char[]> ptr(static_cast<char *>(host_mem));
    void* dev_mem = CopyToDeviceMemory(host_mem, size);
    if (dev_mem == nullptr) {
        return 1;
    }
    // 指针赋值
    AssignMemoryPointer(p, dev_mem, index);
    return 0;
}

static int MallocDeviceMemoryOnly(TensorDesc* p, int32_t index) {
    LogDebug("[MallocDeviceMemoryOnly] Tensor DataSize= shape_size * dateType_szie =" + to_string(p->tensorSize));
    auto size = p->tensorSize;
    if (size <= 0) {
        LogError("[MallocDeviceMemoryOnly] output Malloc device memory faile because the size is illegal : " + to_string(size));
        return 1;
    }
    void* dev_mem = nullptr;
    rtError_t error = rtMalloc((void**)&dev_mem, size, RT_MEMORY_HBM);
    if (dev_mem == nullptr || error != RT_ERROR_NONE) {
        LogError( "[MallocDeviceMemoryOnly] Device Out Put Malloc Memory Failed errcode is  " + to_string(error));
        return 1;
    }
    LogDebug("output malloc device memory size: " + to_string(size) + " success ..");
    AssignMemoryPointer(p, dev_mem, index);
    return 0;
}

// 算子在单流、单次内存申请的场景下的处理
static int ReloadDataFromBinaryFileBySingleStream(TensorDesc *tensor_desc, size_t index, size_t count,
                                                  const string& file_name, int32_t EXEC_CNT)
{
    if (index < count) {  // 处理 INPUT
         return LoadBinaryFile(tensor_desc, file_name, EXEC_CNT);
    } else {  // 处理 OUTPUT
        return MallocDeviceMemoryOnly(tensor_desc, EXEC_CNT);
    }
}

// 算子在多流、多次执行、多次申请内存的场景处理分支
static int ReloadDataFromBinaryFileByMultiStream(TensorDesc *tensor_desc, size_t index, size_t count,
                                                 const string& file_name, int32_t EXEC_CNT, bool MEM_CACHE_TEST)
{
    tensor_desc->memoryAddressList = (void **)malloc(EXEC_CNT * sizeof(void *));
    if (tensor_desc->memoryAddressList == nullptr) {
        LogError("[ReloadDataFromBinaryFileByMultiStream] Malloc memoryAddressList Failed ...");
        return 1;
    }
    if (index < count) {  // 处理 INPUT
        int ret = LoadBinaryFile(tensor_desc, file_name, 0);
        if (ret == 1) {
            return ret;
        }

        size_t size = 0;
        void* host_mem = ReadBinaryFile(file_name, size);
        if (host_mem == nullptr) {
            return 1;
        }
        unique_ptr<char[]> ptr(static_cast<char *>(host_mem));
        for (int i = 1; i < EXEC_CNT; i++) {
            if (MEM_CACHE_TEST) {
                void* dev_mem = CopyToDeviceMemory(host_mem, size);
                if (dev_mem == nullptr) {
                    return 1;
                }
                AssignMemoryPointer(tensor_desc, dev_mem, i);
            } else {
                tensor_desc->memoryAddressList[i] = tensor_desc->memoryAddressList[0];
            }
        }
    } else {  // 处理 OUTPUT
        for (int i = 0; i < EXEC_CNT; i++) {
            int ret = MallocDeviceMemoryOnly(tensor_desc, i);
            if (ret == 1) {
                return ret;
            }
        }
    }
    return 0;
}
/*<-- 读取BIN文件的主函数 -->*/
int ReloadDataFromBinaryFile(TensorDesc *tensor_desc, size_t index, size_t count, const string& file_prefix,
                             int EXEC_CNT, bool MEM_CACHE_TEST) {
    if (tensor_desc == nullptr) {
        return 0;
    }

    std::string file_name = "";
    std::string prt_info = "";
    // 判断.BIN的加载类型
    if (tensor_desc->set_bin_to_value_file_) {
        prt_info = string("Reload Customized Input BIN File. Index : ") + to_string(index);
        file_name = tensor_desc->value_;
    } else if (tensor_desc->gen_data_from_onnx_) {
        prt_info = string("Reload Customized Input BIN File Depends On Onnx. Index : ") + to_string(index);
        file_name = tensor_desc->value_; // 这里只包含节点名，应该还需要完整路径
    } else {
        prt_info = string("Reload Input for tensor. Index: ") + to_string(index) + ". ";
        file_name = file_prefix + "_input_" + to_string(index) + ".bin";
    }

    if (index < count) {
        LogDebug(prt_info);
    } else {
        LogDebug("Reload output for tensor. Index: " + to_string(index) + ". ");
    }
    LogDebug(file_name);

    if (EXEC_CNT == -1) {
        return ReloadDataFromBinaryFileBySingleStream(tensor_desc, index, count, file_name, EXEC_CNT);
    }

    return ReloadDataFromBinaryFileByMultiStream(tensor_desc, index, count, file_name, EXEC_CNT, MEM_CACHE_TEST);
}

/*
 * ========================
 *  对算子输出BIN文件的存入操作
 * ========================
*/

/* <-- 保存BIN文件开始 --> */
static int SaveToBinaryFile(TensorDesc* tensor_desc, const string& binary_file, int array_subscript) {
    if (tensor_desc == nullptr) {
        return 0;
    }

    // Host侧分配
    uint64_t size = tensor_desc->tensorSize;
    if (size <= 0) {
        LogError("[SaveToBinaryFile] Calculation Host Memory Size " + to_string(size) + " Failed .. ");
        return 1;
    }

    void* host_mem = nullptr;
    host_mem = static_cast<void *>(new(nothrow) char[size]);
    if (host_mem == nullptr) {
        LogError("[SaveToBinaryFile] Alloc Host Memory Failed. need memory: ");
        return 1;
    }
    memset_s(host_mem, size, 0, size);
    LogDebug("output malloc host memory " + to_string(size) + " size success .. ");
    unique_ptr<char[]> ptr(static_cast<char *>(host_mem));

    // 获得DEVICE侧的内存地址
    void *dev_mem = nullptr;
    LogDebug("array subscript is [ " + to_string(array_subscript) + " ]");
    if (array_subscript == -1) {
        dev_mem = tensor_desc->GetMemoryAddress();
    } else {
        dev_mem = tensor_desc->GetMemoryAddressListByIndex(array_subscript);
    }
    LogMemAddress(dev_mem, "SaveToBinaryFile");
    if (dev_mem == nullptr) {
        LogError("[SaveToBinaryFile] get device memory failed, return.. ");
        return 1;
    }

    // Device->Host 内存拷贝
    rtError_t ret = rtMemcpy(host_mem, size, dev_mem, size, RT_MEMCPY_DEVICE_TO_HOST);
    if (ret != RT_ERROR_NONE) {
        LogError("[SaveToBinaryFile] rtMemcpy to host failed, return.. ");
        return 1;
    }
    LogDebug("memory copy Device->Host success ..");
    // 写文件
    return WriteBinFile(host_mem, binary_file, size);
}

static short int CalcRealOutputIndex(short int index, const set<short int>& inplace_output) {
    short int diff = 0;
    short int le = 0;
    do {
        diff = le;
        short int cmp = index + le;
        le = inplace_output.count(cmp) + distance(inplace_output.begin(), inplace_output.lower_bound(cmp));
    } while(diff < le);
    return index + le;
}

int SaveResultToBinaryFile(TensorDesc* tensor_desc, size_t index, const string& file_prefix, int array_subscript) {
    string file_name = file_prefix + "_output_" + to_string(index) +".bin";
    return SaveToBinaryFile(tensor_desc, file_name, array_subscript);
}

int SaveResultToBinaryFile(TensorDesc* tensor_desc, size_t index, size_t total_input, const string& file_prefix,
                           const set<short int>& inplace_output, int array_subscript) {
    if (index < total_input) {
        return 0;
    }
    auto output_idx = CalcRealOutputIndex(index - total_input, inplace_output);
    return SaveResultToBinaryFile(tensor_desc, output_idx, file_prefix, array_subscript);
}
