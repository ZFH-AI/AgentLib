#include "config_reader.h"
#include <sys/stat.h>
#include <sys/types.h>

using namespace std;

using json = nlohmann::json;
using KeyValueMap = std::unordered_map<std::string, std::string>;

class ConfigReader {
private:
    static std::unique_ptr<ConfigReader> instance;  // 使用智能指针管理单例
    static std::mutex instance_mtx;                 // 用于单例初始化的互斥锁
    std::shared_mutex mutex_;                       // 用于保护 keyValueMap_ 的读写锁
    std::string path_;
    bool initialized_ = false;
    std::unordered_map<std::string, std::string> keyValueMap_;

    // 私有构造函数
    explicit ConfigReader(const std::string& path) : path_(path) {}

    // 禁止拷贝和赋值
    ConfigReader(const ConfigReader&) = delete;
    ConfigReader& operator=(const ConfigReader&) = delete;

public:
    ~ConfigReader() = default;

    // 获取单例（线程安全，双重检查锁定优化）
    static ConfigReader& getInstance(const std::string& path = "config.json") {
        if (!instance) {  // 第一次检查（无锁，提高性能）
            std::lock_guard<std::mutex> lock(instance_mtx);
            if (!instance) {  // 第二次检查（加锁，确保线程安全）
                instance = std::unique_ptr<ConfigReader>(new ConfigReader(path));
            }
        }
        return *instance;
    }

    // 初始化（线程安全）
    bool init() {
        std::unique_lock<std::shared_mutex> lock(mutex_);  // 写锁
        if (initialized_) {
            return true;
        }

        std::ifstream file(path_);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open config file: " + path_);
        }
        
        try {
            json j;
            file >> j;
            file.close();
            keyValueMap_ = j.get<KeyValueMap>();
        } catch (const json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            return false;
        }
        // 使用迭代器遍历
        //for (auto it = keyValueMap_.begin(); it != keyValueMap_.end(); ++it) {
        //    std::cout << "Key: " << it->first << ", Value: " << it->second << std::endl;
        //}
        initialized_ = true;  // 确保在成功加载后设置
        return true;
    }

    // 获取配置项（线程安全）
    const std::string& get(const std::string& key) {
        std::shared_lock<std::shared_mutex> lock(mutex_);  // 读锁
        if (!initialized_) {
            throw std::runtime_error("ConfigReader not initialized");
        }
        auto it = keyValueMap_.find(key);
        if (it == keyValueMap_.end()) {
            throw std::runtime_error("Key not found: " + key);
        }
        return it->second;
    }

    // 可选：动态更新配置（线程安全）
    void update(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);  // 写锁
        keyValueMap_[key] = value;
    }
};

// 静态成员初始化
std::unique_ptr<ConfigReader> ConfigReader::instance = nullptr;
std::mutex ConfigReader::instance_mtx;

void InitConfigJson(const std::string& path) {
    ConfigReader::getInstance(path).init();
}
// Key-> Value，Value是文件形式
const std::string& GetConfigValueByKey(const std::string& key) {
    return  ConfigReader::getInstance().get(key);
}

bool directoryExists(const std::string& path) {
    struct stat info;
    return (stat(path.c_str(), &info) == 0 && (info.st_mode & S_IFDIR));
}
bool CheckConfigDir(const std::string& value) {
    if (!value.empty() && value.back() != '/' && value.back() != '\\') {
        return false;
    }
    if (!directoryExists(value)) {
#ifdef _WIN32
        if (_mkdir(value.c_str()) !=0) {
           throw std::runtime_error("_WIN32 Failed to create directory: " + value + ", error code: " + std::to_string(errno));
        }
#else
        if (mkdir(value.c_str(), 0777) !=0) { // Linux/Unix 权限 0777
           throw std::runtime_error("Linux Failed to create directory: " + value + ", error code: " + std::to_string(errno));
        }
#endif
    }
    return true;
}
// Key-> Value中的Value是目录形式
const std::string& GetConfigValueDirByKey(const std::string& key) {
    const std::string& value = ConfigReader::getInstance().get(key);
    if(!CheckConfigDir(value)) {
        throw std::runtime_error("config_reader GetConfigValueDirByKey Value must be a directory, ending with / ");
    }
    return value;
}

