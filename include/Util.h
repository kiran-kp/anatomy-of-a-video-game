#define ensureNoLog(x) if (!(x)) { int *y = 0; *y = 42; }
#define ensure(x) if (!(x)) { LOG("ensure failed: %s"#x); LOGGER_FLUSH(); int *y = 0; *y = 42; }