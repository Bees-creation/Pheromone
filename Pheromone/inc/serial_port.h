#pragma once

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#error
#endif

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

/**
 * @brief 串口配置参数
 * @param portName 端口号
 * @param friendlyName 设备名称
 * @param baudRate 波特率
 * @param dataBits 数据位
 * @param stopBits 停止位
 * @param parity 校验位
 */
struct SerialConfig {
    std::string portName;
    std::string friendlyName;
    int baudRate     = 115200;
    int dataBits     = 8;
    int stopBits     = 1;
    int parity       = 0;
};

/**
 * @brief 枚举当前系统可用的串口
 * @retval 当前系统可用的串口
 */
std::vector<SerialConfig> enumerateSerialPorts();

/**
 * @brief 串口封装类
 * @note 同步读写，配合后台线程使用
 */
class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    /**
     * @brief 打开串口
     * @param cfg 串口配置
     * @retval 是否打开成功
     * @note 失败时使用 lastError() 获取原因
     */
    bool open(const SerialConfig& cfg);

    // 关闭串口
    void close();

    // 获取串口打开状态
    bool isOpen() const { return handle_ != INVALID_HANDLE_VALUE; }

    /**
     * @brief 写入数据
     * @param data 字节流地址
     * @param len 字节流长度
     * @retval 实际写入字节数
     */
    int write(const uint8_t* data, int len);

    /**
     * @brief 读取数据
     * @param buf 接收缓冲区地址
     * @param bufSize 接收缓冲区长度
     * @retval 实际读取字节数，返回 -1 表示错误
     */
    int read(uint8_t* buf, int bufSize);

    // 获取底层句柄，可用于 Fl::add_fd()
    HANDLE nativeHandle() const { return handle_; }

    // 获取最近一次错误信息
    const std::string& lastError() const { return lastError_; }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::string lastError_;
    SerialConfig config_;

    /**
     * @brief 将 config_ 保存的配置应用到打开的 handle_ 上
     * @retval 是否应用成功
     */
    bool applyConfig();

    // 保存错误信息到 lastError_
    void setError(const std::string& msg);
};
