<?php

require_once('Socket.php');

function makeRequest($size)
{
	$result = '';
	for ($i = 0; $i < $size; $i++) $result .= chr(rand() % 256);
	return $result;
}

function testSocket($amount, $reconnect, $size)
{
	$s = null;
	$current = 0;
	for ($i = 0; $i < $amount; $i++)
	{
		if ($s === null)
		{
			$s = new IPSocket('127.0.0.1', 19876);
			$s->connect();
			$current = 0;
		}
		$m = makeRequest($size);
		$s->sendMessage($m);
		$r = $s->receiveMessage();
		$current++;
		if ($current >= $reconnect)
		{
			$s = null;
		}
	}
}

function testUnixSocket($amount, $reconnect, $size)
{
	$s = null;
	$current = 0;
	for ($i = 0; $i < $amount; $i++)
	{
		if ($s === null)
		{
			$s = new UnixSocket('/tmp/ipc_test_socket');
			$s->connect();
			$current = 0;
		}
		$m = makeRequest($size);
		$s->sendMessage($m);
		$r = $s->receiveMessage();
		$current++;
		if ($current >= $reconnect)
		{
			$s = null;
		}
	}
}

$requests_amount = $argv[1];
$requests_per_connect = $argv[2];
$request_size = $argv[3];

print('Testing '.$requests_amount.' requests, reconnect after '.
		$requests_per_connect.' size '.$request_size."\n");


print("Testing IP socket:\n");
$start = microtime(true);
testSocket($requests_amount, $requests_per_connect, $request_size);
$finish = microtime(true);

$elapsed = $finish - $start;
print('Total: '.round($elapsed, 3).' seconds, '.round($requests_amount / $elapsed, 3).' RPS'."\n");

print("Testing Unix socket:\n");
$start = microtime(true);
testUnixSocket($requests_amount, $requests_per_connect, $request_size);
$finish = microtime(true);

$elapsed = $finish - $start;
print('Total: '.round($elapsed, 3).' seconds, '.round($requests_amount / $elapsed, 3).' RPS'."\n");
