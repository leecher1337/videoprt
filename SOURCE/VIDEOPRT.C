#define _X86_
#include <miniport.h>
#include <ntddvdeo.h>
#include <video.h>
#include <winerror.h>

#pragma comment(linker,"/export:VideoPortSetTrappedEmulatorPorts=_VideoPortSetTrappedEmulatorPorts1@12,@88")

enum { MAX_RANGES = 32 };

ULONG numRanges = 0;
VIDEO_ACCESS_RANGE ranges[MAX_RANGES];

BOOLEAN tried = FALSE, loaded = FALSE;

BOOLEAN RegLoad();

ULONG __stdcall DriverEntry(void* p1,void* p2)  { return 0; }


VP_STATUS __stdcall VideoPortSetTrappedEmulatorPorts1(
  IN PVOID  HwDeviceExtension,
  IN ULONG  NumAccessRanges,
  IN PVIDEO_ACCESS_RANGE  AccessRanges
  )
{
	PVIDEO_ACCESS_RANGE ar = AccessRanges;
	ULONG n = NumAccessRanges;
	while (n-- > 0)
	{
		if (ar++ -> RangeVisible)
		{
			if (!tried)
			{
				tried = TRUE;
				if (NumAccessRanges > MAX_RANGES) break;

				memcpy(ranges, AccessRanges, sizeof(*ranges) * NumAccessRanges);
				n = numRanges = NumAccessRanges;
				ar = ranges;
				while (n-- > 0)
					ar++ -> RangeVisible = 1;

				loaded = RegLoad();
			}
			if (!loaded) break;
			return VideoPortSetTrappedEmulatorPorts(
					HwDeviceExtension, numRanges, ranges);
		}
	}
	return VideoPortSetTrappedEmulatorPorts(
			HwDeviceExtension, NumAccessRanges, AccessRanges);
}

#pragma comment(linker,"/export:VideoPortGetAccessRanges=_VideoPortGetAccessRanges1@32,@32")
#pragma comment(linker,"/export:VideoPortMapMemory=_VideoPortMapMemory1@24,@56")
#pragma comment(linker,"/export:VideoPortUnmapMemory=_VideoPortUnmapMemory1@12,@98")

VP_STATUS VideoPortGetAccessRanges1(
  PVOID  HwDeviceExtension,
  ULONG  NumRequestedResources,
  PIO_RESOURCE_DESCRIPTOR  RequestedResources,
  ULONG  NumAccessRanges,
  PVIDEO_ACCESS_RANGE  AccessRanges,
  PVOID  VendorId,
  PVOID  DeviceId,
  PULONG  Slot
  )
{
	ULONG i;
	VP_STATUS status = VideoPortGetAccessRanges(
		HwDeviceExtension,
		NumRequestedResources,
		RequestedResources,
		NumAccessRanges,
		AccessRanges,
		VendorId,
		DeviceId,
		Slot);
	if (NumAccessRanges > MAX_RANGES) NumAccessRanges = MAX_RANGES;
	for (i=0; i<NumAccessRanges; i++)
	{
		if (AccessRanges[i].RangeInIoSpace)
			ranges[numRanges++].RangeStart = AccessRanges[i].RangeStart;
	}
	return status;
}

char buf[256];

VP_STATUS VideoPortMapMemory1(
  IN PVOID  HwDeviceExtension,
  IN PHYSICAL_ADDRESS  PhysicalAddress,
  IN OUT PULONG  Length,
  IN PULONG  InIoSpace,
  IN OUT PVOID  *VirtualAddress
  )
{
	ULONG i;
	for (i=0; i<numRanges; i++)
	{
		if (PhysicalAddress.QuadPart == ranges[i].RangeStart.QuadPart)
		{
			*VirtualAddress = buf;
			*Length = sizeof(buf);
			return 0;
		}
	}
	return VideoPortMapMemory(HwDeviceExtension, PhysicalAddress,
		Length, InIoSpace, VirtualAddress);
}

VP_STATUS VideoPortUnmapMemory1(
  IN PVOID  HwDeviceExtension,
  IN OUT PVOID  VirtualAddress,
  IN HANDLE  ProcessHandle
  )
{
	if (VirtualAddress == 0 || VirtualAddress == buf) return 0;
	return VideoPortUnmapMemory(HwDeviceExtension,
		VirtualAddress, ProcessHandle);
}