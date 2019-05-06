#include "FpMaxeye.h"

#include "hidapi.h"
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>

#include "vcsTypes.h"
#include "vcsfw_v4.h"

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

int FpMaxeye::GetDeviceInfo()
{
    int rc{ 0 };

#if 0
    if (hid_init())
        return ERROR_HID_INIT;

    struct hid_device_info *devs, *cur_dev;

    devs = hid_enumerate(SYNA_MAXEYE_VID, SYNA_MAXEYE_PID);
    cur_dev = devs;
    while (cur_dev) 
    {
        cout << "get instance : " << cur_dev->path << endl;
        cout << "manufacturer_string : " << cur_dev->manufacturer_string << endl;
        cout << "product_string : " << cur_dev->product_string << endl;
        cout << "release_number : " << cur_dev->release_number << endl;
        cout << "interface_number : " << cur_dev->interface_number << endl;
        cout << "serial_number : " << cur_dev->serial_number << endl;

        cur_dev = cur_dev->next;
    }
    hid_free_enumeration(devs);
#endif

    return rc;
}

int FpMaxeye::CreateInstance(FpMaxeye * &opFpMaxeye)
{
    opFpMaxeye = new FpMaxeye();

    return 0;
}

int FpMaxeye::Open(string strPath)
{
    int rc{ ERROR_OK };

    if (hid_init())
        return ERROR_HID_INIT;

    _handle = nullptr;
    if (0 != strPath.size())
        _handle = hid_open_path(strPath.c_str());
    else
        _handle = hid_open(SYNA_MAXEYE_VID, SYNA_MAXEYE_PID, NULL);
    if (!_handle)
    {
        return ERROR_OPEN_DEVICE;
    }

    return rc;
}

int FpMaxeye::FpGetVersion(uint8_t *arrVersion, uint32_t size)
{
    int rc(0);

    uint16_t status(0);
    uint32_t replysize(0);

    vcsfw_reply_get_version_t version = { 0 };
    rc = this->ExecuteCmd(VCSFW_CMD_GET_VERSION, NULL, 0, (uint8_t*)&version, sizeof(vcsfw_reply_get_version_t), status, replysize);
    if (0 != rc || 0 != status)
    {
        return 0 != rc ? rc : status;
    }

    memcpy(arrVersion, &version, size);

    return rc;
}

int FpMaxeye::FpTestRun(uint8_t subCmd, uint8_t *arrParameter, uint32_t parameterSize, uint8_t *arrResponse, uint32_t responseSize, uint32_t &result, uint32_t &oReplyCounts)
{
    /*
     *  Combined the command VCSFW_CMD_TEST_RUN + Sub command.
     *  Then get the response.
     */
    uint32_t rc(0);

    uint16_t status(0);
    uint32_t replysize(0);

    vector<uint8_t> listOfData(sizeof(uint8_t)+parameterSize);
    memcpy(listOfData.data(), &subCmd, sizeof(uint8_t));
    if (NULL != arrParameter && 0 != parameterSize)
        memcpy(&(listOfData.data()[sizeof(uint8_t)]), arrParameter, parameterSize);
    rc = this->ExecuteCmd(VCSFW_CMD_TEST_RUN, listOfData.data(), listOfData.size(), NULL, 0, status, replysize);
    if (0 != rc)
    {
        return rc;
    }

    //Test Results Read
    uint32_t replySubCmd(0);
    uint32_t timeout = 2000;
    vector<uint8_t> listOfReply(sizeof(uint32_t)+sizeof(uint32_t)+responseSize);
    do
    {
        rc = this->ExecuteCmd(VCSFW_CMD_TEST_RESULTS_READ, NULL, 0, listOfReply.data(), listOfReply.size(), status, replysize);
        if (0 == status)
            break;
        else if (VCSFW_STATUS_ERR_IST_RESULTS_NOTYET == status)
        {
            timeout--;
            ::Sleep(10);
        }
        else
            return 0 != rc ? rc : status;
    } while (0 != timeout);
    if (0 == timeout)
        return ERROR_TIMEOUT;

    oReplyCounts = replysize;

    rc = this->ExecuteCmd(VCSFW_CMD_TEST_COMPLETE, NULL, 0, NULL, 0, status, replysize);
    if (0 != rc || 0 != status)
    {
        return 0 != rc ? rc : status;
    }

    //parse reply
    memcpy(&result, listOfReply.data(), sizeof(uint32_t));
    memcpy(&replySubCmd, &(listOfReply.data()[sizeof(uint32_t)]), sizeof(uint32_t));
    oReplyCounts = oReplyCounts - (sizeof(uint16_t)+sizeof(uint32_t)* 2);//minus status, result, subcmd

    if (NULL != arrResponse && 0 != responseSize)
    {
        if (responseSize <= oReplyCounts)
            memcpy(arrResponse, &(listOfReply.data()[sizeof(uint32_t)+sizeof(uint32_t)]), responseSize);
        else
            memcpy(arrResponse, &(listOfReply.data()[sizeof(uint32_t)+sizeof(uint32_t)]), oReplyCounts);
    }

    return rc;
}

int FpMaxeye::ExecuteCmd(uint8_t cmdName, uint8_t *inputData, uint32_t inputSize, uint8_t *replyData, uint32_t replySize, uint16_t &oStatus, uint32_t &oReplyCounts)
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

    vector<uint8_t> listOfReplyData(sizeof(uint16_t)+replySize);//status + replysize
    rc = Read(listOfReplyData.data(), listOfReplyData.size(), oReplyCounts);
    if (0 != rc)
        return rc;

    memcpy(&oStatus, listOfReplyData.data(), sizeof(uint16_t));
    memcpy(replyData, &(listOfReplyData.data()[sizeof(uint16_t)]), replySize);

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
            wstring strErrorMsg = hid_error(_handle);
            wcout << "Write - Error message : " << strErrorMsg << endl;
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
            cout << "rc = "<< rc << endl;
            if (rc < 0)
            {
                wstring strErrorMsg = hid_error(_handle);
                wcout << "Read - Error message : " << strErrorMsg << endl;
                return ERROR_DEVICE_READ;
            }
            else
            {
                //waiting...
                cout << "Sleep" << endl;
                Sleep(500);
            }
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