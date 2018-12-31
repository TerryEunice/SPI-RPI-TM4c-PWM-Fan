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

void int32_to_chararray(int32_t integer, char *array); //assumes the array is at least 4 in size


int main(int argc,char **argv){
   int32_t temperature = 50;
   int32_t fan_speed = 90;
   char split_speed[4];
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
      int32_to_chararray(fan_speed, split_speed);
      bcm2835_spi_writenb(split_speed, 4); //send 4 bytes over SPI 
   }
   bcm2835_spi_end();
   bcm2835_close();
   return 0;

}

void int32_to_chararray(int32_t integer, char *array){
   array[0] = (char) (integer & 0xFF); //LSB first
   array[1] = (char) ((integer >> 8) & 0xFF);
   array[2] = (char) ((integer >> 16) & 0xFF);
   array[3] = (char) ((integer >> 24) & 0xFF);
 
   //debug code to ensure working correctly
   printf("Integer: %x\t LSB: %x\t BT1: %x\t BT2: %x\t MSB: %x\n", integer, array[0], array[1], array[2], array[3]);
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
