create function set_speed(int) returns Datum
as '$libdir/proxy_extension' language C;