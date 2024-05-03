#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Protocol/Udp4.h>
#include <Protocol/ServiceBinding.h>

BOOLEAN TransmitCompleteFlag = FALSE;