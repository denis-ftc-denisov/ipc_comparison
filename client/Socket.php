<?php

abstract class Socket
{
	const RECEIVE_TIMEOUT = 10;

	protected $sock;
	protected $throw_exception_on_timeout;
	protected $connected;

	abstract public function connect();
	
	/**
	 * Закрытие соединения
	 */
	public function close()
	{
		if ($this->connected) 
		{
			socket_close($this->sock);
			$this->connected = false;
		}
	}

	/*
	 * Пропихивает строку в сокет.
	 */

	public function iterativeSend($s)
	{
		while ($s != "") 
		{
			$cnt = @socket_send($this->sock, $s, strlen($s), 0);
			if ($cnt === FALSE) throw new Exception("Sending error: " . socket_strerror(socket_last_error()));
			$s = substr($s, $cnt);
		}
	}

	/*
	 * Притаскивает из сокета заданное число байт
	 */

	public function iterativeReceive($cnt, $timeout = 0)
	{
		$s = "";
		$start = microtime(true);
		while (strlen($s) != $cnt) 
		{
			$buf = "";
			if (socket_recv($this->sock, $buf, $cnt - strlen($s), MSG_DONTWAIT) === FALSE) 
			{
				if (socket_last_error() != 11) 
				{
					throw new Exception("Receiving error: " . socket_strerror(socket_last_error()));
				}
			}
			if (strlen($buf) == 0) usleep(10 * 1000);
			$s .= $buf;
			if ($timeout > 0 && microtime(true) - $start > $timeout) 
			{
				if ($this->throw_exception_on_timeout) 
				{
					throw new \Exception("Timeout on receiving, wanted " . $cnt . " received so far " . strlen($s));
				} 
				else 
				{
					return $s;
				}
			}
		}
		return $s;
	}

	/*
	 * Тянет из сокета данные, пока не вытянет заданную строку
	 */

	public function receiveTerminated($t, $timeout = 0)
	{
		$s = "";
		$start = microtime(true);
		while (true) 
		{
			$s .= $this->iterativeReceive(1, $timeout);
			if (strlen($t) <= strlen($s) && substr($s, strlen($s) - strlen($t)) == $t) break;
			if ($timeout > 0 && microtime(true) - $start > $timeout) 
			{
				if ($this->throw_exception_on_timeout) 
				{
					throw new \Exception("Timeout on terminated receiving, received so far " . strlen($s));
				} 
				else 
				{
					return $s;
				}
			}
		}
		return $s;
	}


	/*
	 * Отправляет строку в сокет согласно протоколу (4 байта длина, потом данные)
	 */

	public function sendMessage($s)
	{
		$len = strlen($s);
		$lens = chr(($len >> 0) & 0xFF) . chr(($len >> 8) & 0xFF) . chr(($len >> 16) & 0xFF) . chr(($len >> 24) & 0xFF);
		$this->iterativeSend($lens);
		$this->iterativeSend($s);
	}

	/*
	 * Принимает строку из сокета. Бросит exception если что-то не так
	 */

	public function receiveMessage()
	{
		$lens = $this->iterativeReceive(4, self::RECEIVE_TIMEOUT);
		if (strlen($lens) < 4) {
			throw new \Exception("Too short length string received");
		}
		$len = ord($lens[0]) << 0;
		$len += ord($lens[1]) << 8;
		$len += ord($lens[2]) << 16;
		$len += ord($lens[3]) << 24;
		return $this->iterativeReceive($len, 1);
	}

}

class IPSocket extends Socket
{
	private $host;
	private $port;
	
	public function __construct($ip='', $port='', $throw_exception_on_timeout = false, $auto_connect = true)
	{
		$this->sock = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
		if ($this->sock === FALSE) 
		{
			throw new Exception("Error creating socket: ".socket_last_error()." ".socket_strerror(socket_last_error()));
		}
		$this->connected = false;
		$this->host = $ip;
		$this->port = $port;
		if ($auto_connect)
		{
			$this->connect();
		}
		$this->throw_exception_on_timeout = $throw_exception_on_timeout;
	}

	public function connect()
	{
		if ($this->connected) return;
		// тушим ошибки, иначе вывалится с PHP Error вместо exception
		if (@socket_connect($this->sock, $this->host, $this->port) === FALSE) 
		{
			throw new Exception("Error connecting: ".socket_last_error()." ".socket_strerror(socket_last_error()));
		}
		$this->connected = true;
	}
}

class UnixSocket extends Socket
{
	private $path;

	public function __construct($path='', $throw_exception_on_timeout = false, $auto_connect = true)
	{
		$this->sock = socket_create(AF_UNIX, SOCK_STREAM, 0);
		if ($this->sock === FALSE) 
		{
			throw new Exception("Error creating socket: ".socket_last_error()." ".socket_strerror(socket_last_error()));
		}
		$this->connected = false;
		$this->path = $path;
		if ($auto_connect)
		{
			$this->connect();
		}
		$this->throw_exception_on_timeout = $throw_exception_on_timeout;
	}

	public function connect()
	{
		if ($this->connected) return;
		// тушим ошибки, иначе вывалится с PHP Error вместо exception
		if (@socket_connect($this->sock, $this->path) === FALSE) 
		{
			throw new Exception("Error connecting: ".socket_last_error()." ".socket_strerror(socket_last_error()));
		}
		$this->connected = true;
	}
}