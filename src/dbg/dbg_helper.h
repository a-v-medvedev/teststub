#pragma once

#include "gdb_monitor.h"
#include "stacktrace.h"
#include <signal.h>

// NOTE: if this file is included somewhere else in the project, linker will fail claiming
// multiple definition of this function
void *__cxa_allocate_exception(size_t n) {
    void *(*orig__cxa_allocate_exception)(size_t) =
        (void *(*)(long unsigned int))dlsym(RTLD_NEXT, "__cxa_allocate_exception");
    stacktrace::print("\n>> EXCEPTION!", 2);
    return orig__cxa_allocate_exception(n);
}

// NOTE: if this file is included somewhere else in the project, linker will fail claiming
// multiple definition of this function
void dbg_helper_signal_handler(int signo) {
    if (SIGALRM == signo) {
        stacktrace::print("\n>> TIMEOUT: ", 3);
    } else {
        stacktrace::print("\n>> FATAL SIGNAL: " + std::to_string(signo), 3);
    }
    usleep(100000);
    if (SIGALRM == signo) {
        signo = SIGABRT;
    }

    // FIXME change it for more correct handling (it might be not a SIG_DFL action expected)
    signal(signo, SIG_DFL);
    raise(signo);
}

struct dbg_helper {
    enum mode_t { SIGHANDLERS=1, GDB=2, TIMER=4 };
#ifdef WITH_ARGSPARSER    
    args_parser *parser = nullptr;
    int flags = 0; 
    unsigned seconds = 0;
    std::vector<std::string> gdbopts;
#endif    
    gdb_monitor g;
#ifdef WITH_ARGSPARSER
    void add_parser_options(args_parser &_parser) {
        parser = &_parser;
        // NOTE: SYS group is a group of options which are not read or saved into YAML-config
        parser->set_current_group("SYS");
        parser->add_vector<std::string>("gdb", "gdb.cmd,gdb_output.%r,gdb", ',', 1, 3)
            .set_caption("<gdb_script_file_name,gdb_output_file_name,gdb_executable_name>");
        parser->add<int>("timeout", 0);
        parser->set_default_current_group();
    }
    void get_parser_options() {
        flags = SIGHANDLERS;
        if (!parser->is_option_defaulted("gdb")) {
            flags = flags | GDB;
            parser->get<std::string>("gdb", gdbopts);
        }
        if (!parser->is_option_defaulted("timeout")) {
            flags = flags | TIMER;
            seconds = parser->get<int>("timeout");
        }
    }
#endif

#ifdef WITH_ARGSPARSER        
    void start(const char *argv0) {
        assert(parser);
#else
    void start(const char *argv0, int flags, unsigned seconds = 10) {
#endif        
        if (flags & SIGHANDLERS) {
            signal(SIGSEGV, dbg_helper_signal_handler);
            signal(SIGILL, dbg_helper_signal_handler);
            signal(SIGBUS, dbg_helper_signal_handler);
            signal(SIGFPE, dbg_helper_signal_handler);
            signal(SIGABRT, dbg_helper_signal_handler);
        }
        if (flags & GDB) {
#ifdef WITH_ARGSPARSER        
            // opts[2] is a gdb executable name;
            // opts[0] is a gdb script file name
            g.start(gdbopts[2], argv0, gdbopts[0]);
#else
            g.start("gdb", argv0, "gdb.cmd");
#endif        
        }
        if ((flags & TIMER) && seconds) {
            signal(SIGALRM, dbg_helper_signal_handler);
            alarm(seconds);
        }
    }
    void init_output(int rank = 0) {
#ifdef WITH_ARGSPARSER        
        assert(parser);
        if (flags & GDB) {
            // opts[1] is output file name (%r if presents is replaced with rank id)
            g.set_output_file(gdbopts[1], rank);
        }
#else
        g.set_output_file("gdb.%r.out", rank);
#endif        
    }
};
