#include "sensor_utilities.h"

#include "bme280.h"

#include <fcntl.h>
#include <gpiod.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

#define SPI_DEVICE "/dev/spidev0.0"
#define GPIO_CHIP_DEVICE "/dev/gpiochip0"
#define SPI_SPEED_HZ 2000000U
#define CS_PIN 27

namespace sensor_utilities 
{

    int spi_fd = -1;
    gpiod_chip* gpio_chip = nullptr;
    gpiod_line_request* cs_request = nullptr;

    void SPI_BME280_CS_High(void)
    {
        if (chip_select_ready()) {
            (void)set_chip_select(true);
        }
    }

    void SPI_BME280_CS_Low(void)
    {
        if (chip_select_ready()) {
            (void)set_chip_select(false);
        }
    }

    bool chip_select_ready()
   {
      return cs_request != nullptr;
   }

   bool set_chip_select(bool high)
   {
      if (cs_request == nullptr) {
         return false;
      }

      const auto value = high ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
      if (gpiod_line_request_set_value(cs_request, CS_PIN, value) < 0) {
         std::cerr << "Failed to set GPIO line " << CS_PIN << " value: " << std::strerror(errno) << '\n';
         return false;
      }

      return true;
   }

    bool configure_spi_device()
    {
        spi_fd = open(SPI_DEVICE, O_RDWR);
        if (spi_fd < 0) {
            std::cerr << "Failed to open " << SPI_DEVICE << ": " << std::strerror(errno) << '\n';
            return false;
        }

        uint8_t mode = SPI_MODE_0 | SPI_NO_CS;
        uint8_t bits_per_word = 8;
        uint32_t speed_hz = SPI_SPEED_HZ;

        if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
            ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word) < 0 ||
            ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0) {
            std::cerr << "Failed to configure " << SPI_DEVICE << ": " << std::strerror(errno) << '\n';
            close_transport();
            return false;
        }

        return true;
    }

    bool configure_chip_select()
    {
        gpio_chip = gpiod_chip_open(GPIO_CHIP_DEVICE);
        if (gpio_chip == nullptr) {
            std::cerr << "Failed to open " << GPIO_CHIP_DEVICE << ": " << std::strerror(errno) << '\n';
            return false;
        }

        gpiod_line_settings* line_settings = gpiod_line_settings_new();
        gpiod_line_config* line_config = gpiod_line_config_new();
        gpiod_request_config* request_config = gpiod_request_config_new();

        if (line_settings == nullptr || line_config == nullptr || request_config == nullptr) {
            std::cerr << "Failed to allocate GPIO line request configuration\n";
            if (request_config != nullptr) gpiod_request_config_free(request_config);
            if (line_config != nullptr) gpiod_line_config_free(line_config);
            if (line_settings != nullptr) gpiod_line_settings_free(line_settings);
            close_transport();
            return false;
        }

        bool configured = false;
        do {
            // CS is active-low; start deasserted (logical ACTIVE == physical high
            // by default, since active_low is not set).
            if (gpiod_line_settings_set_direction(line_settings, GPIOD_LINE_DIRECTION_OUTPUT) < 0) break;
            if (gpiod_line_settings_set_output_value(line_settings, GPIOD_LINE_VALUE_ACTIVE) < 0) break;

            unsigned int offsets[] = { CS_PIN };
            if (gpiod_line_config_add_line_settings(line_config, offsets, 1, line_settings) < 0) break;

            gpiod_request_config_set_consumer(request_config, "bme280-spi-cs");
            cs_request = gpiod_chip_request_lines(gpio_chip, request_config, line_config);
            if (cs_request == nullptr) break;

            configured = true;
        } while (false);

        gpiod_request_config_free(request_config);
        gpiod_line_config_free(line_config);
        gpiod_line_settings_free(line_settings);

        if (!configured) {
            std::cerr << "Failed to request GPIO line " << CS_PIN << " for output: " << std::strerror(errno) << '\n';
            close_transport();
            return false;
        }

        return true;
    }

    bool initialize_transport()
    {
        return configure_spi_device() && configure_chip_select();
    }

    void close_transport()
    {
        if (cs_request != nullptr) {
            gpiod_line_request_release(cs_request);
            cs_request = nullptr;
        }

        if (gpio_chip != nullptr) {
            gpiod_chip_close(gpio_chip);
            gpio_chip = nullptr;
        }

        if (spi_fd >= 0) {
            close(spi_fd);
            spi_fd = -1;
        }
    }

    int8_t spi_transfer(const uint8_t* tx_buffer, uint8_t* rx_buffer, uint16_t length)
    {
        struct spi_ioc_transfer transfer = {};
        transfer.tx_buf = reinterpret_cast<unsigned long>(tx_buffer);
        transfer.rx_buf = reinterpret_cast<unsigned long>(rx_buffer);
        transfer.len = length;
        transfer.speed_hz = SPI_SPEED_HZ;
        transfer.bits_per_word = 8;

        if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
            std::cerr << "SPI transfer failed: " << std::strerror(errno) << '\n';
            return BME280_E_COMM_FAIL;
        }

        return BME280_OK;
    }

    void user_delay_ms(uint32_t period)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(period));
    }

    int8_t user_spi_read(uint8_t dev_id, uint8_t reg_addr, uint8_t* reg_data, uint16_t len)
    {
        if (spi_fd < 0 || !chip_select_ready()) {
            return BME280_E_COMM_FAIL;
        }

        SPI_BME280_CS_Low();

        int8_t rslt = spi_transfer(&reg_addr, nullptr, 1);
        if (rslt == BME280_OK) {
            rslt = spi_transfer(nullptr, reg_data, len);
        }

        SPI_BME280_CS_High();

        return rslt;
    }

    int8_t user_spi_write(uint8_t dev_id, uint8_t reg_addr, uint8_t* reg_data, uint16_t len)
    {
        if (spi_fd < 0 || !chip_select_ready()) {
            return BME280_E_COMM_FAIL;
        }

        SPI_BME280_CS_Low();

        int8_t rslt = spi_transfer(&reg_addr, nullptr, 1);
        if (rslt == BME280_OK) {
            rslt = spi_transfer(reg_data, nullptr, len);
        }

        SPI_BME280_CS_High();

        return rslt;
    }

} // namespace sensor_utilities
