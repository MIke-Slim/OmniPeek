package main

import (
	"fmt"
	"net/http"
	"time"
)

func handlePreview(w http.ResponseWriter, r *http.Request) {
	fmt.Fprintf(w, "OmniPeek Go Microservice: Status OK (Time: %s)", time.Now().Format(time.RFC3339))
}

func main() {
	http.HandleFunc("/api/health", handlePreview)
	fmt.Println("OmniPeek Go Service listening on port 8080...")
}
