#include "usbhub.h"

//#define NDEBUG
#include <debug.h>

NTSTATUS
NTAPI
USBH_HubSetD0(IN PUSBHUB_FDO_EXTENSION HubExtension)
{
    DPRINT1("USBH_HubSetD0: UNIMPLEMENTED. FIXME. \n");
    DbgBreakPoint();
    return 0;
}

NTSTATUS
NTAPI
USBH_FdoPower(IN PUSBHUB_FDO_EXTENSION HubExtension,
              IN PIRP Irp,
              IN UCHAR Minor)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;
    POWER_STATE PowerState;
    POWER_STATE DevicePwrState;
    BOOLEAN IsAllPortsD3;
    PUSBHUB_PORT_DATA PortData;
    PDEVICE_OBJECT PdoDevice;
    PUSBHUB_PORT_PDO_EXTENSION PortExtension;
    ULONG Port;

    DPRINT("USBH_FdoPower: HubExtension - %p, Irp - %p, Minor - %x\n",
           HubExtension,
           Irp,
           Minor);

    switch (Minor)
    {
      case IRP_MN_WAIT_WAKE:
          DPRINT("USBH_FdoPower: IRP_MN_WAIT_WAKE\n");
          IoCopyCurrentIrpStackLocationToNext(Irp);

          IoSetCompletionRoutine(Irp,
                                 USBH_FdoWWIrpIoCompletion,
                                 HubExtension,
                                 TRUE,
                                 TRUE,
                                 TRUE);

          PoStartNextPowerIrp(Irp);
          IoMarkIrpPending(Irp);

          PoCallDriver(HubExtension->LowerDevice, Irp);

          return STATUS_PENDING;

      case IRP_MN_POWER_SEQUENCE:
          DPRINT("USBH_FdoPower: IRP_MN_POWER_SEQUENCE\n");
          break;

      case IRP_MN_SET_POWER:
          DPRINT("USBH_FdoPower: IRP_MN_SET_POWER\n");

          IoStack = IoGetCurrentIrpStackLocation(Irp);
          DPRINT("USBH_FdoPower: IRP_MN_SET_POWER/DevicePowerState\n");
          PowerState = IoStack->Parameters.Power.State;

          if (IoStack->Parameters.Power.Type == DevicePowerState)
          {
              DPRINT("USBH_FdoPower: PowerState - %x\n", PowerState);

              if (HubExtension->CurrentPowerState.DeviceState == PowerState.DeviceState)
              {
                    IoCopyCurrentIrpStackLocationToNext(Irp);
                    PoStartNextPowerIrp(Irp);
                    IoMarkIrpPending(Irp);

                    PoCallDriver(HubExtension->LowerDevice, Irp);

                    return STATUS_PENDING;
              }

              switch (PowerState.DeviceState)
              {
                  case PowerDeviceD0:
                      if (!(HubExtension->HubFlags & USBHUB_FDO_FLAG_SET_D0_STATE))
                      {
                          HubExtension->HubFlags &= ~(USBHUB_FDO_FLAG_NOT_D0_STATE |
                                                      USBHUB_FDO_FLAG_DEVICE_STOPPING);

                          HubExtension->HubFlags |= USBHUB_FDO_FLAG_SET_D0_STATE;

                          IoCopyCurrentIrpStackLocationToNext(Irp);

                          IoSetCompletionRoutine(Irp,
                                                 USBH_PowerIrpCompletion,
                                                 HubExtension,
                                                 TRUE,
                                                 TRUE,
                                                 TRUE);
                      }
                      else
                      {
                          IoCopyCurrentIrpStackLocationToNext(Irp);
                          PoStartNextPowerIrp(Irp);
                      }

                      IoMarkIrpPending(Irp);
                      PoCallDriver(HubExtension->LowerDevice, Irp);
                      return STATUS_PENDING;

                  case PowerDeviceD1:
                  case PowerDeviceD2:
                  case PowerDeviceD3:
                      if (HubExtension->ResetRequestCount)
                      {
                          IoCancelIrp(HubExtension->ResetPortIrp);

                          KeWaitForSingleObject(&HubExtension->ResetEvent,
                                                Suspended,
                                                KernelMode,
                                                FALSE,
                                                NULL);
                      }

                      if (!(HubExtension->HubFlags & USBHUB_FDO_FLAG_DEVICE_STOPPED))
                      {
                          HubExtension->HubFlags |= (USBHUB_FDO_FLAG_NOT_D0_STATE |
                                                     USBHUB_FDO_FLAG_DEVICE_STOPPING);

                          IoCancelIrp(HubExtension->SCEIrp);

                          KeWaitForSingleObject(&HubExtension->StatusChangeEvent,
                                                Suspended,
                                                KernelMode,
                                                FALSE,
                                                NULL);
                      }

                      HubExtension->CurrentPowerState.DeviceState = PowerState.DeviceState;

                      if (HubExtension->HubFlags & USBHUB_FDO_FLAG_DO_SUSPENSE &&
                          USBH_CheckIdleAbort(HubExtension, TRUE, TRUE) == TRUE)
                      {
                          HubExtension->HubFlags &= ~(USBHUB_FDO_FLAG_NOT_D0_STATE |
                                                      USBHUB_FDO_FLAG_DEVICE_STOPPING);

                          HubExtension->CurrentPowerState.DeviceState = PowerDeviceD0;

                          USBH_SubmitStatusChangeTransfer(HubExtension);

                          PoStartNextPowerIrp(Irp);

                          Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
                          IoCompleteRequest(Irp, IO_NO_INCREMENT);

                          HubExtension->HubFlags &= ~USBHUB_FDO_FLAG_DO_SUSPENSE;

                          KeReleaseSemaphore(&HubExtension->IdleSemaphore,
                                             LOW_REALTIME_PRIORITY,
                                             1,
                                             FALSE);

                          return STATUS_UNSUCCESSFUL;
                      }

                      IoCopyCurrentIrpStackLocationToNext(Irp);

                      IoSetCompletionRoutine(Irp,
                                             USBH_PowerIrpCompletion,
                                             HubExtension,
                                             TRUE,
                                             TRUE,
                                             TRUE);

                      PoStartNextPowerIrp(Irp);

                      IoMarkIrpPending(Irp);
                      PoCallDriver(HubExtension->LowerDevice, Irp);

                      if (HubExtension->HubFlags & USBHUB_FDO_FLAG_DO_SUSPENSE)
                      {
                          HubExtension->HubFlags &= ~USBHUB_FDO_FLAG_DO_SUSPENSE;

                          KeReleaseSemaphore(&HubExtension->IdleSemaphore,
                                             LOW_REALTIME_PRIORITY,
                                             1,
                                             FALSE);
                      }

                      return STATUS_PENDING;

                  default:
                      DPRINT1("USBH_FdoPower: Unsupported PowerState.DeviceState\n");
                      DbgBreakPoint();
                      break;
              }
          }
          else
          {
              if (PowerState.SystemState != PowerSystemWorking)
              {
                  USBH_GetRootHubExtension(HubExtension)->SystemPowerState.SystemState =
                                                          PowerState.SystemState;
              }

              if (PowerState.SystemState == PowerSystemHibernate)
              {
                  HubExtension->HubFlags |= USBHUB_FDO_FLAG_HIBERNATE_STATE;
              }

              PortData = HubExtension->PortData;

              IsAllPortsD3 = 1;

              if (PortData)
              {
                  if (HubExtension->HubDescriptor &&
                      HubExtension->HubDescriptor->bNumberOfPorts)
                  {
                      Port = 0;

                      while (TRUE)
                      {
                          PdoDevice = PortData[Port].DeviceObject;

                          if (PdoDevice)
                          {
                              PortExtension = (PUSBHUB_PORT_PDO_EXTENSION)PdoDevice->DeviceExtension;

                              if (PortExtension->CurrentPowerState.DeviceState != PowerDeviceD3)
                              {
                                  break;
                              }
                          }

                          ++Port;

                          if (Port >= HubExtension->HubDescriptor->bNumberOfPorts)
                          {
                              goto Next;
                          }
                      }

                      IsAllPortsD3 = FALSE;
                  }
              }

          Next:

              if (PowerState.SystemState == PowerSystemWorking)
              {
                  DevicePwrState.DeviceState = PowerDeviceD0;
              }
              else if (HubExtension->HubFlags & USBHUB_FDO_FLAG_PENDING_WAKE_IRP ||
                      !IsAllPortsD3)
              {
                  DevicePwrState.DeviceState = HubExtension->DeviceState[PowerState.SystemState];

                  if (DevicePwrState.DeviceState == PowerDeviceUnspecified)
                  {
                      goto Exit;
                  }
              }
              else
              {
                  DevicePwrState.DeviceState = PowerDeviceD3;
              }

              if (DevicePwrState.DeviceState != HubExtension->CurrentPowerState.DeviceState &&
                  HubExtension->HubFlags & USBHUB_FDO_FLAG_DEVICE_STARTED)
              {
                  HubExtension->PowerIrp = Irp;

                  IoMarkIrpPending(Irp);

                  if (PoRequestPowerIrp(HubExtension->LowerPDO,
                                        IRP_MN_SET_POWER,
                                        DevicePwrState,
                                        USBH_FdoDeferPoRequestCompletion,
                                        (PVOID)HubExtension,
                                        0) == STATUS_PENDING)
                  {
                      return STATUS_PENDING;
                  }

                  IoCopyCurrentIrpStackLocationToNext(Irp);
                  PoStartNextPowerIrp(Irp);
                  PoCallDriver(HubExtension->LowerDevice, Irp);

                  return STATUS_PENDING;
              }

          Exit:

              HubExtension->SystemPowerState.SystemState = PowerState.SystemState;

              if (PowerState.SystemState == PowerSystemWorking)
              {
                  USBH_CheckIdleDeferred(HubExtension);
              }

              IoCopyCurrentIrpStackLocationToNext(Irp);
              PoStartNextPowerIrp(Irp);

              return PoCallDriver(HubExtension->LowerDevice, Irp);
          }

          break;

      case IRP_MN_QUERY_POWER:
          DPRINT("USBH_FdoPower: IRP_MN_QUERY_POWER\n");
          break;

      default:
          DPRINT1("USBH_FdoPower: unknown IRP_MN_POWER!\n");
          break;
    }

    IoCopyCurrentIrpStackLocationToNext(Irp);
    PoStartNextPowerIrp(Irp);
    Status = PoCallDriver(HubExtension->LowerDevice, Irp);

    return Status;
}

NTSTATUS
NTAPI
USBH_PdoPower(IN PUSBHUB_PORT_PDO_EXTENSION PortExtension,
              IN PIRP Irp,
              IN UCHAR Minor)
{
    NTSTATUS Status=0;

    DPRINT("USBH_FdoPower: PortExtension - %p, Irp - %p, Minor - %x\n",
           PortExtension,
           Irp,
           Minor);

    switch (Minor)
    {
      case IRP_MN_WAIT_WAKE:
          DPRINT("USBPORT_PdoPower: IRP_MN_WAIT_WAKE\n");
          PoStartNextPowerIrp(Irp);
          break;

      case IRP_MN_POWER_SEQUENCE:
          DPRINT("USBPORT_PdoPower: IRP_MN_POWER_SEQUENCE\n");
          PoStartNextPowerIrp(Irp);
          break;

      case IRP_MN_SET_POWER:
          DPRINT("USBPORT_PdoPower: IRP_MN_SET_POWER\n");
          PoStartNextPowerIrp(Irp);
          break;

      case IRP_MN_QUERY_POWER:
          DPRINT("USBPORT_PdoPower: IRP_MN_QUERY_POWER\n");
          PoStartNextPowerIrp(Irp);
          break;

      default:
          DPRINT1("USBPORT_PdoPower: unknown IRP_MN_POWER!\n");
          PoStartNextPowerIrp(Irp);
          break;
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}
