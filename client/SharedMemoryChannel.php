<?php

class SharedMemoryChannel
{
	const MAX_LENGTH = 1048576 - 4;
	const TIMEOUT = 10;

	private $semaphore_send;
	private $semaphore_receive;
	private $shared_mem;

	public function __construct($args)
	{
		// оба семафора и блок памяти должны быть выставлены в C++ коде
		$this->semaphore_send = posix_sem_open($args['send'], 0);
		if ($this->semaphore_send === null) {
			throw new \Exception("Unable to open send semaphore");
		}
		$this->semaphore_receive = posix_sem_open($args['receive'], 0);
		if ($this->semaphore_receive === null) {
			throw new \Exception("Unable to open receive semaphore");
		}
		$this->shared_mem = shmop_open($args['mem'], 'w', 0, 0);
	}

	// Предполагалось в деструкторе очищать семаформы и расшаренную память,
	// однако это чревато неприятными кейсами, когда мы удаляем семафор, который
	// к моменту удаления уже переиспользован. Потому управление этими
	// ресурсами - прерогатива исключительно "той" стороны.
	// Однако закрыть семафоры и shared-mem было бы неплохо,
	// ибо иначе при реконнектах эти ресурсы могут "зависнуть".
	public function __destruct()
	{
		$this->close();
	}

	public function close()
	{
		if ($this->semaphore_send !== null) {
			posix_sem_close($this->semaphore_send);
			$this->semaphore_send = null;
		}
		if ($this->semaphore_receive !== null) {
			posix_sem_close($this->semaphore_receive);
			$this->semaphore_receive = null;
		}
		if ($this->shared_mem !== null) {
			shmop_close($this->shared_mem);
			$this->shared_mem = null;
		}
	}

	public function sendMessage($s)
	{
		$len = strlen($s);
		if ($len > self::MAX_LENGTH) throw new \Exception("Too long message");
		$lens = chr(($len >> 0) & 0xFF) . chr(($len >> 8) & 0xFF) . chr(($len >> 16) & 0xFF) . chr(($len >> 24) & 0xFF);
		shmop_write($this->shared_mem, $lens, 0);
		shmop_write($this->shared_mem, $s, 4);
		posix_sem_post($this->semaphore_send);
	}

	public function receiveMessage()
	{
		$start = time();
		while (time() < $start + self::TIMEOUT) {
			if (posix_sem_timedwait($this->semaphore_receive, time() + 10, 0) === true) {
				$lens = shmop_read($this->shared_mem, 0, 4);
				$len = ord($lens[0]) << 0;
				$len += ord($lens[1]) << 8;
				$len += ord($lens[2]) << 16;
				$len += ord($lens[3]) << 24;
				$s = shmop_read($this->shared_mem, 4, $len);
				return $s;
			}
		}
		throw new \Exception("Timeout reading from SharedMemoryChannel");
	}

}