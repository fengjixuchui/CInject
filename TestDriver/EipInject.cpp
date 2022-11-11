#include "EipInject.h"

FuncType KeSuspendThread = NULL;
FuncType KeResumeThread = NULL;


//�ж��ǲ���GUI�߳�
bool IsGuiThread(PETHREAD thread) {
	PUCHAR pteb64 = (PUCHAR)PsGetThreadTeb(thread);

	if (*(PULONG64)(pteb64 + 0x78) != 0) {
		return true;
	}

	return false;
}



PETHREAD GetFirstThread(PEPROCESS tempep) {
	//windows��ͳ���ڳ��������һ��������GUI�߳�

	PETHREAD pretthreadojb = NULL, ptempthreadobj = NULL;

	PLIST_ENTRY plisthead = NULL;

	PLIST_ENTRY plistflink = NULL;

	int i = 0;

	plisthead = (PLIST_ENTRY)((PUCHAR)tempep + 0x30);

	plistflink = plisthead->Flink;

	ptempthreadobj = (PETHREAD)((PUCHAR)plistflink - 0x2f8);

	return ptempthreadobj;
}



KTRAP_FRAME MyGetThreadContext(PETHREAD thread)
{
	PKTRAP_FRAME threadContext =(PKTRAP_FRAME)   *(PULONG64)((ULONG64)thread + OFFSET_KTRAP_FRAME);
	return *threadContext;
}

bool MySetThreadContext(PETHREAD thread, KTRAP_FRAME context)
{
	PKTRAP_FRAME threadContext = (PKTRAP_FRAME)  *(PULONG64)((ULONG64)thread + OFFSET_KTRAP_FRAME);
	*threadContext = context;
	return true;
}

//1909��������KeSuspendThread   (u KeSuspendThread)
void FindKeSuspendThread() {
	//��ȡ Ntoskrnl
	PVOID  ntoskrnlBase = NULL;
	ULONG64 size = NULL;
	GetNtoskrnlBase(&ntoskrnlBase, &size);
	PUCHAR ModuleStart = (PUCHAR)ntoskrnlBase;

	KeSuspendThread = (FuncType)FindPattern_Wrapper(ModuleStart, size,"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 30 48 8B D9 44 0F 20 C7 41 BF 02 00 00 00 45 0F 22 C7");
	//fffff801584a5de4 +0x29
	//KeSuspendThread = (FuncType)FindPattern_Wrapper(ModuleStart, size, "8B 05 ? ? ? ? 85 C0 0F 85 ? ? ? ? 65 4C 8B 34 25 20 00 00 00");

}

//1909��������KeResumeThread    (u KeResumeThread)
void FindKeResumeThread() {
	//��ȡ Ntoskrnl
	PVOID  ntoskrnlBase = NULL;
	ULONG64 size = NULL;
	GetNtoskrnlBase(&ntoskrnlBase, &size);
	PUCHAR ModuleStart = (PUCHAR)ntoskrnlBase;
	

	//																																														    |
	KeResumeThread = (FuncType)FindPattern_Wrapper(ModuleStart, size,"48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 48 89 78 20 41 56 48 83 EC 30 48 8B D9 44 0F 20 C6 B9 02 00 00 00 44 0F 22 C1 8B 05 ? ? ? ? 85 C0 0F 85 83 07 13 00 65 48 8B 2C 25 20 00 00 00");
}

void initKethreadFunc() {
	FindKeSuspendThread();
	FindKeResumeThread();

	DbgPrint("KeSuspendThread : %p", KeSuspendThread);
	DbgPrint("KeResumeThread : %p", KeResumeThread);
}

void EipExcuteFuntion(PEPROCESS process, PVOID func,ULONG64 modulebase,LONGLONG cleartimeSecond)
{
	if ( !KeSuspendThread || !KeResumeThread) {
		DbgPrint("KeSuspendThread �� KeResumeThread δ�ҵ�");
		return;
	}

	//��ѡ��һ���߳�
	PETHREAD thread = GetFirstThread(process);
	DbgPrint("first_thread�߳�ID: %d \r\n", PsGetThreadId(thread));

	//�����߳�
	KeSuspendThread(thread);

	//��ȡ�Ĵ���������
	KTRAP_FRAME context = MyGetThreadContext(thread);

	/*
		push rcx
		push rax
		mov rdx,00000001 //�ڶ�������DLL_PROCESS_ATTACH
		mov rax,entryaddr //��ַ
		sub rsp,28
		call rax
		add rsp,28
		pop rax
		pop rcx
		jmp rip  rip�Ĵ�����ֵ

	*/
	//����shellcode
	//BYTE shellcode[] = {
	//	0x51, 0x50, 0x48 ,0xBA, 0x00 ,0x00 ,0x00, 0x00, 0x00, 0x00, 0x00 ,0x00 ,
	//	0x48 ,0xB8 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x48, 0x83 ,0xEC,
	//	0x28 ,0xFF ,0xD0 ,0x48 ,0x83 ,0xC4 ,0x28 ,0x58 ,0x59 ,0xFF ,0x25 ,0x00 ,0x00,
	//	0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
	//};

	BYTE shellcode[] = {
		0x51,	//push rcx
		0x50,	//push rax
		0x48,0xB9,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 ,//mov rcx,modulebase
		0x48,0xBA,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 ,//mov rdx,DLL_PROCESS_ATTACH
		0x48,0xB8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 ,//mov rax,entryaddr
		0x48,0x83,0xEC,0x28,								//sub rsp,28
		0xFF,0xD0,											//call rax
		0x48,0x83,0xC4,0x28,								//add rsp,28
		0x58,												//pop rax
		0x59,												//pop rcx
		0xFF,0x25,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00//jmp Rip
	};


	//�������
	*(PULONG64)(shellcode + 4) = modulebase;
	*(PULONG64)(shellcode + 14) = DLL_PROCESS_ATTACH;
	*(PULONG64)(shellcode + 24) = (ULONG64)func;
	*(PULONG64)(shellcode + sizeof(shellcode) - 8) = context.Rip;

	//����Զ���ڴ�
	PVOID virtualaddr = NULL;
	RemoteAllcateMemory(process,sizeof(shellcode),&virtualaddr);

	//����shellcode
	size_t bytes = 0;
	NTSTATUS res =  MmCopyVirtualMemory(IoGetCurrentProcess(), shellcode, process, virtualaddr, sizeof(shellcode), KernelMode, &bytes);
	if (!NT_SUCCESS(res)) {
		DbgPrint("����shellcode ʧ�ܣ�");
		return;
	}

	//����rip
	context.Rip = (DWORD64)virtualaddr;
	MySetThreadContext(thread, context);

	//�ָ��߳�
	KeResumeThread(thread);

	//�ȴ�30�룬���shellcode
	LARGE_INTEGER li = { 0 };
	li.QuadPart = -10000 * 1000 * cleartimeSecond;
	KeDelayExecutionThread(KernelMode, NULL, &li);
	RemoteFreeMemory(process, virtualaddr, sizeof(shellcode));
	DbgPrint("shellcode ��գ�");
}
