// C++ Integration Layer - handles build systems, compiler detection, ABI
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <iostream>
#include <subprocess.hpp>  // For process execution
#include <nlohmann/json.hpp>

extern "C" {
    // Export functions to Rust
    int cpp_build_cmake(const char* package_name, size_t name_len);
    const char* cpp_detect_compiler();
    const char* cpp_get_abi_info();
}

class CompilerDetector {
public:
    enum class CompilerType {
        GCC, Clang, MSVC, Unknown
    };
    
    struct CompilerInfo {
        CompilerType type;
        std::string version;
        std::string path;
        std::string target_triple;
        std::string stdlib;
    };
    
    static CompilerInfo detect_system_compiler() {
        CompilerInfo info;
        
        // Try different compilers
        if (test_compiler("g++")) {
            info.type = CompilerType::GCC;
            info.version = get_gcc_version();
            info.path = find_executable("g++");
            info.stdlib = "libstdc++";
        } else if (test_compiler("clang++")) {
            info.type = CompilerType::Clang;
            info.version = get_clang_version();
            info.path = find_executable("clang++");
            info.stdlib = "libc++";
        } else if (test_compiler("cl.exe")) {
            info.type = CompilerType::MSVC;
            info.version = get_msvc_version();
            info.path = find_executable("cl.exe");
            info.stdlib = "msvc_stl";
        } else {
            info.type = CompilerType::Unknown;
        }
        
        return info;
    }
    
private:
    static bool test_compiler(const std::string& compiler) {
        try {
            auto result = subprocess::run({compiler, "--version"}, 
                                        subprocess::RunOptions{.check = false});
            return result.returncode == 0;
        } catch (...) {
            return false;
        }
    }
    
    static std::string get_gcc_version() {
        auto result = subprocess::run({"g++", "--version"});
        // Parse version from output
        return extract_version_from_output(result.cout);
    }
    
    static std::string get_clang_version() {
        auto result = subprocess::run({"clang++", "--version"});
        return extract_version_from_output(result.cout);
    }
    
    static std::string get_msvc_version() {
        auto result = subprocess::run({"cl.exe", "/?"});
        return extract_version_from_output(result.cout);
    }
    
    static std::string extract_version_from_output(const std::string& output) {
        // Simple version extraction - would need more robust parsing
        size_t pos = output.find(" ");
        if (pos != std::string::npos) {
            return output.substr(0, pos);
        }
        return "unknown";
    }
    
    static std::string find_executable(const std::string& name) {
        // Find executable in PATH
        return name; // Simplified
    }
};

class CMakeBuilder {
public:
    struct BuildConfig {
        std::string build_type = "Release";
        std::string install_prefix = "/usr/local";
        std::vector<std::string> cmake_args;
        bool verbose = false;
    };
    
    static int build_package(const std::string& package_name, 
                           const std::string& source_dir,
                           const BuildConfig& config = {}) {
        try {
            std::filesystem::path build_dir = 
                std::filesystem::temp_directory_path() / "cpppm_build" / package_name;
            
            // Create build directory
            std::filesystem::create_directories(build_dir);
            
            // Configure with CMake
            std::vector<std::string> configure_cmd = {
                "cmake",
                "-S", source_dir,
                "-B", build_dir.string(),
                "-DCMAKE_BUILD_TYPE=" + config.build_type,
                "-DCMAKE_INSTALL_PREFIX=" + config.install_prefix
            };
            
            // Add custom CMake args
            for (const auto& arg : config.cmake_args) {
                configure_cmd.push_back(arg);
            }
            
            std::cout << "Configuring " << package_name << " with CMake..." << std::endl;
            auto configure_result = subprocess::run(configure_cmd);
            
            if (configure_result.returncode != 0) {
                std::cerr << "CMake configure failed: " << configure_result.cerr << std::endl;
                return 1;
            }
            
            // Build
            std::cout << "Building " << package_name << "..." << std::endl;
            auto build_result = subprocess::run({
                "cmake", "--build", build_dir.string(), 
                "--parallel", std::to_string(std::thread::hardware_concurrency())
            });
            
            if (build_result.returncode != 0) {
                std::cerr << "Build failed: " << build_result.cerr << std::endl;
                return 1;
            }
            
            // Install
            std::cout << "Installing " << package_name << "..." << std::endl;
            auto install_result = subprocess::run({
                "cmake", "--install", build_dir.string()
            });
            
            if (install_result.returncode != 0) {
                std::cerr << "Install failed: " << install_result.cerr << std::endl;
                return 1;
            }
            
            std::cout << "Successfully built and installed " << package_name << std::endl;
            return 0;
            
        } catch (const std::exception& e) {
            std::cerr << "Build error: " << e.what() << std::endl;
            return 1;
        }
    }
};

class ABIManager {
public:
    struct ABIInfo {
        std::string compiler;
        std::string compiler_version;
        std::string stdlib;
        std::string cpu_arch;
        std::string os;
        bool debug_mode;
        std::string cxx_standard;
    };
    
    static ABIInfo get_current_abi() {
        ABIInfo info;
        
        auto compiler_info = CompilerDetector::detect_system_compiler();
        info.compiler = compiler_type_to_string(compiler_info.type);
        info.compiler_version = compiler_info.version;
        info.stdlib = compiler_info.stdlib;
        
        // Detect architecture
        #ifdef __x86_64__
            info.cpu_arch = "x86_64";
        #elif __aarch64__
            info.cpu_arch = "aarch64";
        #elif __arm__
            info.cpu_arch = "arm";
        #else
            info.cpu_arch = "unknown";
        #endif
        
        // Detect OS
        #ifdef __linux__
            info.os = "linux";
        #elif __APPLE__
            info.os = "macos";
        #elif _WIN32
            info.os = "windows";
        #else
            info.os = "unknown";
        #endif
        
        // Check debug mode
        #ifdef NDEBUG
            info.debug_mode = false;
        #else
            info.debug_mode = true;
        #endif
        
        // Detect C++ standard
        #if __cplusplus >= 202002L
            info.cxx_standard = "c++20";
        #elif __cplusplus >= 201703L
            info.cxx_standard = "c++17";
        #elif __cplusplus >= 201402L
            info.cxx_standard = "c++14";
        #elif __cplusplus >= 201103L
            info.cxx_standard = "c++11";
        #else
            info.cxx_standard = "c++98";
        #endif
        
        return info;
    }
    
    static std::string abi_to_string(const ABIInfo& info) {
        nlohmann::json j;
        j["compiler"] = info.compiler;
        j["compiler_version"] = info.compiler_version;
        j["stdlib"] = info.stdlib;
        j["cpu_arch"] = info.cpu_arch;
        j["os"] = info.os;
        j["debug_mode"] = info.debug_mode;
        j["cxx_standard"] = info.cxx_standard;
        
        return j.dump();
    }
    
private:
    static std::string compiler_type_to_string(CompilerDetector::CompilerType type) {
        switch (type) {
            case CompilerDetector::CompilerType::GCC: return "gcc";
            case CompilerDetector::CompilerType::Clang: return "clang";
            case CompilerDetector::CompilerType::MSVC: return "msvc";
            default: return "unknown";
        }
    }
};

// C interface for Rust FFI
extern "C" {
    int cpp_build_cmake(const char* package_name, size_t name_len) {
        std::string pkg_name(package_name, name_len);
        
        // In a real implementation, source_dir would be determined from cache
        std::string source_dir = "/tmp/cpppm_cache/" + pkg_name;
        
        return CMakeBuilder::build_package(pkg_name, source_dir);
    }
    
    const char* cpp_detect_compiler() {
        static std::string compiler_info;
        auto info = CompilerDetector::detect_system_compiler();
        
        nlohmann::json j;
        j["type"] = static_cast<int>(info.type);
        j["version"] = info.version;
        j["path"] = info.path;
        j["stdlib"] = info.stdlib;
        
        compiler_info = j.dump();
        return compiler_info.c_str();
    }
    
    const char* cpp_get_abi_info() {
        static std::string abi_info;
        auto info = ABIManager::get_current_abi();
        abi_info = ABIManager::abi_to_string(info);
        return abi_info.c_str();
    }
}