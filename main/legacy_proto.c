#include "legacy_proto.h"

#include "command_adapter.h"

bool legacy_handle_text(const char *text)
{
	return command_adapter_execute(text, NULL, NULL, 0);
}
