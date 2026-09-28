#pragma once

#include <FL/Fl_Window.H>
#include <FL/Fl_Flex.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Multiline_Input.H>
#include <FL/Fl_Input_Choice.H>

#include "serial_port.h"

#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <vector>

class SerialAssistant : public Fl_Window {
public:
    SerialAssistant(int W, int H, const char* title);
    ~SerialAssistant() override;

private:
    // UI 控件
    Fl_Choice* portChoice_        = nullptr;    // 串口选择
    Fl_Input_Choice* baudChoice_  = nullptr;    // 波特率选择
    Fl_Choice* dataChoice_        = nullptr;    // 数据位选择
    Fl_Choice* stopChoice_        = nullptr;    // 停止位选择
    Fl_Choice* parityChoice_      = nullptr;    // 校验位选择
    Fl_Button* openBtn_           = nullptr;    // 串口开关按钮

    Fl_Text_Buffer* recvBuf_      = nullptr;    // 用于接收的缓冲区
    Fl_Text_Display* recvDisplay_ = nullptr;    // 用于接收显示的显示栏

    Fl_Multiline_Input* sendInput_ = nullptr;   // 用于发送的多行输入栏
    Fl_Check_Button* hexRecv_      = nullptr;   // 以 Hex 模式接收的选项
    Fl_Check_Button* hexSend_      = nullptr;   // 以 Hex 模式发送的选项
    Fl_Check_Button* autoScroll_   = nullptr;   // 自动滚动模式的选项
    Fl_Button* sendBtn_            = nullptr;   // 发送按钮

    // 串口状态
    SerialPort serial_;                  // 串口实例
    std::thread readThread_;             // 读取线程
    std::atomic<bool> reading_{false};   // 读取标志位
    std::mutex portMutex_;               // 互斥锁
    std::vector<SerialConfig> ports_;    // 最近一次枚举到的串口，菜单项的 user_data 指向这里的元素

    /**
     * @brief 辅助函数，用于构建一个文本显示 UI
     * @param text 显示的文本
     * @retval Fl_Box 标签控件
     */
    Fl_Box* makeLabel(const char* text);

    // 将提示框定位到屏幕中间
    void showMessage(const std::string& msg);
    void showAlert(const std::string& msg);

    // 构建所有 UI
    void buildUI();
    
    // 刷新串口列表
    void refreshPortList();

    /**
     * @brief FL 按钮的回调函数
     * @param w 触发回调的控件指针
     * @param data 用户自定义的数据指针，通常指向到本类的实例，用于重定向到业务函数
     */
    static void onOpenClose(Fl_Widget* w, void* data);
    static void onSend(Fl_Widget* w, void* data);
    static void onRefreshPorts(Fl_Widget* w, void* data);

    // 业务函数
    void toggleOpenClose();
    void sendData();
    void startReadThread();
    void stopReadThread();
    void readLoop();

    // 线程安全的 UI 更新函数
    void appendRecvText(const std::string& text);
    static void appendRecvText_cb(void* data);

    // 字节流 Hex 流相互转换
    static std::string bytesToHex(const uint8_t* data, int len);
    static bool hexStringToBytes(const std::string& hex, std::vector<uint8_t>& out);

    // 字节流转显示
    static std::string bytesToDisplay(const uint8_t* data, int len, bool hexMode);

    // 返回选定的串口
    std::string getSelectedPort() const;
};
