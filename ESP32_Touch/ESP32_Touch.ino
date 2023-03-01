
//Start of program
/*
ESP32 Touch test with direct calls to the ESP libraries
Board: Sparkfun ESP32 Thing
Tested ESP32_Lib version: 1.0.0 RC4

Goal of the program: Read the capacitance of a stretch sensor and transmit the read value as an analog voltage.

Touch Sensor Pin Layout
T0 = GPIO4
T1 = GPIO0
T2 = GPIO2
T3 = GPIO15
T4 = GPIO13
T5 = GPIO12    --> Currently in use
T6 = GPIO14
T7 = GPIO27
T8 = GPIO33
T9 = GPIO32


Touch settings:
touch_pad_set_voltage(TOUCH_HVOLT_2V4,TOUCH_LVOLT_0V8,TOUCH_HVOLT_ATTEN_1V5);    //0,3,3   --> Check att voltages, now 0 att.
touch_pad_set_cnt_mode(TOUCH_PAD_NUM5,TOUCH_PAD_SLOPE_7,TOUCH_PAD_TIE_OPT_LOW)

DAC pin: DAC_1 == GPIO_25		(8Bit real DAC, No PWM)

Additional information: http://esp-idf.readthedocs.io/en/latest/api-reference/peripherals/touch_pad.html


																								Thijs Vandenryt 2018
*/
#include "driver/DAC.h" 	 //Required to use the DAC outputs


#define MAX_CAP_VAL 1500	//Maximum recorded value for the given belt (can be collected in a calibration routine: e.g. 3x max depth inhalation
#define MIN_CAP_VAL 1200	//Lowest value recorded for the given belt/patient
#define DelayTimeMS 90		//Time between new capcitive datapoints

void setup()
{
	Serial.begin(115200);
	dac_output_enable(DAC_CHANNEL_1);			//Enable DAC1 == GPIO 25
	
	Serial.println("\n\n\n\n Touch sensor init.");
	if (touch_pad_init()) {
		Serial.println("!!!!Init ERROR");
	}

	touch_pad_config(TOUCH_PAD_NUM5, 0);			//Num5==GPIO12

	if (touch_pad_filter_start(10)) {				//Setup filter setup (10ms filter duration)
		Serial.println("!!!!Filter Start ERROR");
	}

}


void loop()
{
	uint16_t ReadVal[2] = { 0 };	//[0]Holds the actual capacitive value, [1]The previous value
	uint8_t DataRDY = 0;			//DataReady signal to start data manipulation
	long TempTime = millis();		//Contains the time for the updaterate

	PrintThresholdSettings();

	if (touch_pad_set_cnt_mode(TOUCH_PAD_NUM5, TOUCH_PAD_SLOPE_7, TOUCH_PAD_TIE_OPT_LOW)) { //slope_1: Slowest ,7 fastest, 4 Default
		Serial.println("!!!!Slope setting ERROR");
	}

	PrintSlopeSettings();

	if (touch_pad_set_voltage(TOUCH_HVOLT_2V4, TOUCH_LVOLT_0V8, TOUCH_HVOLT_ATTEN_1V5)) {//0,3,3   --> Check att voltages, now 0 att.
		Serial.println("!!!!Voltage setting ERROR");
	}

	delay(10);		//wait for settings to be applied
	PrintVoltageSettings();
	PrintMeasTimeSettings();
	Serial.println("--------------------\n\n\n");    //Separate header from data
	
	void DacCycle(void);		//Cycle the DAC for 1s to show µC is ready to read data.
	
	//Superloop
	while (1) {
		if (TempTime+DelayTimeMS <= (millis())) {     //If DelayTimeMS has elapsed, new datapoint is read
			if (touch_pad_read_filtered(TOUCH_PAD_NUM5, &ReadVal[0])) {	//Raw data is read 
																		//if(touch_pad_read_filtered(TOUCH_PAD_NUM5,&ReadVal){		//Filtered data is read--> Needs initialisation with "touch_pad_filter_start(Filter_time_in_ms)"
				Serial.println("Read filtered value error");
			}
			else if (ReadVal[0] == ReadVal[1]){		//No update is neccessary
				DataRDY = 0;
			}
			else {
				Serial.print(ReadVal[0]);		//Print raw read value
				DataRDY = 1;
			}
			ReadVal[1] = ReadVal[0];
		}


		if (DataRDY) {
			//Sanitize read data
			if (ReadVal[0] > MAX_CAP_VAL) {
				ReadVal[0] = MAX_CAP_VAL;
			}
			else if (ReadVal[0] < MIN_CAP_VAL) {
				ReadVal[0] = MIN_CAP_VAL;
			}
			else {
				//ReadVal = map(ReadVal, 0, 2400, 0, 255);
			}

			//Flip the data: Stretch --> Increase in signal
			ReadVal[0] = MAX_CAP_VAL - ReadVal[0];		//ReadVal between 2400 to 1200 ==> New ReadVal = 0 to 1200
			ReadVal[0] = map(ReadVal[0], 0, (MAX_CAP_VAL-MIN_CAP_VAL), 0, 255);		//Remap data to 8-Bit DAC output

			//Print converted data
			Serial.print(","); Serial.println(ReadVal[0]);

			//Analog output of the relative capacity
			if (dac_output_voltage(DAC_CHANNEL_1, ReadVal[0]) != ESP_OK) {
				Serial.print("Error setting the DAC voltage at ");
				Serial.print(ReadVal[0]); Serial.println(".");
			}
			DataRDY = 0;
		}
	}
}



void PrintVoltageSettings(void)		//Prints the excitation voltage settings
{
	uint8_t VoltH = 100;
	uint8_t VoltL = 100;
	uint8_t VoltAtten = 100;

	Serial.print("Voltages: Status ");
	Serial.print(touch_pad_get_voltage((touch_high_volt_t*)&VoltH, (touch_low_volt_t*)&VoltL, (touch_volt_atten_t*)&VoltAtten));
	Serial.print(", VoltH "); Serial.print(VoltH);
	Serial.print(", VoltL "); Serial.print(VoltL);
	Serial.print(", VoltAtten "); Serial.println(VoltAtten);
}

void PrintThresholdSettings(void) {
	uint16_t retval = 100;

	Serial.print("Threshold: Status "); Serial.print(touch_pad_get_thresh(TOUCH_PAD_NUM5, &retval));
	Serial.print(", Threshold "); 	Serial.println(retval);
}

void PrintSlopeSettings(void) {
	uint16_t retval = 100, retval2 = 100;

	Serial.print("Slope: Status "); Serial.print(touch_pad_get_cnt_mode(TOUCH_PAD_NUM5, (touch_cnt_slope_t*)&retval, (touch_tie_opt_t*)&retval2));
	Serial.print(", Slope "); Serial.print(retval);
	Serial.print(", Init Voltage "); Serial.println(retval2);
}


void PrintMeasTimeSettings(void) {
	uint16_t sleep_cycleVal = 100, meas_cycleVal = 100;

	Serial.print("Get Measurement Time: Status ");
	Serial.print(touch_pad_get_meas_time(&sleep_cycleVal, &meas_cycleVal));
	Serial.print(", NumOfSleepCycles ");
	Serial.print(sleep_cycleVal);
	Serial.print(", Measurements Cycles: ");
	Serial.println(meas_cycleVal);
}


void DacCycle(void) {			//Cycles the output of the DAC for 1s, to show the start of the output.
	for (int i = 0; i < 4; i++) {
		dac_output_voltage(DAC_CHANNEL_1, 200);
		delay(120);
		dac_output_voltage(DAC_CHANNEL_1, 50);
		delay(120);
	}
	dac_output_voltage(DAC_CHANNEL_1, 0);
}

//End of program
