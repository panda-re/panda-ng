#include "iohal/memory/virtual_memory.h"
#include "offset/offset.h"
#include "osi/windows/manager.h"
#include "osi/windows/wintrospection.h"
#include "gtest/gtest.h"
#include <set>
#include <unistd.h>

#include <iostream>
#include <map>
#include <vector>

char* testfile = nullptr;

struct ModuleInfo {
    uint64_t base;
    uint32_t size;
    uint16_t loadcount;
    std::string path;
};

std::map<uint64_t, std::vector<struct ModuleInfo>> EXPECTED_RESULTS = {
    {4, {}},                      // None
    {220, {}},                    // PEB paged out
    {320, {}},                    // PEB paged out
    {368, {}},                    // None
    {396, {}},                    // PEB paged out
    {420, {}},                    // None + errors
    {436, {}},                    // None + errors
    {444, {}},                    // None + errors
    {468, {}},                    // None + errors
    {592, {}},                    // PEB paged out
    {664, {}},                    // None + errors
    {704, {{0x0, 0x0, 0x0, ""}}}, // 0,0,0  errors
    {828, {}},                    // None
    {876,
     {{0x00000000ff100000, 0xb000, 0xffff, "C:\\Windows\\system32\\svchost.exe"},
      {0x00000000775c0000, 0x1a9000, 0xffff, "C:\\Windows\\SYSTEM32\\ntdll.dll"},
      {0x00000000774a0000, 0x11f000, 0xffff, "C:\\Windows\\system32\\kernel32.dll"},
      {0x000007fefd770000, 0x6b000, 0xffff, "C:\\Windows\\system32\\KERNELBASE.dll"},
      {0x000007fefeeb0000, 0x9f000, 0xffff, "C:\\Windows\\system32\\msvcrt.dll"},
      {0x000007feff360000, 0x1f000, 0xffff, "C:\\Windows\\SYSTEM32\\sechost.dll"},
      {0x000007feff5e0000, 0x12d000, 0xffff, "C:\\Windows\\system32\\RPCRT4.dll"},
      {0x000007fefeae0000, 0x203000, 0x64, "C:\\Windows\\system32\\ole32.dll"},
      {0x000007feff7e0000, 0x67000, 0x1af, "C:\\Windows\\system32\\GDI32.dll"},
      {0x00000000773a0000, 0xfa000, 0x1de, "C:\\Windows\\system32\\USER32.dll"},
      {0x000007fefed90000, 0xe000, 0x5f, ""},
      {0x000007feff030000, 0xc9000, 0x5f, "C:\\Windows\\system32\\USP10.dll"},
      {0x000007feff7b0000, 0x2e000, 0x2, "C:\\Windows\\system32\\IMM32.DLL"},
      {0x000007fefeda0000, 0x109000, 0x1, "C:\\Windows\\system32\\MSCTF.dll"},
      {0x000007fefd3e0000, 0xf000, 0x2, ""},
      {0x000007fefea00000, 0xdb000, 0x46, ""}}},
    {972, {}}, // None
    {412, {}}, // PEB paged out
    {1012,
     {
         {0x00000000ff800000, 0x2c0000, 0xffff, "C:\\Windows\\Explorer.EXE"},
         {0x00000000775c0000, 0x1a9000, 0xffff, "C:\\Windows\\SYSTEM32\\ntdll.dll"},
         {0x00000000774a0000, 0x11f000, 0xffff, "C:\\Windows\\system32\\kernel32.dll"},
         {0x000007fefd770000, 0x6b000, 0xffff, "C:\\Windows\\system32\\KERNELBASE.dll"},
         {0x000007fefea00000, 0xdb000, 0xffff, "C:\\Windows\\system32\\ADVAPI32.dll"},
         {0x000007fefeeb0000, 0x9f000, 0xffff, "C:\\Windows\\system32\\msvcrt.dll"},
         {0x000007feff360000, 0x1f000, 0xffff, "C:\\Windows\\SYSTEM32\\sechost.dll"},
         {0x000007feff5e0000, 0x12d000, 0xffff, "C:\\Windows\\system32\\RPCRT4.dll"},
         {0x000007feff7e0000, 0x67000, 0xffff, "C:\\Windows\\system32\\GDI32.dll"},
         {0x00000000773a0000, 0xfa000, 0xffff, "C:\\Windows\\system32\\USER32.dll"},
         {0x000007fefed90000, 0xe000, 0xffff, "C:\\Windows\\system32\\LPK.dll"},
         {0x000007feff030000, 0xc9000, 0xffff, "C:\\Windows\\system32\\USP10.dll"},
         {0x000007feff850000, 0x71000, 0xffff, "C:\\Windows\\system32\\SHLWAPI.dll"},
         {0x000007fefda90000, 0xd88000, 0xffff, "C:\\Windows\\system32\\SHELL32.dll"},
         {0x000007fefeae0000, 0x203000, 0xffff, "C:\\Windows\\system32\\ole32.dll"},
         {0x000007fefef50000, 0xd7000, 0xffff, "C:\\Windows\\system32\\OLEAUT32.dll"},
         {0x000007fefa960000, 0x1ca000, 0xffff,
          "C:\\Windows\\system32\\EXPLORERFRAME.dll"},
         {0x000007fefba90000, 0x43000, 0xffff, "C:\\Windows\\system32\\DUser.dll"},
         {0x000007fefbae0000, 0xf2000, 0xffff, "C:\\Windows\\system32\\DUI70.dll"},
         {0x000007feff7b0000, 0x2e000, 0xffff, "C:\\Windows\\system32\\IMM32.dll"},
         {0x000007fefeda0000, 0x109000, 0xffff, "C:\\Windows\\system32\\MSCTF.dll"},
         {0x000007fefbe00000, 0x56000, 0xffff, "C:\\Windows\\system32\\UxTheme.dll"},
         {0x000007fefb480000, 0x2c000, 0xffff, "C:\\Windows\\system32\\POWRPROF.dll"},
         {0x000007fefe820000, 0x1d7000, 0xffff, "C:\\Windows\\system32\\SETUPAPI.dll"},
         {0x000007fefd5c0000, 0x36000, 0xffff, "C:\\Windows\\system32\\CFGMGR32.dll"},
         {0x000007fefd7e0000, 0x1a000, 0xffff, "C:\\Windows\\system32\\DEVOBJ.dll"},
         {0x000007fefb9d0000, 0x18000, 0xffff, "C:\\Windows\\system32\\dwmapi.dll"},
         {0x000007fefb080000, 0xb000, 0xffff, "C:\\Windows\\system32\\slc.dll"},
         {0x000007fefbbe0000, 0x215000, 0xffff,
          "C:\\Windows\\WinSxS\\amd64_microsoft.windows.gdiplus_6595b64144ccf1df_1.1."
          "7601.17514_none_2b24536c71ed437a\\gdiplus.dll"},
         {0x000007fefd380000, 0xb000, 0xffff, "C:\\Windows\\system32\\Secur32.dll"},
         {0x000007fefd3b0000, 0x25000, 0xffff, "C:\\Windows\\system32\\SSPICLI.DLL"},
         {0x000007fefbe60000, 0x12c000, 0xffff, "C:\\Windows\\system32\\PROPSYS.dll"},
         {0x000007fefd2a0000, 0x3d000, 0x4, "C:\\Windows\\system32\\WINSTA.dll"},
         {0x000007fefd3e0000, 0xf000, 0x2, "C:\\Windows\\system32\\CRYPTBASE.dll"},
         {0x000007fefbfe0000, 0x1f4000, 0x42,
          "C:\\Windows\\WinSxS\\amd64_microsoft.windows.common-controls_6595b64144ccf1df_"
          "6.0.7601.17514_none_fa396087175ac9ac\\comctl32.dll"},
         {0x000007fefb860000, 0x12a000, 0x2, "C:\\Windows\\system32\\WindowsCodecs.dll"},
         {0x000007fefd510000, 0xf000, 0x5, "C:\\Windows\\system32\\profapi.dll"},
         {0x000007fefd490000, 0x57000, 0xffff, "C:\\Windows\\system32\\apphelp.dll"},
         {0x000007feff710000, 0x99000, 0x1, "C:\\Windows\\system32\\CLBCatQ.DLL"},
         {0x000007fefa920000, 0x35000, 0x2, "C:\\Windows\\system32\\EhStorShell.dll"},
         {0x000007fefa8a0000, 0x7e000, 0x2, "C:\\Windows\\System32\\cscui.dll"},
         {0x000007fefa890000, 0xc000, 0x2, "C:\\Windows\\System32\\CSCDLL.dll"},
         {0x000007fefa880000, 0xf000, 0x4, "C:\\Windows\\system32\\CSCAPI.dll"},
         {0x000007fefa800000, 0x80000, 0x4, "C:\\Windows\\system32\\ntshrui.dll"},
         {0x000007fefd2e0000, 0x23000, 0x2, "C:\\Windows\\system32\\srvcli.dll"},
         {0x000007fefa790000, 0x8000, 0x1, "C:\\Windows\\system32\\IconCodecService.dll"},
         {0x000007fefcda0000, 0x17000, 0x3, "C:\\Windows\\system32\\CRYPTSP.dll"},
         {0x000007fefcaa0000, 0x47000, 0x1, "C:\\Windows\\system32\\rsaenh.dll"},
         {0x000007fefd4f0000, 0x14000, 0x1, "C:\\Windows\\system32\\RpcRtRemote.dll"},
         {0x000007fefb6d0000, 0x15000, 0x4, "C:\\Windows\\system32\\wkscli.dll"},
         {0x000007fefceb0000, 0x32000, 0x1, "C:\\Windows\\system32\\netjoin.dll"},
         {0x000007fefb6f0000, 0xc000, 0x6, "C:\\Windows\\system32\\netutils.dll"},
         {0x000007fefba50000, 0x3b000, 0x3, "C:\\Windows\\system32\\SndVolSSO.DLL"},
         {0x000007fefba40000, 0xb000, 0x4, "C:\\Windows\\system32\\HID.DLL"},
         {0x000007fefb9f0000, 0x4b000, 0x2, "C:\\Windows\\System32\\MMDevApi.dll"},
         {0x000007fefa3c0000, 0x83000, 0x1, "C:\\Windows\\system32\\timedate.cpl"},
         {0x000007fefb220000, 0x19000, 0x3, "C:\\Windows\\system32\\ATL.DLL"},
         {0x000007fefa270000, 0xee000, 0x1, "C:\\Windows\\system32\\actxprxy.dll"},
         {0x000007fefaf90000, 0x2d000, 0x1, "C:\\Windows\\system32\\ntmarta.dll"},
         {0x000007feff300000, 0x52000, 0x1, "C:\\Windows\\system32\\WLDAP32.dll"},
         {0x000007fefa100000, 0x34000, 0x4, "C:\\Windows\\System32\\shdocvw.dll"},
         {0x000007fefa0f0000, 0xc000, 0x1, "C:\\Windows\\system32\\LINKINFO.dll"},
         {0x000007fefc860000, 0x1e000, 0x4, "C:\\Windows\\system32\\USERENV.dll"},
         {0x000007fefbf90000, 0x1d000, 0x3, "C:\\Windows\\system32\\SAMLIB.dll"},
         {0x000007fefb6b0000, 0x14000, 0x1, "C:\\Windows\\system32\\samcli.dll"},
         {0x000007fef9fe0000, 0x3b000, 0x2, "C:\\Windows\\system32\\msls31.dll"},
         {0x000007fef9f60000, 0x7f000, 0x1,
          "C:\\Program Files\\Common Files\\microsoft shared\\ink\\tiptsf.dll"},
         {0x000007fefc2f0000, 0x1da000, 0x1, "C:\\Windows\\system32\\authui.dll"},
         {0x000007fefc1e0000, 0x109000, 0x1, "C:\\Windows\\system32\\CRYPTUI.dll"},
         {0x000007fefd600000, 0x167000, 0x10, "C:\\Windows\\system32\\CRYPT32.dll"},
         {0x000007fefd5b0000, 0xf000, 0x10, "C:\\Windows\\system32\\MSASN1.dll"},
         {0x000007fef9cb0000, 0x2a3000, 0x1, "C:\\Windows\\System32\\gameux.dll"},
         {0x000007fefb990000, 0x35000, 0xc, "C:\\Windows\\System32\\XmlLite.dll"},
         {0x000007fef9c30000, 0x7c000, 0x3, "C:\\Windows\\System32\\wer.dll"},
         {0x0000000077790000, 0x7000, 0x4, "C:\\Windows\\system32\\PSAPI.DLL"},
         {0x000007fef99f0000, 0x19c000, 0x1,
          "C:\\Windows\\system32\\NetworkExplorer.dll"},
         {0x000007fef99a0000, 0x43000, 0x5, "C:\\Windows\\system32\\stobject.dll"},
         {0x000007fef98e0000, 0xba000, 0x5, "C:\\Windows\\system32\\BatMeter.dll"},
         {0x000007fefb830000, 0x11000, 0x2, "C:\\Windows\\system32\\WTSAPI32.dll"},
         {0x000007fef9820000, 0x3b000, 0x3, "C:\\Windows\\system32\\WINMM.dll"},
         {0x000007fefb7b0000, 0x69000, 0x1, "C:\\Windows\\system32\\prnfldr.dll"},
         {0x000007fefb730000, 0x71000, 0x2, "C:\\Windows\\system32\\WINSPOOL.DRV"},
         {0x000007fefb000000, 0x67000, 0x1, "C:\\Windows\\system32\\es.dll"},
         {0x000007fefb610000, 0x74000, 0x1, "C:\\Windows\\system32\\dxp.dll"},
         {0x000007feff100000, 0x178000, 0xd, "C:\\Windows\\system32\\urlmon.dll"},
         {0x000007fefd930000, 0x12a000, 0x8, "C:\\Windows\\system32\\WININET.dll"},
         {0x000007feff380000, 0x259000, 0x11, "C:\\Windows\\system32\\iertutil.dll"},
         {0x000007fef9800000, 0x16000, 0x1, "C:\\Windows\\system32\\Syncreg.dll"},
         {0x000007fefb850000, 0xb000, 0x1, "C:\\Windows\\ehome\\ehSSO.d"},
         // ^^ This path is partially paged out
         {0x000007fef9570000, 0x28b000, 0x3, "C:\\Windows\\System32\\netshell.dll"},
         {0x000007fefafd0000, 0x27000, 0x8, "C:\\Windows\\System32\\IPHLPAPI.DLL"},
         {0x000007fefda80000, 0x8000, 0x1a, "C:\\Windows\\system32\\NSI.dll"},
         {0x000007fefafc0000, 0xb000, 0x8, "C:\\Windows\\System32\\WINNSI.DLL"},
         {0x000007fefb280000, 0x15000, 0x4, "C:\\Windows\\System32\\nlaapi.dll"},
         {0x000007fef9550000, 0x20000, 0x1, "C:\\Windows\\system32\\wpdshserviceobj.dll"},
         {0x000007fef9510000, 0x39000, 0x1,
          "C:\\Windows\\system32\\PortableDeviceTypes.dll"},
         {0x000007fef9450000, 0xbd000, 0x1,
          "C:\\Windows\\system32\\PortableDeviceApi.dll"},
         {0x000007fef93f0000, 0x10000, 0x2, "C:\\Windows\\System32\\AltTab.dll"},
         {0x000007fef93b0000, 0x3f000, 0x2, ""},
         {0x000007fef91f0000, 0x1bd000, 0x1, ""},
         {0x000007fef91d0000, 0x1f000, 0x2, ""},
         {0x000007fefcfb0000, 0x6d000, 0xc, ""},
         {0x000007fef9110000, 0xb5000, 0x1, ""},
         {0x000007fef8550000, 0xbb7000, 0x3, ""},
         {0x000007fef84f0000, 0x54000, 0x5, ""},
         {0x000007fefaed0000, 0x11000, 0x1, ""},
         {0x000007fefd8e0000, 0x4d000, 0x8, ""},
         {0x000007fef82f0000, 0xc2000, 0x2, "C:\\Windows\\System32\\Actioncenter.dll"},
         {0x000007fef8290000, 0x58000, 0x1, ""},
         {0x000007fefaeb0000, 0x18000, 0x1, ""},
         {0x000007fefc970000, 0xa000, 0x1, ""},
         {0x000007fef81b0000, 0xd7000, 0x1, ""},
         {0x000007fef8110000, 0x9d000, 0x1, ""},
         {0x00000000751f0000, 0xe3000, 0x1, ""},
         {0x000007fefc670000, 0xc000, 0x5, ""},
         {0x000007fefd800000, 0x3a000, 0x1, "C:\\Windows\\system32\\WINTRUST.dll"},
         {0x000007fef62b0000, 0xc000, 0x1, "C:\\Windows\\System32\\npmproxy.dll"},
         {0x000007fef6280000, 0x1d000, 0x1, "C:\\Windows\\system32\\mssprxy.dll"},
         {0x000007fef6180000, 0xc6000, 0x3, "C:\\Windows\\system32\\MsftEdit.dll"},
         {0x000007fef5fb0000, 0x20000, 0x3, "C:\\Windows\\system32\\Wlanapi.dll"},
         {0x000007fef5fe0000, 0x7000, 0x5, "C:\\Windows\\system32\\wlanutil.dll"},
         {0x000007fef5f50000, 0x5e000, 0x1, "C:\\Windows\\system32\\wwanapi.dll"},
         {0x000007fef5fd0000, 0xd000, 0x2, "C:\\Windows\\system32\\wwapi.dll"},
         {0x000007fef5f00000, 0x45000, 0x1, "C:\\Windows\\System32\\QAgent.dll"},
         {0x000007fef5ec0000, 0x31000, 0x1, "C:\\Windows\\System32\\provsvc.dll"},
         {0x000007fef5c90000, 0x22b000, 0x1, "C:\\Windows\\System32\\SyncCenter.dll"},
         {0x000007fef5c10000, 0x7f000, 0x1, "C:\\Windows\\system32\\imapi2.dll"},
         {0x000007fef5bb0000, 0x55000, 0x1, "C:\\Windows\\System32\\hgcpl.dll"},
         {0x000007fefd3f0000, 0x91000, 0x1, "C:\\Windows\\system32\\SXS.DLL"},
         {0x000007fef5a70000, 0x28000, 0x1, "C:\\Windows\\System32\\wscinterop.dll"},
         {0x000007fef5a50000, 0x13000, 0x4, "C:\\Windows\\System32\\WSCAPI.dll"},
         {0x000007fef5930000, 0x11f000, 0x2, "C:\\Windows\\System32\\wscui.cpl"},
         {0x000007fef57f0000, 0x13c000, 0x1, "C:\\Windows\\System32\\werconcpl.dll"},
         {0x000007fef57a0000, 0x4c000, 0x1, ""},
         {0x000007fef5780000, 0x19000, 0x1, ""},
         {0x000007fef7950000, 0x1f2000, 0x1, "C:\\Windows\\System32\\msxml6.dll"},
         {0x000007fef9b90000, 0xb000, 0x1, "C:\\Windows\\System32\\hcproviders.dll"},
         {0x000007fef4bb0000, 0x21000, 0x1, "C:\\Windows\\System32\\UIAnimation.dll"},
         {0x000007fef46e0000, 0x73000, 0x1,
          "C:\\Program Files\\Internet Explorer\\ieproxy.dll"},
         {0x000007fef46c0000, 0x1f000, 0x1, "C:\\Windows\\system32\\thumbcache.dll"},
         {0x000007fef3e00000, 0x7b000, 0x2, "C:\\Windows\\System32\\StructuredQuery.dll"},
         {0x000007fefae90000, 0x18000, 0x1, "C:\\Windows\\system32\\MPR.dll"},
         {0x000007fef4010000, 0xad000, 0x1, "C:\\Windows\\system32\\van.dll"},
         {0x000007fef3ba0000, 0xd9000, 0x1, "C:\\Windows\\system32\\RasMM.dll"},
         {0x000007fef6030000, 0x62000, 0x1, "C:\\Windows\\system32\\RASAPI32.dll"},
         {0x000007fefa030000, 0x1c000, 0x1, "C:\\Windows\\system32\\rasman.dll"},
         {0x000007fef3eb0000, 0xac000, 0x1, "C:\\Windows\\system32\\WWanMM.dll"},
         {0x000007fef39d0000, 0xcf000, 0x1, "C:\\Windows\\system32\\WlanMM.dll"},
         {0x000007fef48f0000, 0x21000, 0x1, "C:\\Windows\\system32\\wlanhlp.dll"},
         {0x000007fef4be0000, 0x40000, 0x1, "C:\\Windows\\system32\\OneX.DLL"},
         {0x000007fef48d0000, 0x14000, 0x1, "C:\\Windows\\system32\\eappprxy.dll"},
         {0x000007fef3fc0000, 0x44000, 0x1, "C:\\Windows\\system32\\eappcfg.dll"},
         {0x000007fefc880000, 0x12000, 0x3, "C:\\Windows\\system32\\devrtl.DLL"},
         {0x000007fef2cf0000, 0x1b0000, 0x2, "C:\\Windows\\system32\\comsvcs.dll"},
         {0x000007fef38f0000, 0xd7000, 0x3, ""},
         {0x000007fef4760000, 0x148000, 0x1, ""},
         {0x000007fef40c0000, 0x5ff000, 0x1, "C:\\Windows\\System32\\NLSData0009.dll"},
         {0x00000000714d0000, 0x283000, 0x1, ""},
         {0x000007fefb090000, 0xc000, 0x1, "C:\\Windows\\system32\\dsrole.dll"},
         {0x000007fefbfb0000, 0x9000, 0x3, "C:\\Windows\\system32\\msiltcfg.dll"},
         {0x000007fef3000000, 0x316000, 0x1, ""},
         {0x000007fef2eb0000, 0x24000, 0x1, "C:\\Windows\\System32\\shacct.dll"},
         {0x000007fef6b90000, 0x238000, 0x1, "C:\\Windows\\system32\\tquery.dll"},
         {0x00000000751e0000, 0x3000, 0x1, "C:\\Windows\\system32\\SFC.DLL"},
         {0x000007fef78e0000, 0x10000, 0x2, "C:\\Windows\\system32\\sfc_os.DLL"},
     }},
    {308,
     {
         {0x00000000ff100000, 0xb000, 0xffff, "C:\\Windows\\system32\\svchost.exe"},
         {0x00000000775c0000, 0x1a9000, 0xffff, "C:\\Windows\\SYSTEM32\\ntdll.dll"},
         {0x00000000774a0000, 0x11f000, 0xffff, "C:\\Windows\\system32\\kernel32.dll"},
         {0x000007fefd770000, 0x6b000, 0xffff, "C:\\Windows\\system32\\KERNELBASE.dll"},
         {0x000007fefeeb0000, 0x9f000, 0xffff, "C:\\Windows\\system32\\msvcrt.dll"},
         {0x000007feff360000, 0x1f000, 0xffff, "C:\\Windows\\SYSTEM32\\sechost.dll"},
         {0x000007feff5e0000, 0x12d000, 0xffff, "C:\\Windows\\system32\\RPCRT4.dll"},
         {0x000007fefeae0000, 0x203000, 0x13, "C:\\Windows\\system32\\ole32.dll"},
         {0x000007feff7e0000, 0x67000, 0x63, "C:\\Windows\\system32\\GDI32.dll"},
         {0x00000000773a0000, 0xfa000, 0x6c, "C:\\Windows\\system32\\USER32.dll"},
         {0x000007fefed90000, 0xe000, 0x17, "C:\\Windows\\system32\\LPK.dll"},
         {0x000007feff030000, 0xc9000, 0x17, "C:\\Windows\\system32\\USP10.dll"},
         {0x000007feff7b0000, 0x2e000, 0x2, "C:\\Windows\\system32\\IMM32.DLL"},
         {0x000007fefeda0000, 0x109000, 0x1, "C:\\Windows\\system32\\MSCTF.dll"},
         {0x000007fefd3e0000, 0xf000, 0x3, "C:\\Windows\\system32\\CRYPTBASE.dll"},
     }},
    {1180, {}},
    {1216, {}},
    {1236, {}},
    {1456, {}},
    {1760,
     {
         {0x00000000ffca0000, 0x94000, 0xffff,
          "C:\\Windows\\system32\\SearchIndexer.exe"},
         {0x00000000775c0000, 0x1a9000, 0xffff, "C:\\Windows\\SYSTEM32\\ntdll.dll"},
         {0x00000000774a0000, 0x11f000, 0xffff, "C:\\Windows\\system32\\kernel32.dll"},
         {0x000007fefd770000, 0x6b000, 0xffff, "C:\\Windows\\system32\\KERNELBASE.dll"},
         {0x000007fefea00000, 0xdb000, 0xffff, "C:\\Windows\\system32\\ADVAPI32.dll"},
         {0x000007fefeeb0000, 0x9f000, 0xffff, "C:\\Windows\\system32\\msvcrt.dll"},
         {0x000007feff360000, 0x1f000, 0xffff, "C:\\Windows\\SYSTEM32\\sechost.dll"},
         {0x000007feff5e0000, 0x12d000, 0xffff, "C:\\Windows\\system32\\RPCRT4.dll"},
         {0x00000000773a0000, 0xfa000, 0xffff, "C:\\Windows\\system32\\USER32.dll"},
         {0x000007feff7e0000, 0x67000, 0xffff, "C:\\Windows\\system32\\GDI32.dll"},
         {0x000007fefed90000, 0xe000, 0xffff, "C:\\Windows\\system32\\LPK.dll"},
         {0x000007feff030000, 0xc9000, 0xffff, "C:\\Windows\\system32\\USP10.dll"},
         {0x000007fefeae0000, 0x203000, 0xffff, "C:\\Windows\\system32\\ole32.dll"},
         {0x000007fefef50000, 0xd7000, 0xffff, "C:\\Windows\\system32\\OLEAUT32.dll"},
         {0x000007fef6b90000, 0x238000, 0xffff, "C:\\Windows\\system32\\TQUERY.DLL"},
         {0x000007feff850000, 0x71000, 0xffff, "C:\\Windows\\system32\\SHLWAPI.dll"},
         {0x000007fef6960000, 0x223000, 0xffff, "C:\\Windows\\system32\\MSSRCH.DLL"},
         {0x000007fef66e0000, 0x27a000, 0xffff, "C:\\Windows\\system32\\ESENT.dll"},
         {0x000007feff7b0000, 0x2e000, 0xffff, "C:\\Windows\\system32\\IMM32.dll"},
         {0x000007fefeda0000, 0x109000, 0xffff, "C:\\Windows\\system32\\MSCTF.dll"},
         {0x0000000077790000, 0x7000, 0x1, "C:\\Windows\\system32\\psapi.dll"},
         {0x000007fefda90000, 0xd88000, 0xb, "C:\\Windows\\system32\\SHELL32.dll"},
         {0x000007fefd510000, 0xf000, 0x2, "C:\\Windows\\system32\\profapi.dll"},
         {0x000007fefd3e0000, 0xf000, 0x2, "C:\\Windows\\system32\\CRYPTBASE.dll"},
         {0x000007fefd380000, 0xb000, 0x1, "C:\\Windows\\system32\\secur32.dll"},
         {0x000007fefd3b0000, 0x25000, 0x3, "C:\\Windows\\system32\\SSPICLI.DLL"},
         {0x000007fefc970000, 0xa000, 0x1, "C:\\Windows\\system32\\credssp.dll"},
         {0x000007feff710000, 0x99000, 0x1, "C:\\Windows\\system32\\CLBCatQ.DLL"},
         {0x000007fef64f0000, 0x7000, 0x1, "C:\\Windows\\system32\\Msidle.dll"},
         {0x000007fefcda0000, 0x17000, 0x1, "C:\\Windows\\system32\\CRYPTSP.dll"},
         {0x000007fefcaa0000, 0x47000, 0x1, "C:\\Windows\\system32\\rsaenh.dll"},
         {0x000007fefd4f0000, 0x14000, 0x1, "C:\\Windows\\system32\\RpcRtRemote.dll"},
         {0x000007fef6280000, 0x1d000, 0x1, ""},
         {0x000007fefbe60000, 0x12c000, 0x5, ""},
         {0x000007fefa090000, 0x31000, 0x1, ""},
         {0x000007fefaf90000, 0x2d000, 0x1, "C:\\Windows\\system32\\ntmarta.dll"},
         {0x000007feff300000, 0x52000, 0x1, "C:\\Windows\\system32\\WLDAP32.dll"},
         {0x000007fef7e30000, 0x1b0000, 0x1, "C:\\Windows\\system32\\VSSAPI.DLL"},
         {0x000007fefb220000, 0x19000, 0x1, "C:\\Windows\\system32\\ATL.DLL"},
         {0x000007fef84b0000, 0x17000, 0x2, "C:\\Windows\\system32\\VssTrace.DLL"},
         {0x000007fefb6b0000, 0x14000, 0x1, "C:\\Windows\\system32\\samcli.dll"},
         {0x000007fefbf90000, 0x1d000, 0x1, "C:\\Windows\\system32\\SAMLIB.dll"},
         {0x000007fefb6f0000, 0xc000, 0x1, "C:\\Windows\\system32\\netutils.dll"},
         {0x000007fefb000000, 0x67000, 0x1, "C:\\Windows\\system32\\es.dll"},
         {0x000007fefd5c0000, 0x36000, 0x5, "C:\\Windows\\system32\\CFGMGR32.dll"},
         {0x000007fefb830000, 0x11000, 0x1, ""},
         {0x000007fefd2a0000, 0x3d000, 0x1, ""},
         {0x000007fefc860000, 0x1e000, 0x1, ""},
         {0x000007fefd3f0000, 0x91000, 0x1, ""},
         {0x000007fefd490000, 0x57000, 0xffff, ""},
         {0x000007fef4760000, 0x148000, 0x7, ""},
         {0x000007fefd600000, 0x167000, 0x7, ""},
         {0x000007fefd5b0000, 0xf000, 0x7, ""},
         {0x000007fefbfe0000, 0x1f4000, 0x2, ""},
         {0x000007fefe820000, 0x1d7000, 0x1, "C:\\Windows\\system32\\SETUPAPI.dll"},
         {0x000007fefd7e0000, 0x1a000, 0x1, "C:\\Windows\\system32\\DEVOBJ.dll"},
         {0x000007fefb450000, 0xf000, 0x1, ""},
         {0x000007fef49e0000, 0xa3000, 0x1, ""},
         {0x000007fef2800000, 0x14000, 0x1, ""},
         {0x000007fef3650000, 0x1d4000, 0x1, ""},
         {0x000007fef40c0000, 0x5ff000, 0x1, "\u45c0\u0f13"},
         // ^^^ This needs more debugging
         {0x00000000714d0000, 0x283000, 0x1,
          "C:\\Windows\\System32\\NLSLexicons0009.dll"},
     }},
    {1648, {}},
    {1808, {}},
    {1652,
     {
         {0x0000000001200000, 0xa6000, 0xffff,
          "C:\\Program Files (x86)\\Internet Explorer\\iexplore.exe"},
         {0x00000000775c0000, 0x1a9000, 0xffff, "C:\\Windows\\SYSTEM32\\ntdll.dll"},
         {0x0000000074700000, 0x3f000, 0x3, "C:\\Windows\\SYSTEM32\\wow64.dll"},
         {0x00000000746a0000, 0x5c000, 0x1, "C:\\Windows\\SYSTEM32\\wow64win.dll"},
         {0x0000000074690000, 0x8000, 0x1, "C:\\Windows\\SYSTEM32\\wow64cpu.dll"},
     }},
    {1880, {}},
    {1524, {}},
    {2060, {}},
    {2920, {}},
    {2824,
     {
         {0x000000013fc70000, 0x1e000, 0xffff, "C:\\Windows\\ehome\\ehshell.exe"},
         {0x00000000775c0000, 0x1a9000, 0xffff, "C:\\Windows\\SYSTEM32\\ntdll.dll"},
         {0x00000000774a0000, 0x11f000, 0xffff, "C:\\Windows\\system32\\kernel32.dll"},
         {0x000007fefd770000, 0x6b000, 0xffff, "C:\\Windows\\system32\\KERNELBASE.dll"},
         {0x000007fefeeb0000, 0x9f000, 0xffff, "C:\\Windows\\system32\\msvcrt.dll"},
         {0x000007fefeae0000, 0x203000, 0xffff, "C:\\Windows\\system32\\ole32.dll"},
         {0x000007feff7e0000, 0x67000, 0xffff, "C:\\Windows\\system32\\GDI32.dll"},
         {0x00000000773a0000, 0xfa000, 0xffff, "C:\\Windows\\system32\\USER32.dll"},
         {0x000007fefed90000, 0xe000, 0xffff, "C:\\Windows\\system32\\LPK.dll"},
         {0x000007feff030000, 0xc9000, 0xffff, "C:\\Windows\\system32\\USP10.dll"},
         {0x000007feff5e0000, 0x12d000, 0xffff, "C:\\Windows\\system32\\RPCRT4.dll"},
         {0x000007fefef50000, 0xd7000, 0xffff, "C:\\Windows\\system32\\OLEAUT32.dll"},
         {0x000007feff850000, 0x71000, 0xffff, "C:\\Windows\\system32\\SHLWAPI.dll"},
         {0x000007fef34c0000, 0x6f000, 0xffff, "C:\\Windows\\system32\\mscoree.dll"},
         {0x000007fefea00000, 0xdb000, 0xffff, "C:\\Windows\\system32\\ADVAPI32.dll"},
         {0x000007feff360000, 0x1f000, 0xffff, "C:\\Windows\\SYSTEM32\\sechost.dll"},
         {0x000007feff7b0000, 0x2e000, 0x6, "C:\\Windows\\system32\\IMM32.DLL"},
         {0x000007fefeda0000, 0x109000, 0x3, "C:\\Windows\\system32\\MSCTF.dll"},
         {0x000007fefd3e0000, 0xf000, 0x3, "C:\\Windows\\system32\\CRYPTBASE.dll"},
         {0x000007fefbe00000, 0x56000, 0x2, "C:\\Windows\\system32\\uxtheme.dll"},
         {0x000007fef1e50000, 0x99d000, 0x1,
          "C:\\Windows\\Microsoft.NET\\Framework64\\v2.0.50727\\mscorwks.dll"},
         {0x00000000700a0000, 0xc9000, 0x2,
          "C:\\Windows\\WinSxS\\amd64_microsoft.vc80."
          "crt_1fc8b3b9a1e18e3b_8.0.50727.4940_none_"
          "88df89932faf0bf6\\MSVCR80.dll"},
         {0x000007fefda90000, 0xd88000, 0xb, "C:\\Windows\\system32\\shell32.dll"},
         {0x000007fefd510000, 0xf000, 0x4, "C:\\Windows\\system32\\profapi.dll"},
         {0x000007fef0f70000, 0xedc000, 0x1,
          "C:\\Windows\\assembly\\NativeImages_v2.0.50727_"
          "64\\mscorlib\\9469491f37d9c35b596968b206615309\\mscorlib.ni.dll"},
         {0x000007feff710000, 0x99000, 0x1, "C:\\Windows\\system32\\CLBCatQ.DLL"},
         {0x000007fefd3f0000, 0x91000, 0x1, "C:\\Windows\\system32\\sxs.dll"},
         {0x000007fef0540000, 0xa23000, 0x1,
          "C:\\Windows\\assembly\\NativeImages_v2.0.50727_"
          "64\\System\\adff7dd9fe8e541775c46b6363401b22\\System.ni.dll"},
         {0x000007fef2c00000, 0xf0000, 0x1,
          "C:\\Windows\\assembly\\NativeImages_v2.0.50727_64\\Microsoft.MediaCent#"
          "\\dc34242bf840d340e94d2657c7c33371\\Microsoft.MediaCenter.Sports.ni.dll"},
         {0x000007fef2a80000, 0x176000, 0x1,
          "C:\\Windows\\assembly\\NativeImages_v2.0.50727_64\\Microsoft.MediaCent#"
          "\\9ae837dc03e8519b40fe2c35c8752146\\Microsoft.MediaCenter.ni.dll"},
         {0x000007feefca0000, 0x895000, 0x1,
          "C:\\Windows\\assembly\\NativeImages_v2.0.50727_64\\Microsoft.MediaCent#"
          "\\618ab8996b43e841efdcfb273393fc02\\Microsoft.MediaCenter.UI.ni.dll"},
         {0x000007feefb80000, 0x11a000, 0x1,
          "C:\\Windows\\assembly\\NativeImages_v2.0.50727_64\\Microsoft.MediaCent#"
          "\\1f517ecba89b0f399021bdbc8fb3db82\\Microsoft.MediaCenter.Shell.ni.dll"},
         {0x000007feee330000, 0x184f000, 0x1,
          "C:\\Windows\\assembly\\NativeImages_v2.0.50727_"
          "64\\ehshell\\d1dc67c666bc15291be843bd67cd2a2e\\ehshell.ni.dll"},
         {0x000007feee080000, 0x2b0000, 0x1,
          "C:\\Windows\\assembly\\NativeImages_v2.0.50727_"
          "64\\mcstore\\67c2902f53638a9056174f6130a8bde7\\mcstore.ni.dll"},
         {0x000007fefcda0000, 0x17000, 0x2, "C:\\Windows\\system32\\CRYPTSP.dll"},
         {0x000007fefcaa0000, 0x47000, 0x1, "C:\\Windows\\system32\\rsaenh.dll"},
         {0x000007fefd4f0000, 0x14000, 0x1, "C:\\Windows\\system32\\RpcRtRemote.dll"},
         {0x000007feedf50000, 0x128000, 0x1, ""},
         {0x000007feedd50000, 0x1ff000, 0x1, "C:\\Windows\\system32\\d3d9.dll"},
         {0x000007fefc670000, 0xc000, 0x4, "C:\\Windows\\system32\\VERSION.dll"},
         {0x000007fefa770000, 0x7000, 0x1, "C:\\Windows\\system32\\d3d8thk.dll"},
         {0x000007fefb9d0000, 0x18000, 0x3, "C:\\Windows\\system32\\dwmapi.dll"},
         {0x000007fefab40000, 0xa7000, 0x1, "C:\\Windows\\system32\\dxgi.dll"},
         {0x000007feedcc0000, 0x88000, 0x2, "C:\\Windows\\system32\\DSOUND.dll"},
         {0x000007fef9820000, 0x3b000, 0x5, "C:\\Windows\\system32\\WINMM.dll"},
         {0x000007fefb480000, 0x2c000, 0x4, "C:\\Windows\\system32\\POWRPROF.dll"},
         {0x000007fefe820000, 0x1d7000, 0x9, "C:\\Windows\\system32\\SETUPAPI.dll"},
         {0x000007fefd5c0000, 0x36000, 0x45, "C:\\Windows\\system32\\CFGMGR32.dll"},
         {0x000007fefd7e0000, 0x1a000, 0x8, "C:\\Windows\\system32\\DEVOBJ.dll"},
         {0x000007fefbbe0000, 0x215000, 0x4,
          "C:\\Windows\\WinSxS\\amd64_microsoft."
          "windows.gdiplus_6595b64144ccf1df_1.1.7601."
          "17514_none_2b24536c71ed437a\\gdiplus.dll"},
         {0x000007fefa750000, 0x7000, 0x1, "C:\\Windows\\system32\\MSIMG32.dll"},
         {0x000007fefd8e0000, 0x4d000, 0x5, "C:\\Windows\\system32\\WS2_32.dll"},
         {0x000007fefda80000, 0x8000, 0xb, ""},
         {0x000007fefb830000, 0x11000, 0x5, "C:\\Windows\\system32\\WTSAPI32.dll"},
         {0x000007fef3480000, 0x32000, 0x2, "C:\\Windows\\ehome\\ehtrace.dll"},
         {0x000007fef6180000, 0xc6000, 0x1, "C:\\Windows\\system32\\msftedit.DLL"},
         {0x000007fef28e0000, 0x19b000, 0x2, "C:\\Windows\\ehome\\ehuihlp.dll"},
         {0x000007fefb220000, 0x19000, 0x3, ""},
         {0x000007fefb080000, 0xb000, 0x2, ""},
         {0x000007fef4c80000, 0x27000, 0x2, "C:\\Windows\\system32\\SPPC.DLL"},
         {0x000007fef2840000, 0xa0000, 0x2, ""},
         {0x000007fefb700000, 0x16000, 0x3, "C:\\Windows\\system32\\NETAPI32.dll"},
         {0x000007fefb6f0000, 0xc000, 0x8, "C:\\Windows\\system32\\netutils.dll"},
         {0x000007fefd2e0000, 0x23000, 0x5, "C:\\Windows\\system32\\srvcli.dll"},
         {0x000007fefb6d0000, 0x15000, 0x3, "C:\\Windows\\system32\\wkscli.dll"},
         {0x000007fefd2a0000, 0x3d000, 0x4, "C:\\Windows\\system32\\WINSTA.dll"},
         {0x000007fefb730000, 0x71000, 0x2, "C:\\Windows\\system32\\WINSPOOL.DRV"},
         {0x000007fefd800000, 0x3a000, 0x4, "C:\\Windows\\system32\\WINTRUST.dll"},
         {0x000007fefd600000, 0x167000, 0x8, "C:\\Windows\\system32\\CRYPT32.dll"},
         {0x000007fefd5b0000, 0xf000, 0x9, "C:\\Windows\\system32\\MSASN1.dll"},
         {0x000007fefafd0000, 0x27000, 0x3, "C:\\Windows\\system32\\IPHLPAPI.DLL"},
         {0x000007fefafc0000, 0xb000, 0x3, "C:\\Windows\\system32\\WINNSI.DLL"},
         {0x000007fefaf90000, 0x2d000, 0x1, "C:\\Windows\\system32\\ntmarta.dll"},
         {0x000007feff300000, 0x52000, 0x1, "C:\\Windows\\system32\\WLDAP32.dll"},
         {0x000000006eb70000, 0x1528000, 0x2, "C:\\Windows\\ehome\\ehres.dll"},
         {0x000007feecaa0000, 0x6a5000, 0x1,
          "C:\\Windows\\assembly\\NativeImages_v2.0.50727_64\\System."
          "Xml\\ee795155543768ea67eecddc686a1e9e\\System.Xml.ni.dll"},
         {0x000007fefd380000, 0xb000, 0x1, "C:\\Windows\\system32\\Secur32.dll"},
         {0x000007fefd3b0000, 0x25000, 0x2, "C:\\Windows\\system32\\SSPICLI.DLL"},
         {0x000007fefb6b0000, 0x14000, 0x3, "C:\\Windows\\system32\\samcli.dll"},
         {0x000007fefbf90000, 0x1d000, 0x1, "C:\\Windows\\system32\\SAMLIB.dll"},
         {0x000007fefb9f0000, 0x4b000, 0x2, "C:\\Windows\\System32\\MMDevApi.dll"},
         {0x000007fefbe60000, 0x12c000, 0x4, "C:\\Windows\\System32\\PROPSYS.dll"},
         {0x000007feec8d0000, 0x7000, 0x1, "C:\\Windows\\system32\\shfolder.dll"},
         {0x000007fef7520000, 0x42000, 0x1, "C:\\Windows\\system32\\sqmapi.dll"},
         {0x000007feec740000, 0x184000, 0x1, ""},
         {0x000007feec6b0000, 0x85000, 0x1, ""},
         {0x00000000747c0000, 0x27000, 0x1,
          "C:\\Windows\\assembly\\GAC_64\\mcstoredb\\6."
          "1.0.0__31bf3856ad364e35\\mcstoredb.dll"},
         {0x000007fef6030000, 0x62000, 0x1, ""},
         {0x000007fefa030000, 0x1c000, 0x1, ""},
         {0x000007feec580000, 0x12a000, 0x1, ""},
         {0x000007feec540000, 0x33000, 0x1, ""},
         {0x000007feec4c0000, 0x74000, 0x1, ""},
         {0x000007feec3e0000, 0xd1000, 0x1, ""},
         {0x000007fefc860000, 0x1e000, 0x3, ""},
         {0x000007feebff0000, 0x3ea000, 0x1,
          "C:\\Windows\\assembly\\NativeImages_v2.0.50727_"
          "64\\mcepg\\13b4ad00d1167ff3ed7d2a8e4994f1ff\\mcepg.ni.dll"},
         {0x000007feebf80000, 0x64000, 0x1,
          "C:\\Windows\\assembly\\NativeImages_v2.0.50727_64\\System.Runtime.Seri#"
          "\\8ad0e1382ab6565741bbb64b965f2748\\System.Runtime.Serialization.Formatters."
          "Soap.ni.dll"},
     }},
    {2872, {}},
    {2808, {}},
    {3040, {}},
    {2900, {}},
    {2140,
     {
         {0x000000001ce80000, 0xc000, 0xffff, "C:\\Python27\\python.exe"},
         {0x00000000775c0000, 0x1a9000, 0xffff, "C:\\Windows\\SYSTEM32\\ntdll.dll"},
         {0x00000000774a0000, 0x11f000, 0xffff, "C:\\Windows\\system32\\kernel32.dll"},
         {0x000007fefd770000, 0x6b000, 0xffff, "C:\\Windows\\system32\\KERNELBASE.dll"},
         {0x000000006d870000, 0x368000, 0xffff, "C:\\Windows\\system32\\python27.dll"},
         {0x00000000773a0000, 0xfa000, 0xffff, "C:\\Windows\\system32\\USER32.dll"},
         {0x000007feff7e0000, 0x67000, 0xffff, "C:\\Windows\\system32\\GDI32.dll"},
         {0x000007fefed90000, 0xe000, 0xffff, "C:\\Windows\\system32\\LPK.dll"},
         {0x000007feff030000, 0xc9000, 0xffff, "C:\\Windows\\system32\\USP10.dll"},
         {0x000007fefeeb0000, 0x9f000, 0xffff, "C:\\Windows\\system32\\msvcrt.dll"},
         {0x000007fefea00000, 0xdb000, 0xffff, "C:\\Windows\\system32\\ADVAPI32.dll"},
         {0x000007feff360000, 0x1f000, 0xffff, "C:\\Windows\\SYSTEM32\\sechost.dll"},
         {0x000007feff5e0000, 0x12d000, 0xffff, "C:\\Windows\\system32\\RPCRT4.dll"},
         {0x000007fefda90000, 0xd88000, 0xffff, "C:\\Windows\\system32\\SHELL32.dll"},
         {0x000007feff850000, 0x71000, 0xffff, "C:\\Windows\\system32\\SHLWAPI.dll"},
         {0x000000006e630000, 0x9d000, 0xffff,
          "C:\\Windows\\WinSxS\\amd64_microsoft."
          "vc90.crt_1fc8b3b9a1e18e3b_9.0.30729.4940_"
          "none_08e4299fa83d7e3c\\MSVCR90.dll"},
         {0x000007feff7b0000, 0x2e000, 0x2, "C:\\Windows\\system32\\IMM32.DLL"},
         {0x000007fefeda0000, 0x109000, 0x1, "C:\\Windows\\system32\\MSCTF.dll"},
         {0x000007feeb4b0000, 0x170000, 0x1, "C:\\Python27\\DLLs\\_hashlib.pyd"},
         {0x000007fefcda0000, 0x17000, 0x1, "C:\\Windows\\system32\\CRYPTSP.dll"},
         {0x000007fefcaa0000, 0x47000, 0x1, "C:\\Windows\\system32\\rsaenh.dll"},
         {0x000007fefd3e0000, 0xf000, 0x1, "C:\\Windows\\system32\\CRYPTBASE.dll"},
     }},
    {2024,
     {
         {0x00000000ffba0000, 0x57000, 0xffff, "C:\\Windows\\system32\\conhost.exe"},
         {0x00000000775c0000, 0x1a9000, 0xffff, "C:\\Windows\\SYSTEM32\\ntdll.dll"},
         {0x00000000774a0000, 0x11f000, 0xffff, "C:\\Windows\\system32\\kernel32.dll"},
         {0x000007fefd770000, 0x6b000, 0xffff, "C:\\Windows\\system32\\KERNELBASE.dll"},
         {0x000007feff7e0000, 0x67000, 0xffff, "C:\\Windows\\system32\\GDI32.dll"},
         {0x00000000773a0000, 0xfa000, 0xffff, "C:\\Windows\\system32\\USER32.dll"},
         {0x000007fefed90000, 0xe000, 0xffff, "C:\\Windows\\system32\\LPK.dll"},
         {0x000007feff030000, 0xc9000, 0xffff, "C:\\Windows\\system32\\USP10.dll"},
         {0x000007fefeeb0000, 0x9f000, 0xffff, "C:\\Windows\\system32\\msvcrt.dll"},
         {0x000007feff7b0000, 0x2e000, 0xffff, "C:\\Windows\\system32\\IMM32.dll"},
         {0x000007fefeda0000, 0x109000, 0xffff, "C:\\Windows\\system32\\MSCTF.dll"},
         {0x000007fefeae0000, 0x203000, 0xffff, "C:\\Windows\\system32\\ole32.dll"},
         {0x000007feff5e0000, 0x12d000, 0xffff, "C:\\Windows\\system32\\RPCRT4.dll"},
         {0x000007fefef50000, 0xd7000, 0xffff, "C:\\Windows\\system32\\OLEAUT32.dll"},
         {0x000007fefbe00000, 0x56000, 0x3, "C:\\Windows\\system32\\uxtheme.dll"},
         {0x000007fefb9d0000, 0x18000, 0x1, "C:\\Windows\\system32\\dwmapi.dll"},
         {0x000007fefea00000, 0xdb000, 0x1, "C:\\Windows\\system32\\ADVAPI32.dll"},
         {0x000007feff360000, 0x1f000, 0x4, "C:\\Windows\\SYSTEM32\\sechost.dll"},
         {0x000007fefbfe0000, 0x1f4000, 0x1,
          "C:\\Windows\\WinSxS\\amd64_microsoft.windows.common-controls_6595b64144ccf1df_"
          "6.0.7601.17514_none_fa396087175ac9ac\\comctl32.DLL"},
         {0x000007feff850000, 0x71000, 0x1, "C:\\Windows\\system32\\SHLWAPI.dll"},
         {0x000007fefd3e0000, 0xf000, 0x1, "C:\\Windows\\system32\\CRYPTBASE.dll"},
     }}};

bool find_match(struct WindowsModuleEntry* me, std::vector<struct ModuleInfo>& mi)
{
    uint64_t base_addr = module_entry_get_base_address(me);
    for (auto& entry : mi) {
        if (base_addr == entry.base) {
            EXPECT_EQ(module_entry_get_modulesize(me), entry.size)
                << "ModuleSize mismatch";
            EXPECT_EQ(module_entry_get_loadcount(me), entry.loadcount)
                << "LoadCount mismatch";
            EXPECT_EQ(std::string(module_entry_get_dllpath(me)), entry.path)
                << "dllpath mismatch";
            return true;
        }
    }
    return false;
}

void handle_proces_modlist(struct WindowsKernelOSI* wintro, struct WindowsProcess* p)
{
    auto pid = process_get_pid(p);
    auto candidate = EXPECTED_RESULTS.find(pid);
    ASSERT_TRUE(candidate != EXPECTED_RESULTS.end()) << "Failed to find PID";
    auto& entry = candidate->second;

    uint32_t module_count = 0;
    auto modlist = get_module_list(wintro, process_get_eprocess(p), process_is_wow64(p));
    if (modlist) {
        auto me = module_list_next(modlist);
        while (me) {
            EXPECT_TRUE(find_match(me, entry))
                << "Did not find a match for " << module_entry_get_base_address(me);
            module_count++;
            free_module_entry(me);
            me = module_list_next(modlist);
        }
    } else {
        ASSERT_TRUE(entry.size() == 0)
            << "Didn't find a module list where one was expected. PID: " << pid;
    }
    fprintf(stderr, "%u vs %lu\n", module_count, entry.size());
    ASSERT_EQ(module_count, entry.size())
        << "Found an unexpected number of modules for PID: " << pid;
    free_module_list(modlist);
}

TEST(TestAmd64Plist, Win7SP1Amd64)
{
    ASSERT_TRUE(testfile) << "Couldn't load input test file!";
    ASSERT_TRUE(access(testfile, R_OK) == 0) << "Could not read input file";

    WindowsKernelManager manager = WindowsKernelManager("windows-64-7sp1");

    auto pmem = load_physical_memory_snapshot(testfile);
    ASSERT_TRUE(pmem != nullptr) << "failed to load physical memory snapshot";

    struct WindowsKernelOSI* kosi = manager.get_kernel_object();

    ASSERT_TRUE(manager.initialize(pmem, 8, 0x1c55a000, 0xfffff8000284cd00))
        << "Failed to initialize kernel osi";

    auto plist = get_process_list(kosi);
    ASSERT_TRUE(plist != nullptr) << "Failed to get process list";

    for (unsigned int ix = 0; ix < EXPECTED_RESULTS.size(); ++ix) {
        auto process = process_list_next(plist);
        ASSERT_TRUE(process != nullptr) << "Didn't find enough processes";
        handle_proces_modlist(kosi, process);
        free_process(process);
    }
    ASSERT_TRUE(process_list_next(plist) == nullptr) << "Found too many processes";

    free_process_list(plist);

    pmem->free(pmem);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if (argc != 2) {
        fprintf(stderr, "usage: %s amd64.raw\n", argv[0]);
        return 3;
    }

    testfile = argv[1];

    return RUN_ALL_TESTS();
}
