#include "Test.h"
#include "smoothpad/Types.h"
#include "smoothpad/input/TouchpadHid.h"

#include <cmath>

using namespace smoothpad;
using namespace smoothpad::input;

SP_TEST(Input_IdentifiesPtp) {
  SP_CHECK(IsPrecisionTouchpad(0x0D, 0x05));
  SP_CHECK(!IsPrecisionTouchpad(0x01, 0x02));
}

SP_TEST(Input_TransportGuess) {
  SP_CHECK(GuessTransport(L"\\\\?\\HID#BTHENUM") == Transport::Bluetooth);
  SP_CHECK(GuessTransport(L"\\\\?\\HID#VID_046D&USB") == Transport::Usb);
  SP_CHECK(GuessTransport(L"\\\\?\\ACPI#MSFT0001#I2C") == Transport::I2c);
  SP_CHECK(GuessTransport(L"\\\\?\\HID#UNIW0001&Col02#4") == Transport::I2c);
}

SP_TEST(Input_HidPhysicalToMm_SignNibble) {
  // Typical PTP: Units 0x11 (cm), UnitsExp nibble 0xE (-2), physical 1210 → 121 mm.
  SP_CHECK(std::abs(HidPhysicalToMm(1210, 0x11, 0xE) - 121.0) < 0.01);
  SP_CHECK(std::abs(HidPhysicalToMm(740, 0x11, -2) - 74.0) < 0.01);
  SP_CHECK(!PlausibleTouchpadMm(1.21e18, 1.0));
}

SP_TEST(Input_SyntheticParserReady) {
  HidReportParser p;
  DeviceCaps caps{};
  caps.identity.usagePage = 0x0D;
  caps.identity.usage = 0x05;
  p.SetSyntheticCaps(caps, {}, {});
  SP_CHECK(p.Ready());
}
