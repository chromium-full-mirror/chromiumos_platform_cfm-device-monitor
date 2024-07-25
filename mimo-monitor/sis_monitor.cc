// Copyright 2017 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/mimo-monitor/sis_monitor.h"

#include <base/logging.h>
#include <brillo/syslog_logging.h>

#include <libusb-1.0/libusb.h>
#include <array>
#include <ctime>
#include <string>

#include "cfm-device-monitor/mimo-monitor/utils.h"

namespace {

const int kMaxCmdLen = 64;
const int kPingResponseSize = 0x3b;
const int kResetResponseSize = 0x07;

const unsigned char kCmdReset[4] = {0x09, 0x03, 0x82, 0xca};
const unsigned char kCmdPwr[6] = {0x09, 0x05, 0x85, 0x0d, 0x51, 0x09};
const unsigned char kCmdDiag[6] = {0x09, 0x05, 0x85, 0x5c, 0x21, 0x01};
// Disable report to OS.
const unsigned char kCmdDisableReport[6] = {0x09, 0x05, 0x85, 0x45, 0x00, 0x0f};
// Enable report to OS.
const unsigned char kCmdEnableReport[6] = {0x09, 0x05, 0x85, 0x74, 0x01, 0x0f};
const unsigned char kCmdRead[10] = {0x09, 0x09, 0x86, 0xee, 0xe0,
                                    0xbf, 0x00, 0xa0, 0x34, 0x00};

const std::array<int, 5> kExpectedResponse = {0x0a, 0xef, 0xbe, 0, 0};
const int kBitsToCheck[5] = {0, 4, 5, 6, 7};

const uint16_t kIdVendorSiS = 0x266e;
// SiS VID in recovery mode.
const uint16_t kIdVendorSiSRecovery = 0x0457;
// SiS VID of some engineering units.
const uint16_t kIdVendorSiSEngineer = 0x1391;
const uint16_t kIdProductSiSRecovery = 0xf817;
const uint16_t kIdProductSiSEngineer = 0x2112;

const int kTransferTimeoutMs = 1000;

const int kNumInterface = 1;
const int kNumAltSetting = 1;
const int kNumEndpoint = 2;

}  // namespace

namespace mimo_monitor {

SiSMonitor::SiSMonitor() {
  touch_version_ = Unknown;
  touch_status_ = false;
  mfg_size_ = 0;
  mfg_port_ = 0;
  addr_in_ = 0;
  addr_out_ = 0;
  manufacturer_ = "";
}

bool SiSMonitor::IsSiS(libusb_device *device,
                       const libusb_device_descriptor &desc) {
  int ret = 0;
  libusb_config_descriptor *cfg_desc_;
  std::string info;

  if (desc.idVendor != kIdVendorSiS &&
      ((desc.idVendor != kIdVendorSiSRecovery) ||
       (desc.idProduct != kIdProductSiSRecovery)) &&
      ((desc.idVendor != kIdVendorSiSEngineer) ||
       (desc.idProduct != kIdProductSiSEngineer))) {
    return false;
  }

  touch_version_ = Unknown;

  // Read Manufacturer.
  mfg_port_ = desc.iManufacturer;
  mfg_size_ = ReadID(device, mfg_port_, &manufacturer_);
  if (mfg_size_ <= 0) {
    LOG(WARNING) << "Read touch manufacturer failed.";
    return false;
  }
  VLOG(1) << "Touch manufacturer: " << manufacturer_;

  // Read product.
  ret = ReadID(device, desc.iProduct, &info);
  if (ret <= 0) {
    LOG(WARNING) << "Read touch product fail.";
    return false;
  }
  VLOG(1) << "Touch product: " << info;

  if (info == "SiS") {
    touch_version_ = SiS9255;
  }

  ret = libusb_get_config_descriptor(device, 0, &cfg_desc_);

  ScopedUsbCfgDesc cfg_desc(cfg_desc_);

  if (!cfg_desc.is_valid()) {
    LOG(WARNING) << "Failed to get config descriptor.";
    return false;
  }

  if (cfg_desc.get()->bNumInterfaces != kNumInterface) {
    LOG(WARNING) << "SiS: number of interface mismatch.";
    return false;
  }

  if (cfg_desc.get()->interface[0].num_altsetting != kNumAltSetting) {
    LOG(WARNING) << "SiS: number of altsetting mismatch.";
    return false;
  }

  if (cfg_desc.get()->interface[0].altsetting[0].bNumEndpoints !=
      kNumEndpoint) {
    LOG(WARNING) << "SiS: number of endpoint mismatch.";
    return false;
  }

  // Thare two endpoints and the input endpoint address should be > 0x7f.
  if (cfg_desc.get()->interface[0].altsetting[0].endpoint[0].bEndpointAddress >
      0x7f) {
    addr_in_ =
        cfg_desc.get()->interface[0].altsetting[0].endpoint[0].bEndpointAddress;
    addr_out_ =
        cfg_desc.get()->interface[0].altsetting[0].endpoint[1].bEndpointAddress;
  } else {
    addr_out_ =
        cfg_desc.get()->interface[0].altsetting[0].endpoint[0].bEndpointAddress;
    addr_in_ =
        cfg_desc.get()->interface[0].altsetting[0].endpoint[1].bEndpointAddress;
  }

  // SiS9225+F321 has output endpoint address 0x01 while SiS9255 has 0x02.
  if ((addr_in_ == 0x82) && (addr_out_ == 0x01)) {
    touch_version_ = SiS9255_F321;
  }

  touch_status_ = true;
  return true;
}

bool SiSMonitor::PingSiS(libusb_device *device, int interface) {
  int ret;
  int ping_ret;
  libusb_device_handle *dev_handle_ = NULL;
  unsigned char data_buf[kMaxCmdLen] = {0};

  ret = libusb_open(device, &dev_handle_);
  if (ret < 0) {
    // There was an error opening the handle. Device is not there.
    LOG(WARNING) << "Failed to Open device in PingSiS.";
    return false;
  }

  ScopedUsbDevHandle dev_handle(dev_handle_);

  // If there is a driver attached, this detachs it so that we can send it the
  // request
  ret = libusb_kernel_driver_active(dev_handle.get(), interface);
  if (ret < 0) {
    LOG(WARNING) << "Failed to get kernel driver status, error: " << ret;
    return false;
  }
  if (ret == 1) {
    VLOG(1) << "Kernel driver is attached, detach it first.";
    if ((ret = libusb_detach_kernel_driver(dev_handle.get(), interface))) {
      LOG(WARNING) << "Failed to detach kernel driver when ping sis.";
      return false;
    }
  }

  ret = libusb_claim_interface(dev_handle.get(), interface);
  // There was an error trying to claim the USB interface from the kernel.
  if (ret < 0) {
    LOG(WARNING) << "Failed to claim interface when ping SiS, error: " << ret;
    return false;
  }

  // Disable report to OS.
  if (touch_version_ == SiS9255) {
    ret = SendSiSCommand(dev_handle.get(), kCmdDisableReport,
                         sizeof(kCmdDisableReport), data_buf, kMaxCmdLen);
    if (ret < 0) return false;
  }

  // Read data command.
  ret = SendSiSCommand(dev_handle.get(), kCmdRead, sizeof(kCmdRead), data_buf,
                       kMaxCmdLen);
  if (ret < 0) return false;

  // According to vendor, the second byte of received data is the response
  // length.
  int response_size = data_buf[1];
  ping_ret = CheckPingResponse(data_buf, response_size);

  // Re-enable report to OS.
  if (touch_version_ == SiS9255) {
    SendSiSCommand(dev_handle.get(), kCmdEnableReport, sizeof(kCmdEnableReport),
                   data_buf, kMaxCmdLen);
    if (ret < 0) return false;
  }

  // Touch panel did not respond.
  if (ping_ret) {
    VLOG(1) << "Pinging SiS respond as expected.";
  }

  // Let everything re-attach and release back to the kernel driver
  ret = libusb_release_interface(dev_handle.get(), interface);
  if (ret < 0) {
    LOG(WARNING) << "Failed to release device interface, error " << ret;
    return false;
  }

  ret = libusb_attach_kernel_driver(dev_handle.get(), interface);
  if (ret < 0) {
    LOG(WARNING) << "Re-attach SiS kernal driver fail, error: " << ret;
    return false;
  }
  return ping_ret;
}

int SiSMonitor::SendSiSCommand(libusb_device_handle *dev_handle,
                               const unsigned char *cmd, int cmd_size,
                               unsigned char *response, int max_response_size) {
  unsigned char cmd_buf[kMaxCmdLen] = {0};
  int ret = 0;
  int transfer_size = 0;

  memcpy(cmd_buf, cmd, cmd_size);
  memset(response, 0, max_response_size);

  ret = libusb_interrupt_transfer(dev_handle, addr_out_, cmd_buf, cmd_size,
                                  &transfer_size, kTransferTimeoutMs);
  if (ret < 0) {
    LOG(WARNING) << "Send SiS command fail, error: " << ret;
    return -1;
  }
  if (transfer_size != cmd_size) {
    LOG(WARNING) << "Send SiS command incomplete, expect " << cmd_size
                 << " bytes, acutal " << transfer_size << " bytes.";
    return -1;
  }

  ret = libusb_interrupt_transfer(dev_handle, addr_in_, response,
                                  max_response_size, &transfer_size,
                                  kTransferTimeoutMs);
  if (ret < 0) {
    LOG(WARNING) << "Get SiS response fail, error: " << ret;
    return -1;
  }
  if (transfer_size != max_response_size) {
    LOG(WARNING) << "Receive SiS command incomplete, expect "
                 << max_response_size << " bytes, acutal " << transfer_size
                 << " bytes.";
    return -1;
  }

  return 0;
}

bool SiSMonitor::CheckPingResponse(unsigned char *response, int size) {
  if (size != kPingResponseSize) {
    LOG(WARNING) << "SiS didn't respond with expected size of data to ping.";
    return false;
  }
  int array_size = kExpectedResponse.size();
  for (int i = 0; i < array_size; ++i) {
    if (response[kBitsToCheck[i]] != kExpectedResponse[i]) {
      LOG(WARNING) << "SiS didn't respond with expected data to ping.";
      return false;
    }
  }
  return true;
}

int SiSMonitor::ResetSiS(libusb_device *device, int interface) {
  int ret = false, panel_ret = false;
  libusb_device_handle *dev_handle_ = NULL;

  if (touch_version_) {
    if (libusb_open(device, &dev_handle_) < 0) {
      // There was an error opening the handle. Device is not there.
      LOG(WARNING) << "Failed to open device.";
      return -1;
    }

    ScopedUsbDevHandle dev_handle(dev_handle_);

    // Send SiS specific cmd to reset it.
    panel_ret = SendResetCmd(dev_handle.get(), interface);

    // let everything re-attach and release back to the kernel driver
    ret = libusb_release_interface(dev_handle.get(), interface);
    if ((ret = libusb_attach_kernel_driver(dev_handle.get(), interface))) {
      LOG(ERROR) << "Failed to attach kernel driver.";
      return -1;
    }
  } else {
    VLOG(1) << "Reseting touch via libusb_reset_device.";
    ResetDevice(libusb_get_parent(device));
  }
  return panel_ret;
}

int SiSMonitor::SendResetCmd(libusb_device_handle *dev_handle, int interface) {
  int ret = 0;
  int panel_ret = 0;
  unsigned char dumbuf[kMaxCmdLen] = {0};

  // If there is a driver attached, detach it so that we can send the request.
  if (libusb_kernel_driver_active(dev_handle, interface)) {
    ret = libusb_detach_kernel_driver(dev_handle, interface);
    if (ret) {
      LOG(WARNING) << "Failed to detach kernal driver, error: " << ret;
      return -1;
    }
  }

  // Set up communication channel.
  ret = libusb_set_configuration(dev_handle, 1);
  if (ret < 0) {
    LOG(WARNING) << "Failed to set configuration, error: " << ret;
    return -1;
  }

  ret = libusb_claim_interface(dev_handle, interface);
  if (ret < 0) {
    LOG(WARNING) << "Failed to claim interface, error: " << ret;
    return -1;
  }

  // Send reset commands.
  if (touch_version_ == SiS9255) {
    ret = SendSiSCommand(dev_handle, kCmdDisableReport,
                         sizeof(kCmdDisableReport), dumbuf, kMaxCmdLen);
    if (ret < 0) return -1;
  }
  ret =
      SendSiSCommand(dev_handle, kCmdPwr, sizeof(kCmdPwr), dumbuf, kMaxCmdLen);
  if (ret < 0) return -1;
  ret = SendSiSCommand(dev_handle, kCmdDiag, sizeof(kCmdDiag), dumbuf,
                       kMaxCmdLen);
  if (ret < 0) return -1;
  ret = SendSiSCommand(dev_handle, kCmdReset, sizeof(kCmdReset), dumbuf,
                       kMaxCmdLen);
  if (ret < 0) return -1;

  int response_size = dumbuf[1];
  panel_ret = CheckResetResponse(dumbuf, response_size);

  if (touch_version_ == SiS9255) {
    SendSiSCommand(dev_handle, kCmdEnableReport, sizeof(kCmdEnableReport),
                   dumbuf, 64);
    if (ret < 0) return -1;
  }

  return panel_ret;
}

bool SiSMonitor::CheckResetResponse(unsigned char *response, int size) {
  if (size != kResetResponseSize) {
    LOG(WARNING) << "SiS didn't respond with expected size of data to reset.";
    return false;
  }
  int array_size = sizeof(kBitsToCheck) / sizeof(kBitsToCheck[0]);
  for (int i = 0; i < array_size; ++i) {
    if (response[kBitsToCheck[i]] != kExpectedResponse[i]) {
      LOG(WARNING) << "SiS didn't respond with expected data to reset.";
      return false;
    }
  }
  return true;
}

void SiSMonitor::CheckSiSHealth(libusb_device *device) {
  int ret = 0;
  std::string info;

  if (UseDeepPing(touch_version_)) {
    // Reads from internal register of SiS panel. Only works on SiS panels.
    if (PingSiS(device, 0)) {
      touch_status_ = true;
      return;
    }
  } else {
    // Read Manufacturer from the touch panel under the assumption that the path
    // is alive.
    ret = ReadID(device, mfg_port_, &info);

    if (ret < 0) {
      // In this case, there was no response from the panel at all.
      LOG(WARNING) << "Read MFG info fail, error: " << ret;
      touch_status_ = false;
      return;
    }

    if (manufacturer_ != info) {
      // The manufacturer does not match the last time it was read.
      LOG(WARNING) << "MFG info mismatch. Orignial MFG: "
                   << manufacturer_.c_str() << "New: MFG: " << info.c_str();
      touch_status_ = false;
      return;
    }
    touch_status_ = true;
  }
}

bool SiSMonitor::TouchStatus() { return touch_status_; }

}  // namespace mimo_monitor
