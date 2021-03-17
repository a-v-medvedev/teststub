#include <iostream>
#include <string>
#include <stdlib.h>
#include <cassert>
#include <mpi.h>

#define WITH_ARGSPARSER
#ifdef WITH_ARGSPARSER
#include "argsparser.h"
#endif

#include "dbg_helper.h"


void cmdline_failure(const std::string &s) {
    std::cerr << s << std::endl;
    exit(-1);
}

bool good_output = true;
int case_passed() {
    // make good yaml
    return 0;
}

int case_failed() {
    // make bad yaml
    good_output = false;
    return 0;
}

int endless_loop() {
    double a = 2, b = 4, c = 0, i = 0;
    for (;;) {
        i++;
        c = a * a * i + b * b / i + c;
        if (i == 1000000) {
            i = 0;
        }
    }
    return c;
}

int case_timeout() {
    endless_loop();
    return 0;
}

void func_with_assert_lev3() {
    assert(0 && "here the assert bangs");
}

void func_with_assert_lev2() {
    func_with_assert_lev3();
}

void func_with_assert_lev1() {
    func_with_assert_lev2();
}

int case_assert() {
    func_with_assert_lev1();
    return 0;
}

void func_with_sigsegv_lev3() {
    char *p = 0;
    *p = 0;
}

void func_with_sigsegv_lev2() {
    func_with_sigsegv_lev3();
}

void func_with_sigsegv_lev1() {
    func_with_sigsegv_lev2();
}

int case_crash() {
    func_with_sigsegv_lev1();
    return 0;
}

int case_nonzero() {
    return 1;
}

void func_with_exception_lev2() {
    throw 0;
}

void func_with_exception_lev1() {
    func_with_exception_lev2();
}

int case_exception() {
    func_with_exception_lev1();
    return 0;
}

#define YAML_OUT(KEY, VALUE) out << YAML::Key << YAML::Flow << KEY << YAML::Value << VALUE;

int main(int argc, char **argv) 
{
    MPI_Init(&argc, &argv);
    int retcode = 0;
    
    //-- dbg --
    dbg_helper dbg;
#ifndef WITH_ARGSPARSER
    if (argc != 2) {
        cmdline_failure("Single argument required: {P,F,N,T,A,C,E}.");
    }
    char code = *(argv[1]);
    int mode = dbg_helper::SIGHANDLERS;
    if (code == 'T') {
        mode = mode | (int)dbg_helper::TIMER;
    }
    //mode = mode | (int)dbg_helper::GDB;
    dbg.start(argv[0], mode, 20);
#else
    args_parser parser(argc, argv, "-", ' ');
    parser.add<std::string>("load");
    parser.add<std::string>("output");
    parser.add<std::string>("mode", "always");
    parser.add<std::string>("code", "P");
    dbg.add_parser_options(parser);    
    if (!parser.parse())
        return 1;
    dbg.get_parser_options();
    dbg.start(argv[0]);
    char code = parser.get<std::string>("code").c_str()[0];
    auto randomization = parser.get<std::string>("mode");
    int threshold = 0;
    if (randomization == "always") {
        threshold = 0;
    } else if (randomization == "never") {
        threshold = 100;
    } else if (randomization == "rand1") {
        threshold = 1;
    } else if (randomization == "rand5") {
        threshold = 5;
    } else if (randomization == "rand10") {
        threshold = 10;
    } else if (randomization == "rand50") {
        threshold = 50;
    } else if (randomization == "rand90") {
        threshold = 90;
    } else if (randomization == "rand95") {
        threshold = 95;
    } else if (randomization == "rand99") {
        threshold = 99;
    } else {
        cmdline_failure("Wrong mode option");
    }
    srand(time(NULL));
    if (rand()%100 < threshold)
        code = 'P';
#endif
   
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    dbg.init_output(rank);
    //-- /dbg --

    try {
        switch (code) {
            case 'P': retcode = case_passed(); break;
            case 'F': retcode = case_failed(); break;
            case 'N': retcode = case_nonzero(); break;
            case 'T': retcode = case_timeout(); break;
            case 'A': retcode = case_assert(); break;
            case 'C': retcode = case_crash(); break;
            case 'E': retcode = case_exception(); break;
            default: 
                cmdline_failure("Single argument required: {P,F,T,A,C,N,E}.");
        }
    }
    catch(...) {
        stacktrace::print("\n>> CAUGHT UNHANDLED EXCEPTION!", 2);
    }
    MPI_Finalize();
#ifdef WITH_ARGSPARSER
    if (rank == 0) {
        auto infilename =  parser.get<std::string>("load");
        // yaml in
        std::ifstream ifs(infilename);
        auto stream = YAML::Load(ifs);
        auto X = stream["parameter"].as<double>();

        auto outfilename = parser.get<std::string>("output");
        // yaml out
        std::ofstream ofs(outfilename);
        YAML::Emitter out;
        out << YAML::BeginDoc;
        out << YAML::BeginMap;
        out << YAML::Key << "sec1" << YAML::Value;
            out << YAML::BeginMap;
            YAML_OUT("p1", X + .1010101 + (good_output ? 0 : (rand()%100) * 0.015));
            YAML_OUT("p2", X + .2020202);
            out << YAML::EndMap;
        out << YAML::Key << "sec2" << YAML::Value;
            out << YAML::BeginMap;
            YAML_OUT("r1", X + 1.010101 + (good_output ? 0 : (rand()%200) * 0.007));
            out << YAML::EndMap;
        out << YAML::EndMap;
        out << YAML::EndDoc;
        ofs << out.c_str();
    }
#endif
    return retcode;
}
