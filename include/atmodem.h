#ifndef __ND_AT_MODEM_H__
#define __ND_AT_MODEM_H__

#include <mutex>
#include <string>
#include <vector>

namespace pt_tool {

class AtModem {
  public:
    AtModem();
    virtual ~AtModem();
    bool open(const std::string& modemPort);
    bool isOpened() const;
    void close();
    bool flush();
    bool write(const std::string& command);
    bool readLine(int timeoutMs, std::string* output);
    std::string expect(const std::vector<std::string>& keywords, int timeoutMs);
    bool sendCmd(const std::string& cmd, int timeoutMs, std::string* strOuput = nullptr);
    bool sendCmdExpect(const std::string& cmd, const std::string& keyword, int timeoutMs,
                       std::string* strOuput = nullptr);

  private:
    int fd_;
    std::string dataLeft_;
    std::mutex mutex_;
    static constexpr const size_t kBufferSize = 1024;
    bool getLine(std::string* data, std::string* output);
    std::string escapeCr(const std::string& str);
};

}  // namespace pt_tool

#endif
