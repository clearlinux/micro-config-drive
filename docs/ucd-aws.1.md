ucd-aws(1) -- Fetch user-data from AWS user-data service
========================================================

## SYNOPSIS

`/usr/bin/ucd-aws`

## DESCRIPTION

`ucd-aws` is a helper agent program that fetches openstack user-data
from the http://169.254.169.254/ service, as well as the SSH pubkey
provided to the cloud instance. After fetching the two pieces of data,
ucd-aws combines them into a valid `#cloud-config` user-data text
block and passes the output to `ucd`(1) for processing/execution.

## OPTIONS

`ucd-aws` has no configurable options.

## EXIT STATUS

On success, 0 is returned, a non-zero failure code otherwise. The exit
code returned may be the exit code of the subsequent `ucd` program
execution.

## COPYRIGHT

 * Copyright (C) 2017 Intel Corporation, License: CC-BY-SA-3.0

## SEE ALSO

`ucd`(1)

## NOTES

Creative Commons Attribution-ShareAlike 3.0 Unported

 * http://creativecommons.org/licenses/by-sa/3.0/
