#include <godot_cpp/variant/utility_functions.hpp>
#include <sstream>

class PrintUtility {
public:
    // Variadic template function to format and print
    template<typename... Args>
    static void print_verbose(const std::string& format, Args... args) {
        std::string formatted_string = format_string(format, args...);
        godot::UtilityFunctions::print_verbose(formatted_string.c_str());
    }

private:
    // Helper function to format the string using std::ostringstream
    template<typename... Args>
    static std::string format_string(const std::string& format, Args... args) {
        std::ostringstream oss;
        size_t index = 0;
        std::array<std::string, sizeof...(args)> arguments{to_string(args)...};
        
        for (char ch : format) {
            if (ch == '%' && index < sizeof...(args)) {
                oss << arguments[index++];
            } else {
                oss << ch;
            }
        }
        return oss.str();
    }

    // Helper function to convert arguments to strings (generic case)
    template<typename T>
    static std::string to_string(T arg) {
        std::ostringstream oss;
        oss << arg;
        return oss.str();
    }
};
