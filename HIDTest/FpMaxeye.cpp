#include "FpMaxeye.h"

#include "hidapi.h"
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>

using namespace std;

#define SYNA_MAXEYE_VID 0x045e//0x2045e
#define SYNA_MAXEYE_PID 0x0813

#define SYNA_MAGIC_DATA   "synaptic"
#define SYNA_MAGIC_SIZE   8

#define HID_REPORT_ID_CMD           0x0E
#define HID_REPORT_ID_RESP          0x0F
#define HID_REPORT_ID_INTERRUPT     0x10
#define HID_REPORT_ID_FEATURE       0x11

#define MAX_STR 255

#define MAXEYE_INTERACTIONDATA_SIZE 20
#define MAXEYE_MAX_TIMEOUT_VALUE    15000

FpMaxeye::FpMaxeye()
:_handle(nullptr)
{
}

FpMaxeye::~FpMaxeye()
{
    if (nullptr != _handle)
    {
        hid_close(_handle);
        _handle = nullptr;
    }

    hid_exit();
}

int FpMaxeye::CreateInstance(FpMaxeye * &opFpMaxeye)
{
    opFpMaxeye = new FpMaxeye();

    return 0;
}

int FpMaxeye::Open()
{
    int rc{ ERROR_OK };

    if (hid_init())
        return ERROR_HID_INIT;

    _handle = nullptr;
    _handle = hid_open(SYNA_MAXEYE_VID, SYNA_MAXEYE_PID, NULL);
    if (!_handle)
    {
        return ERROR_OPEN_DEVICE;
    }

    return rc;
}

int FpMaxeye::ExecuteCmd(uint8_t cmdName, uint8_t *inputData, uint32_t inputSize, uint8_t *replyData, uint32_t replySize)
{
    int rc{ ERROR_OK };

    if (nullptr == _handle)
        return ERROR_NULL_DEVICE;

    vector<uint8_t> listOfInputData(sizeof(uint8_t)+inputSize);
    memcpy(listOfInputData.data(), &cmdName, sizeof(uint8_t));
    if (NULL != inputData && 0 != inputSize)
        memcpy(&(listOfInputData.data()[sizeof(uint8_t)]), inputData, inputSize);

    rc = this->Write(listOfInputData.data(), listOfInputData.size());
    if (0 != rc)
        return rc;

    uint16_t status{ 0 };
    uint32_t realReplySize{ 0 };
    vector<uint8_t> listOfReplyData(sizeof(status)+replySize);
    rc = Read(listOfReplyData.data(), listOfReplyData.size(), realReplySize);
    if (0 != rc)
        return rc;

    memcpy(&status, listOfReplyData.data(), sizeof(uint16_t));
    memcpy(replyData, &(listOfReplyData.data()[sizeof(uint16_t)]), replySize);

    if (0 != status)
        rc = status;

    return rc;
}

int FpMaxeye::Write(uint8_t *inputBuffer, uint32_t size)
{
    int rc{ ERROR_OK };

    if (nullptr == _handle)
        return ERROR_NULL_DEVICE;

    //write
    uint8_t arrSendData[MAXEYE_INTERACTIONDATA_SIZE] = { 0 };
    arrSendData[0] = HID_REPORT_ID_CMD;
    string strMagicData = SYNA_MAGIC_DATA;

    bool firstSend{ true };
    uint32_t offset{ 0 };
    do
    {
        memset(&(arrSendData[1]), 0, MAXEYE_INTERACTIONDATA_SIZE - 1);
        if (firstSend)
        {
            memcpy(&(arrSendData[sizeof(uint8_t)]), strMagicData.c_str(), strMagicData.size());
            memcpy(&(arrSendData[sizeof(uint8_t)+strMagicData.size()]), &size, sizeof(uint32_t));
            //calculate offset
            uint32_t dataCounts = MAXEYE_INTERACTIONDATA_SIZE - (sizeof(uint8_t)+strMagicData.size() + sizeof(uint32_t));
            if (dataCounts >= size)
            {
                memcpy(&(arrSendData[sizeof(uint8_t)+strMagicData.size() + sizeof(uint32_t)]), inputBuffer, size);
                offset += size;
            }
            else
            {
                memcpy(&(arrSendData[sizeof(uint8_t)+strMagicData.size() + sizeof(uint32_t)]), inputBuffer, dataCounts);
                offset += dataCounts;
            }
            firstSend = false;
        }
        else
        {
            uint32_t dataCounts = MAXEYE_INTERACTIONDATA_SIZE - sizeof(uint8_t);
            uint32_t restCounts = size - offset;
            if (dataCounts >= restCounts)
            {
                memcpy(&(arrSendData[sizeof(uint8_t)]), &(inputBuffer[offset]), restCounts);
                offset += restCounts;
            }
            else
            {
                memcpy(&(arrSendData[sizeof(uint8_t)]), &(inputBuffer[offset]), dataCounts);
                offset += dataCounts;
            }
        }

        int result = hid_write(_handle, arrSendData, sizeof(arrSendData));
        if (result < 0)
        {
            return ERROR_DEVICE_WRITE;
        }

    } while (offset != size);

    return rc;
}

int FpMaxeye::Read(uint8_t *replyBuffer, uint32_t size, uint32_t &replySize)
{
    int rc{ ERROR_OK };

    if (nullptr == _handle)
        return ERROR_NULL_DEVICE;

    replySize = 0;
    vector<uint8_t> listOfReplyData;
    uint32_t offset{ 0 };
    bool readComplete{ false };
    do
    {
        rc = 0;
        uint8_t readBuffer[MAXEYE_INTERACTIONDATA_SIZE] = { 0 };
        while (0 == rc)
        {
            rc = hid_read_timeout(_handle, readBuffer, sizeof(readBuffer), MAXEYE_MAX_TIMEOUT_VALUE);
            if (_handle < 0)
                return ERROR_DEVICE_READ;
            //waiting...
            Sleep(500);
        }

        if (HID_REPORT_ID_RESP == readBuffer[0])
        {
            if (0 == replySize)
            {
                string strMagicValue;
                for (uint32_t i = 1; i <= SYNA_MAGIC_SIZE; i++)
                    strMagicValue.push_back(readBuffer[i]);

                if (strMagicValue == SYNA_MAGIC_DATA)
                {
                    memcpy(&replySize, &(readBuffer[sizeof(uint8_t)+SYNA_MAGIC_SIZE]), sizeof(uint32_t));
                    listOfReplyData.resize(replySize);
                    uint32_t dataCounts = MAXEYE_INTERACTIONDATA_SIZE - (sizeof(uint8_t)+SYNA_MAGIC_SIZE + sizeof(uint32_t));
                    memcpy(listOfReplyData.data(), &(readBuffer[sizeof(uint8_t)+SYNA_MAGIC_SIZE + sizeof(uint32_t)]), dataCounts);

                    offset += dataCounts;
                }
            }
            else
            {
                uint32_t restDataCounts = replySize - offset;
                if (restDataCounts < (MAXEYE_INTERACTIONDATA_SIZE - 1))
                {
                    memcpy(&(listOfReplyData.data()[offset]), &(readBuffer[sizeof(uint8_t)]), restDataCounts);
                    offset += restDataCounts;
                }
                else
                {
                    memcpy(&(listOfReplyData.data()[offset]), &(readBuffer[sizeof(uint8_t)]), MAXEYE_INTERACTIONDATA_SIZE - 1);
                    offset += (MAXEYE_INTERACTIONDATA_SIZE - 1);
                }
            }

            if (offset == replySize)
            {
                readComplete = true;
                rc = 0;
            }
        }

    } while (!readComplete);

    if (size > listOfReplyData.size())
        memcpy(replyBuffer, listOfReplyData.data(), listOfReplyData.size());
    else
        memcpy(replyBuffer, listOfReplyData.data(), size);

    return rc;
}