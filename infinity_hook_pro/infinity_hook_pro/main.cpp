#include "hook.hpp"
#include "imports.hpp"
#include"ssdt.h"

typedef NTSTATUS(*FNtCreateFile)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
FNtCreateFile g_NtCreateFile = 0;
NTSTATUS MyNtCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock, PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength)
{

	if (
		!(KeGetCurrentIrql() != PASSIVE_LEVEL
			|| ExGetPreviousMode() == KernelMode
			//			|| PsGetProcessSessionId(IoGetCurrentProcess()) == 0
			)
		)
	{
		if (ObjectAttributes &&
			ObjectAttributes->ObjectName &&
			ObjectAttributes->ObjectName->Buffer)
		{
			wchar_t* name = (wchar_t*)ExAllocatePool(NonPagedPool, ObjectAttributes->ObjectName->Length + sizeof(wchar_t));
			if (name)
			{
				RtlZeroMemory(name, ObjectAttributes->ObjectName->Length + sizeof(wchar_t));
				RtlCopyMemory(name, ObjectAttributes->ObjectName->Buffer, ObjectAttributes->ObjectName->Length);

				if (wcsstr(name, L"test.txt"))
				{
					ExFreePool(name);
					return STATUS_ACCESS_DENIED;
				}

				ExFreePool(name);
			}
		}
	}

	return g_NtCreateFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, AllocationSize, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, EaBuffer, EaLength);



}



typedef NTSTATUS(*FNtCreateMutant)(
	_Out_ PHANDLE MutantHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_ BOOLEAN InitialOwner
	);
FNtCreateMutant g_NtCreateMutant = 0;

NTSTATUS MyNtCreateMutant(
	_Out_ PHANDLE MutantHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_ BOOLEAN InitialOwner
)
{
	// NtCreateFile 的调用方必须在 IRQL = PASSIVE_LEVEL且 启用了特殊内核 APC 的情况下运行
	if (
		!(KeGetCurrentIrql() != PASSIVE_LEVEL
			|| ExGetPreviousMode() == KernelMode
			//			|| PsGetProcessSessionId(IoGetCurrentProcess()) == 0
			)
		)
	{
		//DbgPrintEx(0, 0, "Call %wZ \n", ObjectAttributes->ObjectName);
		NTSTATUS ret = STATUS_SUCCESS;
		if (ObjectAttributes)
		{

			wchar_t* name = (wchar_t*)ExAllocatePool(NonPagedPool, ObjectAttributes->ObjectName->Length + sizeof(wchar_t));
			if (name)
			{
				RtlZeroMemory(name, ObjectAttributes->ObjectName->Length + sizeof(wchar_t));
				RtlCopyMemory(name, ObjectAttributes->ObjectName->Buffer, ObjectAttributes->ObjectName->Length);

				if (wcsstr(name, L"SUN_GAME"))
				{
					DECLARE_UNICODE_STRING_SIZE(NewmutexName, 256);

					//格式成新的符号链接名
					if (NT_SUCCESS(RtlUnicodeStringPrintf(&NewmutexName, L"\\SUN_GAME\\COM%d", PsGetCurrentThreadId())))
					{
						OBJECT_ATTRIBUTES obj = *ObjectAttributes;
						obj.ObjectName = &NewmutexName;
						
						ObjectAttributes = &obj;
					}

				}

				ExFreePool(name);
			}
			ret = g_NtCreateMutant(MutantHandle, DesiredAccess, ObjectAttributes, InitialOwner);
			return ret;
		}
		ret = g_NtCreateMutant(MutantHandle, DesiredAccess, ObjectAttributes, InitialOwner);
		return ret;
	}
	else
	{
		return g_NtCreateMutant(MutantHandle, DesiredAccess, ObjectAttributes, InitialOwner);
	}
}


void __fastcall ssdt_call_back(unsigned long ssdt_index, void** ssdt_address)
{
	// https://hfiref0x.github.io/
	UNREFERENCED_PARAMETER(ssdt_index);


	if (*ssdt_address == g_NtCreateFile)
	{
		*ssdt_address = MyNtCreateFile;
	}
	else if (*ssdt_address == g_NtCreateMutant)
	{
		*ssdt_address = MyNtCreateMutant;
	}
}

VOID DriverUnload(PDRIVER_OBJECT driver)
{
	UNREFERENCED_PARAMETER(driver);

	k_hook::stop();

	// 这里需要注意,确保系统的执行点已经不再当前驱动里面了
	// 比如当前驱动卸载掉了,但是你挂钩的MyNtCreateFile还在执行for操作,当然蓝屏啊
	// 这里的休眠10秒手段可以直接改进
	LARGE_INTEGER integer{ 0 };
	integer.QuadPart = -10000;
	integer.QuadPart *= 10000;
	KeDelayExecutionThread(KernelMode, FALSE, &integer);
}

EXTERN_C
NTSTATUS
DriverEntry(
	PDRIVER_OBJECT driver,
	PUNICODE_STRING registe)
{
	UNREFERENCED_PARAMETER(registe);

	driver->DriverUnload = DriverUnload;

	{
		UNICODE_STRING str;
		WCHAR name[256]{ L"NtCreateFile" };
		RtlInitUnicodeString(&str, name);
		g_NtCreateFile = (FNtCreateFile)MmGetSystemRoutineAddress(&str);
	}
	{
		g_NtCreateMutant = (FNtCreateMutant)GetFunctionAddress("NtCreateMutant");
	}


	// 初始化并挂钩
	return k_hook::initialize(ssdt_call_back) && k_hook::start() ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}