package main

import (
	"context"
	"fmt"
	"log"
	"math/rand"
	"net"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

const (
	port = ":50051"
)

// HelloWorld service implementation
type greeterServer struct {
	UnimplementedGreeterServer
}

func (s *greeterServer) SayHello(ctx context.Context, req *HelloRequest) (*HelloReply, error) {
	log.Printf("Received SayHello request: name=%s", req.GetName())
	return &HelloReply{
		Message: fmt.Sprintf("Hello, %s! Welcome to godot_grpc demo server.", req.GetName()),
	}, nil
}

// Metrics service implementation
type monitorServer struct {
	UnimplementedMonitorServer
}

func (s *monitorServer) StreamMetrics(req *MetricsRequest, stream Monitor_StreamMetricsServer) error {
	interval := time.Duration(req.GetIntervalMs()) * time.Millisecond
	if interval == 0 {
		interval = 1000 * time.Millisecond
	}

	count := int(req.GetCount())
	if count == 0 {
		count = 10 // Default to 10 metrics
	}

	log.Printf("Starting metrics stream: interval=%v, count=%d", interval, count)

	metricNames := []string{"cpu_usage", "memory_usage", "disk_io", "network_throughput", "request_count"}

	for i := 0; i < count; i++ {
		select {
		case <-stream.Context().Done():
			log.Println("Client cancelled metrics stream")
			return stream.Context().Err()
		default:
			// Generate random metric
			metric := &MetricData{
				Name:      metricNames[rand.Intn(len(metricNames))],
				Value:     rand.Float64() * 100,
				Timestamp: time.Now().Unix(),
			}

			log.Printf("Sending metric #%d: %s = %.2f", i+1, metric.Name, metric.Value)

			if err := stream.Send(metric); err != nil {
				log.Printf("Error sending metric: %v", err)
				return err
			}

			time.Sleep(interval)
		}
	}

	log.Println("Metrics stream completed")
	return nil
}

func main() {
	rand.Seed(time.Now().UnixNano())

	lis, err := net.Listen("tcp", port)
	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	s := grpc.NewServer()

	// Register services
	RegisterGreeterServer(s, &greeterServer{})
	RegisterMonitorServer(s, &monitorServer{})

	// Register reflection service on gRPC server.
	reflection.Register(s)

	log.Printf("godot_grpc demo server listening on %s", port)
	log.Println("Available services:")
	log.Println("  - helloworld.Greeter/SayHello (unary)")
	log.Println("  - metrics.Monitor/StreamMetrics (server-streaming)")

	if err := s.Serve(lis); err != nil {
		log.Fatalf("Failed to serve: %v", err)
	}
}
