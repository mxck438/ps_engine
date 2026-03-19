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
