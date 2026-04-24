#include "logging.h"

#include <libwebsockets.h>
#include <cstdarg>
#include <cctype>
#include <cstdio>
#include <algorithm>

static FILE* logFP = NULL;
static int currentAppLogLevelMask = LLL_ERR | LLL_WARN | LLL_NOTICE;
static constexpr int kLwsLogLevelMask = LLL_ERR | LLL_WARN | LLL_NOTICE;

const char* logLevel(int level)
{
	switch(level) {
		case LLL_ERR: return "ERROR";
		case LLL_WARN: return "WARNING";
		case LLL_NOTICE: return "NOTICE";
		case LLL_INFO: return "INFO";
		case LLL_DEBUG: return "DEBUG";
		case LLL_PARSER: return "PARSER";
		case LLL_HEADER: return "HEADER";
		case LLL_EXT: return "EXT";
		case LLL_CLIENT: return "CLIENT";
		case LLL_LATENCY: return "LATENCY";
		default: return "UNKNOWN";
	}
}

std::string logTime()
{
#ifdef _WIN32
	return std::string("");
#else
	struct tm tparts;
	struct timeval t;
	gettimeofday(&t, NULL);
	gmtime_r(&t.tv_sec, &tparts);
	char buff[100], buff2[40];
	strftime(buff2, sizeof(buff2), "%y-%m-%d %H:%M:%S", &tparts);
	snprintf(buff, sizeof(buff), "%s.%03d", buff2, t.tv_usec / 1000);
	return std::string(buff);
#endif
}

void log_func_var(int level, const char* fmt, ...)
{
	if ((currentAppLogLevelMask & level) == 0) {
		return;
	}
	va_list vl;
	va_start(vl, fmt);
	fprintf(logFP, "[%s]: %s: ", logTime().c_str(), logLevel(level));
	vfprintf(logFP, fmt, vl);
	va_end(vl);
}

void log_func(int level, const char* message)
{
	if ((currentAppLogLevelMask & level) == 0) {
		return;
	}
	fprintf(logFP, "[%s]: %s: %s", logTime().c_str(), logLevel(level), message);
}

int logLevelMaskFromName(const std::string& level)
{
	std::string normalized = level;
	std::transform(normalized.begin(), normalized.end(), normalized.begin(),
		[](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	if (normalized == "ERROR" || normalized == "ERR" || normalized == "LOG_ERROR") {
		return LLL_ERR;
	}
	if (normalized == "WARN" || normalized == "WARNING" || normalized == "LOG_WARNING") {
		return LLL_ERR | LLL_WARN;
	}
	if (normalized == "NOTICE" || normalized == "LOG_NOTICE") {
		return LLL_ERR | LLL_WARN | LLL_NOTICE;
	}
	if (normalized == "INFO" || normalized == "LOG_INFO") {
		return LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO;
	}
	if (normalized == "DEBUG" || normalized == "LOG_DEBUG") {
		return LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG;
	}
	return 0;
}

void logFileOpen(const std::string& logFile, int appLogLevelMask)
{
	currentAppLogLevelMask = appLogLevelMask;
	if (logFile.empty()) {
		logFP = stdout;
		setvbuf(logFP, NULL, _IOLBF, BUFSIZ);
	} else {
#if defined(WIN32)
		fopen_s(&logFP, logFile.c_str(), "a");
#else
		logFP = fopen(logFile.c_str(), "a");
#endif
		setvbuf(logFP, NULL, _IOLBF, BUFSIZ);
	}
	lws_set_log_level(kLwsLogLevelMask, &log_func);
}
