#pragma once

#include "smoothpad/Types.h"
#include "smoothpad/motion/MotionEngine.h"

#include <memory>
#include <string>

namespace smoothpad::output {

struct OutputFrame {
  MotionOutput motion;
  RawContactFrame contacts;
  DeviceCaps caps;
};

class IPointerOutput {
 public:
  virtual ~IPointerOutput() = default;
  virtual bool Initialize(const DeviceCaps& caps, std::string* error) = 0;
  virtual void Shutdown() = 0;
  virtual bool Inject(const OutputFrame& frame, std::string* error) = 0;
  [[nodiscard]] virtual OutputBackend Backend() const noexcept = 0;
  [[nodiscard]] virtual const wchar_t* Name() const noexcept = 0;
  [[nodiscard]] virtual bool Available() const noexcept = 0;
};

class DiagnosticMouseOutput final : public IPointerOutput {
 public:
  bool Initialize(const DeviceCaps& caps, std::string* error) override;
  void Shutdown() override;
  bool Inject(const OutputFrame& frame, std::string* error) override;
  [[nodiscard]] OutputBackend Backend() const noexcept override { return OutputBackend::DiagnosticMouse; }
  [[nodiscard]] const wchar_t* Name() const noexcept override { return L"Diagnostic SendInput (dev only)"; }
  [[nodiscard]] bool Available() const noexcept override { return true; }

 private:
  bool ready_ = false;
};

class SyntheticTouchpadOutput final : public IPointerOutput {
 public:
  bool Initialize(const DeviceCaps& caps, std::string* error) override;
  void Shutdown() override;
  bool Inject(const OutputFrame& frame, std::string* error) override;
  [[nodiscard]] OutputBackend Backend() const noexcept override { return OutputBackend::SyntheticTouchpad; }
  [[nodiscard]] const wchar_t* Name() const noexcept override { return L"Synthetic PT_TOUCHPAD"; }
  [[nodiscard]] bool Available() const noexcept override { return available_; }
  [[nodiscard]] bool UsesCreate2() const noexcept { return usesCreate2_; }

 private:
  void* device_ = nullptr;
  bool available_ = false;
  bool usesCreate2_ = false;
  DeviceCaps caps_{};
  bool contactDown_ = false;
};

[[nodiscard]] std::unique_ptr<IPointerOutput> CreatePreferredOutput(bool allowDiagnosticFallback);

}  // namespace smoothpad::output
