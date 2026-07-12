#include <gpiod.h>
#include <linux/spi/spidev.h>

namespace sensor_utilities 
{
    bool configure_spi_device();

    bool configure_chip_select();
    
    bool initialize_transport();

    void close_transport();

    void user_delay_ms(uint32_t period);

    int8_t user_spi_read(uint8_t dev_id, uint8_t reg_addr, uint8_t* reg_data, uint16_t len);

    int8_t user_spi_write(uint8_t dev_id, uint8_t reg_addr, uint8_t* reg_data, uint16_t len);

} // namespace sensor_utilities
