
# PS Engine

The **PS Engine** for Firebird RDBMS enables the integration of custom functions into any database. A primary advantage of this engine is the support for **live updates** to function logic, eliminating the need for server downtime.

For the motivation behind the project, see [design and implementation walk-through](<Firebird - building hot-pluggable external functions.md>).

### Building

Currently, only **Linux** builds are supported.

Four targets are produced:
- libps_engine.so - The PS Engine module.
- libps_module.so - An example module implementing user functions.
- so_test_load - A basic utility to test the successful loading of a shared library.
- test_pse - A test program for the PS Engine.

To build PS Engine, you must also clone [XSI library](https://github.com/mxck438/xsi). XSI is a C framework I developed to provide facilities like threading, logging, async I/O and more. 

You can modify the *libps_engine.so* source files to remove the XSI dependency if desired, as it is primarily used for logging. The same applies to *so_test_load*. While *libps_module.so* does not depend on XSI, the *test_pse* target is heavily dependent on it.


```
git clone https://github.com/mxck438/xsi
git clone https://github.com/mxck438/ps_engine
cd ps_engine
mkdir build && cd build
cmake ..
make
```

### Installation

Copy *libps_engine.so* and *libps_module.so* to the Firebird *plugins* directory. Alternatively, create **symbolic links** in the *plugins* directory pointing to them. The latter method speeds up development; thanks to the engine's architecture, you don't need to restart the server, and the link ensures the freshly built module is always active.

Edit *plugins.conf* to incude the following:
```
Plugin = PSE {
        Module = $(dir_plugins)/ps_engine
}
```

**Restart the Firebird server** (yes, a single restart is required to reload the configuration files).

To test manually, use a Firebird client of your choice to install the necessary external functions into a database. The test project (described below) installs these automatically.
```
create function pse_reload_module (
    dummy integer
) returns varchar(1024)
    external name 'pse_reload_module'
    engine pse;

create function pse_get_module_info (
    dummy integer
) returns varchar(1024)
    external name 'pse_get_module_info'
    engine pse;    

create or alter function my_sum_args (
    n1 integer,
    n2 integer,
    n3 integer
) returns integer
    external name 'my_sum_args'
    engine pse;

create function my_sum_args2 (
    n1 integer,
    n2 integer,
    n3 integer
) returns integer
    external name 'my_sum_args2'
    engine pse;

```
To execute a function:
```
select my_sum_args(1, 5, 100) from rdb$database;
// or
select my_sum_args2(1, 5, 100) from rdb$database;
```
To reload a module:
```
select pse_reload_module(0) from rdb$database;
```

### Testing

Adjust the test configuration in *test_pse.conf* (a symlink is created in the build directory).

#### General Section:

```
[general]
libfbclient = libfbclient.so.3.0.7 # may be empty or a filename or a full path
fbserver = 127.0.0.1 
port = 3050
user = sysdba
password = masterkey
database_count = 2      # Loader will look for [database_1], [database_2], etc.
functions_count = 2     # Loader will look for [function_1], [function_2], etc.
drop_funcs_before_use = false
use_create_or_alter = true
run_module_reloder = true   # Runs a separate thread to reload the module at random intervals
```

#### Function Sections:

```
[function_1]
name = my_sum_args
parameter_list = n1 integer, n2 integer, n3 integer
returns = integer
external_name = my_sum_args
engine = pse 
test_count = 2 # test1=, test2=, etc. should follow
test1 = select MY_SUM_ARGS(1,2,3) from RDB$DATABASE;    # func exec sql
test_result1 = 6    # function should return
test2 = select coalesce(MY_SUM_ARGS(1,null,3), -113) from RDB$DATABASE; # func exec sql
test_result2 = -113 # function should return
```
*Note: The testing code converts return values to text for comparison. Ensure your SQL query returns a single integer or varchar.*

#### Database Section

```
[database_1]
dbpath = /data/fbdata/test.gdb
functions = my_sum_args, my_sum_args2
test_repeat_count = 6   # Executions per function
test_threads = 2    # Concurrent execution threads
```

Run the tests from the build directory:
```
./test_pse
```

To run continuous stress tests, use the provided script:
```
./test_pse_times 1000 ./test_pse
```
This runs the suite 1,000 times. Press **Enter** to stop the loop.

### PS Engine logs

The PS Engine logs to Firebird *logs* directory, usually */var/log/firebird*. You may monitor this file:
```
> sudo tail -f /var/log/firebird/ps_engine.log
```

