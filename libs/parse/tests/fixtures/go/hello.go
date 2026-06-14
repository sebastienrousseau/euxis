// Minimal Go fixture used by libs/parse tests. Covers package, import,
// struct, method, interface, goroutine, and channel — the grammar paths
// most Go code we'll scan exercises.

package main

import (
	"context"
	"fmt"
	"sync"
)

type Greeting struct {
	Subject string
	Reps    int
}

type Renderable interface {
	Render() string
}

func (g Greeting) Render() string {
	return fmt.Sprintf("Hello, %s!", g.Subject)
}

func add(a, b int) int {
	return a + b
}

func main() {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		g := Greeting{Subject: "world", Reps: add(40, 2)}
		fmt.Println(g.Render(), ctx.Err())
	}()
	wg.Wait()
}
