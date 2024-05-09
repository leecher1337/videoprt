#pragma comment(linker,"/subsystem:native")

typedef struct { char s[12]; } T12;
typedef struct { char s[24]; } T24;
typedef struct { char s[32]; } T32;

void __stdcall VideoPortGetAccessRanges(T32 p) { }
void __stdcall VideoPortSetTrappedEmulatorPorts(T12 p) { }
void __stdcall VideoPortMapMemory(T24 p) { }
void __stdcall VideoPortUnmapMemory(T12 p) { }

long __stdcall DriverEntry(void* p1,void* p2)
{
	return 0;
}
