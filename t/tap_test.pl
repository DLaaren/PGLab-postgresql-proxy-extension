 
#!/usr/bin/env perl

# Each test script should begin with:
use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

#
# Test set-up
#

# then it will generally need to set up one or more nodes, run commands
# against them and evaluate the results

my $node1 = PostgreSQL::Test::Cluster->new('tap_test');

# Create a data directory with initdb
$node1->init;

# Add a setting
$node1->append_conf('postgresql.conf', "shared_preload_libraries = proxy");

# Start the PostgreSQL server
$node1->start();

#
# Test set-up
#

my $res;
my $addr = "port=15001 host=127.0.0.1 dbname='postgres'";
$node1->psql('postgres', 'SELECT (505 + 101)',
                        stdout => \$res,
                       connstr => $addr);


#my $res = qx/ psql -h 127.0.0.1 -p 15001 / ;

is ($res, '606', 'SELECT (505+101) returns 606');


# Stop the PostgreSQL server
$node1->stop('fast');

# Each test script should end with:
done_testing();






# Modify or delete an existing setting
# $node->adjust_conf('postgresql.conf', 'max_wal_senders', '10');