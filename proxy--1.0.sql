/* contrib/proxy/proxy--1.0.sql */

CREATE FUNCTION set_speed(int) RETURNS void
AS 'MODULE_PATHNAME', 'set_speed' LANGUAGE C;

/* TODO: необходимо объявить тип в psql CREATE TYPE Channel, CREATE TYPE Toxic
 * https://postgrespro.ru/docs/postgresql/15/xtypes
 */
CREATE FUNCTION run(int port, text toxic_name) RETURNS void
AS 'MODULE_PATHNAME/toxics/', 'run' LANGUAGE C;