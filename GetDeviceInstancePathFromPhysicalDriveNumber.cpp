// https://social.msdn.microsoft.com/Forums/vstudio/en-US/d0504fce-08e6-463d-974e-c83b5d0b9de8/how-to-get-the-device-instance-path-from-drive-letter?forum=vcgeneral
// How to get the Device Instance Path from drive letter


#define UNICODE 1
#define _UNICODE 1

/* The code of interest is in the subroutine GetDriveGeometry. The
   code in main shows how to interpret the results of the call. */

#include <windows.h>
#include <winerror.h>
#include <winioctl.h>
#include <setupapi.h>
#include <strsafe.h>
#include <stdio.h>
   // Add to get parent property S.Asai start
#include <initguid.h>
#include <Devpkey.h>
// Add to get parent property S.Asai end
#pragma	comment(lib, "setupapi.lib")

// Add for MAX_DEVICE_ID_LEN S.Asai start
#include <cfgmgr32.h>
// Add for MAX_DEVICE_ID_LEN S.Asai end

// Add for CString  S.Asai start
#include "atlstr.h"
// Add for CString  S.Asai end


HRESULT ResultFromKnownLastError()
{

	HRESULT hr = HRESULT_FROM_WIN32(GetLastError());

	return (SUCCEEDED(hr) ? E_FAIL : hr);

}

// Add Param4 to get parent property S.Asai
HRESULT PhysicalDriveNumberToDeviceInstancePath(__in int num, __out LPTSTR pszDeviceInstancePath, __in size_t cchDest, __out CString& strParentProperty)
{
	HRESULT hr;

	// Just in case the caller doesn't initialize his string and forgets also to check for return code
	*pszDeviceInstancePath = '\0';

	// Re-open handle as volume from drive letter
	WCHAR szPhysicalDrivePath[128];

	wsprintf(szPhysicalDrivePath, L"\\\\.\\PhysicalDrive%d", num);


	HANDLE hDrive = CreateFile(szPhysicalDrivePath, FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	hr = (INVALID_HANDLE_VALUE != hDrive) ? S_OK : ResultFromKnownLastError();

	STORAGE_DEVICE_NUMBER storageDeviceNumber = {};

	if (SUCCEEDED(hr))
	{
		DWORD dwByteReturned = 0;
		hr = (DeviceIoControl(hDrive,
			IOCTL_STORAGE_GET_DEVICE_NUMBER,
			NULL,
			0,
			&storageDeviceNumber,
			(DWORD)sizeof(STORAGE_DEVICE_NUMBER),
			&dwByteReturned,
			NULL)) ? S_OK : ResultFromKnownLastError();
	}

	if (SUCCEEDED(hr))
	{
		hr = (storageDeviceNumber.DeviceType == FILE_DEVICE_DISK) ? S_OK : E_FAIL;
	}

	HDEVINFO hdevInfo = INVALID_HANDLE_VALUE;

	if (SUCCEEDED(hr))
	{
		hdevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_DISK,
			nullptr,
			NULL,
			DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
		hr = (hdevInfo != INVALID_HANDLE_VALUE) ? S_OK : ResultFromKnownLastError();
	}

	for (DWORD ndx = 0; SUCCEEDED(hr); ndx++)
	{
		SP_DEVICE_INTERFACE_DATA devIntfData;
		ZeroMemory(&devIntfData, sizeof(devIntfData));

		devIntfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		hr = (SetupDiEnumDeviceInterfaces(hdevInfo,
			nullptr,
			&GUID_DEVINTERFACE_DISK,
			ndx,
			&devIntfData)) ? S_OK : ResultFromKnownLastError();

		if (SUCCEEDED(hr))
		{
			DWORD reqSize = 0;

			// This next call is expected to fail with ERROR_INSUFFICIENT_BUFFER

			hr = (SetupDiGetDeviceInterfaceDetail(hdevInfo,
				&devIntfData,
				nullptr,
				0,
				&reqSize,
				nullptr)) ? E_UNEXPECTED : ResultFromKnownLastError();

			if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
			{

				PSP_DEVICE_INTERFACE_DETAIL_DATA pDiDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR, reqSize);

				hr = (pDiDetail != nullptr) ? S_OK : E_OUTOFMEMORY; // bail out of loop on out of memory failure

				if (SUCCEEDED(hr))
				{
					pDiDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

					SP_DEVINFO_DATA devInfoData;

					ZeroMemory(&devInfoData, sizeof(devInfoData));

					devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

					if (SetupDiGetDeviceInterfaceDetail(hdevInfo,
						&devIntfData,
						pDiDetail,
						reqSize,
						nullptr,
						&devInfoData))
					{
						HANDLE hDevice = CreateFile(pDiDetail->DevicePath,
							FILE_READ_ATTRIBUTES | SYNCHRONIZE,
							FILE_SHARE_READ | FILE_SHARE_WRITE,
							NULL,
							OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL,
							NULL);

						if (hDevice != INVALID_HANDLE_VALUE)
						{
							STORAGE_DEVICE_NUMBER storageDeviceNumber2 = {};
							DWORD dwByteReturned = 0;
							if (DeviceIoControl(hDevice,
								IOCTL_STORAGE_GET_DEVICE_NUMBER,
								NULL,
								0,
								&storageDeviceNumber2,
								(DWORD)sizeof(STORAGE_DEVICE_NUMBER),
								&dwByteReturned,
								NULL))
							{
								// do we have a match?

								if (storageDeviceNumber2.DeviceNumber == storageDeviceNumber.DeviceNumber)
								{
									// Add to get parent property S.Asai start
									strParentProperty = "";
									DEVPROPTYPE PropertyType;
									wchar_t Buffer[MAX_DEVICE_ID_LEN] = {};

									if (SetupDiGetDeviceProperty(hdevInfo, &devInfoData,
										&DEVPKEY_Device_Parent, &PropertyType,
										(PBYTE)Buffer, sizeof(Buffer), NULL, 0))
									{
										if (PropertyType == DEVPROP_TYPE_STRING)
										{
											strParentProperty = Buffer;
										}
									}
									// Add to get parent property S.Asai end

									// we have a match. Close all handles.
									CloseHandle(hDrive);

									hDrive = NULL;

									CloseHandle(hDevice);

									hDevice = NULL;

									StringCchCopy(pszDeviceInstancePath, cchDest, pDiDetail->DevicePath);

									/* if pDiDetail->DevicePath doesn't work - look at CM_Get_Device_ID instead

									WCHAR DeviceInstId[MAX_DEVICE_ID_LEN] = {0};

									CONFIGRET CmRet = CM_Get_Device_ID(devInfoData.DevInst,
												DeviceInstId,
												ARRAYSIZE(DeviceInstId),
												0);

									hr = (CmRet == CR_SUCCESS) ? S_OK : E_FAIL;

									StringCchCopy(pszDeviceInstancePath, cchDest, DeviceInstId);

									*/

									// Exit the loop with potential success.

									LocalFree(pDiDetail);

									break;
								}

								// not a match, continue iterating

							}

							CloseHandle(hDevice);

						}

					}

					LocalFree(pDiDetail);

				}

			}

		}

	}

	return hr;

}


int main(int argc, wchar_t* argv[])
{


	HRESULT bResult;
	LPTSTR pszDeviceInstancePath;
	size_t cchDest = MAX_DEVICE_ID_LEN;

	wchar_t  buf[MAX_DEVICE_ID_LEN];
	pszDeviceInstancePath = (LPTSTR)buf;

	CString strDeviceInstancePath;
	SHORT   nVendorId;
	SHORT   nProductId;
	CString strVendorId;
	CString strProductId;
	CString striSerialNumber;
	CString strParentProperty;


	for (int i = 0; i < 4; i++)
	{
		ZeroMemory(pszDeviceInstancePath, sizeof(buf));
		strParentProperty = L"";

		bResult = PhysicalDriveNumberToDeviceInstancePath(i, pszDeviceInstancePath, cchDest, strParentProperty);

		if (bResult == S_OK)
		{
			wprintf(L"\\\\.\\PhysicalDrive%d DeviceInstancePath :\n", i);
			wprintf(L"%s\n", pszDeviceInstancePath);
			wprintf(L"Parent device property = %s\n", (LPCTSTR)strParentProperty);

			// 親プロパティよりUSB機器であるかをチェックする
			if (strParentProperty.Left(4) == L"USB\\") {

				// USB機器であるなら親プロパティよりVendor IDとProduct ID
				// iSerial値が含まれた情報を取得する
				wchar_t* pChar = new wchar_t[512];

				// For VendorId
				strVendorId = strParentProperty.Mid(8, 4);
				strVendorId = L"0x" + strVendorId;

				ZeroMemory(pChar, sizeof(pChar));
				wcscpy_s(pChar, sizeof(pChar), strVendorId);

				// 数値化する
				nVendorId = (SHORT)wcstol(pChar, NULL, 16);
				wprintf(L"  VendorId      = 0x%x\n", nVendorId);


				// For ProductId
				strProductId = strParentProperty.Mid(17, 4);
				strProductId = L"0x" + strProductId;

				ZeroMemory(pChar, sizeof(pChar));
				wcscpy_s(pChar, sizeof(pChar), strProductId);

				// 数値化する
				nProductId = (SHORT)wcstol(pChar, NULL, 16);
				wprintf(L"  ProductId     = 0x%x\n", nProductId);


				// For iSerialNumber
				striSerialNumber = strParentProperty.Mid(22, 256);
				wprintf(L"  iSerialNumber = %s\n", (LPCTSTR)striSerialNumber);


				delete[] pChar; // newした場合は忘れずに削除

			}
			wprintf(L"\n");
		}
	}

	return (0);
}
