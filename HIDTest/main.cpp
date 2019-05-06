#include "stdint.h"

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include "hidapi.h"

#include <windows.h>
#include <time.h>
#include <io.h>

#include "FpMaxeye.h"
#include "vcsTypes.h"
#include "vcsfw_v4.h"

using namespace std;

uint32_t GetPatchInfoFromPath(string strPathFilePath, vector<uint8_t> &oListOfPatch)
{
    if (-1 == _access(strPathFilePath.c_str(), 4))
    {
        return 1;
    }

    ifstream patchStream;

    //open file
    patchStream.open(strPathFilePath, std::ifstream::binary);
    if (!patchStream)
        return 1;

    patchStream.seekg(0, patchStream.end);
    uint32_t length = (uint32_t)patchStream.tellg();
    patchStream.seekg(0, patchStream.beg);

    // allocate memory:
    vector<char> listOfData(length);
    // read data as a block:
    patchStream.read(listOfData.data(), length);
    patchStream.close();

    oListOfPatch.resize(length);
    memcpy(oListOfPatch.data(), listOfData.data(), length);

    return 0;
}

string convert_version(vcsfw_reply_get_version_t version)
{
    string strVersion;

    time_t buildtime = { 0 };
    struct tm mytm = { 0 };
    char timestr[80] = { 0 };
    buildtime = (time_t)version.buildtime;
    localtime_s(&mytm, &buildtime);
    (void)strftime(&(timestr[0]), (sizeof((timestr)) / sizeof((timestr)[0])), "%d-%b-%Y %I:%M:%S %p %Z", &mytm);
    strVersion += "buildtime = ";
    strVersion += timestr;
    strVersion += "\n";

    strVersion += "buildnum = " + std::to_string(version.buildnum) + "\n";
    strVersion += "vmajor = " + std::to_string(version.vmajor) + "\n";
    strVersion += "vminor = " + std::to_string(version.vminor) + "\n";
    strVersion += "target = " + std::to_string(version.target) + "\n";

    string strProduct;
    if (65 == version.product)
        strProduct = "PROMETHEUS";
    else if (66 == version.product)
        strProduct = "PROMETHEUSPBL";
    else if (67 == version.product)
        strProduct = "PROMETHEUSMSBL";
    else
        strProduct = "?";
    strVersion += "product = " + std::to_string(version.product) + " (" + strProduct + ")\n";

    strVersion += "siliconrev = " + std::to_string(version.siliconrev) + "\n";
    strVersion += "formalrel = " + std::to_string(version.formalrel) + "\n";
    strVersion += "platform = " + std::to_string(version.platform) + "\n";
    strVersion += "patch = " + std::to_string(version.patch) + "\n";

    strVersion += "serial_number = 0x";
    for (uint8_t i = 0; i < 6; i++)
    {
        char buffer[_MAX_INT_DIG] = { 0 };
        sprintf_s(buffer, sizeof (buffer), "%02x", version.serial_number[i]);
        strVersion += buffer;
        strVersion += " ";
    }
    strVersion += "\n";

    strVersion += "security[2] = 0x";
    for (uint8_t i = 0; i < 2; i++)
    {
        char buffer[_MAX_INT_DIG] = { 0 };
        sprintf_s(buffer, sizeof (buffer), "%02x", version.security[i]);
        strVersion += buffer;
        strVersion += " ";
    }
    strVersion += "\n";

    strVersion += "patchsig = 0x";
    char buf[_MAX_INT_DIG] = { 0 };
    sprintf_s(buf, sizeof (buf), "%08lx", version.patchsig);
    strVersion += buf;
    strVersion += "\n";

    strVersion += "iface = " + std::to_string(version.iface) + "\n";

    strVersion += "otpsig[3] = 0x";
    for (uint8_t i = 0; i < 3; i++)
    {
        char buffer[_MAX_INT_DIG] = { 0 };
        sprintf_s(buffer, sizeof (buffer), "%02x", version.otpsig[i]);
        strVersion += buffer;
        strVersion += " ";
    }
    strVersion += "\n";

    memset(buf, 0, _MAX_INT_DIG);
    strVersion += "otpspare1 = 0x";
    sprintf_s(buf, sizeof (buf), "%04x", version.otpspare1);
    strVersion += buf;
    strVersion += "\n";

    memset(buf, 0, _MAX_INT_DIG);
    strVersion += "reserved = 0x";
    sprintf_s(buf, sizeof (buf), "%02x", version.reserved);
    strVersion += buf;
    strVersion += "\n";

    memset(buf, 0, _MAX_INT_DIG);
    strVersion += "device_type = 0x";
    memset(buf, 0, _MAX_INT_DIG);
    sprintf_s(buf, sizeof (buf), "%02x", version.device_type);
    strVersion += buf;
    strVersion += "\n";

    return strVersion;
}

int main(int argc, char* argv[])
{
    int rc{ 0 };

    rc = FpMaxeye::GetDeviceInfo();
    
    FpMaxeye *pFpMaxeye = nullptr;
    rc = FpMaxeye::CreateInstance(pFpMaxeye);
    if (0 != rc || nullptr == pFpMaxeye)
    {
        cout << "Error to CreateInstance!" << endl;
        return rc;
    }

    rc = pFpMaxeye->Open();
    if (0 != rc)
    {
        delete pFpMaxeye; pFpMaxeye = nullptr;
        cout << "Error to Open!" << endl;
        return rc;
    }
    
    uint16_t status(0);
    uint32_t replysize(0);
    ::Sleep(100);

    //TIDLE
   /* uint16_t idle_time(0);
    rc = pFpMaxeye->ExecuteCmd(VCSFW_CMD_TIDLE_SET, (uint8_t*)&idle_time, sizeof(uint16_t), NULL, 0, status, replysize);
    if (0 != rc || 0 != status)
    {
        delete pFpMaxeye; pFpMaxeye = nullptr;
        cout << "Error to ExecuteCmd!" << endl;
        return rc == 0 ? rc : status;
    }*/


    vcsfw_reply_get_version_t version = { 0 };
    rc = pFpMaxeye->FpGetVersion((uint8_t*)&version, sizeof(vcsfw_reply_get_version_t));
    if (0 != rc)
    {
        delete pFpMaxeye; pFpMaxeye = nullptr;
        cout << "Error to FpGetVersion!" << endl;
        return rc;
    }
    string strVersion = convert_version(version);
    cout << "version : \n" << strVersion << endl;

    vcsfw_reply_get_startinfo_t startinfo = { 0 };
    rc = pFpMaxeye->ExecuteCmd(VCSFW_CMD_GET_STARTINFO, NULL, 0, (uint8_t*)&startinfo, sizeof(vcsfw_reply_get_startinfo_t), status, replysize);
    if (0 != rc || 0 != status)
    {
        delete pFpMaxeye; pFpMaxeye = nullptr;
        cout << "Error to ExecuteCmd!" << endl;
        return rc == 0 ? rc : status;
    }
    cout << "start_type : " << startinfo.start_type << endl;
    cout << "reset_type : " << startinfo.reset_type << endl;
    cout << "start_status : " << startinfo.start_status << endl;
    cout << "sanity_pc : " << startinfo.sanity_pc << endl;
    cout << "sanity_code : " << startinfo.sanity_code << endl;
    cout << "reset_nvinfo : ";
    for (uint32_t i = 0; i < 13; i++)
        cout << startinfo.reset_nvinfo[i];
    cout << endl;

    ::Sleep(500);
    //reply
    uint32_t result(0), replyCounts(0);
    vector<uint8_t> listOfMTRecord(12);
    rc = pFpMaxeye->FpTestRun(9, NULL, 0, listOfMTRecord.data(), listOfMTRecord.size(), result, replyCounts);
    if (0 != rc)
    {
        delete pFpMaxeye; pFpMaxeye = nullptr;
        cout << "Error to ExecuteCmd!" << endl;
        return rc;
    }
    for (size_t t = 0; t < listOfMTRecord.size(); t++)
        cout << listOfMTRecord[t] << endl;

#if 0
    //pixeltest
    vcsfw_cmd_pixel_test_t cmd_pixel_test = { 0 };
    cmd_pixel_test.maxPixelAdc = 950;
    cmd_pixel_test.minPixelAdc = 450;
    cmd_pixel_test.maxRowAveDelta = 324;
    cmd_pixel_test.maxColAveDelta = 162;
    cmd_pixel_test.maxRowPixelRange = 320;
    cmd_pixel_test.maxColPixelRange = 320;

    ::Sleep(500);
    //reply
    uint32_t result(0), replyCounts(0);
    vcsfw_reply_pixel_test_results_t reply_pixel_test_results = { 0 };
    rc = pFpMaxeye->FpTestRun(VCSFW_IST_OPCODE_PIXEL_TEST, (uint8_t*)&cmd_pixel_test, sizeof(vcsfw_cmd_pixel_test_t),
        (uint8_t*)&reply_pixel_test_results, sizeof(vcsfw_reply_pixel_test_results_t), result, replyCounts);
    if (0 != rc)
    {
        delete pFpMaxeye; pFpMaxeye = nullptr;
        cout << "Error to ExecuteCmd!" << endl;
        return rc;
    }

    printf("PIXEL_TEST:\n");
    printf("result : %d\n", result);
    printf("numMaxPixels : %d\n", reply_pixel_test_results.numMaxPixels);
    printf("numMinPixels : %d\n", reply_pixel_test_results.numMinPixels);
    printf("numRowAvePixels : %d\n", reply_pixel_test_results.numRowAvePixels);
    printf("numColAvePixels : %d\n", reply_pixel_test_results.numColAvePixels);
    printf("numRangeRows : %d\n", reply_pixel_test_results.numRangeRows);
    printf("numRangeCols : %d\n", reply_pixel_test_results.numRangeCols);
#endif

    delete pFpMaxeye; 
    pFpMaxeye = nullptr;

    system("pause");

    return 0;
}