#define _CRT_SECURE_NO_WARNINGS

#include <cstdlib>
#include <windows.h>
#include <setupapi.h>
#include <shlwapi.h>
#include <iostream>

#pragma comment (lib, "Setupapi.lib")
#pragma comment (lib, "shlwapi.lib")

// The following function is from: 
// https://stackoverflow.com/questions/1387064/how-to-get-the-error-message-from-the-error-code-returned-by-getlasterror//////
static std::string GetLastErrorAsString()
{
	//Get the error message ID, if any.
	DWORD errorMessageID = ::GetLastError();
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
			(LPSTR)&messageBuffer, 0, NULL);

	//Copy the error message into a std::string.
	std::string message(messageBuffer, size);

	//Free the Win32's string's buffer.
	LocalFree(messageBuffer);

	return message;
}

int main(int argc, char* argv[]) {
	if (argc == 1) {
		std::cout << "ERROR: No GUID specified.\n";
		return EXIT_FAILURE;
	}

	if (argc > 2) {
		PathStripPathA(argv[1]);
		std::cout << "Usage: " << argv[1] << " GUID\n";
		return EXIT_FAILURE;
	}

	size_t guidLength = strlen(argv[1]);
	wchar_t* guidAsWideChar = new wchar_t[guidLength + 1];
	guidAsWideChar[guidLength] = L'\0';
	mbstowcs(guidAsWideChar, argv[1], guidLength);

	GUID guid;
	HRESULT hResult =
		CLSIDFromString(
			guidAsWideChar,
			(LPCLSID)&guid);

	if (hResult == CO_E_CLASSSTRING) {
		std::cerr << "ERROR: Bad GUID string: "
				  << GetLastErrorAsString() 
				  << "\n";

		return EXIT_FAILURE;
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

		return EXIT_FAILURE;
	}

	SP_DEVINFO_DATA deviceInfoData;
	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	deviceInfoData.ClassGuid = guid;

	BOOL deviceEnumerated =
		SetupDiEnumDeviceInfo(
			hDeviceInfo,
			0,
			&deviceInfoData);

	if (!deviceEnumerated) {
		std::cerr << "ERROR: Could not enumerate the SP_DEVINFO_DATA: " 
				  << GetLastErrorAsString()
				  << "\n";

		return EXIT_FAILURE;
	}

	BOOL removeStatus =
		SetupDiCallClassInstaller(
			DIF_REMOVE,
			hDeviceInfo,
			&deviceInfoData);

	if (!removeStatus) {
		std::cerr << "ERROR: Could not remove the device: " 
				  << GetLastErrorAsString()
				  << "\n";

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}