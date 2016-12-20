#include <ntifs.h>
#include <wdmsec.h>
#include <ntstrsafe.h>
#include <mountmgr.h>
#include <ntdddisk.h>
#include <ntddvol.h>

#include "defi.h"

//typedef struct _DEVICE_EXTENSION {
//	BOOLEAN                     media_in_device;
//	UNICODE_STRING              device_name;
//	ULONG                       device_number;
//	DEVICE_TYPE                 device_type;
//	HANDLE                      file_handle;
//	ANSI_STRING                 file_name;
//	LARGE_INTEGER               file_size;
//	BOOLEAN                     read_only;
//	PSECURITY_CLIENT_CONTEXT    security_client_context;
//	LIST_ENTRY                  list_head;
//	KSPIN_LOCK                  list_lock;
//	KEVENT                      request_event;
//	PVOID                       thread_pointer;
//	BOOLEAN                     terminate_thread;
//} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef struct _DEVICE_EXTENSION {
	UINT32 number;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef struct _MANAGER_DEVICE_EXTENSION {
	UINT32 number;
	struct mstructure
	{

		UNICODE_STRING              device_name;
		DEVICE_TYPE                 device_type;
		UCHAR						device_letter;

		PSECURITY_CLIENT_CONTEXT    security_client_context;
		LIST_ENTRY                  list_head;
		KSPIN_LOCK                  list_lock;
		KEVENT                      request_event;
		PVOID                       thread_pointer;
		BOOLEAN                     terminate_thread;

		HANDLE                      file_handle;
		ANSI_STRING                 file_name;
		LARGE_INTEGER               file_size;

		LARGE_INTEGER				SectorSize;
		PAesKey						MyKey;
		USHORT						DiskOffset;
	} mstruct;
} MDEVICE_EXTENSION, *PMDEVICE_EXTENSION;

typedef struct _STORAGE_DEVICE_EXTENSION {
	UINT32 number;
	struct mstructure2
	{
		//BOOLEAN                     media_in_device;
		UNICODE_STRING              device_name;
		DEVICE_TYPE                 device_type;
		//BOOLEAN                     read_only;
		PSECURITY_CLIENT_CONTEXT    security_client_context;
		LIST_ENTRY                  list_head;
		KSPIN_LOCK                  list_lock;
		KEVENT                      request_event;
		PVOID                       thread_pointer;
		BOOLEAN                     terminate_thread;
	} mstruct2;
} SDEVICE_EXTENSION, *PSDEVICE_EXTENSION;

USHORT n;
UNICODE_STRING gSymbolicLinkName;
//OBJECT_ATTRIBUTES           object_attributes;

NTSTATUS DriverEntry(IN PDRIVER_OBJECT   DriverObject, IN PUNICODE_STRING  RegistryPath);
NTSTATUS FileDiskCreateDevice(IN PDRIVER_OBJECT DriverObject, IN ULONG Number, IN DEVICE_TYPE DeviceType);
VOID FileDiskThread(IN PVOID Context);
VOID FileDiskUnload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS FileDiskCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS FileDiskReadWrite(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS FileDiskDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS FileDiskMakeFile(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
PDEVICE_OBJECT FileDiskDeleteDevice(IN PDEVICE_OBJECT DeviceObject);
VOID FileDiskUnmountAll(IN PDEVICE_OBJECT DeviceObject);
NTSTATUS FileDiskCloseFile(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp);

//#pragma alloc_text(INIT, DriverEntry)
//#pragma alloc_text(PAGE, FileDiskUnload)
#pragma code_seg("INIT")
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS                    status;
	UNICODE_STRING              device_dir_name;

	n = 0;
	RtlInitUnicodeString(&device_dir_name, DEVICE_DIR_NAME);

	gSymbolicLinkName.Buffer = (PWCHAR)ExAllocatePoolWithTag(PagedPool, MAXIMUM_FILENAME_LENGTH * 2, FILE_DISK_POOL_TAG);
	gSymbolicLinkName.Length = 0;
	gSymbolicLinkName.MaximumLength = MAXIMUM_FILENAME_LENGTH * 2;

	status = FileDiskCreateDevice(DriverObject, n, FILE_DEVICE_NULL);

	DriverObject->MajorFunction[IRP_MJ_CREATE] = FileDiskCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = FileDiskCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = FileDiskReadWrite;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = FileDiskReadWrite;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = FileDiskDeviceControl;

	DriverObject->DriverUnload = FileDiskUnload;

	return STATUS_SUCCESS;
}
#pragma code_seg("PAGE")
VOID FileDiskUnload(IN PDRIVER_OBJECT DriverObject)
{
	PDEVICE_OBJECT device_object;
	PAGED_CODE();
	device_object = DriverObject->DeviceObject;
	while (device_object)
	{
		device_object = FileDiskDeleteDevice(device_object);
	}
	IoDeleteSymbolicLink(&gSymbolicLinkName);
}

PDEVICE_OBJECT FileDiskDeleteDevice(IN PDEVICE_OBJECT DeviceObject)
{
	PMDEVICE_EXTENSION   mdevice_extension;
	PDEVICE_OBJECT      next_device_object;

	ASSERT(DeviceObject != NULL);

	mdevice_extension = (PMDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	mdevice_extension->number = INVALID_NUMBER_DEV;

	mdevice_extension->mstruct.terminate_thread = TRUE;

	KeSetEvent(&mdevice_extension->mstruct.request_event,(KPRIORITY)0,FALSE);

	KeWaitForSingleObject(mdevice_extension->mstruct.thread_pointer,Executive,KernelMode,FALSE,NULL);

	ObDereferenceObject(mdevice_extension->mstruct.thread_pointer);

	if (mdevice_extension->mstruct.device_name.Buffer != NULL)
	{
		ExFreePool(mdevice_extension->mstruct.device_name.Buffer);
	}

	if (mdevice_extension->mstruct.security_client_context != NULL)
	{
		SeDeleteClientSecurity(mdevice_extension->mstruct.security_client_context);
		ExFreePool(mdevice_extension->mstruct.security_client_context);
	}

	next_device_object = DeviceObject->NextDevice;

	IoDeleteDevice(DeviceObject);
	n--;
	return next_device_object;
}

VOID FileDiskUnmountAll(IN PDEVICE_OBJECT DeviceObject)
{
	PMDEVICE_EXTENSION mdevice_extension;//
	mdevice_extension = (PMDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	while (mdevice_extension->number)//while (device_object)
	{
		ExFreePool(mdevice_extension->mstruct.file_name.Buffer);
		ZwClose(mdevice_extension->mstruct.file_handle);
		//mdevice_extension->number = INVALID_NUMBER_DEV;
		DeviceObject = FileDiskDeleteDevice(DeviceObject);
		mdevice_extension = (PMDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	}
}
	
#pragma code_seg()

NTSTATUS FileDiskCreateDevice(IN PDRIVER_OBJECT DriverObject, IN ULONG Number, IN DEVICE_TYPE DeviceType)
{
	UNICODE_STRING      device_name;
	NTSTATUS            status;
	PDEVICE_OBJECT      device_object;
	PMDEVICE_EXTENSION  mdevice_extension;
	HANDLE              thread_handle;
	UNICODE_STRING      sddl;

	device_name.Buffer = (PWCHAR)ExAllocatePoolWithTag(PagedPool, MAXIMUM_FILENAME_LENGTH * 2, FILE_DISK_POOL_TAG);
	device_name.Length = 0;
	device_name.MaximumLength = MAXIMUM_FILENAME_LENGTH * 2;

	RtlInitUnicodeString(&sddl, _T("D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;BU)"));

	RtlUnicodeStringPrintf(&device_name, DEVICE_NAME_PREFIX L"%u", Number);//нет для CDrom

	status = IoCreateDeviceSecure(
		DriverObject,
		sizeof(MDEVICE_EXTENSION),
		&device_name,
		DeviceType,
		0,
		FALSE,
		&sddl,
		NULL,
		&device_object
	);
	if (!NT_SUCCESS(status))
	{
		ExFreePool(device_name.Buffer);
		return status;
	}


	if (n == 0)
	{
		RtlInitUnicodeString(&gSymbolicLinkName, SYMBLINK0);//формирование имени симв.ссылки
		status = IoCreateSymbolicLink(&gSymbolicLinkName, &device_name);
		if (status != STATUS_SUCCESS)
			return status;
	}
	else
	{
		//RtlInitUnicodeString(&gSymbolicLinkName1, SYMBLINK1);//формирование имени симв.ссылки
		//status = IoCreateSymbolicLink(&gSymbolicLinkName1, &device_name);
		//if (status != STATUS_SUCCESS)
		//	return status;
	}

	device_object->Flags |= DO_DIRECT_IO;
	//////////////
	if (n == 0)
	{
		mdevice_extension = (PMDEVICE_EXTENSION)device_object->DeviceExtension;

		mdevice_extension->mstruct.device_name.Length = device_name.Length;
		mdevice_extension->mstruct.device_name.MaximumLength = device_name.MaximumLength;
		mdevice_extension->mstruct.device_name.Buffer = device_name.Buffer;
		mdevice_extension->number = Number;
		mdevice_extension->mstruct.device_type = DeviceType;
		mdevice_extension->mstruct.file_handle = NULL;
	}
	else
	{
		mdevice_extension = (PMDEVICE_EXTENSION)device_object->DeviceExtension;

		mdevice_extension->mstruct.device_name.Length = device_name.Length;
		mdevice_extension->mstruct.device_name.MaximumLength = device_name.MaximumLength;
		mdevice_extension->mstruct.device_name.Buffer = device_name.Buffer;
		mdevice_extension->number = Number;
		mdevice_extension->mstruct.device_type = DeviceType;
		mdevice_extension->mstruct.file_handle = NULL;
		device_object->Flags &= ~DO_DEVICE_INITIALIZING;
	}
	InitializeListHead(&mdevice_extension->mstruct.list_head);
	KeInitializeSpinLock(&mdevice_extension->mstruct.list_lock);
	KeInitializeEvent(&mdevice_extension->mstruct.request_event, SynchronizationEvent, FALSE);

	mdevice_extension->mstruct.terminate_thread = FALSE;

	status = PsCreateSystemThread(
		&thread_handle,
		(ACCESS_MASK)0L,
		NULL,
		NULL,
		NULL,
		FileDiskThread,
		device_object);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(device_object);
		ExFreePool(device_name.Buffer);
		return status;
	}

	status = ObReferenceObjectByHandle(
		thread_handle,
		THREAD_ALL_ACCESS,
		NULL,
		KernelMode,
		&mdevice_extension->mstruct.thread_pointer,
		NULL
	);

	n++;
	ZwClose(thread_handle);
	return STATUS_SUCCESS;
}


NTSTATUS FileDiskCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = FILE_OPENED;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS FileDiskReadWrite(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PMDEVICE_EXTENSION  mdevice_extension;
	PIO_STACK_LOCATION  io_stack;
	DbgPrint("Going into FileDiskReadWrite \n");
	mdevice_extension = (PMDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	if (mdevice_extension->mstruct.file_handle == NULL)
	{
		Irp->IoStatus.Status = STATUS_NO_MEDIA_IN_DEVICE;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);

		return STATUS_NO_MEDIA_IN_DEVICE;
	}

	io_stack = IoGetCurrentIrpStackLocation(Irp);

	if (io_stack->Parameters.Read.Length == 0)
	{
		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);

		return STATUS_SUCCESS;
	}

	IoMarkIrpPending(Irp);

	ExInterlockedInsertTailList(&mdevice_extension->mstruct.list_head,&Irp->Tail.Overlay.ListEntry,&mdevice_extension->mstruct.list_lock);

	KeSetEvent(&mdevice_extension->mstruct.request_event,(KPRIORITY)0,FALSE);

	return /*STATUS_SUCCESS*/STATUS_PENDING;
}

NTSTATUS FileDiskDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PMDEVICE_EXTENSION  mdevice_extension;
	PIO_STACK_LOCATION  io_stack;
	NTSTATUS            status;
	status = 0;
	mdevice_extension = (PMDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	io_stack = IoGetCurrentIrpStackLocation(Irp);

	switch (io_stack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_FILE_DISK_OPEN_FILE:
	{
		SECURITY_QUALITY_OF_SERVICE security_quality_of_service;
		if (mdevice_extension->mstruct.security_client_context != NULL)
		{
			SeDeleteClientSecurity(mdevice_extension->mstruct.security_client_context);
		}
		else
		{
			mdevice_extension->mstruct.security_client_context =
				ExAllocatePoolWithTag(NonPagedPool, sizeof(SECURITY_CLIENT_CONTEXT), FILE_DISK_POOL_TAG);
		}
		RtlZeroMemory(&security_quality_of_service, sizeof(SECURITY_QUALITY_OF_SERVICE));

		security_quality_of_service.Length = sizeof(SECURITY_QUALITY_OF_SERVICE);
		security_quality_of_service.ImpersonationLevel = SecurityImpersonation;
		security_quality_of_service.ContextTrackingMode = SECURITY_STATIC_TRACKING;
		security_quality_of_service.EffectiveOnly = FALSE;

		SeCreateClientSecurity(
			PsGetCurrentThread(),
			&security_quality_of_service,
			FALSE,
			mdevice_extension->mstruct.security_client_context);
		IoMarkIrpPending(Irp);

		ExInterlockedInsertTailList(
			&mdevice_extension->mstruct.list_head,
			&Irp->Tail.Overlay.ListEntry,
			&mdevice_extension->mstruct.list_lock);

		KeSetEvent(&mdevice_extension->mstruct.request_event, (KPRIORITY)0, FALSE);
		status = STATUS_PENDING;
		break;
	}
	
	case IOCTL_FILE_DISK_CLOSE_FILE:
	{
		IoMarkIrpPending(Irp);
	
		ExInterlockedInsertTailList(&mdevice_extension->mstruct.list_head,
				&Irp->Tail.Overlay.ListEntry,
				&mdevice_extension->mstruct.list_lock);
		
		KeSetEvent(&mdevice_extension->mstruct.request_event,
					(KPRIORITY)0,
					FALSE);

		status = STATUS_PENDING;
		
		break;
	}
	case IOCTL_FILE_DISK_UNMOUNT_ALL:
	{
		//IoMarkIrpPending(Irp);

		//ExInterlockedInsertTailList(&mdevice_extension->mstruct.list_head,
		//	&Irp->Tail.Overlay.ListEntry,
		//	&mdevice_extension->mstruct.list_lock);

		//KeSetEvent(&mdevice_extension->mstruct.request_event,
		//	(KPRIORITY)0,
		//	FALSE);

		//status = STATUS_PENDING;
		PDEVICE_OBJECT myDevice;
		myDevice = DeviceObject->DriverObject->DeviceObject;
		FileDiskUnmountAll(myDevice);
		status = STATUS_SUCCESS;
		break;
	}
	case IOCTL_FILE_DISK_QUERY_FILE:
	{
		PDISK_INFO myDisk_info;
		if (io_stack->Parameters.DeviceIoControl.OutputBufferLength < 
			sizeof(DISK_INFO) + 26 * (sizeof(OPEN_FILE_INFORMATION) + MAX_PATH1))
		{
			DbgPrint("Miss size\n");
			status = STATUS_BUFFER_TOO_SMALL;
			Irp->IoStatus.Information = 0;
			break;
		}
		myDisk_info = (PDISK_INFO)Irp->AssociatedIrp.SystemBuffer;
		if (n == 1)
		{
			myDisk_info->DiskCount = 0;
			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof(PDISK_INFO) + 26 * (sizeof(OPEN_FILE_INFORMATION) + MAX_PATH1);
			break;
		}
		USHORT k = 0;
		//USHORT z = 1;
		DEVICE_OBJECT device_obj;
		device_obj = *(DeviceObject->DriverObject->DeviceObject);
		PMDEVICE_EXTENSION  mdevice_extension1;
		mdevice_extension1 = (PMDEVICE_EXTENSION)device_obj.DeviceExtension;
		while (mdevice_extension1->number)
		{
			if (mdevice_extension1->number != INVALID_NUMBER_DEV)
			{
				myDisk_info->disks[k].DriveLetter = mdevice_extension1->mstruct.device_letter;
				myDisk_info->disks[k].FileSize.QuadPart = mdevice_extension1->mstruct.file_size.QuadPart;
				myDisk_info->disks[k].FileNameLength = mdevice_extension1->mstruct.file_name.Length;
				RtlCopyMemory(myDisk_info->disks[k].FileName,
					mdevice_extension1->mstruct.file_name.Buffer,
					mdevice_extension1->mstruct.file_name.Length);
				k++;
			}
			device_obj = *device_obj.NextDevice;
			mdevice_extension1 = (PMDEVICE_EXTENSION)device_obj.DeviceExtension;
		}
		myDisk_info->DiskCount = k;
		status = STATUS_SUCCESS;
		
		Irp->IoStatus.Information = sizeof(PDISK_INFO) + 26 * (sizeof(OPEN_FILE_INFORMATION) + MAX_PATH1);

		break;
	}
	case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME:
	{
		PMOUNTDEV_NAME name;

		if (io_stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(MOUNTDEV_NAME))
		{
			status = STATUS_INVALID_PARAMETER;
			Irp->IoStatus.Information = 0;
			break;
		}

		name = (PMOUNTDEV_NAME)Irp->AssociatedIrp.SystemBuffer;
		name->NameLength = mdevice_extension->mstruct.device_name.Length * sizeof(WCHAR);

		if (io_stack->Parameters.DeviceIoControl.OutputBufferLength <
			name->NameLength + sizeof(USHORT))
		{
			status = STATUS_BUFFER_OVERFLOW;
			Irp->IoStatus.Information = sizeof(MOUNTDEV_NAME);
			break;
		}

		RtlCopyMemory(name->Name, mdevice_extension->mstruct.device_name.Buffer, name->NameLength);

		status = STATUS_SUCCESS;
		Irp->IoStatus.Information = name->NameLength + sizeof(USHORT);

		break;
	}
	case IOCTL_DISK_CHECK_VERIFY:
	case IOCTL_STORAGE_CHECK_VERIFY:
	case IOCTL_STORAGE_CHECK_VERIFY2:
	{
		status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;
		break;
	}
	case IOCTL_DISK_GET_DRIVE_GEOMETRY:
	{
		PDISK_GEOMETRY  disk_geometry;
		ULONGLONG       length;
		ULONG           sector_size;
	
		//if (io_stack->Parameters.DeviceIoControl.OutputBufferLength <	sizeof(DISK_GEOMETRY))
		//{
		//	status = STATUS_BUFFER_TOO_SMALL;
		//	Irp->IoStatus.Information = 0;
		//	break;
		//}
	
		disk_geometry = (PDISK_GEOMETRY)Irp->AssociatedIrp.SystemBuffer;
	
		length = mdevice_extension->mstruct.file_size.QuadPart;
	
		if (mdevice_extension->mstruct.device_type != FILE_DEVICE_CD_ROM)
		{
			sector_size = 512;
		}
		else
		{
			sector_size = 2048;
		}
	
		disk_geometry->Cylinders.QuadPart = length / sector_size / 32 / 2;
		disk_geometry->MediaType = FixedMedia;
		disk_geometry->TracksPerCylinder = 2;
		disk_geometry->SectorsPerTrack = 32;
		disk_geometry->BytesPerSector = sector_size;
	
		status = STATUS_SUCCESS;
		Irp->IoStatus.Information = sizeof(DISK_GEOMETRY);
	
		break;
	}
	case IOCTL_DISK_GET_LENGTH_INFO:
	{
		PGET_LENGTH_INFORMATION get_length_information;
	
		//if (io_stack->Parameters.DeviceIoControl.OutputBufferLength <
		//		sizeof(GET_LENGTH_INFORMATION))
		//{
		//	status = STATUS_BUFFER_TOO_SMALL;
		//	Irp->IoStatus.Information = 0;
		//	break;
		//}
	
		get_length_information = (PGET_LENGTH_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
	
		get_length_information->Length.QuadPart = mdevice_extension->mstruct.file_size.QuadPart;
	
		status = STATUS_SUCCESS;
		Irp->IoStatus.Information = sizeof(GET_LENGTH_INFORMATION);
	
		break;
	}
		case IOCTL_DISK_GET_PARTITION_INFO:
		{
			PPARTITION_INFORMATION  partition_information;
			ULONGLONG               length;
			DbgPrint("IOCTL_DISK_GET_PARTITION_INFO\n");
			if (io_stack->Parameters.DeviceIoControl.OutputBufferLength <
				sizeof(PARTITION_INFORMATION))
			{
				status = STATUS_BUFFER_TOO_SMALL;
				Irp->IoStatus.Information = 0;
				break;
			}
	
			partition_information = (PPARTITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
	
			length = mdevice_extension->mstruct.file_size.QuadPart;
	
			partition_information->StartingOffset.QuadPart = 0;
			//partition_information->StartingOffset.QuadPart = (LONGLONG)mdevice_extension->mstruct.DiskOffset;
			partition_information->PartitionLength.QuadPart = length;
			partition_information->HiddenSectors = 1;
			partition_information->PartitionNumber = 0;
			partition_information->PartitionType = 0;
			partition_information->BootIndicator = FALSE;
			partition_information->RecognizedPartition = FALSE;
			partition_information->RewritePartition = FALSE;
	
			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof(PARTITION_INFORMATION);
	
			break;
		}
	
		case IOCTL_DISK_GET_PARTITION_INFO_EX:
		{
			PPARTITION_INFORMATION_EX   partition_information_ex;
			ULONGLONG                   length;
			DbgPrint("IOCTL_DISK_GET_PARTITION_INFO_EX\n");
			if (io_stack->Parameters.DeviceIoControl.OutputBufferLength <
				sizeof(PARTITION_INFORMATION_EX))
			{
				status = STATUS_BUFFER_TOO_SMALL;
				Irp->IoStatus.Information = 0;
				break;
			}
	
			partition_information_ex = (PPARTITION_INFORMATION_EX)Irp->AssociatedIrp.SystemBuffer;
	
			length = mdevice_extension->mstruct.file_size.QuadPart;
	
			partition_information_ex->PartitionStyle = PARTITION_STYLE_MBR;
			partition_information_ex->StartingOffset.QuadPart = 0;
			//partition_information_ex->StartingOffset.QuadPart = (LONGLONG)mdevice_extension->mstruct.DiskOffset;
			partition_information_ex->PartitionLength.QuadPart = length;
			partition_information_ex->PartitionNumber = 0;
			partition_information_ex->RewritePartition = FALSE;
			partition_information_ex->Mbr.PartitionType = 0;
			partition_information_ex->Mbr.BootIndicator = FALSE;
			partition_information_ex->Mbr.RecognizedPartition = FALSE;
			partition_information_ex->Mbr.HiddenSectors = 1;
	
			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof(PARTITION_INFORMATION_EX);
	
			break;
		}
	
		case IOCTL_DISK_IS_WRITABLE:
		{
			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = 0;
			break;
		}
	
		case IOCTL_DISK_MEDIA_REMOVAL:
		case IOCTL_STORAGE_MEDIA_REMOVAL:
		{
			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = 0;
			break;
		}
	
		case IOCTL_DISK_SET_PARTITION_INFO:
		{
			status = STATUS_MEDIA_WRITE_PROTECTED;
			Irp->IoStatus.Information = 0;
			break;
			//if (io_stack->Parameters.DeviceIoControl.InputBufferLength <
			//	sizeof(SET_PARTITION_INFORMATION))
			//{
			//	status = STATUS_INVALID_PARAMETER;
			//	Irp->IoStatus.Information = 0;
			//	break;
			//}
		}
	
		case IOCTL_DISK_VERIFY:
		{
			PVERIFY_INFORMATION verify_information;
	
			if (io_stack->Parameters.DeviceIoControl.InputBufferLength <
				sizeof(VERIFY_INFORMATION))
			{
				status = STATUS_INVALID_PARAMETER;
				Irp->IoStatus.Information = 0;
				break;
			}
	
			verify_information = (PVERIFY_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
	
			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = verify_information->Length;
	
			break;
		}
	
		case IOCTL_STORAGE_GET_DEVICE_NUMBER:
		{
			PSTORAGE_DEVICE_NUMBER number;
	
			if (io_stack->Parameters.DeviceIoControl.OutputBufferLength <
				sizeof(STORAGE_DEVICE_NUMBER))
			{
				status = STATUS_BUFFER_TOO_SMALL;
				Irp->IoStatus.Information = 0;
				break;
			}
	
			number = (PSTORAGE_DEVICE_NUMBER)Irp->AssociatedIrp.SystemBuffer;
	
			number->DeviceType = mdevice_extension->mstruct.device_type;
			number->DeviceNumber = mdevice_extension->number;
			number->PartitionNumber = (ULONG)-1;
	
			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof(STORAGE_DEVICE_NUMBER);
	
			break;
		}
	
		case IOCTL_STORAGE_GET_HOTPLUG_INFO:
		{
			PSTORAGE_HOTPLUG_INFO info;
	
			if (io_stack->Parameters.DeviceIoControl.OutputBufferLength <
				sizeof(STORAGE_HOTPLUG_INFO))
			{
				status = STATUS_BUFFER_TOO_SMALL;
				Irp->IoStatus.Information = 0;
				break;
			}
	
			info = (PSTORAGE_HOTPLUG_INFO)Irp->AssociatedIrp.SystemBuffer;
	
			info->Size = sizeof(STORAGE_HOTPLUG_INFO);
			info->MediaRemovable = 0;
			info->MediaHotplug = 0;
			info->DeviceHotplug = 0;
			info->WriteCacheEnableOverride = 0;
	
			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof(STORAGE_HOTPLUG_INFO);
	
			break;
		}
	
		case IOCTL_VOLUME_GET_GPT_ATTRIBUTES:
		{
			PVOLUME_GET_GPT_ATTRIBUTES_INFORMATION attr;
	
			if (io_stack->Parameters.DeviceIoControl.OutputBufferLength <
				sizeof(VOLUME_GET_GPT_ATTRIBUTES_INFORMATION))
			{
				status = STATUS_BUFFER_TOO_SMALL;
				Irp->IoStatus.Information = 0;
				break;
			}
	
			attr = (PVOLUME_GET_GPT_ATTRIBUTES_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
	
			attr->GptAttributes = 0;
	
			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof(VOLUME_GET_GPT_ATTRIBUTES_INFORMATION);
	
			break;
		}
	
		case IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS:
		{
			PVOLUME_DISK_EXTENTS ext;
			DbgPrint("IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS\n");
			if (io_stack->Parameters.DeviceIoControl.OutputBufferLength <
				sizeof(VOLUME_DISK_EXTENTS))
			{
				status = STATUS_INVALID_PARAMETER;
				Irp->IoStatus.Information = 0;
				break;
			}
			/*
			// not needed since there is only one disk extent to return
			if (io_stack->Parameters.DeviceIoControl.OutputBufferLength <
			sizeof(VOLUME_DISK_EXTENTS) + ((NumberOfDiskExtents - 1) * sizeof(DISK_EXTENT)))
			{
			status = STATUS_BUFFER_OVERFLOW;
			Irp->IoStatus.Information = 0;
			break;
			}
			*/
			ext = (PVOLUME_DISK_EXTENTS)Irp->AssociatedIrp.SystemBuffer;
	
			ext->NumberOfDiskExtents = 1;
			ext->Extents[0].DiskNumber = mdevice_extension->number;
			ext->Extents[0].StartingOffset.QuadPart = 0/*(LONGLONG)mdevice_extension->mstruct.DiskOffset*/;
			ext->Extents[0].ExtentLength.QuadPart = mdevice_extension->mstruct.file_size.QuadPart;
	
			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof(VOLUME_DISK_EXTENTS) /*+ ((NumberOfDiskExtents - 1) * sizeof(DISK_EXTENT))*/;
	
			break;
		}
	
	#if (NTDDI_VERSION < NTDDI_VISTA)
	#define IOCTL_DISK_IS_CLUSTERED CTL_CODE(IOCTL_DISK_BASE, 0x003e, METHOD_BUFFERED, FILE_ANY_ACCESS)
	#endif  // NTDDI_VERSION < NTDDI_VISTA
	
		case IOCTL_DISK_IS_CLUSTERED:
		{
			PBOOLEAN clus;
	
			if (io_stack->Parameters.DeviceIoControl.OutputBufferLength <
				sizeof(BOOLEAN))
			{
				status = STATUS_BUFFER_TOO_SMALL;
				Irp->IoStatus.Information = 0;
				break;
			}
	
			clus = (PBOOLEAN)Irp->AssociatedIrp.SystemBuffer;
	
			*clus = FALSE;
	
			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof(BOOLEAN);
	
			break;
		}
	default:
	{
		KdPrint(("FileDisk: Unknown IoControlCode %#x\n", io_stack->Parameters.DeviceIoControl.IoControlCode));
		status = STATUS_INVALID_DEVICE_REQUEST;
		Irp->IoStatus.Information = 0;
	}
	}

	if (status != STATUS_PENDING)
	{
		Irp->IoStatus.Status = status;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);
	}

	return status;
}

NTSTATUS FileDiskMakeFile(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PMDEVICE_EXTENSION              mdevice_extension;
	POPEN_FILE_INFORMATION          open_file_information;
	UNICODE_STRING                  ufile_name;
	NTSTATUS                        status;
	OBJECT_ATTRIBUTES               object_attributes;
	//FILE_END_OF_FILE_INFORMATION    file_eof;
	//FILE_STANDARD_INFORMATION       file_standard;
	FILE_ALIGNMENT_INFORMATION      file_alignment;
	//ANSI_STRING						pass;

	mdevice_extension = (PMDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	open_file_information = (POPEN_FILE_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

	mdevice_extension->mstruct.DiskOffset = open_file_information->DiskOffset;
	mdevice_extension->mstruct.SectorSize.QuadPart = open_file_information->SectorSize.QuadPart;
	PCHAR MPAss;
	MPAss = MmAllocateNonCachedMemory(open_file_information->PasswordLength);
	RtlCopyMemory(MPAss, open_file_information->Password, open_file_information->PasswordLength);
	//pass.Length = open_file_information->PasswordLength;
	//pass.MaximumLength = open_file_information->PasswordLength;
	//pass.Buffer = ExAllocatePoolWithTag(NonPagedPool, pass.Length, FILE_DISK_POOL_TAG);
	//RtlCopyMemory(pass.Buffer, open_file_information->Password, pass.Length);
	mdevice_extension->mstruct.MyKey = MmAllocateNonCachedMemory(sizeof(AesKey));
	MakeAesKey(mdevice_extension->mstruct.MyKey, /*MyPass*/MPAss);

	mdevice_extension->mstruct.device_letter = open_file_information->DriveLetter;

	mdevice_extension->mstruct.file_name.Length = open_file_information->FileNameLength;
	mdevice_extension->mstruct.file_name.MaximumLength = open_file_information->FileNameLength;
	mdevice_extension->mstruct.file_name.Buffer = ExAllocatePoolWithTag(NonPagedPool, open_file_information->FileNameLength, FILE_DISK_POOL_TAG);

	if (mdevice_extension->mstruct.file_name.Buffer == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlCopyMemory(mdevice_extension->mstruct.file_name.Buffer,open_file_information->FileName,open_file_information->FileNameLength);

	status = RtlAnsiStringToUnicodeString(
		&ufile_name,
		&mdevice_extension->mstruct.file_name,
		TRUE);

	InitializeObjectAttributes(
		&object_attributes,
		&ufile_name,
		OBJ_CASE_INSENSITIVE,
		NULL,
		NULL
	);

	status = ZwCreateFile(
		&mdevice_extension->mstruct.file_handle,
		GENERIC_READ | GENERIC_WRITE,
		&object_attributes,
		&Irp->IoStatus,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FILE_OPEN,
		FILE_NON_DIRECTORY_FILE |
		FILE_RANDOM_ACCESS |
		FILE_NO_INTERMEDIATE_BUFFERING |
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0);

	if (NT_SUCCESS(status))
	{
		KdPrint(("FileDisk: File %.*S opened.\n", ufile_name.Length / 2, ufile_name.Buffer));
	}
	/*file_eof.EndOfFile.QuadPart = open_file_information->FileSize.QuadPart;

	status = ZwSetInformationFile(
		mdevice_extension->mstruct.file_handle,
		&Irp->IoStatus,
		&file_eof,
		sizeof(FILE_END_OF_FILE_INFORMATION),
		FileEndOfFileInformation
	);*/
	/*status = ZwQueryInformationFile(
		mdevice_extension->mstruct.file_handle,
		&Irp->IoStatus,
		&file_standard,
		sizeof(FILE_STANDARD_INFORMATION),
		FileStandardInformation
	);

	if (!NT_SUCCESS(status))
	{
		ExFreePool(mdevice_extension->mstruct.file_name.Buffer);
		ZwClose(mdevice_extension->mstruct.file_handle);
		return status;
	}*/
	
	mdevice_extension->mstruct.file_size.QuadPart = open_file_information->FileSize.QuadPart/*file_standard.EndOfFile.QuadPart*/;

	status = ZwQueryInformationFile(
		mdevice_extension->mstruct.file_handle,
		&Irp->IoStatus,
		&file_alignment,
		sizeof(FILE_ALIGNMENT_INFORMATION),
		FileAlignmentInformation
	);

	if (!NT_SUCCESS(status))
	{
		ExFreePool(mdevice_extension->mstruct.file_name.Buffer);
		ZwClose(mdevice_extension->mstruct.file_handle);
		return status;
	}

	DeviceObject->AlignmentRequirement = file_alignment.AlignmentRequirement;


	DeviceObject->Characteristics &= ~FILE_READ_ONLY_DEVICE;

	Irp->IoStatus.Status = /*STATUS_PENDING*/STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	return /*STATUS_PENDING*/STATUS_SUCCESS;
}

NTSTATUS FileDiskCloseFile(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	PMDEVICE_EXTENSION mdevice_extension;
	DbgPrint("Going into FileDiskCloseFile\n");
	PAGED_CODE();

	ASSERT(DeviceObject != NULL);
	ASSERT(Irp != NULL);

	mdevice_extension = (PMDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	ExFreePool(mdevice_extension->mstruct.file_name.Buffer);

	ZwClose(mdevice_extension->mstruct.file_handle);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
#pragma warning( disable : 4242 4244)
VOID FileDiskThread(IN PVOID Context)
{
	PDEVICE_OBJECT      device_object;
	PMDEVICE_EXTENSION  mdevice_extension;
	PLIST_ENTRY         request;
	PIRP                irp;
	PIO_STACK_LOCATION  io_stack;
	PUCHAR              system_buffer;
	//PUCHAR              buffer;
	HANDLE              thread_handle;

	DbgPrint("Going into FileDiskThread\n");

	device_object = (PDEVICE_OBJECT)Context;
	mdevice_extension = (PMDEVICE_EXTENSION)device_object->DeviceExtension;

	KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);

	//FileDiskAdjustPrivilege(SE_IMPERSONATE_PRIVILEGE, TRUE);

	while (1)
	{
		KeWaitForSingleObject(
			&mdevice_extension->mstruct.request_event,
			Executive,
			KernelMode,
			FALSE,
			NULL
		);

		if (mdevice_extension->mstruct.terminate_thread)
		{
			PsTerminateSystemThread(STATUS_SUCCESS);
		}

		while ((request = ExInterlockedRemoveHeadList(
			&mdevice_extension->mstruct.list_head,
			&mdevice_extension->mstruct.list_lock)) != NULL)
		{
			irp = CONTAINING_RECORD(request, IRP, Tail.Overlay.ListEntry);

			io_stack = IoGetCurrentIrpStackLocation(irp);
			switch (io_stack->MajorFunction)
			{
			case IRP_MJ_READ:
				//system_buffer = (PUCHAR)MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
				//if (system_buffer == NULL)
				//{
				//	irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				//	irp->IoStatus.Information = 0;
				//	break;
				//}
				//buffer = (PUCHAR)ExAllocatePoolWithTag(PagedPool, io_stack->Parameters.Read.Length, FILE_DISK_POOL_TAG);
				//if (buffer == NULL)
				//{
				//	irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				//	irp->IoStatus.Information = 0;
				//	break;
				//}
				//ZwReadFile(
				//	mdevice_extension->mstruct.file_handle,
				//	NULL,
				//	NULL,
				//	NULL,
				//	&irp->IoStatus,
				//	buffer,
				//	io_stack->Parameters.Read.Length,
				//	&io_stack->Parameters.Read.ByteOffset,
				//	NULL
				//);
				////
				//RtlCopyMemory(system_buffer, buffer, io_stack->Parameters.Read.Length);
				//ExFreePool(buffer);

				system_buffer = (PUCHAR)MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
				if (system_buffer == NULL)
				{
					irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					irp->IoStatus.Information = 0;
					break;
				}

				AesKey* key = mdevice_extension->mstruct.MyKey;
				LONGLONG SectorSize = mdevice_extension->mstruct.SectorSize.QuadPart;
				const SIZE_T blocks_count = io_stack->Parameters.Read.Length / SectorSize;
				const SIZE_T buffer_length = io_stack->Parameters.Read.Length;
				PUCHAR encrypted_buffer = (PUCHAR)ExAllocatePoolWithTag(PagedPool, buffer_length, FILE_DISK_POOL_TAG);
				if (encrypted_buffer == NULL)
				{
					irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					irp->IoStatus.Information = 0;
					break;
				}

				ZwReadFile(
					mdevice_extension->mstruct.file_handle,
					NULL,
					NULL,
					NULL,
					&irp->IoStatus,
					encrypted_buffer,
					buffer_length,
					&io_stack->Parameters.Read.ByteOffset,
					NULL
				);

				PUCHAR decrypted_block_buffer = (PUCHAR)ExAllocatePoolWithTag(PagedPool, SectorSize, FILE_DISK_POOL_TAG);
				if (decrypted_block_buffer == NULL)
				{
					irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					irp->IoStatus.Information = 0;
					break;
				}

				PUCHAR iv = (PUCHAR)ExAllocatePoolWithTag(PagedPool, sizeof(unsigned char) * key->blockSize, FILE_DISK_POOL_TAG);
				if (iv == NULL)
				{
					irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					irp->IoStatus.Information = 0;
					break;
				}

				LONGLONG offsetInBlocks = io_stack->Parameters.Read.ByteOffset.QuadPart / SectorSize;
				LONGLONG memoryOffset = 0;
				for (ULONG block_index = 0; block_index < blocks_count; block_index++)
				{
					PrepareIv(key, block_index + offsetInBlocks, iv);
					EncrypDecryptData(key, encrypted_buffer + memoryOffset, SectorSize, decrypted_block_buffer, SectorSize, FALSE, iv);
					RtlCopyMemory(system_buffer + memoryOffset, decrypted_block_buffer, SectorSize);

					memoryOffset += SectorSize;
				}

				ExFreePool(decrypted_block_buffer);
				ExFreePool(encrypted_buffer);
				ExFreePool(iv);

				break;

			case IRP_MJ_WRITE:
				//if ((io_stack->Parameters.Write.ByteOffset.QuadPart +
				//	io_stack->Parameters.Write.Length) >
				//	mdevice_extension->mstruct.file_size.QuadPart)
				//{
				//	irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
				//	irp->IoStatus.Information = 0;
				//	break;
				//}
				////
				//ZwWriteFile(
				//	mdevice_extension->mstruct.file_handle,
				//	NULL,
				//	NULL,
				//	NULL,
				//	&irp->IoStatus,
				//	MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority),
				//	io_stack->Parameters.Write.Length,
				//	&io_stack->Parameters.Write.ByteOffset,
				//	NULL
				//);
				if ((io_stack->Parameters.Write.ByteOffset.QuadPart +
					io_stack->Parameters.Write.Length) >
					mdevice_extension->mstruct.file_size.QuadPart)
				{
					irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
					irp->IoStatus.Information = 0;
					break;
				}

				system_buffer = (PUCHAR)MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
				if (system_buffer == NULL)
				{
					irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					irp->IoStatus.Information = 0;
					break;
				}

				AesKey* key_wr = mdevice_extension->mstruct.MyKey;
				LONGLONG SectorSize_wr = mdevice_extension->mstruct.SectorSize.QuadPart;
				PUCHAR encrypted_block_buffer = (PUCHAR)ExAllocatePoolWithTag(PagedPool, SectorSize_wr, FILE_DISK_POOL_TAG);
				if (encrypted_block_buffer == NULL)
				{
					irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					irp->IoStatus.Information = 0;
					break;
				}

				const LONGLONG blocks_count_wr = io_stack->Parameters.Write.Length / SectorSize_wr;
				const LONGLONG buffer_length_wr = io_stack->Parameters.Write.Length;
				PUCHAR encrypted_data = (PUCHAR)ExAllocatePoolWithTag(PagedPool, buffer_length_wr, FILE_DISK_POOL_TAG);
				if (encrypted_data == NULL)
				{
					irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					irp->IoStatus.Information = 0;
					break;
				}

				PUCHAR iv_wr = (PUCHAR)ExAllocatePoolWithTag(PagedPool, sizeof(unsigned char) *  mdevice_extension->mstruct.MyKey->blockSize, FILE_DISK_POOL_TAG);
				if (iv_wr == NULL)
				{
					irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
					irp->IoStatus.Information = 0;
					break;
				}

				LONGLONG offsetInBlocks_wr = io_stack->Parameters.Write.ByteOffset.QuadPart / SectorSize_wr;
				LONGLONG memoryOffset_wr = 0;
				for (ULONG block_index = 0; block_index < blocks_count_wr; block_index++)
				{
					PrepareIv(key_wr, block_index + offsetInBlocks_wr, iv_wr);
					EncrypDecryptData(key_wr, system_buffer + memoryOffset_wr, SectorSize_wr, encrypted_block_buffer, SectorSize_wr, TRUE, iv_wr);
					RtlCopyMemory(encrypted_data + memoryOffset_wr, encrypted_block_buffer, SectorSize_wr);

					memoryOffset_wr += SectorSize_wr;
				}

				ZwWriteFile(
					mdevice_extension->mstruct.file_handle,
					NULL,
					NULL,
					NULL,
					&irp->IoStatus,
					encrypted_data,
					buffer_length_wr,
					&io_stack->Parameters.Write.ByteOffset,
					NULL
				);

				ExFreePool(encrypted_data);
				ExFreePool(encrypted_block_buffer);
				ExFreePool(iv_wr);
				break;

			case IRP_MJ_DEVICE_CONTROL:
				switch (io_stack->Parameters.DeviceIoControl.IoControlCode)
				{
				case IOCTL_FILE_DISK_OPEN_FILE:
					DbgPrint("Going into IRP_MJ_DEV_CONT : IOCTL_DISK_OPEN\n");
					//SeImpersonateClient(mdevice_extension->mstruct.security_client_context, NULL);

					irp->IoStatus.Status = FileDiskCreateDevice(device_object->DriverObject, n, FILE_DEVICE_DISK);
					if (!NT_SUCCESS(irp->IoStatus.Status))
					{
						PDEVICE_OBJECT dev_obbj;
						dev_obbj = device_object->DriverObject->DeviceObject;
						PsCreateSystemThread(
							&thread_handle,
							(ACCESS_MASK)0L,
							NULL,
							NULL,
							NULL,
							(PKSTART_ROUTINE)FileDiskDeleteDevice,
							(PVOID)dev_obbj);
						ZwClose(thread_handle);
						//if(!NT_SUCCESS(status))
						//PMDEVICE_EXTENSION pmm = (PMDEVICE_EXTENSION)(device_object->DeviceExtension);
						//pmm->number = INVALID_NUMBER_DEV;
						device_object = dev_obbj->NextDevice;
					}
					SeImpersonateClient(mdevice_extension->mstruct.security_client_context, NULL);
					if (NT_SUCCESS(irp->IoStatus.Status))
						irp->IoStatus.Status = FileDiskMakeFile(device_object->DriverObject->DeviceObject, irp);
					PsRevertToSelf();
					break;
				case IOCTL_FILE_DISK_CLOSE_FILE:
					if (mdevice_extension->number!=0)
					{
						irp->IoStatus.Status = FileDiskCloseFile(device_object, irp);
						//if (NT_SUCCESS(irp->IoStatus.Status))
						//{
						//	if ((device_object = FileDiskDeleteDevice(device_object))!= NULL)
						//		irp->IoStatus.Status = STATUS_SUCCESS;
						//}
						//ObDereferenceObject(mdevice_extension->mstruct.thread_pointer);
						if (NT_SUCCESS(irp->IoStatus.Status))
						{
							PsCreateSystemThread(
								&thread_handle,
								(ACCESS_MASK)0L,
								NULL,
								NULL,
								NULL,
								(PKSTART_ROUTINE)FileDiskDeleteDevice,
								(PVOID)device_object);
							ZwClose(thread_handle);
							//if(!NT_SUCCESS(status))
							//PMDEVICE_EXTENSION pmm = (PMDEVICE_EXTENSION)(device_object->DeviceExtension);
							//pmm->number = INVALID_NUMBER_DEV;
							device_object = device_object->NextDevice;
						}
					}
					break;
				//case IOCTL_FILE_DISK_UNMOUNT_ALL:
				//	if (mdevice_extension->number == 0)
				//	{
				//		PsCreateSystemThread(
				//			&thread_handle,
				//			(ACCESS_MASK)0L,
				//			NULL,
				//			NULL,
				//			NULL,
				//			FileDiskUnmountAll,
				//			(PVOID)device_object);
				//		ZwClose(thread_handle);
				//		//if(!NT_SUCCESS(status))
				//		//PMDEVICE_EXTENSION pmm = (PMDEVICE_EXTENSION)(device_object->DeviceExtension);
				//		//pmm->number = INVALID_NUMBER_DEV;
				//	}
				//	break;
				default:
					irp->IoStatus.Status = STATUS_DRIVER_INTERNAL_ERROR;
				}
				break;

			default:
				irp->IoStatus.Status = STATUS_DRIVER_INTERNAL_ERROR;
			}

			IoCompleteRequest(irp, (CCHAR)(NT_SUCCESS(irp->IoStatus.Status) ? IO_DISK_INCREMENT : IO_NO_INCREMENT));
		}
	}
}

#pragma code_seg() 