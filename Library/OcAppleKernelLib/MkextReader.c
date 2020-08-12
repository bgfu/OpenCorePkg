/** @file
  Mkext support.

  Copyright (c) 2020, Goldfish64. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/OcAppleKernelLib.h>
#include <Library/OcCompressionLib.h>
#include <Library/OcFileLib.h>
#include <Library/OcGuardLib.h>
#include <Library/OcStringLib.h>

//
// Pick a reasonable maximum to fit.
//
#define MKEXT_HEADER_SIZE (EFI_PAGE_SIZE * 2)

EFI_STATUS
ReadAppleMkext (
  IN     EFI_FILE_PROTOCOL  *File,
     OUT UINT8              **Mkext,
     OUT UINT32             *MkextSize,
     OUT UINT32             *AllocatedSize,
  IN     UINT32             ReservedSize
  )
{
  EFI_STATUS        Status;
  UINT32            Offset;
  UINT8             *TmpMkext;
  UINT32            TmpMkextSize;
  UINT32            TmpMkextFileSize;

  TmpMkextSize = MKEXT_HEADER_SIZE;

  Status = GetFileSize (File, &TmpMkextFileSize);
  if (TmpMkextSize >= TmpMkextFileSize) {
    return EFI_INVALID_PARAMETER;
  }
  TmpMkext  = AllocatePool (TmpMkextSize);

  if (TmpMkext == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetFileData (File, 0, TmpMkextSize, TmpMkext);
  if (EFI_ERROR (Status)) {
    FreePool (TmpMkext);
    return Status;
  }
  MachoGetFatArchitectureOffset (TmpMkext, TmpMkextSize, TmpMkextFileSize, MachCpuTypeI386, &Offset, &TmpMkextSize);
  FreePool (TmpMkext);

  TmpMkext = AllocatePool (TmpMkextSize);
  if (TmpMkext == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  GetFileData (File, Offset, TmpMkextSize, TmpMkext);

  *AllocatedSize = 0;
  Status = MkextDecompress (TmpMkext, TmpMkextSize, 5, NULL, 0, AllocatedSize);
  if (EFI_ERROR (Status)) {
    FreePool (TmpMkext);
    return Status;
  }
  DEBUG ((DEBUG_INFO, "Mkext will be 0x%X\n", *AllocatedSize));

  if (OcOverflowAddU32 (*AllocatedSize, ReservedSize, AllocatedSize)) {
    FreePool (TmpMkext);
    return EFI_INVALID_PARAMETER;
  }

  *Mkext = AllocatePool (*AllocatedSize);
  if (*Mkext == NULL) {
    FreePool (TmpMkext);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = MkextDecompress (TmpMkext, TmpMkextSize, 5, *Mkext, *AllocatedSize, MkextSize);
  DEBUG ((DEBUG_INFO, "Mkextdecom %r 0x%X 0x%X\n", Status, *AllocatedSize, *MkextSize));
  FreePool (TmpMkext);

  if (EFI_ERROR (Status)) {
    FreePool (*Mkext);
  }

  return Status;
}