#include "utils.h"

HANDLE EnumProcessByZwQuerySysInfo(PUNICODE_STRING processName)
{
    PVOID	pBuftmp = NULL;
    ULONG	 dwRetSize = 0;
    NTSTATUS status = STATUS_SUCCESS;
    PSYSTEM_PROCESS_INFORMATION	pSysProcInfo = NULL;
    PEPROCESS	pEproc;

    //��ȡ��С
    status = ZwQuerySystemInformation(SystemProcessInformation, NULL, 0, &dwRetSize);
    //�����ڴ�
    pBuftmp = ExAllocatePool(NonPagedPool, dwRetSize);		//dwRetSize ��Ҫ�Ĵ�С
    if (pBuftmp != NULL)
    {
        //�ٴ�ִ��,��ö�ٽ���ŵ�ָ�����ڴ�����
        status = ZwQuerySystemInformation(SystemProcessInformation, pBuftmp, dwRetSize, NULL);
        if (NT_SUCCESS(status))
        {
            pSysProcInfo = (PSYSTEM_PROCESS_INFORMATION)pBuftmp;
            //ѭ������
            while (TRUE)
            {
                pEproc = NULL;

                DbgPrint("processname :%ws pid:%d", pSysProcInfo->ImageName.Buffer, pSysProcInfo->UniqueProcessId);

                if (!RtlCompareUnicodeString(&(pSysProcInfo->ImageName), processName ,TRUE )) {
                    DbgPrint("�ҵ��� %d", pSysProcInfo->UniqueProcessId);
                    ExFreePool(pBuftmp);
                    return pSysProcInfo->UniqueProcessId;
                }

                //ptagProc->NextEntryOffset==0 ��������������β��
                if (pSysProcInfo->NextEntryOffset == 0)
                    break;

                //��һ���ṹ
                pSysProcInfo = (PSYSTEM_PROCESS_INFORMATION)((ULONG64)pSysProcInfo + pSysProcInfo->NextEntryOffset);
            }
        }

        ExFreePool(pBuftmp);
    }


    return NULL;
}


PEPROCESS GetEprocessByName(LPCWSTR exeName)
{
    //for (int i = 4; i < 2147483648; i += 4)
    //{
    //    PEPROCESS pEpro = NULL;
    //    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    //    ntStatus = PsLookupProcessByProcessId((HANDLE)i, &pEpro);

    //    if (!NT_SUCCESS(ntStatus))
    //    {
    //        continue;
    //    }


    //    UCHAR* currentName = PsGetProcessImageFileName(pEpro);

    //    if (strcmp((const char*)currentName, exeName)) {
    //        ObDereferenceObject(pEpro); //������
    //        continue;
    //    }

    //    DbgPrint("��������Ϊ: %s ����PID = %d \r\n",
    //        PsGetProcessImageFileName(pEpro),
    //        PsGetProcessId(pEpro));

    //    ObDereferenceObject(pEpro); //������

    //    return pEpro;
    //}
    UNICODE_STRING proc_name = { 0 };
    RtlInitUnicodeString(&proc_name, exeName);
    HANDLE pid =  EnumProcessByZwQuerySysInfo(&proc_name);

    PEPROCESS pEpro = NULL;
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    ntStatus = PsLookupProcessByProcessId(pid, &pEpro);

    ObDereferenceObject(pEpro); //������
    if (!NT_SUCCESS(ntStatus))
    {
        return NULL;
    }


    return pEpro;
}

//lpcstrתlpcwstr
NTSTATUS ANSIToUNCODESTRING(LPCSTR source, PUNICODE_STRING  dst) {

    UNICODE_STRING wchar = {0};
    //RtlInitUnicodeString(wchar, L"");

    ANSI_STRING cchar = { 0 };
    RtlInitAnsiString(&cchar,source);
    RtlAnsiStringToUnicodeString(&wchar, &cchar,TRUE);

    *dst = wchar;
    
    return STATUS_SUCCESS;
}

PBYTE FindPattern_Wrapper(PUCHAR start, SIZE_T size, const char* Pattern)
{
    //find pattern utils
    #define InRange(x, a, b) (x >= a && x <= b) 
    #define GetBits(x) (InRange(x, '0', '9') ? (x - '0') : ((x - 'A') + 0xA))
    #define GetByte(x) ((BYTE)(GetBits(x[0]) << 4 | GetBits(x[1])))

    //get module range
    PBYTE ModuleStart = start;
    PBYTE ModuleEnd = (PBYTE)(ModuleStart + size);

    //scan pattern main
    PBYTE FirstMatch = nullptr;
    const char* CurPatt = Pattern;
    for (; ModuleStart < ModuleEnd; ++ModuleStart)
    {
        bool SkipByte = (*CurPatt == '\?');
        if (SkipByte || *ModuleStart == GetByte(CurPatt)) {
            if (!FirstMatch) FirstMatch = ModuleStart;
            SkipByte ? CurPatt += 2 : CurPatt += 3;
            if (CurPatt[-1] == 0) return FirstMatch;
        }

        else if (FirstMatch) {
            ModuleStart = FirstMatch;
            FirstMatch = nullptr;
            CurPatt = Pattern;
        }
    }

    return NULL;
}

NTSTATUS RemoteAllcateMemory(PEPROCESS process, SIZE_T size,PVOID * addr) {
    //����
    KAPC_STATE KAPC;
    if (!process) {
        DbgPrint("׼�����������ڴ�ʧ��");
        return STATUS_UNSUCCESSFUL;
    }

    KeStackAttachProcess(process, &KAPC);

    //����
    PVOID virtualbase = NULL;
    NTSTATUS AllocateStatus = NtAllocateVirtualMemory(NtCurrentProcess(), &virtualbase, NULL, &size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    if (!NT_SUCCESS(AllocateStatus)) {
        return STATUS_UNSUCCESSFUL;
    }
    //�ڴ��0
    memset(virtualbase, 0, size);

    DbgPrint("���������ڴ��ַ�� %p �����С��%llu", virtualbase, size);

    KeUnstackDetachProcess(&KAPC);
    //ObDereferenceObject(process);

    *addr = virtualbase;

    return STATUS_SUCCESS;
}

NTSTATUS RemoteFreeMemory(PEPROCESS process,PVOID addr,SIZE_T size) {
    //����
    KAPC_STATE KAPC;
    if (!process) {
        DbgPrint("׼�����������ڴ�ʧ��");
        return STATUS_UNSUCCESSFUL;
    }

    KeStackAttachProcess(process, &KAPC);

    memset(addr, 0, size);
    NTSTATUS freestatus =  NtFreeVirtualMemory(NtCurrentProcess(),&addr, &size, MEM_RELEASE);

    if (!NT_SUCCESS(freestatus)) {
        DbgPrint("�ͷ�ʧ��");
        return STATUS_UNSUCCESSFUL;
    }

    DbgPrint("�ͷ��ڴ� !");

    KeUnstackDetachProcess(&KAPC);

    return STATUS_SUCCESS;
}

bool GetNtoskrnlBase(PVOID* ntoskrnlBase, PULONG64 size) {
    //PVOID 
    ULONG bytes = 0;
    NTSTATUS QuerySystemInformationstatus = ZwQuerySystemInformation(SystemModuleInformation, 0, bytes, &bytes);
    if (bytes == 0)
    {
        DbgPrint("QuerySystemInformationstatus ��һ��ʧ��");
        return false;
    }

    PRTL_PROCESS_MODULES pMods = (PRTL_PROCESS_MODULES)ExAllocatePool(NonPagedPoolNx, bytes);
    RtlZeroMemory(pMods, bytes);

    QuerySystemInformationstatus = ZwQuerySystemInformation(SystemModuleInformation, pMods, bytes, &bytes);
    if (!NT_SUCCESS(QuerySystemInformationstatus)) {
        DbgPrint("QuerySystemInformationstatus �ڶ���ʧ��");
        return false;
    }

    //Ntoskrnl�϶��ǵ�һ��
    PRTL_PROCESS_MODULE_INFORMATION pMod = pMods->Modules;

    DbgPrint("ģ�飺 %s ��С��%d", pMod->FullPathName, pMod->ImageSize);
    *ntoskrnlBase = pMod->ImageBase;
    *size = pMod->ImageSize;

    return true;
}


