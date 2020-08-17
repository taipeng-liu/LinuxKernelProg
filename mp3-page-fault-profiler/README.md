To save your time:

`make test` -- This will sequentially compile all c files, insert our module, test with testcase1 and testcase2, record the data, remove the module, and clean up all compiled files.

Insert and remove module:

`make`-- Compile all c files.

`make insm`-- Insert kernel module "mp3".

`make remm` -- Remove kernel module "mp3".

`make clean` -- Clear all compiled files.

Test module:

'make testcase1' -- Run the bash script testcase1.sh, you can also run './testcase1.sh'

'make testcase2' -- Run the bash script testcase2.sh, you can also run './testcase2.sh'

Self running:

`echo "R, [pid]" > /proc/mp3/status` -- Register task

`echo "U, [pid]" > /proc/mp3/status` -- Delete task

`cat /proc/mp3/status` -- Check status of all registered tasks, such as PID, minor fault, major fault, and CPU time. 

'./work' -- Use this command to check the usage of work process

`./monitor > YOURFILE` -- Read data from shared buffer into YOURFILE
