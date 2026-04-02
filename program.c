#include <stdio.h>
#include <stdlib.h>

int global_counter = 5;

// Function 1
int compute_sum(int *arr, int size) {
    int sum = 0;
    for (int i = 0; i < size; i++) {
        if (arr[i] > 0) {
            sum += arr[i];
        }
    }
    return sum;
}

// Function 2
void fill_array(int *arr, int size) {
    for (int i = 0; i < size; i++) {
        arr[i] = i - 2;
    }
}

// Function 3
void print_result(int result) {
    printf("Result: %d\n", result);
}

int main() {
    int size = 5;

    int *arr = malloc(size * sizeof(int));
    if (!arr) return 1;

    fill_array(arr, size);
    int result = compute_sum(arr, size) + global_counter;

    print_result(result);

    free(arr);
    return 0;
}