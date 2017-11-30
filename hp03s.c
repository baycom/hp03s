#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <arpa/inet.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#define PIN_MCLK 1
#define PIN_XCLR 4
#define I2C_ADDRESS_EEPROM 0x50
#define I2C_ADDRESS_SENSOR 0x77
#define I2C_REGISTER_PRESSURE 0xf0
#define I2C_REGISTER_TEMP 0xe8

typedef struct {
    unsigned short c1;
    unsigned short c2;
    unsigned short c3;
    unsigned short c4;
    unsigned short c5;
    unsigned short c6;
    unsigned short c7;
    unsigned char a;
    unsigned char b;
    unsigned char c;
    unsigned char d;
} parameters_t;

int debug=0;
int fdEEPROM;
int fdSensor;
parameters_t parms;

void mclk(int enable)
{
  if(enable) {
    pinMode (PIN_MCLK, PWM_OUTPUT);
    pwmSetMode (PWM_MODE_MS);
    pwmSetClock(2);
    pwmSetRange(293);
    pwmWrite(PIN_MCLK, 146) ;
  } else {
    pinMode (PIN_MCLK, OUTPUT);
    digitalWrite(PIN_MCLK, 0);
  }
}

void xclr(int enable) 
{
  pinMode (PIN_XCLR, PWM_OUTPUT);
  if(enable) {
    digitalWrite(PIN_XCLR, 1);
  } else {
    digitalWrite(PIN_XCLR, 0);
  }
}

unsigned char addr_lookup[11] = {0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e, 0x1f, 0x20, 0x21};

int i2c_read_eeprom(parameters_t *p)
{
  for(int i=0; i<11; i++) {
    int val;
    int addr=addr_lookup[i];
    if(i < 7) {
      val=wiringPiI2CReadReg16(fdEEPROM, addr);
      val=ntohs(val);
    } else {
      val=wiringPiI2CReadReg8(fdEEPROM, addr);
    }
    if(val < 0) {
      return 0;
    }
    switch(i) {
      case 0: p->c1 = val; break;
      case 1: p->c2 = val; break;
      case 2: p->c3 = val; break;
      case 3: p->c4 = val; break;
      case 4: p->c5 = val; break;
      case 5: p->c6 = val; break;
      case 6: p->c7 = val; break;
      case 7: p->a = val; break;
      case 8: p->b = val; break;
      case 9: p->c = val; break;
      case 10: p->d = val; break;
    }
    if(debug) {
      fprintf(stderr,"n:0x%02x 0x%02x: 0x%02x\n",i,addr,val);
    }
  }
  return 1;
}

int i2c_read_raw_sensor_value(int address)
{
  wiringPiI2CWriteReg8(fdSensor, 0xff, address);
  delay(40);
  int val=wiringPiI2CReadReg16(fdSensor, 0xfd);

  if(val < 0) {
    return val;
  }
  val=ntohs(val);
  return val;
}

int readHP03s(float *temperature, float *pressure)
{
#if TESTSET
  parms.c1 = 29908;
  parms.c2 = 3724;
  parms.c3 = 312;
  parms.c4 = 441;
  parms.c5 = 9191;
  parms.c6 = 3990;
  parms.c7 = 2500;
  parms.a = 1;
  parms.b = 4;
  parms.c = 4;
  parms.d = 9;
  d1 = 30036;
  d2 = 4107;
#endif

  mclk(1);
  xclr(1);
  int d1=i2c_read_raw_sensor_value(I2C_REGISTER_PRESSURE);  
  int d2=i2c_read_raw_sensor_value(I2C_REGISTER_TEMP);
  mclk(0);
  xclr(0);
  
  if(d1<=0 || d2<=0) {
    return 0;
  }

  if(debug) {
    printf("c1: %d\n", parms.c1);
    printf("c2: %d\n", parms.c2);
    printf("c3: %d\n", parms.c3);
    printf("c4: %d\n", parms.c4);
    printf("c5: %d\n", parms.c5);
    printf("c6: %d\n", parms.c6);
    printf("c7: %d\n", parms.c7);
    printf("a: %d\n", parms.a);
    printf("b: %d\n", parms.b);
    printf("c: %d\n", parms.c);
    printf("d: %d\n", parms.d);
    printf("d1: %d\n",d1);
    printf("d2: %d\n",d2);
  }

  long dUT;
  long diff = d2-parms.c5;
  long tval = pow(diff,2)/16384;

  if(d2 >= parms.c5) {
    dUT = diff - tval * parms.a/pow(2,parms.c);
  } else {
    dUT = diff - tval * parms.b/pow(2,parms.c);
  }

  long off =  (parms.c2 + (parms.c4 - 1024) * dUT /16384) * 4;
  long sens = parms.c1 + parms.c3 * dUT / 1024;
  long x = sens * (d1 - 7168) / 16384 - off;

  *pressure = (x * 10 / 32 + parms.c7) / 10.0;
  *temperature = (250 + dUT * parms.c6 / 65536 - dUT / pow(2,parms.d))/10.0;

  return 1;
}

#define SEALEVELPRESSURE_HPA (1013.25)
float readAltitude(float pressure)
{
    return 44330.0 * (1.0 - pow(pressure / SEALEVELPRESSURE_HPA, 0.1903));
}

float getPressureAtSeaLevel(float height, float pressure)
{
    float gradient = 0.0065;
    float tempAtSea = 15.0;
    tempAtSea += 273.15;  // °C to K
    return pressure / pow((1 - gradient * height / tempAtSea), (0.03416 / gradient));
}

void execute(char *exec, float temp, float sealevel, float pressure, float altitude, float altitude_est)
{
  char cmd[512];
  if(exec) {
    snprintf(cmd,sizeof(cmd),"%s %.1f %.1f %.1f %.1f %.1f", exec, temp, sealevel, pressure, altitude, altitude_est);
    system(cmd);
  }
}

void usage(void)
{
	fprintf(stderr,
		"hp03s - raspberry pi tool for hp03s sensors\n"
		"Options:\n"
		" -a <altitude> : Set altitude in m (default 664.0)\n"
		" -D            : Debug, print raw messages and some other stuff\n"
		" -e <exec>     : Executable to be called for every message (try echo)\n"
		" -w <wait>     : Number of seconds to sleep in loop mode (default 0)\n"
		" -m <mode>     : 0: execute once, 1: execute in loop with sleep time\n"
		);
}
//-------------------------------------------------------------------------
int main (int argc, char **argv)
{
	char *exec=NULL;
	int mode=0;
	int wait=0;
	float altitude=664.0;
	
	while(1) {
		signed char c=getopt(argc,argv,
			      "a:De:w:m:h");
		if (c==-1)
			break;
		switch (c) {
		case 'a':
			altitude=atof(optarg);
			break;
		case 'D':
			debug++;
			break;
		case 'e':
			exec=strdup(optarg);
			break;
		case 'w':
			wait=atoi(optarg);
			break;
		case 'm':
			mode=atoi(optarg);
			break;
		case 'h':
		default:
			usage();
			return 0;
		}
	}
  if (geteuid() != 0) {
    fprintf(stderr, "hp03s: Need to be root to run (sudo?)\n") ;
    exit(0);
  }
  wiringPiSetup();
  mclk(0);
  xclr(0);

  fdEEPROM=wiringPiI2CSetup (I2C_ADDRESS_EEPROM);
  fdSensor=wiringPiI2CSetup (I2C_ADDRESS_SENSOR);
  if(fdEEPROM < 0 || fdSensor < 0) {
    fprintf(stderr, "error opening I2C devices\n");
    exit(-1);
  }
  
  if(!i2c_read_eeprom(&parms)) {
    fprintf(stderr, "Failed to read parameters\n");
  }
  float temp,pressure;
  float altitude_est,sealevel;

  do {
    if(readHP03s(&temp, &pressure)) {
      altitude_est=readAltitude(pressure);
      sealevel=getPressureAtSeaLevel(altitude, pressure);
      printf( "Temp                : %.2f°C\n"
            "Pressure at sealevel: %.2fhPa\n"
            "Pressure            : %.2fhPa@%.1fm\n"
            "Altitude estimated  : %.2fm\n", temp, sealevel, pressure, altitude, altitude_est);
            execute(exec, temp, sealevel, pressure, altitude, altitude_est);
      sleep(wait);
    } else {
      fprintf(stderr, "Failed to read sensors values, retrying\n");
      sleep(1);
    }
  } while(mode);
  
  return 0 ;
}
