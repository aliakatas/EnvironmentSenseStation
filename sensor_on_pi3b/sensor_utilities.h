#include <gpiod.h>
#include <linux/spi/spidev.h>

namespace sensor_utilities 
{
    void SPI_BME280_CS_High(void);

    void SPI_BME280_CS_Low(void);

    bool chip_select_ready();

    bool set_chip_select(bool high);
    
    bool configure_spi_device();

    bool configure_chip_select();
    
    bool initialize_transport();

    void close_transport();

    void user_delay_ms(uint32_t period);

    int8_t user_spi_read(uint8_t dev_id, uint8_t reg_addr, uint8_t* reg_data, uint16_t len);

    int8_t user_spi_write(uint8_t dev_id, uint8_t reg_addr, uint8_t* reg_data, uint16_t len);

} // namespace sensor_utilities
