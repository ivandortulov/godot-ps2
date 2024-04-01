#include <tamtypes.h>
#include <sifrpc.h>
#include <kernel.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <loadfile.h>
#include <stdio.h>
#include <sbv_patches.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <debug.h>

#include "os_ps2.hpp"
#include "main/main.h"

int main(int argc, char* argv[])
{
	SifInitRpc(0);

	init_scr();
	sleep(3);

	printf("Entered main!n");

	OS_PS2 os;

	printf("Core constructed\n");

	char* cwd = new char[256];
	getcwd(cwd, 256);
	printf("CWD is '%s'\n", cwd);

	// char* main_pack_arg = "-main_pack";
	// char* main_pack_argv = "mass://data.pck";
	// char* new_args[2];
	// new_args[0] = main_pack_arg;
	// new_args[1] = main_pack_argv;

	Error err = Main::setup(argv[0], argc - 1, &argv[1]);
	printf("Setup result: %d (argv[0] = %s, argc = %d)\n", int(err), argv[0], argc - 1);
	scr_printf("Setup result: %d (argv[0] = %s, argc = %d)\n", int(err), argv[0], argc - 1);

	if (err != OK)
	{
		delete[] cwd;
		printf("Failed to initialize! Exiting ....\n");
		scr_printf("Failed to initialize! Exiting ....\n");
		while (1) {}
		return err;
	}

	if (Main::start())
	{
		printf("Main started\n");
		scr_printf("Main started\n");
		os.run();
	}
	printf("Main exited\n");
	scr_printf("Main exited\n");
	Main::cleanup();
	printf("Cleanup complete\n");
	scr_printf("Main exited\n");

	chdir(cwd);
	delete[] cwd;

	return os.get_exit_code();
}