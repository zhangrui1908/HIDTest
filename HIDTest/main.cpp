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
//#include "vcsTypes.h"
//#include "vcsfw_v4.h"

typedef struct GetVersion_s
{
    uint32_t   buildtime;        /* Unix-style build time, in seconds   */
    /*  from 1/1/1970 12:00 AM GMT         */
    uint32_t   buildnum;         /* build number                        */
    uint8_t    vmajor;           /* major version                       */
    uint8_t    vminor;           /* minor version                       */
    uint8_t    target;           /* target, e.g. VCSFW_TARGET_ROM       */
    uint8_t    product;          /* product, e.g.  VCSFW_PRODUCT_FALCON */
    uint8_t    siliconrev;       /* silicon revision                    */
    uint8_t    formalrel;        /* boolean: non-zero -> formal release */
    uint8_t    platform;         /* Platform (PCB) revision             */
    uint8_t    patch;            /* patch level                         */
    uint8_t    serial_number[6]; /* 48-bit Serial Number                */
    uint8_t    security[2];      /* bytes 0 and 1 of OTP                */
    uint32_t   patchsig;         /* opaque patch signature              */
    uint8_t    iface;            /* interface type, see below           */
    uint8_t    otpsig[3];        /* OTP Patch Signature                 */
    uint16_t   otpspare1;        /* spare space                         */
    uint8_t    reserved;         /* reserved byte                       */
    uint8_t    device_type;      /* device type                         */
} GetVersion_t;

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

int main(int argc, char* argv[])
{
    int rc{ 0 };
    
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

    GetVersion_t version = { 0 };
    rc = pFpMaxeye->ExecuteCmd(1, NULL, 0, (uint8_t*)&version, sizeof(GetVersion_t));
    if (0 != rc)
    {
        delete pFpMaxeye; pFpMaxeye = nullptr;
        cout << "Error to ExecuteCmd!" << endl;
        return rc;
    }

    time_t buildtime = { 0 };
    struct tm mytm = { 0 };
    wchar_t timestr[80] = { 0 };
    buildtime = (time_t)version.buildtime;
    localtime_s(&mytm, &buildtime);
    (void)wcsftime(&(timestr[0]), (sizeof((timestr)) / sizeof((timestr)[0])), L"%d-%b-%Y %I:%M:%S %p %Z", &mytm);
    wprintf(L"buildtime = %s\n", timestr);
    wprintf(L"buildnum = %lu\n", version.buildnum);
    wprintf(L"vmajor = %lu\n", version.vmajor);
    wprintf(L"vminor = %lu\n", version.vminor);
    wprintf(L"target = %u\n", version.target);

    wstring strProduct;
    if (65 == version.product)
        strProduct = L"PROMETHEUS";
    else if (66 == version.product)
        strProduct = L"PROMETHEUSPBL";
    else if (67 == version.product)
        strProduct = L"PROMETHEUSMSBL";
    else
        strProduct = L"?";
    wprintf(L"product = %u (%s)\n", version.product, strProduct.c_str());

    wprintf(L"siliconrev = %u\n", version.siliconrev);
    wprintf(L"formalrel = %u\n", version.formalrel);
    wprintf(L"platform = %u 0x%02x\n", version.platform, version.platform);

    wprintf(L"patch = %u\n", version.patch);

    wprintf(L"serial_number = 0x%02x %02x %02x %02x %02x %02x\n",
        version.serial_number[0], version.serial_number[1], version.serial_number[2], version.serial_number[3], version.serial_number[4], version.serial_number[5]);

    wprintf(L"security[2]= 0x%02x %02x\n", version.security[0], version.security[1]);
    wprintf(L"patchsig = 0x%08lx\n", version.patchsig);
    wprintf(L"iface = %u\n", version.iface);
    wprintf(L"otpsig[3] = %02x %02x %02x\n", version.otpsig[0], version.otpsig[1], version.otpsig[2]);
    wprintf(L"otpspare1 = 0x%04x\n", version.otpspare1);
    wprintf(L"reserved = 0x%02x\n", version.reserved);
    wprintf(L"device_type = 0x%02x\n", version.device_type);

    delete pFpMaxeye; 
    pFpMaxeye = nullptr;

    return 0;
}