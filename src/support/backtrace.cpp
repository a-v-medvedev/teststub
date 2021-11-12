#include <errno.h>
#include <signal.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <unistd.h>
#include <iostream>

namespace helpers {

static inline std::vector<std::string> str_split(const std::string &s, char delimiter) {
    std::vector<std::string> result;
    std::string token;
    std::istringstream token_stream(s);
    while (std::getline(token_stream, token, delimiter)) {
        result.push_back(token);
    }
    return result;
}

//--- Stacktrace ---
void stacktrace(int nskip) {
    int n;
    void *buf[100];
    char **sptr;
    n = backtrace(buf, 100);
    sptr = backtrace_symbols(buf, n);
    if (sptr == nullptr) {
        perror("backtrace_symbols");
        exit(EXIT_FAILURE);
    }
    std::cerr << "Stack trace:" << std::endl;
    std::cerr << "-------------------------------------------" << std::endl;
    for (int i = 0; i < n; i++) {
        if (i < nskip) {
            continue;
        }
        std::string symbol = "???";
        std::string line(sptr[i]);
        auto x = helpers::str_split(line, '(');
        if (x.size() > 1) {
            auto y = helpers::str_split(x[1], '+');
            if (y.size() > 1) {
                int status;
                char *demangled = abi::__cxa_demangle(y[0].c_str(), NULL, 0, &status);
                if (status == 0 && demangled) {
                    symbol = demangled;
                } else if (y[0] != "") {
                    symbol = y[0];
                }
            }
        }
        std::cerr << x[0] << ": " << symbol << std::endl;
    }
    std::cerr << "-------------------------------------------" << std::endl << std::endl;
    free(sptr);
}

void timeout_signal_handler(int signo) {
    if (SIGALRM == signo) {
        std::cerr << "\n>> TIMEOUT: " << std::endl;
        stacktrace(3);
    }
    signal(signo, SIG_DFL);
    raise(signo);
}

void generic_signal_handler(int signo) {
    std::cerr << "\n>> FATAL SIGNAL: " << std::to_string(signo) << std::endl;
    stacktrace(3);
    signal(signo, SIG_DFL);
    raise(signo);
}

void *__cxa_allocate_exception(size_t x) {
    void *(*orig__cxa_allocate_exception)(size_t) =
        (void *(*)(long unsigned int))dlsym(RTLD_NEXT, "__cxa_allocate_exception");
    std::cerr << "\n>> EXCEPTION!" << std::endl;
    stacktrace(2);
    return orig__cxa_allocate_exception(x);
}

void setup_signal_handlers(int timeout) {
    if (timeout) {
        signal(SIGALRM, timeout_signal_handler);
        alarm(timeout);
    }
    signal(SIGSEGV, generic_signal_handler);
    signal(SIGILL, generic_signal_handler);
    signal(SIGBUS, generic_signal_handler);
    signal(SIGFPE, generic_signal_handler);
    signal(SIGABRT, generic_signal_handler);
}

}


