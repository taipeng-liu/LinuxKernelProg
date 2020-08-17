To save your time:

`make test` -- This will sequentially compile all *.c files, insert our module, test with default parameters, remove the module, and clean up all compiled files.

Insert and remove module:

`make`-- Compile all *.c files.

`make insm`-- Insert kernel module "mp2".

`make remm` -- Remove kernel module "mp2".

`make clean` -- Clear all compiled files.

Test module:

`echo "R, [pid], [compute_time_ms], [period_ms]" > /proc/mp2/status` -- Register task

`echo "Y, [pid]" > /proc/mp2/status` -- Yield task

`echo "D, [pid]" > /proc/mp2/status` -- Delete task

`cat /proc/mp2/status` -- Check status of all registered tasks

`./userapp [period_ms] [num of jobs]` -- Run user application with specified period and num of jobs.

`make testcase` -- Run default user application by execute "./userapp 10000 5".

