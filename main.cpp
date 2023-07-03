#define _CRT_SECURE_NO_WARNINGS

#include <cstdlib>
#include <string>
#include <windows.h>
#include <setupapi.h>
#include <shlwapi.h>
#include <newdev.h>
#include <iostream>
#include <conio.h>

#pragma comment (lib, "Setupapi.lib")
#pragma comment (lib, "shlwapi.lib")
#pragma comment (lib, "newdev.lib")

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

static wchar_t* ConvertCharStringToWcharString(char* source) {
	size_t guidLength = strlen(source);
	wchar_t* target = new wchar_t[guidLength + 1];
	target[guidLength] = L'\0';
	mbstowcs(target, source, guidLength);
	return target;
}

static void PromptToExit(std::ostream& out) {
	out << "Press any key to exit.\n";
	_getch();
}

static bool LoadDeviceData(
	char* guidString, 
	HDEVINFO* pDevInfo, 
	PSP_DEVINFO_DATA pData) {

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
			DIGCF_PRESENT,
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

	pDevInfo = &hDeviceInfo;
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
		std::cout << "Usage: " << argv[0] << "[--install] GUID\n";
		PromptToExit(std::cerr);
		return EXIT_FAILURE;
	}

	bool install = false;
	char* guidParameter = NULL;

	if (argc == 2) {
		guidParameter = argv[1];
	} else if (argc == 3) {
		std::string flagParameter = argv[1];
		const std::string expectedFlag = "--install";

		if (flagParameter != expectedFlag) {
			std::cerr << "ERROR: Wrong flag: " << flagParameter << "\n";
			PromptToExit(std::cerr);
			return EXIT_FAILURE;
		}

		guidParameter = argv[2];
		install = true;
	}

	HDEVINFO hDevInfo;
	SP_DEVINFO_DATA deviceData;

	bool ok = 
		LoadDeviceData(
			guidParameter, 
			&hDevInfo, 
			&deviceData);

	if (!ok) {
		PromptToExit(std::cerr);
		return EXIT_FAILURE;
	}

	if (install) {
		BOOL installStatus =
			DiInstallDevice(
				NULL,
				hDevInfo, 
				&deviceData, 
				NULL,
				0,
				NULL);

		if (!installStatus) {
			std::cerr << "ERROR: Could not install the device: "
					  << GetLastErrorAsString()
				      << "\n";

			PromptToExit(std::cerr);
			return EXIT_FAILURE;
		}
	} else {
		BOOL removeStatus =
			SetupDiCallClassInstaller(
				DIF_REMOVE,
				hDevInfo,
				&deviceData);

		if (!removeStatus) {
			std::cerr << "ERROR: Could not remove the device: "
				<< GetLastErrorAsString()
				<< "\n";

			PromptToExit(std::cerr);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}