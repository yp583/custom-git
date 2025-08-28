package main

import "fmt"

func main() {
    message := "Hello Go World!"
    fmt.Println(message)
    
    numbers := []int{1, 2, 3, 4, 5}
    sum := 0
    for _, num := range numbers {
        sum += num
    }
    fmt.Printf("Sum: %d\n", sum)
}