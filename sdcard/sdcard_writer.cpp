
#include <Windows.h>
#include <WinIoCtl.h>
#include <WinBase.h>
#include <tchar.h>
#include <stdio.h>
#include "sdcard_writer\sdcard_writer_defines.h"

//get the physical geometry of the target drive
BOOL GetDriveGeometry(LPWSTR wsdPath, DISK_GEOMETRY *pdg)
{
	HANDLE hDevice			= INVALID_HANDLE_VALUE;
	BOOL bResult			= FALSE;
	DWORD dError			= 0;
	DWORD nBytesReturned    = 0;

	//open the sd drive
	hDevice = CreateFile(wsdPath,
						 GENERIC_READ,
						 FILE_SHARE_READ,
						 NULL,
						 OPEN_EXISTING,
						 0,
						 NULL);

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		dError = GetLastError();
	}
	else
	{
		bResult = DeviceIoControl(hDevice,
								  IOCTL_DISK_GET_DRIVE_GEOMETRY,
								  NULL,
								  0,
								  (LPVOID)pdg,
								  (DWORD)sizeof(*pdg),
								  (LPDWORD)&nBytesReturned,
								  (LPOVERLAPPED)NULL
								  );

		if (0 == nBytesReturned)
		{
			bResult = 0;
		}

		CloseHandle(hDevice);
	}

	return bResult;
}

//get the partition layout of the target drive
BOOL GetPartitionLayout(LPWSTR wsdPath, 
						DRIVE_LAYOUT_INFORMATION *pdli)
{
	HANDLE hDevice				= INVALID_HANDLE_VALUE;
	BOOL bResult				= FALSE;
	PBYTE pByOutBuffer			= NULL;
	DWORD dError				= 0;
	DWORD dwNumBytesReturned	= 0;
	const DWORD BUFFER_LENGTH	= 1024;

	//open the sd drive
	hDevice = CreateFile(wsdPath,
						 GENERIC_READ,
						 FILE_SHARE_READ,
						 NULL,
						 OPEN_EXISTING,
						 0,
						 NULL);
	if (INVALID_HANDLE_VALUE == hDevice)
	{
		goto error;
	}
	else
	{
		pByOutBuffer = (PBYTE)calloc(BUFFER_LENGTH,sizeof(BYTE));
		if (NULL == pByOutBuffer)
		{
			goto error;
		}
		else
		{
			bResult = DeviceIoControl(hDevice,
									  IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
									  NULL,
									  0,
									  (LPVOID)pByOutBuffer,
									  BUFFER_LENGTH,
									  (LPDWORD)&dwNumBytesReturned,
									  (LPOVERLAPPED)NULL
									 );

			if (1 == bResult)
			{
				if (0 != memcpy_s(pdli,sizeof(DRIVE_LAYOUT_INFORMATION),
								  pByOutBuffer,sizeof(DRIVE_LAYOUT_INFORMATION)))
				{
					bResult = 0;
				}
			}
			free(pByOutBuffer);
			pByOutBuffer = NULL;
		}
		CloseHandle(hDevice);
	}
		
error:
	dError = GetLastError();

	return bResult;
}

//delete the sector 0 of the input drive
BOOL DeletePartitionLayout(LPWSTR wsdPath)
{
	HANDLE	hDevice				= INVALID_HANDLE_VALUE;
	BOOL	bResult				= TRUE;
	DWORD	numBytesReturned	= 0;

	//open the sd drive
	hDevice = CreateFile(wsdPath,
						 GENERIC_READ|GENERIC_WRITE,
						 FILE_SHARE_DELETE,
						 NULL,
						 OPEN_EXISTING,
						 0,
						 NULL);
	if (INVALID_HANDLE_VALUE == hDevice)
	{
		bResult = FALSE;
		goto error;
	}
	else
	{
		//delete the drive layout
		bResult = DeviceIoControl(hDevice,
								  IOCTL_DISK_DELETE_DRIVE_LAYOUT,
								  NULL,
								  0,
								  NULL,
								  0,
								  (LPDWORD) &numBytesReturned,
								  (LPOVERLAPPED) NULL
								  );

		CloseHandle(hDevice);
		hDevice = NULL;
	}

error:
	if (FALSE == bResult)
	{
		DWORD err = GetLastError();
	}

	return bResult;
}

//format and create a partition layout of the input drive
BOOL FormatInputDrive(LPWSTR wsdPath)
{
	HANDLE	hDevice				= INVALID_HANDLE_VALUE;
	BOOL	bResult				= TRUE;

	//open the sd drive
	hDevice = CreateFile(wsdPath,
						 GENERIC_READ|GENERIC_WRITE,
						 FILE_SHARE_READ|FILE_SHARE_WRITE,
						 NULL,
						 OPEN_EXISTING,
						 0,
						 NULL);
	if (INVALID_HANDLE_VALUE == hDevice)
	{
		bResult = FALSE;
		goto error;
	}
	else
	{
		//create a disk
		CREATE_DISK		dsk;
		DWORD			numBytesReturned	= 0;

		dsk.PartitionStyle	=	PARTITION_STYLE_MBR;
		dsk.Mbr.Signature	=	4;

		bResult = DeviceIoControl(hDevice,
							      IOCTL_DISK_CREATE_DISK,
								  (LPVOID)&dsk,
								  (DWORD)sizeof(dsk),
								  NULL,
								  0,
								  (LPDWORD)&numBytesReturned,
								  NULL);
		CloseHandle(hDevice);
	}

error:
	if (FALSE == bResult)
	{
		DWORD	dwError = GetLastError();
	}

	return bResult;
}

BOOL WriteText2BinSector(LPWSTR wSdPath,
						 LPWSTR	wInTextFileName)
{
	HANDLE	hFile					=	NULL;
	HANDLE	hDevice					=	NULL;
	BOOL	bResult					=	TRUE;
	BYTE	buffer[LENGTH_SECTOR]	=	{0};
	DWORD	nSectors				=	0;
	DWORD	numBytesRead			=	0;
	DWORD	numBytesWritten			=	0;

	//open the input text file for reading
	hFile = CreateFile(wInTextFileName,
					   GENERIC_READ,
					   FILE_SHARE_READ,
					   NULL,
					   OPEN_EXISTING,
					   FILE_ATTRIBUTE_NORMAL,
					   NULL
					   );
	if (INVALID_HANDLE_VALUE == hFile)
	{
		bResult = FALSE;
		goto error;
	}

	//open the sd drive for writing
	hDevice = CreateFile(wSdPath,
						 GENERIC_WRITE,
						 FILE_SHARE_WRITE,
						 NULL,
						 OPEN_EXISTING,
						 FILE_ATTRIBUTE_NORMAL,
						 NULL
						 );
	if (INVALID_HANDLE_VALUE == hDevice)
	{
		bResult = FALSE;
		goto error;
	}

	//count the number of non-empty sectors in the input text file
	{
		DWORD	dwFileSizeHigh = 0;
		nSectors = (DWORD)(GetFileSize(hFile,&dwFileSizeHigh)/(float)LENGTH_SECTOR+0.5f);
		if (0 < dwFileSizeHigh)
		{
			_tprintf(_T("Input text file size is more than 2GB.\n"));
			bResult = FALSE;
			goto error;
		}
	}

	//read & write in a sector-wise loop
	for (DWORD sector = 0; sector < nSectors; sector++)
	{
		//read a sector of data from the input text file
		bResult = ReadFile(hFile,
						   (LPVOID)buffer,
						   (DWORD)LENGTH_SECTOR,
						   (LPDWORD)&numBytesRead,
						   NULL
						  );
		if (FALSE == bResult)
		{
			goto error;
		}
			
		//write numBytesRead bytes into the sd drive
		bResult = WriteFile(hDevice,
							(LPVOID)buffer,
							numBytesRead,
							(LPDWORD)&numBytesWritten,
							NULL
						   );
		if ( (FALSE == bResult) || (numBytesRead != numBytesWritten) )
		{
			bResult = FALSE;
			goto error;
		}
	}

error:
	if (NULL != hFile)
	{
		CloseHandle(hFile);
	}
	if (NULL != hDevice)
	{
		CloseHandle(hDevice);
	}
	if (FALSE == bResult)
	{
		DWORD	dwError = GetLastError();
	}

	return bResult;
}

int main(int argc, TCHAR *argv[])
{
	BOOL bResult					= FALSE;
	DISK_GEOMETRY pdg				= {0};
	ULONGLONG llDiskSize			= 0;
	DRIVE_LAYOUT_INFORMATION dli	= {0};

	bResult = GetDriveGeometry(tsdDrive,&pdg);
	if (bResult)
	{
		_tprintf(_T("Drive path		= %ws\n"),	tsdDrive);
		_tprintf(_T("Cylinders		= %I64d\n"),pdg.Cylinders.QuadPart);
		_tprintf(_T("Tracks/cylinder	= %ld\n"),	(ULONG)pdg.TracksPerCylinder);
		_tprintf(_T("Sectors/track   = %ld\n"),	(ULONG)pdg.SectorsPerTrack);
		_tprintf(_T("Bytes/sector	= %ld\n"),	(ULONG)pdg.BytesPerSector);

		llDiskSize = pdg.Cylinders.QuadPart *
					 (ULONG)pdg.TracksPerCylinder *
					 (ULONG)pdg.SectorsPerTrack *
					 (ULONG)pdg.BytesPerSector;
		_tprintf(_T("Disk size (Bytes)	= %I64d\n"), llDiskSize);
		_tprintf(_T("          (Gb)		= %.2f\n"),	(double)llDiskSize/(1024*1024*1024));
	}
	else
	{
		_tprintf(_T("Error: either no disk found or GetDriveGeometry failed\n"));
		_tprintf(_T("Error Code:%ld\n"), GetLastError());
	}

	GetPartitionLayout(tsdDrive, &dli);
	if (bResult)
	{
		_tprintf(_T("Partition count		= %ld\n"),	dli.PartitionCount);

		if (0 == dli.PartitionCount)
		{
			//delete the parition layout
			bResult = DeletePartitionLayout(tsdDrive);
		}
		else
		{
			for (DWORD i = 0; i <= dli.PartitionCount; i++)
			{
				PARTITION_INFORMATION *temp = &dli.PartitionEntry[1];

				_tprintf(_T("PartitionNumber	= %ld\n"),		(ULONG)temp->PartitionNumber);
				_tprintf(_T("PartitionLength	= %I64dd\n"),	temp->PartitionLength.QuadPart);
				_tprintf(_T("PartitionType		= %c\n"),		temp->PartitionType);
				_tprintf(_T("StartingOffset		= %I64d\n"),	temp->StartingOffset.QuadPart);
				_tprintf(_T("HiddenSectors		= %ld\n"),		temp->HiddenSectors);
			}
		}
	}

	WriteText2BinSector(tsdDrive,tinTextFile);

	return 0;
}