#include "serial_port.h"

#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>

#include <sstream>
#include <cstdio>

/**
 * GUID_DEVINTERFACE_COMPORT = {86E0D1E0-8089-11D0-9CE4-08003E301F73};
 * 这是 Windows 定义的串口类设备标准 GUID，只要系统里注册了“串口”这个设备接口类，
 * Windows 就会把所有串口设备挂载到这个 GUID 下，
 * 官方推荐使用设备接口类来枚举 COM 口，而不是遍历注册表或尝试打开 COM1~COM256
 */
DEFINE_GUID(GUID_DEVINTERFACE_COMPORT,
    0x86E0D1E0, 0x8089, 0x11D0,
    0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73);

/**
 * @brief 宽字符处理，将 wchar_t 格式的字符转换为 string 格式的字符串
 * @param wstr wchar_t 格式的字符
 * @retval string 格式的字符串
 */
static std::string wideToUtf8(const wchar_t* wstr) {
    if (!wstr || !*wstr) return {};

    int len = WideCharToMultiByte(
        CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};

    std::string result(len, '\0');
    int written = WideCharToMultiByte(
        CP_UTF8, 0, wstr, -1, result.data(), len, nullptr, nullptr);
    if (written <= 0) return {};

    result.resize(written - 1); // 去掉字符串结尾的 '\0'
    return result;
}

/**
 * @brief 获取设备属性
 * @param devInfo 设备信息集句柄
 * @param data 具体设备实例的信息
 * @param property 指定读取的属性
 * @retval 设备属性
 */
static std::string getDeviceProperty(HDEVINFO devInfo, SP_DEVINFO_DATA& data, DWORD property) {
    DWORD type = 0;
    DWORD size = 0;

    // 查询所需字节数
    SetupDiGetDeviceRegistryPropertyW(
        devInfo, &data, property, &type,
        nullptr, 0, &size);

    if (size == 0) return {};  // 属性不存在或为空

    std::wstring buf(size / sizeof(wchar_t), L'\0');

    if (!SetupDiGetDeviceRegistryPropertyW(
            devInfo, &data, property, &type,
            reinterpret_cast<PBYTE>(buf.data()),
            size, &size)) {
        return {};
    }

    // 根据实际写入长度裁剪
    buf.resize(size / sizeof(wchar_t));
    if (!buf.empty() && buf.back() == L'\0') {
        buf.pop_back();
    }

    return wideToUtf8(buf.c_str());
}

std::vector<SerialConfig> enumerateSerialPorts() {
    std::vector<SerialConfig> ports;

    // 获取设备信息集句柄
    HDEVINFO devInfo = SetupDiGetClassDevsW(
        &GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (devInfo == INVALID_HANDLE_VALUE)
        return ports;

    SP_DEVINFO_DATA devInfoData;  // 用于描述设备信息集中的某个设备
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devInfoData); ++i) {
        SerialConfig cfg;

        // 打开设备注册表键
        HKEY hKey = SetupDiOpenDevRegKey(
            devInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);

        if (hKey == INVALID_HANDLE_VALUE) continue;

        wchar_t portNameW[256] = {};
        DWORD size = sizeof(portNameW);
        DWORD type = 0;

        // 读取 PortName
        if (RegQueryValueExW(hKey, L"PortName", nullptr, &type,
                             reinterpret_cast<LPBYTE>(portNameW), &size) == ERROR_SUCCESS) {
            if (portNameW[0] != L'\0')
                cfg.portName = wideToUtf8(portNameW);
        }

        // 关闭注册表键
        RegCloseKey(hKey);

        if (cfg.portName.empty()) continue;

        // 读取设备友好名称
        cfg.friendlyName = getDeviceProperty(devInfo, devInfoData, SPDRP_FRIENDLYNAME);

        // 读取设备描述
        std::string description = getDeviceProperty(devInfo, devInfoData, SPDRP_DEVICEDESC);

        if (cfg.friendlyName.empty()) {
            if (!description.empty())
                cfg.friendlyName = description + " (" + cfg.portName + ")";
            else
                cfg.friendlyName = cfg.portName;
        }

        ports.push_back(std::move(cfg));
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return ports;
}

SerialPort::~SerialPort() {
    close();
}

void SerialPort::setError(const std::string& msg) {
    lastError_ = msg;
}

bool SerialPort::open(const SerialConfig& cfg) {
    close();
    config_ = cfg;
    lastError_.clear();

    // \\.\ 是 Windows 设备命名空间前缀
    // 如果直接访问 COMx，当 x >= 10 时可能会被当成普通文件
    // 如果用 \\.\COMx，则可访问任意编号的串口
    std::string fullName = "\\\\.\\" + cfg.portName;

    handle_ = CreateFileA(
        fullName.c_str(),
        GENERIC_READ | GENERIC_WRITE,  // 读写权限
        0,                      // 串口独占
        nullptr,
        OPEN_EXISTING,          // 只打开已存在的设备
        FILE_FLAG_OVERLAPPED,   // 异步 I/O，配合 read 超时
        nullptr);

    if (handle_ == INVALID_HANDLE_VALUE) {
        setError("无法打开 " + fullName + "，错误码 " +
                 std::to_string(GetLastError()));
        return false;
    }

    if (!applyConfig()) {
        close();
        return false;
    }

    // 设置驱动内部收发缓冲区建议大小
    // 这是建议值，驱动可以自行调整，不设定时系统会调用默认值
    SetupComm(handle_, 4096, 4096);
    PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);

    return true;
}

void SerialPort::close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
}

bool SerialPort::applyConfig() {
    DCB dcb = {};  // 用于描述串口参数
    dcb.DCBlength = sizeof(DCB);

    // 将当前配置信息读入 dcb，后续只需要更改指定配置，其他配置保持默认
    if (!GetCommState(handle_, &dcb)) {
        setError("GetCommState 失败");
        return false;
    }

    // 配置波特率和数据位
    dcb.BaudRate = static_cast<DWORD>(config_.baudRate);
    dcb.ByteSize = static_cast<BYTE>(config_.dataBits);

    // 配置停止位
    switch (config_.stopBits) {
        case 1:  dcb.StopBits = ONESTOPBIT;   break;
        case 2:  dcb.StopBits = TWOSTOPBITS;  break;
        default: dcb.StopBits = ONESTOPBIT;   break;
        // Windows 还支持 1.5 位，宏定义是 ONE5STOPBITS
    }

    // 配置校验位
    switch (config_.parity) {
        case 1:  dcb.Parity = ODDPARITY;   dcb.fParity = TRUE; break;
        case 2:  dcb.Parity = EVENPARITY;  dcb.fParity = TRUE; break;
        default: dcb.Parity = NOPARITY;    dcb.fParity = FALSE; break;
    }

    dcb.fBinary = TRUE;        // 二进制模式，Windows 串口必须为 TRUE
    dcb.fOutxCtsFlow = FALSE;  // CTS 硬件流控
    dcb.fOutxDsrFlow = FALSE;  // DSR 硬件流控
    dcb.fDtrControl  = DTR_CONTROL_ENABLE;  // DTR 线保持有效
    dcb.fRtsControl  = RTS_CONTROL_ENABLE;  // RTS 线保持有效

    // 写入更改后的配置
    if (!SetCommState(handle_, &dcb)) {
        setError("SetCommState 失败，错误码 " +
                 std::to_string(GetLastError()));
        return false;
    }

    // 超时设置
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout         = 50;    // 读操作字节间超时
    timeouts.ReadTotalTimeoutMultiplier  = 0;     // 不按每次读取的字节数而增加超时时间
    timeouts.ReadTotalTimeoutConstant    = 50;    // 读操作总超时
    timeouts.WriteTotalTimeoutMultiplier = 0;     // 不按每次写入的字节数而增加超时时间
    timeouts.WriteTotalTimeoutConstant   = 100;   // 写操作总超时

    if (!SetCommTimeouts(handle_, &timeouts)) {
        setError("SetCommTimeouts 失败");
        return false;
    }

    return true;
}

int SerialPort::write(const uint8_t* data, int len) {
    if (!isOpen()) {
        setError("串口未打开");
        return -1;
    }

    DWORD written = 0;   // 实际写入的字节数
    OVERLAPPED ov = {};  // 创建 OVERLAPPED 事件
    ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    /**
     * 因为串口的打开方式是 FILE_FLAG_OVERLAPPED，异步 I/O，写入操作可能是立即完成，
     * 也可能是操作被提交在后台，在 GetOverlappedResult 完成之后
     */
    if (!WriteFile(handle_, data, static_cast<DWORD>(len), &written, &ov)) {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING) {
            if (!GetOverlappedResult(handle_, &ov, &written, TRUE)) {
                CloseHandle(ov.hEvent);
                setError("写超时或失败");
                return -1;
            }
        } else {
            CloseHandle(ov.hEvent);
            setError("WriteFile 失败，错误码 " + std::to_string(err));
            return -1;
        }
    }

    CloseHandle(ov.hEvent);
    return static_cast<int>(written);
}

int SerialPort::read(uint8_t* buf, int bufSize) {
    if (!isOpen()) return -1;

    DWORD readBytes = 0;
    OVERLAPPED ov = {};
    ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    // 和写入类似
    if (!ReadFile(handle_, buf, static_cast<DWORD>(bufSize), &readBytes, &ov)) {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING) {
            if (!GetOverlappedResult(handle_, &ov, &readBytes, TRUE)) {
                CloseHandle(ov.hEvent);
                return -1;
            }
        } else {
            CloseHandle(ov.hEvent);
            return -1;
        }
    }

    CloseHandle(ov.hEvent);
    return static_cast<int>(readBytes);
}
