#pragma once
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <memory>
#include <thread>
#include <mutex>

#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

//extern ID id;

static inline void str_replace_all(std::string &str, const std::string &from,
                                   const std::string &to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

class gdb_monitor {
  public:
    std::string gdb, exe, batch, save;
    pid_t child_pid = 0;
    pid_t parent_pid = 0;
    int pipefd[2];
    FILE *output;
    std::thread thr;
    std::mutex mtx;
    bool got_output = false;
    void start(const std::string &_gdb, const std::string &_exe, const std::string &_batch) {
        gdb = _gdb;
        exe = _exe;
        batch = _batch;
        std::string str_parent_pid;
        parent_pid = getpid();
        str_parent_pid = std::to_string(parent_pid);
        pipe(pipefd);       // create a pipe
        child_pid = fork(); // span a child process
        if (child_pid == 0) {
            // Child. Let's redirect its standard output to our pipe and replace process with gdb
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            execlp(gdb.c_str(), gdb.c_str(), "-batch", "-x", batch.c_str(), "--pid",
                   str_parent_pid.c_str(), exe.c_str(), (char *)NULL);
            std::cerr << "ERROR: execlp failed!" << std::endl;
            std::cerr << "ERROR: execlp line was: " << gdb.c_str() << " "
                      << "-batch"
                      << " "
                      << "-x"
                      << " " << batch.c_str() << " "
                      << "--pid"
                      << " " << str_parent_pid.c_str() << " " << exe.c_str() << std::endl;
            exit(1);
        }

        // Only parent gets here. Listen to what the gdb says
        close(pipefd[1]);

        int flags;
        flags = fcntl(pipefd[0], F_GETFL, 0);
        flags |= O_NONBLOCK;
        fcntl(pipefd[0], F_SETFL, flags);
        output = fdopen(pipefd[0], "r");
        std::thread t(&gdb_monitor::output_reader, this);
        t.detach();
        thr = std::move(t);
        wait_for_some_output();
    }
    void wait_for_some_output() {
        size_t cnt = 100000;
        while (!got_output && --cnt) {
            usleep(100);
        }
        if (cnt == 0) {
            std::cout << "gdb: ERROR: set_output_file: failed at waiting stage, Child process is "
                         "silent."
                      << std::endl;
        }
        usleep(cnt * 10);
    }
    void set_output_file(const std::string &_save, int rank = 0) {
        std::lock_guard<std::mutex> lk(mtx);
        save = _save;
        str_replace_all(save, "%r", std::to_string(rank));
    }
    void output_reader() {
        std::ofstream file_to_save;
        std::string out;
        while (!update(out)) {
            {
                std::lock_guard<std::mutex> lk(mtx);
                if (save == "") {
                    if (!out.empty()) {
                        std::cout << gdb << ": " << parent_pid << ": " << out;
                        got_output = true;
                    }
                } else {
                    if (file_to_save.is_open()) {
                        file_to_save << out;
                        got_output = true;
                    } else {
                        file_to_save.open(save);
                    }
                }
            }
            out = "";
            file_to_save.flush();
            usleep(1000);
        }
        file_to_save.close();
    }
    bool update(std::string &out) {
        if (child_pid == 0) {
            return true;
        }
        char line[256];
        char *ret = fgets(line, sizeof(line), output);
        if (ret == NULL) {
            if (errno == EWOULDBLOCK) {
                if (wait_non_blocking()) {
                    return true;
                }
                return false;
            }
            wait();
            return true;
        }
        out = ret;
        return false;
    }
    void kill() {
        if (child_pid) {
            usleep(100000);
            if (!wait_non_blocking())
                ::kill(child_pid, SIGKILL);
            wait();
        }
    }
    void wait() {
        int status;
        if (child_pid)
            waitpid(child_pid, &status, 0);
        child_pid = 0;
    }
    bool wait_non_blocking() {
        if (!child_pid)
            return true;
        int status;
        int res = 0;
        res = waitpid(child_pid, &status, WNOHANG);
        if (res == 0)
            return false;
        child_pid = 0;
        return true;
    }
    ~gdb_monitor() { kill(); }
};
