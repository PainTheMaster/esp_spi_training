#include <stdio.h>
#include <string.h>
#include "driver/spi_common.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/task.h"

#define PIN_SPI_MOSI GPIO_NUM_19
#define PIN_SPI_CLK GPIO_NUM_18
#define PIN_GPIO_CS GPIO_NUM_23
#define PIN_GPIO_DC GPIO_NUM_22
#define PIN_GPIO_RES GPIO_NUM_21
#define PIN_GPIO_LED GPIO_NUM_2

#define LEVEL_COMMAND 0
#define LEVEL_DATA 1

#define OLED_HEIGHT_PX 64
#define OLED_WIDTH_PX 96



esp_err_t sendCommand(uint8_t, spi_device_handle_t *);
esp_err_t sendData(uint8_t, spi_device_handle_t *);


void app_main(void)
{

    esp_err_t err;
    //GPIO
    gpio_set_direction(PIN_GPIO_CS, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_GPIO_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_GPIO_RES, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_GPIO_LED, GPIO_MODE_OUTPUT);


    //SPI
    spi_bus_config_t bus_spi = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4092, //maybe more alloance be appreciated
    };

    spi_bus_initialize(SPI3_HOST, &bus_spi, SPI_DMA_CH_AUTO);


    spi_device_interface_config_t device_oled = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 3,
        .clock_source = SPI_CLK_SRC_DEFAULT,
        .clock_speed_hz = 5000000,//5MHz, SSD1661 max spi freq = 7.6 MHz
        .spics_io_num = PIN_GPIO_CS,
        .queue_size = OLED_HEIGHT_PX * OLED_WIDTH_PX,
    };

    spi_device_handle_t handle_spi_oled;
    spi_bus_add_device(SPI3_HOST, &device_oled, &handle_spi_oled);


    //Display
    gpio_set_level(PIN_GPIO_RES, 1);
    gpio_set_level(PIN_GPIO_RES, 0);
    vTaskDelay(20/portTICK_PERIOD_MS);
    gpio_set_level(PIN_GPIO_RES, 1);



    sendCommand((uint8_t)0xAE, &handle_spi_oled);

    sendCommand((uint8_t)0xA0, &handle_spi_oled); //driver remap and color depth
        sendCommand((uint8_t)0b00110010, &handle_spi_oled);
    sendCommand((uint8_t)0xAD, &handle_spi_oled);
        sendCommand((uint8_t)0b10001110, &handle_spi_oled);


    uint8_t pix[OLED_HEIGHT_PX][OLED_WIDTH_PX];
    const uint8_t red = (uint8_t)0b111 << 5;
    const uint8_t green = (uint8_t)0b111 << 2;
    const uint8_t blue = (uint8_t)0b11;
    const uint8_t white = red | green | blue;
    const uint8_t black = 0;
    const uint8_t rgb[3] = {red, green, blue};


    sendCommand((uint8_t)0xAF, &handle_spi_oled);
    vTaskDelay(110/portTICK_PERIOD_MS);


    uint8_t x, y, dimension;
    uint8_t touch;
    int8_t vx, vy;

    x=10;
    y=10;
    vx=1;
    vy=1;
    dimension = 10;
    uint8_t bkg = white;
    uint8_t obj;
    uint8_t led = 0;
    touch = 0;
    while(1){
        obj = rgb[touch%3]; //| rgb[(touch+1)%3];
        x += vx;
        y += vy;
        int8_t row, col;

        for(row = 0; row <= OLED_HEIGHT_PX-1; row++){
            for(col = 0; col <= OLED_WIDTH_PX-1; col++){
                if( (x <= col) && (col <= x+dimension-1)) {
                    if((y <= row) && (row <= y+dimension-1)) {
                        sendData(obj, &handle_spi_oled);
                    } else {
                        sendData(bkg, &handle_spi_oled);
                    }
                } else {
                    sendData(bkg, &handle_spi_oled);
                }
            }
        }


        if(x == 0 || x == OLED_WIDTH_PX-dimension){
            vx *= ((int8_t)-1);
            touch++;
            gpio_set_level(PIN_GPIO_LED, (led ^= 1));
        }
        if(y == 0 || y == OLED_HEIGHT_PX-dimension){
            vy *= ((int8_t)-1);
            touch++;
            gpio_set_level(PIN_GPIO_LED, (led ^= 1));
        }

    }


    while (1){
        gpio_set_level(PIN_GPIO_LED, 1);
        vTaskDelay(1000/portTICK_PERIOD_MS);
        gpio_set_level(PIN_GPIO_LED, 0);
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }


}

esp_err_t sendCommand(uint8_t command, spi_device_handle_t *handle_spi){

    esp_err_t err;
    gpio_set_level(PIN_GPIO_DC, LEVEL_COMMAND);

    spi_transaction_t transaction; 
    memset(&transaction, 0, sizeof(transaction));
    transaction.length = sizeof(command)*8,
    transaction.tx_buffer = &command,

    err = spi_device_transmit(*handle_spi, &transaction);
    return err;

}

esp_err_t sendData(uint8_t data, spi_device_handle_t *handle_spi){

    esp_err_t err;
    gpio_set_level(PIN_GPIO_DC, LEVEL_DATA);

    spi_transaction_t transaction = {
        .length = sizeof(data)*8,
        .tx_buffer = &data,
    };

    err = spi_device_transmit(*handle_spi, &transaction);
    return err;

}