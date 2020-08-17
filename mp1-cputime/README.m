For normal usage:
`make` Compile all *.c files
`make insm` Insert kernel module "mp1"
`make testcase` Run testcase by execute "./userapp"
`make remm` Remove kernel module "mp1"

For simple testing:
`make test` Automatically compile, insert, test, and remove the module

At both conditions, "./userapp" should print out result in the following format:
<PID1>: <CPU_time1>
<PID2>: <CPU_time2>
...
