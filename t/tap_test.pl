#!/usr/bin/env perl

# https://git.postgresql.org/gitweb/?p=postgresql.git;a=blob;f=src/test/perl/README

# Each test script should begin with:
use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

# then it will generally need to set up one or more nodes, run commands
# against them and evaluate the results

my $node = PostgreSQL::Test::Cluster->new('test');
$node->init;
$node->start;

my $ret = $node->safe_psql('postgres', 'SELECT 1');
is ($ret, '1', 'SELECT 1 returns 1');

$node->stop('fast');


# Each test script should end with:
done_testing();