#include "serial_assistant.h"

#include <FL/Fl.H>
#include <FL/fl_ask.H>

#include <cstring>
#include <sstream>
#include <iomanip>

SerialAssistant::SerialAssistant(int W, int H, const char* title)
    : Fl_Window(W, H, title) {
    recvBuf_ = new Fl_Text_Buffer();
    buildUI();

    // 初始化串口列表
    refreshPortList();

    // 屏幕居中
    int sx, sy, sw, sh;
    Fl::screen_work_area(sx, sy, sw, sh, 0);
    position(sx + (sw - W) / 2, sy + (sh - H) / 2);

    // 限制最小尺寸，避免控件被挤压缩成一团
    size_range(800, 450);

    end();
}

SerialAssistant::~SerialAssistant() {
    stopReadThread();
    serial_.close();
    delete recvBuf_;
}

Fl_Box* SerialAssistant::makeLabel(const char* text) {
    // 右对齐、无边框的纯文本标签，在参数行中紧贴右侧控件
    auto* b = new Fl_Box(0, 0, 0, 28, text);
    b->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
    b->box(FL_NO_BOX);
    return b;
}

void SerialAssistant::buildUI() {
    const int W = w();
    const int H = h();

    // 根布局，创建一个弹性布局容器，占据整个窗口，方向为水平，子控件从左到右排列
    Fl_Flex* root = new Fl_Flex(0, 0, W, H, Fl_Flex::HORIZONTAL);
    root->margin(4);    // 窗口四周留 4 像素空白
    root->gap(4);       // 子控件之间的间距为 4 像素

    // 显示区
    auto* mainArea = new Fl_Flex(0, 0, 0, 0, Fl_Flex::VERTICAL);
    mainArea->gap(4);                     // 子控件之间的间距为 4 像素
    recvDisplay_ = new Fl_Text_Display(0, 0, 0, 0);              // 接收显示区控件
    recvDisplay_->buffer(recvBuf_);                              // 绑定显示 recvBuf_ 的内容
    recvDisplay_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0); // 设置自动换行
    recvDisplay_->textfont(FL_COURIER);   // 设定为等宽字体
    recvDisplay_->textsize(13);           // 设定字号大小
    recvBuf_->text("");                   // 清空初始内容

    // 发送区
    auto* sendArea = new Fl_Flex(0, 0, 0, 0, Fl_Flex::VERTICAL);
    sendArea->gap(4);                                 // 子控件之间的间距为 4 像素
    sendInput_ = new Fl_Multiline_Input(0, 0, 0, 0);  // 发送区控件
    sendInput_->value("");                            // 清空初始内容
    sendInput_->when(FL_WHEN_ENTER_KEY_ALWAYS);       // 当按下回车键总是触发
    sendInput_->callback(onSend, this);               // 绑定发送回调到按钮

    // 选项行
    auto* sendOpts = new Fl_Flex(0, 0, 0, 0, Fl_Flex::HORIZONTAL);
    sendOpts->gap(4);       // 间距 4 像素
    hexRecv_    = new Fl_Check_Button(0, 0, 0, 0, "HEX 显示");
    hexSend_    = new Fl_Check_Button(0, 0, 0, 0, "HEX 发送");
    autoScroll_ = new Fl_Check_Button(0, 0, 0, 0, "自动滚动");
    autoScroll_->value(1);
    // 弹性占位，把发送按钮推到最右侧
    auto* spacerL = new Fl_Box(0, 0, 0, 0);
    spacerL->box(FL_NO_BOX);
    // 发送按钮
    sendBtn_ = new Fl_Button(0, 0, 0, 0, "发送");
    sendBtn_->callback(onSend, this);

    // 固定控件宽度，不参与弹性伸缩
    sendOpts->fixed(hexRecv_,     80);
    sendOpts->fixed(hexSend_,     80);
    sendOpts->fixed(autoScroll_,  80);
    sendOpts->fixed(sendBtn_,     64);
    sendOpts->end();

    // 固定控件高度
    sendArea->fixed(sendInput_, 90);   // 输入框高度
    sendArea->fixed(sendOpts,   24);   // 选项行高度
    sendArea->end();

    // 发送区固定高度
    mainArea->fixed(sendArea, 124);    // 90 + 4 + 30
    mainArea->end();

    // 右侧参数边栏
    auto* sideBar = new Fl_Flex(0, 0, 0, 0, Fl_Flex::VERTICAL);
    sideBar->gap(4);

    // 串口选项
    auto* portRow = new Fl_Flex(0, 0, 0, 0, Fl_Flex::HORIZONTAL);
    portRow->gap(4);
    auto* portLabel = makeLabel("端口号");
    portChoice_ = new Fl_Choice(0, 0, 0, 0);
    portRow->fixed(portLabel, 50);
    portRow->end();

    // 刷新按钮
    auto* refreshBtn = new Fl_Button(0, 0, 0, 0, "刷新串口");
    refreshBtn->callback(onRefreshPorts, this);

    // 波特率选项
    auto* baudRow = new Fl_Flex(0, 0, 0, 0, Fl_Flex::HORIZONTAL);
    baudRow->gap(4);
    auto* baudLabel = makeLabel("波特率");
    baudChoice_ = new Fl_Input_Choice(0, 0, 0, 0);
    baudChoice_->add("9600");
    baudChoice_->add("19200");
    baudChoice_->add("38400");
    baudChoice_->add("57600");
    baudChoice_->add("115200");
    baudChoice_->add("230400");
    baudChoice_->add("460800");
    baudChoice_->value("115200");   // 默认值，同时会选中下拉项
    baudRow->fixed(baudLabel, 50);
    baudRow->end();

    // 数据位选项
    auto* dataRow = new Fl_Flex(0, 0, 0, 0, Fl_Flex::HORIZONTAL);
    dataRow->gap(4);
    auto* dataLabel = makeLabel("数据位");
    dataChoice_ = new Fl_Choice(0, 0, 0, 0);
    dataChoice_->add("5|6|7|8");
    dataChoice_->value(3);
    dataRow->fixed(dataLabel, 50);
    dataRow->end();

    // 停止位选项
    auto* stopRow = new Fl_Flex(0, 0, 0, 0, Fl_Flex::HORIZONTAL);
    stopRow->gap(4);
    auto* stopLabel = makeLabel("停止位");
    stopChoice_ = new Fl_Choice(0, 0, 0, 0);
    stopChoice_->add("1|2");
    stopChoice_->value(0);
    stopRow->fixed(stopLabel, 50);
    stopRow->end();

    // 校验位选项
    auto* parityRow = new Fl_Flex(0, 0, 0, 0, Fl_Flex::HORIZONTAL);
    parityRow->gap(4);
    auto* parityLabel = makeLabel("校验位");
    parityChoice_ = new Fl_Choice(0, 0, 0, 0);
    parityChoice_->add("无|奇|偶");
    parityChoice_->value(0);
    parityRow->fixed(parityLabel, 50);
    parityRow->end();

    // 串口开关
    openBtn_ = new Fl_Button(0, 0, 0, 0, "打开串口");
    openBtn_->callback(onOpenClose, this);

    // 参数栏内各行高度
    sideBar->fixed(portRow,    28);
    sideBar->fixed(refreshBtn, 28);
    sideBar->fixed(baudRow,    28);
    sideBar->fixed(dataRow,    28);
    sideBar->fixed(stopRow,    28);
    sideBar->fixed(parityRow,  28);
    sideBar->fixed(openBtn_,   28);
    sideBar->end();

    root->fixed(sideBar, 200);  // 右侧参数栏固定宽度
    root->end();

    // root 作为缩放对象，窗口拉伸时内部自动重排
    resizable(root);
}

void SerialAssistant::refreshPortList() {
    portChoice_->clear();
    auto ports = enumerateSerialPorts();

    if (ports.empty()) {
        portChoice_->add("(无可用串口)");
        portChoice_->value(0);
        portChoice_->deactivate();
    } else {
        for (const auto& p : ports)
            portChoice_->add(p.friendlyName.c_str());
        portChoice_->value(0);
        portChoice_->activate();
    }
}

void SerialAssistant::onRefreshPorts(Fl_Widget* w, void* data) {
    auto* self = static_cast<SerialAssistant*>(data);
    if (self->serial_.isOpen()) {
        self->showMessage("请先关闭串口");
        return;
    }
    self->refreshPortList();
}

std::string SerialAssistant::getSelectedPort() const {
    const Fl_Menu_Item* item = portChoice_->mvalue();
    return item ? std::string(item->label()) : std::string();
}

void SerialAssistant::onOpenClose(Fl_Widget* w, void* data) {
    static_cast<SerialAssistant*>(data)->toggleOpenClose();
}

void SerialAssistant::toggleOpenClose() {
    if (serial_.isOpen()) {
        // 关闭：先停线程，再关串口，最后恢复 UI
        stopReadThread();
        serial_.close();
        openBtn_->label("打开串口");
        portChoice_->activate();
        baudChoice_->activate();
        dataChoice_->activate();
        stopChoice_->activate();
        parityChoice_->activate();
        return;
    }

    // 打开：从 UI 收集配置
    SerialConfig cfg;
    cfg.portName = getSelectedPort();
    cfg.baudRate = std::atoi(baudChoice_->value());

    // 校验：用户可能手输了非数字，atoi 会返回 0
    if (cfg.baudRate <= 0) {
        showAlert("波特率无效");
        return;
    }
    cfg.dataBits = std::atoi(dataChoice_->mvalue()->label());
    cfg.stopBits = std::atoi(stopChoice_->mvalue()->label());

    const char* parityStr = parityChoice_->mvalue()->label();
    if (strcmp(parityStr, "奇") == 0)      cfg.parity = 1;
    else if (strcmp(parityStr, "偶") == 0) cfg.parity = 2;
    else                                    cfg.parity = 0;

    if (cfg.portName.empty() || cfg.portName[0] == '(') {
        showMessage("请选择有效串口");
        return;
    }

    if (!serial_.open(cfg)) {
        showAlert("打开失败：" + serial_.lastError());
        return;
    }

    // 打开成功：锁定参数栏，启动接收线程
    openBtn_->label("关闭串口");
    portChoice_->deactivate();
    baudChoice_->deactivate();
    dataChoice_->deactivate();
    stopChoice_->deactivate();
    parityChoice_->deactivate();

    appendRecvText("[已打开 " + cfg.portName + "]\n");
    startReadThread();
}

void SerialAssistant::onSend(Fl_Widget* w, void* data) {
    static_cast<SerialAssistant*>(data)->sendData();
}

void SerialAssistant::sendData() {
    if (!serial_.isOpen()) {
        showMessage("请先打开串口");
        return;
    }

    std::string text = sendInput_->value();
    if (text.empty()) return;

    std::vector<uint8_t> bytes;

    if (hexSend_->value()) {
        // HEX 模式：按用户输入的十六进制解析
        if (!hexStringToBytes(text, bytes)) {
            showAlert("HEX 格式错误");
            return;
        }
    } else {
        // 文本模式：追加 \r\n
        bytes.assign(text.begin(), text.end());
        bytes.push_back('\r');
        bytes.push_back('\n');
    }

    int written = serial_.write(bytes.data(), static_cast<int>(bytes.size()));
    if (written < 0) {
        showAlert("发送失败：" + serial_.lastError());
        return;
    }

    // 回显到接收区
    std::string echo = "[发送] " + bytesToDisplay(bytes.data(), written, hexSend_->value()) + "\n";
    appendRecvText(echo);
}

void SerialAssistant::startReadThread() {
    reading_ = true;
    readThread_ = std::thread(&SerialAssistant::readLoop, this);
}

void SerialAssistant::stopReadThread() {
    reading_ = false;
    if (readThread_.joinable())
        readThread_.join();
}

void SerialAssistant::readLoop() {
    uint8_t buf[4096];

    while (reading_) {
        int n = serial_.read(buf, sizeof(buf));

        if (n > 0) {
            // 通过 Fl::awake 把接收数据投递到主线程
            std::string display = bytesToDisplay(buf, n, hexRecv_->value());
            auto* payload = new std::string(display);
            Fl::awake(appendRecvText_cb, payload);
        } else if (n < 0) {
            if (reading_) {
                auto* payload = new std::string("[读取错误，串口可能已断开]\n");
                Fl::awake(appendRecvText_cb, payload);
            }
            break;
        }
    }
}

void SerialAssistant::appendRecvText_cb(void* data) {
    auto* str = static_cast<std::string*>(data);
    auto* win = dynamic_cast<SerialAssistant*>(Fl::first_window());
    if (win)
        win->appendRecvText(*str);
    delete str;
}

void SerialAssistant::appendRecvText(const std::string& text) {
    recvBuf_->append(text.c_str());

    if (autoScroll_->value()) {
        recvDisplay_->insert_position(recvBuf_->length());
        recvDisplay_->show_insert_position();
    }
}

void SerialAssistant::showMessage(const std::string& msg) {
    // 以主窗口中心为基准，让提示框出现在窗口正中央
    fl_message_position(this);
    fl_message("%s", msg.c_str());
}

void SerialAssistant::showAlert(const std::string& msg) {
    fl_message_position(this);
    fl_alert("%s", msg.c_str());
}

std::string SerialAssistant::bytesToHex(const uint8_t* data, int len) {
    std::ostringstream oss;
    for (int i = 0; i < len; ++i) {
        oss << std::uppercase << std::setfill('0') << std::setw(2)
            << static_cast<int>(data[i]);
        if (i != len - 1) oss << ' ';
    }
    return oss.str();
}

bool SerialAssistant::hexStringToBytes(const std::string& hex,
                                       std::vector<uint8_t>& out) {
    out.clear();
    // 过滤非十六进制字符
    std::string cleaned;
    for (char c : hex) {
        if (std::isxdigit(static_cast<unsigned char>(c)))
            cleaned.push_back(c);
    }
    if (cleaned.size() % 2 != 0) return false;

    for (size_t i = 0; i < cleaned.size(); i += 2) {
        unsigned int byte = 0;
        std::istringstream iss(cleaned.substr(i, 2));
        if (!(iss >> std::hex >> byte)) return false;
        out.push_back(static_cast<uint8_t>(byte));
    }
    return true;
}

std::string SerialAssistant::bytesToDisplay(const uint8_t* data, int len,
                                            bool hexMode) {
    if (hexMode)
        return bytesToHex(data, len);

    // 文本模式：过滤不可打印字符
    std::string result;
    result.reserve(len);
    for (int i = 0; i < len; ++i) {
        uint8_t c = data[i];
        if (c == '\r') {
            // 忽略，避免与 \n 重复换行
        } else if (c == '\n' || (c >= 32 && c < 127)) {
            result.push_back(static_cast<char>(c));
        } else {
            result.push_back('.');
        }
    }
    return result;
}
