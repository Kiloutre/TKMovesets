#include "Process.h"

void Process::Attach()
{
	if (attached)
		throw("Process is already attached : not re-attaching.");

	attached = false;
}
