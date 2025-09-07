#define _X86_
#include <ntddk.h>
#include <ntdef.h>
#include <initguid.h>
#include <ntddvdeo.h>

#pragma pack(1)

typedef struct _VIDEO_ACCESS_RANGE {
    PHYSICAL_ADDRESS RangeStart;
    ULONG RangeLength;
	UCHAR RangeInIoSpace;
	UCHAR RangeVisible;
    UCHAR RangeShareable;
    UCHAR RangePassive;
} VIDEO_ACCESS_RANGE, *PVIDEO_ACCESS_RANGE;

enum { MAX_RANGES = 32 };

ULONG numRanges;
VIDEO_ACCESS_RANGE ranges[MAX_RANGES];

enum { MAXINFO = 2000 };

WCHAR wsPci[] = L"\\REGISTRY\\Machine\\System\\CurrentControlSet\\Enum\\PCI";
WCHAR wsCtl[] = L"Control";
WCHAR wsCfg[] = L"AllocConfig";
WCHAR wsCls[] = L"Class";
WCHAR wsDisplay[] = L"Display";

struct _UNICODE_STRING usCls = { sizeof(wsCls) - 2, sizeof(wsCls), wsCls };
struct _UNICODE_STRING usCfg = { sizeof(wsCfg) - 2, sizeof(wsCfg), wsCfg };
struct _UNICODE_STRING usDisplay = { sizeof(wsDisplay) - 2, sizeof(wsDisplay), wsDisplay };

struct _OBJECT_ATTRIBUTES attr = { sizeof(attr), 0, 0, 0, 0, 0 };

void* pInfo;


static BOOLEAN AddRange(CM_PARTIAL_RESOURCE_LIST* cmprl)
{
	ULONG k;
	CM_PARTIAL_RESOURCE_DESCRIPTOR* rd;
	VIDEO_ACCESS_RANGE* ar = ranges + numRanges;
	BOOLEAN status = FALSE;

	for (k = 0; k < cmprl->Count && numRanges < MAX_RANGES; k++)
	{
		rd = cmprl->PartialDescriptors + k;
		if (rd->Type == CmResourceTypePort)			ar->RangeInIoSpace = 1;
		//else if (rd->Type == CmResourceTypeMemory)	ar->RangeInIoSpace = 0;
		else continue;

		ar->RangeStart = rd->u.Port.Start;
		ar->RangeLength = rd->u.Port.Length;
		ar->RangeVisible = 1;
		ar->RangeShareable = 1;
		ar->RangePassive = 0;
		DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_TRACE_LEVEL, "videoprt found an IO-Range %x-%x", rd->u.Port.Start.u.LowPart, rd->u.Port.Start.u.LowPart + rd->u.Port.Length);

		ar++;
		numRanges++;
		status = TRUE;
	}
	return status;
}

BOOLEAN LoadBootConfig()
{
	PWSTR pszszDeviceList = NULL, p;
	GUID Interface = GUID_DISPLAY_ADAPTER_INTERFACE;
	NTSTATUS Status;
	PVOID pInfo;
	BOOLEAN status = FALSE;

	if (NT_SUCCESS(Status = IoGetDeviceInterfaces(&Interface, NULL, 0, &pszszDeviceList))) 
	{
		pInfo = ExAllocatePool(PagedPool, MAXINFO);
		if (!pInfo)
		{
			DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, "videoprt failed to allocate memory for info.");
			ExFreePool(pszszDeviceList);
			return FALSE;
		}

		for (p = pszszDeviceList; *p; p += wcslen(p))
		{
			UNICODE_STRING SymbolicLink = { 0 };
			PFILE_OBJECT FileObject;
			PDEVICE_OBJECT DeviceObject = NULL;

			RtlInitUnicodeString(&SymbolicLink, p);
			if (NT_SUCCESS(Status = IoGetDeviceObjectPointer(&SymbolicLink, FILE_READ_DATA, &FileObject, &DeviceObject)))
			{
				ULONG Size = MAXINFO;

				Status = IoGetDeviceProperty(FileObject->DeviceObject,
					DevicePropertyBootConfiguration, Size, pInfo, &Size
					);
				if (NT_SUCCESS(Status))
				{
					status = AddRange(&((CM_RESOURCE_LIST*)pInfo)->List[0].PartialResourceList);
				}
				else
				{
					DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, "IoGetDeviceProperty for %S failed: %08X", p, Status);
				}
			}
			else 
			{
				DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, "IoGetDeviceObjectPointer %S failed: %08X", p, Status);
			}
		}
		ExFreePool(pInfo);
		ExFreePool(pszszDeviceList);
	}
	else
	{
		DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, "IoGetDeviceInterfaces failed: %08X", Status);
	}
	return status;
}

NTSTATUS OpenKey(HANDLE* pkey, HANDLE root, PWSTR name)
{
	UNICODE_STRING usKey;
	RtlInitUnicodeString(&usKey, name);
	attr.RootDirectory = root;
	attr.ObjectName = &usKey;
	return ZwOpenKey(pkey, KEY_ENUMERATE_SUB_KEYS|KEY_QUERY_VALUE, &attr);
}


HANDLE KeyEnum(HANDLE root, ULONG ord)
{
	UNICODE_STRING usKey;
	HANDLE hkey;
	ULONG size;
	KEY_BASIC_INFORMATION* kinfo = (KEY_BASIC_INFORMATION*) pInfo;
	if (ZwEnumerateKey(root, ord, KeyBasicInformation, kinfo, MAXINFO, &size)) return 0;
	usKey.Length = usKey.MaximumLength = (USHORT)kinfo->NameLength;
	usKey.Buffer = kinfo->Name;
	attr.RootDirectory = root;
	attr.ObjectName = &usKey;
	if (ZwOpenKey(&hkey, KEY_ENUMERATE_SUB_KEYS|KEY_QUERY_VALUE, &attr)) return 0;
	return hkey;
}


#define KEY_GET_VALUE(hkey, name)\
	(Status = ZwQueryValueKey(hkey, &name, KeyValuePartialInformation, vinfo, MAXINFO, &size))


BOOLEAN RegLoad()
{
	ULONG i, j, size;
	HANDLE hkPci, hkDev, hkRev, hkCtl;
	UNICODE_STRING usClass;
	KEY_VALUE_PARTIAL_INFORMATION* vinfo;
	CM_PARTIAL_RESOURCE_LIST* cmprl;
	NTSTATUS Status;
	BOOLEAN status = FALSE;

	pInfo = ExAllocatePool(PagedPool, MAXINFO);
	if (!pInfo)
	{
		DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, "videoprt failed to allocate memory for info.");
		return FALSE;
	}

	vinfo = (KEY_VALUE_PARTIAL_INFORMATION*) pInfo;
	cmprl = & ((CM_RESOURCE_LIST*)vinfo->Data)->List[0].PartialResourceList;

	i = j = size = 0;

	if ((Status = OpenKey(&hkPci, 0, wsPci)) == STATUS_SUCCESS)
	{
		for (i=0;  hkDev = KeyEnum(hkPci, i);  i++)
		{
			for (j=0;  hkRev = KeyEnum(hkDev, j);  j++)
			{
				if (!KEY_GET_VALUE(hkRev, usCls))
				{
					RtlInitUnicodeString(&usClass, (PWSTR)vinfo->Data);
					if (RtlCompareUnicodeString(&usClass, &usDisplay, TRUE)) continue;
					
					DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_TRACE_LEVEL, "videoprt found display class key (%d/%d)", i, j);
					if (Status = OpenKey(&hkCtl, hkRev, wsCtl))
					{
						DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_TRACE_LEVEL, "videoprt NO Control-Subkey (%08X).", Status);
						continue;
					}

					if (!KEY_GET_VALUE(hkCtl, usCfg))
					{
						DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_TRACE_LEVEL, "videoprt found AllocConfig!");
						status = AddRange(cmprl);
					}
					else
					{
						DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_TRACE_LEVEL, "videoprt NO AllocConfig (%08X)", Status);
					}
					ZwClose(hkCtl);
				}
				else
				{
					DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_TRACE_LEVEL, "videoprt Cannot find Class value (%08X) (%d/%d)", Status, i, j);
				}
				ZwClose(hkRev);
			}
			ZwClose(hkDev);
		}
		ZwClose(hkPci);
	}
	else
	{
		DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, "videoprt failed to open root key: %08X", Status);
	}
	ExFreePool(pInfo);
	if (!status) return LoadBootConfig();
	return status;
}


