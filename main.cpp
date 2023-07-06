#define _CRT_SECURE_NO_WARNINGS

#include <cstdlib>
#include <string>
#include <windows.h>
#include <setupapi.h>
#include <shlwapi.h>
#include <newdev.h>
#include <iostream>
#include <conio.h>
#include <algorithm> 
#include <cctype>

#pragma comment (lib, "Setupapi.lib")
#pragma comment (lib, "shlwapi.lib")
#pragma comment (lib, "newdev.lib")

static const std::string installFlag = "--install";

// The following function is from: 
// https://stackoverflow.com/questions/1387064/how-to-get-the-error-message-from-the-error-code-returned-by-getlasterror
static std::string GetLastErrorAsString()
{
	//Get the error message ID, if any.
	DWORD errorMessageID = GetLastError();

	if (errorMessageID == 0) {
		return std::string(); //No error message has been recorded
	}

	LPSTR messageBuffer = nullptr;

	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	size_t size = 
		FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM     | 
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			errorMessageID,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR) &messageBuffer, 
			0,
			NULL);

	//Copy the error message into a std::string.
	std::string message(messageBuffer, size);

	//Free the Win32's string's buffer.
	LocalFree(messageBuffer);
	return message;
}

// The following three trim functions are from:
// https://stackoverflow.com/questions/216823/how-to-trim-an-stdstring
// trim from start (in place)
static void ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
		return !std::isspace(ch);
		}));
}

// trim from end (in place)
static void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
		}).base(), s.end());
}

// trim from both ends (in place)
static void trim(std::string& s) {
	rtrim(s);
	ltrim(s);
}

static wchar_t* ConvertCharStringToWcharString(char* source) {
	std::string s = source;
	trim(s);
	size_t guidLength = s.length();
	wchar_t* target = new wchar_t[guidLength + 1];
	target[guidLength] = L'\0';
	mbstowcs(target, s.c_str(), guidLength);
	return target;
}

static void PromptToExit(std::ostream& out) {
	out << "Press any key to exit.\n";
	_getch();
}

static bool LoadDeviceData(
	char* guidString, 
	HDEVINFO* pDevInfo, 
	PSP_DEVINFO_DATA pData,
	PSP_DRVINFO_DATA pDriverData,
	bool install) {
	
	GUID guid;
	HRESULT hResult =
		CLSIDFromString(
			ConvertCharStringToWcharString(guidString),
			(LPCLSID) &guid);

	if (hResult == CO_E_CLASSSTRING) {
		std::cerr << "ERROR: Bad GUID string: "
			<< GetLastErrorAsString()
			<< "\n";

		PromptToExit(std::cerr);
		return false;
	}

	HDEVINFO hDeviceInfo =
		SetupDiGetClassDevsExA(
			&guid,
			NULL,
			NULL,
			install ? DIGCF_ALLCLASSES : DIGCF_PRESENT,
			NULL,
			NULL,
			NULL);

	if (hDeviceInfo == INVALID_HANDLE_VALUE) {
		std::cerr << "ERROR: Could not obtain HDEVINFO: "
			<< GetLastErrorAsString()
			<< "\n";

		PromptToExit(std::cerr);
		return false;
	}

	*pDevInfo = hDeviceInfo;
	pData->cbSize = sizeof(SP_DEVINFO_DATA);
	pData->ClassGuid = guid;

	BOOL deviceEnumerated =
		SetupDiEnumDeviceInfo(
			hDeviceInfo,
			0,
			pData);

	if (!deviceEnumerated) {
		std::cerr << "ERROR: Could not enumerate the SP_DEVINFO_DATA: "
			<< GetLastErrorAsString()
			<< "\n";

		PromptToExit(std::cerr);
		return false;
	}

	if (install) {
		pDriverData->cbSize = sizeof(SP_DRVINFO_DATA);
		pDriverData->DriverType = SPDIT_CLASSDRIVER;

		BOOL ok =
			SetupDiGetSelectedDriver(
				hDeviceInfo,
				pData,
				pDriverData);
			
		if (!ok) {
			std::cerr << "ERROR: Could not obtain device driver info: "
				      << GetLastErrorAsString()
				      << "\n";

			PromptToExit(std::cerr);
			return false;
		}
	}

	return true;
}

int main(int argc, char* argv[]) {
	if (argc == 1) {
		std::cout << "ERROR: No GUID specified.\n";
		PromptToExit(std::cerr);
		return EXIT_FAILURE;
	}

	if (argc > 3) {
		PathStripPathA(argv[0]);
		std::cout << "Usage: " << argv[0] << " [--install] GUID\n";
		PromptToExit(std::cerr);
		return EXIT_FAILURE;
	}

	bool install = false;
	char* guidParameter = NULL;

	if (argc == 2) {
		guidParameter = argv[1];
	} else if (argc == 3) {
		std::string flagParameter = argv[1];

		if (flagParameter != installFlag) {
			std::cerr << "ERROR: Wrong flag: " << flagParameter << "\n";
			PromptToExit(std::cerr);
			return EXIT_FAILURE;
		}

		guidParameter = argv[2];
		install = true;
	}

	HDEVINFO hDevInfo;
	SP_DEVINFO_DATA deviceData;
	SP_DRVINFO_DATA driverData;

	bool ok = 
		LoadDeviceData(
			guidParameter, 
			&hDevInfo, 
			&deviceData,
			&driverData,
			install);

	if (!ok) {
		return EXIT_FAILURE;
	}

	if (install) {
		BOOL rebootNeeded;

		BOOL installStatus =
			DiInstallDevice(
				NULL,
				hDevInfo, 
				&deviceData, 
				&driverData,
				0,
				&rebootNeeded);

		if (!installStatus) {
			std::cerr << "ERROR: Could not install the device: "
					  << GetLastErrorAsString()
				      << "\n";

			return EXIT_FAILURE;
		}
		else {
			if (rebootNeeded) {
				std::cout << "You need to reboot your computer " 
					      << "for changes to take effect.\n";

				PromptToExit(std::cout);
			}

			return EXIT_SUCCESS;
		}
	} else {
		BOOL rebootNeeded;

		BOOL removeStatus =
			DiUninstallDevice(
				NULL, 
				hDevInfo, 
				&deviceData, 
				0, 
				&rebootNeeded);

		if (!removeStatus) {
			std::cerr << "ERROR: Could not remove the device: "
				<< GetLastErrorAsString()
				<< "\n";
			
			return EXIT_FAILURE;
		}
		else {
			if (rebootNeeded) {
				std::cout << "You need to reboot your computer "
					      << "for changes to take effect.\n";

				PromptToExit(std::cout);
			}

			return EXIT_SUCCESS;
		}
	}
	

	return EXIT_SUCCESS;
}
