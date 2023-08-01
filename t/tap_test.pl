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

my $ret = $node1->psql('postgres', 'SELECT 506',
                       extra_params => ['-h 127.0.0.1', '-p 15001']);
                       
$ret = $node1->psql('postgres', 'SELECT 505',
                       extra_params => ['-h 127.0.0.1', '-p 15001']);
                       
is ($ret, '505', 'SELECT 505 returns 505');


# Stop the PostgreSQL server
$node1->stop('fast');

# Each test script should end with:
done_testing();






# Modify or delete an existing setting
# $node->adjust_conf('postgresql.conf', 'max_wal_senders', '10');