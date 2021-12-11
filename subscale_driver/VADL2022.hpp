#ifndef VADL2022_HPP
#define VADL2022_HPP

#include "LOG.hpp"
#include "IMU.hpp"

using namespace std;

class VADL2022
{
public:
    LOG *mLog;     // LOG
    IMU *mImu;     // IMU
    LIDAR *mLidar; // LIDAR
    LDS *mLds;     // LDS
    MOTOR *mMotor; // ODRIVE

    VADL2022();
    ~VADL2022();

    void GDS();
    void PDS();
    void IS();
    void LS();
    void COMMS();

private:
    bool GDSTimeout = 0;

    void connect_GPIO();
    void disconnect_GPIO();
    void connect_Python();
    void disconnect_Python();

    static void *GDSReleaseTimeoutThread(void *);
};

#endif
