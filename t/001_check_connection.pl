 
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

my $port = PostgreSQL::Test::Cluster::get_free_port();
my $addr = "port=$port host=127.0.0.1";

# then it will generally need to set up one or more nodes, run commands
# against them and evaluate the results
my $node = PostgreSQL::Test::Cluster->new('connection_test');

# Create a data directory with initdb
$node->init;

# Add a setting
$node->append_conf('postgresql.conf', "shared_preload_libraries = proxy");
$node->append_conf('postgresql.conf', "proxy.node1_listening_socket_port = $port");

# Start the PostgreSQL server
$node->start();



my $res;
$node->psql('connection_test',
            'SELECT 35',
            stdout => \$res,
            connstr => $addr);

is ($res, '35', 'SELECT 35 returns 35');


# Stop the PostgreSQL server
$node->stop('fast');

# Each test script should end with:
done_testing();