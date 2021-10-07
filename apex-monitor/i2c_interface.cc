// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/apex-monitor/i2c_interface.h"

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>

#include <memory>
#include <string>

namespace {

const uint8_t kByteDataSize = 1;

const base::FilePath::StringType kI2cBusPath = "/dev/i2c-";
const uint8_t kSetPageNumRegAddr = 0xfe;

}  // namespace

namespace apex_monitor {

I2cInterface::I2cInterface() : ioexp_lock_() {}

I2cInterface::~I2cInterface() {}

std::unique_ptr<I2cInterface> I2cInterface::Create(const int& adapter_id,
                                                   const uint8_t& slave_addr) {
  auto interface = std::make_unique<I2cInterface>();
  if (!interface->I2cInit(adapter_id, slave_addr)) {
    return nullptr;
  }
  return interface;
}

bool I2cInterface::I2cInit(const int& adapter_id, const uint8_t& slave_addr) {
  base::FilePath adapter_path(kI2cBusPath + std::to_string(adapter_id));
  adapter_fd_.reset(open(adapter_path.value().c_str(), O_RDWR | O_CLOEXEC));
  if (!adapter_fd_.is_valid()) {
    PLOG(ERROR) << "Failed to open I2C adapter " << adapter_id;
    return false;
  }

  slave_addr_ = slave_addr;

  current_page_ = 0xff;

  return true;
}

bool I2cInterface::I2cWriteAddr(const uint8_t& addr, const unsigned char* buf,
                                const uint16_t& write_size) {
  // Append the register address to the head of the write buf.
  uint16_t write_buf_size = write_size + kByteDataSize;
  auto write_buf = std::make_unique<unsigned char[]>(write_buf_size);
  memcpy(write_buf.get(), &addr, kByteDataSize);
  memcpy(write_buf.get() + kByteDataSize, buf, write_size);

  VLOG(1) << "Write ";
  for (int i = 0; i < write_buf_size; ++i)
    VLOG(1) << base::StringPrintf("0x%x", write_buf.get()[i]);
  VLOG(1) << " to " << base::StringPrintf("0x%x", slave_addr_);

  struct i2c_msg msg = {
      .addr = slave_addr_,
      .flags = 0,
      .len = write_buf_size,
      .buf = write_buf.get(),
  };

  struct i2c_rdwr_ioctl_data write_data = {
      .msgs = &msg,
      .nmsgs = 1,
  };

  int r = ioctl(adapter_fd_.get(), I2C_RDWR, &write_data);
  if (r < 0) {
    PLOG(ERROR) << "Failed to write data";
    return false;
  }

  return true;
}

bool I2cInterface::I2cReadAddr(const uint8_t& addr, unsigned char* buf,
                               const uint16_t& read_size) {
  auto read_addr = std::make_unique<unsigned char>(kByteDataSize);
  memcpy(read_addr.get(), &addr, kByteDataSize);

  struct i2c_msg msgs[] = {{
                               .addr = slave_addr_,
                               .flags = 0,
                               .len = kByteDataSize,
                               .buf = read_addr.get(),
                           },
                           {
                               .addr = slave_addr_,
                               .flags = I2C_M_RD,
                               .len = read_size,
                               .buf = buf,
                           }};

  struct i2c_rdwr_ioctl_data read_data = {
      .msgs = msgs,
      .nmsgs = 2,
  };

  int r = ioctl(adapter_fd_.get(), I2C_RDWR, &read_data);
  if (r < 0) {
    PLOG(ERROR) << "Failed to write data";
    return false;
  }

  return true;
}

bool I2cInterface::I2cWriteReg(const uint8_t& reg_addr,
                               const unsigned char* buf,
                               const uint16_t& write_size,
                               const uint8_t& page_number) {
  ioexp_lock_.Acquire();
  if (page_number != kPageNumberNone && current_page_ != page_number) {
    if (!I2cWriteAddr(kSetPageNumRegAddr, &page_number, 1)) {
      LOG(ERROR) << "Failed to set page number " << +page_number;
      ioexp_lock_.Release();
      return false;
    }
    current_page_ = page_number;
  }

  if (!I2cWriteAddr(reg_addr, buf, write_size)) {
    LOG(ERROR) << "Failed to write data to register " << +reg_addr;
    ioexp_lock_.Release();
    return false;
  }

  ioexp_lock_.Release();
  return true;
}

bool I2cInterface::I2cReadReg(const uint8_t& reg_addr, unsigned char* buf,
                              const uint16_t& read_size,
                              const uint8_t& page_number) {
  if (page_number != kPageNumberNone && current_page_ != page_number) {
    if (!I2cWriteAddr(kSetPageNumRegAddr, &page_number, 1)) {
      LOG(ERROR) << "Failed to set page number " << +page_number;
      return false;
    }
    current_page_ = page_number;
  }

  if (!I2cReadAddr(reg_addr, buf, read_size)) {
    LOG(ERROR) << "Failed to read data at register " << +reg_addr;
    return false;
  }

  return true;
}

bool I2cInterface::SetBit(const uint8_t& reg_addr, const uint8_t& bit_id,
                          const uint8_t& page_number) {
  uint8_t reg_value;
  uint8_t reg_mask = (1 << bit_id);

  // Read current register value.
  if (!I2cReadReg(reg_addr, &reg_value, kByteDataSize, page_number)) {
    LOG(ERROR) << "Failed to read current register value.";
    return false;
  }

  VLOG(1) << "Current register value: "
          << base::StringPrintf("0x%02x", reg_value);

  // If the bit is already set, not need to set it again.
  if (reg_value & reg_mask) {
    VLOG(1) << "Bit value is already set.";
    return true;
  }

  // Set the register to target value.
  reg_value |= reg_mask;
  if (!I2cWriteReg(reg_addr, &reg_value, kByteDataSize, page_number)) {
    return false;
  }

  // Check if the value is set as expected.
  if (!I2cReadReg(reg_addr, &reg_value, kByteDataSize, page_number)) {
    return false;
  }

  if (!(reg_value & reg_mask)) {
    LOG(ERROR) << "Set bit " << +bit_id << " in reg "
               << base::StringPrintf("0x%02x", reg_addr) << " failed.";
    return false;
  }

  return true;
}

bool I2cInterface::ClearBit(const uint8_t& reg_addr, const uint8_t& bit_id,
                            const uint8_t& page_number) {
  uint8_t reg_value;
  uint8_t reg_mask = ~(1 << bit_id);

  // Read current register value.
  if (!I2cReadReg(reg_addr, &reg_value, kByteDataSize, page_number)) {
    LOG(ERROR) << "Failed to read current register value.";
    return false;
  }

  VLOG(1) << "Current register value: "
          << base::StringPrintf("0x%02x", reg_value);

  // If the bit is already 0, not need to clear it.
  if (!(reg_value & (~reg_mask))) {
    VLOG(1) << "Bit value is already 0.";
    return true;
  }

  // Set the register to target value.
  reg_value &= reg_mask;
  if (!I2cWriteReg(reg_addr, &reg_value, kByteDataSize, page_number)) {
    return false;
  }

  // Check if the value is set as expected.
  if (!I2cReadReg(reg_addr, &reg_value, kByteDataSize, page_number)) {
    return false;
  }

  if (reg_value & (~reg_mask)) {
    LOG(ERROR) << "Clear bit " << +bit_id << " in reg "
               << base::StringPrintf("0x%02x", reg_addr) << " failed.";
    return false;
  }

  return true;
}

}  // namespace apex_monitor
