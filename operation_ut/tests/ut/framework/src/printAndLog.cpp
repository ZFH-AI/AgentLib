#include "printAndLog.h"
using namespace std;

class Logger {
    private:
        static Logger* instance;
        static std::mutex mtx;
        std::ofstream logFile;
        LogLevel currentLevel;

        Logger(const std::string& fileName, LogLevel level) {
            logFile.open(fileName, std::ios::app);
            if (!logFile.is_open()) {
                throw std::runtime_error("Failed to open log file: " + fileName);
            }
            currentLevel = level;
        }

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

    public:
        ~Logger() {
            if (logFile.is_open()) {
                logFile.close();
            }
        }

        static Logger* getInstance(const std::string& fileName = "log.txt", LogLevel level = INFO) {
            std::lock_guard<std::mutex> lock(mtx);
            if (instance == nullptr) {
                instance = new Logger(fileName, level);
            }
            return instance;
        }

        static void releaseInstance() {
            std::lock_guard<std::mutex> lock(mtx);
            if (instance != nullptr) {
                delete instance;
                instance = nullptr;
            }
        }

        void log(LogLevel level, const std::string& message) {
            if (level >= currentLevel) {
                // 获取当前时间
                auto now = std::chrono::system_clock::now();
                auto now_time = std::chrono::system_clock::to_time_t(now);

                // 格式化为字符串
                std::tm tm_info;
                localtime_r(&now_time, &tm_info);  // 使用localtime_s在Windows上

                std::stringstream time_stream;
                time_stream << std::put_time(&tm_info, "%Y-%m-%d %H:%M:%S");

                // 获取毫秒部分
                auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()) % 1000;

                // 构造完整日志行
                std::string log_line = time_stream.str() + "." +
                                     std::to_string(milliseconds.count()) + " " +
                                     getLevelString(level) + ": " + message;
                logFile << log_line << std::endl;
            }
        }

    private:
        std::string getLevelString(LogLevel level) {
            switch (level) {
                case LOG_DEBUG: return "[DEBUG]";
                case INFO: return "[INFO]";
                case WARNING: return "[WARNING]";
                case ERROR: return "[ERROR]";
                case CRITICAL: return "[CRITICAL]";
                default: return "[UNKNOWN]";
            }
        }
};

Logger* Logger::instance = nullptr;
std::mutex Logger::mtx;

// 使用 static 缓存结果 GetDebugLog() 只执行一次，后续调用直接返回缓存值
inline bool ShouldLog() {
    static const bool debugLog = (GetDebugLog() != 1);
    return debugLog;
}

void InitLogger(const std::string& fileName, LogLevel level) {
    if (ShouldLog()) {
        return;
    }
    Logger::getInstance(fileName, level);
}
void ReleaseLogger() {
    if (ShouldLog()) {
        return;
    }
    Logger::releaseInstance();
}

void LogMessage(LogLevel level, const std::string& message) {
    if (ShouldLog()) {
        return;
    }
    Logger::getInstance()->log(level, message);
}

void LogInfo(const std::string& message) {
    if (ShouldLog()) {
        return;
    }
    LogMessage(INFO, message);
}

void LogWarning(const std::string& message) {
    if (ShouldLog()) {
        return;
    }
    LogMessage(WARNING, message);
}

void LogError(const std::string& message) {
    if (ShouldLog()) {
        return;
    }
    LogMessage(ERROR, message);
}

void LogCritical(const std::string& message) {
    if (ShouldLog()) {
        return;
    }
    LogMessage(CRITICAL, message);
}

void LogMemAddress(void* mem_addr, const std::string& fun_name) {
    if (ShouldLog()) {
        return;
    }
    std::ostringstream oss;
    oss << fun_name<<"  "<< mem_addr;
    LogDebug(oss.str());
}

void LogDebug(const std::string& message) {
    if (ShouldLog()) {
        return;
    }
    std::cout << "[DEBUG] " << message << std::endl;
    LogMessage(LOG_DEBUG, message);
}
