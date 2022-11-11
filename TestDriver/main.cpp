#include "ntifs.h"
#include "ntddk.h"
#include "wdm.h"

#include "utils.h"
#include "filehelp.h"
#include "ApcInject.h"
#include "PeHelper.h"
#include "EipInject.h"

PDRIVER_OBJECT G_DriverObject = NULL;

typedef void (*LoopthreadCallback)(PETHREAD thread);

//��������
PETHREAD LoopThreadInProcess(PEPROCESS tempep, LoopthreadCallback func)
{
	PETHREAD pretthreadojb = NULL, ptempthreadobj = NULL;

	PLIST_ENTRY plisthead = NULL;

	PLIST_ENTRY plistflink = NULL;

	int i = 0;

	plisthead = (PLIST_ENTRY)((PUCHAR)tempep + 0x30);

	plistflink = plisthead->Flink;

	//����
	for (plistflink; plistflink != plisthead; plistflink = plistflink->Flink)
	{
		ptempthreadobj = (PETHREAD)((PUCHAR)plistflink - 0x2f8);

		HANDLE threadId = PsGetThreadId(ptempthreadobj);

		func(ptempthreadobj);

		DbgPrint("�߳�ID: %d \r\n", threadId);

		i++;

	}

	return pretthreadojb;
}

void RemoteLoadPeData(PEPROCESS process, PVOID filebufeer, ULONG64 filesize,PVOID * entry,PVOID * moduleBase) {
	//����
	KAPC_STATE KAPC;
	//PEPROCESS pEProc;
	//NTSTATUS GetPEPROCESSStatus = PsLookupProcessByProcessId((PsGetProcessId(process)), &pEProc);
	//if (!NT_SUCCESS(GetPEPROCESSStatus)) {
	//	DbgPrint("׼�����������ڴ�ʧ��");
	//	ObDereferenceObject(pEProc);
	//	return;
	//}

	KeStackAttachProcess(process, &KAPC);

	//���������ڴ�
	PVOID virtualbase = NULL;
	//ULONG64 imagesize = GetImageSize((PUCHAR)filebufeer);
	//NTSTATUS AllocateStatus = NtAllocateVirtualMemory(NtCurrentProcess(), &virtualbase, NULL, &imagesize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	//memset(virtualbase, 0, imagesize);
	//if (!NT_SUCCESS(AllocateStatus)) {
	//	DbgPrint("���������ڴ�����ʧ�� ���룺%ullx", AllocateStatus);
	//	return;
	//}
	//DbgPrint("���������ڴ��ַ�� %p �����С��%llu", virtualbase, imagesize);

	NTSTATUS allocatePeStatus =  RemoteAllcateMemory(process, GetImageSize((PUCHAR)filebufeer), &virtualbase);
	if (!NT_SUCCESS(allocatePeStatus))
	{
		DbgPrint("���������ڴ�����ʧ��");
		return;
	}
	DbgPrint("Զ������Pe�ڴ�ɹ���");

	PVOID Pebuffer = NULL;
	ULONG64 PEsize = NULL;
	PVOID entrypoint = NULL;
	PELoaderDLL((PUCHAR)filebufeer, (PUCHAR)virtualbase, &Pebuffer, &PEsize, &entrypoint, (PsGetProcessId(process)));
	DbgPrint("PEbuffer ��ַ��%p  PEsize : %llu ��ڵ�λ�ã�%p", Pebuffer, PEsize, entrypoint);

	KeUnstackDetachProcess(&KAPC);
	//ObDereferenceObject(pEProc);

	//��������õ�Pe�����̿ռ�
	size_t bytes = 0;
	MmCopyVirtualMemory(IoGetCurrentProcess(), Pebuffer, process, virtualbase, filesize, KernelMode, &bytes);

	*entry = entrypoint;
	*moduleBase = virtualbase;

	//�ͷ�Pebuffer
	ExFreePool(Pebuffer);

}

void injectDll(LPCWSTR procName, PVOID filebuffer, ULONG64 filesize) {
	//��Ŀ�����
	//r5apex.exe
	//mspaint.exe
	PEPROCESS process =  GetEprocessByName(procName);
	if (!process) {
		DbgPrint("����δ�ҵ�");
		return;
	}

	LoopThreadInProcess(process, [](PETHREAD thread) {});

	//����PE
	PVOID entrypoint = NULL;
	PVOID moduleBase = NULL;
	RemoteLoadPeData(process, filebuffer,filesize,&entrypoint,&moduleBase);

	//APCִ�к���
	//APCExecuteFunction(process, entrypoint,(ULONG64)moduleBase);


	//Eipִ�к���
	EipExcuteFuntion(process, entrypoint,(ULONG64)moduleBase,30);
	
}

//ж�غ���
VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
	DbgPrint("����ж�سɹ���");
}

//void LoopSSDT() {
//	//dq KeServiceDescriptorTable
//
//	//��ȡ Ntoskrnl
//	PVOID  ntoskrnlBase = NULL;
//	ULONG64 size = NULL;
//	GetNtoskrnlBase(&ntoskrnlBase, &size);
//	PUCHAR ModuleStart = (PUCHAR)ntoskrnlBase;
//	PUCHAR ModuleEnd = ModuleStart + size;
//
//	//��  u KiSystemServiceRepeat
//	PUCHAR KiSystemServiceRepeat =   FindPattern_Wrapper(ModuleStart, size,"4C 8D 15 ? ? ? ? 4C 8D 1D");
//	DbgPrint("KiSystemServiceRepeat : %p", KiSystemServiceRepeat);
//	//��λKeServiceDescriptorTable
//	PUCHAR KeServiceDescriptorTable = *(PULONG)(KiSystemServiceRepeat + 3)+(KiSystemServiceRepeat + 7);
//	DbgPrint("KeServiceDescriptorTable : %p", KeServiceDescriptorTable);
//
//	ULONG index = *(PULONG)(KeServiceDescriptorTable + 0x10);
//	ULONG64 SSDT = *(PULONG64)KeServiceDescriptorTable;
//	for (size_t i = 0; i < index; i++)
//	{
//		PUCHAR func = (PUCHAR)(((PLONG)SSDT)[i] >> 4) + SSDT;
//		DbgPrint("index : %d  func : %p",i,func);
//	}
//}

//���غ���
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	DriverObject->DriverUnload = DriverUnload;
	G_DriverObject = DriverObject;

	//��ʼ������
	initKethreadFunc();

	//���ļ�
	//C:\Users\admin\Desktop
	//\\??\\C:\\Users\\admin\\Desktop\\TestDLL.dll
	PVOID filebuffer = NULL;
	ULONG64 filesize = NULL;
	ReadFile(L"\\??\\C:\\Users\\admin\\Desktop\\TestDLL.dll", &filebuffer, &filesize);
	DbgPrint("�ļ�����ַ�� %p ��С��%d", filebuffer, filesize);

	//ע��
	injectDll(L"notepad.exe", filebuffer, filesize);

	//�ͷ�filebuffer
	ExFreePool(filebuffer);

	DbgPrint("�������سɹ���");
	return STATUS_SUCCESS;
}