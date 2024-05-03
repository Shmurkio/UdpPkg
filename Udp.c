#include "Udp.h"

VOID EFIAPI TransmitEventCallback(IN EFI_EVENT Event, IN VOID *UserData) {
    TransmitCompleteFlag = TRUE;

    return;
}

EFI_STATUS EFIAPI WaitForFlag(IN BOOLEAN *Flag, IN EFI_UDP4_PROTOCOL *Udp4Protocol OPTIONAL, IN UINTN Timeout) {
    EFI_STATUS Status = EFI_SUCCESS;
    UINT8 LastSecond = MAX_UINT8;
    UINT8 Timer = 0;
    EFI_TIME CurrentTime;

    while (!*Flag && (Timeout == 0 || Timer < Timeout)) {
        if (Udp4Protocol) {
            Udp4Protocol->Poll(
                Udp4Protocol);
        }

        Status = gRT->GetTime(&CurrentTime, NULL);

        if (EFI_ERROR(Status)) {
            return Status;
        }

        if (LastSecond != CurrentTime.Second) {
            LastSecond = CurrentTime.Second;
            Timer++;
        }
    }

    return *Flag ? EFI_SUCCESS : EFI_TIMEOUT;
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status = EFI_SUCCESS;

    /* (1) Search for ServiceBinding protocol & create child instance */

    EFI_SERVICE_BINDING_PROTOCOL *Udp4ServiceBindingProtocol;

    Status = gBS->LocateProtocol(&gEfiUdp4ServiceBindingProtocolGuid, NULL, (VOID**)&Udp4ServiceBindingProtocol);

    if (EFI_ERROR(Status)) {
        Print(L"Failed to search for ServiceBinding protocol. Status: %r\n", Status);

        return Status;
    }

    EFI_HANDLE Udp4ChildHandle;

    Status = Udp4ServiceBindingProtocol->CreateChild(Udp4ServiceBindingProtocol, &Udp4ChildHandle);

    if (EFI_ERROR(Status)) {
        Print(L"Failed to create child instance. Status: %r\n", Status);

        return Status;
    }

    /* (2) Get protocol from child instance handle */

    EFI_UDP4_PROTOCOL *Udp4Protocol;

    Status = gBS->HandleProtocol(Udp4ChildHandle, &gEfiUdp4ProtocolGuid, (VOID**)&Udp4Protocol);

    if (EFI_ERROR(Status)) {
        Print(L"Failed to get protocol from child instance handle. Status: %r\n", Status);

        return Status;
    }

    /* (3) Configure child instance */

    EFI_UDP4_CONFIG_DATA Udp4ConfigData;

    EFI_IPv4_ADDRESS LocalAddress = { 127, 0, 0, 1 }; // Your local IP address
    EFI_IPv4_ADDRESS SubnetMask = { 255, 255, 255, 0 };
    UINT16 LocalPort = 0; // Any port

    EFI_IPv4_ADDRESS RemoteAddress = { 127, 0, 0, 1 }; // Server IP
    UINT16 RemotePort = 5555; // Server port

    Udp4ConfigData.AcceptBroadcast = FALSE;
    Udp4ConfigData.AcceptPromiscuous = FALSE;
    Udp4ConfigData.AcceptAnyPort = FALSE;
    Udp4ConfigData.AllowDuplicatePort = FALSE;

    Udp4ConfigData.TimeToLive = 16;
    Udp4ConfigData.TypeOfService = 0;
    Udp4ConfigData.DoNotFragment = TRUE;
    Udp4ConfigData.ReceiveTimeout = 0;
    Udp4ConfigData.TransmitTimeout = 0;

    Udp4ConfigData.UseDefaultAddress = FALSE;

    gBS->CopyMem(&Udp4ConfigData.StationAddress, &LocalAddress, sizeof(Udp4ConfigData.StationAddress));
    gBS->CopyMem(&Udp4ConfigData.SubnetMask, &SubnetMask, sizeof(Udp4ConfigData.SubnetMask));
    Udp4ConfigData.StationPort = LocalPort;
    gBS->CopyMem(&Udp4ConfigData.RemoteAddress, &RemoteAddress, sizeof(Udp4ConfigData.RemoteAddress));
    Udp4ConfigData.RemotePort = RemotePort;

    Status = Udp4Protocol->Configure(Udp4Protocol, &Udp4ConfigData);

    if (EFI_ERROR(Status)) {
        Print(L"Failed to get configure child instance handle. Status: %r\n", Status);

        return Status;
    }

    /* (4) Create Transmit Token */

    EFI_UDP4_COMPLETION_TOKEN Udp4TransmitCompletionToken;

    Udp4TransmitCompletionToken.Status = EFI_SUCCESS;
    Udp4TransmitCompletionToken.Event = NULL;

    Status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, TransmitEventCallback, NULL, &Udp4TransmitCompletionToken.Event);

    if (EFI_ERROR(Status)) {
        Print(L"Failed to create Transmit Token. Status: %r\n", Status);

        return Status;
    }

    EFI_UDP4_TRANSMIT_DATA Udp4TransmitData;

    Udp4TransmitCompletionToken.Packet.TxData = &Udp4TransmitData;

    CHAR8 TxBuffer[] = "Hello, Server!";

    Udp4TransmitData.UdpSessionData = NULL;
    gBS->SetMem(&Udp4TransmitData.GatewayAddress, sizeof(Udp4TransmitData.GatewayAddress), 0x00);
    Udp4TransmitData.DataLength = sizeof(TxBuffer);
    Udp4TransmitData.FragmentCount = 1;
    Udp4TransmitData.FragmentTable[0].FragmentLength = Udp4TransmitData.DataLength;
    Udp4TransmitData.FragmentTable[0].FragmentBuffer = TxBuffer;

    /* (5) Transmit data */

    TransmitCompleteFlag = FALSE;

    Status = Udp4Protocol->Transmit(Udp4Protocol, &Udp4TransmitCompletionToken);

    if (EFI_ERROR(Status)) {
        Print(L"Failed to initialize data transmission. Status: %r\n", Status);

        return Status;
    }

    Status = WaitForFlag(&TransmitCompleteFlag, Udp4Protocol, 10);

    if (EFI_ERROR(Status) || EFI_ERROR(Udp4TransmitCompletionToken.Status)) {
        Print(L"Failed to transmit data. Status: %r\n", Status);

        return Status;
    }

    Print(L"Sent your data!\n");

    /* (6) Cleanup */

    Status = Udp4ServiceBindingProtocol->DestroyChild(Udp4ServiceBindingProtocol, Udp4ChildHandle);

    if (EFI_ERROR(Status)) {
        Print(L"Failed to destroy child instance. Status: %r\n", Status);
    }

    return Status;
}