#include <iostream>
using namespace std;

void swap(int* a, int* b) {
	int temp = *a;
	*a = *b;
	*b = temp;
}

void iSort(int a[], int n) {
	for (int i = 1; i < n; i++) {
		for (int j = i; j > 0; j--) {
			swap(a[j], a[j-1]);
		}
	}
}


// int main() {
// 	// tester stuff for array
// 	int n = 10;
// 	int a[10] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

// 	for (int i = 0; i < n; i++) {
// 		cout << a[i] << " ";
// 	}
// 	cout << endl;

// 	iSort(a, n);

// 	for (int i = 0; i < n; i++) {
// 		cout << a[i] << " ";
// 	}
// 	cout << endl;
	

// 	return 0;
// }