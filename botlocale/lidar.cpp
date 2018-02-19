#include "lidar.hpp"

Lidar::Lidar(std::string path, _u32 baudrate) : path(path), baudrate(baudrate) {
    this->driver = rplidar::RPlidarDriver::CreateDriver(rplidar::RPlidarDriver::DRIVER_TYPE_SERIALPORT);
    if (!this->driver) {
        std::fprintf(stderr, "Insufficient memory, exit\n");
        exit(-2);
    }
}

bool Lidar::checkHealth() {
    rplidar_response_device_health_t healthInfo;
    auto healthResp = this->driver->getHealth(healthInfo);
    if(IS_OK(healthResp)) {
        std::printf("RPLidar Health Status: %d\n", healthInfo.status);
        if(healthInfo.status == RPLIDAR_STATUS_ERROR) {
            std::fprintf(stderr, "Error; rplidar internal error detected. Please reboot the device to retry.\n");
            return false;
        }
        return true;
    } else {
        std::fprintf(stderr, "Error; cannot retrieve the lidar health code: %x\n", healthResp);
        return false;
    }
};

void Lidar::run(const bool* stop) {
    auto connResp = this->driver->connect(this->path.c_str(), baudrate);
    if(IS_FAIL(connResp)) {
        std::fprintf(stderr, "Error; cannot bind to the specified serial port %s.\n", this->path.c_str());
        this->stop();
    }

    rplidar_response_device_info_t devInfo;
    auto deviResp = this->driver->getDeviceInfo(devInfo);
    if(IS_FAIL(deviResp)) {
        std::fprintf(stderr, "Error; cannot get device info.\n");
        this->stop();
    }

    std::printf("RPLIDAR S/N: ");
    for (unsigned char pos : devInfo.serialnum) {
        std::printf("%02X", pos);
    }
    std::printf("\n");

    std::printf("Firmware Ver: %d.%02d\n"
                "Hardware Rev: %d\n",
                devInfo.firmware_version>>8,
                devInfo.firmware_version & 0xFF,
                (int) devInfo.hardware_version);

    if(!this->checkHealth()) {
        this->stop();
    }

    this->driver->startMotor();
    this->driver->startScan();

    while (true) {
        //TODO: This should be rewritten to respect C++11 idioms.
        //TODO: Unfortunately, that might involve a deeper wrapper for proper destructors hidden away...
        rplidar_response_measurement_node_t nodes[360*2];
        size_t count = _countof(nodes);
        auto scanResp = this->driver->grabScanData(nodes, count);
        if(IS_OK(scanResp)) {
            this->driver->ascendScanData(nodes, count);
            LidarScan tmp;

            for(int pos = 0; pos < (int) count; pos++) {
                float angle = (nodes[pos].angle_q6_checkbit >> RPLIDAR_RESP_MEASUREMENT_ANGLE_SHIFT) / 64.0f;
                tmp.distances[((int)(angle+0.5f)) % 360] = nodes[pos].distance_q2 / 4.0f;
                std::printf("%s theta: %03.2f Dist: %08.2f Q: %d\n",
                            (nodes[pos].sync_quality & RPLIDAR_RESP_MEASUREMENT_SYNCBIT) ?"S ":"  ",
                            (nodes[pos].angle_q6_checkbit >> RPLIDAR_RESP_MEASUREMENT_ANGLE_SHIFT) / 64.0f,
                            nodes[pos].distance_q2/4.0f,
                            nodes[pos].sync_quality >> RPLIDAR_RESP_MEASUREMENT_QUALITY_SHIFT);
            }
        }

        if (*stop) {
            break;
        }
    }
};

void Lidar::stop() {
    this->driver->stop();
    this->driver->stopMotor();

    rplidar::RPlidarDriver::DisposeDriver(this->driver);
}

LidarScan::LidarScan() = default;
LidarScan::LidarScan(const LidarScan& other, int newOffset) {
    memcpy(distances, other.distances, 360*(sizeof(float)));
    offset = other.offset + newOffset;
    if (offset >= 360) {
        offset -= 360;
    }
}

float LidarScan::compare(const LidarScan& expected) {
    float err = 0;
    for(int i = 0; i < 360; i++) {
        if(getAtAngle(i) < expected.getAtAngle(i)) {
            err += (expected.getAtAngle(i) - getAtAngle(i)) * 0.1;
        } else {
            err += (getAtAngle(i) - expected.getAtAngle(i));
        }
    }
    return err;
}

LidarScan LidarScan::getAtLocation(int xCm, int yCm){
    //TODO: Implement getAtLocation functionality
    return LidarScan();
}
LidarScan LidarScan::generateExpected(const Pose& pose){
    return LidarScan(getAtLocation((int)pose.x,(int)pose.y), (int)(pose.yaw*180/3.14159));
}