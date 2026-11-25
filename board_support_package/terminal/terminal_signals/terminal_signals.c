/*
 * terminal_signals.c
 *
 *  Created on: Oct 6, 2020
 *      Author: Auramenka Andrew
 */

#include	"dspa.h"
#include	"dspa_defs.h"
#include 	"dspa_sigdefs.h"
#include	"terminal_signals.h"


#include 	"am1805.h"
#include 	"ds1621.h"
#include 	"fram_locker.h"

static	char *sDEV = "";
static	char *sTIME = "";
static  char *sTIME_CORRECTION = "";
static  char *sLOCK = "";

static	AM1805_Time_t set_time={
	    .year     = 2026,
	    .month    = 1,
	    .day      = 1,
	    .hours    = 0,
	    .minutes  = 0,
	    .seconds  = 0,
	    .hundredths = 0,
	    .is_xt_active = true  /* По умолчанию используем кварц */
	};
static	bool run_set_data=	false;
static	bool run_set_correction= false;
static	float  	ppm_rtc= 0;
static  bool reset_1805= false;
uint32_t temp_password=	0xFFFFFFFF;
bool stlink_attached = false;

SIGNALS_BEGIN(DSPA_SIGNALS_NAME)
	_STRING_R_  ("Ballistic computer(bootloader)", sDEV, NULL),
		_U32_R_	("hardware testing status",	test_hardware_result,	&sDEV),
		_FLOAT_R_("board temperature(ds1621s+)[°C] ", current_temp, &sDEV),
		_STRING_R_ ("Date+Time", sTIME, &sDEV),
			_STRING_R_ ("current data", data_time_string, &sTIME),
			_BOOL_R_("XT_osc present",set_time.is_xt_active, &sTIME),
			_BYTE_RW_("hours", set_time.hours, &sTIME),
			_BYTE_RW_("minutes", set_time.minutes, &sTIME),
			_BYTE_RW_("seconds", set_time.seconds, &sTIME),
			_U16_RW_("year",set_time.year, &sTIME),
			_BYTE_RW_("month", set_time.month, &sTIME),
			_BYTE_RW_("day", set_time.day, &sTIME),
			_BOOL_RW_("setting  date and time", run_set_data, &sTIME),
			_STRING_R_ ("Time correction", sTIME_CORRECTION, &sTIME),
				_BYTE_R_("BREF",  rtc_diag.bref, &sTIME_CORRECTION),
				_BYTE_R_("CAL_XT",  rtc_diag.cal_xt, &sTIME_CORRECTION),
				_BYTE_R_("CTRL1",  rtc_diag.ctrl1, &sTIME_CORRECTION),
				_BYTE_R_("OSC CTRL",  rtc_diag.osc_ctrl, &sTIME_CORRECTION),
				_BYTE_R_("OSC STATUS",  rtc_diag.osc_stat, &sTIME_CORRECTION),
				_BYTE_R_("STATUS",  rtc_diag.status, &sTIME_CORRECTION),
				_FLOAT_RW_("corr.[sec./hour]('+' if clock is fast)",ppm_rtc,&sTIME_CORRECTION ),
				_BOOL_RW_("set correction",run_set_correction, &sTIME_CORRECTION),
				_BOOL_RW_("be careful!forced hard reset of the AM1805",reset_1805, &sTIME_CORRECTION),
		_STRING_R_ ("System info", sLOCK, &sDEV),
			_BOOL_R_ ("STLINK connected", stlink_attached, &sLOCK),
			_U32_RW_("magic",	temp_password, &sLOCK),
			_U32_RW_("current counter",FRAM_LOCKER_COUNTER, &sLOCK),
			_U32_RW_("threshold",FRAM_LOCKER_THRESHOLD, &sLOCK)


SIGNALS_END(DSPA_SIGNALS_NAME)


int	init_terminal_signals(void){
	return	SIG_INIT(DSPA_SIGNALS_NAME);
}


void signal_change_handler(void *s) {


	if (s == &temp_password) {
		if (FramLocker_SaveWithPassword(temp_password) == osOK) {
			temp_password = 0xFFFFFFFF;
		}
	}

	// установка даты
	if (s == &run_set_data) {
		run_set_data = false;
		AM1805_SetTime(&set_time);

	}

	// запись коррекции RTC
	if (s == &run_set_correction) {
		run_set_correction = false;
		AM1805_SetCalibrationByDrift(ppm_rtc);
		AM1805_GetDiagnostics(&rtc_diag);
	}

	// reset 1805
	if (s == &reset_1805) {
		reset_1805 = false;

		AM1805_ForceInit(&hi2c1, DS1621_GetI2CMutex(), AM1805_INIT_PASSWORD);
		AM1805_GetDiagnostics(&rtc_diag);

	}

}
