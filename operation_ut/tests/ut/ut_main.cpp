#include <gtest/gtest.h>
//#include "op_api_ut.h"
//#include "op_kernel_ut.h"
//#include "op_kernel_st.h"
#include "scalar_desc.h"
#include "tensor_desc.h"
#include <string.h>
#include "c_shell.h"
#include "c_py.h"
#include "file_io.h"
#include "rts_interface.h"
#include "printAndLog.h"
#include "config_reader.h"
using namespace std;

// 输入参数解析
string getCmdOpt(int argc, char** argv, const string& option) {
    for (int i = 0; i < argc; ++i) {
        std::string arg = argv[i];
        size_t found = arg.find(option);
        if (found == std::string::npos) {
            continue;
        }
        size_t equalSign = arg.find('=', found);
        if (equalSign != std::string::npos) {
            return arg.substr(equalSign + 1);
        }
    }
    return "";
}

// soc 规范化
static string NormalizeSoc(string &soc) {
    string soc_(soc);
    std::transform(soc_.begin(), soc_.end(), soc_.begin(), ::toupper);

    if (soc_.compare("ASCEND910") == 0 || soc_.compare("ASCEND910A") == 0) {
        return "Ascend910A";
    }

    if (soc_.compare(0, strlen("ASCEND910B"), "ASCEND910B") == 0) {
        return "Ascend910B2";
    }

    return "UnsupportedSoc";
}

static void set_env() {
    setenv("PYTHONMALLOC", "malloc", 1);
    setenv("ASCEND_SLOG_PRINT_TO_STDOUT", "1", 1);
    string ascend_opp_path = string(ASCEND_PATH) + "/opp";
    setenv("ASCEND_OPP_PATH", ascend_opp_path.c_str(), 1);
}

// 全局测试环境类
class OpApiUtEnvironment : public testing::Environment {
    public:
        OpApiUtEnvironment() {}
        OpApiUtEnvironment(const string& soc): soc_(soc) {}

        // 在所有测试运行之前执行
        virtual void SetUp()  {
            std::cout << "Global setup: Initialize resources." << std::endl;
            //设置环境变量
            //set_env();
            // OP_API_UT_SRC_DIR: 为ou_ut目录所在的路径
            SetOpUtSrcPath(OP_API_UT_SRC_DIR);
            // 设置配置信息路径
            InitConfigJson(GetOpUtSrcPath() + "/config/path.json");
             // 开启日志记录
            SetDebugLog(ENABLE_DEBUG_LOG);
            InitLogger(GetConfigValueByKey("Case_Log_Root_Path"), LOG_DEBUG);


            //SetUtTmpFileSwitch();
            //
            //桩函数
            //UtMock::GetInstance().SetAscendPath(ASCEND_PATH);
            //UtMock::GetInstance().SetSocVersion(soc_);
            //UtMock::GetInstance().DelegateToFake();

            // Python 脚本
            ASSERT_EQ(PyHolder::GetInstance().Initialize(), 0);
            ASSERT_EQ(PyScripts::GetInstance().Initialize(GetOpUtSrcPath() + "/script"), 0);

            //数据切分
            //OpTilingDll::GetInstance().Load();
            //OppDll::GetInstance().Load();
            //
            //fe::PlatformInfoManager::Instance().InitializePlatformInfo();
            //fe::PlatformInfoManager::GeInstance().InitializePlatformInfo();
            //fe::PlatformInfoManager::Instance().InitRuntimePlatformInfos(soc_);
            //fe::PlatformInfoManager::GeInstance().InitRuntimePlatformInfos(soc_);

             ASSERT_EQ(RtsInit(), 0);

        }

        // 在所有测试完成之后执行
        virtual void TearDown()  {
            std::cout << "Global teardown: Clean up resources." << std::endl;
            RtsUnInit();
            PyScripts::GetInstance().Clean();
            PyHolder::GetInstance().Clean();
            // 删除构建中生成的文件
            DeleteCwdFilesEndsWith(".dump");
            DeleteCwdFilesEndsWith(".toml");
            setenv("ASCEND_SLOG_PRINT_TO_STDOUT", "0", 1);
            ReleaseLogger();
        }
    private:
        string soc_;
};

/**<---- Gtest的主函数 ---->**/
int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    string soc("ASCEND910B");
//    string opt_soc = getCmdOpt(argc, argv, "--soc=");
//    if (!opt_soc.empty()) {
//        soc = opt_soc;
//    }
//    soc = NormalizeSoc(soc);
//    if (soc.compare(0, strlen("UnsupportedSoc"), "UnsupportedSoc") == 0) {
//        cout << "Input --soc=" << soc << "UnsupportedSoc" << endl;
//    }
    // 注册自定义全局测试环境类
    testing::AddGlobalTestEnvironment(new OpApiUtEnvironment(soc));

    return RUN_ALL_TESTS();
}

