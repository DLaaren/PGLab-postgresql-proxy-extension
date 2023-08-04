#!/usr/bin/env perl

use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

my $port1 = PostgreSQL::Test::Cluster::get_free_port();
my $addr1 = "port=$port1 host=127.0.0.1";

my $port2 = PostgreSQL::Test::Cluster::get_free_port();
my $addr2 = "port=$port2 host=127.0.0.1";

my $node1 = PostgreSQL::Test::Cluster->new('replication_test1');
$node1->init;
$node1->append_conf('postgresql.conf', "shared_preload_libraries = proxy");
$node1->append_conf('postgresql.conf', "proxy.node1_listening_socket_port = $port1");
$node1->append_conf('postgresql.conf', "wal_level = logical");
$node1->append_conf('postgresql.conf', "max_wal_senders = 10");
$node1->start(allows_streaming => 'logical');

my $node2 = PostgreSQL::Test::Cluster->new('replication_test2');
$node2->init;
$node2->append_conf('postgresql.conf', "shared_preload_libraries = proxy");
$node2->append_conf('postgresql.conf', "proxy.node1_listening_socket_port = $port2");
$node2->append_conf('postgresql.conf', "wal_level = logical");
$node2->start(allows_streaming => 'logical');

#
#   CREATE TABLE
#
$node1->psql('postgres', 
             "CREATE TABLE demo(id serial primary key, data text);",
             connstr => $addr1);

$node2->psql('postgres', 
             "CREATE TABLE demo(id serial primary key, data text);",
             connstr => $addr2);

#
#   CREATE PUBLICATION
#
$node1->psql('postgres', 
             "CREATE PUBLICATION pub1 FOR TABLE demo;",
             connstr => $addr1);

#
#   CREATE SUBSCRIPTION
#
$node2->psql('postgres', 
             "CREATE SUBSCRIPTION sub1 CONNECTION 'dbname=postgres $addr1' PUBLICATION pub1;",
             connstr => $addr2);

#
#   INSERT DATA ON NODE1
#
$node1->psql('postgres',
             "INSERT INTO demo VALUES(1, 'node1');",
             connstr => $addr1);

sleep(1);
#
#   CHECK DATA ON NODE2
#
my $res; 
$node2->psql('postgres',
             "SELECT * FROM demo;",
             stdout => \$res,
             connstr => $addr2);

is ($res, '1|node1', 'replication_test');

$node2->stop('fast');
$node1->stop('fast');

done_testing();

