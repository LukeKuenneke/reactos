/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS system libraries
 * FILE:            lib/kernel32/misc/console.c
 * PURPOSE:         Win32 server console functions
 * PROGRAMMER:      ???
 * UPDATE HISTORY:
 *	199901?? ??	Created
 *	19990204 EA	SetConsoleTitleA
 *      19990306 EA	Stubs
 */

/* INCLUDES ******************************************************************/

#include <ddk/ntddk.h>
#include <ddk/ntddblue.h>

#include <ntos/keyboard.h>

#include "usetup.h"
//#include <windows.h>
//#include <assert.h>
//#include <wchar.h>


/* GLOBALS ******************************************************************/

static HANDLE StdInput  = INVALID_HANDLE_VALUE;
static HANDLE StdOutput = INVALID_HANDLE_VALUE;

static SHORT xScreen = 0;
static SHORT yScreen = 0;


NTSTATUS
GetConsoleScreenBufferInfo(PCONSOLE_SCREEN_BUFFER_INFO ConsoleScreenBufferInfo);


/* FUNCTIONS *****************************************************************/



NTSTATUS
AllocConsole(VOID)
{
  OBJECT_ATTRIBUTES ObjectAttributes;
  IO_STATUS_BLOCK IoStatusBlock;
  UNICODE_STRING Name;
  NTSTATUS Status;
  CONSOLE_SCREEN_BUFFER_INFO csbi;

  /* Open the screen */
  RtlInitUnicodeString(&Name,
		       L"\\??\\BlueScreen");
  InitializeObjectAttributes(&ObjectAttributes,
			     &Name,
			     0,
			     NULL,
			     NULL);
  Status = NtCreateFile(&StdOutput,
			FILE_ALL_ACCESS,
			&ObjectAttributes,
			&IoStatusBlock,
			NULL,
			0,
			0,
			FILE_OPEN,
			0,
			NULL,
			0);
  if (!NT_SUCCESS(Status))
    return(Status);

  /* Open the keyboard */
  RtlInitUnicodeString(&Name,
		       L"\\??\\Keyboard");
  InitializeObjectAttributes(&ObjectAttributes,
			     &Name,
			     0,
			     NULL,
			     NULL);
  Status = NtCreateFile(&StdInput,
			FILE_ALL_ACCESS,
			&ObjectAttributes,
			&IoStatusBlock,
			NULL,
			0,
			0,
			FILE_OPEN,
			0,
			NULL,
			0);

  GetConsoleScreenBufferInfo(&csbi);

  xScreen = csbi.dwSize.X;
  yScreen = csbi.dwSize.Y;

  return(Status);
}


VOID
FreeConsole(VOID)
{
  if (StdInput != INVALID_HANDLE_VALUE)
    NtClose(StdInput);

  if (StdOutput != INVALID_HANDLE_VALUE)
    NtClose(StdOutput);
}




NTSTATUS
WriteConsole(PCHAR Buffer,
	     ULONG NumberOfCharsToWrite,
	     PULONG NumberOfCharsWritten)
{
  IO_STATUS_BLOCK IoStatusBlock;
  NTSTATUS Status = STATUS_SUCCESS;
  ULONG i;

  Status = NtWriteFile(StdOutput,
		       NULL,
		       NULL,
		       NULL,
		       &IoStatusBlock,
		       Buffer,
		       NumberOfCharsToWrite,
		       NULL,
		       NULL);
  if (Status == STATUS_PENDING)
    {
      NtWaitForSingleObject(StdOutput,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }

  if (NumberOfCharsWritten != NULL)
    {
      *NumberOfCharsWritten = IoStatusBlock.Information;
    }

  return(Status);
}


#if 0
/*--------------------------------------------------------------
 *	ReadConsoleA
 */
WINBOOL
STDCALL
ReadConsoleA(HANDLE hConsoleInput,
			     LPVOID lpBuffer,
			     DWORD nNumberOfCharsToRead,
			     LPDWORD lpNumberOfCharsRead,
			     LPVOID lpReserved)
{
  KEY_EVENT_RECORD KeyEventRecord;
  BOOL  stat = TRUE;
  PCHAR Buffer = (PCHAR)lpBuffer;
  DWORD Result;
  int   i;

  for (i=0; (stat && i<nNumberOfCharsToRead);)
    {
      stat = ReadFile(hConsoleInput,
		      &KeyEventRecord,
		      sizeof(KEY_EVENT_RECORD),
		      &Result,
		      NULL);
      if (stat && KeyEventRecord.bKeyDown && KeyEventRecord.uChar.AsciiChar != 0)
	{
	  Buffer[i] = KeyEventRecord.uChar.AsciiChar;
	  i++;
	}
    }
  if (lpNumberOfCharsRead != NULL)
    {
      *lpNumberOfCharsRead = i;
    }
  return(stat);
}
#endif



NTSTATUS
ReadConsoleInput(PINPUT_RECORD Buffer,
		 ULONG Length,
		 PULONG NumberOfEventsRead)
{
  IO_STATUS_BLOCK IoStatusBlock;
  NTSTATUS Status = STATUS_SUCCESS;
  ULONG i;

  for (i=0; (NT_SUCCESS(Status) && i < Length);)
    {
      Status = NtReadFile(StdInput,
			  NULL,
			  NULL,
			  NULL,
			  &IoStatusBlock,
			  &Buffer[i].Event.KeyEvent,
			  sizeof(KEY_EVENT_RECORD),
			  NULL,
			  NULL);
      if (Status == STATUS_PENDING)
	{
	  NtWaitForSingleObject(StdInput,
				FALSE,
				NULL);
	  Status = IoStatusBlock.Status;
	}
      if (NT_SUCCESS(Status))
	{
	  Buffer[i].EventType = KEY_EVENT;
	  i++;
	}
    }

  if (NumberOfEventsRead != NULL)
    {
      *NumberOfEventsRead = i;
    }

   return(Status);
}


NTSTATUS
ReadConsoleOutputCharacters(LPSTR lpCharacter,
			    ULONG nLength,
			    COORD dwReadCoord,
			    PULONG lpNumberOfCharsRead)
{
  IO_STATUS_BLOCK IoStatusBlock;
  ULONG dwBytesReturned;
  OUTPUT_CHARACTER Buffer;
  NTSTATUS Status;

  Buffer.dwCoord    = dwReadCoord;

  Status = NtDeviceIoControlFile(StdOutput,
				 NULL,
				 NULL,
				 NULL,
				 &IoStatusBlock,
				 IOCTL_CONSOLE_READ_OUTPUT_CHARACTER,
				 &Buffer,
				 sizeof(OUTPUT_CHARACTER),
				 (PVOID)lpCharacter,
				 nLength);
  if (Status == STATUS_PENDING)
    {
      NtWaitForSingleObject(StdOutput,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }

  if (NT_SUCCESS(Status) && lpNumberOfCharsRead != NULL)
    {
      *lpNumberOfCharsRead = Buffer.dwTransfered;
    }

  return(Status);
}


NTSTATUS
ReadConsoleOutputAttributes(PUSHORT lpAttribute,
			    ULONG nLength,
			    COORD dwReadCoord,
			    PULONG lpNumberOfAttrsRead)
{
  IO_STATUS_BLOCK IoStatusBlock;
  ULONG dwBytesReturned;
  OUTPUT_ATTRIBUTE Buffer;
  NTSTATUS Status;

  Buffer.dwCoord = dwReadCoord;

  Status = NtDeviceIoControlFile(StdOutput,
				 NULL,
				 NULL,
				 NULL,
				 &IoStatusBlock,
				 IOCTL_CONSOLE_READ_OUTPUT_ATTRIBUTE,
				 &Buffer,
				 sizeof(OUTPUT_ATTRIBUTE),
				 (PVOID)lpAttribute,
				 nLength);
  if (Status == STATUS_PENDING)
    {
      NtWaitForSingleObject(StdOutput,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }

  if (NT_SUCCESS(Status) && lpNumberOfAttrsRead != NULL)
    {
      *lpNumberOfAttrsRead = Buffer.dwTransfered;
    }

  return(Status);
}


NTSTATUS
WriteConsoleOutputCharacters(LPCSTR lpCharacter,
			     ULONG nLength,
			     COORD dwWriteCoord)
{
  IO_STATUS_BLOCK IoStatusBlock;
  PCHAR Buffer;
  COORD *pCoord;
  PCHAR pText;
  NTSTATUS Status;

  Buffer = RtlAllocateHeap(ProcessHeap,
			   0,
			   nLength + sizeof(COORD));
  pCoord = (COORD *)Buffer;
  pText = (PCHAR)(pCoord + 1);

  *pCoord = dwWriteCoord;
  memcpy(pText, lpCharacter, nLength);

  Status = NtDeviceIoControlFile(StdOutput,
				 NULL,
				 NULL,
				 NULL,
				 &IoStatusBlock,
				 IOCTL_CONSOLE_WRITE_OUTPUT_CHARACTER,
				 NULL,
				 0,
				 Buffer,
				 nLength + sizeof(COORD));
  if (Status == STATUS_PENDING)
    {
      NtWaitForSingleObject(StdOutput,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }

  RtlFreeHeap(ProcessHeap,
	      0,
	      Buffer);

  return(Status);
}


NTSTATUS
WriteConsoleOutputAttributes(CONST USHORT *lpAttribute,
			     ULONG nLength,
			     COORD dwWriteCoord,
			     PULONG lpNumberOfAttrsWritten)
{
  IO_STATUS_BLOCK IoStatusBlock;
  PUSHORT Buffer;
  COORD *pCoord;
  PUSHORT pAttrib;
  NTSTATUS Status;

  Buffer = RtlAllocateHeap(ProcessHeap,
			   0,
			   nLength * sizeof(USHORT) + sizeof(COORD));
  pCoord = (COORD *)Buffer;
  pAttrib = (PUSHORT)(pCoord + 1);

  *pCoord = dwWriteCoord;
  memcpy(pAttrib, lpAttribute, nLength * sizeof(USHORT));

  Status = NtDeviceIoControlFile(StdOutput,
				 NULL,
				 NULL,
				 NULL,
				 &IoStatusBlock,
				 IOCTL_CONSOLE_WRITE_OUTPUT_ATTRIBUTE,
				 NULL,
				 0,
				 Buffer,
				 nLength * sizeof(USHORT) + sizeof(COORD));
  if (Status == STATUS_PENDING)
    {
      NtWaitForSingleObject(StdOutput,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }

  RtlFreeHeap(ProcessHeap,
	      0,
	      Buffer);

  return(Status);
}


NTSTATUS
FillConsoleOutputAttribute(USHORT wAttribute,
			   ULONG nLength,
			   COORD dwWriteCoord,
			   PULONG lpNumberOfAttrsWritten)
{
  IO_STATUS_BLOCK IoStatusBlock;
  OUTPUT_ATTRIBUTE Buffer;
  ULONG dwBytesReturned;
  NTSTATUS Status;

  Buffer.wAttribute = wAttribute;
  Buffer.nLength    = nLength;
  Buffer.dwCoord    = dwWriteCoord;

  Status = NtDeviceIoControlFile(StdOutput,
				 NULL,
				 NULL,
				 NULL,
				 &IoStatusBlock,
				 IOCTL_CONSOLE_FILL_OUTPUT_ATTRIBUTE,
				 &Buffer,
				 sizeof(OUTPUT_ATTRIBUTE),
				 &Buffer,
				 sizeof(OUTPUT_ATTRIBUTE));
  if (Status == STATUS_PENDING)
    {
      NtWaitForSingleObject(StdOutput,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }
  if (NT_SUCCESS(Status))
    {
      *lpNumberOfAttrsWritten = Buffer.dwTransfered;
    }

  return(Status);
}


NTSTATUS
FillConsoleOutputCharacter(CHAR Character,
			   ULONG Length,
			   COORD WriteCoord,
			   PULONG NumberOfCharsWritten)
{
  IO_STATUS_BLOCK IoStatusBlock;
  OUTPUT_CHARACTER Buffer;
  ULONG dwBytesReturned;
  NTSTATUS Status;

  Buffer.cCharacter = Character;
  Buffer.nLength = Length;
  Buffer.dwCoord = WriteCoord;

  Status = NtDeviceIoControlFile(StdOutput,
				 NULL,
				 NULL,
				 NULL,
				 &IoStatusBlock,
				 IOCTL_CONSOLE_FILL_OUTPUT_CHARACTER,
				 &Buffer,
				 sizeof(OUTPUT_CHARACTER),
				 &Buffer,
				 sizeof(OUTPUT_CHARACTER));
  if (Status == STATUS_PENDING)
    {
      NtWaitForSingleObject(StdOutput,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }
  if (NT_SUCCESS(Status))
    {
      *NumberOfCharsWritten = Buffer.dwTransfered;
    }

  return(Status);
}


#if 0
/*--------------------------------------------------------------
 * 	GetConsoleMode
 */
WINBASEAPI
BOOL
WINAPI
GetConsoleMode(
	HANDLE		hConsoleHandle,
	LPDWORD		lpMode
	)
{
    CONSOLE_MODE Buffer;
    DWORD   dwBytesReturned;
	
    if (DeviceIoControl (hConsoleHandle,
                         IOCTL_CONSOLE_GET_MODE,
                         NULL,
                         0,
                         &Buffer,
                         sizeof(CONSOLE_MODE),
                         &dwBytesReturned,
                         NULL))
    {
        *lpMode = Buffer.dwMode;
        SetLastError (ERROR_SUCCESS);
        return TRUE;
    }

    SetLastError(0); /* FIXME: What error code? */
    return FALSE;
}


/*--------------------------------------------------------------
 *	GetConsoleCursorInfo
 */
WINBASEAPI
BOOL
WINAPI
GetConsoleCursorInfo(
	HANDLE			hConsoleOutput,
	PCONSOLE_CURSOR_INFO	lpConsoleCursorInfo
	)
{
    DWORD   dwBytesReturned;
	
    if (DeviceIoControl (hConsoleOutput,
                         IOCTL_CONSOLE_GET_CURSOR_INFO,
                         NULL,
                         0,
                         lpConsoleCursorInfo,
                         sizeof(CONSOLE_CURSOR_INFO),
                         &dwBytesReturned,
                         NULL))
        return TRUE;

    return FALSE;
}



/*--------------------------------------------------------------
 * 	SetConsoleMode
 */
WINBASEAPI
BOOL
WINAPI
SetConsoleMode(
	HANDLE		hConsoleHandle,
	DWORD		dwMode
	)
{
  CONSOLE_MODE Buffer;
  DWORD dwBytesReturned;

  Buffer.dwMode = dwMode;

  if (DeviceIoControl(hConsoleHandle,
                      IOCTL_CONSOLE_SET_MODE,
                         &Buffer,
                         sizeof(CONSOLE_MODE),
                         NULL,
                         0,
                         &dwBytesReturned,
                         NULL))
    {
        SetLastError (ERROR_SUCCESS);
        return TRUE;
    }

    SetLastError(0); /* FIXME: What error code? */
    return FALSE;
}
#endif


NTSTATUS
GetConsoleScreenBufferInfo(PCONSOLE_SCREEN_BUFFER_INFO ConsoleScreenBufferInfo)
{
  IO_STATUS_BLOCK IoStatusBlock;
  NTSTATUS Status;

  Status = NtDeviceIoControlFile(StdOutput,
				 NULL,
				 NULL,
				 NULL,
				 &IoStatusBlock,
				 IOCTL_CONSOLE_GET_SCREEN_BUFFER_INFO,
				 NULL,
				 0,
				 ConsoleScreenBufferInfo,
				 sizeof(CONSOLE_SCREEN_BUFFER_INFO));
  if (Status == STATUS_PENDING)
    {
      NtWaitForSingleObject(StdOutput,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }

  return(Status);
}


NTSTATUS
SetConsoleCursorInfo(PCONSOLE_CURSOR_INFO lpConsoleCursorInfo)
{
  IO_STATUS_BLOCK IoStatusBlock;
  NTSTATUS Status;

  Status = NtDeviceIoControlFile(StdOutput,
				 NULL,
				 NULL,
				 NULL,
				 &IoStatusBlock,
				 IOCTL_CONSOLE_SET_CURSOR_INFO,
				 (PCONSOLE_CURSOR_INFO)lpConsoleCursorInfo,
				 sizeof(CONSOLE_CURSOR_INFO),
				 NULL,
				 0);
  if (Status == STATUS_PENDING)
    {
      NtWaitForSingleObject(StdOutput,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }
  return(Status);
}


NTSTATUS
SetConsoleCursorPosition(COORD dwCursorPosition)
{
  CONSOLE_SCREEN_BUFFER_INFO ConsoleScreenBufferInfo;
  IO_STATUS_BLOCK IoStatusBlock;
  NTSTATUS Status;

  Status = GetConsoleScreenBufferInfo(&ConsoleScreenBufferInfo);
  if (!NT_SUCCESS(Status))
    return(Status);

  ConsoleScreenBufferInfo.dwCursorPosition.X = dwCursorPosition.X;
  ConsoleScreenBufferInfo.dwCursorPosition.Y = dwCursorPosition.Y;

  Status = NtDeviceIoControlFile(StdOutput,
				 NULL,
				 NULL,
				 NULL,
				 &IoStatusBlock,
				 IOCTL_CONSOLE_SET_SCREEN_BUFFER_INFO,
				 &ConsoleScreenBufferInfo,
				 sizeof(CONSOLE_SCREEN_BUFFER_INFO),
				 NULL,
				 0);
  if (Status == STATUS_PENDING)
    {
      NtWaitForSingleObject(StdOutput,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }
  return(Status);
}


NTSTATUS
SetConsoleTextAttribute(WORD wAttributes)
{
  IO_STATUS_BLOCK IoStatusBlock;
  NTSTATUS Status;

  Status = NtDeviceIoControlFile(StdOutput,
				 NULL,
				 NULL,
				 NULL,
				 &IoStatusBlock,
				 IOCTL_CONSOLE_SET_TEXT_ATTRIBUTE,
				 &wAttributes,
				 sizeof(WORD),
				 NULL,
				 0);
  if (Status == STATUS_PENDING)
    {
      NtWaitForSingleObject(StdOutput,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }
  return(Status);
}




VOID
ConInKey(PINPUT_RECORD Buffer)
{
  ULONG KeysRead;

  while (TRUE)
    {
      ReadConsoleInput(Buffer, 1, &KeysRead);
      if ((Buffer->EventType == KEY_EVENT) &&
	  (Buffer->Event.KeyEvent.bKeyDown == TRUE))
	  break;
    }
}


VOID
ConOutChar(CHAR c)
{
  ULONG Written;

  WriteConsole(&c,
	       1,
	       &Written);
}


VOID
ConOutPuts(LPSTR szText)
{
  ULONG Written;

  WriteConsole(szText,
	       strlen(szText),
	       &Written);
  WriteConsole("\n",
	       1,
	       &Written);
}


VOID
ConOutPrintf(LPSTR szFormat, ...)
{
  CHAR szOut[256];
  DWORD dwWritten;
  va_list arg_ptr;

  va_start(arg_ptr, szFormat);
  vsprintf(szOut, szFormat, arg_ptr);
  va_end(arg_ptr);

  WriteConsole(szOut,
	       strlen(szOut),
	       &dwWritten);
}





SHORT
GetCursorX(VOID)
{
  CONSOLE_SCREEN_BUFFER_INFO csbi;

  GetConsoleScreenBufferInfo(&csbi);

  return(csbi.dwCursorPosition.X);
}


SHORT
GetCursorY(VOID)
{
  CONSOLE_SCREEN_BUFFER_INFO csbi;

  GetConsoleScreenBufferInfo(&csbi);

  return(csbi.dwCursorPosition.Y);
}


VOID
GetScreenSize(SHORT *maxx,
	      SHORT *maxy)
{
  CONSOLE_SCREEN_BUFFER_INFO csbi;

  GetConsoleScreenBufferInfo(&csbi);

  if (maxx)
    *maxx = csbi.dwSize.X;

  if (maxy)
    *maxy = csbi.dwSize.Y;
}


VOID
SetCursorType(BOOL bInsert,
	      BOOL bVisible)
{
  CONSOLE_CURSOR_INFO cci;

  cci.dwSize = bInsert ? 10 : 99;
  cci.bVisible = bVisible;

  SetConsoleCursorInfo(&cci);
}


VOID
SetCursorXY(SHORT x,
	    SHORT y)
{
  COORD coPos;

  coPos.X = x;
  coPos.Y = y;
  SetConsoleCursorPosition(coPos);
}


VOID
ClearScreen(VOID)
{
  COORD coPos;
  ULONG Written;

  coPos.X = 0;
  coPos.Y = 0;

  FillConsoleOutputCharacter(' ',
			     xScreen * yScreen,
			     coPos,
			     &Written);
}


VOID
SetStatusText(PCHAR Text)
{
  COORD coPos;
  ULONG Written;

  coPos.X = 0;
  coPos.Y = yScreen - 1;

  FillConsoleOutputAttribute(0x70,
			     xScreen,
			     coPos,
			     &Written);

  FillConsoleOutputCharacter(' ',
			     xScreen,
			     coPos,
			     &Written);

  WriteConsoleOutputCharacters(Text,
			       strlen(Text),
			       coPos);
}


VOID
SetTextXY(SHORT x, SHORT y, PCHAR Text)
{
  COORD coPos;

  coPos.X = x;
  coPos.Y = y;

  WriteConsoleOutputCharacters(Text,
			       strlen(Text),
			       coPos);
}

/* EOF */
