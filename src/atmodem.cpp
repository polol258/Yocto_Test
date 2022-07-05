#define LOG_TAG "NM_LTE"
#include "atmodem.h"

#include <fcntl.h>
//#include <log/log.h>
//#include <ndprop.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <regex>
#include <string>
#include <vector>

namespace pt_tool {

AtModem::AtModem() : fd_(-1) {}

AtModem::~AtModem() {
    if (fd_ != -1) ::close(fd_);
}

bool AtModem::open(const std::string& port) {
    if (fd_ != -1) {
        dataLeft_.clear();
        ::close(fd_);
    }

    fd_ = ::open(port.c_str(), O_RDWR);
    if (fd_ == -1) return false;

    struct termios pts;
    tcgetattr(fd_, &pts);

    pts.c_lflag &= ~(ECHOE | ECHOCTL);
    pts.c_cflag &= ~HUPCL;
    pts.c_cc[VMIN] = 1;
    pts.c_cc[VTIME] = 0;  // 0-block, 1-100ms

    pts.c_oflag &= ~ONLCR;
    pts.c_iflag &= ~INPCK;

    // ignore modem control & enable read
    pts.c_cflag |= (CLOCAL | CREAD);

    /* no flow control */
    pts.c_cflag &= ~CRTSCTS;
    pts.c_iflag &= ~(IXON | IXOFF | IXANY);

    /* stop bit=1 */
    pts.c_cflag &= ~CSTOPB;

    /* raw mode */
    pts.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    pts.c_oflag &= ~OPOST;
    pts.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    pts.c_cflag &= ~(CSIZE | PARENB);
    pts.c_cflag |= CS8;

    /* set baudrate */
    cfsetospeed(&pts, B115200);
    cfsetispeed(&pts, B115200);

    tcsetattr(fd_, TCSANOW, &pts);

    return true;
}

void AtModem::close() {
    if (fd_ != -1) {
        dataLeft_.clear();
        ::close(fd_);
        fd_ = -1;
    }
}

bool AtModem::flush() {
    if (isOpened()) {
        return (0 == tcflush(fd_, TCIOFLUSH));
    }
    dataLeft_.clear();
    return false;
}

bool AtModem::isOpened() const { return (fd_ != -1); }

bool AtModem::write(const std::string& command) {
    if (!isOpened()) return false;

    //ALOGV("--> %s", escapeCr(command).c_str());

    ssize_t wSize = ::write(fd_, command.data(), command.length());
    if (wSize < 0) {
        //ALOGE("%s failed, error:%s", __func__, strerror(errno));
        return false;
    } else if (static_cast<size_t>(wSize) != command.length()) {
        //ALOGE("%s incompleted size, real/expect=%d/%d", __func__, wSize,
        //      command.length());
        return false;
    }
    return true;
}

// retrurn value:
// true  - buffer contains valid line
// false - buffer does not contain valid line
bool AtModem::getLine(std::string* data, std::string* output) {
    // remove heading \r \n
    while (!data->empty() && (data->at(0) == '\n' || data->at(0) == '\r')) {
        data->erase(data->begin());
    }
    if (data->empty()) return false;

    // search for CR
    std::size_t pos = data->find_first_of("\r\n");
    if (pos == std::string::npos) return false;

    *output = data->substr(0, pos);
    data->erase(0, pos + 1);

    while (!data->empty() && (data->at(0) == '\n' || data->at(0) == '\r')) {
        data->erase(data->begin());
    }

    return true;
}

bool AtModem::readLine(int timeoutMs, std::string* output) {
    if (!isOpened()) return false;

    std::string data;

    // dataLeft_ may contain vaild lines
    if (getLine(&dataLeft_, &data)) {
        // got a complete line!
        //ALOGV("<-- %s", escapeCr(data).c_str());
        if (output != nullptr) *output = data;
        return true;
    }

    // set up fd_ monitor
    struct pollfd fds[1];
    fds[0].fd = fd_;
    fds[0].events = POLLIN;

    while (true) {
        // Check read timeout
        int ret = poll(fds, 1, timeoutMs);
        if (ret == -1) {
            // error
            //ALOGW("poll() error, data=%s", data.c_str());
            dataLeft_.clear();
            return false;
        } else if (ret == 0) {
            // timeout
            //ALOGV("Read timeout(%dms), data=%s", timeoutMs, data.c_str());
            dataLeft_.clear();
            return false;
        }

        char readBuf[kBufferSize] = {0};
        size_t rSize = ::read(fd_, readBuf, kBufferSize);
        if (rSize > 0) {
            // append readBuf to dataLeft_ and search for a valid line
            dataLeft_ += readBuf;
            if (getLine(&dataLeft_, &data)) {
                // got a complete line!
                //ALOGV("<-- %s", escapeCr(data).c_str());
                if (output != nullptr) *output = data;
                return true;
            }
        }
    }
}

// return string read so far
std::string AtModem::expect(const std::vector<std::string>& keywords, int timeoutMs) {
    if (!isOpened()) return std::string();

    std::string output;

    while (true) {
        std::string s;
        bool ret = AtModem::readLine(timeoutMs, &s);
        // ret == false          -- timeout or other error
        // keywords.empty()      -- treated as error
        if (!ret || keywords.empty()) {
            return std::string();
        }

        // if any keyword is found, return the string read so far
        output += s;
        for (auto&& word : keywords) {
            // return matched response
            std::size_t found = s.find(word);
            if (found != std::string::npos) {
                return output;
            }
        }
    }
}

/**
 * AtModem::sendCmd():
 * send $cmd to serial port and wait $timeoutMs for modem's OK/ERROR response.
 * return true if OK is received.
 *
 * @param cmd        command to be send to serial port module.
 * @param timeoutMs  wait $timeoutMs in millisecond for OK or ERROR response
 * @param strOutput  optinal. serial port data is read so far. this buffer is only
 *                   filled up when OK or ERROR is received.
 *
 * @return true if the AT command is successful (OK is received) or false when ERROR
 *         is received or timeout.
 */
bool AtModem::sendCmd(const std::string& cmd, int timeoutMs, std::string* strOutput) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!flush()) {
        //ALOGE("[%s:%d]%s() tcflush failed: %s", __FILE__, __LINE__, __func__,
        //      strerror(errno));
        return false;
    }
    if (!write(cmd)) {
        //ALOGE("[%s:%d]%s() write cmd fail!! %s", __FILE__, __LINE__, __func__,
        //      escapeCr(cmd).c_str());
        return false;
    }

    // expect OK or ERROR
    std::vector<std::string> atResults({"OK", "ERROR"});
    std::string s = expect(atResults, timeoutMs);
    if (s.empty()) {
        //ALOGE("[%s:%d]%s() cmd[%s] has no OK or ERROR response in %dms", __FILE__,
        //      __LINE__, __func__, cmd.c_str(), timeoutMs);
        return false;
    }

    // Fill the output buffer if either OK or ERROR is received.
    if (strOutput != nullptr) *strOutput = s;

    if (s.find("ERROR") != std::string::npos) {
        //ALOGW("[%s:%d]%s() cmd[%s] response \"ERROR\"", __FILE__, __LINE__, __func__,
        //      escapeCr(cmd).c_str());
        return false;
    }

    return true;
}

/**
 * AtModem::sendCmdExpect():
 * send $cmd to serial port and wait $timeoutMs for modem's OK/ERROR response.
 * return true if OK is received and the output contains $keyword.
 *
 * @param cmd        command to be send to serial port module.
 * @param keyword    find $keyword in response string.
 * @param timeoutMs  wait $timeoutMs in millisecond for OK or ERROR response
 * @param strOutput  optinal. serial port data is read so far. this buffer is only
 *                   filled up when OK or ERROR is received.
 * returning string  in serial port response (default is true).
 *
 * @return true if we found $keyword in module response string, false has following
 *         condiions,
 *         1) ERROR is received.
 *         2) OK/ERROR is expected, but timed out.
 *         3) $keyword is not found
 */
bool AtModem::sendCmdExpect(const std::string& cmd, const std::string& keyword,
                            int timeoutMs, std::string* strOutput) {
    std::string s;
    if (!sendCmd(cmd, timeoutMs, &s)) return false;

    if (strOutput != nullptr) *strOutput = s;

    // return true if keyword is found, otherwise false
    return s.find(keyword) != std::string::npos;
}

std::string AtModem::escapeCr(const std::string& str) {
    return std::regex_replace(str, std::regex("\r"), "\\r");
}

}  // namespace pt_tool
