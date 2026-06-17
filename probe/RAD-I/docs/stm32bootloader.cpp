#include "stm32bootloader.h"
#include <QtEndian>
#include <QThread>
#include <QFile>
#include <QElapsedTimer>
#include <QInputDialog>

STM32Bootloader::STM32Bootloader(QObject *parent) : QObject(parent) {}
STM32Bootloader::~STM32Bootloader() {}

bool STM32Bootloader::returnTransferState() {
    return isTransferring.load();
}

bool STM32Bootloader::initializeDFU(libusb_device_handle* handle) {
    // 1. 清除状态
    int ret = libusb_control_transfer(handle,
                                      LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                      DFU_CLRSTATUS,
                                      0, 0,
                                      nullptr, 0,
                                      DFU_TIMEOUT);
    if (ret < 0) {
        emit operationStatus(QString("清除状态失败: %1").arg(libusb_error_name(ret)), "orange");
        // 继续尝试，不是致命错误
    }

    // 2. 获取状态
    uint8_t status[6] = {0};
    ret = libusb_control_transfer(handle,
                                  LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                  DFU_GETSTATUS,
                                  0, 0,
                                  status, sizeof(status),
                                  DFU_TIMEOUT);

    if (ret < 0) {
        emit operationStatus(QString("获取状态失败: %1").arg(libusb_error_name(ret)), "red");
        return false;
    }

    // 3. 如果已经是IDLE状态，直接返回
    if (status[4] == DFU_STATE_DFU_IDLE) {
        return true;
    }

    // 4. 如果是错误状态，先清除
    if (status[4] == DFU_STATE_DFU_ERROR) {
        libusb_control_transfer(handle,
                                LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                DFU_CLRSTATUS,
                                0, 0,
                                nullptr, 0,
                                DFU_TIMEOUT);
        QThread::msleep(100);
    }

    // 5. 发送ABORT命令
    ret = libusb_control_transfer(handle,
                                  LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                  DFU_ABORT,
                                  0, 0,
                                  nullptr, 0,
                                  DFU_TIMEOUT);
    if (ret < 0) {
        emit operationStatus(QString("发送ABORT命令失败: %1").arg(libusb_error_name(ret)), "orange");
    }
    QThread::msleep(100);

    // 6. 再次检查状态
    ret = libusb_control_transfer(handle,
                                  LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                  DFU_GETSTATUS,
                                  0, 0,
                                  status, sizeof(status),
                                  DFU_TIMEOUT);

    if (ret < 0) {
        emit operationStatus(QString("最终状态检查失败: %1").arg(libusb_error_name(ret)), "red");
        return false;
    }

    if (status[4] != DFU_STATE_DFU_IDLE) {
        emit operationStatus(QString("无法进入DFU IDLE状态(当前状态:0x%1)").arg(status[4],2,16,QChar('0')), "red");
        return false;
    }

    return true;
}

QString STM32Bootloader::getDeviceDescription(libusb_device *device) {
    libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(device, &desc) != 0) {
        return "Unknown Device";
    }

    char manufacturer[256] = {0};
    char product[256] = {0};
    libusb_device_handle *handle = nullptr;

    if (libusb_open(device, &handle) == 0) {
        libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, (unsigned char*)manufacturer, sizeof(manufacturer));
        libusb_get_string_descriptor_ascii(handle, desc.iProduct, (unsigned char*)product, sizeof(product));
        libusb_close(handle);
    }

    return QString("%1 %2 (VID: 0x%3 PID: 0x%4)")
        .arg(manufacturer)
        .arg(product)
        .arg(desc.idVendor, 4, 16, QChar('0'))
        .arg(desc.idProduct, 4, 16, QChar('0'));
}

bool STM32Bootloader::detectDFUDevices(libusb_context *context, QList<libusb_device*>& dfuDevices) {
    libusb_device **list;
    ssize_t cnt = libusb_get_device_list(context, &list);

    for (ssize_t i = 0; i < cnt; i++) {
        libusb_device *device = list[i];
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(device, &desc) == 0) {
            // 标准DFU模式设备
            if ((desc.idVendor == 0x0483 && desc.idProduct == 0xDF11) ||
                (desc.idVendor == 0x0483 && desc.idProduct == 0xDF12)) {
                dfuDevices.append(device);
                libusb_ref_device(device);
                emit operationStatus(QString("检测到DFU设备: %1").arg(getDeviceDescription(device)), "blue");
            }
        }
    }

    libusb_free_device_list(list, 1);
    return !dfuDevices.isEmpty();
}

void STM32Bootloader::startUpdate(QSerialPort *port, const QString &filePath) {
    if (isTransferring.load()) {
        emit operationStatus("错误: 已有文件正在传输!", "red");
        emit finished();
        return;
    }





    if (!port)
    {
        emit operationStatus("错误: 串口未连接!", "red");
        return;
    }

    serialBootloaderConfig(*port);
    port->clear();

    emit operationStatus("尝试通过硬件控制进入BootLoader模式...", "blue");
    port->setRequestToSend(true);
    port->setDataTerminalReady(false);
    QThread::msleep(100);
    port->setRequestToSend(false);
    QThread::msleep(500);





    isTransferring.store(true);
    libusb_device_handle *usbHandle = nullptr;
    bool useUSB = false;
    libusb_context *context = nullptr;

    // 尝试使用USB模式
    if (libusb_init(&context) == 0) {
        libusb_set_option(context, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
        emit operationStatus("正在扫描USB设备...", "blue");

        QList<libusb_device*> dfuDevices;
        if (detectDFUDevices(context, dfuDevices)) {
            libusb_device *selectedDevice = dfuDevices.first();

            if (dfuDevices.size() > 1) {
                QStringList deviceNames;
                for (libusb_device* device : dfuDevices) {
                    deviceNames << getDeviceDescription(device);
                }

                bool ok;
                QString selected = QInputDialog::getItem(nullptr, "选择USB设备",
                                                         "检测到多个USB设备，请选择:",
                                                         deviceNames, 0, false, &ok);
                if (ok && !selected.isEmpty()) {
                    int index = deviceNames.indexOf(selected);
                    if (index >= 0 && index < dfuDevices.size()) {
                        selectedDevice = dfuDevices[index];
                        emit operationStatus(QString("已选择USB设备: %1").arg(selected), "green");
                    }
                }
            }

            // 打开选定的USB设备
            int ret = libusb_open(selectedDevice, &usbHandle);
            if (ret == 0) {
                if (libusb_claim_interface(usbHandle, 0) == 0) {
                    useUSB = true;
                    emit operationStatus("USB设备已连接并准备就绪", "green");
                } else {
                    emit operationStatus("USB设备接口无法被占用", "red");
                    libusb_close(usbHandle);
                    usbHandle = nullptr;
                }
            } else {
                emit operationStatus(QString("无法打开USB设备: %1").arg(libusb_error_name(ret)), "red");
            }
        } else {
            emit operationStatus("未检测到兼容的DFU设备", "orange");
        }
    } else {
        emit operationStatus("无法初始化libusb", "red");
    }

    if (!useUSB) {
        if (!port || !port->isOpen()) {
            emit operationStatus("错误: 未找到USB设备且串口未打开!", "red");
            isTransferring.store(false);
            if (context) libusb_exit(context);
            emit finished();
            return;
        }
        emit operationStatus("将使用串口模式进行固件更新", "blue");
    }

    try {
        emit operationStatus("DFU设备状态初始化...", "blue");
        emit progressUpdated(0);

        // 1. 进入Bootloader模式
        if (useUSB) {
            // 新增DFU初始化检查
            if (!initializeDFU(usbHandle)) {
                emit operationStatus("USB设备初始化失败", "red");
                isTransferring.store(false);
                if (usbHandle) {
                    libusb_release_interface(usbHandle, 0);
                    libusb_close(usbHandle);
                }
                if (context) libusb_exit(context);
                emit finished();
                return;
            }
            emit operationStatus("USB DFU模式已激活", "green");
        } else if (!enterBootloaderMode(port)) {
            throw std::runtime_error("进入Bootloader模式失败");
        }

        // 2. 准备文件
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("无法打开文件");
        }

        qint64 totalSize = file.size();
        int totalPages = qCeil(totalSize / (128.0 * 1024));
        emit operationStatus(QString("文件总大小: %1 字节，需擦除%2页").arg(totalSize).arg(totalPages), "blue");

        // 3. 擦除Flash
        if (useUSB) {
            if (!usbEraseFlash(usbHandle, totalPages)) {
                file.close();
                throw std::runtime_error("FLASH擦除失败");
            }
            else
                emit operationStatus("FLASH擦除成功！", "green");
        }
        else if (!eraseFlash(*port, totalPages)) {
            file.close();
            throw std::runtime_error("串口擦除失败");
        }

        // 3.5 设置程序起始地址
        // if(useUSB)
        // {
        //     if(!usbSetAddress(usbHandle, 0x08000000)){
        //         file.close();
        //         throw std::runtime_error("USB程序烧写地址设置失败");
        //     }
        // }

        // 4. 写入数据
        emit operationStatus("开始更新固件...", "blue");
        qint64 writtenBytes = 0;
        while (!file.atEnd() && !m_abort.load()) {
            QByteArray data = file.read(BLOCK_SIZE);

            if (useUSB) {
                if(!usbSetAddress(usbHandle, 0x08000000 + writtenBytes)){
                    file.close();
                    throw std::runtime_error("USB程序烧写地址设置失败");
                }

                if (!usbWriteMemory(usbHandle, 0x08000000 + writtenBytes, data)) {
                    file.close();
                    throw std::runtime_error("USB写入失败");
                }
            } else {
                if (!sendWriteMemoryCommand(*port, 0x08000000 + writtenBytes, data)) {
                    file.close();
                    throw std::runtime_error("串口写入失败");
                }
            }

            writtenBytes += data.size();
            int progress = (writtenBytes * 100 / totalSize);
            emit progressUpdated(progress);
            emit bytesWritten(writtenBytes);

            if (writtenBytes % 1024 == 0) {
                QCoreApplication::processEvents();
            }
        }

        file.close();

        // 5. 完成处理
        if (m_abort.load()) {
            emit operationStatus("更新已取消", "orange");
        } else {
            emit operationStatus("固件更新完成！", "green");
            emit progressUpdated(100);
            emit operationStatus("尝试跳转至APP...", "blue");
            if (useUSB) {
                if (!usbJumpToApp(usbHandle, 0x08000000)) {
                    emit operationStatus("USB跳转APP失败", "orange");
                }
                else
                    emit operationStatus("跳转APP成功！", "green");
            } else {
                if (!jumpToApp(*port, 0x08000000)) {
                    resetToApp(*port);
                }
            }
        }

    } catch (const std::exception &e) {
        emit operationStatus(QString("错误: %1").arg(e.what()), "red");
    }

    // 清理USB资源
    if (usbHandle) {
        libusb_release_interface(usbHandle, 0);
        libusb_close(usbHandle);
    }
    if (context) {
        libusb_exit(context);
    }

    m_abort.store(false);
    isTransferring.store(false);
    emit finished();
}

// USB相关函数实现
bool STM32Bootloader::usbSendData(libusb_device_handle *handle,
                                     const QByteArray &command) {

    int ret;
    // uint8_t bState;
    uint8_t status[6] = {0};
    QElapsedTimer timer;

    if (!handle) return false;

    // 发送DNLOAD命令
    ret = libusb_control_transfer(handle,
                                      LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                      DFU_DNLOAD,
                                      2,  // wValue (block number) 禁止计算地址偏移量，只依靠DFU_SET_ADDRESS设置的地址
                                      0,  // interface
                                      (unsigned char*)command.data(),
                                      command.size(),
                                      DFU_TIMEOUT);

    if (ret < 0) {
        emit operationStatus(QString("USB数据传输失败(%1): %2").arg(ret).arg(libusb_error_name(ret)), "red");
        return false;
    }

    // QString statusMsg = QString("DFU状态: %1, 超时: %2ms")
    //                         .arg(bState, 2, 16, QChar('0'));
    // emit operationStatus(statusMsg, "blue");

    timer.start();
    while (timer.elapsed() < 5000) { // 5秒超时
        ret = libusb_control_transfer(handle,
                                      LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                      DFU_GETSTATUS,
                                      0, 0,
                                      status, sizeof(status),
                                      DFU_TIMEOUT);

        if (ret < 0) {
            emit operationStatus(QString("获取状态失败: %1").arg(libusb_error_name(ret)), "red");
            return false;
        }

        // 解析状态
        uint8_t bStatus = status[0];
        // uint32_t bwPollTimeout = (status[3] << 16) | (status[2] << 8) | status[1];
        uint8_t bState = status[4];

        // QString statusMsg = QString("DFU状态: %1, 超时: %2ms")
        //                         .arg(bState, 2, 16, QChar('0'))
        //                         .arg(bwPollTimeout);
        // emit operationStatus(statusMsg, "blue");

        switch (bState) {
        case DFU_STATE_DFU_DNLOAD_BUSY:
            // 设备正忙，等待指定的时间
            // emit operationStatus("烧写程序中...", "green");
            QThread::usleep(1);
            continue;
        case DFU_STATE_DFU_ERROR:
            emit operationStatus(QString("DFU错误状态: 0x%1").arg(bStatus, 2, 16, QChar('0')), "red");
            // 清除错误状态
            libusb_control_transfer(handle,
                                    LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                    DFU_CLRSTATUS,
                                    0, 0,
                                    nullptr, 0,
                                    DFU_TIMEOUT);
            return false;
        case DFU_STATE_DFU_IDLE:
            // emit operationStatus("烧写地址设置成功！", "green");
            return true;
        case DFU_STATE_DFU_DNLOAD_IDLE:
            // emit operationStatus("烧写地址设置成功！", "green");
            return true;
        case DFU_STATE_DFU_UPLOAD_IDLE:
            // emit operationStatus("烧写地址设置成功！", "green");
            return true;
        default:
            QThread::usleep(10);
            break;
        }
    }

    if(timer.elapsed() >= 5000){
        emit operationStatus("程序烧写超时", "red");
        return false;
    }

    return true;
}

bool STM32Bootloader::usbSetAddress(libusb_device_handle *handle, quint32 address) {
    int ret;
    // uint8_t bState;
    uint8_t status[6] = {0};
    unsigned char SetAddressCmd[5] = {0};

    QElapsedTimer timer;

    // emit operationStatus(QString("设置程序烧写地址：%1...").arg(address,8,16,QChar('0')), "blue");

    SetAddressCmd[0] = 0x21;
    SetAddressCmd[1] = address & 0xff;
    SetAddressCmd[2] = (address >> 8) & 0xff;
    SetAddressCmd[3] = (address >> 16) & 0xff;
    SetAddressCmd[4] = (address >> 24) & 0xff;

    ret = libusb_control_transfer(handle,
                                  LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                  DFU_DNLOAD,
                                  0,  // wValue (block number)
                                  0,  // interface
                                  SetAddressCmd,
                                  5,
                                  DFU_TIMEOUT);
    if (ret < 0) {
        emit operationStatus(QString("发送设置地址命令失败: %1").arg(libusb_error_name(ret)), "red");
        return false;
    }


    timer.start();
    while (timer.elapsed() < 30000) { // 30秒超时
        ret = libusb_control_transfer(handle,
                                      LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                      DFU_GETSTATUS,
                                      0, 0,
                                      status, sizeof(status),
                                      DFU_TIMEOUT);

        if (ret < 0) {
            emit operationStatus(QString("获取状态失败: %1").arg(libusb_error_name(ret)), "red");
            return false;
        }

        // 解析状态
        uint8_t bStatus = status[0];
        // uint32_t bwPollTimeout = (status[3] << 16) | (status[2] << 8) | status[1];
        uint8_t bState = status[4];

        // QString statusMsg = QString("DFU状态: %1, 超时: %2ms")
        //                         .arg(bState, 2, 16, QChar('0'))
        //                         .arg(bwPollTimeout);
        // emit operationStatus(statusMsg, "blue");

        switch (bState) {
            case DFU_STATE_DFU_DNLOAD_BUSY:
                // 设备正忙，等待指定的时间
                // emit operationStatus("烧写地址设置中...", "green");
                QThread::usleep(1);
                continue;
            case DFU_STATE_DFU_ERROR:
                emit operationStatus(QString("DFU错误状态: 0x%1").arg(bStatus, 2, 16, QChar('0')), "red");
                // 清除错误状态
                libusb_control_transfer(handle,
                                        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                        DFU_CLRSTATUS,
                                        0, 0,
                                        nullptr, 0,
                                        DFU_TIMEOUT);
                return false;
            case DFU_STATE_DFU_IDLE:
                // emit operationStatus("设置地址成功！", "green");
                return true;
            case DFU_STATE_DFU_DNLOAD_IDLE:
                // emit operationStatus("设置地址成功！", "green");
                return true;
            case DFU_STATE_DFU_UPLOAD_IDLE:
                // emit operationStatus("设置地址成功！", "green");
                return true;
            default:
                QThread::usleep(1);
                break;
        }
    }
    if(timer.elapsed() >= 30000){
        emit operationStatus("设置地址超时", "red");
        return false;
    }
    return true;
}

bool STM32Bootloader::usbSendAbortTransmit(libusb_device_handle *handle) {
    int ret;
    uint8_t bState;
    uint8_t status[6] = {0};
    QElapsedTimer timer;


    ret = libusb_control_transfer(handle,
                                  LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                  DFU_DNLOAD,
                                  0,  // wValue (block number)
                                  2,  // interface
                                  nullptr,
                                  0,
                                  DFU_TIMEOUT);
    if (ret < 0) {
        emit operationStatus(QString("发送设置地址命令失败: %1").arg(libusb_error_name(ret)), "red");
        return false;
    }

    ret = libusb_control_transfer(handle,
                              LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                              DFU_GETSTATUS,
                              0, 0,
                              status, sizeof(status),
                              DFU_TIMEOUT);

    bState = status[4];

    while ((bState != DFU_STATE_DFU_IDLE) || (bState != DFU_STATE_DFU_DNLOAD_IDLE) || (bState != DFU_STATE_DFU_UPLOAD_IDLE)) {
        ret = libusb_control_transfer(handle,
                                      LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                      DFU_GETSTATUS,
                                      0, 0,
                                      status, sizeof(status),
                                      DFU_TIMEOUT);

        if (ret < 0) {
            break;
        }
    }

    return true;
}

bool STM32Bootloader::usbWriteMemory(libusb_device_handle *handle, quint32 address, const QByteArray &data) {
    // 1. 准备写内存命令
    QByteArray command;

    command.append(data);
    // emit dataSent(command);

    // 2. 发送命令
    if (!usbSendData(handle, command)) {
        return false;
    }

    return true;
}

bool STM32Bootloader::usbEraseFlash(libusb_device_handle* handle, int totalPages) {
    int ret;
    // uint8_t bState;
    uint8_t status[6] = {0};
    uint32_t eraseAddr = 0x08000000;
    unsigned char eraseCmd[5] = {0};

    QElapsedTimer timer;


    emit operationStatus("开始擦除Flash...", "blue");

    if (totalPages > 16)
        totalPages = 16;

    // 2. 构造DFU擦除命令（全片擦除）
    eraseCmd[0] = 0x41;
    for(uint8_t i = 0;i < totalPages;i++)
    {
        eraseCmd[1] = eraseAddr & 0xff;
        eraseCmd[2] = (eraseAddr >> 8) & 0xff;
        eraseCmd[3] = (eraseAddr >> 16) & 0xff;
        eraseCmd[4] = (eraseAddr >> 24) & 0xff;
        eraseAddr += 0x20000;
        ret = libusb_control_transfer(handle,
                                          LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                          DFU_DNLOAD,
                                          0,  // wValue (block number)
                                          0,  // interface
                                          eraseCmd,
                                          5,
                                          DFU_TIMEOUT);
        if (ret < 0) {
            emit operationStatus(QString("发送擦除命令失败: %1").arg(libusb_error_name(ret)), "red");
            return false;
        }


        timer.start();
        while (timer.elapsed() < 30000) { // 30秒超时
            ret = libusb_control_transfer(handle,
                                          LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                          DFU_GETSTATUS,
                                          0, 0,
                                          status, sizeof(status),
                                          DFU_TIMEOUT);

            if (ret < 0) {
                emit operationStatus(QString("获取状态失败: %1").arg(libusb_error_name(ret)), "red");
                return false;
            }

            // 解析状态
            uint8_t bStatus = status[0];
            // uint32_t bwPollTimeout = (status[3] << 16) | (status[2] << 8) | status[1];
            uint8_t bState = status[4];

            switch (bState) {
                case DFU_STATE_DFU_DNLOAD_BUSY:
                    // 设备正忙，等待指定的时间
                    emit operationStatus(QString("Flash 扇区%1 擦除中...").arg(i), "blue");
                    QThread::usleep(1);
                    continue;
                case DFU_STATE_DFU_ERROR:
                    emit operationStatus(QString("DFU错误状态: 0x%1").arg(bStatus, 2, 16, QChar('0')), "red");
                    // 清除错误状态
                    libusb_control_transfer(handle,
                                            LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                            DFU_CLRSTATUS,
                                            0, 0,
                                            nullptr, 0,
                                            DFU_TIMEOUT);
                    return false;
                case DFU_STATE_DFU_IDLE:
                    emit progressUpdated(((i+1)/totalPages)*100);
                    emit operationStatus(QString("Flash 扇区%1 擦除成功！").arg(i), "green");
                    break;
                case DFU_STATE_DFU_DNLOAD_IDLE:
                    emit progressUpdated(((i+1)/totalPages)*100);
                    emit operationStatus(QString("Flash 扇区%1 擦除成功！").arg(i), "green");
                    break;
                case DFU_STATE_DFU_UPLOAD_IDLE:
                    emit progressUpdated(((i+1)/totalPages)*100);
                    emit operationStatus(QString("Flash 扇区%1 擦除成功！").arg(i), "green");
                    break;
                default:
                    QThread::usleep(10);
                    break;
            }

            if((bState == DFU_STATE_DFU_IDLE) || (bState == DFU_STATE_DFU_DNLOAD_IDLE) || (bState == DFU_STATE_DFU_UPLOAD_IDLE))
                break;
        }
        if(timer.elapsed() >= 30000){
            emit operationStatus("擦除操作超时", "red");
            return false;
        }
    }
    emit progressUpdated(100);
    return true;
}

bool STM32Bootloader::usbJumpToApp(libusb_device_handle *handle, quint32 address) {

    // 构造跳转命令包
    if(!usbSetAddress(handle, address)){
        throw std::runtime_error("USB跳转APP地址失败");
    }

    if(!usbSendAbortTransmit(handle)){
        throw std::runtime_error("USB终止传输指令发送失败");
    }

    return true; // 设备将自动复位执行APP
}

void STM32Bootloader::abortOperation()
{
    m_abort = true;
}

bool STM32Bootloader::enterBootloaderMode(QSerialPort *port) {
    if (!port) return false;

    serialBootloaderConfig(*port);
    port->clear();

    for (int i = 0; i < 3 && !m_abort.load(); ++i) {
        QByteArray sync(1, 0x7F);
        port->write(sync);
        emit dataSent(sync);

        if (port->waitForReadyRead(10)) {
            QByteArray response = port->readAll();
            emit dataReceived(response, true);

            if (response.contains(0x79)) {
                emit operationStatus("成功进入Bootloader模式", "green");
                return true;
            }
        }
        QThread::msleep(100);
    }

    emit operationStatus("尝试通过硬件控制进入BootLoader模式...", "blue");
    port->setRequestToSend(true);
    port->setDataTerminalReady(false);
    QThread::msleep(100);
    port->setRequestToSend(false);
    QThread::msleep(500);

    emit operationStatus("发送复位信号...", "blue");

    for (int i = 0; i < 3 && !m_abort.load(); ++i) {
        QByteArray sync(1, 0x7F);
        port->write(sync);
        emit dataSent(sync);

        if (port->waitForReadyRead(10)) {
            QByteArray response = port->readAll();
            emit dataReceived(response, true);

            if (response.contains(0x79)) {
                emit operationStatus("成功进入Bootloader模式", "green");
                return true;
            }
        }
        QThread::msleep(100);
    }

    emit operationStatus("无法进入Bootloader模式", "red");
    return false;
}

bool STM32Bootloader::eraseFlash(QSerialPort &port, int totalPages)
{
    emit operationStatus("开始擦除Flash...", "blue");

    // 分多次擦除，每次擦除一个页（参考实际通信日志）
    for (int page = 0; page < totalPages && !m_abort; ++page) {
        // 1. 发送扩展擦除命令 (0x44 BB)
        QByteArray cmd;
        cmd.append(static_cast<char>(0x44));  // 命令
        cmd.append(static_cast<char>(0xBB));  // 校验和
        port.write(cmd);
        emit dataSent(cmd);

        // 2. 等待ACK (0x79)
        if (!port.waitForReadyRead(500)) {
            emit operationStatus(QString("擦除页%1命令超时").arg(page), "red");
            return false;
        }
        QByteArray ack = port.readAll();
        emit dataReceived(ack, true);
        if (!ack.contains(0x79)) {
            emit operationStatus(QString("擦除页%1命令被拒绝").arg(page), "red");
            return false;
        }

        // 3. 发送页擦除参数（单页模式）
        QByteArray params;
        params.append(static_cast<char>(0x00));  // 页号高字节（始终0）
        params.append(static_cast<char>(0x00));  // 页号中字节（始终0）
        params.append(static_cast<char>(0x00));  // 页号低字节（起始页）
        params.append(static_cast<char>(page));   // 页号（当前页）
        params.append(static_cast<char>(page));   // 校验和（简化计算）
        port.write(params);
        emit dataSent(params);

        // 4. 等待擦除完成
        if (!port.waitForReadyRead(1000)) {
            emit operationStatus(QString("擦除页%1操作超时").arg(page), "red");
            return false;
        }
        QByteArray result = port.readAll();
        emit dataReceived(result, true);
        if (!result.contains(0x79)) {
            emit operationStatus(QString("擦除页%1失败，收到NACK").arg(page), "red");
            return false;
        }

        // 5. 更新进度
        int progress = (page * 100 / totalPages);
        emit progressUpdated(progress);
    }

    if (m_abort) return false;

    emit progressUpdated(100);
    emit operationStatus("Flash擦除成功", "green");
    return true;
}

bool STM32Bootloader::sendWriteMemoryCommand(QSerialPort &port, quint32 address, const QByteArray &data)
{
    QByteArray cmd;
    cmd.append(0x31);
    cmd.append(0xCE);
    port.write(cmd);
    emit dataSent(cmd);

    if (!port.waitForReadyRead(500)) return false;
    QByteArray ack = port.readAll();
    emit dataReceived(ack, true);
    if (!ack.contains(0x79)) return false;

    QByteArray addr;
    addr.append((address >> 24) & 0xFF);
    addr.append((address >> 16) & 0xFF);
    addr.append((address >> 8) & 0xFF);
    addr.append(address & 0xFF);
    quint8 checksum = calculateChecksum(addr);
    port.write(addr);
    port.putChar(checksum);
    emit dataSent(addr + QByteArray(1, checksum));

    if (!port.waitForReadyRead(500)) return false;
    ack = port.readAll();
    emit dataReceived(ack, true);
    if (!ack.contains(0x79)) return false;

    quint8 length = data.size() - 1;
    QByteArray toSend;
    toSend.append(length);
    toSend.append(data);
    checksum = calculateChecksum(toSend);

    for (int i = 0; i < toSend.size(); i += CHUNK_SIZE) {
        if (m_abort) return false;

        QByteArray chunk = toSend.mid(i, qMin(CHUNK_SIZE, toSend.size() - i));
        port.write(chunk);
        emit dataSent(chunk);

        if (!port.waitForBytesWritten(50)) return false;
        QCoreApplication::processEvents();
    }

    port.putChar(checksum);
    emit dataSent(QByteArray(1, checksum));

    if (!port.waitForReadyRead(1000)) return false;
    ack = port.readAll();
    emit dataReceived(ack, true);

    return ack.contains(0x79);
}

bool STM32Bootloader::jumpToApp(QSerialPort &port, quint32 address)
{
    // 1. 发送Go命令 (0x21)
    QByteArray cmd;
    cmd.append(static_cast<char>(0x21));  // Go命令
    cmd.append(static_cast<char>(0xDE));  // 校验和: 0x21 ^ 0xFF
    port.write(cmd);
    emit dataSent(cmd);

    // 2. 等待ACK
    if (!port.waitForReadyRead(500)) return false;
    QByteArray ack = port.readAll();
    emit dataReceived(ack, true);
    if (!ack.contains(0x79)) return false;

    // 3. 发送目标地址
    QByteArray addr;
    addr.append(static_cast<char>((address >> 24) & 0xFF));
    addr.append(static_cast<char>((address >> 16) & 0xFF));
    addr.append(static_cast<char>((address >> 8) & 0xFF));
    addr.append(static_cast<char>(address & 0xFF));
    quint8 checksum = addr[0] ^ addr[1] ^ addr[2] ^ addr[3];
    port.write(addr);
    port.write(reinterpret_cast<const char*>(&checksum), 1);
    emit dataSent(addr + QByteArray(1, checksum));

    emit operationStatus(QString("Go命令跳转APP！"), "green");

    // 4. 不需要等待响应（设备会立即跳转）
    QThread::msleep(100); // 给设备跳转时间
    return true;
}

bool STM32Bootloader::resetToApp(QSerialPort &port)
{
    // 1. 发送复位命令
    port.setRequestToSend(true);  // 拉低NRST
    port.setDataTerminalReady(true); // 确保BOOT0为低电平
    QThread::msleep(100);
    port.setRequestToSend(false); // 释放NRST

    emit operationStatus(QString("复位跳转APP！"), "green");
    return true;
}

quint8 STM32Bootloader::calculateChecksum(const QByteArray &data)
{
    quint8 checksum = 0;
    for (int i = 0; i < data.size(); ++i) {
        checksum ^= static_cast<quint8>(data.at(i));
    }
    return checksum;
}

void STM32Bootloader::serialBootloaderConfig(QSerialPort &port)
{
    port.setBaudRate(QSerialPort::Baud115200);
    port.setDataBits(QSerialPort::Data8);
    port.setParity(QSerialPort::EvenParity);
    port.setStopBits(QSerialPort::OneStop);
    port.setFlowControl(QSerialPort::NoFlowControl);
    port.setReadBufferSize(8192);
}

void STM32Bootloader::serialCmdConfig(QSerialPort &port)
{
    port.setBaudRate(921600);
    port.setDataBits(QSerialPort::Data8);
    port.setParity(QSerialPort::NoParity);
    port.setStopBits(QSerialPort::OneStop);
    port.setFlowControl(QSerialPort::NoFlowControl);
    port.setReadBufferSize(8192);
}

void STM32Bootloader::printSupportedCommands(QSerialPort &port)
{
    // 1. 发送获取指令列表命令（0x00 0xFF）
    QByteArray cmd;
    cmd.append(char(0x00));  // 修正为char类型
    cmd.append(char(0xFF));  // 同样修正

    if(port.write(cmd) != cmd.size()) {
        emit operationStatus("发送Get命令失败", "red");
        return;
    }
    port.flush();

    // 2. 等待应答（超时500ms）
    if(!port.waitForReadyRead(500)) {
        emit operationStatus("设备无响应", "orange");
        return;
    }

    // 3. 读取返回数据
    QByteArray response = port.readAll();
    while(port.waitForReadyRead(50)) {
        response += port.readAll();
    }

    // 4. 解析应答
    if(response.isEmpty() || static_cast<uint8_t>(response[0]) != 0x79) {
        emit operationStatus("无效应答", "red");
        return;
    }

    // 5. 打印支持的指令列表（从第2字节开始）
    QString supportedCommands = "当前设备支持的BootLoader指令（部分芯片可能有所不同）：\n";
    emit operationStatus(supportedCommands, "blue");

    for(int i = 1; i < response.size(); ++i) {
        uint8_t cmdCode = static_cast<uint8_t>(response[i]);
        supportedCommands = QString("0x%1 - %2\n")
                                .arg(cmdCode, 2, 16, QLatin1Char('0'))
                                .arg(getCommandDescription(cmdCode));
        emit operationStatus(supportedCommands, "blue");
    }
}

QString STM32Bootloader::getCommandDescription(uint8_t cmdCode)
{
    switch(cmdCode) {
    case 0x00: return "Get命令（获取支持指令列表和Bootloader版本）\n";
    case 0x01: return "Get Version（获取Bootloader版本和读保护状态）\n";
    case 0x02: return "Get ID（获取芯片Device ID/PID）\n";
    case 0x11: return "Read Memory（读取内存/Flash数据）\n";
    case 0x21: return "Go（跳转到指定地址执行）\n";
    case 0x31: return "Write Memory（写入内存/Flash数据）\n";
    case 0x43: return "Erase Flash（擦除Flash页）\n";
    case 0x44: return "Extended Erase（扩展擦除，支持全片擦除）\n";
    case 0x51: return "Write Protect/Switch Baudrate（写保护配置/切换传输速率）\n";
    case 0x63: return "Readout Protect（读保护状态查询）\n";
    case 0x73: return "Readout Unprotect（解除读保护）\n";
    case 0x82: return "Write Option Bytes（写选项字节）\n";
    case 0x92: return "Execute（执行特殊操作，如复位）\n";
    case 0x0B: return "Get Checksum（计算Flash校验和）\n";
    case 0x79: return "ACK（命令应答信号）\n";
    default: return "未知指令\n";
    }
}
