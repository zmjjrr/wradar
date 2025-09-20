#define IOCTL
#include "memoryproxy.h"
#include <string>
#include <basetsd.h>
#include <mutex>
#include <windows.h>
#include <iostream>
#include <tlhelp32.h>
#include <codecvt>
#include <algorithm>
#include <winioctl.h>
#ifdef SYSCALL_HOOK

//typedef struct _COPY_MEMORY {
//	void* buffer;
//	UINT64		address;
//	UINT64		size;
//	bool		read;
//	bool		write;
//	bool		get_module_base;
//	bool		init;
//	const char* module_name;
//
//}COPY_MEMORY;
//
//static std::once_flag flag;
//
//template<typename ... A>
//uint64_t call_hook(const A ... arguments)
//{
//	std::call_once(flag, [] { LoadLibrary("user32.dll"); });
//	static void* control_function = GetProcAddress(LoadLibrary("win32u.dll"), "NtOpenCompositionSurfaceSectionInfo"); // same function as driver
//
//	const auto control = static_cast<uint64_t(__stdcall*)(A...)>(control_function);
//	return control(arguments ...);
//}
//
//class Driver
//{
//public:
//	Driver() {
//		COPY_MEMORY m = { 0 };
//		m.init = true;
//		call_hook(&m);
//	}
//	~Driver() {}
//
//
//	template <class T>
//	inline bool WriteMemory(uintptr_t addr, T v)
//	{
//		COPY_MEMORY m = { 0 };
//		m.write = true;
//		m.address = addr;
//		m.buffer = &v;
//		m.size = sizeof(T);
//		call_hook(&m);
//		return true;
//
//	}
//
//	template <typename type>
//	inline type ReadMemory(uintptr_t addr)
//	{
//		type buffer{};
//		COPY_MEMORY m = { 0 };
//		m.read = true;
//		m.address = addr;
//		m.buffer = &buffer;
//		m.size = sizeof(type);
//		printf("call params: addr: 0x%llx, size: 0x%llx\n", m.address, m.size);
//		call_hook(&m);
//		printf("return value: \n");
//		for (int i = 0; i < m.size; i++) {
//			printf("%02x ", ((UINT8*)m.buffer)[i]);
//		}
//		printf("\n");
//
//		return buffer;
//	}
//
//	inline uintptr_t GetModuleBase(std::string mod)
//	{
//		COPY_MEMORY m = { 0 };
//		m.get_module_base = true;
//		m.module_name = mod.c_str();
//		call_hook(&m);
//		printf("GetModuleBase return 0x%llx\n", m.address);
//		return (uintptr_t)m.address;// pointer stored in address
//	}
//
//private:
//	int pid;
//
//};

#endif

#ifdef IOCTL

namespace driver {
	namespace codes {
		constexpr ULONG attach = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
		constexpr ULONG read = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
		constexpr ULONG write = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
		constexpr ULONG get_module_base = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x699, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	}

	struct Request {

		HANDLE processId;

		PVOID target;
		PVOID buffer;

		SIZE_T size;
		SIZE_T returnSize;
	};

	inline bool attach_to_process(HANDLE driver_handle, const DWORD pid) {
		Request r;
		r.processId = reinterpret_cast<HANDLE>(pid);

		return DeviceIoControl(driver_handle, codes::attach, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);
	}


}

static DWORD GetProcessIdByName(const wchar_t* processName)
{
	if (!processName || !*processName) return 0;

	// 取文件名部分（去掉路径/盘符）
	const wchar_t* base = processName;
	for (const wchar_t* p = processName; *p; ++p) {
		if (*p == L'\\' || *p == L'/' || *p == L':') base = p + 1;
	}

	std::wstring target(base);

	// 去掉可能包裹的引号
	if (!target.empty() && target.front() == L'"' && target.back() == L'"' && target.size() >= 2) {
		target = target.substr(1, target.size() - 2);
	}

	// 如果不带 .exe，则备用一个附带 .exe 的候选
	auto ends_with_exe_icase = [](const std::wstring& s) -> bool {
		if (s.size() < 4) return false;
		return _wcsicmp(s.c_str() + s.size() - 4, L".exe") == 0;
		};
	bool hasExe = ends_with_exe_icase(target);
	std::wstring targetWithExe = hasExe ? target : (target + L".exe");

	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) return 0;

	PROCESSENTRY32W pe{};
	pe.dwSize = sizeof(pe);

	DWORD pid = 0;
	if (Process32FirstW(snap, &pe)) {
		do {
			// pe.szExeFile 是进程的可执行文件名（仅文件名，不含路径）
			if (_wcsicmp(pe.szExeFile, target.c_str()) == 0 ||
				_wcsicmp(pe.szExeFile, targetWithExe.c_str()) == 0)
			{
				pid = pe.th32ProcessID;
				break;
			}
		} while (Process32NextW(snap, &pe));
	}

	CloseHandle(snap);
	return pid; // 未找到返回 0
}

static std::wstring ToWide(const std::string& s)
{
	if (s.empty()) return {};
	int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
	std::wstring w(n ? (n - 1) : 0, L'\0');
	if (n) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
	return w;
}

static uintptr_t GetRemoteModuleBase(DWORD pid, const std::string& moduleName)
{
	std::wstring wmod = ToWide(moduleName);
	std::wstring wmodLower = wmod;
	std::transform(wmodLower.begin(), wmodLower.end(), wmodLower.begin(), ::towlower);

	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if (snap == INVALID_HANDLE_VALUE) return 0;

	MODULEENTRY32W me{};
	me.dwSize = sizeof(me);

	if (Module32FirstW(snap, &me))
	{
		do {
			std::wstring base(me.szModule);
			std::transform(base.begin(), base.end(), base.begin(), ::towlower);

			if (base == wmodLower)
			{
				CloseHandle(snap);
				return reinterpret_cast<uintptr_t>(me.modBaseAddr); // 基址
			}
		} while (Module32NextW(snap, &me));
	}
	CloseHandle(snap);
	return 0;
}

class Driver
{
public:
	Driver() {
		pid = GetProcessIdByName(L"cs2.exe");
		if (!pid) {
			std::cout << "invalid pid" << std::endl;
		}
		if (driver::attach_to_process(driver_handle, pid) == true) {
			std::cout << "Attach success" << std::endl;
		}
		else {
			std::cout << "Attach fail" << std::endl;
		}
	}
	~Driver() {}


	template <class T>
	inline bool WriteMemory(uintptr_t addr, T v)
	{
		driver::Request r;
		r.target = reinterpret_cast<PVOID>(addr);
		r.buffer = (PVOID)&v;
		r.size = sizeof(T);

		DeviceIoControl(driver_handle, driver::codes::write, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);
	}

	template <typename T>
	inline T ReadMemory(uintptr_t addr)
	{
		T temp = {};

		driver::Request r;
		r.target = reinterpret_cast<PVOID>(addr);
		r.buffer = &temp;
		r.size = sizeof(T);

		DeviceIoControl(driver_handle, driver::codes::read, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);

		return temp;
	}

	inline uintptr_t GetModuleBase(std::string mod) {
		return GetRemoteModuleBase(pid, mod);
	}

private:
	int pid;
	const HANDLE driver_handle = CreateFileW(L"\\\\.\\SexyDriver", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

};


#endif
