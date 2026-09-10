// SmoothPadFilter — minimal KMDF HID upper filter.
// Default: PASS-THROUGH. Custom suppression is armed only while user-mode
// heartbeats arrive within SMOOTHPAD_WATCHDOG_MS.
//
// This file is the production skeleton. It compiles only with WDK.
// Without WDK the user-mode product still builds; the filter is optional.

#include "public.h"

#ifdef _KERNEL_MODE

#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD SmoothPadEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL SmoothPadEvtInternalIoctl;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL SmoothPadEvtDeviceControl;
EVT_WDF_TIMER SmoothPadEvtWatchdog;

typedef struct _DEVICE_CONTEXT {
  WDFDEVICE device;
  WDFQUEUE defaultQueue;
  WDFQUEUE hidQueue;
  WDFTIMER watchdog;
  LONG mode;  // SMOOTHPAD_MODE_*
  LARGE_INTEGER lastHeartbeat;
  ULONG droppedReports;
  BOOLEAN matched;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

static BOOLEAN
WatchdogExpired(PDEVICE_CONTEXT ctx)
{
  LARGE_INTEGER now;
  KeQuerySystemTimePrecise(&now);
  const LONGLONG delta100ns = now.QuadPart - ctx->lastHeartbeat.QuadPart;
  const ULONG ms = (ULONG)(delta100ns / 10000);
  return ms > SMOOTHPAD_WATCHDOG_MS;
}

static VOID
FailOpen(PDEVICE_CONTEXT ctx)
{
  InterlockedExchange(&ctx->mode, (LONG)SMOOTHPAD_MODE_PASSTHROUGH);
}

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT driverObject, _In_ PUNICODE_STRING registryPath)
{
  WDF_DRIVER_CONFIG config;
  WDF_DRIVER_CONFIG_INIT(&config, SmoothPadEvtDeviceAdd);
  return WdfDriverCreate(driverObject, registryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}

NTSTATUS
SmoothPadEvtDeviceAdd(_In_ WDFDRIVER driver, _Inout_ PWDFDEVICE_INIT deviceInit)
{
  UNREFERENCED_PARAMETER(driver);
  WdfFdoInitSetFilter(deviceInit);

  WDF_OBJECT_ATTRIBUTES attrs;
  WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attrs, DEVICE_CONTEXT);

  WDFDEVICE device;
  NTSTATUS status = WdfDeviceCreate(&deviceInit, &attrs, &device);
  if (!NT_SUCCESS(status)) {
    return status;
  }

  PDEVICE_CONTEXT ctx = DeviceGetContext(device);
  RtlZeroMemory(ctx, sizeof(*ctx));
  ctx->device = device;
  ctx->mode = SMOOTHPAD_MODE_PASSTHROUGH;
  ctx->matched = TRUE;
  KeQuerySystemTimePrecise(&ctx->lastHeartbeat);

  WDF_IO_QUEUE_CONFIG qcfg;
  WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&qcfg, WdfIoQueueDispatchParallel);
  qcfg.EvtIoDeviceControl = SmoothPadEvtDeviceControl;
  qcfg.EvtIoInternalDeviceControl = SmoothPadEvtInternalIoctl;
  status = WdfIoQueueCreate(device, &qcfg, WDF_NO_OBJECT_ATTRIBUTES, &ctx->defaultQueue);
  if (!NT_SUCCESS(status)) {
    return status;
  }

  WDF_TIMER_CONFIG tcfg;
  WDF_TIMER_CONFIG_INIT(&tcfg, SmoothPadEvtWatchdog);
  tcfg.Period = 50;
  WDF_OBJECT_ATTRIBUTES tattrs;
  WDF_OBJECT_ATTRIBUTES_INIT(&tattrs);
  tattrs.ParentObject = device;
  status = WdfTimerCreate(&tcfg, &tattrs, &ctx->watchdog);
  if (NT_SUCCESS(status)) {
    WdfTimerStart(ctx->watchdog, WDF_REL_TIMEOUT_IN_MS(50));
  }

  DECLARE_CONST_UNICODE_STRING(symlink, SMOOTHPAD_SYMLINK_NAME);
  (void)WdfDeviceCreateSymbolicLink(device, &symlink);
  return STATUS_SUCCESS;
}

VOID
SmoothPadEvtWatchdog(_In_ WDFTIMER timer)
{
  WDFDEVICE device = WdfTimerGetParentObject(timer);
  PDEVICE_CONTEXT ctx = DeviceGetContext(device);
  if (ctx->mode == (LONG)SMOOTHPAD_MODE_CUSTOM && WatchdogExpired(ctx)) {
    FailOpen(ctx);
  }
}

VOID
SmoothPadEvtDeviceControl(_In_ WDFQUEUE queue,
                          _In_ WDFREQUEST request,
                          _In_ size_t outputBufferLength,
                          _In_ size_t inputBufferLength,
                          _In_ ULONG ioControlCode)
{
  UNREFERENCED_PARAMETER(outputBufferLength);
  UNREFERENCED_PARAMETER(inputBufferLength);
  PDEVICE_CONTEXT ctx = DeviceGetContext(WdfIoQueueGetDevice(queue));
  NTSTATUS status = STATUS_SUCCESS;
  ULONG_PTR info = 0;

  switch (ioControlCode) {
    case SMOOTHPAD_IOCTL_HEARTBEAT:
      KeQuerySystemTimePrecise(&ctx->lastHeartbeat);
      break;
    case SMOOTHPAD_IOCTL_SET_MODE: {
      unsigned long* mode = NULL;
      status = WdfRequestRetrieveInputBuffer(request, sizeof(unsigned long), (PVOID*)&mode, NULL);
      if (NT_SUCCESS(status) && mode) {
        if (*mode > SMOOTHPAD_MODE_CUSTOM) {
          status = STATUS_INVALID_PARAMETER;
        } else {
          InterlockedExchange(&ctx->mode, (LONG)(*mode));
          KeQuerySystemTimePrecise(&ctx->lastHeartbeat);
        }
      }
      break;
    }
    case SMOOTHPAD_IOCTL_GET_STATUS: {
      SMOOTHPAD_FILTER_STATUS* out = NULL;
      status = WdfRequestRetrieveOutputBuffer(request, sizeof(*out), (PVOID*)&out, NULL);
      if (NT_SUCCESS(status) && out) {
        RtlZeroMemory(out, sizeof(*out));
        out->mode = (unsigned long)ctx->mode;
        out->connected = 1;
        out->deviceMatched = ctx->matched;
        out->droppedReports = ctx->droppedReports;
        LARGE_INTEGER now;
        KeQuerySystemTimePrecise(&now);
        out->lastHeartbeatMs = (unsigned long)((now.QuadPart - ctx->lastHeartbeat.QuadPart) / 10000);
        info = sizeof(*out);
      }
      break;
    }
    default:
      WdfRequestSend(request, WdfDeviceGetIoTarget(WdfIoQueueGetDevice(queue)), WDF_NO_SEND_OPTIONS);
      return;
  }
  WdfRequestCompleteWithInformation(request, status, info);
}

VOID
SmoothPadEvtInternalIoctl(_In_ WDFQUEUE queue,
                          _In_ WDFREQUEST request,
                          _In_ size_t outputBufferLength,
                          _In_ size_t inputBufferLength,
                          _In_ ULONG ioControlCode)
{
  UNREFERENCED_PARAMETER(outputBufferLength);
  UNREFERENCED_PARAMETER(inputBufferLength);
  PDEVICE_CONTEXT ctx = DeviceGetContext(WdfIoQueueGetDevice(queue));

  // Default and watchdog-expired: never touch reports.
  if (ctx->mode != (LONG)SMOOTHPAD_MODE_CUSTOM || WatchdogExpired(ctx)) {
    if (WatchdogExpired(ctx)) {
      FailOpen(ctx);
    }
    WdfRequestSend(request, WdfDeviceGetIoTarget(WdfIoQueueGetDevice(queue)), WDF_NO_SEND_OPTIONS);
    return;
  }

  // Custom mode: still pass IOCTL_HID_READ_REPORT / IOCTL_HID_GET_REPORT through
  // by default. Suppression is staged (observe first). Completing a read with
  // zero bytes is how native promotion is stopped — only after heartbeat + mode.
  // Step 6 of the driver order: currently observe-only; user-mode must set CUSTOM
  // AND the watchdog must be fresh. Completing with STATUS_SUCCESS / 0 is the
  // suppression path and is compiled behind a conservative default of pass-through
  // unless the report is a PTP mouse-collection sibling. Keep pass-through until
  // hardware verification.
  UNREFERENCED_PARAMETER(ioControlCode);
  WdfRequestSend(request, WdfDeviceGetIoTarget(WdfIoQueueGetDevice(queue)), WDF_NO_SEND_OPTIONS);
}

#else

// User-mode stub so the filter sources stay in the tree without WDK.
int SmoothPadFilter_UserModeStub(void) { return 0; }

#endif
