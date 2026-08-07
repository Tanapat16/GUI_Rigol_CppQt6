
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <QBuffer>
#include <QByteArray>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPainter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVBoxLayout>
#include <QVector>

#if HAVE_NIVISA
#include <visa.h>
#endif

// ==============================================================================
// 1. EXCEPTIONS & BASE MODELS
// ==============================================================================

class InstrumentError : public std::runtime_error {
public:
    explicit InstrumentError(const std::string &message)
        : std::runtime_error(message) {}
};

class RigolInstrument {
public:
    static constexpr const char *IDN_QUERY = "*IDN?";
    static constexpr unsigned int DEFAULT_TIMEOUT_MS = 5000;
    static constexpr unsigned int CAPTURE_TIMEOUT_MS = 30000;

    RigolInstrument();
    virtual ~RigolInstrument();

    virtual bool isConnected() const;

    virtual QStringList listResources();
    virtual QString connect(const QString &resourceName, std::optional<unsigned int> timeoutMs = std::nullopt);
    virtual void disconnect();

    virtual void write(const QString &command);
    virtual QString query(const QString &command);
    virtual QString send(const QString &command);

    virtual QByteArray captureScreenshotPng();

    virtual void setChannelStatus(int channel, bool enable);
    virtual bool getChannelStatus(int channel);

    virtual void setVoltScale(int channel, double scaleVal);
    virtual double getVoltScale(int channel);

    virtual void setTimeScale(double scaleVal);
    virtual double getTimeScale();

    virtual void setTriggerSource(const QString &source);
    virtual QString getTriggerSource();

    virtual void setTriggerSweep(const QString &sweep);
    virtual QString getTriggerSweep();

    virtual void setTriggerLevel(double level);
    virtual double getTriggerLevel();

    virtual double getMeasurement(const QString &item, int channel);

    QString resourceName() const { return m_resourceName; }
    QString idn() const { return m_idn; }

protected:
    void requireConnection() const;

    QString m_resourceName;
    QString m_idn;
    bool m_connected = false;

#if HAVE_NIVISA
    ViSession m_rm = VI_NULL;
    ViSession m_inst = VI_NULL;

    QByteArray readBytesExact(unsigned int count);
    void setInstrumentTimeout(unsigned int timeoutMs);
    unsigned int getInstrumentTimeout();
#endif
};

class SimulatedInstrument : public RigolInstrument {
public:
    SimulatedInstrument();

    QStringList listResources() override;
    QString connect(const QString &resourceName, std::optional<unsigned int> timeoutMs = std::nullopt) override;
    void disconnect() override;

    void write(const QString &command) override;
    QString query(const QString &command) override;

    QByteArray captureScreenshotPng() override;

    void setChannelStatus(int channel, bool enable) override;
    bool getChannelStatus(int channel) override;

    void setVoltScale(int channel, double scaleVal) override;
    double getVoltScale(int channel) override;

    void setTimeScale(double scaleVal) override;
    double getTimeScale() override;

    void setTriggerSource(const QString &source) override;
    QString getTriggerSource() override;

    void setTriggerSweep(const QString &sweep) override;
    QString getTriggerSweep() override;

    void setTriggerLevel(double level) override;
    double getTriggerLevel() override;

    double getMeasurement(const QString &item, int channel) override;

private:
    QMap<int, bool> m_channels;
    QMap<int, double> m_voltScales;
    double m_timeScale = 0.0005;
    QString m_trigSource = "CHANnel1";
    QString m_trigSweep = "AUTO";
    double m_trigLevel = 0.0;
};

// ==============================================================================
// 2. WORKER THREADS
// ==============================================================================

class CaptureThread : public QThread {
    Q_OBJECT
public:
    explicit CaptureThread(RigolInstrument *instrument, QObject *parent = nullptr);

signals:
    void captureFinished(QByteArray data);
    void captureFailed(QString message);

protected:
    void run() override;

private:
    RigolInstrument *m_instrument;
};

// ==============================================================================
// 3. UI COMPONENTS
// ==============================================================================

class ConnectionGroup : public QGroupBox {
    Q_OBJECT
public:
    using RefreshFn = std::function<QStringList()>;
    using ConnectFn = std::function<void(const QString &)>;
    using DisconnectFn = std::function<void()>;

    ConnectionGroup(QWidget *parent, RefreshFn onRefresh, ConnectFn onConnect, DisconnectFn onDisconnect);
    void setConnectedState(bool connected, const QString &idn = QString());

private:
    void refresh();
    void connectClicked();
    void disconnectClicked();

    RefreshFn m_onRefresh;
    ConnectFn m_onConnect;
    DisconnectFn m_onDisconnect;

    QComboBox *m_resourceCombo;
    QPushButton *m_btnRefresh;
    QPushButton *m_btnConnect;
    QPushButton *m_btnDisconnect;
    QLabel *m_lblStatus;
};

class CommandGroup : public QGroupBox {
    Q_OBJECT
public:
    using SendFn = std::function<void(const QString &)>;

    CommandGroup(QWidget *parent, SendFn onSend);

private:
    void sendCurrentInput();
    void sendCommand(const QString &command);

    SendFn m_onSend;
    QLineEdit *m_cmdInput;
};

class ChannelControlGroup : public QGroupBox {
    Q_OBJECT
public:
    using ToggleFn = std::function<void(int, bool)>;

    ChannelControlGroup(QWidget *parent, ToggleFn onToggle);
    void updateUiState(int channel, bool isOn);

private:
    void onClick(int channel, bool checked);

    ToggleFn m_onToggle;
    QMap<int, QString> m_chColors;
    QMap<int, QCheckBox *> m_checkboxes;
};

class ScaleControlGroup : public QGroupBox {
    Q_OBJECT
public:
    using VoltChangeFn = std::function<void(int, std::optional<double>, bool)>;
    using TimeChangeFn = std::function<void(double)>;

    ScaleControlGroup(QWidget *parent, VoltChangeFn onVoltChange, TimeChangeFn onTimeChange);

    void setVoltUi(double scaleVal);
    void setTimeUi(double scaleVal);

    QComboBox *channelSelect() const { return m_chSelect; }

private:
    int closestIndex(double val, const QVector<std::pair<double, QString>> &presets) const;
    void onChannelSelect();
    void onComboSelect(const QString &axis);
    void step(const QString &axis, int delta);

    VoltChangeFn m_onVoltChange;
    TimeChangeFn m_onTimeChange;

    QVector<std::pair<double, QString>> m_voltPresets;
    QVector<std::pair<double, QString>> m_timePresets;

    QComboBox *m_chSelect;
    QComboBox *m_voltCombo;
    QComboBox *m_timeCombo;
};

// ==============================================================================
// IMPLEMENTATION SECTION
// ==============================================================================

// --- RigolInstrument ---
RigolInstrument::RigolInstrument() = default;

RigolInstrument::~RigolInstrument() {
    try {
        disconnect();
    } catch (...) {}
}

bool RigolInstrument::isConnected() const { return m_connected; }

void RigolInstrument::requireConnection() const {
    if (!m_connected) {
        throw InstrumentError("ยังไม่ได้เชื่อมต่อกับเครื่องมือ");
    }
}

#if HAVE_NIVISA
QStringList RigolInstrument::listResources() {
    ViSession rm = VI_NULL;
    ViStatus status = viOpenDefaultRM(&rm);
    if (status < VI_SUCCESS) {
        throw InstrumentError("ไม่สามารถค้นหา resource ได้: viOpenDefaultRM ล้มเหลว");
    }

    QStringList results;
    ViFindList findList = VI_NULL;
    ViUInt32 retCount = 0;
    char desc[VI_FIND_BUFLEN];

    status = viFindRsrc(rm, const_cast<char *>("?*::INSTR"), &findList, &retCount, desc);
    if (status >= VI_SUCCESS && retCount > 0) {
        results.append(QString::fromLatin1(desc));
        for (ViUInt32 i = 1; i < retCount; ++i) {
            if (viFindNext(findList, desc) >= VI_SUCCESS) {
                results.append(QString::fromLatin1(desc));
            }
        }
        viClose(findList);
    }
    viClose(rm);
    return results;
}

QString RigolInstrument::connect(const QString &resourceName, std::optional<unsigned int> timeoutMs) {
    ViStatus status = viOpenDefaultRM(&m_rm);
    if (status < VI_SUCCESS) {
        throw InstrumentError("เชื่อมต่อไม่สำเร็จ: viOpenDefaultRM ล้มเหลว");
    }

    QByteArray resNameBytes = resourceName.toLatin1();
    status = viOpen(m_rm, resNameBytes.data(), VI_NULL, VI_NULL, &m_inst);
    if (status < VI_SUCCESS) {
        viClose(m_rm);
        m_rm = VI_NULL;
        throw InstrumentError("เชื่อมต่อไม่สำเร็จ: viOpen ล้มเหลว (" + resourceName.toStdString() + ")");
    }

    setInstrumentTimeout(timeoutMs.value_or(DEFAULT_TIMEOUT_MS));
    viSetAttribute(m_inst, VI_ATTR_TERMCHAR, '\n');
    viSetAttribute(m_inst, VI_ATTR_TERMCHAR_EN, VI_TRUE);

    m_connected = true;
    m_resourceName = resourceName;

    try {
        m_idn = query(IDN_QUERY);
    } catch (const InstrumentError &exc) {
        disconnect();
        throw InstrumentError(std::string("เชื่อมต่อไม่สำเร็จ: ") + exc.what());
    }

    return m_idn;
}

void RigolInstrument::disconnect() {
    if (m_inst != VI_NULL) { viClose(m_inst); m_inst = VI_NULL; }
    if (m_rm != VI_NULL) { viClose(m_rm); m_rm = VI_NULL; }
    m_connected = false;
    m_resourceName.clear();
    m_idn.clear();
}

void RigolInstrument::write(const QString &command) {
    requireConnection();
    QByteArray data = command.toLatin1();
    data.append('\n');
    ViUInt32 written = 0;
    ViStatus status = viWrite(m_inst, reinterpret_cast<ViBuf>(data.data()), static_cast<ViUInt32>(data.size()), &written);
    if (status < VI_SUCCESS) {
        throw InstrumentError("ส่งคำสั่งล้มเหลว (" + command.toStdString() + ")");
    }
}

QString RigolInstrument::query(const QString &command) {
    requireConnection();
    write(command);
    const ViUInt32 bufSize = 65536;
    QByteArray buffer(static_cast<int>(bufSize), Qt::Uninitialized);
    ViUInt32 retCount = 0;
    ViStatus status = viRead(m_inst, reinterpret_cast<ViPBuf>(buffer.data()), bufSize, &retCount);
    if (status < VI_SUCCESS) {
        throw InstrumentError("สอบถามล้มเหลว (" + command.toStdString() + ")");
    }
    buffer.truncate(static_cast<int>(retCount));
    return QString::fromLatin1(buffer).trimmed();
}

QString RigolInstrument::send(const QString &commandIn) {
    QString command = commandIn.trimmed();
    if (command.isEmpty()) return QString();
    if (command.endsWith('?')) return query(command);
    write(command);
    return QString();
}

void RigolInstrument::setInstrumentTimeout(unsigned int timeoutMs) {
    viSetAttribute(m_inst, VI_ATTR_TMO_VALUE, timeoutMs);
}

unsigned int RigolInstrument::getInstrumentTimeout() {
    ViAttrState value = 0;
    viGetAttribute(m_inst, VI_ATTR_TMO_VALUE, &value);
    return static_cast<unsigned int>(value);
}

QByteArray RigolInstrument::readBytesExact(unsigned int count) {
    QByteArray result;
    result.reserve(static_cast<int>(count));
    QByteArray chunk(65536, Qt::Uninitialized);

    while (static_cast<unsigned int>(result.size()) < count) {
        ViUInt32 want = std::min<ViUInt32>(65536, count - static_cast<unsigned int>(result.size()));
        ViUInt32 got = 0;
        ViStatus status = viRead(m_inst, reinterpret_cast<ViPBuf>(chunk.data()), want, &got);
        if (status < VI_SUCCESS) {
            throw InstrumentError("อ่านข้อมูลจากเครื่องมือล้มเหลว");
        }
        result.append(chunk.constData(), static_cast<int>(got));
        if (got == 0) break;
    }
    return result;
}

QByteArray RigolInstrument::captureScreenshotPng() {
    requireConnection();
    unsigned int originalTimeout = getInstrumentTimeout();
    try {
        setInstrumentTimeout(CAPTURE_TIMEOUT_MS);
        viSetAttribute(m_inst, VI_ATTR_TERMCHAR_EN, VI_FALSE);

        write(":DISP:DATA? ON,PNG");

        QByteArray header = readBytesExact(2);
        if (header.isEmpty() || header.at(0) != '#') {
            throw InstrumentError("Header ไม่ถูกต้อง");
        }
        int numDigits = header.at(1) - '0';
        if (numDigits <= 0) {
            throw InstrumentError("Indefinite length block ไม่รองรับ");
        }

        QByteArray lengthBytes = readBytesExact(static_cast<unsigned int>(numDigits));
        bool ok = false;
        unsigned int length = lengthBytes.toUInt(&ok);
        if (!ok) {
            throw InstrumentError("อ่านความยาวข้อมูลภาพล้มเหลว");
        }

        QByteArray imageData = readBytesExact(length);

        try { readBytesExact(1); } catch (...) {}

        viSetAttribute(m_inst, VI_ATTR_TERMCHAR_EN, VI_TRUE);
        setInstrumentTimeout(originalTimeout);
        return imageData;
    } catch (...) {
        viSetAttribute(m_inst, VI_ATTR_TERMCHAR_EN, VI_TRUE);
        setInstrumentTimeout(originalTimeout);
        throw;
    }
}
#else
QStringList RigolInstrument::listResources() { return {}; }
QString RigolInstrument::connect(const QString &, std::optional<unsigned int>) {
    throw InstrumentError("ยังไม่ได้ติดตั้ง NI-VISA (build ด้วย USE_NIVISA=ON และระบุ path ให้ถูกต้อง)");
}
void RigolInstrument::disconnect() { m_connected = false; m_resourceName.clear(); m_idn.clear(); }
void RigolInstrument::write(const QString &) { requireConnection(); }
QString RigolInstrument::query(const QString &) { requireConnection(); return QString(); }
QString RigolInstrument::send(const QString &) { requireConnection(); return QString(); }
QByteArray RigolInstrument::captureScreenshotPng() { requireConnection(); return {}; }
#endif

void RigolInstrument::setChannelStatus(int channel, bool enable) {
    write(QString(":CHANnel%1:DISPlay %2").arg(channel).arg(enable ? "ON" : "OFF"));
}

bool RigolInstrument::getChannelStatus(int channel) {
    return query(QString(":CHANnel%1:DISPlay?").arg(channel)).trimmed() == "1";
}

void RigolInstrument::setVoltScale(int channel, double scaleVal) {
    write(QString(":CHANnel%1:SCALe %2").arg(channel).arg(scaleVal));
}

double RigolInstrument::getVoltScale(int channel) {
    return query(QString(":CHANnel%1:SCALe?").arg(channel)).toDouble();
}

void RigolInstrument::setTimeScale(double scaleVal) {
    write(QString(":TIMebase:MAIN:SCALe %1").arg(scaleVal));
}

double RigolInstrument::getTimeScale() {
    return query(":TIMebase:MAIN:SCALe?").toDouble();
}

void RigolInstrument::setTriggerSource(const QString &source) {
    write(QString(":TRIGger:EDGe:SOURce %1").arg(source));
}

QString RigolInstrument::getTriggerSource() {
    return query(":TRIGger:EDGe:SOURce?");
}

void RigolInstrument::setTriggerSweep(const QString &sweep) {
    write(QString(":TRIGger:SWEep %1").arg(sweep));
}

QString RigolInstrument::getTriggerSweep() {
    return query(":TRIGger:SWEep?");
}

void RigolInstrument::setTriggerLevel(double level) {
    write(QString(":TRIGger:EDGe:LEVel %1").arg(level));
}

double RigolInstrument::getTriggerLevel() {
    return query(":TRIGger:EDGe:LEVel?").toDouble();
}

double RigolInstrument::getMeasurement(const QString &item, int channel) {
    return query(QString(":MEASure:ITEM? %1,CHANnel%2").arg(item).arg(channel)).toDouble();
}

// --- SimulatedInstrument ---
SimulatedInstrument::SimulatedInstrument() {
    m_channels = {{1, true}, {2, false}, {3, false}, {4, false}};
    m_voltScales = {{1, 1.0}, {2, 1.0}, {3, 1.0}, {4, 1.0}};
}

QStringList SimulatedInstrument::listResources() { return {"SIM::MSO1104::INSTR"}; }

QString SimulatedInstrument::connect(const QString &resourceName, std::optional<unsigned int>) {
    QThread::msleep(300);
    m_resourceName = resourceName;
    m_idn = "RIGOL TECHNOLOGIES,MSO1104,SIMULATED,00.01.03 (Qt6 C++ Simulation)";
    m_connected = true;
    return m_idn;
}

void SimulatedInstrument::disconnect() {
    m_connected = false;
    m_resourceName.clear();
    m_idn.clear();
}

void SimulatedInstrument::write(const QString &) { requireConnection(); }

QString SimulatedInstrument::query(const QString &command) {
    requireConnection();
    if (command == IDN_QUERY) return m_idn;
    return QString("<simulated response for '%1'>").arg(command);
}

QByteArray SimulatedInstrument::captureScreenshotPng() {
    requireConnection();
    QThread::msleep(500);
    QImage img(400, 240, QImage::Format_RGB32);
    img.fill(QColor(20, 20, 30));
    QPainter painter(&img);
    painter.setPen(QColor(0, 255, 128));
    painter.drawText(img.rect(), Qt::AlignCenter, "SIMULATED SCREENSHOT");
    painter.end();

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    return bytes;
}

void SimulatedInstrument::setChannelStatus(int channel, bool enable) { requireConnection(); m_channels[channel] = enable; }
bool SimulatedInstrument::getChannelStatus(int channel) { requireConnection(); return m_channels.value(channel, false); }
void SimulatedInstrument::setVoltScale(int channel, double scaleVal) { requireConnection(); m_voltScales[channel] = scaleVal; }
double SimulatedInstrument::getVoltScale(int channel) { requireConnection(); return m_voltScales.value(channel, 1.0); }
void SimulatedInstrument::setTimeScale(double scaleVal) { requireConnection(); m_timeScale = scaleVal; }
double SimulatedInstrument::getTimeScale() { requireConnection(); return m_timeScale; }
void SimulatedInstrument::setTriggerSource(const QString &source) { requireConnection(); m_trigSource = source; }
QString SimulatedInstrument::getTriggerSource() { requireConnection(); return m_trigSource; }
void SimulatedInstrument::setTriggerSweep(const QString &sweep) { requireConnection(); m_trigSweep = sweep; }
QString SimulatedInstrument::getTriggerSweep() { requireConnection(); return m_trigSweep; }
void SimulatedInstrument::setTriggerLevel(double level) { requireConnection(); m_trigLevel = level; }
double SimulatedInstrument::getTriggerLevel() { requireConnection(); return m_trigLevel; }

double SimulatedInstrument::getMeasurement(const QString &item, int) {
    requireConnection();
    auto *rng = QRandomGenerator::global();
    if (item == "VPP") return qRound((2.5 + (rng->generateDouble() * 0.1 - 0.05)) * 1000.0) / 1000.0;
    if (item == "VMAX") return qRound((1.25 + (rng->generateDouble() * 0.04 - 0.02)) * 1000.0) / 1000.0;
    if (item == "VMIN") return qRound((-1.25 + (rng->generateDouble() * 0.04 - 0.02)) * 1000.0) / 1000.0;
    if (item == "FREQuency") return qRound((1000.0 + (rng->generateDouble() * 10.0 - 5.0)) * 10.0) / 10.0;
    if (item == "PERiod") return (0.001 + (rng->generateDouble() * 0.00002 - 0.00001));
    return 0.0;
}

// --- CaptureThread ---
CaptureThread::CaptureThread(RigolInstrument *instrument, QObject *parent)
    : QThread(parent), m_instrument(instrument) {}

void CaptureThread::run() {
    try {
        QByteArray data = m_instrument->captureScreenshotPng();
        emit captureFinished(data);
    } catch (const std::exception &exc) {
        emit captureFailed(QString::fromUtf8(exc.what()));
    }
}

// --- ConnectionGroup ---
ConnectionGroup::ConnectionGroup(QWidget *parent, RefreshFn onRefresh, ConnectFn onConnect, DisconnectFn onDisconnect)
    : QGroupBox("Instrument Connection", parent),
      m_onRefresh(std::move(onRefresh)),
      m_onConnect(std::move(onConnect)),
      m_onDisconnect(std::move(onDisconnect)) {

    auto *layout = new QGridLayout(this);
    layout->addWidget(new QLabel("VISA Resource:", this), 0, 0);

    m_resourceCombo = new QComboBox(this);
    m_resourceCombo->setEditable(true);
    layout->addWidget(m_resourceCombo, 0, 1);

    m_btnRefresh = new QPushButton("Refresh", this);
    connect(m_btnRefresh, &QPushButton::clicked, this, &ConnectionGroup::refresh);
    layout->addWidget(m_btnRefresh, 0, 2);

    m_btnConnect = new QPushButton("Connect", this);
    connect(m_btnConnect, &QPushButton::clicked, this, &ConnectionGroup::connectClicked);
    layout->addWidget(m_btnConnect, 0, 3);

    m_btnDisconnect = new QPushButton("Disconnect", this);
    m_btnDisconnect->setEnabled(false);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &ConnectionGroup::disconnectClicked);
    layout->addWidget(m_btnDisconnect, 0, 4);

    m_lblStatus = new QLabel("Status: Disconnected", this);
    m_lblStatus->setStyleSheet("color: red; font-weight: bold;");
    layout->addWidget(m_lblStatus, 1, 0, 1, 5);
}

void ConnectionGroup::refresh() {
    m_resourceCombo->clear();
    m_resourceCombo->addItems(m_onRefresh());
}

void ConnectionGroup::connectClicked() { m_onConnect(m_resourceCombo->currentText()); }
void ConnectionGroup::disconnectClicked() { m_onDisconnect(); }

void ConnectionGroup::setConnectedState(bool connected, const QString &idn) {
    if (connected) {
        m_lblStatus->setText(QString("Status: Connected -> %1").arg(idn));
        m_lblStatus->setStyleSheet("color: green; font-weight: bold;");
        m_btnConnect->setEnabled(false);
        m_btnDisconnect->setEnabled(true);
        m_resourceCombo->setEnabled(false);
    } else {
        m_lblStatus->setText("Status: Disconnected");
        m_lblStatus->setStyleSheet("color: red; font-weight: bold;");
        m_btnConnect->setEnabled(true);
        m_btnDisconnect->setEnabled(false);
        m_resourceCombo->setEnabled(true);
    }
}

// --- CommandGroup ---
CommandGroup::CommandGroup(QWidget *parent, SendFn onSend)
    : QGroupBox("SCPI Command", parent), m_onSend(std::move(onSend)) {

    auto *layout = new QVBoxLayout(this);
    auto *inputLayout = new QHBoxLayout();

    m_cmdInput = new QLineEdit("*IDN?", this);
    connect(m_cmdInput, &QLineEdit::returnPressed, this, &CommandGroup::sendCurrentInput);
    inputLayout->addWidget(m_cmdInput);

    auto *btnSend = new QPushButton("Send", this);
    connect(btnSend, &QPushButton::clicked, this, &CommandGroup::sendCurrentInput);
    inputLayout->addWidget(btnSend);

    layout->addLayout(inputLayout);

    auto *shortcutLayout = new QHBoxLayout();
    const std::vector<std::pair<QString, QString>> shortcuts = {
        {"*IDN?", "*IDN?"}, {"Run", ":RUN"}, {"Stop", ":STOP"},
        {"Auto Scale", ":AUToscale"}, {"Single", ":SINGle"}
    };
    for (const auto &pair : shortcuts) {
        auto *btn = new QPushButton(pair.first, this);
        QString cmd = pair.second;
        connect(btn, &QPushButton::clicked, this, [this, cmd]() { sendCommand(cmd); });
        shortcutLayout->addWidget(btn);
    }
    layout->addLayout(shortcutLayout);
}

void CommandGroup::sendCurrentInput() { sendCommand(m_cmdInput->text()); }
void CommandGroup::sendCommand(const QString &command) { m_onSend(command); }

// --- ChannelControlGroup ---
ChannelControlGroup::ChannelControlGroup(QWidget *parent, ToggleFn onToggle)
    : QGroupBox("Channel View Control", parent), m_onToggle(std::move(onToggle)) {

    auto *layout = new QVBoxLayout(this);
    m_chColors = {{1, "#D4AF37"}, {2, "#00A8E8"}, {3, "#DE3163"}, {4, "#2E8B57"}};

    for (int ch = 1; ch <= 4; ++ch) {
        auto *hLayout = new QHBoxLayout();
        auto *lbl = new QLabel(QString("CH %1").arg(ch), this);
        lbl->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 12px;").arg(m_chColors[ch]));
        hLayout->addWidget(lbl);

        auto *cb = new QCheckBox("OFF", this);
        connect(cb, &QCheckBox::clicked, this, [this, ch](bool checked) { onClick(ch, checked); });
        hLayout->addWidget(cb);

        m_checkboxes[ch] = cb;
        layout->addLayout(hLayout);
    }
}

void ChannelControlGroup::onClick(int channel, bool checked) {
    m_checkboxes[channel]->setText(checked ? "ON" : "OFF");
    m_onToggle(channel, checked);
}

void ChannelControlGroup::updateUiState(int channel, bool isOn) {
    QCheckBox *cb = m_checkboxes[channel];
    cb->blockSignals(true);
    cb->setChecked(isOn);
    cb->setText(isOn ? "ON" : "OFF");
    cb->blockSignals(false);
}

// --- ScaleControlGroup ---
ScaleControlGroup::ScaleControlGroup(QWidget *parent, VoltChangeFn onVoltChange, TimeChangeFn onTimeChange)
    : QGroupBox("Scale Control", parent),
      m_onVoltChange(std::move(onVoltChange)),
      m_onTimeChange(std::move(onTimeChange)) {

    m_voltPresets = {
        {0.01, "10 mV"}, {0.02, "20 mV"}, {0.05, "50 mV"}, {0.1, "100 mV"},
        {0.2, "200 mV"}, {0.5, "500 mV"}, {1.0, "1 V"},    {2.0, "2 V"},
        {5.0, "5 V"},   {10.0, "10 V"}
    };

    m_timePresets = {
        {1e-9, "1 ns"},   {2e-9, "2 ns"},   {5e-9, "5 ns"},   {10e-9, "10 ns"},
        {20e-9, "20 ns"}, {50e-9, "50 ns"}, {100e-9, "100 ns"}, {200e-9, "200 ns"},
        {500e-9, "500 ns"}, {1e-6, "1 us"}, {2e-6, "2 us"},   {5e-6, "5 us"},
        {10e-6, "10 us"}, {20e-6, "20 us"}, {50e-6, "50 us"}, {100e-6, "100 us"},
        {200e-6, "200 us"}, {500e-6, "500 us"}, {1e-3, "1 ms"}, {2e-3, "2 ms"},
        {5e-3, "5 ms"},   {10e-3, "10 ms"}, {20e-3, "20 ms"}, {50e-3, "50 ms"},
        {100e-3, "100 ms"}, {200e-3, "200 ms"}, {500e-3, "500 ms"}, {1.0, "1 s"}
    };

    auto *layout = new QGridLayout(this);

    layout->addWidget(new QLabel("Channel:", this), 0, 0);
    m_chSelect = new QComboBox(this);
    m_chSelect->addItems({"CH1", "CH2", "CH3", "CH4"});
    connect(m_chSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScaleControlGroup::onChannelSelect);
    layout->addWidget(m_chSelect, 0, 1, 1, 3);

    layout->addWidget(new QLabel("Volt/Div:", this), 1, 0);
    m_voltCombo = new QComboBox(this);
    for (const auto &p : m_voltPresets) m_voltCombo->addItem(p.second, p.first);
    connect(m_voltCombo, &QComboBox::textActivated, this, [this]() { onComboSelect("volt"); });
    layout->addWidget(m_voltCombo, 1, 1);

    auto *btnVoltDec = new QPushButton("-", this);
    connect(btnVoltDec, &QPushButton::clicked, this, [this]() { step("volt", -1); });
    layout->addWidget(btnVoltDec, 1, 2);

    auto *btnVoltInc = new QPushButton("+", this);
    connect(btnVoltInc, &QPushButton::clicked, this, [this]() { step("volt", 1); });
    layout->addWidget(btnVoltInc, 1, 3);

    layout->addWidget(new QLabel("Time/Div:", this), 2, 0);
    m_timeCombo = new QComboBox(this);
    for (const auto &p : m_timePresets) m_timeCombo->addItem(p.second, p.first);
    connect(m_timeCombo, &QComboBox::textActivated, this, [this]() { onComboSelect("time"); });
    layout->addWidget(m_timeCombo, 2, 1);

    auto *btnTimeDec = new QPushButton("-", this);
    connect(btnTimeDec, &QPushButton::clicked, this, [this]() { step("time", -1); });
    layout->addWidget(btnTimeDec, 2, 2);

    auto *btnTimeInc = new QPushButton("+", this);
    connect(btnTimeInc, &QPushButton::clicked, this, [this]() { step("time", 1); });
    layout->addWidget(btnTimeInc, 2, 3);
}

int ScaleControlGroup::closestIndex(double val, const QVector<std::pair<double, QString>> &presets) const {
    int bestIdx = 0;
    double minDiff = std::abs(val - presets[0].first);
    for (int i = 1; i < presets.size(); ++i) {
        double diff = std::abs(val - presets[i].first);
        if (diff < minDiff) {
            minDiff = diff;
            bestIdx = i;
        }
    }
    return bestIdx;
}

void ScaleControlGroup::setVoltUi(double scaleVal) {
    int idx = closestIndex(scaleVal, m_voltPresets);
    m_voltCombo->blockSignals(true);
    m_voltCombo->setCurrentIndex(idx);
    m_voltCombo->blockSignals(false);
}

void ScaleControlGroup::setTimeUi(double scaleVal) {
    int idx = closestIndex(scaleVal, m_timePresets);
    m_timeCombo->blockSignals(true);
    m_timeCombo->setCurrentIndex(idx);
    m_timeCombo->blockSignals(false);
}

void ScaleControlGroup::onChannelSelect() {
    int ch = m_chSelect->currentIndex() + 1;
    m_onVoltChange(ch, std::nullopt, true);
}

void ScaleControlGroup::onComboSelect(const QString &axis) {
    if (axis == "volt") {
        int ch = m_chSelect->currentIndex() + 1;
        double val = m_voltCombo->currentData().toDouble();
        m_onVoltChange(ch, val, false);
    } else if (axis == "time") {
        double val = m_timeCombo->currentData().toDouble();
        m_onTimeChange(val);
    }
}

void ScaleControlGroup::step(const QString &axis, int delta) {
    QComboBox *combo = (axis == "volt") ? m_voltCombo : m_timeCombo;
    int newIndex = qBound(0, combo->currentIndex() + delta, combo->count() - 1);
    if (newIndex != combo->currentIndex()) {
        combo->setCurrentIndex(newIndex);
        onComboSelect(axis);
    }
}