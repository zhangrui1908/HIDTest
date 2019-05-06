#pragma once

#include "stdint.h"
#include "hidapi.h"
#include <string>

#define ERROR_OK            0
#define ERROR_HID_INIT      1
#define ERROR_OPEN_DEVICE   2
#define ERROR_NULL_DEVICE   3
#define ERROR_DEVICE_WRITE  4
#define ERROR_DEVICE_READ   5

//struct hid_device;
using namespace std;

class FpMaxeye
{
public:
    //FpMaxeye();
    virtual ~FpMaxeye();

    static int GetDeviceInfo();

    static int CreateInstance(FpMaxeye * &opFpMaxeye);

    int Open(string strPath = "");

    int FpGetVersion(uint8_t *arrVersion, uint32_t size);

    int FpTestRun(uint8_t subCmd, uint8_t *arrParameter, uint32_t parameterSize, uint8_t *arrResponse, uint32_t responseSize, uint32_t &result, uint32_t &oReplyCounts);

    int ExecuteCmd(uint8_t cmdName, uint8_t *inputData, uint32_t inputSize, uint8_t *replyData, uint32_t replySize, uint16_t &oStatus, uint32_t &oReplyCounts);

    int Write(uint8_t *inputBuffer, uint32_t size);

    int Read(uint8_t *replyBuffer, uint32_t size, uint32_t &replySize);

protected:

    FpMaxeye();

protected:

    hid_device *_handle;
};

