#ifndef STM32BOOTLOADER_H
#define STM32BOOTLOADER_H

#include <QObject>
#include <QSerialPort>
#include <QByteArray>
#include <atomic>
#include <QApplication>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QElapsedTimer>
#include <libusb.h>

// DFU协议相关定义
#define DFU_DETACH          0x00
#define DFU_DNLOAD          0x01
#define DFU_UPLOAD          0x02
#define DFU_GETSTATUS       0x03
#define DFU_CLRSTATUS       0x04
#define DFU_GETSTATE        0x05
#define DFU_ABORT           0x06

// DFU状态定义
#define DFU_STATE_APP_IDLE          0x00
#define DFU_STATE_APP_DETACH        0x01
#define DFU_STATE_DFU_IDLE          0x02
#define DFU_STATE_DFU_DNLOAD_SYNC   0x03
#define DFU_STATE_DFU_DNLOAD_BUSY   0x04
#define DFU_STATE_DFU_DNLOAD_IDLE   0x05
#define DFU_STATE_DFU_MANIFEST_SYNC 0x06
#define DFU_STATE_DFU_MANIFEST      0x07
#define DFU_STATE_DFU_MANIFEST_WAIT_RESET 0x08
#define DFU_STATE_DFU_UPLOAD_IDLE   0x09
#define DFU_STATE_DFU_ERROR         0x0A

class STM32Bootloader : public QObject {
    Q_OBJECT
public:
    explicit STM32Bootloader(QObject *parent = nullptr);
    ~STM32Bootloader();


public slots:
    void startUpdate(QSerialPort *port, const QString &filePath);
    void abortOperation();
    bool returnTransferState();

signals:
    void progressUpdated(int value);
    void operationStatus(const QString &message, const QString &color);
    void dataSent(const QByteArray &data);
    void dataReceived(const QByteArray &data, bool isHex);
    void bytesWritten(qint64 bytes);
    void finished();

private:
    // USB相关函数
    bool initializeDFU(libusb_device_handle *handle);
    bool sendDFUCommand(libusb_device_handle *handle, uint8_t request, uint16_t value, QByteArray &data);
    bool usbSendData(libusb_device_handle *handle, const QByteArray &command);
    bool usbSetAddress(libusb_device_handle *handle, quint32 address);
    bool usbSendAbortTransmit(libusb_device_handle *handle);
    bool usbWriteMemory(libusb_device_handle *handle, quint32 address, const QByteArray &data);
    bool usbEraseFlash(libusb_device_handle *handle, int totalPages);
    bool usbJumpToApp(libusb_device_handle *handle, quint32 address);
    bool detectDFUDevices(libusb_context *context, QList<libusb_device*>& dfuDevices);
    QString getDeviceDescription(libusb_device *device);

    // 串口相关函数
    bool enterBootloaderMode(QSerialPort *port);
    bool eraseFlash(QSerialPort &port, int totalPages);
    bool sendWriteMemoryCommand(QSerialPort &port, quint32 address, const QByteArray &data);
    quint8 calculateChecksum(const QByteArray &data);
    void serialBootloaderConfig(QSerialPort &port);
    void serialCmdConfig(QSerialPort &port);
    bool jumpToApp(QSerialPort &port, quint32 address);
    bool resetToApp(QSerialPort &port);
    void printSupportedCommands(QSerialPort &port);
    QString getCommandDescription(uint8_t cmdCode);

    std::atomic<bool> m_abort{false};
    const int BLOCK_SIZE = 1024;
    const int CHUNK_SIZE = 64;
    const int DFU_TIMEOUT = 1000;
    std::atomic<bool> isTransferring{false};
};

#endif // STM32BOOTLOADER_H
