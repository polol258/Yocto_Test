#ifndef __ND_SHELL__
#define __ND_SHELL__

//#include <log/log.h>
#include <syslog.h>
#include <stdio.h>     // popen()
#include <stdlib.h>    // system()
#include <sys/wait.h>  // WIFEXITED
#include <string>

static inline void shell(const std::string& cmd) {
    if (cmd.empty()) {
        //ALOGE("%s: empty shell command", __func__);
        return;
    }

    int status = system(cmd.c_str());
    if (status < 0) {
        //ALOGE("%s(\"%s\") initialize error", __func__, cmd.c_str());
        return;
    }

    if (WIFEXITED(status)) {
        //ALOGV("%s(\"%s\") = %d", __func__, cmd.c_str(), WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        //ALOGW("%s(\"%s\") terminated by sig=%d", __func__, cmd.c_str(), WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
        //ALOGW("%s(\"%s\") stopped by sig=%d", __func__, cmd.c_str(), WSTOPSIG(status));
    }
}

static inline int shellEx(const std::string& cmd, std::string* output) {
    if (cmd.empty()) {
        //ALOGE("%s: empty shell command", __func__);
        return -1;
    }

    if (output == nullptr) {
        //ALOGE("%s(\"%s\"): output ptr cannot be null", __func__, cmd.c_str());
        return -1;
    }

    FILE* fp = popen(cmd.c_str(), "r");
    if (fp == nullptr) {
        //ALOGE("%s(\"%s\"): popen failed", __func__, cmd.c_str());
        return -1;
    }

    char buf[2048];
    output->clear();
    while (fgets(buf, sizeof(buf) - 1, fp)) {
        *output += std::string(buf);
    }

    int ret = -1;
    int status = pclose(fp);
    if (status < 0) {
        // ECHILD: Cannot obtain child status
        //ALOGE("%s(\"%s\"): pclose failed", __func__, cmd.c_str());
    } else if (WIFEXITED(status)) {
        ret = WEXITSTATUS(status);
        //ALOGV("%s(\"%s\") = %d", __func__, cmd.c_str(), ret);
    } else if (WIFSIGNALED(status)) {
        //ALOGW("%s(\"%s\") terminated by sig=%d", __func__, cmd.c_str(), WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
        //ALOGW("%s(\"%s\") stopped by sig=%d", __func__, cmd.c_str(), WSTOPSIG(status));
    }
    return ret;
}

#endif
