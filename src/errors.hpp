#pragma once


enum {
	NO_ERROR,
	MISC_ERROR,
	TOO_MUCH_DATA_TO_ENCODE,
	INVALID_PNG_MAGIC_NUMBER,
	OOM,
	CANNOT_CREATE_PNG_READ_STRUCT,
	EMPTY_BITPLANE,
	COULD_NOT_OPEN_PNG_FILE,
	CANNOT_CREATE_PNG_INFO_STRUCT,
	PNG_ERROR_1,
	PNG_ERROR_2,
	PNG_ERROR_3,
	PNG_ERROR_4,
	TOO_MANY_BITPLANES,
	IMAGE_IS_NOT_RGB,
	WRONG_NUMBER_OF_CHANNELS,
	N_ERRORS
};
#ifdef NO_EXCEPTIONS
# define handler(msg) exit(msg)
#else
# include <stdexcept>
# define handler(msg) throw std::runtime_error(handler_msgs[msg])
constexpr static
const char* const handler_msgs[] = {
	"No error",
	"Misc error",
	"Reached last image (too much data to encode?)",
	"Invalid PNG magic number",
	"Out of memory",
	"Cannot create PNG read struct",
	"Empty bitplane",
	"Could not open PNG file",
	"Could not create PNG info struct",
	"PNG error 1",
	"PNG error 2",
	"PNG error 3",
	"PNG error 4",
	"Bitdepth is too high",
	"Image is not RGB",
	"Wrong number of colour channels in image",
	""
};
#endif
