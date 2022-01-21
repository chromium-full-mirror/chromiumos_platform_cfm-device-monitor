// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APEX_MONITOR_I2C_INTERFACE_H_
#define APEX_MONITOR_I2C_INTERFACE_H_

#include <base/files/scoped_file.h>
#include <base/synchronization/lock.h>

#include <memory>

// Use 0xff to indicate no need to set page number.
const uint8_t kPageNumberNone = 0xff;

namespace apex_monitor {

class BaseI2cInterface {
 public:
  virtual ~BaseI2cInterface() = default;

  virtual bool I2cWriteReg(const uint8_t& reg_addr, const unsigned char* buf,
                           const uint16_t& write_size,
                           const uint8_t& page_number = kPageNumberNone) = 0;
  virtual bool I2cReadReg(const uint8_t& reg_addr, unsigned char* buf,
                          const uint16_t& read_size,
                          const uint8_t& page_number = kPageNumberNone) = 0;
  virtual bool SetBit(const uint8_t& reg_addr, const uint8_t& bit_id,
                      const uint8_t& page_number = kPageNumberNone) = 0;
  virtual bool ClearBit(const uint8_t& reg_addr, const uint8_t& bit_id,
                        const uint8_t& page_number = kPageNumberNone) = 0;
};

class I2cInterface : public BaseI2cInterface {
 public:
  I2cInterface();
  I2cInterface(const I2cInterface&) = delete;
  I2cInterface& operator=(const I2cInterface&) = delete;

  ~I2cInterface();

  static std::unique_ptr<I2cInterface> Create(const int& adapter_id,
                                              const uint8_t& slave_addr);

  bool I2cInit(const int& adapter_id, const uint8_t& slave_addr);
  bool I2cWriteReg(const uint8_t& reg_addr, const unsigned char* buf,
                   const uint16_t& write_size,
                   const uint8_t& page_number = kPageNumberNone) override;
  bool I2cReadReg(const uint8_t& reg_addr, unsigned char* buf,
                  const uint16_t& read_size,
                  const uint8_t& page_number = kPageNumberNone) override;
  bool SetBit(const uint8_t& reg_addr, const uint8_t& bit_id,
              const uint8_t& page_number = kPageNumberNone) override;
  bool ClearBit(const uint8_t& reg_addr, const uint8_t& bit_id,
                const uint8_t& page_number = kPageNumberNone) override;

 private:
  bool I2cWriteAddr(const uint8_t& addr, const unsigned char* buf,
                    const uint16_t& write_size);
  bool I2cReadAddr(const uint8_t& addr, unsigned char* buf,
                   const uint16_t& write_size);

  // Because IO expander is shared among both chips, we use a lock
  // to protect write operation.
  base::Lock ioexp_lock_;
  base::ScopedFD adapter_fd_;
  uint8_t current_page_;
  uint8_t slave_addr_;
};

}  // namespace apex_monitor

#endif  // APEX_MONITOR_I2C_INTERFACE_H_
