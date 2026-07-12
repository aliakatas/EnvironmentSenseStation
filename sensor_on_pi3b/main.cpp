#include <bme280.h>

#include <fcntl.h>
#include <gpiod.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <filesystem>

// Raspberry Pi 3B+ defaults.
#define channel 0
#define SPI_DEVICE "/dev/spidev0.0"
#define GPIO_CHIP_DEVICE "/dev/gpiochip0"
#define SPI_SPEED_HZ 2000000U

// Default write it to the register in one time
#define USESPISINGLEREADWRITE 0

// This definition you use I2C or SPI to drive the bme280
// When it is 1 means use I2C interface, When it is 0,use SPI interface
#define USEIIC 0

#define CS_PIN 27

namespace {

int spi_fd = -1;
gpiod_chip* gpio_chip = nullptr;
gpiod_line* cs_line = nullptr;

void close_transport()
{
   if (cs_line != nullptr) {
      gpiod_line_release(cs_line);
      cs_line = nullptr;
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

   cs_line = gpiod_chip_get_line(gpio_chip, CS_PIN);
   if (cs_line == nullptr) {
      std::cerr << "Failed to request GPIO line " << CS_PIN << ": " << std::strerror(errno) << '\n';
      close_transport();
      return false;
   }

   if (gpiod_line_request_output(cs_line, "bme280-spi-cs", 1) < 0) {
      std::cerr << "Failed to configure GPIO line " << CS_PIN << " as output: " << std::strerror(errno) << '\n';
      close_transport();
      return false;
   }

   return true;
}

bool initialize_transport()
{
   if (!configure_spi_device()) {
      return false;
   }

   return configure_chip_select();
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
   if (cs_line != nullptr) {
      gpiod_line_set_value(cs_line, 1);
   }
}

void SPI_BME280_CS_Low(void)
{
   if (cs_line != nullptr) {
      gpiod_line_set_value(cs_line, 0);
   }
}

void user_delay_ms(uint32_t period)
{
   std::this_thread::sleep_for(std::chrono::milliseconds(period));
}

int8_t user_spi_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
   int8_t rslt = BME280_OK;

   (void)dev_id;

   if (spi_fd < 0 || cs_line == nullptr) {
      return BME280_E_COMM_FAIL;
   }

   SPI_BME280_CS_Low();

   rslt = spi_transfer(&reg_addr, nullptr, 1);
   if (rslt == BME280_OK) {
#if(USESPISINGLEREADWRITE)
      for (uint16_t index = 0; index < len && rslt == BME280_OK; index++) {
         rslt = spi_transfer(nullptr, reg_data + index, 1);
      }
#else
      rslt = spi_transfer(nullptr, reg_data, len);
#endif
   }

   SPI_BME280_CS_High();

   return rslt;
}

int8_t user_spi_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
   int8_t rslt = BME280_OK;

   (void)dev_id;

   if (spi_fd < 0 || cs_line == nullptr) {
      return BME280_E_COMM_FAIL;
   }

   SPI_BME280_CS_Low();

   rslt = spi_transfer(&reg_addr, nullptr, 1);
   if (rslt == BME280_OK) {
#if(USESPISINGLEREADWRITE)
      for (uint16_t index = 0; index < len && rslt == BME280_OK; index++) {
         rslt = spi_transfer(reg_data + index, nullptr, 1);
      }
#else
      rslt = spi_transfer(reg_data, nullptr, len);
#endif
   }

   SPI_BME280_CS_High();

   return rslt;
}

void print_sensor_data(struct bme280_data *comp_data)
{
#ifdef BME280_FLOAT_ENABLE
   printf("temperature:%0.2f*C   pressure:%0.2fhPa   humidity:%0.2f%%\r\n",comp_data->temperature, comp_data->pressure/100, comp_data->humidity);
#else
   printf("temperature:%ld*C   pressure:%ldhPa   humidity:%ld%%\r\n",comp_data->temperature, comp_data->pressure/100, comp_data->humidity);
#endif
}

int8_t stream_sensor_data_forced_mode(struct bme280_dev *dev)
{
    int8_t rslt;
    uint8_t settings_sel;
    struct bme280_data comp_data;

    /* Recommended mode of operation: Indoor navigation */
    dev->settings.osr_h = BME280_OVERSAMPLING_1X;
    dev->settings.osr_p = BME280_OVERSAMPLING_16X;
    dev->settings.osr_t = BME280_OVERSAMPLING_2X;
    dev->settings.filter = BME280_FILTER_COEFF_16;

    settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;

    rslt = bme280_set_sensor_settings(settings_sel, dev);

    printf("Temperature           Pressure             Humidity\r\n");
    /* Continuously stream sensor data */
    while (1) {
        rslt = bme280_set_sensor_mode(BME280_FORCED_MODE, dev);
        /* Wait for the measurement to complete and print data @25Hz */
        dev->delay_ms(40);
        rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, dev);
        print_sensor_data(&comp_data);
    }
    return rslt;
}


int8_t stream_sensor_data_normal_mode(struct bme280_dev *dev)
{
   int8_t rslt;
   uint8_t settings_sel;
   struct bme280_data comp_data;

   /* Recommended mode of operation: Indoor navigation */
   dev->settings.osr_h = BME280_OVERSAMPLING_1X;
   dev->settings.osr_p = BME280_OVERSAMPLING_16X;
   dev->settings.osr_t = BME280_OVERSAMPLING_2X;
   dev->settings.filter = BME280_FILTER_COEFF_16;
   dev->settings.standby_time = BME280_STANDBY_TIME_62_5_MS;

   settings_sel = BME280_OSR_PRESS_SEL;
   settings_sel |= BME280_OSR_TEMP_SEL;
   settings_sel |= BME280_OSR_HUM_SEL;
   settings_sel |= BME280_STANDBY_SEL;
   settings_sel |= BME280_FILTER_SEL;
   rslt = bme280_set_sensor_settings(settings_sel, dev);
   rslt = bme280_set_sensor_mode(BME280_NORMAL_MODE, dev);

   printf("Temperature           Pressure             Humidity\r\n");
   while (1) {
      /* Delay while the sensor completes a measurement */
      dev->delay_ms(70);
      rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, dev);
      print_sensor_data(&comp_data);
   }

   return rslt;
}

int main(int argc, char** argv)
{
    std::cout << " \nStarting " << std::filesystem::path(argv[0]).stem().string() << "...\n" << std::endl;

    std::atexit(close_transport);

    // Give the sensor some time to warm up
    const int warmup_time_seconds = 2;
    std::cout << " Warming up the sensor for " << warmup_time_seconds << " seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(warmup_time_seconds));

    if (!initialize_transport()) 
        return EXIT_FAILURE;

    struct bme280_dev dev;
    int8_t rslt = BME280_OK;

    dev.dev_id = 0;
    dev.intf = BME280_SPI_INTF;
    dev.read = user_spi_read;
    dev.write = user_spi_write;
    dev.delay_ms = user_delay_ms;

    rslt = bme280_init(&dev);
    printf("\r\n BME280 Init Result is:%d \r\n",rslt);
    if (rslt != BME280_OK) {
        return EXIT_FAILURE;
    }

    //stream_sensor_data_forced_mode(&dev);
    rslt = stream_sensor_data_normal_mode(&dev);

    std::cout << " \nDone \n" << std::endl;
    return rslt == BME280_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
