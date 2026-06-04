package main

import (
	"encoding/base64"
	"flag"
	"fmt"
	"math/rand"
	"net"
	"os"
	"sort"
	"sync"
	"time"
)

func main() {
	host    := flag.String("host", "localhost", "host to connect to")
	port    := flag.Int("port", 8000, "port to connect to")
	iters   := flag.Int("n", 0, "total number of requests (0 = infinite)")
	noDelay := flag.Bool("nodelay", false, "disable random delay between sends")
	concur  := flag.Int("c", 1, "number of concurrent goroutines")

	flag.Usage = func() {
		fmt.Fprintf(flag.CommandLine.Output(), "Usage of %s:\n", os.Args[0])
		fmt.Fprintf(flag.CommandLine.Output(), "\nProject Sentinel Network Tester\n")
		fmt.Fprintf(flag.CommandLine.Output(), "Sends random base64-encoded payloads to a TCP server and measures response latency.\n\n")
		fmt.Fprintf(flag.CommandLine.Output(), "Options:\n")
		flag.PrintDefaults()
		fmt.Fprintf(flag.CommandLine.Output(), "\nExamples:\n")
		fmt.Fprintf(flag.CommandLine.Output(), "  go run scripts/nctest.go -host localhost -port 8000 -n 100 -c 10\n")
		fmt.Fprintf(flag.CommandLine.Output(), "  go run scripts/nctest.go -port 9000 -c 4 -nodelay\n")
	}

	flag.Parse()

	addr := fmt.Sprintf("%s:%d", *host, *port)

	var (
		mu        sync.Mutex
		latencies []float64
		wg        sync.WaitGroup
	)

	// work channel; closed when all work is distributed
	type job struct{ seq int }
	jobs := make(chan job, *concur*2)

	// result collector
	type result struct {
		seq     int
		ms      float64
		status  string
		ts      string
		payload string
	}
	results := make(chan result, *concur*2)

	// spawn workers
	for i := 0; i < *concur; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for j := range jobs {
				payload := randomPayload(4)
				ts := time.Now().Format("15:04:05")

				start := time.Now()
				err := sendPayload(addr, payload)
				ms := float64(time.Since(start).Nanoseconds()) / 1e6

				status := "OK"
				if err != nil {
					status = "FAIL"
				}

				results <- result{j.seq, ms, status, ts, payload}

				if !*noDelay {
					jitter := time.Duration(rand.Intn(101)+100) * time.Millisecond
					time.Sleep(jitter)
				}
			}
		}()
	}

	// result printer (single goroutine — ordered output)
	done := make(chan struct{})
	go func() {
		for r := range results {
			fmt.Printf("[%s] #%d %s -> %s (%s) %.2fms\n",
				r.ts, r.seq, addr, r.payload, r.status, r.ms)
			mu.Lock()
			latencies = append(latencies, r.ms)
			mu.Unlock()
		}
		close(done)
	}()

	// feed jobs
	if *iters > 0 {
		for i := 0; i < *iters; i++ {
			jobs <- job{i + 1}
		}
	} else {
		for i := 0; ; i++ {
			jobs <- job{i + 1}
		}
	}

	close(jobs)
	wg.Wait()
	close(results)
	<-done

	if *iters > 0 && len(latencies) > 0 {
		printSummary(latencies, *iters)
	}
}

func sendPayload(addr, payload string) error {
	conn, err := net.DialTimeout("tcp", addr, 3*time.Second)
	if err != nil {
		return err
	}
	defer conn.Close()
	conn.SetDeadline(time.Now().Add(3 * time.Second))
	_, err = fmt.Fprintln(conn, payload)
	return err
}

func randomPayload(n int) string {
	b := make([]byte, n)
	rand.Read(b)
	return base64.RawStdEncoding.EncodeToString(b)[:n]
}

func printSummary(latencies []float64, iters int) {
	n := len(latencies)
	sorted := make([]float64, n)
	copy(sorted, latencies)
	sort.Float64s(sorted)

	var sum float64
	for _, v := range latencies {
		sum += v
	}

	avg := sum / float64(n)
	min := sorted[0]
	max := sorted[n-1]
	p50 := percentile(sorted, 50)
	p90 := percentile(sorted, 90)

	fmt.Printf("\nLatency summary for %d iterations (ms): min=%.2f avg=%.2f max=%.2f p50=%.2f p90=%.2f\n",
		iters, min, avg, max, p50, p90)
}

func percentile(sorted []float64, p int) float64 {
	n := len(sorted)
	if n == 1 {
		return sorted[0]
	}
	idx := (n*p + 99) / 100 - 1
	if idx >= n {
		idx = n - 1
	}
	return sorted[idx]
}