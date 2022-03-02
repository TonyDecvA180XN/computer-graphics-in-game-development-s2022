#pragma once

#include <stdexcept>
#include <string>

#define THROW_ERROR(x)                            \
	{                                             \
		std::string message(x);                   \
		message.append(" (at ")                   \
				.append(__FILE__)                 \
				.append(":")                      \
				.append(std::to_string(__LINE__)) \
				.append(")")                      \
				.append("\n");                    \
		throw std::runtime_error(message);        \
	}

#define WIN_ERROR(text) 													   \
	{																		   \
		const DWORD errorId = ::GetLastError();								   \
		LPWSTR messageBuffer = nullptr;										   \
																			   \
		const size_t size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |	   \
			FORMAT_MESSAGE_FROM_SYSTEM |									   \
			FORMAT_MESSAGE_IGNORE_INSERTS,									   \
			nullptr,														   \
			errorId,														   \
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),						   \
			reinterpret_cast<LPWSTR>(&messageBuffer),						   \
			0,																   \
			nullptr);														   \
																			   \
		std::wstring message(messageBuffer, size);							   \
		LocalFree(messageBuffer);											   \
																			   \
		message.insert(0, text);					   \
		MessageBox(nullptr, message.c_str(), L"Runtime Failure", MB_ICONERROR);\
	}