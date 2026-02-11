package main

import (
	"sync"
	"sync/atomic"
)

type Job struct {
	SessionID string
	Fn        func()
}

type WorkerPool struct {
	jobs     chan Job
	wg       sync.WaitGroup
	size     int
	queued   atomic.Int32
	running  atomic.Int32
}

func NewWorkerPool(size int) *WorkerPool {
	p := &WorkerPool{
		jobs: make(chan Job, 100),
		size: size,
	}

	for i := 0; i < size; i++ {
		p.wg.Add(1)
		go p.worker()
	}

	return p
}

func (p *WorkerPool) worker() {
	defer p.wg.Done()
	for job := range p.jobs {
		p.queued.Add(-1)
		p.running.Add(1)
		job.Fn()
		p.running.Add(-1)
	}
}

func (p *WorkerPool) Submit(job Job) int {
	pos := int(p.queued.Add(1))
	p.jobs <- job
	return pos
}

func (p *WorkerPool) QueuePosition() int {
	return int(p.queued.Load())
}

func (p *WorkerPool) Close() {
	close(p.jobs)
	p.wg.Wait()
}
