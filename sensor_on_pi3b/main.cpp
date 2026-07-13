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
#include <filesystem>
#include <iostream>
#include <thread>

// Raspberry Pi 3B+ defaults. CS is driven manually over gpiod since the
// sensor is wired to a GPIO line rather than the SPI controller's own
// hardware chip-select.
#define SPI_DEVICE "/dev/spidev0.0"
#define GPIO_CHIP_DEVICE "/dev/gpiochip0"
#define SPI_SPEED_HZ 2000000U
#define CS_PIN 27

namespace {

   int spi_fd = -1;
   gpiod_chip* gpio_chip = nullptr;
   gpiod_line_request* cs_request = nullptr;

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

} // namespace

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

void user_delay_ms(uint32_t period)
{
   std::this_thread::sleep_for(std::chrono::milliseconds(period));
}

int8_t user_spi_read(uint8_t dev_id, uint8_t reg_addr, uint8_t* reg_data, uint16_t len)
{
   (void)dev_id;

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
   (void)dev_id;

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

void print_sensor_data(struct bme280_data* comp_data)
{
#ifdef BME280_FLOAT_ENABLE
   std::printf("temperature:%0.2f*C   pressure:%0.2fhPa   humidity:%0.2f%%\r\n",
               comp_data->temperature, comp_data->pressure / 100, comp_data->humidity);
#else
   std::printf("temperature:%ld*C   pressure:%ldhPa   humidity:%ld%%\r\n",
               comp_data->temperature, comp_data->pressure / 100, comp_data->humidity);
#endif
}

int8_t stream_sensor_data_forced_mode(struct bme280_dev* dev)
{
   /* Recommended mode of operation: Indoor navigation */
   dev->settings.osr_h = BME280_OVERSAMPLING_1X;
   dev->settings.osr_p = BME280_OVERSAMPLING_16X;
   dev->settings.osr_t = BME280_OVERSAMPLING_2X;
   dev->settings.filter = BME280_FILTER_COEFF_16;

   uint8_t settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;
   int8_t rslt = bme280_set_sensor_settings(settings_sel, dev);

   struct bme280_data comp_data;
   std::printf("Temperature           Pressure             Humidity\r\n");
   /* Continuously stream sensor data */
   while (true) {
      rslt = bme280_set_sensor_mode(BME280_FORCED_MODE, dev);
      /* Wait for the measurement to complete and print data @25Hz */
      dev->delay_ms(40);
      rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, dev);
      print_sensor_data(&comp_data);
   }
   return rslt;
}

int8_t stream_sensor_data_normal_mode(struct bme280_dev* dev)
{
   /* Recommended mode of operation: Indoor navigation */
   dev->settings.osr_h = BME280_OVERSAMPLING_1X;
   dev->settings.osr_p = BME280_OVERSAMPLING_16X;
   dev->settings.osr_t = BME280_OVERSAMPLING_2X;
   dev->settings.filter = BME280_FILTER_COEFF_16;
   dev->settings.standby_time = BME280_STANDBY_TIME_62_5_MS;

   uint8_t settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL |
                           BME280_STANDBY_SEL | BME280_FILTER_SEL;
   int8_t rslt = bme280_set_sensor_settings(settings_sel, dev);
   rslt = bme280_set_sensor_mode(BME280_NORMAL_MODE, dev);

   struct bme280_data comp_data;
   std::printf("Temperature           Pressure             Humidity\r\n");
   while (true) {
      /* Delay while the sensor completes a measurement */
      dev->delay_ms(70);
      rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, dev);
      print_sensor_data(&comp_data);
   }

   return rslt;
}

int main(int argc, char** argv)
{
   std::cout << "\nStarting " << std::filesystem::path(argv[0]).stem().string() << "...\n" << std::endl;

   std::atexit(close_transport);

   // Give the sensor some time to warm up
   const int warmup_time_seconds = 2;
   std::cout << "Warming up the sensor for " << warmup_time_seconds << " seconds..." << std::endl;
   std::this_thread::sleep_for(std::chrono::seconds(warmup_time_seconds));

   if (!initialize_transport()) {
      return EXIT_FAILURE;
   }

   struct bme280_dev dev;
   dev.dev_id = 0;
   dev.intf = BME280_SPI_INTF;
   dev.read = user_spi_read;
   dev.write = user_spi_write;
   dev.delay_ms = user_delay_ms;

   int8_t rslt = bme280_init(&dev);
   std::printf("\r\nBME280 Init Result is: %d\r\n", rslt);
   if (rslt != BME280_OK) {
      return EXIT_FAILURE;
   }

   rslt = stream_sensor_data_normal_mode(&dev);
   // rslt = stream_sensor_data_forced_mode(&dev);

   std::cout << "\nDone\n" << std::endl;
   return rslt == BME280_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
