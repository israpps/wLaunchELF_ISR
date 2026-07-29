#include "launchelf.h"
#include "main_title.h"

#include <osd_config.h>

#define MENU_TITLE_CLOCK_YEAR_BASE 2000
#define MENU_TITLE_PS2_RTC_BASE_OFFSET_MINUTES 540
#define MENU_TITLE_CLOCK_POLL_MS 1000
#define MENU_TITLE_TEMP_POLL_MS 5000
#define MENU_TITLE_FORMAT "$TIME  $DATE  $TEMP"

typedef struct
{
	int initialized;
	int valid;
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int use_12h;
	int date_format;
	u64 last_tick_ms;
	char time_text[16];
	char date_text[16];
} MenuTitleClockState;

typedef struct
{
	int checked;
	int supported;
	u64 last_poll_ms;
	char text[8];
} MenuTitleTempState;

static MenuTitleClockState title_clock = {
	0, 0, MENU_TITLE_CLOCK_YEAR_BASE, 1, 1, 0, 0, 0, 0, 0, 0, "", ""};
static MenuTitleTempState title_temp = {0, 0, 0, ""};

static int bcdToInt(u8 value)
{
	return ((value >> 4) * 10) + (value & 0x0F);
}

static int hasToken(const char *text, const char *token)
{
	return (text != NULL && strstr(text, token) != NULL);
}

static int menuTitleUsesTime(void)
{
	return hasToken(MENU_TITLE_FORMAT, "$TIME");
}

static int menuTitleUsesDate(void)
{
	return hasToken(MENU_TITLE_FORMAT, "$DATE");
}

static int menuTitleUsesClock(void)
{
	return menuTitleUsesTime() || menuTitleUsesDate();
}

static int menuTitleUsesTemp(void)
{
	return hasToken(MENU_TITLE_FORMAT, "$TEMP");
}

static int menuTitleUsesDynamicTokens(void)
{
	return menuTitleUsesClock() || menuTitleUsesTemp();
}

static int daysInMonth(int year, int month)
{
	static const int month_days[12] = {
	    31, 28, 31, 30, 31, 30,
	    31, 31, 30, 31, 30, 31};

	if (month == 2 && ((year % 400) == 0 || ((year % 4) == 0 && (year % 100) != 0)))
		return 29;
	if (month < 1 || month > 12)
		return 31;
	return month_days[month - 1];
}

static void normalizeClockDate(MenuTitleClockState *clock)
{
	int dim;

	if (clock->month < 1)
		clock->month = 1;
	if (clock->month > 12)
		clock->month = 12;
	if (clock->day < 1)
		clock->day = 1;

	dim = daysInMonth(clock->year, clock->month);
	if (clock->day > dim)
		clock->day = dim;
}

static void advanceClockSeconds(MenuTitleClockState *clock, u64 seconds)
{
	while (seconds > 0) {
		int dim;

		clock->second++;
		if (clock->second < 60) {
			seconds--;
			continue;
		}

		clock->second = 0;
		clock->minute++;
		if (clock->minute < 60) {
			seconds--;
			continue;
		}

		clock->minute = 0;
		clock->hour++;
		if (clock->hour < 24) {
			seconds--;
			continue;
		}

		clock->hour = 0;
		clock->day++;
		dim = daysInMonth(clock->year, clock->month);
		if (clock->day <= dim) {
			seconds--;
			continue;
		}

		clock->day = 1;
		clock->month++;
		if (clock->month <= 12) {
			seconds--;
			continue;
		}

		clock->month = 1;
		clock->year++;
		seconds--;
	}
}

static void shiftClockMinutes(MenuTitleClockState *clock, int delta_minutes)
{
	while (delta_minutes > 0) {
		advanceClockSeconds(clock, 60);
		delta_minutes--;
	}

	while (delta_minutes < 0) {
		clock->minute--;
		if (clock->minute >= 0) {
			delta_minutes++;
			continue;
		}

		clock->minute = 59;
		clock->hour--;
		if (clock->hour >= 0) {
			delta_minutes++;
			continue;
		}

		clock->hour = 23;
		clock->day--;
		if (clock->day >= 1) {
			delta_minutes++;
			continue;
		}

		clock->month--;
		if (clock->month < 1) {
			clock->month = 12;
			clock->year--;
		}
		clock->day = daysInMonth(clock->year, clock->month);
		delta_minutes++;
	}
}

static int decodeTimezoneOffsetMinutes(int raw_offset)
{
	int value = raw_offset & 0x7FF;

	if (value & 0x400)
		value -= 0x800;

	return value;
}

static void readOsdClockFormats(int *use_12h, int *date_format, int *local_offset_minutes)
{
	ConfigParam config;
	Config2Param config2;
	int daylight_saving = 0;

	*use_12h = 0;
	*date_format = 0;
	*local_offset_minutes = MENU_TITLE_PS2_RTC_BASE_OFFSET_MINUTES;

	memset(&config, 0, sizeof(config));
	GetOsdConfigParam(&config);
	if (config.version == 0)
		return;

	*local_offset_minutes = decodeTimezoneOffsetMinutes(config.timezoneOffset);
	memset(&config2, 0, sizeof(config2));
	GetOsdConfigParam2(&config2, sizeof(config2), 0);
	*use_12h = (config2.timeFormat != 0);
	*date_format = config2.dateFormat;
	daylight_saving = (config2.daylightSaving != 0);

	if (*date_format < 0 || *date_format > 2)
		*date_format = 0;
	if (daylight_saving)
		*local_offset_minutes += 60;
}

static void formatClockTime(char *dst, size_t dst_size, int hour, int minute, int second, int use_12h)
{
	if (use_12h) {
		const char *suffix = (hour >= 12) ? "PM" : "AM";
		int hour12 = hour % 12;

		if (hour12 == 0)
			hour12 = 12;
		snprintf(dst, dst_size, "%02d:%02d:%02d %s", hour12, minute, second, suffix);
	} else {
		snprintf(dst, dst_size, "%02d:%02d:%02d", hour, minute, second);
	}
}

static void formatClockDate(char *dst, size_t dst_size, int year, int month, int day, int date_format)
{
	switch (date_format) {
		case 1:
			snprintf(dst, dst_size, "%02d/%02d/%04d", month, day, year);
			break;
		case 2:
			snprintf(dst, dst_size, "%02d/%02d/%04d", day, month, year);
			break;
		default:
			snprintf(dst, dst_size, "%04d/%02d/%02d", year, month, day);
			break;
	}
}

static int refreshClockText(int care_time, int care_date)
{
	char old_time[sizeof(title_clock.time_text)];
	char old_date[sizeof(title_clock.date_text)];
	int time_changed;
	int date_changed;

	strncpy(old_time, title_clock.time_text, sizeof(old_time));
	old_time[sizeof(old_time) - 1] = '\0';
	strncpy(old_date, title_clock.date_text, sizeof(old_date));
	old_date[sizeof(old_date) - 1] = '\0';

	if (title_clock.valid) {
		formatClockTime(title_clock.time_text,
		                sizeof(title_clock.time_text),
		                title_clock.hour,
		                title_clock.minute,
		                title_clock.second,
		                title_clock.use_12h);
		formatClockDate(title_clock.date_text,
		                sizeof(title_clock.date_text),
		                title_clock.year,
		                title_clock.month,
		                title_clock.day,
		                title_clock.date_format);
	} else {
		snprintf(title_clock.time_text, sizeof(title_clock.time_text), "--:--:--");
		snprintf(title_clock.date_text, sizeof(title_clock.date_text), "----/--/--");
	}

	time_changed = strcmp(old_time, title_clock.time_text) != 0;
	date_changed = strcmp(old_date, title_clock.date_text) != 0;

	return (care_time && time_changed) || (care_date && date_changed);
}

static int seedClockFromPs2(u64 tick_ms, int care_time, int care_date)
{
	sceCdCLOCK clock_data;
	int local_offset_minutes;

	memset(&clock_data, 0, sizeof(clock_data));
	if (sceCdReadClock(&clock_data) == 0 || clock_data.stat != 0) {
		title_clock.initialized = 1;
		title_clock.valid = 0;
		title_clock.last_tick_ms = tick_ms;
		return refreshClockText(care_time, care_date);
	}

	title_clock.year = MENU_TITLE_CLOCK_YEAR_BASE + bcdToInt(clock_data.year);
	title_clock.month = bcdToInt(clock_data.month & 0x7F);
	title_clock.day = bcdToInt(clock_data.day);
	title_clock.hour = bcdToInt(clock_data.hour);
	title_clock.minute = bcdToInt(clock_data.minute);
	title_clock.second = bcdToInt(clock_data.second);
	readOsdClockFormats(&title_clock.use_12h, &title_clock.date_format, &local_offset_minutes);
	normalizeClockDate(&title_clock);
	shiftClockMinutes(&title_clock, local_offset_minutes - MENU_TITLE_PS2_RTC_BASE_OFFSET_MINUTES);

	title_clock.initialized = 1;
	title_clock.valid = 1;
	title_clock.last_tick_ms = tick_ms;

	return refreshClockText(care_time, care_date);
}

static int updateClock(u64 tick_ms, int force, int care_time, int care_date)
{
	u64 elapsed_ms;
	u64 elapsed_seconds;

	if (force || !title_clock.initialized || tick_ms < title_clock.last_tick_ms)
		return seedClockFromPs2(tick_ms, care_time, care_date);

	elapsed_ms = tick_ms - title_clock.last_tick_ms;
	if (elapsed_ms < MENU_TITLE_CLOCK_POLL_MS)
		return 0;

	elapsed_seconds = elapsed_ms / 1000u;
	if (elapsed_seconds > 0) {
		if (!title_clock.valid)
			return seedClockFromPs2(tick_ms, care_time, care_date);
		advanceClockSeconds(&title_clock, elapsed_seconds);
		title_clock.last_tick_ms += elapsed_seconds * 1000u;
		return refreshClockText(care_time, care_date);
	}

	return 0;
}

static int queryTemperatureCelsius(char *temp_buf, size_t temp_buf_size)
{
	unsigned char in_buffer[1];
	unsigned char out_buffer[16];
	unsigned short temp;
	int whole;
	int tenths;
	int stat = 0;

	if (temp_buf == NULL || temp_buf_size == 0)
		return 0;

	temp_buf[0] = '\0';
	memset(out_buffer, 0, sizeof(out_buffer));

	in_buffer[0] = 0xEF;
	if (sceCdApplySCmd(0x03, in_buffer, sizeof(in_buffer), out_buffer) == 0)
		return 0;
	stat = out_buffer[0];

	if (stat != 0)
		return 0;

	temp = (unsigned short)(out_buffer[1] * 256 + out_buffer[2]);
	whole = (temp - (temp % 128)) / 128;
	tenths = (int)(((temp % 128) * 10 + 64) / 128);
	if (tenths >= 10) {
		whole++;
		tenths -= 10;
	}

	snprintf(temp_buf, temp_buf_size, "%02d.%dC", whole, tenths);
	return 1;
}

static int updateTemp(u64 tick_ms, int force)
{
	char old_temp[sizeof(title_temp.text)];

	if (!force && title_temp.checked && !title_temp.supported)
		return 0;
	if (!force && title_temp.last_poll_ms != 0 && tick_ms >= title_temp.last_poll_ms &&
	    tick_ms - title_temp.last_poll_ms < MENU_TITLE_TEMP_POLL_MS)
		return 0;

	strncpy(old_temp, title_temp.text, sizeof(old_temp));
	old_temp[sizeof(old_temp) - 1] = '\0';

	title_temp.checked = 1;
	title_temp.last_poll_ms = tick_ms;
	if (queryTemperatureCelsius(title_temp.text, sizeof(title_temp.text))) {
		title_temp.supported = 1;
	} else {
		title_temp.text[0] = '\0';
	}

	return strcmp(old_temp, title_temp.text) != 0;
}

int menuTitleUpdateAsync(int force)
{
	u64 tick_ms;
	int changed = 0;
	int uses_time;
	int uses_date;

	if (!menuTitleUsesDynamicTokens())
		return 0;

	tick_ms = Timer();
	uses_time = menuTitleUsesTime();
	uses_date = menuTitleUsesDate();
	if (uses_time || uses_date)
		changed |= updateClock(tick_ms, force, uses_time, uses_date);
	if (menuTitleUsesTemp())
		changed |= updateTemp(tick_ms, force);

	return changed;
}

static void appendChar(char *out, size_t out_size, size_t *out_pos, char value)
{
	if (*out_pos + 1 >= out_size)
		return;

	out[*out_pos] = value;
	(*out_pos)++;
	out[*out_pos] = '\0';
}

static void appendText(char *out, size_t out_size, size_t *out_pos, const char *text)
{
	if (text == NULL)
		return;

	while (*text != '\0') {
		appendChar(out, out_size, out_pos, *text);
		text++;
	}
}

void menuTitleFormat(char *out, size_t out_size)
{
	const char *src;
	size_t out_pos = 0;

	if (out == NULL || out_size == 0)
		return;

	out[0] = '\0';

	if (menuTitleUsesDynamicTokens())
		menuTitleUpdateAsync(menuTitleUsesClock() && !title_clock.initialized);

	src = MENU_TITLE_FORMAT;
	while (*src != '\0') {
		if (!strncmp(src, "$TIME", 5)) {
			appendText(out, out_size, &out_pos,
			           title_clock.time_text[0] != '\0' ? title_clock.time_text : "--:--:--");
			src += 5;
		} else if (!strncmp(src, "$DATE", 5)) {
			appendText(out, out_size, &out_pos,
			           title_clock.date_text[0] != '\0' ? title_clock.date_text : "----/--/--");
			src += 5;
		} else if (!strncmp(src, "$TEMP", 5)) {
			appendText(out, out_size, &out_pos, title_temp.text);
			src += 5;
		} else {
			appendChar(out, out_size, &out_pos, *src);
			src++;
		}
	}

	while (out_pos > 0 && out[out_pos - 1] == ' ') {
		out_pos--;
		out[out_pos] = '\0';
	}
}
