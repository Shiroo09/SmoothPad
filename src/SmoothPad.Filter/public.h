#pragma once

// Shared user/kernel protocol. Keep this header C-compatible.

#define SMOOTHPAD_DEVICE_NAME L"\\Device\\SmoothPadFilter"
#define SMOOTHPAD_SYMLINK_NAME L"\\DosDevices\\SmoothPadFilter"
#define SMOOTHPAD_WIN32_NAME L"\\\\.\\SmoothPadFilter"

#ifndef CTL_CODE
#define CTL_CODE(DeviceType, Function, Method, Access) \
  (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#define METHOD_BUFFERED 0
#define FILE_ANY_ACCESS 0
#define FILE_DEVICE_UNKNOWN 0x00000022
#endif

// Function codes are 12-bit. 0x800–0xFFF is the customer range.
#define SMOOTHPAD_FUNCTION_HEARTBEAT 0x800
#define SMOOTHPAD_FUNCTION_SET_MODE 0x801
#define SMOOTHPAD_FUNCTION_GET_STATUS 0x802
#define SMOOTHPAD_FUNCTION_READ_REPORTS 0x803

#define SMOOTHPAD_IOCTL_HEARTBEAT \
  CTL_CODE(FILE_DEVICE_UNKNOWN, SMOOTHPAD_FUNCTION_HEARTBEAT, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define SMOOTHPAD_IOCTL_SET_MODE \
  CTL_CODE(FILE_DEVICE_UNKNOWN, SMOOTHPAD_FUNCTION_SET_MODE, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define SMOOTHPAD_IOCTL_GET_STATUS \
  CTL_CODE(FILE_DEVICE_UNKNOWN, SMOOTHPAD_FUNCTION_GET_STATUS, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define SMOOTHPAD_IOCTL_READ_REPORTS \
  CTL_CODE(FILE_DEVICE_UNKNOWN, SMOOTHPAD_FUNCTION_READ_REPORTS, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define SMOOTHPAD_MODE_PASSTHROUGH 0u
#define SMOOTHPAD_MODE_OBSERVE 1u
#define SMOOTHPAD_MODE_CUSTOM 2u

#define SMOOTHPAD_WATCHDOG_MS 250u

typedef struct _SMOOTHPAD_FILTER_STATUS {
  unsigned long mode;
  unsigned char connected;
  unsigned char deviceMatched;
  unsigned char reserved[2];
  unsigned long droppedReports;
  unsigned long lastHeartbeatMs;
} SMOOTHPAD_FILTER_STATUS;
