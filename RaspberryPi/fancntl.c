#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <bcm2835.h>

//constant defines
#define MESSAGE_SIZE 64
#define UPDATE_PERIOD 2 //update period in seconds

//Function prototypes
int32_t get_temperature(void); //This fucntion pipes in the output from vcgencmd measure_temp and parses it to get temperature data

/* create_pwm_message
 * Takes a 32 bit integer representing a 10 bit precision fixed point 
 * temperature and converts it into the message format described below
 * for transmission to the microcontroller.
 * Message Format: Byte 0 -> Start Byte = 0xBE
 *                Byte 1 -> Command
 *                Byte 2 -> Amount of Data Bytes to Follow
 *                Bytes 3-n -> Data associated with the command
 * INPUT: 32 bit temperature, byte array for message (assumed to be at least 7 bytes)
 */
void create_pwm_message(int32_t integer, char *array);


int main(int argc,char **argv){
   int32_t temperature = 50;
   int32_t fan_speed = 90;
   char message[7];
   uint8_t temp=0;
   //setup the SPI connexion 
   if(bcm2835_init() == 0){
      exit(1); //failed to initialize gpio
      puts("Failed to setup GPIO; probably cause program not run as root!");
   }
   if(bcm2835_spi_begin() == 0){
      puts("Failed to setup SPI; probably cause program not run as root!");
      exit(2); //failed to setup spi 
   }
   bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
   bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); //SPO =0 SPH = 0?
   bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_512); //clocked at 781 kHz
   bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
   bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW); //active low CS

   //measure temperature and send fan speed as 1024 fixed point number 
   //uC will handle intial value for PWM purposes
   while(1){
      sleep(UPDATE_PERIOD);    
      temperature = get_temperature();
      //Fan uses linear speed function TODO: use configureable lookup table
      fan_speed = 1536 * temperature - 20480;
      printf("Temperature: %d\tFan speed: %d\%\n", temperature, (fan_speed >> 10)); //TODO: Replace with SPI communication
      //SPI communication
      create_pwm_message(fan_speed, message);
      //Due to differences in SPI between PI and TM4c only single byte messages work correctly
      temp = bcm2835_spi_transfer(message[0]);
      temp = bcm2835_spi_transfer(message[1]);
      temp = bcm2835_spi_transfer(message[2]);
      temp = bcm2835_spi_transfer(message[3]);
      temp = bcm2835_spi_transfer(message[4]);
      temp = bcm2835_spi_transfer(message[5]);
      temp = bcm2835_spi_transfer(message[6]);
   }
   bcm2835_spi_end();
   bcm2835_close();
   return 0;

}

void create_pwm_message(int32_t integer, char *array){
   array[0] = 0xBE; //start byte
   array[1] = 0x1; //command number 1
   array[2] = 0x4; //4 bytes to follow
   array[3] = (char) (integer & 0xFF); //LSB first
   array[4] = (char) ((integer >> 8) & 0xFF);
   array[5] = (char) ((integer >> 16) & 0xFF);
   array[6] = (char) ((integer >> 24) & 0xFF);
 
   //debug code to ensure working correctly
   printf("Integer: %x\t LSB: %x\t BT1: %x\t BT2: %x\t MSB: %x\n", integer, array[3], array[4], array[5], array[6]);
}

int32_t get_temperature(void){
    //TODO: Add error handling to this function
    FILE *pf;
    char command[30];
    char data[MESSAGE_SIZE];
    char temp[5];
    int32_t temperature = 0;

    //enter command into the string command
    sprintf(command, "vcgencmd measure_temp"); 

    //Setup our pipe for reading and execute our command.
    pf = popen(command,"r"); 

    // Get the data from the process execution
    fgets(data, MESSAGE_SIZE-1 , pf);
    data[MESSAGE_SIZE-1] = 0; //null terminate it
    

    //We know message format so extract temperature from message
    for(int i =0; i < 4; i++){
       temp[i] = data[i+5]; //5 character offset b/c 'temp='
    } 
    temp[4] = 0; //null terminate
    
    temperature = (int32_t)atof(temp); //convert temp to integer value
    
    //print out the results
    //printf("%s\n Temperature is rounded to %d\n", data, temperature);

    if (pclose(pf) != 0)
        fprintf(stderr," Error: Failed to close command stream \n");

    return temperature;
}
