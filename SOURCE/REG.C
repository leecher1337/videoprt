#define _X86_
#include <ntddk.h>
#include <ntdef.h>

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

enum { MAXINFO = 2000 };


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
	ZwQueryValueKey(hkey, &name, KeyValuePartialInformation, vinfo, MAXINFO, &size)


BOOLEAN RegLoad()
{
	ULONG i, j, k, size;
	HANDLE hkPci, hkDev, hkRev, hkCtl;
	UNICODE_STRING usClass;
	KEY_VALUE_PARTIAL_INFORMATION* vinfo;
	CM_PARTIAL_RESOURCE_LIST* cmprl;
	CM_PARTIAL_RESOURCE_DESCRIPTOR* rd;

	VIDEO_ACCESS_RANGE* ar = ranges + numRanges;
	BOOLEAN status = FALSE;

	pInfo = ExAllocatePool(PagedPool, MAXINFO);
	if (!pInfo) return FALSE;

	vinfo = (KEY_VALUE_PARTIAL_INFORMATION*) pInfo;
	cmprl = & ((CM_RESOURCE_LIST*)vinfo->Data)->List[0].PartialResourceList;

	i = j = k = size = 0;
	rd = 0;

	if (OpenKey(&hkPci, 0, wsPci) == STATUS_SUCCESS)
	{
		for (i=0;  hkDev = KeyEnum(hkPci, i);  i++)
		{
			for (j=0;  hkRev = KeyEnum(hkDev, j);  j++)
			{
				if (!KEY_GET_VALUE(hkRev, usCls))
				{
					RtlInitUnicodeString(&usClass, (PWSTR)vinfo->Data);
					if (RtlCompareUnicodeString(&usClass, &usDisplay, TRUE)) continue;
					
					if (OpenKey(&hkCtl, hkRev, wsCtl)) continue;

					if (!KEY_GET_VALUE(hkCtl, usCfg))
					{
						for (k=0; k < cmprl->Count && numRanges < MAX_RANGES; k++)
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
							
							ar++;
							numRanges++;
							status = TRUE;
						}
					}
					ZwClose(hkCtl);
				}
				ZwClose(hkRev);
			}
			ZwClose(hkDev);
		}
		ZwClose(hkPci);
	}
	ExFreePool(pInfo);
	return status;
}
