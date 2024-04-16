#include "pch.h"
#include "App.h"

int main(int argc, const char* argv[])
{
	App app;
	int r = app.run(argc, argv);
	if (r != 0)
		app.print_usage();

	return r;
}
